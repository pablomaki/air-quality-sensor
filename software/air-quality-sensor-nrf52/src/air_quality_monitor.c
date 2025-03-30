#include <air_quality_monitor.h>

#include <zephyr/logging/log.h>

#include <bluetooth_handler.h>
#include <led_controller.h>
#include <e_ink_display.h>
#include <configs.h>
#include <sensors.h>
#include <battery.h>
#include <variables.h>

LOG_MODULE_REGISTER(air_quality_monitor);

static struct k_work_delayable periodic_work;

int init_air_quality_monitor(void)
{
	int err;

	// Initialize LED controller
	LOG_INF("Initializing LED controller...");
	err = init_led_controller();
	if (err)
	{
		LOG_ERR("Error while initializing LED controller (err %d)", err);
		return err;
	}
	LOG_INF("LED controller initialized succesfully.");
	set_state(INITIALIZING);

	// Initialize E-ink display
	LOG_INF("Initializing E-ink display...");
	err = init_e_ink_display();
	if (err)
	{
		LOG_ERR("Error while initializing E-ink display (err %d)", err);
		return err;
	}
	LOG_INF("E-ink display initialized succesfully.");

	// Initialize bluetooth
	LOG_INF("Initializing BLE...");
	err = init_ble(stop_advertising_cb);
	if (err)
	{
		LOG_ERR("Error while initializing BLE (err %d)", err);
		return err;
	}
	LOG_INF("BLE initialized succesfully. BLE device \"%s\" online!", CONFIG_BT_DEVICE_NAME);

	// Initialize sensors
	LOG_INF("Initializing the sensors...");
	err = init_sensors();
	if (err)
	{
		LOG_ERR("Error while initializing sensors (err %d)", err);
		return err;
	}
	LOG_INF("Sensors initialized succesfully.");

	// Initialize periodic task
	LOG_INF("Initializing the periodic task for measuring and advertising data...");
	k_work_init_delayable(&periodic_work, periodic_task);
	err = k_work_schedule(&periodic_work, K_MSEC(1000));
	if (err != 1)
	{
		LOG_ERR("Error scheduling a task (err %d)", err);
		return err;
	}
	LOG_INF("Periodic task initialized succesfully.");
	set_state(STANDBY);
	return 0;
}

void periodic_task(struct k_work *work)
{
	LOG_INF("Periodic task begin");

	// Log time for calculating correct time to sleep
	int64_t start_time_ms = k_uptime_get();

	int err;

	LOG_INF("Read sensors");
	set_state(MEASURING);
	// Get battery level
	err = read_battery_level();
	if (err)
	{
		LOG_ERR("Error reading battery level (err %d)", err);
	}

	// Get data from sensor(s)
#ifdef ENABLE_SHT4X
	err = read_sht4x_data();
	if (err)
	{
		LOG_ERR("Error reading sht4x sensor data (err %d)", err);
	}
#endif

#ifdef ENABLE_SGP40
	err = read_sgp4x_data();
	if (err)
	{
		LOG_ERR("Error reading sgp4x sensor data (err %d)", err);
	}
#endif

#ifdef ENABLE_BMP280
	err = read_bmp280_data();
	if (err)
	{
		LOG_ERR("Error reading bmp280 sensor data (err %d)", err);
	}
#endif

#ifdef ENABLE_SCD4X
	err = read_scd4x_data();
	if (err)
	{
		LOG_ERR("Error reading scd4x sensor data (err %d)", err);
	}
#endif

	LOG_INF("Update displayed values.");
	err = update_e_ink_display();
	if (err)
	{
		LOG_ERR("Error updating E-ink display (err %d)", err);
	}

	LOG_INF("Update advertised data.");
	// Update and advertise data
	err = update_advertisement_data();
	if (err)
	{
		LOG_ERR("Error updating advertisement data (err %d)", err);
	}

	if (!ble_connection_exists())
	{
		LOG_INF("Begin advertising for connection.");
		set_state(ADVERTISING);
		err = start_advertise();
		if (err)
		{
			LOG_ERR("Error advertising data (err %d)", err);
		}
	}

	LOG_INF("Periodic task done, scheduling a new task");

	// Re-schedule the task to run again after WAKEUP_INTERVAL_MS
	int64_t complete_time_ms = k_uptime_get();
	int64_t delay = DATA_INTERVAL - (complete_time_ms - start_time_ms);

	if (delay < 0)
	{
		LOG_ERR("Missed deadline, scheduling immediately!");
		delay = 0; // Prevent negative delay
	}

	err = k_work_schedule(&periodic_work, K_MSEC(delay));
	if (err != 0 && err != 1)
	{
		LOG_ERR("Error scheduling a task (err %d)", err);
	}

	if (get_state() != ADVERTISING)
	{
		LOG_INF("Task scheduled, entering idle state.");
		set_state(STANDBY);
	}
	else
	{
		LOG_INF("Task scheduled, idle state after advertising stops.");
	}
}

void stop_advertising_cb(void)
{
	LOG_INF("Advertising stopped, entering idle state.");
	set_state(STANDBY);
}