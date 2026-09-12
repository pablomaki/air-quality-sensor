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

/** @brief Upper bound on the averaging window, to bound the buffer memory */
#define SAMPLE_WINDOW_MAX 60

/**
 * @brief Number of samples averaged into each reported value
 *
 * Derived rather than configured: one reporting period's worth of samples, so
 * that every sample taken contributes to exactly one report and the two
 * intervals cannot drift out of step. Clamped to at least one sample, and
 * bounded above so that an extreme combination of intervals cannot size the
 * buffers out of RAM.
 */
#define SAMPLE_WINDOW CLAMP(CONFIG_REPORT_INTERVAL_MS / CONFIG_SAMPLE_INTERVAL_MS, 1, SAMPLE_WINDOW_MAX)

/** @brief Whether the sensor value buffers were allocated successfully */
static bool buffers_ready;

/**
 * @brief Ambient temperature and humidity, for SGP40 compensation
 *
 * Published by the sensor that measures them and consumed by the SGP40, which
 * cannot measure them itself. Valid only while @ref ambient_valid is set.
 */
static __maybe_unused struct sensor_value ambient_temperature, ambient_humidity;
static bool ambient_valid;

/**
 * @brief Ambient pressure in hPa, for SCD4x compensation and for reporting
 *
 * hPa is used throughout because it suits both consumers exactly:
 * SENSOR_ATTR_SCD4X_AMBIENT_PRESSURE is specified in hPa, and Matter's
 * PressureMeasurement MeasuredValue is in units of 0.1 kPa, which is 1 hPa.
 * The drivers disagree on their own unit - SENSOR_CHAN_PRESS is documented as
 * kPa while the BME68x/BSEC driver reports Pa - so each read converts on the
 * way in. Valid only while @ref ambient_pressure_valid is set.
 */
static __maybe_unused struct sensor_value ambient_pressure;
static bool ambient_pressure_valid;

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
/*
 * Automatic self calibration periods are wall clock hours and must be integer
 * multiples of 4, so they do not depend on how often the sensor is sampled.
 */
#define SCD4X_ASC_INITIAL_PERIOD_HOURS 48   /* 2 days */
#define SCD4X_ASC_STANDARD_PERIOD_HOURS 168 /* 7 days */
static struct sensor_value asc_initial_period = {SCD4X_ASC_INITIAL_PERIOD_HOURS, 0};
static struct sensor_value asc_standard_period = {SCD4X_ASC_STANDARD_PERIOD_HOURS, 0};
static struct sensor_value sensor_altitude = {CONFIG_SCD4X_ALTITUDE, 0};
static struct sensor_value temperature_offset = {
    CONFIG_SCD4X_TEMPERATURE_OFFSET_MILLI_C / 1000,
    (CONFIG_SCD4X_TEMPERATURE_OFFSET_MILLI_C % 1000) * 1000,
};
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
        GasIndexAlgorithm_init_with_sampling_interval(&voc_params, GasIndexAlgorithm_ALGORITHM_TYPE_VOC,
                                                      CONFIG_SAMPLE_INTERVAL_MS / 1000.0f);
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
    rc = init_buffers(SAMPLE_WINDOW);
    if (rc != 0)
    {
        LOG_ERR("Failed to initialize sensor value buffers (err %d).", rc);
        status = -ENXIO;
    }
    else
    {
        buffers_ready = true;
        LOG_INF("Averaging %d sample(s) per report.", SAMPLE_WINDOW);
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
 * Applies temperature and humidity compensation from @ref ambient_temperature
 * and @ref ambient_humidity when available. Without them the driver falls back
 * to its own defaults, so the reading still succeeds, just less accurately.
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
        LOG_WRN("No ambient temperature/humidity available, SGP40 uses driver defaults.");
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
    sensor_value_from_float(&ambient_pressure, sensor_value_to_float(&pressure) * 10.0f); // kPa -> hPa
    ambient_pressure_valid = true;
    set_value(PRESSURE, sensor_value_to_float(&ambient_pressure));
    LOG_INF("BMP390 pressure: %d hPa", ambient_pressure.val1);
    LOG_INF("BMP390 temperature: %d.%d °C", temperature_3.val1, temperature_3.val2);
    return 0;
}
#endif

#ifdef CONFIG_ENABLE_SCD4X
/**
 * @brief Read SCD4X sensor data and save the temperature, humidity and CO2 levels to the variables
 *
 * Applies pressure compensation from @ref ambient_pressure when available;
 * without it the sensor falls back to CONFIG_SCD4X_ALTITUDE, so a missing
 * pressure reading must not fail the CO2 read.
 *
 * The SCD4x also measures temperature and humidity, but it self-heats and so
 * reads high - the reason CONFIG_SCD4X_TEMPERATURE_OFFSET_MILLI_C exists. They are
 * therefore only published in a build with no SHT4X.
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

    if (ambient_pressure_valid)
    {
        rc = sensor_attr_set(scd4x_dev_p, SENSOR_CHAN_CO2, SENSOR_ATTR_SCD4X_AMBIENT_PRESSURE, &ambient_pressure);
        if (rc != 0)
        {
            LOG_WRN("Failed to set pressure compensation to %d hPa (err %d), continuing.",
                    ambient_pressure.val1, rc);
        }
    }

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
    if (co2_concentration.val1 == 0)
    {
        LOG_WRN("SCD4X has no measurement ready yet, skipping this sample.");
        set_invalid(CO2_CONCENTRATION);
        return -EAGAIN;
    }

    __maybe_unused bool temperature_ok = true;
    __maybe_unused bool humidity_ok = true;

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

#ifndef CONFIG_ENABLE_SHT4X
    if (temperature_ok && humidity_ok)
    {
        ambient_temperature = temperature_2;
        ambient_humidity = humidity_2;
        ambient_valid = true;
        set_value(TEMPERATURE, sensor_value_to_float(&temperature_2));
        set_value(HUMIDITY, sensor_value_to_float(&humidity_2));
    }
#endif

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
#ifndef CONFIG_ENABLE_BMP390
        set_invalid(PRESSURE);
#endif
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
#ifndef CONFIG_ENABLE_BMP390
        set_invalid(PRESSURE);
#endif
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
#ifndef CONFIG_ENABLE_BMP390
        set_invalid(PRESSURE);
#endif
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
#ifndef CONFIG_ENABLE_BMP390
    sensor_value_from_float(&ambient_pressure, sensor_value_to_float(&pressure_2) / 100.0f); // Pa -> hPa
    ambient_pressure_valid = true;
    set_value(PRESSURE, sensor_value_to_float(&ambient_pressure));
#endif
    LOG_INF("BME680 temperature: %d.%d °C", temperature_4.val1, temperature_4.val2);
    LOG_INF("BME680 pressure: %d Pa", pressure_2.val1);
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

    ambient_valid = false;
    ambient_pressure_valid = false;

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

#ifdef CONFIG_ENABLE_BME680
    rc = read_bme680_data();
    if (rc != 0)
    {
        LOG_ERR("Failed to read BME680 data (err %d).", rc);
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