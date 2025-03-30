#ifndef BATTERY_H
#define BATTERY_H

/**
 * @brief Read SHT4X sensor data and save the temperature, humidity and CO2 levels to the variables
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int read_battery_level(void);

#endif // BATTERY_H