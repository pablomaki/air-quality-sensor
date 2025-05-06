#include <components/sensors.h>
#include <configs.h>
#include <utils/variables.h>

#include <sensirion_gas_index_algorithm.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

#ifdef ENABLE_SHT4X
#include <zephyr/drivers/sensor/sht4x.h>
#endif

#ifdef ENABLE_SGP40
#include <zephyr/drivers/sensor/sgp40.h>
#endif

#ifdef ENABLE_SCD4X
#include <drivers/scd4x.h>
#endif

LOG_MODULE_REGISTER(sensors);

#ifdef ENABLE_SHT4X
static const struct device *sht4x_dev_p;
static struct sensor_value temperature, humidity;
#endif

#ifdef ENABLE_SGP40
static const struct device *sgp40_dev_p;
static struct sensor_value voc_raw, voc_index;
static GasIndexAlgorithmParams voc_params;
#endif

#ifdef ENABLE_SCD4X
static const struct device *scd4x_dev_p;
static struct sensor_value co2_concentration, temperature_2, humidity_2;
static struct sensor_value asc_initial_period = {(2 * 24 * 60 * 60) / (MEASUREMENT_INTERVAL / 1000) / 12, 0};
static struct sensor_value asc_standard_period = {(7 * 24 * 60 * 60) / (MEASUREMENT_INTERVAL / 1000) / 12, 0};
static struct sensor_value sensor_altitude = {ALTITUDE, 0};
static struct sensor_value temperature_offset = {TEMPERATURE_OFFSET, 0};
#endif

#ifdef ENABLE_BMP390
static const struct device *bmp390_dev_p;
static struct sensor_value pressure, temperature_3;
#endif

int init_sensors(void)
{
#ifdef ENABLE_SHT4X
    sht4x_dev_p = DEVICE_DT_GET_ANY(sensirion_sht4x);
    if (!device_is_ready(sht4x_dev_p))
    {
        LOG_ERR("Device sht4x is not ready.");
        return -ENXIO;
    }
#endif

#ifdef ENABLE_SGP40
    sgp40_dev_p = DEVICE_DT_GET_ANY(sensirion_sgp40);
    if (!device_is_ready(sgp40_dev_p))
    {
        LOG_ERR("Device sgp40 is not ready.");
        return -ENXIO;
    }
    GasIndexAlgorithm_init_with_sampling_interval(&voc_params, GasIndexAlgorithm_ALGORITHM_TYPE_VOC, MEASUREMENT_INTERVAL / 1000);
#endif

#ifdef ENABLE_SCD4X
    scd4x_dev_p = DEVICE_DT_GET_ANY(sensirion_scd41);
    if (!device_is_ready(scd4x_dev_p))
    {
        LOG_ERR("Device scd4x is not ready.");
        return -ENXIO;
    }
    int err, err2, err3, err4;
    err = sensor_attr_set(scd4x_dev_p, SENSOR_CHAN_CO2, SENSOR_ATTR_SCD4X_SELF_CALIB_INITIAL_PERIOD, &asc_initial_period);
    err2 = sensor_attr_set(scd4x_dev_p, SENSOR_CHAN_CO2, SENSOR_ATTR_SCD4X_SELF_CALIB_STANDARD_PERIOD, &asc_standard_period);
    err3 = sensor_attr_set(scd4x_dev_p, SENSOR_CHAN_CO2, SENSOR_ATTR_SCD4X_ALTITUDE, &sensor_altitude);
    err4 = sensor_attr_set(scd4x_dev_p, SENSOR_CHAN_CO2, SENSOR_ATTR_SCD4X_TEMPERATURE_OFFSET, &temperature_offset);
    if (err || err2 || err3 || err4)
    {
        LOG_ERR("Failed to set scd4x initial asc period, standard asc period or  (err %d, %d, %d %d)", err, err2, err3, err4);
        return err;
    }
#endif

#ifdef ENABLE_BMP390
    bmp390_dev_p = DEVICE_DT_GET_ANY(bosch_bmp390);
    if (!device_is_ready(bmp390_dev_p))
    {
        LOG_ERR("Device bmp390 is not ready.");
        return -ENXIO;
    }
#endif

    // Initialize the buffers for the sensor values
    err = init_buffers(MEASUREMENTS_PER_INTERVAL);
    if (err)
    {
        LOG_ERR("Failed to initialize sensor value buffers (err %d)", err);
        return -ENXIO;
    }

    return 0;
}

