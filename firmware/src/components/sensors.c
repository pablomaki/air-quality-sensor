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

/**
 * @brief Whether the sensor value buffers were allocated successfully.
 *
 * Without buffers there is nowhere to store readings, so reading is skipped
 * entirely rather than dereferencing unallocated storage.
 */
static bool buffers_ready;

/**
 * @brief Ambient temperature and humidity used for cross-sensor compensation.
 *
 * The SGP40 needs ambient temperature and humidity to compensate its raw
 * reading, but it cannot measure them itself. These are declared
 * unconditionally, independent of which sensor supplies them, so that a build
 * without a temperature/humidity sensor still compiles; consumers must check
 * @ref ambient_valid before use. Set by whichever read populates them and
 * cleared when that read fails.
 */
static struct sensor_value ambient_temperature, ambient_humidity;
static bool ambient_valid;

#ifdef CONFIG_ENABLE_SHT4X
#include <zephyr/drivers/sensor/sht4x.h>
static const struct device *sht4x_dev_p;
static bool sht4x_ready;
#endif

#ifdef CONFIG_ENABLE_SGP40
#include <zephyr/drivers/sensor/sgp40.h>
static const struct device *sgp40_dev_p;
static bool sgp40_ready;
static struct sensor_value voc_raw, voc_index;
static GasIndexAlgorithmParams voc_params;
#endif

#ifdef CONFIG_ENABLE_SCD4X
#include <zephyr/drivers/sensor/scd4x.h>
static const struct device *scd4x_dev_p;
static bool scd4x_ready;
static struct sensor_value co2_concentration, temperature_2, humidity_2;
static struct sensor_value asc_initial_period = {(2 * 24 * 60 * 60) / (CONFIG_ADVERTISEMENT_INTERVAL / CONFIG_MEASUREMENTS_PER_INTERVAL / 1000) / 12, 0};
static struct sensor_value asc_standard_period = {(7 * 24 * 60 * 60) / (CONFIG_ADVERTISEMENT_INTERVAL / CONFIG_MEASUREMENTS_PER_INTERVAL / 1000) / 12, 0};
static struct sensor_value sensor_altitude = {CONFIG_SCD4X_ALTITUDE, 0};
static struct sensor_value temperature_offset = {CONFIG_SCD4X_TEMPERATURE_OFFSET, 0};
#endif

#ifdef CONFIG_ENABLE_BMP390
static const struct device *bmp390_dev_p;
static bool bmp390_ready;
static struct sensor_value pressure, temperature_3;
#endif

#ifdef CONFIG_ENABLE_BME680
#include <drivers/bme68x_iaq.h>
static const struct device *bme680_dev_p;
static bool bme680_ready;
static struct sensor_value temperature_4, pressure_2, humidity_3, iaq_index, co2_concentration_e, voc_concentration_e, iaq_accuracy, co2_accuracy, voc_accuracy, gas_run_in, gas_stabilization_status;
#endif

