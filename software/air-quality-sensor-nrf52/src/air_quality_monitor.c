#include <air_quality_monitor.h>
#include <bluetooth_handler.h>
#include <drivers/e_ink_display.h>
#include <power_manager.h>
#include <configs.h>
#include <sensors.h>
#include <drivers/battery.h>
#include <variables.h>
#include <event_handler.h>
#include <state_handler.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(air_quality_monitor);

#define SCHEDULE_SUCCESS 0
#define SCHEDULE_ALREADY_QUEUED 1

static struct k_work_delayable periodic_work;

/**
 * @brief Read data from a sensor and check for errors
 *
 * @param sensor_func Pointer to the function that reads the sensor data
 * @param sensor_name Name of the sensor for logging
 * @return true if reading was successful
 * @return false if there was an error
 */
static bool check_sensor_reading(int (*sensor_func)(void), const char *sensor_name)
{
    int err = sensor_func();
    if (err)
    {
        LOG_ERR("Error reading %s sensor data (err %d)", sensor_name, err);
        return false;
    }
    return true;
}

/**
 * @brief Read data from each sensor
 *
 * @return true if all sensors return ok after reading
 * @return false if one or more sensors return with an error
 */
static bool read_sensors(void)
{
    bool success = true;

    success &= check_sensor_reading(read_battery_level, "battery");

#ifdef ENABLE_SHT4X
    success &= check_sensor_reading(read_sht4x_data, "sht4x");
#endif

#ifdef ENABLE_SGP40
    success &= check_sensor_reading(read_sgp40_data, "sgp40");
#endif

#ifdef ENABLE_BMP390
    success &= check_sensor_reading(read_bmp390_data, "bmp390");
#endif

#ifdef ENABLE_SCD4X
    success &= check_sensor_reading(read_scd4x_data, "scd4x");
#endif

    return success;
}

/**
 * @brief Update data to BLE service and start data advertisement
 *
 * @return true if updating and advertisement start succeed
 * @return false if any of the above fail
 */
static bool advertise_data(void)
{
    bool success = true;
    int err;
    // Update and advertise data
    LOG_INF("Update advertised data.");
    err = update_advertisement_data();
    if (err)
    {
        LOG_ERR("Error updating advertisement data (err %d)", err);
        success = false;
    }

    LOG_INF("Begin advertising for connection.");
    err = start_advertise();
    if (err)
    {
        LOG_ERR("Error advertising data (err %d)", err);
        success = false;
    }
    return success;
}

/**
 * @brief Schedule the next work task
 *
 * @param delay Delay for launching the task
 * @return true Scheduling succeeds
 * @return false Problem with scheduling
 */
static bool schedule_work_task(int64_t delay)
{
    int err = k_work_schedule(&periodic_work, K_MSEC(delay));
    if (err != SCHEDULE_SUCCESS && err != SCHEDULE_ALREADY_QUEUED)
    {
        LOG_ERR("Error scheduling a task (err %d)", err);
        return false;
    }
    return true;
}

/**
 * @brief Periodic task that takes care of reading sensor data and advertising it over BLE
 *
 * @param work Address of work item.
 */
static void periodic_task(struct k_work *work)
{
    LOG_INF("Periodic task begin");

    // Wake up from the sleep (turn on all necessary peripherals)
    wake_up();
    set_state(ACTIVE);

    // Log time for calculating correct time to sleep
    int64_t start_time_ms = k_uptime_get();

    bool success;

    LOG_INF("Read sensors");
    success = read_sensors();
    if (!success)
    {
        dispatch_event(PERIODIC_TASK_WARNING);
    }

#ifdef ENABLE_EPD
    LOG_INF("Update displayed values.");
    int err;
    err = update_e_ink_display();
    if (err)
    {
        LOG_ERR("Error updating E-ink display (err %d)", err);
        dispatch_event(PERIODIC_TASK_WARNING);
    }
#endif

    LOG_INF("Advertise data");
    success = advertise_data();
    if (!success)
    {
        dispatch_event(PERIODIC_TASK_WARNING);
    }

    LOG_INF("Periodic task done, scheduling a new task");

    // Calculate required interval for sleeping to  execute periodic task once per WAKEUP_INTERVAL_MS
    int64_t delay = DATA_INTERVAL - (k_uptime_get() - start_time_ms);
    if (delay < 0)
    {
        LOG_ERR("Missed deadline, scheduling immediately!");
        delay = 0; // Prevent negative delay
    }
    success = schedule_work_task(delay);
    if (!success)
    {
        dispatch_event(PERIODIC_TASK_ERROR);
        set_state(ERROR);
    }

    LOG_INF("Task scheduled, waiting BLE communication to stop before idle.");
}

/**
 * @brief Enter idle mode
 *
 */
static void idle(void)
{
    set_state(IDLE);
    int err;
    err = enter_low_power_mode();
    if (err)
    {
        LOG_ERR("Something in entering/exiting the low power mode failed (err %d)", err);
        set_state(ERROR);
    }
}

/**
 * @brief Callback when BLE is done (data relayed or timeout)
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
    idle();
}

int init_air_quality_monitor(void)
{
    int err;
    bool success = true;
    set_state(INITIALIZING);

    // Initialize LED controller
    LOG_INF("Initializing power manager...");
    err = init_power_manager();
    if (err)
    {
        LOG_ERR("Error while initializing power manager (err %d)", err);
        dispatch_event(INITIALIZATION_ERROR);
        set_state(ERROR);
        return err;
    }
    LOG_INF("power manager initialized succesfully.");

    // Initialize LED controller
    LOG_INF("Initializing event handler...");
    err = init_event_handler();
    if (err)
    {
        LOG_ERR("Error while initializing event handler (err %d)", err);
        dispatch_event(INITIALIZATION_ERROR);
        set_state(ERROR);
        return err;
    }
    LOG_INF("Event handler initialized succesfully.");

#ifdef ENABLE_EPD
    // Initialize E-ink display
    LOG_INF("Initializing E-ink display...");
    err = init_e_ink_display();
    if (err)
    {
        LOG_ERR("Error while initializing E-ink display (err %d)", err);
        dispatch_event(INITIALIZATION_ERROR);
        set_state(ERROR);
        return err;
    }
    LOG_INF("E-ink display initialized succesfully.");
#endif

    // Initialize bluetooth
    LOG_INF("Initializing BLE...");
    err = init_ble(ble_task_callback);
    if (err)
    {
        LOG_ERR("Error while initializing BLE (err %d)", err);
        dispatch_event(INITIALIZATION_ERROR);
        set_state(ERROR);
        return err;
    }
    LOG_INF("BLE initialized succesfully. BLE device \"%s\" online!", CONFIG_BT_DEVICE_NAME);

    // Initialize sensors
    LOG_INF("Initializing the sensors...");
    err = init_sensors();
    if (err)
    {
        LOG_ERR("Error while initializing sensors (err %d)", err);
        dispatch_event(INITIALIZATION_ERROR);
        set_state(ERROR);
        return err;
    }
    LOG_INF("Sensors initialized succesfully.");

    // Initialize periodic task and time the first task in 10 seconds
    LOG_INF("Initializing the periodic task for measuring and advertising data...");
    k_work_init_delayable(&periodic_work, periodic_task);
    success = schedule_work_task(FIRST_TASK_DELAY);
    if (!success)
    {
        dispatch_event(INITIALIZATION_ERROR);
        set_state(ERROR);
        return -1;
    }
    LOG_INF("Periodic task initialized succesfully.");
    dispatch_event(INITIALIZATION_SUCCESS);
    idle();
    return 0;
}