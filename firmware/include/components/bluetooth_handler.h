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
 * @return int Zero for success, non-zero otherwise.
 */
int init_ble();

/**
 * @brief Register pairing complete callback
 *
 * @param cb callback for when pairing timeout occurs or pairing succeeds
 */
void register_ble_task_cb(ble_exit_cb_t cb);

/**
 * @brief Register pairing complete callback
 *
 * @param cb callback for when pairing timeout occurs or pairing succeeds
 */
void register_ble_connect_cb(ble_connect_cb_t cb);

/**
 * @brief Update advertisement data
 *
 */
void update_advertisement_data();

/**
 * @brief Start advertising data. Setup timeout to end advertisement.
 *
 * @return int Zero for success, non-zero otherwise.
 */
int start_advertise(void);

#endif // BLUETOOTH_HANDLER_H