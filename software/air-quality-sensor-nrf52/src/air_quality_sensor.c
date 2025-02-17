#include <air_quality_sensor.h>

#include <zephyr/logging/log.h>

#include <bluetooth_handler.h>

LOG_MODULE_REGISTER(air_quality_sensor);

int init_air_quality_sensor(void)
{
	LOG_INF("Initializing BLE.");
    int err;

    // Initialize bluetooth
    err = init_ble();
    if (err)
    {
		LOG_ERR("Error while initializing BLE (err %d)\n", err);
        return err;
    }
	LOG_INF("BLE initialized succesfully. BLE device \"%s\" online!", CONFIG_BT_DEVICE_NAME);

    // Initialize sensors

    // Setup periodic data advertisement
	LOG_INF("Starting periodic advertisement.");
    err = start_advertising();
    if (err)
    {
		LOG_ERR("Advertising failed to start (err %d)\n", err);
        return err;
    }
	LOG_INF("Advertising started.");

    return 0;
}