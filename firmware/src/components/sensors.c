#include <components/sensors.h>
#include <utils/variable_buffer.h>

#include <sensirion_gas_index_algorithm.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>

LOG_MODULE_REGISTER(sensors);

#ifdef CONFIG_ENABLE_SHT4X
#include <zephyr/drivers/sensor/sht4x.h>
static const struct device *sht4x_dev_p;
static struct sensor_value temperature, humidity;
#endif

#ifdef CONFIG_ENABLE_SGP40
#include <zephyr/drivers/sensor/sgp40.h>
static const struct device *sgp40_dev_p;
static struct sensor_value voc_raw, voc_index;
static GasIndexAlgorithmParams voc_params;
#endif

#ifdef CONFIG_ENABLE_SCD4X
#include <drivers/scd4x.h>
static const struct device *scd4x_dev_p;
static struct sensor_value co2_concentration, temperature_2, humidity_2;
static struct sensor_value asc_initial_period = {(2 * 24 * 60 * 60) / (CONFIG_ADVERTISEMENT_INTERVAL / CONFIG_MEASUREMENTS_PER_INTERVAL / 1000) / 12, 0};
static struct sensor_value asc_standard_period = {(7 * 24 * 60 * 60) / (CONFIG_ADVERTISEMENT_INTERVAL / CONFIG_MEASUREMENTS_PER_INTERVAL / 1000) / 12, 0};
static struct sensor_value sensor_altitude = {CONFIG_SCD4X_ALTITUDE, 0};
static struct sensor_value temperature_offset = {CONFIG_SCD4X_TEMPERATURE_OFFSET, 0};
#endif

#ifdef CONFIG_ENABLE_BMP390
#include <drivers/bmp390.h>
static const struct device *bmp390_dev_p;
static struct sensor_value pressure, temperature_3;
#endif

int init_sensors(void)
{
    int rc = 0;

#ifdef CONFIG_ENABLE_SHT4X
    sht4x_dev_p = DEVICE_DT_GET_ANY(sensirion_sht4x);
    if (!device_is_ready(sht4x_dev_p))
    {
        LOG_ERR("Device sht4x is not ready.");
        return -ENXIO;
    }
#endif

#ifdef CONFIG_ENABLE_SGP40
    sgp40_dev_p = DEVICE_DT_GET_ANY(sensirion_sgp40);
    if (!device_is_ready(sgp40_dev_p))
    {
        LOG_ERR("Device sgp40 is not ready.");
        return -ENXIO;
    }
    GasIndexAlgorithm_init_with_sampling_interval(&voc_params, GasIndexAlgorithm_ALGORITHM_TYPE_VOC, CONFIG_ADVERTISEMENT_INTERVAL / CONFIG_MEASUREMENTS_PER_INTERVAL / 1000);
#endif

#ifdef CONFIG_ENABLE_SCD4X
    scd4x_dev_p = DEVICE_DT_GET_ANY(sensirion_scd41);
    if (!device_is_ready(scd4x_dev_p))
    {
        LOG_ERR("Device scd4x is not ready.");
        return -ENXIO;
    }
    rc = sensor_attr_set(scd4x_dev_p, SENSOR_CHAN_CO2, SENSOR_ATTR_SCD4X_SELF_CALIB_INITIAL_PERIOD, &asc_initial_period);
    if (rc != 0)
    {
        LOG_ERR("Failed to set scd4x asc initial period (err %d).", rc);
        return rc;
    }
    rc = sensor_attr_set(scd4x_dev_p, SENSOR_CHAN_CO2, SENSOR_ATTR_SCD4X_SELF_CALIB_STANDARD_PERIOD, &asc_standard_period);
    if (rc != 0)
    {
        LOG_ERR("Failed to set scd4x asc standard period (err %d).", rc);
        return rc;
    }
    rc = sensor_attr_set(scd4x_dev_p, SENSOR_CHAN_CO2, SENSOR_ATTR_SCD4X_ALTITUDE, &sensor_altitude);
    if (rc != 0)
    {
        LOG_ERR("Failed to set scd4x sensor altitude (err %d).", rc);
        return rc;
    }
    rc = sensor_attr_set(scd4x_dev_p, SENSOR_CHAN_CO2, SENSOR_ATTR_SCD4X_TEMPERATURE_OFFSET, &temperature_offset);
    if (rc != 0)
    {
        LOG_ERR("Failed to set scd4x sensor temperature offset (err %d).", rc);
        return rc;
    }
#endif

#ifdef CONFIG_ENABLE_BMP390
    bmp390_dev_p = DEVICE_DT_GET_ANY(bosch_bmp390);
    if (!device_is_ready(bmp390_dev_p))
    {
        LOG_ERR("Device bmp390 is not ready.");
        return -ENXIO;
    }
#endif

    // Initialize the buffers for the sensor values
    rc = init_buffers(CONFIG_MEASUREMENTS_PER_INTERVAL);
    if (rc != 0)
    {
        LOG_ERR("Failed to initialize sensor value buffers (err %d).", rc);
        return -ENXIO;
    }

    return 0;
}

