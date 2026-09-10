#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>

/**
 * @brief Initialize all sensors
 *
 * Probes every configured sensor and allocates the value buffers. A sensor
 * that is not ready is marked unavailable and skipped by read_sensors() for
 * the rest of this boot; initialization continues regardless, so one missing
 * sensor does not prevent the others from working.
 *
 * @return int, 0 if ok, non-zero if any sensor or the buffers failed
 */
int init_sensors(void);

/**
 * @brief Read data from each sensor
 *
 * Records every quantity in its value buffer, marked invalid when the read
 * fails. Sensors are read in dependency order because some share values: the
 * parts measuring temperature, humidity and pressure are read before the
 * SGP40 and SCD4x, which consume them for compensation.
 *
 * @return int, 0 if ok, non-zero if an error occured during any of the reads
 */
int read_sensors();

#endif // SENSORS_H
