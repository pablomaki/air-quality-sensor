#ifndef BLUETOOTH_HANDLER_H
#define BLUETOOTH_HANDLER_H

#include <zephyr/bluetooth/bluetooth.h>

/**
 * @brief BLE exit callback type
 *
 */
typedef void (*ble_exit_cb_t)(bool);

/**
 * @brief BLE connect callback type
 *
 */
typedef void (*ble_connect_cb_t)(void);

/**
 * @brief Enable BLE
 *
 * @param exit_cb callback or when advertising is stopped
 * @param connect_cb callback or when connections is established
 * @return int Zero for success, non-zero otherwise.
 */
int init_ble(ble_exit_cb_t exit_cb, ble_connect_cb_t connect_cb);

/**
 * @brief Update advertisement data
 *
 * @return int Zero for success, non-zero otherwise.
 */
int update_advertisement_data();

/**
 * @brief Start advertising data. Setup timeout to end advertisement.
 *
 * @return int Zero for success, non-zero otherwise.
 */
int start_advertise(void);

#endif // BLUETOOTH_HANDLER_H