#include <drivers/sht4x.h>
#include <drivers/sensirion_i2c.h>

#include <rom/ets_sys.h>

#define SHT4X_ADDRESS 0x44
#define SHT4X_CMD_SOFT_RESET 0x94
#define SHT4X_CMD_MEASURE_HIGH_REPEATABILITY 0xFD
#define SHT4X_MEASURE_WAIT_US 8300
#define SHT4X_RESET_WAIT_US 1000
#define SHT4X_I2C_TIMEOUT_MS 100

static i2c_master_dev_handle_t sht4x_dev;

/**
 * @brief Send a single byte SHT4x command
 *
 * @param command Command byte
 * @return esp_err_t, ESP_OK if ok
 */
static esp_err_t write_command(uint8_t command)
{
    return i2c_master_transmit(sht4x_dev, &command, 1, SHT4X_I2C_TIMEOUT_MS);
}

esp_err_t sht4x_init(i2c_master_bus_handle_t bus)
{
    esp_err_t err = sensor_i2c_add_device(bus, SHT4X_ADDRESS, &sht4x_dev);
    if (err != ESP_OK)
    {
        return err;
    }

    err = write_command(SHT4X_CMD_SOFT_RESET);
    if (err != ESP_OK)
    {
        return err;
    }
    ets_delay_us(SHT4X_RESET_WAIT_US);
    return ESP_OK;
}

esp_err_t sht4x_measure(float *temperature, float *humidity)
{
    esp_err_t err = write_command(SHT4X_CMD_MEASURE_HIGH_REPEATABILITY);
    if (err != ESP_OK)
    {
        return err;
    }

    sensor_delay_ms((SHT4X_MEASURE_WAIT_US + 999) / 1000);

    uint16_t words[2];
    err = sensirion_read_words(sht4x_dev, words, 2);
    if (err != ESP_OK)
    {
        return err;
    }

    *temperature = -45.0f + 175.0f * words[0] / 65535.0f;

    float rh = -6.0f + 125.0f * words[1] / 65535.0f;
    *humidity = rh < 0.0f ? 0.0f : (rh > 100.0f ? 100.0f : rh);
    return ESP_OK;
}
