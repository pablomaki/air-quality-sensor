#ifndef SENSORS_H
#define SENSORS_H

/**
 * @brief Initialize all sensors
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int init_sensors(void);

/**
 * @brief Read SHT4X sensor data and save the temperature and humidity to the variables
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int read_sht4x_data();

/**
 * @brief Read SGP4X sensor data and save the VOC index to the variables
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int read_sgp4x_data();

/**
 * @brief Read SHT4X sensor data and save the pressure to the variables
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int read_bmp280_data();

/**
 * @brief Read SHT4X sensor data and save the temperature, humidity and CO2 levels to the variables
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int read_scd4x_data();

#endif // SENSORS_H
