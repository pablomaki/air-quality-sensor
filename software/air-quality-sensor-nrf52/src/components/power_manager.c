#include <components/power_manager.h>
#include <configs.h>

#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/policy.h>
#include <zephyr/sys/poweroff.h>

LOG_MODULE_REGISTER(power_manager);

static const struct device *qspi_dev = DEVICE_DT_GET(DT_ALIAS(spi_flash0));
static const struct device *i2c1_dev = DEVICE_DT_GET(DT_NODELABEL(i2c1));

int init_power_manager(void)
{
    int err;

    err = pm_device_action_run(qspi_dev, PM_DEVICE_ACTION_SUSPEND);
    if (err)
    {
        LOG_ERR("Failed to suspend QSPI device (err, %d)", err);
    }
    return 0;
}

int enter_low_power_mode(void)
{
    int err;

    err = pm_device_action_run(i2c1_dev, PM_DEVICE_ACTION_SUSPEND);
    if (err)
    {
        LOG_ERR("Failed to suspend I2C device (err, %d)", err);
    }

#ifdef ENABLE_SYSTEM_OFF
    // Actual implementation missing, this is just a palceholder
    sys_poweroff();
    k_sleep(K_MSEC(1000));
    err = wake_up();
    if (err)
    {
        LOG_ERR("Wakeup failed (err, %d)", err);
        return err;
    }
#endif
    return 0;
}

int wake_up(void)
{
    LOG_INF("Waking up from the sleep.");

    int err;

    err = pm_device_action_run(i2c1_dev, PM_DEVICE_ACTION_RESUME);
    if (err)
    {
        LOG_ERR("Failed to wake up I2C device (err, %d)", err);
        return err;
    }
    return 0;
}