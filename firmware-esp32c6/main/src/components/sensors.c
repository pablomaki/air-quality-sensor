#include <components/sensors.h>
#include <utils/variable_buffer.h>
#include <drivers/sensirion_i2c.h>

#include <esp_log.h>

#include <errno.h>

static const char *TAG = "sensors";

/** @brief Upper bound on the averaging window, to bound the buffer memory */
#define SAMPLE_WINDOW_MAX 60

#define CLAMP(value, low, high) ((value) < (low) ? (low) : ((value) > (high) ? (high) : (value)))

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
static __attribute__((unused)) float ambient_temperature, ambient_humidity;
static bool ambient_valid;

/**
 * @brief Ambient pressure in hPa, for SCD4x compensation and for reporting
 *
 * hPa suits both consumers exactly: the SCD4x takes its ambient pressure in
 * hPa, and Matter's PressureMeasurement MeasuredValue is in units of 0.1 kPa,
 * which is 1 hPa. Valid only while @ref ambient_pressure_valid is set.
 */
static __attribute__((unused)) float ambient_pressure;
static bool ambient_pressure_valid;

#ifdef CONFIG_ENABLE_SHT4X
#include <drivers/sht4x.h>
static bool sht4x_ready;
#endif

#ifdef CONFIG_ENABLE_SGP40
#include <drivers/sgp40.h>
#include <sensirion_gas_index_algorithm.h>
static bool sgp40_ready;
static GasIndexAlgorithmParams voc_params;
#endif

#ifdef CONFIG_ENABLE_SCD4X
#include <drivers/scd4x.h>
static bool scd4x_ready;
/*
 * Automatic self calibration periods are wall clock hours and must be integer
 * multiples of 4, so they do not depend on how often the sensor is sampled.
 */
#define SCD4X_ASC_INITIAL_PERIOD_HOURS 48   /* 2 days */
#define SCD4X_ASC_STANDARD_PERIOD_HOURS 168 /* 7 days */
#endif

#ifdef CONFIG_ENABLE_BMP390
#include <drivers/bmp390.h>
static bool bmp390_ready;
#endif

#ifdef CONFIG_ENABLE_BME680
#include <drivers/bme680_iaq.h>
static bool bme680_ready;
#endif

int init_sensors(void)
{
    int status = 0;
    esp_err_t err;
    i2c_master_bus_handle_t bus;

    err = sensor_i2c_bus_init(&bus);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create the sensor I2C bus (err %d).", err);
        return -ENXIO;
    }

#ifdef CONFIG_ENABLE_SHT4X
    err = sht4x_init(bus);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Device sht4x is not ready (err %d), skipping it.", err);
        status = -ENXIO;
    }
    else
    {
        sht4x_ready = true;
    }
#endif

#ifdef CONFIG_ENABLE_SGP40
    err = sgp40_init(bus);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Device sgp40 is not ready (err %d), skipping it.", err);
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
    const scd4x_settings_t scd4x_settings = {
        .altitude_m = CONFIG_SCD4X_ALTITUDE,
        .temperature_offset_c = CONFIG_SCD4X_TEMPERATURE_OFFSET_MILLI_C / 1000.0f,
        .asc_initial_period_hours = SCD4X_ASC_INITIAL_PERIOD_HOURS,
        .asc_standard_period_hours = SCD4X_ASC_STANDARD_PERIOD_HOURS,
    };
    err = scd4x_init(bus, &scd4x_settings);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Device scd4x is not ready (err %d), skipping it.", err);
        status = -ENXIO;
    }
    else
    {
        scd4x_ready = true;
    }
#endif

#ifdef CONFIG_ENABLE_BMP390
    err = bmp390_init(bus);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Device bmp390 is not ready (err %d), skipping it.", err);
        status = -ENXIO;
    }
    else
    {
        bmp390_ready = true;
    }
#endif

#ifdef CONFIG_ENABLE_BME680
    err = bme680_iaq_init(bus);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Device bme680 is not ready (err %d), skipping it.", err);
        status = -ENXIO;
    }
    else
    {
        bme680_ready = true;
    }
#endif

    int rc = init_buffers(SAMPLE_WINDOW);
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Failed to initialize sensor value buffers (err %d).", rc);
        status = -ENXIO;
    }
    else
    {
        buffers_ready = true;
        ESP_LOGI(TAG, "Averaging %d sample(s) per report.", SAMPLE_WINDOW);
    }

    return status;
}

