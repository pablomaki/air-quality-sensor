#include <variables.h>
#include <configs.h>

static float battery_level[MEASUREMENTS_PER_INTERVAL] = {0.0};
static float temperature[MEASUREMENTS_PER_INTERVAL] = {0.0};
static float humidity[MEASUREMENTS_PER_INTERVAL] = {0.0};
static float pressure[MEASUREMENTS_PER_INTERVAL] = {0.0};
static float co2_concentration[MEASUREMENTS_PER_INTERVAL] = {0.0};
static float voc_index[MEASUREMENTS_PER_INTERVAL] = {0.0};

/**
 * @brief Get the mean object
 *
 * @param arr Array of floats
 * @return float Mean of the array
 */
static float get_mean(float *arr)
{
	float sum = 0.0f;
	for (int i = 0; i < MEASUREMENTS_PER_INTERVAL; i++)
	{
		sum += arr[i];
	}
	return sum / MEASUREMENTS_PER_INTERVAL;
}

void set_battery_level(float new_battery_level, uint8_t index)
{
	battery_level[index] = new_battery_level;
}

float get_battery_level(void)
{
	return get_mean(battery_level);
}

void set_temperature(float new_temperature, uint8_t index)
{
	temperature[index] = new_temperature;
}

float get_temperature(void)
{
	return get_mean(temperature);
}

void set_humidity(float new_humidity, uint8_t index)
{
	humidity[index] = new_humidity;
}

float get_humidity(void)
{
	return get_mean(humidity);
}

void set_pressure(float new_pressure, uint8_t index)
{
	pressure[index] = new_pressure;
}

float get_pressure(void)
{
	return get_mean(pressure);
}

void set_co2_concentration(float new_co2_concentration, uint8_t index)
{
	co2_concentration[index] = new_co2_concentration;
}

float get_co2_concentration(void)
{
	return get_mean(co2_concentration);
}

void set_voc_index(float new_voc_index, uint8_t index)
{
	voc_index[index] = new_voc_index;
}

float get_voc_index(void)
{
	return get_mean(voc_index);
}