#ifndef AIR_QUALITY_MONITOR_H
#define AIR_QUALITY_MONITOR_H

/**
 * @brief Initialize air quality monitor
 * Initialize BLE, LED controller, sensors and the timed task
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int init_air_quality_monitor(void);

#endif // AIR_QUALITY_MONITOR_H