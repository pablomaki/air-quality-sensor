#include <sensors.h>
#include <configs.h>
#include <variables.h>

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

LOG_MODULE_REGISTER(sensors);

#ifdef ENABLE_SHT4X
static const struct device *sht4x_dev_p;
static struct sensor_value temperature, humidity;
#endif

#ifdef ENABLE_SGP40
static const struct device *sgp40_dev_p;
static struct sensor_value voc_index;
#endif

#ifdef ENABLE_SCD4X
static const struct device *scd4x_dev_p;
static struct sensor_value co2_concentration;
#endif

#ifdef ENABLE_BMP390
static const struct device *bmp390_dev_p;
static struct sensor_value pressure;
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
#endif

#ifdef ENABLE_SCD4X
    scd4x_dev_p = DEVICE_DT_GET_ANY(sensirion_scd4x);
    if (!device_is_ready(scd4x_dev_p))
    {
        LOG_ERR("Device scd4x is not ready.");
        return -ENXIO;
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
    return 0;
}

#ifdef ENABLE_SHT4X
int read_sht4x_data()
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
    set_temperature(sensor_value_to_float(&temperature));
    set_humidity(sensor_value_to_float(&humidity));
    return 0;
}
#endif

#ifdef ENABLE_SGP40
int read_sgp40_data()
{
    int err, err2;
    err = sensor_attr_set(sgp40_dev_p, SENSOR_CHAN_GAS_RES, SENSOR_ATTR_SGP40_TEMPERATURE, &temperature);
    err2 = sensor_attr_set(sgp40_dev_p, SENSOR_CHAN_GAS_RES, SENSOR_ATTR_SGP40_HUMIDITY, &humidity);
    if (err || err2)
    {
        LOG_ERR("Failed to set compensation temperature and ro humidity (err %d, %d)", err, err2);
        return err;
    }

    err = sensor_sample_fetch(sgp40_dev_p);
    if (err)
    {
        LOG_ERR("Failed to fetch sample from SGP40 device (err %d)", err);
        return err;
    }

    err = sensor_channel_get(sgp40_dev_p, SENSOR_CHAN_GAS_RES, &voc_index);
    if (err)
    {
        LOG_ERR("Failed to get VOC idnex data (err %d)", err);
        return err;
    }

    // Save values
    set_voc_index(sensor_value_to_float(&voc_index));
    return 0;
}
#endif

#ifdef ENABLE_BMP390
int read_bmp390_data()
{
    int err;
    err = sensor_sample_fetch(bmp390_dev_p);
    if (err)
    {
        LOG_ERR("Failed to fetch sample from BMP390 device (err %d)", err);
        return err;
    }

    err = sensor_channel_get(bmp390_dev_p, SENSOR_CHAN_PRESS, &pressure);
    if (err)
    {
        LOG_ERR("Failed to get pressure data (err %d)", err);
        return err;
    }

    // Save values
    set_pressure(sensor_value_to_float(&pressure));
    return 0;
}
#endif

#ifdef ENABLE_SCD4X
int read_scd4x_data()
{
    int err, err2, err3;
    err = sensor_sample_fetch(scd4x_dev_p);
    if (err)
    {
        LOG_ERR("Failed to fetch sample from SCD4x device (err %d)", err);
        return err;
    }

    err = sensor_channel_get(scd4x_dev_p, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
    err2 = sensor_channel_get(scd4x_dev_p, SENSOR_CHAN_HUMIDITY, &humidity);
    err3 = sensor_channel_get(scd4x_dev_p, SENSOR_CHAN_CO2, &co2_concentration);
    if (err || err2 || err3)
    {
        LOG_ERR("Failed to get temperature, humidity or CO2 concentration data (err %d, %d, %d)", err, err2, err3);
        return err;
    }

    // Save values
    set_temperature(sensor_value_to_float(&temperature));
    set_humidity(sensor_value_to_float(&humidity));
    set_co2_concentration(sensor_value_to_float(&co2_concentration));
    return 0;
}
#endif