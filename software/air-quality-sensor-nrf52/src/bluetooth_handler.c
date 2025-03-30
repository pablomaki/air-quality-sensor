#include <bluetooth_handler.h>
#include <configs.h>
#include <variables.h>
#include <ess.h>
#include <bas.h>

#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>

LOG_MODULE_REGISTER(bluetooth_handler);

/**
 * @brief Object containing the advertised data
 *
 */
static const struct bt_data adv_data[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),                                       // Options
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_BAS_VAL), BT_UUID_16_ENCODE(BT_UUID_ESS_VAL))}; // Battery & Environmental sensing service

// Work queue for handling advertisement timeout
static struct k_work_delayable adv_stop_work;

/**
 * @brief Register connection callbacks
 *
 */
static struct bt_conn_cb conn_callbacks = {
    .connected = on_connect,
    .disconnected = on_disconnect,
};

static adv_stop_cb_t advertising_stopped_cb = NULL; // Callback for when advertising is stopped for state handling

static struct bt_conn *default_conn = NULL; // Connection tracking variable

int init_ble(adv_stop_cb_t cb)
{
    int err;
    err = bt_enable(NULL);
    if (err)
    {
        LOG_ERR("BLE enable failed (err %d)", err);
        return err;
    }

    // Register bluetooth callbacks for connection and disconnection
    bt_conn_cb_register(&conn_callbacks);

    // Register advertising stopped callback
	advertising_stopped_cb = cb;

    // Initialize work delayable queue for advertisement timeout handling
    k_work_init_delayable(&adv_stop_work, advertise_timeout);
    return 0;
}

void on_connect(struct bt_conn *conn, uint8_t err)
{
    if (err)
    {
        LOG_ERR("Connection failed (err %d)", err);
        return;
    }
    default_conn = bt_conn_ref(conn);
    LOG_INF("Device connected, stopping advertisement.");
    k_work_cancel_delayable(&adv_stop_work);
    stop_advertise();
}

void on_disconnect(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Device disconnected.");
    if (default_conn)
    {
        bt_conn_unref(default_conn);
        default_conn = NULL;
    }
}

void advertise_timeout(struct k_work *work)
{
    LOG_INF("Advertising timeout reached, stopping advertising");
    stop_advertise();
}

int update_advertisement_data()
{
    LOG_INF("Updating advertisement data");

    int err;
    int ret = 0;
    err = bt_bas_set_battery_level(get_battery_level());
    if (err)
    {
        LOG_WRN("Battery level outside of the expected limits (err %d, value %d)", err, (uint8_t)get_battery_level());
    }

#if defined(ENABLE_SHT4X) || defined(ENABLE_SCD4X)
    err = bt_ess_set_temperature(get_temperature());
    if (err)
    {
        LOG_WRN("Temperature outside of the expected limits (err %d, value %d)", err, (uint8_t)get_temperature());
    }
    err = bt_ess_set_humidity(get_humidity());
    if (err)
    {
        LOG_WRN("Humidity outside of the expected limits (err %d, value %d)", err, (uint8_t)get_humidity());
    }
#endif

#ifdef ENABLE_BMP280
    err = bt_ess_set_pressure(get_pressure());
    if (err)
    {
        LOG_WRN("Pressure outside of the expected limits (err %d, value %d)", err, (uint8_t)get_pressure());
    }
#endif

#ifdef ENABLE_SCD4X
    err = bt_ess_set_co2_concentration(get_co2_concentration());
    if (err)
    {
        LOG_WRN("CO2 concentration outside of the expected limits (err %d, value %d)", err, (uint8_t)get_co2_concentration());
    }
#endif

#ifdef ENABLE_SGP40
    err = bt_ess_set_voc_index(get_voc_index());
    if (err)
    {
        LOG_WRN("VOC index outside of the expected limits (err %d, value %d)", err, (uint8_t)get_voc_index());
    }
#endif
    return ret;
}

bool ble_connection_exists(void)
{
    return default_conn ? true : false;
}

int start_advertise(void)
{
    LOG_INF("Starting BLE advertisement");
    int err;
    err = bt_le_adv_start(BT_LE_ADV_CONN_ONE_TIME, adv_data, ARRAY_SIZE(adv_data), NULL, 0);
    if (err)
    {
        LOG_ERR("Failed to start data advertisement (err %d)", err);
        return err;
    }

    // Schedule timeout for stopping advertising
    err = k_work_schedule(&adv_stop_work, K_MSEC(ADV_TIMEOUT));
    if (err != 0 && err != 1)
    {
        LOG_ERR("Error scheduling a advertise timeout task (err %d)", err);
        return err;
    }
    return 0;
}

void stop_advertise(void)
{
    int err;
    err = bt_le_adv_stop();
    if (err)
    {
        LOG_ERR("Failed to stop data advertisement (err %d)", err);
    }
    advertising_stopped_cb();
}