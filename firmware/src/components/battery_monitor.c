#include <components/battery_monitor.h>
#include <utils/variable_buffer.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(battery_monitor);

#define BATTERY_MANAGER_NODE DT_NODELABEL(xiao_nrf52_bm)

#define BM_ADC_CHANNEL SAADC_CH_PSELP_PSELP_AnalogInput7 // ADC channel
#define BM_ADC_REF ADC_REF_INTERNAL						 // Use internal reference voltage
#define BM_ADC_GAIN ADC_GAIN_1_6						 // Gain of 1/6 to fit range of 0 to 4.2V inside ADC reference voltage range of 0.6V
#define BM_ADC_ID 0                						 // ADC channel for battery monitor
#define ADC_SAMPLES_TOTAL 10                             // Number of samples to take for averaging
#define ADC_SAMPLE_INTERVAL_US 500                       // Microseconds
#define BM_ADC_RESOLUTION 12                             // ADC resolution in bits

static const struct device *adc_battery_manager_dev = DEVICE_DT_GET(DT_NODELABEL(adc));

// BM GPIO configurations
static const struct gpio_dt_spec enable_charging_gpios = GPIO_DT_SPEC_GET_OR(BATTERY_MANAGER_NODE, enable_charging_gpios, {0});
static const struct gpio_dt_spec read_gpios = GPIO_DT_SPEC_GET_OR(BATTERY_MANAGER_NODE, read_gpios, {0});
static const struct gpio_dt_spec charge_current_select_gpios = GPIO_DT_SPEC_GET_OR(BATTERY_MANAGER_NODE, charge_current_select_gpios, {0});

/**
 * @brief ADC channel configuration
 *
 */
static struct adc_channel_cfg bm_adc_config = {
	.gain = BM_ADC_GAIN,
	.reference = BM_ADC_REF,
	.acquisition_time = ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 10),
	.channel_id = BM_ADC_ID,
#ifdef CONFIG_ADC_NRFX_SAADC
	.input_positive = BM_ADC_CHANNEL,
#endif
};

/**
 * @brief ADC sequence options
 *
 */
static struct adc_sequence_options options = {
	.extra_samplings = ADC_SAMPLES_TOTAL - 1,
	.interval_us = ADC_SAMPLE_INTERVAL_US,
};

/**
 * @brief Buffer for ADC samples
 *
 */
static int16_t adc_sample_buffer[ADC_SAMPLES_TOTAL];

/**
 * @brief ADC sequence configuration
 *
 */
static struct adc_sequence sequence = {
	.options = &options,
	.channels = BIT(BM_ADC_ID),
	.buffer = adc_sample_buffer,
	.buffer_size = sizeof(adc_sample_buffer),
	.resolution = BM_ADC_RESOLUTION,
};

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
 * @brief Initial battery state
 *
 */
battery_state_t battery = {
	.voltage = 0,
	.percentage = 0.0,
};

/**
 * @brief Calculate the battery percentage based on the voltage
 *
 * @return int
 */
static int calculate_percentage(void)
{

	// Ensure voltage is within bounds
	if (battery.voltage >= battery_capacity_map[0].voltage)
	{
		battery.percentage = 100.0;
		return 0;
	}
	else if (battery.voltage <= battery_capacity_map[sizeof(battery_capacity_map) / sizeof(battery_capacity_map[0]) - 1].voltage)
	{
		battery.percentage = 0.0;
		return 0;
	}

	// Map the voltage to percentage
	for (uint16_t i = 0; i < sizeof(battery_capacity_map) / sizeof(battery_capacity_map[0]) - 1; i++)
	{
		uint16_t voltage_high = battery_capacity_map[i].voltage;
		uint16_t voltage_low = battery_capacity_map[i + 1].voltage;

		if (battery.voltage <= voltage_high && battery.voltage >= voltage_low)
		{
			uint8_t percentage_high = battery_capacity_map[i].percentage;
			uint8_t percentage_low = battery_capacity_map[i + 1].percentage;

			int32_t voltage_range = voltage_high - voltage_low;
			int32_t percentage_range = percentage_high - percentage_low;
			int32_t voltage_diff = battery.voltage - voltage_low;

			if (voltage_range == 0)
			{
				battery.percentage = percentage_high;
			}
			else
			{
				battery.percentage = percentage_low + percentage_range * voltage_diff / voltage_range;
			}
			return 0;
		}
	}

	// Voltage not in any defined range
	return -ESPIPE;
}

