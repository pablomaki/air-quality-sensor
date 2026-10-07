#include <drivers/bme680_iaq.h>
#include <drivers/bosch_i2c.h>
#include <drivers/sensirion_i2c.h>

#include <bme68x.h>
#include <bsec_interface.h>

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <nvs.h>

#include <string.h>

static const char *TAG = "bme680_iaq";

#define BME680_ADDRESS BME68X_I2C_ADDR_HIGH
#define BME680_EXPECTED_AMBIENT_TEMP 25
#define BSEC_TOTAL_HEAT_DUR UINT16_C(140)
#define BSEC_TASK_STACK_SIZE 4096
#define BSEC_TASK_PRIORITY 1
#define BSEC_NVS_NAMESPACE "aqs"
#define BSEC_NVS_KEY "bsec_state"
#define BSEC_INPUT_PRESENT(settings, shift) ((settings).process_data & (1 << ((shift) - 1)))
#define ARRAY_LENGTH(array) (sizeof(array) / sizeof((array)[0]))

static const float temperature_offset = CONFIG_BME680_TEMPERATURE_OFFSET_CENTI_C / 100.0f;
static const int64_t state_save_interval_us = CONFIG_BME680_STATE_SAVE_INTERVAL_MINUTES * 60LL * 1000000LL;

static const bsec_sensor_configuration_t requested_virtual_sensors[] = {
    {.sensor_id = BSEC_OUTPUT_IAQ, .sample_rate = BSEC_SAMPLE_RATE_LP},
    {.sensor_id = BSEC_OUTPUT_CO2_EQUIVALENT, .sample_rate = BSEC_SAMPLE_RATE_LP},
    {.sensor_id = BSEC_OUTPUT_BREATH_VOC_EQUIVALENT, .sample_rate = BSEC_SAMPLE_RATE_LP},
    {.sensor_id = BSEC_OUTPUT_STABILIZATION_STATUS, .sample_rate = BSEC_SAMPLE_RATE_LP},
    {.sensor_id = BSEC_OUTPUT_RUN_IN_STATUS, .sample_rate = BSEC_SAMPLE_RATE_LP},
    {.sensor_id = BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE, .sample_rate = BSEC_SAMPLE_RATE_LP},
    {.sensor_id = BSEC_OUTPUT_RAW_PRESSURE, .sample_rate = BSEC_SAMPLE_RATE_LP},
    {.sensor_id = BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY, .sample_rate = BSEC_SAMPLE_RATE_LP},
};

/*
 * Subscribed until the first output arrives: continuous rate gets values out
 * within seconds of boot instead of after the first low power period.
 */
static const bsec_sensor_configuration_t quick_init_virtual_sensors[] = {
    {.sensor_id = BSEC_OUTPUT_IAQ, .sample_rate = BSEC_SAMPLE_RATE_CONT},
    {.sensor_id = BSEC_OUTPUT_CO2_EQUIVALENT, .sample_rate = BSEC_SAMPLE_RATE_CONT},
    {.sensor_id = BSEC_OUTPUT_BREATH_VOC_EQUIVALENT, .sample_rate = BSEC_SAMPLE_RATE_CONT},
    {.sensor_id = BSEC_OUTPUT_STABILIZATION_STATUS, .sample_rate = BSEC_SAMPLE_RATE_CONT},
    {.sensor_id = BSEC_OUTPUT_RUN_IN_STATUS, .sample_rate = BSEC_SAMPLE_RATE_CONT},
    {.sensor_id = BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE, .sample_rate = BSEC_SAMPLE_RATE_CONT},
    {.sensor_id = BSEC_OUTPUT_RAW_PRESSURE, .sample_rate = BSEC_SAMPLE_RATE_CONT},
    {.sensor_id = BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY, .sample_rate = BSEC_SAMPLE_RATE_CONT},
};

static i2c_master_dev_handle_t bme680_i2c;
static struct bme68x_dev bme680_dev;
static uint8_t work_buffer[BSEC_MAX_WORKBUFFER_SIZE];
static uint8_t state_buffer[BSEC_MAX_STATE_BLOB_SIZE];
static SemaphoreHandle_t output_lock;
static bme680_iaq_output_t latest;
static bool has_output;

