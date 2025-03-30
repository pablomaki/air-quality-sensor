#include <variables.h>
#include <zephyr/types.h>

static float battery_level = 0.0;						// battery level variable
static float temperature = 0.0;							// temperature variable
static float humidity = 0.0;							// humidity variable
static float pressure = 0.0;							// pressure variable
static float co2_concentration = 0.0;					// CO2 concentration variable
static float voc_index = 0.0;							// VOC index variable
static state_t state = NOT_SET;							// Global bluetooth state variable

/**
 * @brief Pointer to the bluetooth state change callback function
 *
 */
static state_change_cb_t state_change_callback = NULL;

void set_state(state_t new_state)
{
	if (state != new_state)
	{
		state = new_state;
		if (state_change_callback)
		{
			state_change_callback(state);
		}
	}
}

state_t get_state(void)
{
	return state;
}

void register_state_callback(state_change_cb_t cb)
{
	state_change_callback = cb;
}

void set_battery_level(float new_battery_level)
{
	battery_level = new_battery_level;
}

float get_battery_level(void)
{
	return battery_level;
}

void set_temperature(float new_temperature)
{
	temperature = new_temperature;
}

float get_temperature(void)
{
	return temperature;
}

void set_humidity(float new_humidity)
{
	humidity = new_humidity;
}

float get_humidity(void)
{
	return humidity;
}

void set_pressure(float new_pressure)
{
	pressure = new_pressure;
}

float get_pressure(void)
{
	return pressure;
}

void set_co2_concentration(float new_co2_concentration)
{
	co2_concentration = new_co2_concentration;
}

float get_co2_concentration(void)
{
	return co2_concentration;
}

void set_voc_index(float new_voc_index)
{
	voc_index = new_voc_index;
}

float get_voc_index(void)
{
	return voc_index;
}