#include <components/thread_monitor.h>

#include <app/server/Server.h>
#include <platform/CHIPDeviceLayer.h>

#include <esp_log.h>
#include <esp_openthread.h>
#include <esp_openthread_lock.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <openthread/link.h>
#include <openthread/thread.h>

static const char *TAG = "thread_monitor";

namespace
{

constexpr uint32_t kTaskStackSize = 3072;
constexpr UBaseType_t kTaskPriority = 1;
constexpr uint32_t kCheckIntervalS =
    CONFIG_THREAD_DIAGNOSTICS_INTERVAL_S > 0 ? CONFIG_THREAD_DIAGNOSTICS_INTERVAL_S : 30;

/**
 * @brief The Thread state sampled under the OpenThread lock
 */
struct LinkSnapshot
{
    otDeviceRole role;
    bool hasParent;
    otRouterInfo parent;
    int8_t averageRssi;
    int8_t lastRssi;
    otMacCounters counters;
};

/**
 * @brief Read the Thread role, the parent link and the radio counters
 *
 * @param snapshot Destination for the state
 * @return bool, true if the OpenThread lock was obtained and the snapshot filled
 */
bool TakeSnapshot(LinkSnapshot &snapshot)
{
    if (!esp_openthread_lock_acquire(pdMS_TO_TICKS(1000)))
    {
        return false;
    }

    otInstance *instance = esp_openthread_get_instance();
    snapshot.role = otThreadGetDeviceRole(instance);
    snapshot.hasParent = snapshot.role == OT_DEVICE_ROLE_CHILD &&
                         otThreadGetParentInfo(instance, &snapshot.parent) == OT_ERROR_NONE;
    if (snapshot.hasParent)
    {
        otThreadGetParentAverageRssi(instance, &snapshot.averageRssi);
        otThreadGetParentLastRssi(instance, &snapshot.lastRssi);
    }
    snapshot.counters = *otLinkGetCounters(instance);

    esp_openthread_lock_release();
    return true;
}

/**
 * @brief Log one diagnostics line: role, parent link, and the counters since the previous line
 *
 * TX no-ack is the share of frames that requested an acknowledgement and got
 * none, the signature of the parent not hearing this device. CCA failures mean
 * the channel was busy, FCS errors corrupted frames received.
 *
 * @param now Current snapshot
 * @param previous Snapshot of the previous line
 */
void LogDiagnostics(const LinkSnapshot &now, const LinkSnapshot &previous)
{
    const otMacCounters &c = now.counters;
    const otMacCounters &p = previous.counters;
    uint32_t ackRequested = c.mTxAckRequested - p.mTxAckRequested;
    uint32_t acked = c.mTxAcked - p.mTxAcked;

    char parent[64] = "no parent";
    if (now.hasParent)
    {
        snprintf(parent, sizeof(parent), "parent 0x%04x rssi avg %d last %d lq in %u out %u", now.parent.mRloc16,
                 now.averageRssi, now.lastRssi, now.parent.mLinkQualityIn, now.parent.mLinkQualityOut);
    }

    ESP_LOGI(TAG, "%s, %s | tx %lu acked %lu/%lu retry %lu maxretry %lu cca %lu abort %lu | rx %lu fcs %lu sec %lu",
             otThreadDeviceRoleToString(now.role), parent, (unsigned long)(c.mTxTotal - p.mTxTotal),
             (unsigned long)acked, (unsigned long)ackRequested, (unsigned long)(c.mTxRetry - p.mTxRetry),
             (unsigned long)(c.mTxDirectMaxRetryExpiry - p.mTxDirectMaxRetryExpiry),
             (unsigned long)(c.mTxErrCca - p.mTxErrCca), (unsigned long)(c.mTxErrAbort - p.mTxErrAbort),
             (unsigned long)(c.mRxTotal - p.mRxTotal), (unsigned long)(c.mRxErrFcs - p.mRxErrFcs),
             (unsigned long)(c.mRxErrSec - p.mRxErrSec));
}

/**
 * @brief Whether the device belongs to at least one fabric
 *
 * An uncommissioned device has no Thread network to attach to, so being
 * detached is its normal state and no reason to reboot.
 *
 * @return bool, true if commissioned
 */
bool IsCommissioned()
{
    chip::DeviceLayer::StackLock lock;
    return chip::Server::GetInstance().GetFabricTable().FabricCount() > 0;
}

/**
 * @brief Sample the link on every interval, log it and enforce the detached reboot
 *
 * @param arg Unused
 */
void MonitorTask(void *arg)
{
    (void)arg;
    LinkSnapshot previous = {};
    TakeSnapshot(previous);
    uint32_t detachedS = 0;

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(kCheckIntervalS * 1000));

        LinkSnapshot now = {};
        if (!TakeSnapshot(now))
        {
            ESP_LOGW(TAG, "OpenThread lock busy, skipping this check.");
            continue;
        }

        if (CONFIG_THREAD_DIAGNOSTICS_INTERVAL_S > 0)
        {
            LogDiagnostics(now, previous);
        }
        previous = now;

        bool attached = now.role == OT_DEVICE_ROLE_CHILD || now.role == OT_DEVICE_ROLE_ROUTER ||
                        now.role == OT_DEVICE_ROLE_LEADER;
        detachedS = attached ? 0 : detachedS + kCheckIntervalS;

        if (CONFIG_THREAD_DETACHED_REBOOT_MINUTES > 0 && detachedS >= CONFIG_THREAD_DETACHED_REBOOT_MINUTES * 60 &&
            IsCommissioned())
        {
            ESP_LOGE(TAG, "Detached from Thread for %lu s, rebooting to recover the radio.", (unsigned long)detachedS);
            esp_restart();
        }
    }
}

} // namespace

int init_thread_monitor(void)
{
    if (xTaskCreate(MonitorTask, "thread_monitor", kTaskStackSize, nullptr, kTaskPriority, nullptr) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to start the Thread monitor.");
        return ESP_ERR_NO_MEM;
    }
    return 0;
}
