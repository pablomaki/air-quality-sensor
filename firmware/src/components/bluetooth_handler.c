#include <components/bluetooth_handler.h>
#include <utils/variable_buffer.h>
#include <ble_services/ess.h>
#include <ble_services/bas.h>

#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(bluetooth_handler);

/**
 * @brief Enumeration for BLE state
 *
 */
typedef enum
{
    BLE_STATE_NOT_SET,
    BLE_IDLE,
    BLE_PAIRING,
    BLE_ADVERTISING,
    BLE_CONNECTED,
    BLE_DISCONNECTED
} ble_state_t;

/**
 *@brief Struct containing the pairing advertisement data
 *
 *
 */
static const struct bt_data pairing_data[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),                        // Options
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, (sizeof(CONFIG_BT_DEVICE_NAME) - 1))}; // Device name

/**
 * @brief Struct containing the advertised data
 *
 */
static const struct bt_data adv_data[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)), // Options
    BT_DATA_BYTES(BT_DATA_UUID16_ALL,
#ifdef CONFIG_ENABLE_BATTERY_MONITOR
                  BT_UUID_16_ENCODE(BT_UUID_BAS_VAL),
#endif
                  BT_UUID_16_ENCODE(BT_UUID_ESS_VAL))}; // Battery & Environmental sensing service

/**
 * @brief Work queue for handling pairing timeout
 *
 */
static struct k_work_delayable pairing_timeout_work;

/**
 * @brief Work queue for handling ble timeout
 *
 */
static struct k_work_delayable stop_ble_work;

/**
 * @brief Current BLE state
 *
 */
static ble_state_t ble_state = BLE_STATE_NOT_SET;

/**
 * @brief Stores the current connection information
 *
 */
static struct bt_conn *current_conn = NULL;

/**
 * @brief Callback for pairing results
 *
 */
static pairing_result_cb_t pairing_result_cb = NULL;

/**
 * @brief Callback for pairing completion
 *
 */
static pairing_result_cb_t pairing_complete_cb = NULL;

/**
 * @brief Callback for when connection is closed or timeout is triggered
 *
 */
static ble_exit_cb_t ble_exit_cb = NULL;

/**
 * @brief Callback for when connection is closed or timeout is triggered
 *
 */
static ble_connect_cb_t ble_connect_cb = NULL;

/**
 * @brief Set the ble state
 *
 * @param new_state new ble state
 */
static void set_ble_state(ble_state_t new_state)
{
    ble_state = new_state;
}

/**
 * @brief Get the ble state object
 *
 * @return current ble state
 */
static ble_state_t get_ble_state(void)
{
    return ble_state;
}

/**
 * @brief Callback for when disconnected,
 * cancel scheduled timeout and stop advertising
 *
 *  @param conn Connection object.
 *  @param reason BT_HCI_ERR_* reason for the disconnection.
 */
static void on_disconnect(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Device disconnected.");
    k_work_cancel_delayable(&stop_ble_work);
    set_ble_state(BLE_DISCONNECTED);

    if (current_conn)
    {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }

    // Report successfull BLE termination as the central device disconnected
    ble_exit_cb((reason == BT_HCI_ERR_REMOTE_USER_TERM_CONN) ? true : false);
}

/**
 * @brief Stop advertising
 *
 */
void stop_advertise(void)
{
    int rc = 0;
    rc = bt_le_adv_stop();
    if (rc != 0)
    {
        LOG_ERR("Failed to stop data advertisement (err %d).", rc);
    }
}

/**
 * @brief Callback for on connection
 *
 *  @param conn New connection object.
 *  @param err HCI error. Zero for success, non-zero otherwise.
 */
static void on_connect(struct bt_conn *conn, uint8_t err)
{
    if (err != 0)
    {
        LOG_ERR("Connection failed (err %d).", err);
        return;
    }
    current_conn = bt_conn_ref(conn);
    const bt_addr_le_t *addr = bt_conn_get_dst(conn);
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

    LOG_INF("Device connected (%s), stopping advertisement.", addr_str);
    stop_advertise();
    ble_connect_cb();
    set_ble_state(BLE_CONNECTED);
}

/**
 * @brief Disconnect from currently connected central device
 *
 */
void disconnect(void)
{
    if (current_conn)
    {
        int rc = 0;
        rc = bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        if (rc != 0)
        {
            LOG_ERR("Failed to disconnect from device (err %d).", rc);
        }
    }
    set_ble_state(BLE_IDLE);
}

/**
 * @brief Callback when security changes
 * This callback will be utilized during pairing and bonding to remove all prior prior should the first pairing attempt fail.
 *
 * @param conn Connection object
 * @param level Security level
 * @param err Security error
 */
static void security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
    if (err != BT_SECURITY_ERR_SUCCESS)
    {
        LOG_INF("Security failed: level %d err %d.", level, err);
        // Remove bond if security fails
        bt_addr_le_t *addr = bt_conn_get_dst(conn);
        bt_unpair(BT_ID_DEFAULT, addr);
        LOG_INF("Old bond removed.");
    }
}

