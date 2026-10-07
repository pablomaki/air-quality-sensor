#ifndef AIR_QUALITY_MONITOR_H
#define AIR_QUALITY_MONITOR_H

/**
 * @brief Initialize air quality monitor
 *
 * Initializes the event handler, Matter and the sensors. A failing subsystem
 * dispatches INITIALIZATION_ERROR but does not abort: startup has to continue
 * so that whatever did come up stays operational and the node remains
 * reachable over Matter.
 *
 * @return int, 0 if ok, non-zero if any subsystem failed
 */
int init_air_quality_monitor(void);

/**
 * @brief Start air quality monitor by initiating the periodic task
 *
 * Enters the Matter task dispatch loop and does not return.
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int start_air_quality_monitor(void);

#endif // AIR_QUALITY_MONITOR_H