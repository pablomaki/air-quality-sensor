#ifndef ESS_H
#define ESS_H

/** @brief Read temperature value.
 *
 * Read the characteristic value of the temperature
 *
 *  @return The temperature in Celcius.
 */
float bt_ess_get_temperature(void);

/** @brief Read humidity value.
 *
 * Read the characteristic value of the humidity
 *
 *  @return The humidity in RH%.
 */
float bt_ess_get_humidity(void);

/** @brief Read pressure value.
 *
 * Read the characteristic value of the pressure
 *
 *  @return The pressure in Pascal.
 */
float bt_ess_get_pressure(void);

/** @brief Read CO2 concentration value.
 *
 * Read the characteristic value of the CO2 concentration
 *
 *  @return The CO2 concentration in ppm.
 */
float bt_ess_get_co2_concentration(void);

/** @brief Read VOC index value.
 *
 * Read the characteristic value of the VOC index
 *
 *  @return The VOC index.
 */
float bt_ess_get_voc_index(void);

/** @brief Update temperature value.
 *
 * Update the characteristic value of the temperature
 *
 *  @param new_temperature The temperature in Celcius.
 *
 *  @return Zero in case of success and error code in case of error.
 */
int bt_ess_set_temperature(float new_temperature);

/** @brief Update humidity value.
 *
 * Update the characteristic value of the humidity
 *
 *  @param new_humidity The new humidity in RH%.
 *
 *  @return Zero in case of success and error code in case of error.
 */
int bt_ess_set_humidity(float new_humidity);

/** @brief Update pressure value.
 *
 * Update the characteristic value of the pressure
 *
 *  @param new_pressure The new pressure in Pascal.
 *
 *  @return Zero in case of success and error code in case of error.
 */
int bt_ess_set_pressure(float new_pressure);

/** @brief Update CO2 concentration value.
 *
 * Update the characteristic value of the CO2 concentration
 *
 *  @param new_co2_concentration The new CO2 concentration in ppm.

 *  @return Zero in case of success and error code in case of error.
 */
int bt_ess_set_co2_concentration(float new_co2_concentration);

/** @brief Update VOC index value.
 *
 * Update the characteristic value of the VOC index
 *
 *  @param new_voc_index The new VOC index.
 *
 *  @return Zero in case of success and error code in case of error.
 */
int bt_ess_set_voc_index(float new_voc_index);

#endif // ESS_H
