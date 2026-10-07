#ifndef BMP390_H
#define BMP390_H

#include <driver/i2c_master.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Probe the BMP390 at 0x77 and start normal mode measurement
 *
 * Same configuration as the nRF build's devicetree node: 12.5 Hz output data
 * rate, 8x pressure and 1x temperature oversampling, IIR filter coefficient 1.
 *
 * @param bus Sensor bus
 * @return esp_err_t, ESP_OK if ok
 */
esp_err_t bmp390_init(i2c_master_bus_handle_t bus);

/**
 * @brief Read the latest compensated measurement
 *
 * @param pressure_hpa Destination for the pressure in hPa
 * @param temperature Destination for the temperature in degrees Celsius
 * @return esp_err_t, ESP_OK if ok
 */
esp_err_t bmp390_read(float *pressure_hpa, float *temperature);

#ifdef __cplusplus
}
#endif

#endif // BMP390_H
