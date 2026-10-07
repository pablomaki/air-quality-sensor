#include <drivers/sgp40.h>
#include <drivers/sensirion_i2c.h>

#define SGP40_ADDRESS 0x59
#define SGP40_CMD_MEASURE_RAW 0x260F
#define SGP40_MEASURE_WAIT_MS 30

static i2c_master_dev_handle_t sgp40_dev;

/**
 * @brief Clamp a value into a range
 *
 * @param value Value to clamp
 * @param min Lower bound
 * @param max Upper bound
 * @return float, the clamped value
 */
static float clampf(float value, float min, float max)
{
    return value < min ? min : (value > max ? max : value);
}

esp_err_t sgp40_init(i2c_master_bus_handle_t bus)
{
    return sensor_i2c_add_device(bus, SGP40_ADDRESS, &sgp40_dev);
}

esp_err_t sgp40_measure_raw(float temperature, float humidity, uint16_t *raw)
{
    const uint16_t args[2] = {
        (uint16_t)(clampf(humidity, 0.0f, 100.0f) * 65535.0f / 100.0f + 0.5f),
        (uint16_t)((clampf(temperature, -45.0f, 130.0f) + 45.0f) * 65535.0f / 175.0f + 0.5f),
    };

    esp_err_t err = sensirion_write_command(sgp40_dev, SGP40_CMD_MEASURE_RAW, args, 2);
    if (err != ESP_OK)
    {
        return err;
    }

    sensor_delay_ms(SGP40_MEASURE_WAIT_MS);

    return sensirion_read_words(sgp40_dev, raw, 1);
}
