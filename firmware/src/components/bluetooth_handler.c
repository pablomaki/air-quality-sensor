#include <components/bluetooth_handler.h>
#include <utils/variable_buffer.h>
#include <ble_services/ess.h>
#include <ble_services/bas.h>

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
    BLE_ADVERTISING,
    BLE_CONNECTED,
    BLE_DISCONNECTED
} ble_state_t;

/**
 * @brief Object containing the advertised data
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
 * @brief Connection callbacks struct
 *
 */
static struct bt_conn_cb conn_callbacks = {
    .connected = on_connect,
    .disconnected = on_disconnect,
};

#ifdef CONFIG_ENABLE_CONN_FILTER_LIST
/**
 * @brief Struct for allowed connections
 *
 */
bt_addr_le_t phone_1_address = {
    .type = BT_ADDR_LE_RANDOM,
    .a = {{0x53, 0xC4, 0x5A, 0x49, 0xA9, 0x2D}},
};
bt_addr_le_t phone_2_address = {
    .type = BT_ADDR_LE_RANDOM,
    .a = {{0xA0, 0xFB, 0xC5, 0x84, 0x2C, 0x85}},
};
bt_addr_le_t rpi_address = {
    .type = BT_ADDR_LE_RANDOM,
    .a = {{0xD8, 0x3A, 0xDD, 0x02, 0xF0, 0x62}},
};

/**
 * @brief Struct for advertisement parameters
 *
 */
static const struct bt_le_adv_param adv_params_multi_whitelist = {
    .options =
        BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_FILTER_CONN | BT_LE_ADV_OPT_USE_IDENTITY | BT_LE_ADV_OPT_ONE_TIME,
    .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
    .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
    .peer = NULL, // NULL means use acceptlist (multiple allowed)
};
#endif

int init_ble()
{
    int rc = 0;
    rc = bt_enable(NULL);
    if (rc != 0)
    {
        LOG_ERR("BLE enable failed (err %d).", rc);
        return rc;
    }

    // Register bluetooth callbacks for connection and disconnection
    bt_conn_cb_register(&conn_callbacks);

    // Initialize work delayable queue for advertisement timeout handling
    k_work_init_delayable(&stop_ble_work, ble_timeout);

#ifdef CONFIG_ENABLE_CONN_FILTER_LIST
    // Register connectable addresses if filter is used
    bt_le_filter_accept_list_add(&phone_1_address);
    bt_le_filter_accept_list_add(&phone_2_address);
    bt_le_filter_accept_list_add(&rpi_address);
#endif
    set_ble_state(BLE_IDLE);
    return 0;
}

void register_ble_task_cb(ble_exit_cb_t cb)
{
    ble_exit_cb = cb;
}

void register_ble_connect_cb(ble_connect_cb_t cb)
{
    ble_connect_cb = cb;
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

#ifdef CONFIG_ENABLE_CONN_FILTER_LIST
    rc = bt_le_adv_start(&adv_params_multi_whitelist, adv_data, ARRAY_SIZE(adv_data), NULL, 0);
#else
    rc = bt_le_adv_start(BT_LE_ADV_CONN_ONE_TIME, adv_data, ARRAY_SIZE(adv_data), NULL, 0);
#endif
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