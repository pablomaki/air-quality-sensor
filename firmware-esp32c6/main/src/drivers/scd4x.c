#include <drivers/scd4x.h>
#include <drivers/sensirion_i2c.h>

#define SCD4X_ADDRESS 0x62

#define SCD4X_CMD_REINIT 0x3646
#define SCD4X_CMD_STOP_PERIODIC_MEASUREMENT 0x3F86
#define SCD4X_CMD_WAKE_UP 0x36F6
#define SCD4X_CMD_START_LOW_POWER_PERIODIC_MEASUREMENT 0x21AC
#define SCD4X_CMD_GET_DATA_READY_STATUS 0xE4B8
#define SCD4X_CMD_READ_MEASUREMENT 0xEC05
#define SCD4X_CMD_SET_TEMPERATURE_OFFSET 0x241D
#define SCD4X_CMD_SET_SENSOR_ALTITUDE 0x2427
#define SCD4X_CMD_SET_AMBIENT_PRESSURE 0xE000
#define SCD4X_CMD_SET_ASC_INITIAL_PERIOD 0x2445
#define SCD4X_CMD_SET_ASC_STANDARD_PERIOD 0x244E

#define SCD4X_REINIT_WAIT_MS 30
#define SCD4X_STOP_WAIT_MS 500
#define SCD4X_WAKE_UP_WAIT_MS 30
#define SCD4X_COMMAND_WAIT_MS 1

#define SCD4X_DATA_READY_MASK 0x07FF
#define SCD4X_MIN_PRESSURE_HPA 700
#define SCD4X_MAX_PRESSURE_HPA 1200

static i2c_master_dev_handle_t scd4x_dev;
static uint16_t co2_sample, temperature_sample, humidity_sample;

/**
 * @brief Send a command with at most one argument, then wait for it to execute
 *
 * @param command Command code
 * @param arg Argument word, ignored when has_arg is false
 * @param has_arg Whether the command takes an argument
 * @param wait_ms Execution time of the command
 * @return esp_err_t, ESP_OK if ok
 */
static esp_err_t send(uint16_t command, uint16_t arg, bool has_arg, uint32_t wait_ms)
{
    esp_err_t err = sensirion_write_command(scd4x_dev, command, &arg, has_arg ? 1 : 0);
    sensor_delay_ms(wait_ms);
    return err;
}

/**
 * @brief Bring the sensor to idle from whatever mode it was left in
 *
 * A warm restart finds it measuring, a cold one idle, and power down mode
 * NACKs the first wake up, so the wake up is sent twice as a fallback.
 *
 * @return esp_err_t, ESP_OK if ok
 */
static esp_err_t enter_idle(void)
{
    if (send(SCD4X_CMD_STOP_PERIODIC_MEASUREMENT, 0, false, SCD4X_STOP_WAIT_MS) == ESP_OK)
    {
        return ESP_OK;
    }
    send(SCD4X_CMD_WAKE_UP, 0, false, SCD4X_WAKE_UP_WAIT_MS);
    return send(SCD4X_CMD_WAKE_UP, 0, false, SCD4X_WAKE_UP_WAIT_MS);
}

esp_err_t scd4x_init(i2c_master_bus_handle_t bus, const scd4x_settings_t *settings)
{
    if (settings->asc_initial_period_hours % 4 != 0 || settings->asc_standard_period_hours % 4 != 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = sensor_i2c_add_device(bus, SCD4X_ADDRESS, &scd4x_dev);
    if (err != ESP_OK)
    {
        return err;
    }

    err = enter_idle();
    if (err != ESP_OK)
    {
        return err;
    }

    err = send(SCD4X_CMD_REINIT, 0, false, SCD4X_REINIT_WAIT_MS);
    if (err != ESP_OK)
    {
        return err;
    }

    const struct
    {
        uint16_t command;
        uint16_t value;
    } writes[] = {
        {SCD4X_CMD_SET_ASC_INITIAL_PERIOD, settings->asc_initial_period_hours},
        {SCD4X_CMD_SET_ASC_STANDARD_PERIOD, settings->asc_standard_period_hours},
        {SCD4X_CMD_SET_SENSOR_ALTITUDE, settings->altitude_m},
        {SCD4X_CMD_SET_TEMPERATURE_OFFSET, (uint16_t)(settings->temperature_offset_c * 65535.0f / 175.0f)},
    };

    for (size_t i = 0; i < sizeof(writes) / sizeof(writes[0]); i++)
    {
        err = send(writes[i].command, writes[i].value, true, SCD4X_COMMAND_WAIT_MS);
        if (err != ESP_OK)
        {
            return err;
        }
    }

    return send(SCD4X_CMD_START_LOW_POWER_PERIODIC_MEASUREMENT, 0, false, 0);
}

esp_err_t scd4x_set_ambient_pressure(uint16_t pressure_hpa)
{
    if (pressure_hpa < SCD4X_MIN_PRESSURE_HPA || pressure_hpa > SCD4X_MAX_PRESSURE_HPA)
    {
        return ESP_ERR_INVALID_ARG;
    }
    return send(SCD4X_CMD_SET_AMBIENT_PRESSURE, pressure_hpa, true, SCD4X_COMMAND_WAIT_MS);
}

esp_err_t scd4x_read(uint16_t *co2_ppm, float *temperature, float *humidity)
{
    uint16_t status;
    esp_err_t err = send(SCD4X_CMD_GET_DATA_READY_STATUS, 0, false, SCD4X_COMMAND_WAIT_MS);
    if (err == ESP_OK)
    {
        err = sensirion_read_words(scd4x_dev, &status, 1);
    }
    if (err != ESP_OK)
    {
        return err;
    }

    if ((status & SCD4X_DATA_READY_MASK) != 0)
    {
        uint16_t words[3];
        err = send(SCD4X_CMD_READ_MEASUREMENT, 0, false, SCD4X_COMMAND_WAIT_MS);
        if (err == ESP_OK)
        {
            err = sensirion_read_words(scd4x_dev, words, 3);
        }
        if (err != ESP_OK)
        {
            return err;
        }
        co2_sample = words[0];
        temperature_sample = words[1];
        humidity_sample = words[2];
    }

    *co2_ppm = co2_sample;
    *temperature = -45.0f + 175.0f * temperature_sample / 65535.0f;
    *humidity = 100.0f * humidity_sample / 65535.0f;
    return ESP_OK;
}