static struct bt_conn_cb pairing_conn_callbacks = {
    .security_changed = security_changed,
};

/**
 * @brief Security callback for pairing complete
 *
 * @param conn Connection object
 * @param bonded True if bonding was successful
 */
static void pairing_complete(struct bt_conn *conn, bool bonded)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr_str, sizeof(addr_str));
    if (bonded)
    {
        LOG_INF("Device paired and bonded successfully: %s, stopping advertising.", addr_str);
        k_work_cancel_delayable(&pairing_timeout_work);
        stop_advertise();
        disconnect();
        pairing_result_cb(true);
        pairing_complete_cb(true);
    }
    else
    {
        LOG_WRN("Device paired but not bonded: %s", addr_str);
        disconnect();
        pairing_result_cb(false);
    }
}

/**
 * @brief Security callback for pairing failed
 *
 * @param conn Connection object
 * @param reason Pairing failure reason
 */
static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)

{
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr_str, sizeof(addr_str));
    LOG_WRN("Pairing failed with %s, reason: %d", addr_str, reason);
    disconnect();
    pairing_result_cb(false);
}

/**
 * @brief Callback for when the timeout timer for BLE functionality runs out.
 * Stop advertising or disconnect the current connection.
 * Call the ble_exit_cb in the end
 *
 * @param work Work item
 */
static void ble_timeout(struct k_work *work)
{
    if (get_ble_state() == BLE_ADVERTISING)
    {
        LOG_INF("BLE timeout reached, stopping advertising.");
        stop_advertise();
        set_ble_state(BLE_IDLE);
        ble_exit_cb(false);
    }
    else if (get_ble_state() == BLE_CONNECTED)
    {
        LOG_INF("BLE timeout reached, disconnecting from connected device.");
        disconnect();
    }
}

/**
 * @brief Pairing timeout callback
 *
 * @param work Work item
 */
static void pairing_timeout(struct k_work *work)
{
    LOG_INF("Pairing timeout reached, stopping advertising.");
    stop_advertise();
    disconnect();
    pairing_complete_cb(false);
}

/**
 * @brief Pairing callbacks strucs
 *
 */
static struct bt_conn_auth_info_cb auth_info_cb = {
    .pairing_complete = pairing_complete,
    .pairing_failed = pairing_failed,
};

/**
 * @brief Connection callbacks struct
 *
 */
static struct bt_conn_cb conn_callbacks = {
    .connected = on_connect,
    .disconnected = on_disconnect,
};

/**
 * @brief Advertisement parameters for multiple devices in whitelist mode
 *
 */
static const struct bt_le_adv_param adv_params_multi_whitelist = {
    .options =
        BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_FILTER_CONN | BT_LE_ADV_OPT_USE_IDENTITY,
    .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
    .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
    .peer = NULL, // NULL means use acceptlist (multiple allowed)
};

int init_ble()
{
    int rc = 0;

    // Enable settings subsystem for persistent bonding
    rc = settings_subsys_init();
    if (rc != 0)
    {
        LOG_ERR("Settings subsystem init failed (err %d).", rc);
        return rc;
    }

    // Enable BT
    rc = bt_enable(NULL);
    if (rc != 0)
    {
        LOG_ERR("BLE enable failed (err %d).", rc);
        return rc;
    }

    // Load settings from flash
    rc = settings_load();
    if (rc != 0)
    {
        LOG_ERR("Loading settings failed (err %d).", rc);
        return rc;
    }

    // Initialize delayable pairing timeout work
    k_work_init_delayable(&pairing_timeout_work, pairing_timeout);

    // Initialize delayable work for advertisement timeout handling
    k_work_init_delayable(&stop_ble_work, ble_timeout);

    set_ble_state(BLE_IDLE);
    return 0;
}

void register_pairing_result_cb(pairing_result_cb_t cb)
{
    pairing_result_cb = cb;
}

void register_pairing_complete_cb(pairing_complete_cb_t cb)
{
    pairing_complete_cb = cb;
}

void register_ble_task_cb(ble_exit_cb_t cb)
{
    ble_exit_cb = cb;
}

void register_ble_connect_cb(ble_connect_cb_t cb)
{
    ble_connect_cb = cb;
}

/**
 * @brief Helper function for counting bonds bonds
 *
 * @param info
 * @param user_data
 */
static void count_bonds(const struct bt_bond_info *info, void *user_data)
{
    (*(int *)user_data) += 1;
}

bool has_bonded_devices(void)

{
    int count = 0;
    bt_foreach_bond(BT_ID_DEFAULT, count_bonds, &count);
    LOG_INF("Bonds: %d", count);
    return count > 0;
}

