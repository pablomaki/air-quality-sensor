#ifndef FLASH_MANAGER_H
#define FLASH_MANAGER_H

/**
 * @brief Initialize flash manager
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int init_flash_manager(void);

/**
 * @brief Suspend the flash memory
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int suspend_flash(void);

/**
 * @brief Activate the flash memory
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int activate_flash(void);

#endif // FLASH_MANAGER_H