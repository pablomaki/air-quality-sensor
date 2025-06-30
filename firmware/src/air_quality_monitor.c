#include <air_quality_monitor.h>
#include <components/bluetooth_handler.h>
#include <components/e_paper_display.h>
#include <components/sensors.h>
#include <components/battery_monitor.h>
#include <utils/variable_buffer.h>
#include <components/event_handler.h>
#include <components/state_manager.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(air_quality_monitor);

#define SCHEDULE_SUCCESS 0
#define SCHEDULE_ALREADY_QUEUED 1

static struct k_work_delayable periodic_work;

/**
 * @brief Update data to BLE service and start data advertisement
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
static int advertise_data(void)
{
    int rc = 0;
    // Update and advertise data
    LOG_INF("Update advertised data.");
    update_advertisement_data();

    LOG_INF("Begin advertising for connection.");
    rc = start_advertise();
    if (rc != 0)
    {
        LOG_ERR("Error advertising data (err %d).", rc);
        return rc;
    }
    return rc;
}

/**
 * @brief Schedule the next work task
 *
 * @param delay Delay for launching the task
 * @return int, 0 if ok, non-zero if an error occured
 */
static int schedule_work_task(int64_t delay)
{
    int rc = 0;
    rc = k_work_schedule(&periodic_work, K_MSEC(delay));
    if (rc != SCHEDULE_SUCCESS && rc != SCHEDULE_ALREADY_QUEUED)
    {
        LOG_ERR("Error scheduling a task (err %d).", rc);
        return rc;
    }
    return 0;
}

/**
 * @brief Calculate the delay for the next task
 *
 * @param start_time_ms Start time of the task
 * @return int64_t Delay in milliseconds
 */
static int64_t calculate_task_delay(int64_t start_time_ms)
{
    int64_t delay = CONFIG_ADVERTISEMENT_INTERVAL / CONFIG_MEASUREMENTS_PER_INTERVAL - (k_uptime_get() - start_time_ms);
    if (delay < 0)
    {
        LOG_ERR("Missed deadline, scheduling immediately. Delay was %lld ms.", delay);
        delay = 0; // Prevent negative delay
    }
    return delay;
}

/**
 * @brief Periodic task that takes care of reading sensor data and advertising it over BLE
 *
 * @param work Address of work item.
 */
static void periodic_task(struct k_work *work)
{
    static uint8_t measurement_counter = 0;
    bool success = true;
    int rc = 0;

    LOG_INF("Periodic task begin.");

    // Log time for calculating correct time to sleep
    int64_t start_time_ms = k_uptime_get();

    // Set state to measuring and idle idle some time for sensor warmup
    set_state(MEASURING);
    k_sleep(K_MSEC(CONFIG_SENSOR_WARMUP_TIME_MS));

#ifdef CONFIG_ENABLE_BATTERY_MONITOR
    LOG_INF("Reading battery level.");
    rc = read_battery_level();
    if (rc != 0)
    {
        LOG_WRN("Failed to read battery percentage (err %d).", rc);
        success = false;
        dispatch_event(PERIODIC_TASK_WARNING);
    }
#endif

    LOG_INF("Reading sensors.");
    rc = read_sensors();
    if (rc != 0)
    {
        LOG_WRN("Failed to read sensor data (err %d).", rc);
        success = false;
        dispatch_event(PERIODIC_TASK_WARNING);
    }

    // Increment measurement counter and print progress in log
    measurement_counter++;
    LOG_INF("Periodic measurement %d/%d done.", measurement_counter, CONFIG_MEASUREMENTS_PER_INTERVAL);

#ifdef CONFIG_ENABLE_EPD

    // Set state to displaying
    set_state(UPDATING);

    LOG_INF("Updating displayed values.");
    rc = update_e_paper_display();
    if (rc != 0)
    {
        LOG_ERR("Error updating E-paper display (err %d).", rc);
        dispatch_event(PERIODIC_TASK_WARNING);
    }
#endif

    // Stop the periodic task in short in case of not enough measurements made yet
    if (measurement_counter < CONFIG_MEASUREMENTS_PER_INTERVAL)
    {
        LOG_INF("Periodic task done, scheduling a new task.");
        int64_t delay = calculate_task_delay(start_time_ms);
        rc = schedule_work_task(delay);
        if (rc != 0)
        {
            dispatch_event(PERIODIC_TASK_ERROR);
            set_state(ERROR);
        }
        LOG_INF("Task scheduled, entering idle state.");
        if (success)
        {
            dispatch_event(PERIODIC_TASK_SUCCESS);
        }

        // Enter idle state
        set_state(IDLE);
        return;
    }

    // Reset measurement counter
    measurement_counter = 0;

    // Set state to advertising
    set_state(ADVERTISING);

    LOG_INF("Advertising data.");
    rc = advertise_data();
    if (rc != 0)
    {
        dispatch_event(PERIODIC_TASK_WARNING);
    }

    LOG_INF("Periodic task done, scheduling a new task.");
    int64_t delay = calculate_task_delay(start_time_ms);
    rc = schedule_work_task(delay);
    if (rc != 0)
    {
        dispatch_event(PERIODIC_TASK_ERROR);
        set_state(ERROR);
    }

    LOG_INF("Task scheduled, waiting BLE communication to stop before idle.");
}

