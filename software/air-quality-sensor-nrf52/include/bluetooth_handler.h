#ifndef BLUETOOTH_HANDLER_H
#define BLUETOOTH_HANDLER_H

#include <zephyr/bluetooth/bluetooth.h>

/**
 * @brief Enumeration for BLE state
 *
 */
typedef enum
{
    BLE_STATE_NOT_SET,
    BLE_IDLE,
    BLE_ADVERTISING,
    BLE_CONNECTED,
    BLE_DISCONNECTED
} ble_state_t;

/**
 * @brief BLE exit callback type
 *
 */
typedef void (*ble_exit_cb_t)(bool);

/**
 * @brief Enable BLE
 *
 * @param cb callback or when advertising is stopped
 * @return int Zero for success, non-zero otherwise.
 */
int init_ble(ble_exit_cb_t cb);


void set_ble_state(ble_state_t new_state);


ble_state_t get_ble_state(void);

/**
 * @brief Callback for on connection
 *
 *  @param conn New connection object.
 *  @param err HCI error. Zero for success, non-zero otherwise.
 */
void on_connect(struct bt_conn *conn, uint8_t err);

/**
 * @brief Callback for when disconnected,
 * cancel scheduled timeout and stop advertising
 *
 *  @param conn Connection object.
 *  @param reason BT_HCI_ERR_* reason for the disconnection.
 */
void on_disconnect(struct bt_conn *conn, uint8_t reason);

void ble_timeout(struct k_work *work);

/**
 * @brief Update advertisement data
 *
 * @return int Zero for success, non-zero otherwise.
 */
int update_advertisement_data();

/**
 * @brief Start advertising data. Setup timeout to end data
 * unless device is connected & disconnected before the timeout
 *
 * @return int Zero for success, non-zero otherwise.
 */
int start_advertise(void);

/**
 * @brief Stop advertising
 * 
 */
void stop_advertise(void);

void disconnect(void);

#endif // BLUETOOTH_HANDLER_H