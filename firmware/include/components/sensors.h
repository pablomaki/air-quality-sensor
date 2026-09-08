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

#endif // SENSORS_H
