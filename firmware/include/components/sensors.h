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
 * @brief Read data from each sensor
 *
 * @return int, 0 if ok, non-zero if an error occured during any of the reads
 */
int read_sensors();

/**
 * @brief Suspend the sensors
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int suspend_sensors(void);

/**
 * @brief Activate the sensors
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int activate_sensors(void);

#endif // SENSORS_H