/**
 * @brief Callback for when BLE is done (data relayed or timeout)
 *
 * @param task_success True if BLE task terminated succesfully, false timeout.
 */
static void ble_task_callback(bool task_success)
{
    if (task_success)
    {
        LOG_INF("BLE data transfer completed succesfully, entering idle state.");
        dispatch_event(PERIODIC_TASK_SUCCESS);
    }
    else
    {
        LOG_INF("BLE data transfer unsuccesful, entering idle state.");
        dispatch_event(PERIODIC_TASK_WARNING);
    }

    // Enter idle state
    set_state(IDLE);
}

/**
 * @brief Callback for when BLE conneciton is established
 *
 */
static void ble_connected_callback(void)
{
    LOG_INF("BLE connection established.");
    dispatch_event(BLE_CONNECTION_SUCCESS);
}

int init_air_quality_monitor(void)
{
    int rc = 0;

    // Set state correctly
    set_state(INITIALIZING);

    // Initialize LED controller
    LOG_INF("Initializing event handler.");
    rc = init_event_handler();
    if (rc != 0)
    {
        LOG_ERR("Error while initializing event handler (err %d).", rc);
        dispatch_event(INITIALIZATION_ERROR);
        set_state(ERROR);
        return rc;
    }
    LOG_INF("Event handler initialized succesfully.");

#ifdef CONFIG_ENABLE_EPD
    // Initialize E-paper display
    LOG_INF("Initializing E-paper display.");
    rc = init_e_paper_display();
    if (rc != 0)
    {
        LOG_ERR("Error while initializing E-paper display (err %d).", rc);
        dispatch_event(INITIALIZATION_ERROR);
        set_state(ERROR);
        return rc;
    }
    LOG_INF("E-paper display initialized succesfully.");
#endif

    // Initialize bluetooth
    LOG_INF("Initializing BLE.");
    rc = init_ble(ble_task_callback, ble_connected_callback);
    if (rc != 0)
    {
        LOG_ERR("Error while initializing BLE (err %d).", rc);
        dispatch_event(INITIALIZATION_ERROR);
        set_state(ERROR);
        return rc;
    }
    LOG_INF("BLE initialized succesfully. BLE device \"%s\" online.", CONFIG_BT_DEVICE_NAME);

    // Initialize sensors
    LOG_INF("Initializing the sensors.");
    rc = init_sensors();
    if (rc != 0)
    {
        LOG_ERR("Error while initializing sensors (err %d).", rc);
        dispatch_event(INITIALIZATION_ERROR);
        set_state(ERROR);
        return rc;
    }
    LOG_INF("Sensors initialized succesfully.");

#ifdef CONFIG_ENABLE_BATTERY_MONITOR
    // Initialize battery monitor
    LOG_INF("Initializing the battery monitor.");
    rc = init_battery_monitor();
    if (rc != 0)
    {
        LOG_ERR("Error while initializing the battery monitor (err %d).", rc);
        dispatch_event(INITIALIZATION_ERROR);
        set_state(ERROR);
        return rc;
    }
    LOG_INF("Battery monitor initialized succesfully.");
#endif

    // Initialize periodic task and time the first task in 10 seconds
    LOG_INF("Setting up the periodic task for measuring and advertising data.");
    k_work_init_delayable(&periodic_work, periodic_task);
    rc = schedule_work_task(1000); // Start the first task in 1 seconds
    if (rc != 0)
    {
        dispatch_event(INITIALIZATION_ERROR);
        set_state(ERROR);
        return rc;
    }
    LOG_INF("Periodic task initialized succesfully.");
    dispatch_event(INITIALIZATION_SUCCESS);

    // Enter idle state
    set_state(IDLE);

    return 0;
}