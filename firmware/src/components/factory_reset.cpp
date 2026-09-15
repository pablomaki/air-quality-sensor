#include <components/factory_reset.h>
#include <drivers/led_controller.h>

#include <app/server/Server.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/reboot.h>

LOG_MODULE_REGISTER(factory_reset);

// Lives outside the Matter "mt/" subtree so it reads like application state, but
// in the same partition, which CONFIG_CHIP_FACTORY_RESET_ERASE_SETTINGS wipes -
// so a completed reset also clears the count that asked for it.
#define BOOT_COUNT_KEY "aqs/boot_count"

// The platform's factory reset ends in PlatformMgr().Shutdown() and never
// reboots, which would leave an erased but idle device behind. It reports no
// completion either, so the reboot is timed rather than chained. The bound is
// known: withdrawing the SRP records waits on a semaphore for at most 2 s, and
// erasing the 32 kB settings partition is eight page erases on top of that.
#define FACTORY_RESET_REBOOT_DELAY_MS 5000

static struct k_work_delayable clear_count_work;
static struct k_work_delayable reboot_work;
static bool reset_requested;

/**
 * @brief Read the stored boot count into the caller's counter
 *
 * @param key Unused, the subtree is the full key
 * @param len Size of the stored value
 * @param read_cb Backend read function
 * @param cb_arg Argument for the backend read function
 * @param param Address of the uint8_t to fill
 * @return int, 0 if ok, negative errno if an error occured
 */
static int read_boot_count(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg, void *param)
{
    ARG_UNUSED(key);

    uint8_t *count = (uint8_t *) param;
    if (len != sizeof(*count))
    {
        return -EINVAL;
    }

    ssize_t rc = read_cb(cb_arg, count, sizeof(*count));
    return rc < 0 ? (int) rc : 0;
}

/**
 * @brief Blink the LED, when there is one to blink
 *
 * Mirrors the guard in event_handler.c: the LED controller is only initialized
 * under CONFIG_ENABLE_EVENT_LED, so nothing may drive it when that is off.
 *
 * @param color Color to blink
 * @param count Count of times to blink
 */
static void signal(led_color_t color, int count)
{
#ifdef CONFIG_ENABLE_EVENT_LED
    blink_led(color, count);
#else
    ARG_UNUSED(color);
    ARG_UNUSED(count);
#endif
}

/**
 * @brief Reboot once the factory reset has had time to complete
 *
 * @param work Address of work item
 */
static void reboot_after_reset(struct k_work *work)
{
    ARG_UNUSED(work);

    sys_reboot(SYS_REBOOT_WARM);
}

/**
 * @brief Drop the boot count once the device has stayed up past the window
 *
 * @param work Address of work item
 */
static void clear_boot_count(struct k_work *work)
{
    ARG_UNUSED(work);

    int rc = settings_delete(BOOT_COUNT_KEY);
    if (rc != 0)
    {
        LOG_WRN("Failed to clear the boot count (err %d).", rc);
    }
}

int init_factory_reset(void)
{
    // Matter initializes the settings subsystem too, but the count has to be
    // read before that; the call is guarded and safe to repeat.
    int rc = settings_subsys_init();
    if (rc != 0)
    {
        LOG_ERR("Failed to initialize the settings subsystem (err %d).", rc);
        return rc;
    }

    uint8_t count = 0;
    rc = settings_load_subtree_direct(BOOT_COUNT_KEY, read_boot_count, &count);
    if (rc != 0)
    {
        LOG_WRN("Failed to read the boot count (err %d), starting from zero.", rc);
        count = 0;
    }

    count++;
    reset_requested = count >= CONFIG_FACTORY_RESET_POWER_CYCLES;

    if (reset_requested)
    {
        // Deliberately not stored: the erase clears it anyway, and leaving the
        // previous count in place keeps the gesture armed if the reset fails.
        LOG_WRN("Power cycled %u times in succession, a factory reset will run.", count);
        return 0;
    }

    rc = settings_save_one(BOOT_COUNT_KEY, &count, sizeof(count));
    if (rc != 0)
    {
        LOG_ERR("Failed to store the boot count (err %d).", rc);
        return rc;
    }

    signal(LED_YELLOW, count);

    k_work_init_delayable(&clear_count_work, clear_boot_count);
    k_work_schedule(&clear_count_work, K_MSEC(CONFIG_FACTORY_RESET_POWER_CYCLE_WINDOW_MS));

    return 0;
}

void run_pending_factory_reset(void)
{
    if (!reset_requested)
    {
        return;
    }

    LOG_WRN("Factory resetting, the device will reboot uncommissioned.");
    signal(LED_RED, CONFIG_FACTORY_RESET_POWER_CYCLES);

    chip::Server::GetInstance().ScheduleFactoryReset();

    k_work_init_delayable(&reboot_work, reboot_after_reset);
    k_work_schedule(&reboot_work, K_MSEC(FACTORY_RESET_REBOOT_DELAY_MS));
}
