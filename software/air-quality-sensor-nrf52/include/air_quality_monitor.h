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

bool read_sensors(void);
bool advertise_data(void);
bool schedule_work_task(int64_t delay);
void ble_task_callback(bool task_success);

#endif // AIR_QUALITY_MONITOR_H