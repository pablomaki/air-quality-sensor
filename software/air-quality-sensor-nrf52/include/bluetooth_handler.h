#ifndef BLUETOOTH_HANDLER_H
#define BLUETOOTH_HANDLER_H

#include <zephyr/bluetooth/bluetooth.h>


/**
 * @brief Advertising stopped callback type
 *
 */
typedef void (*adv_stop_cb_t)(void);

/**
 * @brief Enable BLE
 *
 * @param cb callback or when advertising is stopped
 * @return int Zero for success, non-zero otherwise.
 */
int init_ble(adv_stop_cb_t cb);

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

/**
 * @brief Advertising timeout, stop advertising
 *
 * @param work Address of work item.
 */
void advertise_timeout(struct k_work *work);

/**
 * @brief Update advertisement data
 *
 * @return int Zero for success, non-zero otherwise.
 */
int update_advertisement_data();

/**
 * @brief Check if a functioning connection already exists
 * 
 * @return true if connected
 * @return false if not connected
 */
bool ble_connection_exists(void);

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

#endif // BLUETOOTH_HANDLER_H