/**
 * @brief Restore the BSEC calibration state saved by a previous boot, if any
 */
static void state_load(void)
{
    nvs_handle_t handle;
    if (nvs_open(BSEC_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
    {
        return;
    }

    size_t length = sizeof(state_buffer);
    esp_err_t err = nvs_get_blob(handle, BSEC_NVS_KEY, state_buffer, &length);
    nvs_close(handle);
    if (err != ESP_OK)
    {
        return;
    }

    bsec_library_return_t rc = bsec_set_state(state_buffer, length, work_buffer, sizeof(work_buffer));
    if (rc != BSEC_OK)
    {
        ESP_LOGW(TAG, "Failed to restore the BSEC state (err %d), starting uncalibrated.", rc);
    }
}

/**
 * @brief Persist the current BSEC calibration state
 */
static void state_save(void)
{
    uint32_t length = 0;
    bsec_library_return_t rc =
        bsec_get_state(0, state_buffer, sizeof(state_buffer), work_buffer, sizeof(work_buffer), &length);
    if (rc != BSEC_OK)
    {
        ESP_LOGE(TAG, "Failed to export the BSEC state (err %d).", rc);
        return;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(BSEC_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK)
    {
        err = nvs_set_blob(handle, BSEC_NVS_KEY, state_buffer, length);
        if (err == ESP_OK)
        {
            err = nvs_commit(handle);
        }
        nvs_close(handle);
    }
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to store the BSEC state (err %d).", err);
    }
}

/**
 * @brief Subscribe BSEC to a set of virtual sensors
 *
 * @param sensors Virtual sensors and their sample rates
 * @param count Number of virtual sensors
 * @return bsec_library_return_t, BSEC_OK if ok
 */
static bsec_library_return_t subscribe(const bsec_sensor_configuration_t *sensors, uint8_t count)
{
    bsec_sensor_configuration_t required[BSEC_MAX_PHYSICAL_SENSOR];
    uint8_t n_required = BSEC_MAX_PHYSICAL_SENSOR;

    return bsec_update_subscription(sensors, count, required, &n_required);
}

/**
 * @brief Store the BSEC outputs as the latest values
 *
 * @param outputs BSEC outputs
 * @param count Number of outputs
 */
static void store_outputs(const bsec_output_t *outputs, uint8_t count)
{
    xSemaphoreTake(output_lock, portMAX_DELAY);
    for (uint8_t i = 0; i < count; i++)
    {
        const bsec_output_t *output = &outputs[i];
        switch (output->sensor_id)
        {
        case BSEC_OUTPUT_IAQ:
            latest.iaq = (uint16_t)output->signal;
            latest.iaq_accuracy = output->accuracy;
            break;
        case BSEC_OUTPUT_CO2_EQUIVALENT:
            latest.co2_equivalent = output->signal;
            latest.co2_accuracy = output->accuracy;
            break;
        case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
            latest.voc_equivalent = output->signal;
            latest.voc_accuracy = output->accuracy;
            break;
        case BSEC_OUTPUT_STABILIZATION_STATUS:
            latest.gas_stabilized = output->signal != 0.0f;
            break;
        case BSEC_OUTPUT_RUN_IN_STATUS:
            latest.gas_run_in = output->signal != 0.0f;
            break;
        case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
            latest.temperature = output->signal;
            break;
        case BSEC_OUTPUT_RAW_PRESSURE:
            latest.pressure_pa = output->signal;
            break;
        case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
            latest.humidity = output->signal;
            break;
        default:
            break;
        }
    }
    has_output = true;
    xSemaphoreGive(output_lock);
}

/**
 * @brief Convert one BME680 measurement into BSEC inputs
 *
 * @param settings Settings BSEC requested for this measurement
 * @param data Measurement
 * @param inputs Destination, at least BSEC_MAX_PHYSICAL_SENSOR entries
 * @param timestamp_ns Measurement time
 * @return uint8_t, number of inputs written
 */
static uint8_t to_bsec_inputs(const bsec_bme_settings_t *settings, const struct bme68x_data *data,
                              bsec_input_t *inputs, int64_t timestamp_ns)
{
    uint8_t count = 0;

    if (BSEC_INPUT_PRESENT(*settings, BSEC_INPUT_TEMPERATURE))
    {
        inputs[count++] = (bsec_input_t){.sensor_id = BSEC_INPUT_HEATSOURCE, .signal = temperature_offset, .time_stamp = timestamp_ns};
        inputs[count++] = (bsec_input_t){.sensor_id = BSEC_INPUT_TEMPERATURE, .signal = data->temperature, .time_stamp = timestamp_ns};
    }
    if (BSEC_INPUT_PRESENT(*settings, BSEC_INPUT_HUMIDITY))
    {
        inputs[count++] = (bsec_input_t){.sensor_id = BSEC_INPUT_HUMIDITY, .signal = data->humidity, .time_stamp = timestamp_ns};
    }
    if (BSEC_INPUT_PRESENT(*settings, BSEC_INPUT_PRESSURE))
    {
        inputs[count++] = (bsec_input_t){.sensor_id = BSEC_INPUT_PRESSURE, .signal = data->pressure, .time_stamp = timestamp_ns};
    }
    if (BSEC_INPUT_PRESENT(*settings, BSEC_INPUT_GASRESISTOR))
    {
        inputs[count++] = (bsec_input_t){.sensor_id = BSEC_INPUT_GASRESISTOR, .signal = data->gas_resistance, .time_stamp = timestamp_ns};
    }
    if (BSEC_INPUT_PRESENT(*settings, BSEC_INPUT_PROFILE_PART))
    {
        float profile = settings->op_mode == BME68X_FORCED_MODE ? 0.0f : data->gas_index;
        inputs[count++] = (bsec_input_t){.sensor_id = BSEC_INPUT_PROFILE_PART, .signal = profile, .time_stamp = timestamp_ns};
    }
    return count;
}

/**
 * @brief Configure the BME680 the way BSEC asked for
 *
 * @param settings Settings from bsec_sensor_control()
 * @return int8_t, BME68X_OK if ok
 */
static int8_t apply_settings(bsec_bme_settings_t *settings)
{
    struct bme68x_conf config = {0};
    struct bme68x_heatr_conf heater = {
        .enable = BME68X_ENABLE,
        .heatr_temp = settings->heater_temperature,
        .heatr_dur = settings->heater_duration,
        .heatr_temp_prof = settings->heater_temperature_profile,
        .heatr_dur_prof = settings->heater_duration_profile,
        .profile_len = settings->heater_profile_len,
    };
    int8_t rc;

    if (settings->op_mode == BME68X_PARALLEL_MODE)
    {
        heater.shared_heatr_dur =
            BSEC_TOTAL_HEAT_DUR - (bme68x_get_meas_dur(settings->op_mode, &config, &bme680_dev) / INT64_C(1000));
    }

    if (settings->op_mode == BME68X_PARALLEL_MODE || settings->op_mode == BME68X_FORCED_MODE)
    {
        rc = bme68x_get_conf(&config, &bme680_dev);
        if (rc != BME68X_OK)
        {
            return rc;
        }
        config.os_hum = settings->humidity_oversampling;
        config.os_temp = settings->temperature_oversampling;
        config.os_pres = settings->pressure_oversampling;
        rc = bme68x_set_conf(&config, &bme680_dev);
        if (rc != BME68X_OK)
        {
            return rc;
        }
        bme68x_set_heatr_conf(settings->op_mode, &heater, &bme680_dev);
    }

    return bme68x_set_op_mode(settings->op_mode, &bme680_dev);
}

/**
 * @brief Read the measurement BSEC triggered and run it through BSEC
 *
 * @param settings Settings the measurement was taken with
 * @param timestamp_ns Measurement time
 * @return int, 0 if at least one field was processed
 */
static int process_measurement(const bsec_bme_settings_t *settings, int64_t timestamp_ns)
{
    struct bme68x_data data[3] = {0};
    uint8_t n_fields = 0;

    int8_t rc = bme68x_get_data(settings->op_mode, data, &n_fields, &bme680_dev);
    if (rc != BME68X_OK)
    {
        return rc;
    }

    int status = -1;
    for (uint8_t i = 0; i < n_fields; i++)
    {
        bsec_input_t inputs[BSEC_MAX_PHYSICAL_SENSOR];
        uint8_t n_inputs = to_bsec_inputs(settings, &data[i], inputs, timestamp_ns);
        if (n_inputs == 0)
        {
            continue;
        }

        bsec_output_t outputs[ARRAY_LENGTH(requested_virtual_sensors)];
        uint8_t n_outputs = ARRAY_LENGTH(outputs);
        bsec_library_return_t bsec_rc = bsec_do_steps(inputs, n_inputs, outputs, &n_outputs);
        if (bsec_rc != BSEC_OK)
        {
            ESP_LOGE(TAG, "bsec_do_steps failed (err %d).", bsec_rc);
            continue;
        }
        store_outputs(outputs, n_outputs);
        status = 0;
    }
    return status;
}

/**
 * @brief Run BSEC: follow its schedule, feed it measurements and save its state
 *
 * @param arg Unused
 */
static void bsec_task(void *arg)
{
    (void)arg;
    bool quick_init = true;
    int64_t next_save_us = esp_timer_get_time() + state_save_interval_us;
    bsec_bme_settings_t settings = {0};

    if (subscribe(quick_init_virtual_sensors, ARRAY_LENGTH(quick_init_virtual_sensors)) != BSEC_OK)
    {
        ESP_LOGE(TAG, "Failed to subscribe to the quick start outputs.");
    }

    while (true)
    {
        int64_t now_ns = esp_timer_get_time() * 1000;
        if (now_ns < settings.next_call)
        {
            sensor_delay_ms((uint32_t)((settings.next_call - now_ns) / 1000000));
            continue;
        }

        memset(&settings, 0, sizeof(settings));
        bsec_library_return_t rc = bsec_sensor_control(now_ns, &settings);
        if (rc != BSEC_OK)
        {
            ESP_LOGE(TAG, "bsec_sensor_control failed (err %d).", rc);
            sensor_delay_ms(1000);
            continue;
        }

        if (apply_settings(&settings) != BME68X_OK)
        {
            ESP_LOGE(TAG, "Failed to apply the BSEC sensor settings.");
            sensor_delay_ms(1000);
            continue;
        }

        if (settings.trigger_measurement && settings.op_mode != BME68X_SLEEP_MODE &&
            process_measurement(&settings, now_ns) == 0 && quick_init)
        {
            quick_init = false;
            if (subscribe(requested_virtual_sensors, ARRAY_LENGTH(requested_virtual_sensors)) != BSEC_OK)
            {
                ESP_LOGE(TAG, "Failed to subscribe to the low power outputs.");
            }
        }

        if (esp_timer_get_time() >= next_save_us)
        {
            state_save();
            next_save_us = esp_timer_get_time() + state_save_interval_us;
        }
    }
}

esp_err_t bme680_iaq_init(i2c_master_bus_handle_t bus)
{
    esp_err_t err = sensor_i2c_add_device(bus, BME680_ADDRESS, &bme680_i2c);
    if (err != ESP_OK)
    {
        return err;
    }

    bme680_dev.intf = BME68X_I2C_INTF;
    bme680_dev.intf_ptr = bme680_i2c;
    bme680_dev.read = bosch_i2c_read;
    bme680_dev.write = bosch_i2c_write;
    bme680_dev.delay_us = bosch_delay_us;
    bme680_dev.amb_temp = BME680_EXPECTED_AMBIENT_TEMP;

    int8_t rc = bme68x_init(&bme680_dev);
    if (rc != BME68X_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize the BME680 (err %d).", rc);
        return ESP_FAIL;
    }

    bsec_library_return_t bsec_rc = bsec_init();
    if (bsec_rc != BSEC_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize BSEC (err %d).", bsec_rc);
        return ESP_FAIL;
    }
    state_load();

    output_lock = xSemaphoreCreateMutex();
    if (output_lock == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(bsec_task, "bsec", BSEC_TASK_STACK_SIZE, NULL, BSEC_TASK_PRIORITY, NULL) != pdPASS)
    {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t bme680_iaq_read(bme680_iaq_output_t *output)
{
    if (output_lock == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    xSemaphoreTake(output_lock, portMAX_DELAY);
    bool valid = has_output;
    *output = latest;
    xSemaphoreGive(output_lock);

    return valid ? ESP_OK : ESP_ERR_INVALID_STATE;
}
