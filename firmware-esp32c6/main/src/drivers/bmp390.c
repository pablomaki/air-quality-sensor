#include <drivers/bmp390.h>
#include <drivers/bosch_i2c.h>
#include <drivers/sensirion_i2c.h>

#include <bmp3.h>

#define BMP390_ADDRESS BMP3_ADDR_I2C_SEC

static i2c_master_dev_handle_t bmp390_i2c;
static struct bmp3_dev bmp390_dev;

esp_err_t bmp390_init(i2c_master_bus_handle_t bus)
{
    esp_err_t err = sensor_i2c_add_device(bus, BMP390_ADDRESS, &bmp390_i2c);
    if (err != ESP_OK)
    {
        return err;
    }

    bmp390_dev.intf = BMP3_I2C_INTF;
    bmp390_dev.intf_ptr = bmp390_i2c;
    bmp390_dev.read = bosch_i2c_read;
    bmp390_dev.write = bosch_i2c_write;
    bmp390_dev.delay_us = bosch_delay_us;

    if (bmp3_init(&bmp390_dev) != BMP3_OK)
    {
        return ESP_FAIL;
    }

    struct bmp3_settings settings = {
        .op_mode = BMP3_MODE_NORMAL,
        .press_en = BMP3_ENABLE,
        .temp_en = BMP3_ENABLE,
        .odr_filter = {
            .press_os = BMP3_OVERSAMPLING_8X,
            .temp_os = BMP3_NO_OVERSAMPLING,
            .iir_filter = BMP3_IIR_FILTER_COEFF_1,
            .odr = BMP3_ODR_12_5_HZ,
        },
    };
    const uint32_t selected = BMP3_SEL_PRESS_EN | BMP3_SEL_TEMP_EN | BMP3_SEL_PRESS_OS | BMP3_SEL_TEMP_OS |
                              BMP3_SEL_IIR_FILTER | BMP3_SEL_ODR;

    if (bmp3_set_sensor_settings(selected, &settings, &bmp390_dev) != BMP3_OK ||
        bmp3_set_op_mode(&settings, &bmp390_dev) != BMP3_OK)
    {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t bmp390_read(float *pressure_hpa, float *temperature)
{
    struct bmp3_data data;

    if (bmp3_get_sensor_data(BMP3_PRESS_TEMP, &data, &bmp390_dev) != BMP3_OK)
    {
        return ESP_FAIL;
    }

    *pressure_hpa = (float)(data.pressure / 100.0);
    *temperature = (float)data.temperature;
    return ESP_OK;
}
