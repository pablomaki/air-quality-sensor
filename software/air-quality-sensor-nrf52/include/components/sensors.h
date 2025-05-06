#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>

/**
 * @brief Initialize all sensors
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int init_sensors(void);

/**
 * @brief Read SHT4X sensor data and save the temperature and humidity to the variables
 *
 * @param index Index of the measurement
 * @return int, 0 if ok, non-zero if an error occured
 */
int read_sht4x_data(uint8_t index);

/**
 * @brief Read SGP40 sensor data and save the VOC index to the variables
 *
 * @param index Index of the measurement
 * @return int, 0 if ok, non-zero if an error occured
 */
int read_sgp40_data(uint8_t index);

/**
 * @brief Read BMP390 sensor data and save the pressure to the variables
 *
 * @param index Index of the measurement
 * @return int, 0 if ok, non-zero if an error occured
 */
int read_bmp390_data(uint8_t index);

/**
 * @brief Read SCD4X sensor data and save the temperature, humidity and CO2 levels to the variables
 *
 * @param index Index of the measurement
 * @return int, 0 if ok, non-zero if an error occured
 */
int read_scd4x_data(uint8_t index);

#endif // SENSORS_H
