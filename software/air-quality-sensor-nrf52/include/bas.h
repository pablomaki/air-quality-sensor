#ifndef BAS_H
#define BAS_H

#include <stdint.h>

/** @brief Read battery_level value.
 *
 * Read the characteristic value of the battery_level
 *
 *  @return The battery_level.
 */
float bt_bas_get_battery_level(void);

/** @brief Update battery_level value.
 *
 * Update the characteristic value of the battery_level
 *
 *  @param new_battery_level The battery level in percent.
 *
 *  @return Zero in case of success and error code in case of error.
 */
int bt_bas_set_battery_level(float new_battery_level);



#endif // BAS_H
