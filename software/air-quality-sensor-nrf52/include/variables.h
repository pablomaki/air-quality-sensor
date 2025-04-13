#ifndef VARIABLES_H
#define VARIABLES_H

/**
 * @brief Enumeration for states
 *
 */
typedef enum
{
    NOT_SET,
    INITIALIZING,
    MEASURING,
    ADVERTISING,
    CONNECTED,
    DISCONNECTED,
    STANDBY
} state_t;

/**
 * @brief State change callback type
 *
 */
typedef void (*state_change_cb_t)(state_t new_state);

/**
 * @brief Set the state object
 *
 * @param new_state New state
 */
void set_state(state_t new_state);

/**
 * @brief Get the state object
 *
 * @return state_t, current state
 */
state_t get_state(void);

/**
 * @brief Register callback for state
 *
 * @param cb callback
 */
void register_state_callback(state_change_cb_t cb);

/**
 * @brief Set the battery level value
 *
 * @param new_battery_level new battery level in percent
 */
void set_battery_level(float new_battery_level);

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
 */
void set_temperature(float new_temperature);

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
 */
void set_humidity(float new_humidity);

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
 */
void set_pressure(float new_pressure);

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
 */
void set_co2_concentration(float new_co2_concentration);

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
 */
void set_voc_index(float new_voc_index);

/**
 * @brief Get the VOC index value
 *
 * @return float, current VOC index
 */
float get_voc_index(void);

#endif // VARIABLES_H