#ifdef CONFIG_ENABLE_SHT4X
/**
 * @brief Read SHT4X sensor data and save the temperature and humidity to the variables
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
static int read_sht4x_data(void)
{
    ambient_valid = false;

    if (!sht4x_ready)
    {
        set_invalid(TEMPERATURE);
        set_invalid(HUMIDITY);
        return -ENODEV;
    }

    float temperature, humidity;
    esp_err_t err = sht4x_measure(&temperature, &humidity);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to fetch sample from SHT4X device (err %d).", err);
        set_invalid(TEMPERATURE);
        set_invalid(HUMIDITY);
        return -EIO;
    }

    ambient_temperature = temperature;
    ambient_humidity = humidity;
    ambient_valid = true;
    set_value(TEMPERATURE, temperature);
    set_value(HUMIDITY, humidity);
    ESP_LOGI(TAG, "SHT4X temperature: %.2f °C", temperature);
    ESP_LOGI(TAG, "SHT4X humidity: %.2f %%RH", humidity);
    return 0;
}
#endif

#ifdef CONFIG_ENABLE_SGP40
/**
 * @brief Read SGP40 sensor data and save the VOC index to the variables
 *
 * Applies temperature and humidity compensation from @ref ambient_temperature
 * and @ref ambient_humidity when available. Without them the sensor falls back
 * to its datasheet defaults, so the reading still succeeds, just less accurately.
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
static int read_sgp40_data(void)
{
    if (!sgp40_ready)
    {
        set_invalid(VOC_INDEX);
        return -ENODEV;
    }

    float temperature = SGP40_DEFAULT_TEMPERATURE;
    float humidity = SGP40_DEFAULT_HUMIDITY;
    if (ambient_valid)
    {
        temperature = ambient_temperature;
        humidity = ambient_humidity;
    }
    else
    {
        ESP_LOGW(TAG, "No ambient temperature/humidity available, SGP40 uses defaults.");
    }

    uint16_t voc_raw;
    esp_err_t err = sgp40_measure_raw(temperature, humidity, &voc_raw);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to fetch sample from SGP40 device (err %d).", err);
        set_invalid(VOC_INDEX);
        return -EIO;
    }

    int32_t voc_index;
    GasIndexAlgorithm_process(&voc_params, voc_raw, &voc_index);

    set_value(VOC_INDEX, (float)voc_index);
    ESP_LOGI(TAG, "SGP40 VOC raw: %u", voc_raw);
    ESP_LOGI(TAG, "SGP40 VOC index (0 - 500): %ld", (long)voc_index);
    return 0;
}
#endif

#ifdef CONFIG_ENABLE_BMP390
/**
 * @brief Read BMP390 sensor data and save the pressure to the variables
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
static int read_bmp390_data(void)
{
    if (!bmp390_ready)
    {
        set_invalid(PRESSURE);
        return -ENODEV;
    }

    float pressure, temperature;
    esp_err_t err = bmp390_read(&pressure, &temperature);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to fetch sample from BMP390 device (err %d).", err);
        set_invalid(PRESSURE);
        return -EIO;
    }

    ambient_pressure = pressure;
    ambient_pressure_valid = true;
    set_value(PRESSURE, pressure);
    ESP_LOGI(TAG, "BMP390 pressure: %.1f hPa", pressure);
    ESP_LOGI(TAG, "BMP390 temperature: %.2f °C", temperature);
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
 * therefore only published when neither an SHT4X nor a BME680 is fitted, the
 * BME680 being preferred because BSEC heat compensates its output.
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
static int read_scd4x_data(void)
{
    if (!scd4x_ready)
    {
        set_invalid(CO2_CONCENTRATION);
        return -ENODEV;
    }

    if (ambient_pressure_valid)
    {
        esp_err_t err = scd4x_set_ambient_pressure((uint16_t)(ambient_pressure + 0.5f));
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "Failed to set pressure compensation to %.0f hPa (err %d), continuing.",
                     ambient_pressure, err);
        }
    }

    uint16_t co2;
    float temperature, humidity;
    esp_err_t err = scd4x_read(&co2, &temperature, &humidity);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to fetch sample from SCD4x device (err %d).", err);
        set_invalid(CO2_CONCENTRATION);
        return -EIO;
    }
    if (co2 == 0)
    {
        ESP_LOGW(TAG, "SCD4X has no measurement ready yet, skipping this sample.");
        set_invalid(CO2_CONCENTRATION);
        return -EAGAIN;
    }

#if !defined(CONFIG_ENABLE_SHT4X) && !defined(CONFIG_ENABLE_BME680)
    ambient_temperature = temperature;
    ambient_humidity = humidity;
    ambient_valid = true;
    set_value(TEMPERATURE, temperature);
    set_value(HUMIDITY, humidity);
#endif

    set_value(CO2_CONCENTRATION, (float)co2);
    ESP_LOGI(TAG, "SCD4X CO2 concentration: %u ppm", co2);
    ESP_LOGI(TAG, "SCD4X temperature: %.2f °C", temperature);
    ESP_LOGI(TAG, "SCD4X humidity: %.2f %%RH", humidity);
    return 0;
}
#endif

#ifdef CONFIG_ENABLE_BME680
/**
 * @brief Read the latest BSEC outputs and save the IAQ index to the variables
 *
 * Also publishes temperature and humidity when no SHT4X is fitted, and
 * pressure when no BMP390 is, as the nRF build does.
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
static int read_bme680_data(void)
{
    bme680_iaq_output_t output;

    if (!bme680_ready || bme680_iaq_read(&output) != ESP_OK)
    {
        set_invalid(IAQ_INDEX);
#ifndef CONFIG_ENABLE_BMP390
        set_invalid(PRESSURE);
#endif
        return bme680_ready ? -EAGAIN : -ENODEV;
    }

    set_value(IAQ_INDEX, (float)output.iaq);
#ifndef CONFIG_ENABLE_SHT4X
    ambient_temperature = output.temperature;
    ambient_humidity = output.humidity;
    ambient_valid = true;
    set_value(TEMPERATURE, output.temperature);
    set_value(HUMIDITY, output.humidity);
#endif
#ifndef CONFIG_ENABLE_BMP390
    ambient_pressure = output.pressure_pa / 100.0f;
    ambient_pressure_valid = true;
    set_value(PRESSURE, ambient_pressure);
#endif
    ESP_LOGI(TAG, "BME680 temperature: %.2f °C", output.temperature);
    ESP_LOGI(TAG, "BME680 pressure: %.0f Pa", output.pressure_pa);
    ESP_LOGI(TAG, "BME680 humidity: %.2f %%RH", output.humidity);
    ESP_LOGI(TAG, "BME680 CO2 concentration: %.1f ppm", output.co2_equivalent);
    ESP_LOGI(TAG, "BME680 VOC concentration: %.2f ppm", output.voc_equivalent);
    ESP_LOGI(TAG, "BME680 IAQ index: (0 - 500): %u", output.iaq);
    ESP_LOGI(TAG, "BME680 IAQ accuracy: %u", output.iaq_accuracy);
    ESP_LOGI(TAG, "BME680 CO2 accuracy: %u", output.co2_accuracy);
    ESP_LOGI(TAG, "BME680 VOC accuracy: %u", output.voc_accuracy);
    ESP_LOGI(TAG, "BME680 run in status: %d", output.gas_run_in);
    ESP_LOGI(TAG, "BME680 stabilization status: %d", output.gas_stabilized);
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
        ESP_LOGE(TAG, "Sensor value buffers unavailable, skipping the read.");
        return -ENODEV;
    }

#ifdef CONFIG_ENABLE_SHT4X
    rc = read_sht4x_data();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Failed to read SHT4X data (err %d).", rc);
        success = false;
    }
#endif

#ifdef CONFIG_ENABLE_BMP390
    rc = read_bmp390_data();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Failed to read BMP390 data (err %d).", rc);
        success = false;
    }
#endif

#ifdef CONFIG_ENABLE_BME680
    rc = read_bme680_data();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Failed to read BME680 data (err %d).", rc);
        success = false;
    }
#endif

#ifdef CONFIG_ENABLE_SCD4X
    rc = read_scd4x_data();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Failed to read SCD4X data (err %d).", rc);
        success = false;
    }
#endif

#ifdef CONFIG_ENABLE_SGP40
    rc = read_sgp40_data();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Failed to read SGP40 data (err %d).", rc);
        success = false;
    }
#endif

    (void)rc;
    return success ? 0 : -ENXIO;
}