#ifdef CONFIG_ENABLE_SHT4X
/**
 * @brief Read SHT4X sensor data and save the temperature and humidity to the variables
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
static int read_sht4x_data()
{
    int rc = 0;
    rc = sensor_sample_fetch(sht4x_dev_p);
    if (rc != 0)
    {
        LOG_ERR("Failed to fetch sample from SHT4X device (err %d).", rc);
        set_value(TEMPERATURE, -1.0f); // Error indicator
        set_value(HUMIDITY, -1.0f); // Error indicator
        return rc;
    }

    rc = sensor_channel_get(sht4x_dev_p, SENSOR_CHAN_AMBIENT_TEMP, &temperature);
    if (rc != 0)
    {
        LOG_ERR("Failed to get temperature data (err %d).", rc);
        set_value(TEMPERATURE, -1.0f); // Error indicator
        set_value(HUMIDITY, -1.0f); // Error indicator
        return rc;
    }
    rc = sensor_channel_get(sht4x_dev_p, SENSOR_CHAN_HUMIDITY, &humidity);
    if (rc != 0)
    {
        LOG_ERR("Failed to get humidity data (err %d).", rc);
        set_value(TEMPERATURE, -1.0f); // Error indicator
        set_value(HUMIDITY, -1.0f); // Error indicator
        return rc;
    }

    // Save values
    set_value(TEMPERATURE, sensor_value_to_float(&temperature));
    set_value(HUMIDITY, sensor_value_to_float(&humidity));
    LOG_INF("SHT4X temperature: %d.%d °C", temperature.val1, temperature.val2);
    LOG_INF("SHT4X humidity: %d.%d %%RH", humidity.val1, humidity.val2);
    return 0;
}
#endif

#ifdef CONFIG_ENABLE_SGP40
/**
 * @brief Read SGP40 sensor data and save the VOC index to the variables
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
static int read_sgp40_data()
{
    int rc = 0;
    rc = sensor_attr_set(sgp40_dev_p, SENSOR_CHAN_GAS_RES, SENSOR_ATTR_SGP40_TEMPERATURE, &temperature);
    if (rc != 0)
    {
        LOG_ERR("Failed to set temperature compensation (err %d).", rc);
        set_value(VOC_INDEX, -1.0f); // Error indicator
        return rc;
    }
    rc = sensor_attr_set(sgp40_dev_p, SENSOR_CHAN_GAS_RES, SENSOR_ATTR_SGP40_HUMIDITY, &humidity);
    if (rc != 0)
    {
        LOG_ERR("Failed to set humidity compensation (err %d).", rc);
        set_value(VOC_INDEX, -1.0f); // Error indicator
        return rc;
    }

    rc = sensor_sample_fetch(sgp40_dev_p);
    if (rc != 0)
    {
        LOG_ERR("Failed to fetch sample from SGP40 device (err %d).", rc);
        set_value(VOC_INDEX, -1.0f); // Error indicator
        return rc;
    }

    rc = sensor_channel_get(sgp40_dev_p, SENSOR_CHAN_GAS_RES, &voc_raw);
    if (rc != 0)
    {
        LOG_ERR("Failed to get VOC idnex data (err %d).", rc);
        set_value(VOC_INDEX, -1.0f); // Error indicator
        return rc;
    }
    GasIndexAlgorithm_process(&voc_params, voc_raw.val1, &voc_index.val1);

    // Save values
    set_value(VOC_INDEX, sensor_value_to_float(&voc_index));
    LOG_INF("SGP40 VOC raw: %d.%d", voc_raw.val1, voc_raw.val2);
    LOG_INF("SGP40 VOC index (0 - 500): %d.%d", voc_index.val1, voc_index.val2);
    return 0;
}

/**
 * @brief Warm up the SGP40 sensor by doing a mock measurement without using the result
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
static int warm_up_sgp40()
{
    int rc = 0;
    rc = sensor_sample_fetch(sgp40_dev_p);
    if (rc != 0)
    {
        LOG_ERR("Failed to fetch sample from SGP40 device (err %d).", rc);
        return rc;
    }
    return 0;
}
#endif

#ifdef CONFIG_ENABLE_BMP390
/**
 * @brief Read BMP390 sensor data and save the pressure to the variables
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
static int read_bmp390_data()
{
    int rc = 0;
    rc = sensor_sample_fetch(bmp390_dev_p);
    if (rc != 0)
    {
        LOG_ERR("Failed to fetch sample from BMP390 device (err %d).", rc);
        set_value(PRESSURE, -1.0f); // Error indicator
        return rc;
    }

    rc = sensor_channel_get(bmp390_dev_p, SENSOR_CHAN_PRESS, &pressure);
    if (rc != 0)
    {
        LOG_ERR("Failed to get pressure data (err %d).", rc);
        set_value(PRESSURE, -1.0f); // Error indicator
        return rc;
    }
    rc = sensor_channel_get(bmp390_dev_p, SENSOR_CHAN_AMBIENT_TEMP, &temperature_3);
    if (rc != 0)
    {
        LOG_ERR("Failed to get temperature data (err %d).", rc);
        // return rc; // Non-critical
    }

    // Save values
    set_value(PRESSURE, sensor_value_to_float(&pressure));
    LOG_INF("BMP390 pressure: %d.%d hPa", pressure.val1 / 100, (pressure.val1 % 100) + pressure.val2 / 100);
    LOG_INF("BMP390 temperature: %d.%d °C", temperature_3.val1, temperature_3.val2);
    return 0;
}
#endif

#ifdef CONFIG_ENABLE_SCD4X
/**
 * @brief Read SCD4X sensor data and save the temperature, humidity and CO2 levels to the variables
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
static int read_scd4x_data()
{
    int rc = 0;

#ifdef CONFIG_ENABLE_BMP390
    rc = sensor_attr_set(scd4x_dev_p, SENSOR_CHAN_CO2, SENSOR_ATTR_SCD4X_AMBIENT_PRESSURE, &pressure);
    if (rc != 0)
    {
        LOG_ERR("Failed to set pressure compensation (err %d).", rc);
        set_value(CO2_CONCENTRATION, -1.0f); // Error indicator
        return rc;
    }
#endif

    rc = sensor_sample_fetch(scd4x_dev_p);
    if (rc != 0)
    {
        LOG_ERR("Failed to fetch sample from SCD4x device (err %d).", rc);
        set_value(CO2_CONCENTRATION, -1.0f); // Error indicator
        return rc;
    }

    rc = sensor_channel_get(scd4x_dev_p, SENSOR_CHAN_CO2, &co2_concentration);
    if (rc != 0)
    {
        LOG_ERR("Failed to get CO2 concentration data (err %d).", rc);
        set_value(CO2_CONCENTRATION, -1.0f); // Error indicator
        return rc;
    }
    rc = sensor_channel_get(scd4x_dev_p, SENSOR_CHAN_AMBIENT_TEMP, &temperature_2);
    if (rc != 0)
    {
        LOG_ERR("Failed to get temperature data (err %d).", rc);
        // return rc; // Non-critical
    }
    rc = sensor_channel_get(scd4x_dev_p, SENSOR_CHAN_HUMIDITY, &humidity_2);
    if (rc != 0)
    {
        LOG_ERR("Failed to get humidity data (err %d).", rc);
        // return rc; // Non-critical
    }

    // Save values
    set_value(CO2_CONCENTRATION, sensor_value_to_float(&co2_concentration));
    LOG_INF("SCD4X CO2 concentration: %d.%d ppm", co2_concentration.val1, co2_concentration.val2);
    LOG_INF("SCD4X temperature: %d.%d °C", temperature_2.val1, temperature_2.val2);
    LOG_INF("SCD4X humidity: %d.%d %%RH", humidity_2.val1, humidity_2.val2);
    return 0;
}
#endif

int read_sensors(void)
{
    int rc = 0;
    int success = true;

#ifdef CONFIG_ENABLE_SHT4X
    rc = read_sht4x_data();
    if (rc != 0)
    {
        LOG_ERR("Failed to read SHT4X data (err %d).", rc);
        success = false;
    }
#endif

#ifdef CONFIG_ENABLE_BMP390
    rc = read_bmp390_data();
    if (rc != 0)
    {
        LOG_ERR("Failed to read BMP390 data (err %d).", rc);
        success = false;
    }
#endif

#ifdef CONFIG_ENABLE_SCD4X
    rc = read_scd4x_data();
    if (rc != 0)
    {
        LOG_ERR("Failed to read SCD4X data (err %d).", rc);
        success = false;
    }
#endif

#ifdef CONFIG_ENABLE_SGP40
    rc = read_sgp40_data();
    if (rc != 0)
    {
        LOG_ERR("Failed to read SGP40 data (err %d).", rc);
        success = false;
    }
#endif
    return success ? 0 : -ENXIO;
}

int activate_sensors(void)
{
    LOG_INF("Activating sensors");
    int rc, ret = 0;
#ifdef CONFIG_ENABLE_SGP40
    rc = pm_device_action_run(sgp40_dev_p, PM_DEVICE_ACTION_RESUME);
    if (rc != 0)
    {
        LOG_ERR("Failed to activate SGP40 device (err %d).", rc);
        ret = rc;
    }
    rc = warm_up_sgp40();
    if (rc != 0)
    {
        LOG_ERR("Failed to do a warmup measurement on the SGP40 (err %d).", rc);
        ret = rc;
    }
#endif
#ifdef CONFIG_ENABLE_BMP390
    rc = pm_device_action_run(bmp390_dev_p, PM_DEVICE_ACTION_RESUME);
    if (rc != 0)
    {
        LOG_ERR("Failed to activate BMP390 device (err %d).", rc);
        ret = rc;
    }
#endif
#ifdef CONFIG_ENABLE_SCD4X
    rc = pm_device_action_run(scd4x_dev_p, PM_DEVICE_ACTION_RESUME);
    if (rc != 0)
    {
        LOG_ERR("Failed to activate SCD4X device (err %d).", rc);
        ret = rc;
    }
#endif
    return ret;
}

int suspend_sensors(void)
{
    LOG_INF("Suspending sensors.");
    int rc, ret = 0;
#ifdef CONFIG_ENABLE_SGP40
    rc = pm_device_action_run(sgp40_dev_p, PM_DEVICE_ACTION_SUSPEND);
    if (rc != 0)
    {
        LOG_ERR("Failed to suspend SGP40 device (err %d).", rc);
        ret = rc;
    }
#endif
#ifdef CONFIG_ENABLE_BMP390
    rc = pm_device_action_run(bmp390_dev_p, PM_DEVICE_ACTION_SUSPEND);
    if (rc != 0)
    {
        LOG_ERR("Failed to suspend BMP390 device (err %d).", rc);
        ret = rc;
    }
#endif
#ifdef CONFIG_ENABLE_SCD4X
    rc = pm_device_action_run(scd4x_dev_p, PM_DEVICE_ACTION_SUSPEND);
    if (rc != 0)
    {
        LOG_ERR("Failed to suspend SCD4X device (err %d).", rc);
        ret = rc;
    }
#endif
    return ret;
}