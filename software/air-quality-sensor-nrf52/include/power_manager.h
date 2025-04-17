#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

/**
 * @brief Initialize power manager by forcing the external flash to low power mode
 * 
 * @return int, 0 if ok, non-zero if an error occured
 */
int init_power_manager(void);

/**
 * @brief Enter low power mode, SYSTEM_OFF if ENABLE_SYSTEM_OFF is defined and SYSTEM_ON IDLE otherwise
 * 
 * @return int, 0 if ok, non-zero if an error occured
 */
int enter_low_power_mode(void);

#endif // POWER_MANAGER_H