int init_sensors(void)
{
    int rc = 0;
    int status = 0;

    // A sensor that fails to come up is recorded as unavailable and skipped for
    // the rest of this boot. Initialization deliberately continues so that one
    // missing or misbehaving part cannot take down the whole device: the
    // remaining sensors keep reporting and the node still joins Matter. The
    // accumulated status is returned so the caller can raise an error event.

#ifdef CONFIG_ENABLE_SHT4X
    sht4x_dev_p = DEVICE_DT_GET_ANY(sensirion_sht4x);
    if (!device_is_ready(sht4x_dev_p))
    {
        LOG_ERR("Device sht4x is not ready, skipping it.");
        status = -ENXIO;
    }
    else
    {
        sht4x_ready = true;
    }
#endif

#ifdef CONFIG_ENABLE_SGP40
    sgp40_dev_p = DEVICE_DT_GET_ANY(sensirion_sgp40);
    if (!device_is_ready(sgp40_dev_p))
    {
        LOG_ERR("Device sgp40 is not ready, skipping it.");
        status = -ENXIO;
    }
    else
    {
        GasIndexAlgorithm_init_with_sampling_interval(&voc_params, GasIndexAlgorithm_ALGORITHM_TYPE_VOC, CONFIG_ADVERTISEMENT_INTERVAL / CONFIG_MEASUREMENTS_PER_INTERVAL / 1000);
        sgp40_ready = true;
    }
#endif

#ifdef CONFIG_ENABLE_SCD4X
    scd4x_dev_p = DEVICE_DT_GET_ANY(sensirion_scd41);
    if (!device_is_ready(scd4x_dev_p))
    {
        LOG_ERR("Device scd4x is not ready, skipping it.");
        status = -ENXIO;
    }
    else
    {
        // The sensor is usable even if these tunings do not take, so a failure
        // here is reported but does not disable the sensor.
        scd4x_ready = true;

        rc = sensor_attr_set(scd4x_dev_p, SENSOR_CHAN_CO2, SENSOR_ATTR_SCD4X_SELF_CALIB_INITIAL_PERIOD, &asc_initial_period);
        if (rc != 0)
        {
            LOG_ERR("Failed to set scd4x asc initial period (err %d).", rc);
            status = rc;
        }
        rc = sensor_attr_set(scd4x_dev_p, SENSOR_CHAN_CO2, SENSOR_ATTR_SCD4X_SELF_CALIB_STANDARD_PERIOD, &asc_standard_period);
        if (rc != 0)
        {
            LOG_ERR("Failed to set scd4x asc standard period (err %d).", rc);
            status = rc;
        }
        rc = sensor_attr_set(scd4x_dev_p, SENSOR_CHAN_CO2, SENSOR_ATTR_SCD4X_SENSOR_ALTITUDE, &sensor_altitude);
        if (rc != 0)
        {
            LOG_ERR("Failed to set scd4x sensor altitude (err %d).", rc);
            status = rc;
        }
        rc = sensor_attr_set(scd4x_dev_p, SENSOR_CHAN_CO2, SENSOR_ATTR_SCD4X_TEMPERATURE_OFFSET, &temperature_offset);
        if (rc != 0)
        {
            LOG_ERR("Failed to set scd4x sensor temperature offset (err %d).", rc);
            status = rc;
        }
    }
#endif

#ifdef CONFIG_ENABLE_BMP390
    bmp390_dev_p = DEVICE_DT_GET_ANY(bosch_bmp390);
    if (!device_is_ready(bmp390_dev_p))
    {
        LOG_ERR("Device bmp390 is not ready, skipping it.");
        status = -ENXIO;
    }
    else
    {
        bmp390_ready = true;
    }
#endif

#ifdef CONFIG_ENABLE_BME680
    bme680_dev_p = DEVICE_DT_GET_ANY(bosch_bme680);
    if (!device_is_ready(bme680_dev_p))
    {
        LOG_ERR("Device bme680 is not ready, skipping it.");
        status = -ENXIO;
    }
    else
    {
        bme680_ready = true;
    }
#endif

    // Initialize the buffers for the sensor values
    rc = init_buffers(CONFIG_MEASUREMENTS_PER_INTERVAL);
    if (rc != 0)
    {
        LOG_ERR("Failed to initialize sensor value buffers (err %d).", rc);
        status = -ENXIO;
    }
    else
    {
        buffers_ready = true;
    }

    return status;
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

    if (!sht4x_ready)
    {
        ambient_valid = false;
        set_invalid(TEMPERATURE);
        set_invalid(HUMIDITY);
        return -ENODEV;
    }

    rc = sensor_sample_fetch(sht4x_dev_p);
    if (rc != 0)
    {
        LOG_ERR("Failed to fetch sample from SHT4X device (err %d).", rc);
        ambient_valid = false;
        set_invalid(TEMPERATURE);
        set_invalid(HUMIDITY);
        return rc;
    }

    rc = sensor_channel_get(sht4x_dev_p, SENSOR_CHAN_AMBIENT_TEMP, &ambient_temperature);
    if (rc != 0)
    {
        LOG_ERR("Failed to get temperature data (err %d).", rc);
        ambient_valid = false;
        set_invalid(TEMPERATURE);
        set_invalid(HUMIDITY);
        return rc;
    }
    rc = sensor_channel_get(sht4x_dev_p, SENSOR_CHAN_HUMIDITY, &ambient_humidity);
    if (rc != 0)
    {
        LOG_ERR("Failed to get humidity data (err %d).", rc);
        ambient_valid = false;
        set_invalid(TEMPERATURE);
        set_invalid(HUMIDITY);
        return rc;
    }

    // Save values
    ambient_valid = true;
    set_value(TEMPERATURE, sensor_value_to_float(&ambient_temperature));
    set_value(HUMIDITY, sensor_value_to_float(&ambient_humidity));
    LOG_INF("SHT4X temperature: %d.%d °C", ambient_temperature.val1, ambient_temperature.val2);
    LOG_INF("SHT4X humidity: %d.%d %%RH", ambient_humidity.val1, ambient_humidity.val2);
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

    if (!sgp40_ready)
    {
        set_invalid(VOC_INDEX);
        return -ENODEV;
    }

    // Temperature/humidity compensation is optional: without a source for them
    // the driver falls back to its own defaults, which is far better than
    // refusing to read or compensating against uninitialized values. This is
    // also what makes an SGP40 build without a temperature/humidity sensor
    // work at all - the values used here are not owned by this sensor.
    if (ambient_valid)
    {
        rc = sensor_attr_set(sgp40_dev_p, SENSOR_CHAN_GAS_RES, SENSOR_ATTR_SGP40_TEMPERATURE, &ambient_temperature);
        if (rc != 0)
        {
            LOG_ERR("Failed to set temperature compensation (err %d).", rc);
            set_invalid(VOC_INDEX);
            return rc;
        }
        rc = sensor_attr_set(sgp40_dev_p, SENSOR_CHAN_GAS_RES, SENSOR_ATTR_SGP40_HUMIDITY, &ambient_humidity);
        if (rc != 0)
        {
            LOG_ERR("Failed to set humidity compensation (err %d).", rc);
            set_invalid(VOC_INDEX);
            return rc;
        }
    }
    else
    {
        LOG_DBG("No ambient temperature/humidity available, SGP40 uses driver defaults.");
    }

    rc = sensor_sample_fetch(sgp40_dev_p);
    if (rc != 0)
    {
        LOG_ERR("Failed to fetch sample from SGP40 device (err %d).", rc);
        set_invalid(VOC_INDEX);
        return rc;
    }

    rc = sensor_channel_get(sgp40_dev_p, SENSOR_CHAN_GAS_RES, &voc_raw);
    if (rc != 0)
    {
        LOG_ERR("Failed to get VOC idnex data (err %d).", rc);
        set_invalid(VOC_INDEX);
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

    if (!bmp390_ready)
    {
        set_invalid(PRESSURE);
        return -ENODEV;
    }

    rc = sensor_sample_fetch(bmp390_dev_p);
    if (rc != 0)
    {
        LOG_ERR("Failed to fetch sample from BMP390 device (err %d).", rc);
        set_invalid(PRESSURE);
        return rc;
    }

    rc = sensor_channel_get(bmp390_dev_p, SENSOR_CHAN_PRESS, &pressure);
    if (rc != 0)
    {
        LOG_ERR("Failed to get pressure data (err %d).", rc);
        set_invalid(PRESSURE);
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

    if (!scd4x_ready)
    {
        set_invalid(CO2_CONCENTRATION);
        return -ENODEV;
    }

#ifdef CONFIG_ENABLE_BMP390
    rc = sensor_attr_set(scd4x_dev_p, SENSOR_CHAN_CO2, SENSOR_ATTR_SCD4X_AMBIENT_PRESSURE, &pressure);
    if (rc != 0)
    {
        LOG_ERR("Failed to set pressure compensation (err %d).", rc);
        set_invalid(CO2_CONCENTRATION);
        return rc;
    }
#endif

    rc = sensor_sample_fetch(scd4x_dev_p);
    if (rc != 0)
    {
        LOG_ERR("Failed to fetch sample from SCD4x device (err %d).", rc);
        set_invalid(CO2_CONCENTRATION);
        return rc;
    }

    rc = sensor_channel_get(scd4x_dev_p, SENSOR_CHAN_CO2, &co2_concentration);
    if (rc != 0)
    {
        LOG_ERR("Failed to get CO2 concentration data (err %d).", rc);
        set_invalid(CO2_CONCENTRATION);
        return rc;
    }
    bool temperature_ok = true;
    bool humidity_ok = true;

    rc = sensor_channel_get(scd4x_dev_p, SENSOR_CHAN_AMBIENT_TEMP, &temperature_2);
    if (rc != 0)
    {
        LOG_ERR("Failed to get temperature data (err %d).", rc);
        temperature_ok = false; // Non-critical
    }
    rc = sensor_channel_get(scd4x_dev_p, SENSOR_CHAN_HUMIDITY, &humidity_2);
    if (rc != 0)
    {
        LOG_ERR("Failed to get humidity data (err %d).", rc);
        humidity_ok = false; // Non-critical
    }

    // The SCD4x measures temperature and humidity as well, so use them when no
    // better source has published values in this read cycle - otherwise an
    // SCD4x-only build has no temperature or humidity at all. They are only a
    // fallback because the SCD4x self-heats and so reads high, which is what
    // CONFIG_SCD4X_TEMPERATURE_OFFSET compensates for; a dedicated sensor is
    // preferred whenever one is present and working.
    if (temperature_ok && humidity_ok && !ambient_valid)
    {
        ambient_temperature = temperature_2;
        ambient_humidity = humidity_2;
        ambient_valid = true;
        set_value(TEMPERATURE, sensor_value_to_float(&temperature_2));
        set_value(HUMIDITY, sensor_value_to_float(&humidity_2));
    }

    // Save values
    set_value(CO2_CONCENTRATION, sensor_value_to_float(&co2_concentration));
    LOG_INF("SCD4X CO2 concentration: %d.%d ppm", co2_concentration.val1, co2_concentration.val2);
    LOG_INF("SCD4X temperature: %d.%d °C", temperature_2.val1, temperature_2.val2);
    LOG_INF("SCD4X humidity: %d.%d %%RH", humidity_2.val1, humidity_2.val2);
    return 0;
}
#endif

#ifdef CONFIG_ENABLE_BME680
static int read_bme680_data()
{
    int rc = 0;

    if (!bme680_ready)
    {
        set_invalid(IAQ_INDEX);
        set_invalid(PRESSURE);
        return -ENODEV;
    }

    rc = sensor_sample_fetch(bme680_dev_p);
    if (rc != 0)
    {
        LOG_ERR("Failed to fetch sample from BME680 device (err %d).", rc);
        set_invalid(IAQ_INDEX);
        return rc;
    }

    rc = sensor_channel_get(bme680_dev_p, SENSOR_CHAN_AMBIENT_TEMP, &temperature_4);
    if (rc != 0)
    {
        LOG_ERR("Failed to get temperature data (err %d).", rc);
        return rc;
    }
    rc = sensor_channel_get(bme680_dev_p, SENSOR_CHAN_PRESS, &pressure_2);
    if (rc != 0)
    {
        LOG_ERR("Failed to get pressure data (err %d).", rc);
        set_invalid(IAQ_INDEX);
        set_invalid(PRESSURE);
        return rc;
    }
    rc = sensor_channel_get(bme680_dev_p, SENSOR_CHAN_HUMIDITY, &humidity_3);
    if (rc != 0)
    {
        LOG_ERR("Failed to get humidity data (err %d).", rc);
        // return rc; // Non-critical
    }
    rc = sensor_channel_get(bme680_dev_p, SENSOR_CHAN_CO2, &co2_concentration_e);
    if (rc != 0)
    {
        LOG_ERR("Failed to get co2 concentration equivalent data (err %d).", rc);
        // return rc; // Non-critical
    }
    rc = sensor_channel_get(bme680_dev_p, SENSOR_CHAN_VOC, &voc_concentration_e);
    if (rc != 0)
    {
        LOG_ERR("Failed to get VOC equivalent data (err %d).", rc);
        // return rc; // Non-critical
    }
    rc = sensor_channel_get(bme680_dev_p, SENSOR_CHAN_IAQ, &iaq_index);
    if (rc != 0)
    {
        LOG_ERR("Failed to get IAQ index data (err %d).", rc);
        set_invalid(IAQ_INDEX);
        set_invalid(PRESSURE);
        return rc;
    }
    rc = sensor_channel_get(bme680_dev_p, SENSOR_CHAN_IAQ_ACC, &iaq_accuracy);
    if (rc != 0)
    {
        LOG_ERR("Failed to get IAQ index data (err %d).", rc);
        // return rc; // Non-critical
    }
    rc = sensor_channel_get(bme680_dev_p, SENSOR_CHAN_CO2_ACC, &co2_accuracy);
    if (rc != 0)
    {
        LOG_ERR("Failed to get IAQ index data (err %d).", rc);
        // return rc; // Non-critical
    }
    rc = sensor_channel_get(bme680_dev_p, SENSOR_CHAN_VOC_ACC, &voc_accuracy);
    if (rc != 0)
    {
        LOG_ERR("Failed to get IAQ index data (err %d).", rc);
        // return rc; // Non-critical
    }
    rc = sensor_channel_get(bme680_dev_p, SENSOR_CHAN_GAS_RUN_IN, &gas_run_in);
    if (rc != 0)
    {
        LOG_ERR("Failed to get IAQ index data (err %d).", rc);
        // return rc; // Non-critical
    }
    rc = sensor_channel_get(bme680_dev_p, SENSOR_CHAN_GAS_STAB, &gas_stabilization_status);
    if (rc != 0)
    {
        LOG_ERR("Failed to get IAQ index data (err %d).", rc);
        // return rc; // Non-critical
    }

    // Save values
    set_value(IAQ_INDEX, sensor_value_to_float(&iaq_index));
    set_value(PRESSURE, sensor_value_to_float(&pressure_2));
    LOG_INF("BME680 temperature: %d.%d °C", temperature_4.val1, temperature_4.val2);
    LOG_INF("BME680 pressure: %d.%d hPa", pressure_2.val1 / 100, (pressure_2.val1 % 100) + pressure_2.val2 / 100);
    LOG_INF("BME680 humidity: %d.%d %%RH", humidity_3.val1, humidity_3.val2);
    LOG_INF("BME680 CO2 concentration: %d.%d ppm", co2_concentration_e.val1, co2_concentration_e.val2);
    LOG_INF("BME680 VOC concentration: %d.%d ppb", voc_concentration_e.val1, voc_concentration_e.val2);
    LOG_INF("BME680 IAQ index: (0 - 500): %d.%d", iaq_index.val1, iaq_index.val2);
    LOG_INF("BME680 IAQ accuracy: %d.%d", iaq_accuracy.val1, iaq_accuracy.val2);
    LOG_INF("BME680 CO2 accuracy: %d.%d", voc_accuracy.val1, voc_accuracy.val2);
    LOG_INF("BME680 VOC accuracy: %d.%d", co2_accuracy.val1, co2_accuracy.val2);
    LOG_INF("BME680 run in status: %d.%d", gas_run_in.val1, gas_run_in.val2);
    LOG_INF("BME680 stabilization status: %d.%d", gas_stabilization_status.val1, gas_stabilization_status.val2);
    return 0;
}
#endif

int read_sensors(void)
{
    int rc = 0;
    bool success = true;

    // Order matters. The ambient temperature/humidity claim is reset here and
    // then taken by the first sensor that reads them successfully, so the reads
    // are sequenced best source first: SHT4X (dedicated part) ahead of SCD4X
    // (self-heating fallback). The SGP40 read consumes whatever they published,
    // so it has to come after both.
    ambient_valid = false;

    if (!buffers_ready)
    {
        LOG_ERR("Sensor value buffers unavailable, skipping the read.");
        return -ENODEV;
    }

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

#ifdef CONFIG_ENABLE_BME680
    rc = read_bme680_data();
    if (rc != 0)
    {
        LOG_ERR("Failed to read BME680 data (err %d).", rc);
        success = false;
    }
#endif
    return success ? 0 : -ENXIO;
}