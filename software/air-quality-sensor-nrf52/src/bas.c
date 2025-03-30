#include <bas.h>

#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/gatt.h>

LOG_MODULE_REGISTER(bas);

static uint8_t battery_level = 0; // Battery level

static bool batt_lvl_notif_enabled = false; // Track if notifications are enabled

static struct bt_gatt_attr * batt_lvl_handle;

static void batt_lvl_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	batt_lvl_notif_enabled = (value == BT_GATT_CCC_NOTIFY);
}

ssize_t read_batt_lvl(struct bt_conn *conn,
								  const struct bt_gatt_attr *attr, void *buf,
								  uint16_t len, uint16_t offset)
{
	const uint8_t *sensor_value_ptr = (const uint8_t *)attr->user_data;
	return bt_gatt_attr_read(conn, attr, buf, len, offset, sensor_value_ptr, sizeof(*sensor_value_ptr));
}

BT_GATT_SERVICE_DEFINE(bas,
					   BT_GATT_PRIMARY_SERVICE(BT_UUID_BAS),
					   BT_GATT_CHARACTERISTIC(BT_UUID_BAS_BATTERY_LEVEL, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ, read_batt_lvl, NULL, &battery_level),
					   BT_GATT_CCC(batt_lvl_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE), );

float bt_bas_get_battery_level(void)
{
	return (float)battery_level;
}

int bt_bas_set_battery_level(float new_battery_level)
{
	int ret = 0;
	if (new_battery_level > 100U)
	{
		ret = -EINVAL;
	}
	battery_level = (uint8_t)new_battery_level;
	if (batt_lvl_notif_enabled)
	{
		ret = bt_gatt_notify(NULL, &bas.attrs[1], &battery_level, sizeof(battery_level));
	}
	return ret == -ENOTCONN ? 0 : ret;
}

/**
 * @brief Initialize handles for characteristics
 * 
 * @return int 
 */
static int bas_init(void)
{
    batt_lvl_handle = bt_gatt_find_by_uuid(bas.attrs, 0, BT_UUID_BAS_BATTERY_LEVEL);
	return 0;
}

SYS_INIT(bas_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);