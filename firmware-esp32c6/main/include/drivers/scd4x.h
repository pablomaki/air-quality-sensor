#ifndef SCD4X_H
#define SCD4X_H

#include <driver/i2c_master.h>
#include <esp_err.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Settings applied while the SCD4x is idle, before measurement starts
 */
typedef struct
{
    uint16_t altitude_m;
    float temperature_offset_c;
    uint16_t asc_initial_period_hours;
    uint16_t asc_standard_period_hours;
} scd4x_settings_t;

/**
 * @brief Probe the SCD4x at 0x62, apply the settings and start low power periodic measurement
 *
 * Low power periodic mode measures every 30 s, the mode the nRF build uses.
 *
 * @param bus Sensor bus
 * @param settings Settings to apply; ASC periods must be multiples of 4 hours
 * @return esp_err_t, ESP_OK if ok
 */
esp_err_t scd4x_init(i2c_master_bus_handle_t bus, const scd4x_settings_t *settings);

/**
 * @brief Set the ambient pressure used for CO2 compensation
 *
 * Allowed during periodic measurement and overrides the altitude setting.
 *
 * @param pressure_hpa Ambient pressure in hPa, 700-1200
 * @return esp_err_t, ESP_OK if ok
 */
esp_err_t scd4x_set_ambient_pressure(uint16_t pressure_hpa);

/**
 * @brief Read the latest measurement
 *
 * Returns the previous measurement again when no new one is ready, as the
 * Zephyr driver does. Before the first measurement completes, co2_ppm is 0.
 *
 * @param co2_ppm Destination for the CO2 concentration in ppm
 * @param temperature Destination for the temperature in degrees Celsius
 * @param humidity Destination for the relative humidity in percent
 * @return esp_err_t, ESP_OK if ok
 */
esp_err_t scd4x_read(uint16_t *co2_ppm, float *temperature, float *humidity);

#ifdef __cplusplus
}
#endif

#endif // SCD4X_H
