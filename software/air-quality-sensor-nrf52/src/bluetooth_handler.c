#include <bluetooth_handler.h>

#include <zephyr/logging/log.h>
// #include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
// #include <zephyr/bluetooth/uuid.h>
// #include <zephyr/bluetooth/services/bas.h>

LOG_MODULE_REGISTER(bluetooth_handler);

struct bt_le_ext_adv *extended_advertiser;

static const struct bt_data extended_adv_data[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static const struct bt_data periodic_ad_data[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID16_ALL, BT_UUID_16_ENCODE(BT_UUID_BAS_VAL), BT_UUID_16_ENCODE(BT_UUID_ESS_VAL))};

/* Register the callback */
static struct bt_le_per_adv_sync_cb sync_cb = {
    .synced = per_adv_sync_cb,
    .term = per_adv_sync_lost_cb,
};

bool device_synchronized = false;

int init_ble(void)
{
    int err;
    err = bt_enable(NULL);
    if (err)
    {
        return err;
    }
    return 0;
}

void per_adv_sync_cb(struct bt_le_per_adv_sync *sync, struct bt_le_per_adv_sync_synced_info *info)
{
    int err;
    // char addr_str[BT_ADDR_LE_STR_LEN];
    device_synchronized = true;

    /* Print the device's Bluetooth address */
    // bt_addr_le_to_str(&info->addr, addr_str, sizeof(addr_str));
    // LOG_INF("Device Address: %s\n", addr_str);

    /* Stop Extended Advertising if needed */
    err = bt_le_ext_adv_stop(extended_advertiser);
    if (err)
    {
        LOG_ERR("Failed to stop extended advertising (err %d)\n", err);
    }
}

void per_adv_sync_lost_cb(struct bt_le_per_adv_sync *sync, const struct bt_le_per_adv_sync_term_info *info)
{
    int err;
    LOG_INF("A device lost synchronization!\n");
    err = restart_edvertising();
    if (err)
    {
        LOG_ERR("Failed to restart advertising (err %d)\n", err);
    }
}

int restart_edvertising(void)
{
    int err;
    err = bt_le_per_adv_stop(extended_advertiser);
    if (err)
    {
        LOG_ERR("Failed to stop extended advertising (err %d)\n", err);
        return err;
    }

    err = start_advertising();
    if (err)
    {
        LOG_ERR("Failed to restart advertising (err %d)\n", err);
        return err;
    }
    return 0;
}

int start_advertising()
{
    int err;

    /* Create a non-connectable non-scannable advertising set */
    err = bt_le_ext_adv_create(BT_LE_EXT_ADV_NCONN, NULL, &extended_advertiser);
    if (err)
    {
        LOG_ERR("Failed to create advertising set (err %d)\n", err);
        return err;
    }

    /* Set advertising data to have complete local name set */
    err = bt_le_ext_adv_set_data(extended_advertiser, extended_adv_data, ARRAY_SIZE(extended_adv_data), NULL, 0);
    if (err)
    {
        LOG_ERR("Failed to set advertising data (err %d)\n", err);
        return err;
    }

    /* Set periodic advertising parameters */
    err = bt_le_per_adv_set_param(extended_advertiser, BT_LE_PER_ADV_DEFAULT);
    if (err)
    {
        LOG_ERR("Failed to set periodic advertising parameters (err %d)\n", err);
        return err;
    }

    /* Enable Periodic Advertising */
    err = bt_le_per_adv_start(extended_advertiser);
    if (err)
    {
        LOG_ERR("Failed to enable periodic advertising (err %d)\n", err);
        return 0;
    }

    err = bt_le_ext_adv_start(extended_advertiser, BT_LE_EXT_ADV_START_DEFAULT);
    if (err)
    {
        LOG_ERR("Failed to start extended advertising (err %d)\n", err);
        return err;
    }

    bt_le_per_adv_sync_cb_register(&sync_cb);
    return 0;
}

int update_advertising_data()
{
    int err;
    err = bt_le_per_adv_set_data(extended_advertiser, periodic_ad_data, ARRAY_SIZE(periodic_ad_data));
    if (err)
    {
        LOG_ERR("Failed to set periodic ad data(err %d)\n", err);
        return err;
    }
    return 0;
}
