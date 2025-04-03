#include <power_manager.h>

#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/policy.h>
#include <zephyr/drivers/spi.h>

// #include <zephyr/sys/poweroff.h>

LOG_MODULE_REGISTER(power_manager);

int init_power_manager(void)
{
    int err;

    static const struct device *qspi_dev = DEVICE_DT_GET(DT_ALIAS(spi_flash0));
    err = pm_device_action_run(qspi_dev, PM_DEVICE_ACTION_SUSPEND);
	if (err) {
		LOG_INF("cannot suspend QSPI device\n");
	}
    return 0;
}

int enter_low_power_mode(void)
{
	// sys_poweroff();
    return 0;
}