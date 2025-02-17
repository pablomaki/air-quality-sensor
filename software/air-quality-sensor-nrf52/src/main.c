
#include <zephyr/logging/log.h>

#include <air_quality_sensor.h>

// #include <zephyr/kernel.h>
// #include <zephyr/logging/log.h>

// #include <zephyr/bluetooth/bluetooth.h>
// #include <zephyr/bluetooth/conn.h>
// #include <zephyr/bluetooth/gatt.h>
// #include <zephyr/bluetooth/uuid.h>
// #include <zephyr/bluetooth/services/bas.h>

LOG_MODULE_REGISTER(main);

// int16_t temperature = 2012;
// int16_t humidity  = 3023;
// int16_t co2conc  = 4034;
// int16_t vocconc  = 5045;
// int16_t pressure  = 6056;

// ssize_t read_temperature(struct bt_conn *conn,
// 						 const struct bt_gatt_attr *attr, void *buf,
// 						 uint16_t len, uint16_t offset)
// {
// 	return bt_gatt_attr_read(conn, attr, buf, len, offset, &temperature, sizeof(temperature));
// }
// ssize_t read_humidity(struct bt_conn *conn,
// 						 const struct bt_gatt_attr *attr, void *buf,
// 						 uint16_t len, uint16_t offset)
// {
// 	return bt_gatt_attr_read(conn, attr, buf, len, offset, &humidity, sizeof(humidity));
// }
// ssize_t read_co2conc(struct bt_conn *conn,
// 						 const struct bt_gatt_attr *attr, void *buf,
// 						 uint16_t len, uint16_t offset)
// {
// 	return bt_gatt_attr_read(conn, attr, buf, len, offset, &co2conc, sizeof(co2conc));
// }
// ssize_t read_vocconc(struct bt_conn *conn,
// 						 const struct bt_gatt_attr *attr, void *buf,
// 						 uint16_t len, uint16_t offset)
// {
// 	return bt_gatt_attr_read(conn, attr, buf, len, offset, &vocconc, sizeof(vocconc));
// }
// ssize_t read_pressure(struct bt_conn *conn,
// 						 const struct bt_gatt_attr *attr, void *buf,
// 						 uint16_t len, uint16_t offset)
// {
// 	return bt_gatt_attr_read(conn, attr, buf, len, offset, &pressure, sizeof(pressure));
// }

// BT_GATT_SERVICE_DEFINE(ess_srv, BT_GATT_PRIMARY_SERVICE(BT_UUID_ESS),
// 					   BT_GATT_CHARACTERISTIC(BT_UUID_TEMPERATURE, BT_GATT_CHRC_READ, BT_GATT_PERM_READ, read_temperature, NULL, NULL),
// 					   BT_GATT_CHARACTERISTIC(BT_UUID_HUMIDITY, BT_GATT_CHRC_READ, BT_GATT_PERM_READ, read_humidity, NULL, NULL),
// 					   BT_GATT_CHARACTERISTIC(BT_UUID_GATT_CO2CONC, BT_GATT_CHRC_READ, BT_GATT_PERM_READ, read_co2conc, NULL, NULL),
// 					   BT_GATT_CHARACTERISTIC(BT_UUID_GATT_VOCCONC, BT_GATT_CHRC_READ, BT_GATT_PERM_READ, read_vocconc, NULL, NULL),
// 					   BT_GATT_CHARACTERISTIC(BT_UUID_PRESSURE, BT_GATT_CHRC_READ, BT_GATT_PERM_READ, read_pressure, NULL, NULL), );

int main(void)
{
	LOG_INF("Initializing the air quality sensor.");
	int err;

	// Initialize the air quality sensor
	err = init_air_quality_sensor();
	if (err)
	{
		LOG_ERR("Error while initializing the air quality sensor (err %d)\n", err);
		return err;
	}

	LOG_INF("Initialization complete, exiting main.");
	return 0;
}

// 	LOG_INF("LOOPP!\n");
// 	while (true)
// 	{
// 		k_msleep(2000);

// 		if (battery_level < 25)
// 		{
// 			battery_level = 100;
// 		}
// 		else
// 		{
// 			battery_level--;
// 		}
// 		bt_bas_set_battery_level(battery_level);
// 	}

// 	return 0;
// }
