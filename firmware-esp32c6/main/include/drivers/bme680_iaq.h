#ifndef BME680_IAQ_H
#define BME680_IAQ_H

#include <driver/i2c_master.h>
#include <esp_err.h>

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Latest BSEC outputs for the BME680
 */
typedef struct
{
    float temperature;
    float humidity;
    float pressure_pa;
    uint16_t iaq;
    float co2_equivalent;
    float voc_equivalent;
    uint8_t iaq_accuracy;
    uint8_t co2_accuracy;
    uint8_t voc_accuracy;
    bool gas_run_in;
    bool gas_stabilized;
} bme680_iaq_output_t;

/**
 * @brief Probe the BME680 at 0x77 and start the BSEC processing task
 *
 * Mirrors the nRF Connect SDK bme68x_iaq driver: BSEC drives the sensor in low
 * power mode (one measurement every 3 s) on its own task, independent of the
 * application's sampling, and its calibration state is restored from and
 * periodically saved to NVS so a restart does not lose the IAQ calibration.
 *
 * @param bus Sensor bus
 * @return esp_err_t, ESP_OK if ok
 */
esp_err_t bme680_iaq_init(i2c_master_bus_handle_t bus);

/**
 * @brief Copy the latest BSEC outputs
 *
 * @param output Destination for the outputs
 * @return esp_err_t, ESP_OK if ok, ESP_ERR_INVALID_STATE before the first output
 */
esp_err_t bme680_iaq_read(bme680_iaq_output_t *output);

#ifdef __cplusplus
}
#endif

#endif // BME680_IAQ_H