#ifdef ENABLE_SHT4X
int read_sht4x_data(uint8_t index)
{
    int err, err2;
    err = sensor_sample_fetch(sht4x_dev_p);
    if (err)
    {
        LOG_ERR("Failed to fetch sample from SHT4X device (err %d)", err);
        return err;
    }

    err = sensor_channel_get(sht4x_dev_p, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
    err2 = sensor_channel_get(sht4x_dev_p, SENSOR_CHAN_HUMIDITY, &humidity);
    if (err)
    {
        LOG_ERR("Failed to get temperature or humidity data (err %d, %d)", err, err2);
        return err;
    }

    // Save values
    set_value(TEMPERATURE, sensor_value_to_float(&temperature));
    set_value(HUMIDITY, sensor_value_to_float(&humidity));
    LOG_INF("SHT4X temperature: %d.%d °C", temperature.val1, temperature.val2);
    LOG_INF("SHT4X humidity: %d.%d %%RH", humidity.val1, humidity.val2);
    return 0;
}
#endif

#ifdef ENABLE_SGP40
int read_sgp40_data(uint8_t index)
{
    int err, err2;
    err = sensor_attr_set(sgp40_dev_p, SENSOR_CHAN_GAS_RES, SENSOR_ATTR_SGP40_TEMPERATURE, &temperature);
    err2 = sensor_attr_set(sgp40_dev_p, SENSOR_CHAN_GAS_RES, SENSOR_ATTR_SGP40_HUMIDITY, &humidity);
    if (err || err2)
    {
        LOG_ERR("Failed to set compensation temperature and or humidity (err %d, %d)", err, err2);
        return err;
    }

    err = sensor_sample_fetch(sgp40_dev_p);
    if (err)
    {
        LOG_ERR("Failed to fetch sample from SGP40 device (err %d)", err);
        return err;
    }

    err = sensor_channel_get(sgp40_dev_p, SENSOR_CHAN_GAS_RES, &voc_raw);
    if (err)
    {
        LOG_ERR("Failed to get VOC idnex data (err %d)", err);
        return err;
    }
    GasIndexAlgorithm_process(&voc_params, voc_raw.val1, &voc_index.val1);

    // Save values
    set_value(VOC_INDEX, sensor_value_to_float(&voc_index));
    LOG_INF("SGP40 VOC index (0 - 500): %d.%d", voc_index.val1, voc_index.val2);
    return 0;
}
#endif

#ifdef ENABLE_BMP390
int read_bmp390_data(uint8_t index)
{
    int err;
    err = sensor_sample_fetch(bmp390_dev_p);
    if (err)
    {
        LOG_ERR("Failed to fetch sample from BMP390 device (err %d)", err);
        return err;
    }

    err = sensor_channel_get(bmp390_dev_p, SENSOR_CHAN_PRESS, &pressure);
    err = sensor_channel_get(bmp390_dev_p, SENSOR_CHAN_AMBIENT_TEMP, &temperature_3);
    if (err)
    {
        LOG_ERR("Failed to get pressure data (err %d)", err);
        return err;
    }

    // Save values
    set_value(PRESSURE, sensor_value_to_float(&pressure));
    LOG_INF("BMP390 pressure: %d.%d hPa", pressure.val1 / 100, (pressure.val1 % 100) + pressure.val2 / 100);
    LOG_INF("BMP390 temperature: %d.%d °C", temperature_3.val1, temperature_3.val2);
    return 0;
}
#endif

#ifdef ENABLE_SCD4X
int read_scd4x_data(uint8_t index)
{
    int err, err2, err3;

#ifdef ENABLE_BMP390
    err = sensor_attr_set(scd4x_dev_p, SENSOR_CHAN_CO2, SENSOR_ATTR_SCD4X_AMBIENT_PRESSURE, &pressure);
    if (err)
    {
        LOG_ERR("Failed to set pressure compensation (err %d)", err);
        return err;
    }
#endif

    err = sensor_sample_fetch(scd4x_dev_p);
    if (err)
    {
        LOG_ERR("Failed to fetch sample from SCD4x device (err %d)", err);
        return err;
    }

    err = sensor_channel_get(scd4x_dev_p, SENSOR_CHAN_CO2, &co2_concentration);
    err2 = sensor_channel_get(scd4x_dev_p, SENSOR_CHAN_AMBIENT_TEMP, &temperature_2);
    err3 = sensor_channel_get(scd4x_dev_p, SENSOR_CHAN_HUMIDITY, &humidity_2);
    if (err || err2 || err3)
    {
        LOG_ERR("Failed to get CO2, temperature or humidity concentration data (err %d, %d, %d)", err, err2, err3);
        return err;
    }

    // Save values
    set_value(CO2_CONCENTRATION, sensor_value_to_float(&co2_concentration));
#ifndef ENABLE_SHT4X
    set_value(TEMPERATURE, sensor_value_to_float(&temperature_2));
    set_value(HUMIDITY, sensor_value_to_float(&humidity_2));
#endif
    LOG_INF("SCD4X CO2 concentration: %d.%d ppm", co2_concentration.val1, co2_concentration.val2);
    LOG_INF("SCD4X temperature: %d.%d °C", temperature_2.val1, temperature_2.val2);
    LOG_INF("SCD4X humidity: %d.%d %%RH", humidity_2.val1, humidity_2.val2);
    return 0;
}
#endif