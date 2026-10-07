#ifndef BOSCH_I2C_H
#define BOSCH_I2C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register read callback for the Bosch sensor APIs
 *
 * @param reg_addr First register to read
 * @param data Destination for the register values
 * @param length Number of bytes to read
 * @param intf_ptr The i2c_master_dev_handle_t of the device
 * @return int8_t, 0 if ok, -1 on a bus error
 */
int8_t bosch_i2c_read(uint8_t reg_addr, uint8_t *data, uint32_t length, void *intf_ptr);

/**
 * @brief Register write callback for the Bosch sensor APIs
 *
 * @param reg_addr First register to write
 * @param data Register values to write
 * @param length Number of bytes to write
 * @param intf_ptr The i2c_master_dev_handle_t of the device
 * @return int8_t, 0 if ok, -1 on a bus error
 */
int8_t bosch_i2c_write(uint8_t reg_addr, const uint8_t *data, uint32_t length, void *intf_ptr);

/**
 * @brief Delay callback for the Bosch sensor APIs
 *
 * Busy waits below a millisecond and sleeps above it.
 *
 * @param period_us Minimum delay in microseconds
 * @param intf_ptr Unused
 */
void bosch_delay_us(uint32_t period_us, void *intf_ptr);

#ifdef __cplusplus
}
#endif

#endif // BOSCH_I2C_H
