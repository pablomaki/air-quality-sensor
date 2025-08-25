#ifndef BLUETOOTH_HANDLER_H
#define BLUETOOTH_HANDLER_H

#include <zephyr/bluetooth/bluetooth.h>

/**
 * @brief BLE exit callback type
 *
 */
typedef void (*ble_exit_cb_t)(bool success);

/**
 * @brief BLE connect callback type
 *
 */
typedef void (*ble_connect_cb_t)(void);

/**
 * @brief Pairing result callback type
 *
 */
typedef void (*pairing_result_cb_t)(bool success);

/**
 * @brief Pairing complete callback type
 *
 */
typedef void (*pairing_complete_cb_t)(bool success);

/**
 * @brief Enable BLE
 *
 * @return int Zero for success, non-zero otherwise.
 */
int init_ble(void);

/**
 * @brief Register pairing result callback
 *
 * @param cb callback for when pairing result is available
 */
void register_pairing_result_cb(pairing_result_cb_t cb);

/**
 * @brief Register pairing complete callback
 *
 * @param cb callback for when pairing timeout occurs or pairign otherwise completes
 */
void register_pairing_complete_cb(pairing_complete_cb_t cb);

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
 * @brief Start pairing mode for specified timeout
 *
 * @return int Zero for success, non-zero otherwise.
 */
int start_pairing(void);

/**
 * @brief Check if there are any bonded devices
 *
 * @return true if bonded devices exist, false otherwise
 */
bool has_bonded_devices(void);

/**
 * @brief Unset pairing callbacks, set connection callbacks
 *
 * @return int Zero for success, non-zero otherwise.
 */
int setup_data_advertisement(void);

/**
 * @brief Update advertisement data
 *
 */
void update_advertisement_data(void);

/**
 * @brief Start advertising data. Setup timeout to end advertisement.
 *
 * @return int Zero for success, non-zero otherwise.
 */
int start_advertise(void);

#endif // BLUETOOTH_HANDLER_H