int start_pairing()
{
    LOG_INF("Starting BLE pairing mode.");
    int rc = 0;
    set_ble_state(BLE_PAIRING);

    // Register bluetooth callbacks for pairing complete and failure as well as security changes
    rc = bt_conn_auth_info_cb_register(&auth_info_cb);
    if (rc != 0)
    {
        LOG_ERR("Failed to start pairing callbacks (err %d).", rc);
        return rc;
    }
    rc = bt_conn_cb_register(&pairing_conn_callbacks);
    if (rc != 0)
    {
        LOG_ERR("Failed to register pairing connection callbacks (err %d).", rc);
        return rc;
    }

    // Start advertising for pairing (connectable by any device)
    rc = bt_le_adv_start(BT_LE_ADV_CONN, pairing_data, ARRAY_SIZE(pairing_data), NULL, 0);
    if (rc != 0)
    {
        LOG_ERR("Failed to start pairing advertisement (err %d).", rc);
        return rc;
    }

    // Schedule pairing timeout
    rc = k_work_schedule(&pairing_timeout_work, K_MSEC(CONFIG_PAIRING_TIMEOUT));
    if (rc != 0 && rc != 1)
    {
        LOG_ERR("Error scheduling pairing timeout task (err %d).", rc);
        stop_advertise();
        return rc;
    }
    return 0;
}

int setup_data_advertisement(void)
{
    LOG_INF("Setting up normal advertisement.");
    int rc = 0;

    // Unregister pairing callbacks
    rc = bt_conn_cb_unregister(&pairing_conn_callbacks);
    if (rc != 0)
    {
        LOG_ERR("Failed to unregister pairing connection callbacks (err %d).", rc);
        return rc;
    }
    rc = bt_conn_auth_info_cb_unregister(&auth_info_cb);
    if (rc != 0)
    {
        LOG_ERR("Failed to unregister pairing callbacks (err %d).", rc);
        return rc;
    }

    // Register connection callbacks
    rc = bt_conn_cb_register(&conn_callbacks);
    if (rc != 0)
    {
        LOG_ERR("Failed to register connection callbacks (err %d).", rc);
        return rc;
    }
    return rc;
}

void update_advertisement_data(void)
{
    LOG_INF("Updating advertisement data.");

    int rc = 0;
    rc = bt_bas_set_battery_level(get_mean(BATTERY_LEVEL));
    if (rc != 0)
    {
        LOG_WRN("Battery level outside of the expected limits (err %d, value %d).", rc, (uint16_t)(BATTERY_LEVEL));
    }

#ifdef CONFIG_ENABLE_SHT4X
    rc = bt_ess_set_temperature(get_mean(TEMPERATURE));
    if (rc != 0)
    {
        LOG_WRN("Temperature outside of the expected limits (err %d, value %d).", rc, (uint16_t)get_mean(TEMPERATURE));
    }
    rc = bt_ess_set_humidity(get_mean(HUMIDITY));
    if (rc != 0)
    {
        LOG_WRN("Humidity outside of the expected limits (err %d, value %d).", rc, (uint16_t)get_mean(HUMIDITY));
    }
#endif

#ifdef CONFIG_ENABLE_BMP390
    rc = bt_ess_set_pressure(get_mean(PRESSURE));
    if (rc != 0)
    {
        LOG_WRN("Pressure outside of the expected limits (err %d, value %d).", rc, (uint16_t)get_mean(PRESSURE));
    }
#endif

#ifdef CONFIG_ENABLE_SCD4X
    rc = bt_ess_set_co2_concentration(get_mean(CO2_CONCENTRATION));
    if (rc != 0)
    {
        LOG_WRN("CO2 concentration outside of the expected limits (err %d, value %d).", rc, (uint16_t)get_mean(CO2_CONCENTRATION));
    }
#endif

#ifdef CONFIG_ENABLE_SGP40
    rc = bt_ess_set_voc_index(get_mean(VOC_INDEX));
    if (rc != 0)
    {
        LOG_WRN("VOC index outside of the expected limits (err %d, value %d).", rc, (uint16_t)get_mean(VOC_INDEX));
    }
#endif
}

int start_advertise(void)
{
    LOG_INF("Starting BLE advertisement.");
    int rc = 0;

    rc = bt_le_adv_start(BT_LE_ADV_CONN_ONE_TIME, adv_data, ARRAY_SIZE(adv_data), NULL, 0);
    if (rc != 0)
    {
        LOG_ERR("Failed to start data advertisement (err %d).", rc);
        return rc;
    }
    set_ble_state(BLE_ADVERTISING);

    // Schedule timeout for stopping advertising
    rc = k_work_schedule(&stop_ble_work, K_MSEC(CONFIG_BLE_TIMEOUT));
    if (rc != 0 && rc != 1)
    {
        LOG_ERR("Error scheduling a advertise timeout task (err %d).", rc);
        stop_advertise();
        return rc;
    }
    return 0;
}