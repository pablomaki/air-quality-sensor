#include <components/battery_monitor.h>
#include <utils/variable_buffer.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/adc/voltage_divider.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(battery_monitor);

#define BQ25100_NODE DT_NODELABEL(bq25100)

// BM GPIO configurations
static const struct gpio_dt_spec enable_led_gpios = GPIO_DT_SPEC_GET_OR(BQ25100_NODE, enable_led_gpios, {0});
static const struct gpio_dt_spec enable_read_gpios = GPIO_DT_SPEC_GET_OR(BQ25100_NODE, enable_read_gpios, {0});
static const struct gpio_dt_spec charge_current_select_gpios = GPIO_DT_SPEC_GET_OR(BQ25100_NODE, charge_current_select_gpios, {0});

// Battery voltage divider device and voltage reading
static const struct device *voltage_divider;
static struct sensor_value battery_voltage;

typedef struct
{
	uint16_t voltage; // millivolts
	float percentage; // percentage
} battery_state_t;

/**
 * @brief Battery capacity map for each supported battery type (estimate, needs to be tuned)
 *
 */
static battery_state_t battery_capacity_map[11] = {
	{4200, 100.0}, // Fully charged
	{4060, 90.0},
	{4030, 80.0},
	{3920, 70.0},
	{3830, 60.0},
	{3750, 50.0},
	{3670, 40.0},
	{3550, 30.0},
	{3460, 20.0},
	{3270, 10.0},
	{3060, 0.0} // Minimum safe voltage
};

/**
 * @brief Calculate the battery percentage based on the voltage
 *
 * @return int
 */
static int calculate_percentage(const struct sensor_value* voltage, float *percentage)
{
	uint16_t battery_voltage_mv = sensor_value_to_milli(voltage);

	// Ensure voltage is within bounds
	if (battery_voltage_mv >= battery_capacity_map[0].voltage)
	{
		*percentage = 100.0;
		return 0;
	}
	else if (battery_voltage_mv <= battery_capacity_map[sizeof(battery_capacity_map) / sizeof(battery_capacity_map[0]) - 1].voltage)
	{
		*percentage = 0.0;
		return 0;
	}

	// Map the voltage to percentage
	for (uint16_t i = 0; i < sizeof(battery_capacity_map) / sizeof(battery_capacity_map[0]) - 1; i++)
	{
		uint16_t voltage_high = battery_capacity_map[i].voltage;
		uint16_t voltage_low = battery_capacity_map[i + 1].voltage;

		if (battery_voltage_mv <= voltage_high && battery_voltage_mv >= voltage_low)
		{
			uint8_t percentage_high = battery_capacity_map[i].percentage;
			uint8_t percentage_low = battery_capacity_map[i + 1].percentage;

			int32_t voltage_range = voltage_high - voltage_low;
			int32_t percentage_range = percentage_high - percentage_low;
			int32_t voltage_diff = battery_voltage_mv - voltage_low;

			if (voltage_range == 0)
			{
				*percentage = percentage_high;
			}
			else
			{
				*percentage = percentage_low + percentage_range * voltage_diff / voltage_range;
			}
			return 0;
		}
	}

	// Voltage not in any defined range
	return -ESPIPE;
}

int init_battery_monitor()
{
	int rc = 0;

	// Voltage divider ready check
    voltage_divider = DEVICE_DT_GET_ANY(voltage_divider);
	if (!device_is_ready(voltage_divider))
	{
		LOG_ERR("Device voltage-divider is not ready.");
		return -ENXIO;
	}

	// GPIO setup
	if (!gpio_is_ready_dt(&enable_led_gpios))
	{
		LOG_ERR("GPIO enable_led_gpios not ready.");
		return -EIO;
	}

	if (!gpio_is_ready_dt(&enable_read_gpios))
	{
		LOG_ERR("GPIO enable_read_gpios not ready.");
		return -EIO;
	}

	if (!gpio_is_ready_dt(&charge_current_select_gpios))
	{
		LOG_ERR("GPIO charge_current_select_gpios not ready.");
		return -EIO;
	}

	rc = gpio_pin_configure_dt(&enable_led_gpios, GPIO_INPUT | GPIO_ACTIVE_LOW);
	if (rc != 0)
	{
		LOG_ERR("GPIO configuration for led enable pin failed (err %d).", rc);
		return rc;
	}

	rc = gpio_pin_configure_dt(&enable_read_gpios, GPIO_OUTPUT | GPIO_ACTIVE_LOW);
	if (rc != 0)
	{
		LOG_ERR("GPIO configuration for enable battery voltage read pin failed (err %d).", rc);
		return rc;
	}

	rc = gpio_pin_configure_dt(&charge_current_select_gpios, GPIO_OUTPUT | GPIO_ACTIVE_LOW);
	if (rc != 0)
	{
		LOG_ERR("GPIO configuration for charge current select pin failed (err %d).", rc);
		return rc;
	}

	rc = gpio_pin_set_dt(&charge_current_select_gpios, CONFIG_USE_FAST_CHARGING);
	if (rc != 0)
	{
		LOG_ERR("Failed to set charge current (err %d).", rc);
		return rc;
	}

	rc = gpio_pin_set_dt(&enable_read_gpios, 1);
	if (rc != 0)
	{
		LOG_ERR("Failed to enable battery read (err %d).", rc);
		return rc;
	}

	return 0;
}

int read_battery_level()
{
	int rc = 0;

	// Get ADC samples
	rc = sensor_sample_fetch(voltage_divider);
	if (rc != 0)
	{
		LOG_ERR("Failed to fetch sample from voltage divider (err %d).", rc);
		set_value(BATTERY_LEVEL, -1.0f); // Error indicator
		return rc;
	}

	rc = sensor_channel_get(voltage_divider, SENSOR_CHAN_VOLTAGE, &battery_voltage);
	if (rc != 0)
	{
		LOG_ERR("Failed to get voltage from voltage divider (err %d).", rc);
		set_value(BATTERY_LEVEL, -1.0f); // Error indicator
		return rc;
	}

	// Get battery percentage.
	float percentage = 0.0;
	rc = calculate_percentage(&battery_voltage, &percentage);
	if (rc != 0)
	{
		LOG_ERR("Battery percentage calculation failed (err %d).", rc);
		set_value(BATTERY_LEVEL, -1.0f); // Error indicator
		return rc;
	}

	set_value(BATTERY_LEVEL, percentage);
	LOG_INF("Battery charge: %d %%", (uint8_t)percentage);
	LOG_INF("Battery voltage: %lld mV", sensor_value_to_milli(&battery_voltage));
	return 0;
}