/**
 * @brief Get the battery charge level
 *
 * @return int
 */
static int battery_get_charge_lvl(void)
{

	int rc = 0;

	uint16_t adc_vref = adc_ref_internal(adc_battery_manager_dev);

	// Get ADC samples
	rc = adc_read(adc_battery_manager_dev, &sequence);
	if (rc)
	{
		LOG_WRN("ADC read failed (err %d)", rc);
		return rc;
	}

	// Get average sample value.
	uint32_t adc_sum = 0;
	for (uint8_t sample = 0; sample < ADC_SAMPLES_TOTAL; sample++)
	{
		adc_sum += adc_sample_buffer[sample];
	}
	uint32_t adc_average = adc_sum / ADC_SAMPLES_TOTAL;

	// Convert ADC value to millivolts
	uint32_t adc_mv = adc_average;
	rc = adc_raw_to_millivolts(adc_vref, BM_ADC_GAIN, BM_ADC_RESOLUTION, &adc_mv);
	if (rc)
	{
		LOG_WRN("ADC conversion to millivolts failed (err %d)", rc);
		return rc;
	}

	// Calculate battery voltage.
	float scale_factor = ((float)(CONFIG_VOLTAGE_DIVIDER_R1 + CONFIG_VOLTAGE_DIVIDER_R2)) / CONFIG_VOLTAGE_DIVIDER_R2;
	battery.voltage = (uint16_t)(adc_mv * scale_factor);

	// Get battery percentage.
	rc = calculate_percentage();
	if (rc)
	{
		LOG_ERR("Battery percentage calculation failed (err %d)", rc);
		return rc;
	}
	return 0;
}

int init_battery_monitor()
{
	int rc = 0;

	// ADC setup
	if (!device_is_ready(adc_battery_manager_dev))
	{
		LOG_ERR("ADC device not found!");
		return -EIO;
	}
	rc = adc_channel_setup(adc_battery_manager_dev, &bm_adc_config);
	if (rc)
	{
		LOG_ERR("ADC setup failed (err %d)", rc);
		return rc;
	}

	// GPIO setup
	if (!gpio_is_ready_dt(&enable_charging_gpios))
	{
		LOG_ERR("GPIO enable_charging_gpios not ready");
		return -EIO;
	}

	if (!gpio_is_ready_dt(&read_gpios))
	{
		LOG_ERR("GPIO read_gpios not ready");
		return -EIO;
	}

	if (!gpio_is_ready_dt(&charge_current_select_gpios))
	{
		LOG_ERR("GPIO charge_current_select_gpios not ready");
		return -EIO;
	}

	rc = gpio_pin_configure_dt(&enable_charging_gpios, GPIO_INPUT | GPIO_ACTIVE_LOW);
	if (rc)
	{
		LOG_ERR("GPIO configuration for chargeing enable pin failed (err %d)", rc);
		return rc;
	}

	rc = gpio_pin_configure_dt(&read_gpios, GPIO_OUTPUT | GPIO_ACTIVE_LOW);
	if (rc)
	{
		LOG_ERR("GPIO configuration for battery voltage read pin failed (err %d)", rc);
		return rc;
	}

	rc = gpio_pin_configure_dt(&charge_current_select_gpios, GPIO_OUTPUT | GPIO_ACTIVE_LOW);
	if (rc)
	{
		LOG_ERR("GPIO configuration for charge current select pin failed (err %d)", rc);
		return rc;
	}

	rc = gpio_pin_set_dt(&charge_current_select_gpios, CONFIG_USE_FAST_CHARGING);
	if (rc)
	{
		LOG_ERR("Failed to set charge current (err %d)", rc);
		return rc;
	}

	rc = gpio_pin_set_dt(&read_gpios, 1);
	if (rc)
	{
		LOG_ERR("Failed to enable battery read (err %d)", rc);
		return rc;
	}

	return 0;
}

int read_battery_level()
{
	int rc = battery_get_charge_lvl();
	if (rc)
	{
		LOG_ERR("Failed to fetch sample for battery percentage err (%d)", rc);
		return rc;
	}

	set_value(BATTERY_LEVEL, battery.percentage);
	LOG_INF("Battery charge: %d %%", (uint8_t)battery.percentage);
	LOG_INF("Battery voltage: %d mV", battery.voltage);
	return 0;
}