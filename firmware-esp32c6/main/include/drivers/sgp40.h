#ifndef SGP40_H
#define SGP40_H

#include <driver/i2c_master.h>
#include <esp_err.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Compensation the SGP40 assumes when no ambient values are available */
#define SGP40_DEFAULT_TEMPERATURE 25.0f
#define SGP40_DEFAULT_HUMIDITY 50.0f

/**
 * @brief Probe the SGP40 at 0x59
 *
 * @param bus Sensor bus
 * @return esp_err_t, ESP_OK if ok
 */
esp_err_t sgp40_init(i2c_master_bus_handle_t bus);

/**
 * @brief Measure the raw VOC signal, compensated for ambient conditions
 *
 * @param temperature Ambient temperature in degrees Celsius, clamped to -45-130
 * @param humidity Ambient relative humidity in percent, clamped to 0-100
 * @param raw Destination for the raw signal, input to the gas index algorithm
 * @return esp_err_t, ESP_OK if ok
 */
esp_err_t sgp40_measure_raw(float temperature, float humidity, uint16_t *raw);

#ifdef __cplusplus
}
#endif

#endif // SGP40_H
