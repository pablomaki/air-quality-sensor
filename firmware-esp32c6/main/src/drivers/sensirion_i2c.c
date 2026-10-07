#include <drivers/sensirion_i2c.h>
#include <board.h>

#define SENSOR_I2C_TIMEOUT_MS 100
#define SENSIRION_MAX_WORDS 3

esp_err_t sensor_i2c_bus_init(i2c_master_bus_handle_t *bus)
{
    const i2c_master_bus_config_t config = {
        .i2c_port = -1,
        .sda_io_num = BOARD_I2C_SDA_GPIO,
        .scl_io_num = BOARD_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&config, bus);
}

esp_err_t sensor_i2c_add_device(i2c_master_bus_handle_t bus, uint16_t address, i2c_master_dev_handle_t *dev)
{
    esp_err_t err = i2c_master_probe(bus, address, SENSOR_I2C_TIMEOUT_MS);
    if (err != ESP_OK)
    {
        return err;
    }

    const i2c_device_config_t config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = SENSOR_I2C_SPEED_HZ,
    };
    return i2c_master_bus_add_device(bus, &config, dev);
}

uint8_t sensirion_crc(const uint8_t word[2])
{
    uint8_t crc = 0xFF;

    for (int i = 0; i < 2; i++)
    {
        crc ^= word[i];
        for (int bit = 0; bit < 8; bit++)
        {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

esp_err_t sensirion_write_command(i2c_master_dev_handle_t dev, uint16_t command, const uint16_t *args, size_t count)
{
    if (count > SENSIRION_MAX_WORDS)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buffer[2 + SENSIRION_MAX_WORDS * 3];
    size_t length = 0;

    buffer[length++] = command >> 8;
    buffer[length++] = command & 0xFF;
    for (size_t i = 0; i < count; i++)
    {
        buffer[length] = args[i] >> 8;
        buffer[length + 1] = args[i] & 0xFF;
        buffer[length + 2] = sensirion_crc(&buffer[length]);
        length += 3;
    }
    return i2c_master_transmit(dev, buffer, length, SENSOR_I2C_TIMEOUT_MS);
}

esp_err_t sensirion_read_words(i2c_master_dev_handle_t dev, uint16_t *words, size_t count)
{
    if (count > SENSIRION_MAX_WORDS)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buffer[SENSIRION_MAX_WORDS * 3];
    esp_err_t err = i2c_master_receive(dev, buffer, count * 3, SENSOR_I2C_TIMEOUT_MS);
    if (err != ESP_OK)
    {
        return err;
    }

    for (size_t i = 0; i < count; i++)
    {
        const uint8_t *word = &buffer[i * 3];
        if (sensirion_crc(word) != word[2])
        {
            return ESP_ERR_INVALID_CRC;
        }
        words[i] = (uint16_t)((word[0] << 8) | word[1]);
    }
    return ESP_OK;
}
