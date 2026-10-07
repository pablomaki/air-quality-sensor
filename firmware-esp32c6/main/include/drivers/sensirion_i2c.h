#ifndef SENSIRION_I2C_H
#define SENSIRION_I2C_H

#include <driver/i2c_master.h>
#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief I2C clock shared by every sensor on the bus, matching I2C_BITRATE_FAST on nRF */
#define SENSOR_I2C_SPEED_HZ 400000

/**
 * @brief Sleep for at least the given time
 *
 * pdMS_TO_TICKS() rounds down, which turns a short conversion wait into no wait
 * at all on a coarse tick. This rounds up and adds the partial tick in progress.
 *
 * @param ms Minimum time to sleep in milliseconds
 */
static inline void sensor_delay_ms(uint32_t ms)
{
    vTaskDelay((ms + portTICK_PERIOD_MS - 1) / portTICK_PERIOD_MS + 1);
}

/**
 * @brief Create the sensor I2C bus on the carrier PCB pins
 *
 * @param bus Destination for the bus handle
 * @return esp_err_t, ESP_OK if ok
 */
esp_err_t sensor_i2c_bus_init(i2c_master_bus_handle_t *bus);

/**
 * @brief Add a device at a 7-bit address to the sensor bus
 *
 * @param bus Sensor bus
 * @param address 7-bit I2C address
 * @param dev Destination for the device handle
 * @return esp_err_t, ESP_OK if ok, ESP_ERR_NOT_FOUND if nothing acknowledges the address
 */
esp_err_t sensor_i2c_add_device(i2c_master_bus_handle_t bus, uint16_t address, i2c_master_dev_handle_t *dev);

/**
 * @brief Sensirion CRC-8 (polynomial 0x31, init 0xFF) over one 16-bit word
 *
 * @param word Big-endian word as transmitted
 * @return uint8_t, the checksum byte that follows the word on the wire
 */
uint8_t sensirion_crc(const uint8_t word[2]);

/**
 * @brief Send a 16-bit command followed by CRC protected argument words
 *
 * @param dev Device handle
 * @param command Command code
 * @param args Argument words, may be NULL when count is 0
 * @param count Number of argument words
 * @return esp_err_t, ESP_OK if ok
 */
esp_err_t sensirion_write_command(i2c_master_dev_handle_t dev, uint16_t command, const uint16_t *args, size_t count);

/**
 * @brief Read CRC protected 16-bit words
 *
 * @param dev Device handle
 * @param words Destination for the words
 * @param count Number of words to read
 * @return esp_err_t, ESP_OK if ok, ESP_ERR_INVALID_CRC if any checksum mismatches
 */
esp_err_t sensirion_read_words(i2c_master_dev_handle_t dev, uint16_t *words, size_t count);

#ifdef __cplusplus
}
#endif

#endif // SENSIRION_I2C_H
