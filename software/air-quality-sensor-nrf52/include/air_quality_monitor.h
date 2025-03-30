#ifndef AIR_QUALITY_MONITOR_H
#define AIR_QUALITY_MONITOR_H

#include <zephyr/kernel.h>

/**
 * @brief Initialize air quality monitor
 * Initialize BLE, LED controller, sensors and the timed task
 * 
 * @return int, 0 if ok, non-zero if an error occured
 */
int init_air_quality_monitor(void);

/**
 * @brief Periodic task that takes care of reading sensor data and advertising it over BLE
 * 
 * @param work Address of work item.
 */
void periodic_task(struct k_work *work);

/**
 * @brief Callback for when advertising is stopped, set standby state
 * 
 */
void stop_advertising_cb(void);

#endif // AIR_QUALITY_MONITOR_H