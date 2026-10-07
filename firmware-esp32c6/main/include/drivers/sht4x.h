#ifndef SHT4X_H
#define SHT4X_H

#include <driver/i2c_master.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Probe and soft reset the SHT4x at 0x44
 *
 * @param bus Sensor bus
 * @return esp_err_t, ESP_OK if ok
 */
esp_err_t sht4x_init(i2c_master_bus_handle_t bus);

/**
 * @brief Take one high repeatability measurement
 *
 * @param temperature Destination for the temperature in degrees Celsius
 * @param humidity Destination for the relative humidity in percent, cropped to 0-100
 * @return esp_err_t, ESP_OK if ok
 */
esp_err_t sht4x_measure(float *temperature, float *humidity);

#ifdef __cplusplus
}
#endif

#endif // SHT4X_H
