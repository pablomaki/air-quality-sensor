#include <air_quality_monitor.h>
#include <board.h>
#include <components/matter_handler.h>
#include <components/sensors.h>
#include <components/event_handler.h>
#include <components/factory_reset.h>

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "air_quality_monitor";

/** @brief Delay before the first sample, to let Matter and Thread settle */
#define STARTUP_DELAY_MS 10000

#define PERIODIC_TASK_STACK_SIZE 4096
#define PERIODIC_TASK_PRIORITY 2

#define SAMPLE_BIT (1 << 0)
#define REPORT_BIT (1 << 1)

/**
 * @brief Timers for the two independent clocks
 *
 * Sampling and reporting are scheduled separately: the sampling rate belongs to
 * the sensors, the reporting rate to the application. The timers only wake
 * the periodic task, which runs both jobs, so they are serialised against each
 * other and the value buffers need no further locking.
 */
static esp_timer_handle_t sample_timer;
static esp_timer_handle_t report_timer;
static TaskHandle_t periodic_task_handle;

/**
 * @brief Wake the periodic task for the job a timer stands for
 *
 * @param arg The notification bit of the job
 */
static void notify_periodic_task(void *arg)
{
    xTaskNotify(periodic_task_handle, (uint32_t)(uintptr_t)arg, eSetBits);
}

/**
 * @brief Reschedule a timer, compensating for how long the work itself took
 *
 * @param timer Timer to reschedule
 * @param period_ms Nominal period of the work
 * @param start_time_us Time at which this run started
 * @return int, 0 if ok, non-zero if an error occured
 */
static int reschedule(esp_timer_handle_t timer, int64_t period_ms, int64_t start_time_us)
{
    int64_t delay_us = period_ms * 1000 - (esp_timer_get_time() - start_time_us);

    if (delay_us < 0)
    {
        ESP_LOGE(TAG, "Missed deadline by %lld ms, scheduling immediately.", -delay_us / 1000);
        delay_us = 0;
    }

    esp_err_t err = esp_timer_start_once(timer, (uint64_t)delay_us);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error scheduling a task (err %d).", err);
        return err;
    }
    return 0;
}

/**
 * @brief Read every enabled sensor into the value buffers
 */
static void sample_task(void)
{
    int64_t start_time_us = esp_timer_get_time();
    bool success = true;

    if (read_sensors() != 0)
    {
        ESP_LOGW(TAG, "Failed to read some sensor data.");
        success = false;
        dispatch_event(PERIODIC_TASK_WARNING);
    }

    if (reschedule(sample_timer, CONFIG_SAMPLE_INTERVAL_MS, start_time_us) != 0)
    {
        dispatch_event(PERIODIC_TASK_ERROR);
    }
    else if (success)
    {
        dispatch_event(PERIODIC_TASK_SUCCESS);
    }
}

/**
 * @brief Publish the buffered values to the Matter data model
 */
static void report_task(void)
{
    int64_t start_time_us = esp_timer_get_time();

    if (update_cluster_states() != 0)
    {
        dispatch_event(PERIODIC_TASK_WARNING);
    }

    if (reschedule(report_timer, CONFIG_REPORT_INTERVAL_MS, start_time_us) != 0)
    {
        dispatch_event(PERIODIC_TASK_ERROR);
    }
}

/**
 * @brief Run whichever of the sampling and reporting jobs are due
 *
 * @param arg Unused
 */
static void periodic_task(void *arg)
{
    (void)arg;

    while (true)
    {
        uint32_t due = 0;
        xTaskNotifyWait(0, UINT32_MAX, &due, portMAX_DELAY);

        if (due & SAMPLE_BIT)
        {
            sample_task();
        }
        if (due & REPORT_BIT)
        {
            report_task();
        }
    }
}

int init_air_quality_monitor(void)
{
    int rc = 0;
    int status = 0;

    rc = board_init_rf_switch();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Error while routing the antenna (err %d).", rc);
        status = rc;
    }

    // Initialize LED controller
    ESP_LOGI(TAG, "Initializing event handler.");
    rc = init_event_handler();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Error while initializing event handler (err %d).", rc);
        dispatch_event(INITIALIZATION_ERROR);
        status = rc;
    }
    else
    {
        ESP_LOGI(TAG, "Event handler initialized succesfully.");
    }

    // Counted before Matter starts, so a requested reset is known by the time the
    // server is up to carry it out.
    ESP_LOGI(TAG, "Checking the power cycle factory reset gesture.");
    rc = init_factory_reset();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Error while initializing the factory reset trigger (err %d).", rc);
        dispatch_event(INITIALIZATION_ERROR);
        status = rc;
    }

    // Initialize matter
    ESP_LOGI(TAG, "Initializing matter.");
    rc = init_matter();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Error while initializing matter (err %d).", rc);
        dispatch_event(INITIALIZATION_ERROR);
        status = rc;
    }
    else
    {
        // Needs a running Matter server: the reset withdraws the device's SRP
        // records from the border router before it erases storage.
        run_pending_factory_reset();
    }

    // Initialize sensors
    ESP_LOGI(TAG, "Initializing the sensors.");
    rc = init_sensors();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Error while initializing sensors (err %d).", rc);
        dispatch_event(INITIALIZATION_ERROR);
        status = rc;
    }
    else
    {
        ESP_LOGI(TAG, "Sensors initialized succesfully.");
    }

    if (status == 0)
    {
        dispatch_event(INITIALIZATION_SUCCESS);
    }

    return status;
}

int start_air_quality_monitor(void)
{
    int rc = 0;

    ESP_LOGI(TAG, "Setting up the sampling and reporting tasks.");
    if (xTaskCreate(periodic_task, "periodic", PERIODIC_TASK_STACK_SIZE, NULL, PERIODIC_TASK_PRIORITY,
                    &periodic_task_handle) != pdPASS)
    {
        rc = ESP_ERR_NO_MEM;
    }

    const esp_timer_create_args_t sample_args = {
        .callback = notify_periodic_task,
        .arg = (void *)(uintptr_t)SAMPLE_BIT,
        .name = "sample",
    };
    const esp_timer_create_args_t report_args = {
        .callback = notify_periodic_task,
        .arg = (void *)(uintptr_t)REPORT_BIT,
        .name = "report",
    };
    if (rc == 0)
    {
        rc = esp_timer_create(&sample_args, &sample_timer);
    }
    if (rc == 0)
    {
        rc = esp_timer_create(&report_args, &report_timer);
    }

    int64_t now = esp_timer_get_time();

    if (rc == 0)
    {
        rc = reschedule(sample_timer, STARTUP_DELAY_MS, now);
    }
    if (rc == 0)
    {
        // Offset the first report so it publishes samples that already exist
        rc = reschedule(report_timer, STARTUP_DELAY_MS + CONFIG_SAMPLE_INTERVAL_MS, now);
    }

    if (rc != 0)
    {
        // Matter keeps running on its own tasks, so the node stays addressable
        ESP_LOGE(TAG, "Failed to schedule the periodic tasks (err %d).", rc);
        dispatch_event(STARTUP_ERROR);
    }
    else
    {
        ESP_LOGI(TAG, "Sampling and reporting tasks started succesfully.");
        dispatch_event(STARTUP_SUCCESS);
    }

    return rc;
}
