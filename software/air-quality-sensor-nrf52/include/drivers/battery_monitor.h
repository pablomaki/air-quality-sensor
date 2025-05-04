#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <stdint.h>

/**
 * @brief Initialize the battery level reader
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int init_battery_monitor(void);

/**
 * @brief Read SHT4X sensor data and save the temperature, humidity and CO2 levels to the variables
 *
 * @param index Index of the measurement
 * @return int, 0 if ok, non-zero if an error occured
 */
int read_battery_level(uint8_t index);

#endif // BATTERY_MONITOR_H