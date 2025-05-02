#ifndef VARIABLES_H
#define VARIABLES_H

#include <stdint.h>

/**
 * @brief Set the battery level value
 *
 * @param new_battery_level new battery level in percent
 * @param index index of the measurement
 */
void set_battery_level(float new_battery_level, uint8_t index);

/**
 * @brief Get the battery level value
 *
 * @return float, current battery level in percent
 */
float get_battery_level(void);

/**
 * @brief Set the temperature value
 *
 * @param new_temperature new temperature in Celcius
 * @param index index of the measurement
 */
void set_temperature(float new_temperature, uint8_t index);

/**
 * @brief Get the temperature value
 *
 * @return float, current temperature in Celcius
 */
float get_temperature(void);

/**
 * @brief Set the humidity value
 *
 * @param new_humidity new humidity in RH%
 * @param index index of the measurement
 */
void set_humidity(float new_humidity, uint8_t index);

/**
 * @brief Get the humidity value
 *
 * @return float, current humidity in RH%
 */
float get_humidity(void);

/**
 * @brief Set the pressure value
 *
 * @param new_pressure new pressure in Pascal
 * @param index index of the measurement
 */
void set_pressure(float new_pressure, uint8_t index);

/**
 * @brief Get the pressure value
 *
 * @return float, current pressure in Pascal
 */
float get_pressure(void);

/**
 * @brief Set the CO2 concentration value
 *
 * @param new_co2_concentration new CO2 concentration in ppm
 * @param index index of the measurement
 */
void set_co2_concentration(float new_co2_concentration, uint8_t index);

/**
 * @brief Get the CO2 concentration value
 *
 * @return float, current CO2 concentration in ppm
 */
float get_co2_concentration(void);

/**
 * @brief Set the VOC index value
 *
 * @param new_voc_index new VOC index
 * @param index index of the measurement
 */
void set_voc_index(float new_voc_index, uint8_t index);

/**
 * @brief Get the VOC index value
 *
 * @return float, current VOC index
 */
float get_voc_index(void);

#endif // VARIABLES_H