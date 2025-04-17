#include <air_quality_monitor.h>

#include <zephyr/logging/log.h>

#include <bluetooth_handler.h>
#include <e_ink_display.h>
#include <power_manager.h>
#include <configs.h>
#include <sensors.h>
#include <battery.h>
#include <variables.h>
#include <event_handler.h>
#include <state_handler.h>

LOG_MODULE_REGISTER(air_quality_monitor);

static struct k_work_delayable periodic_work;

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
    set_state(IDLE);
    return 0;
}

void periodic_task(struct k_work *work)
{
    LOG_INF("Periodic task begin");
    set_state(ACTIVE);

    // Log time for calculating correct time to sleep
    int64_t start_time_ms = k_uptime_get();

    int err;
    bool success;

    LOG_INF("Read sensors");
    success = read_sensors();
    if (!success)
    {
        dispatch_event(PERIODIC_TASK_WARNING);
    }

#ifdef ENABLE_EPD
    LOG_INF("Update displayed values.");
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

bool read_sensors(void)
{
    bool success = true;
    int err;
    // Get battery level
    err = read_battery_level();
    if (err)
    {
        LOG_ERR("Error reading battery level (err %d)", err);
        success = false;
    }

    // Get data from sensor(s)
#ifdef ENABLE_SHT4X
    err = read_sht4x_data();
    if (err)
    {
        LOG_ERR("Error reading sht4x sensor data (err %d)", err);
        success = false;
    }
#endif

#ifdef ENABLE_SGP40
    err = read_sgp4x_data();
    if (err)
    {
        LOG_ERR("Error reading sgp4x sensor data (err %d)", err);
        success = false;
    }
#endif

#ifdef ENABLE_BMP280
    err = read_bmp280_data();
    if (err)
    {
        LOG_ERR("Error reading bmp280 sensor data (err %d)", err);
        success = false;
    }
#endif

#ifdef ENABLE_SCD4X
    err = read_scd4x_data();
    if (err)
    {
        LOG_ERR("Error reading scd4x sensor data (err %d)", err);
        success = false;
    }
#endif
    return success;
}

bool advertise_data(void)
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

bool schedule_work_task(int64_t delay)
{
    int err;
    bool success = true;
    err = k_work_schedule(&periodic_work, K_MSEC(delay));
    if (err != 0 && err != 1)
    {
        LOG_ERR("Error scheduling a task (err %d)", err);
        success = false;
    }
    return success;
}

void ble_task_callback(bool task_success)
{
    if (task_success)
    {
        LOG_INF("BLE data transfer completed succesfully, entering idle state.");
        dispatch_event(PERIODIC_TASK_SUCCESS);
    }
    else{
        LOG_INF("BLE data transfer unsuccesful, entering idle state.");
        dispatch_event(PERIODIC_TASK_WARNING);

    }
    set_state(IDLE);
    enter_low_power_mode();
}