#include <drivers/bosch_i2c.h>
#include <drivers/sensirion_i2c.h>

#include <rom/ets_sys.h>

#include <string.h>

#define BOSCH_I2C_TIMEOUT_MS 100
#define BOSCH_MAX_WRITE 32

int8_t bosch_i2c_read(uint8_t reg_addr, uint8_t *data, uint32_t length, void *intf_ptr)
{
    i2c_master_dev_handle_t dev = (i2c_master_dev_handle_t)intf_ptr;
    return i2c_master_transmit_receive(dev, &reg_addr, 1, data, length, BOSCH_I2C_TIMEOUT_MS) == ESP_OK ? 0 : -1;
}

int8_t bosch_i2c_write(uint8_t reg_addr, const uint8_t *data, uint32_t length, void *intf_ptr)
{
    if (length > BOSCH_MAX_WRITE)
    {
        return -1;
    }

    uint8_t buffer[1 + BOSCH_MAX_WRITE];
    buffer[0] = reg_addr;
    memcpy(&buffer[1], data, length);

    i2c_master_dev_handle_t dev = (i2c_master_dev_handle_t)intf_ptr;
    return i2c_master_transmit(dev, buffer, length + 1, BOSCH_I2C_TIMEOUT_MS) == ESP_OK ? 0 : -1;
}

void bosch_delay_us(uint32_t period_us, void *intf_ptr)
{
    (void)intf_ptr;

    if (period_us < 1000)
    {
        ets_delay_us(period_us);
    }
    else
    {
        sensor_delay_ms((period_us + 999) / 1000);
    }
}
