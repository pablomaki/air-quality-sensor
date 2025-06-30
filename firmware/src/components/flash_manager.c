#include <components/flash_manager.h>

#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/pm/device.h>

LOG_MODULE_REGISTER(flash_manager);

static const struct device *qspi_dev;

int init_flash_manager(void)
{
    qspi_dev = DEVICE_DT_GET(DT_ALIAS(spi_flash0));
    if (!device_is_ready(qspi_dev))
    {
        LOG_ERR("Device P25Q16H is not ready.");
        return -ENXIO;
    }
    return 0;
}

int activate_flash(void)
{
    LOG_INF("Activating flash.");
    int rc = 0;
    rc = pm_device_action_run(qspi_dev, PM_DEVICE_ACTION_RESUME);
    if (rc != 0)
    {
        LOG_ERR("Failed to activate P25Q16H (err %d).", rc);
    }
    return rc;
}

int suspend_flash(void)
{
    LOG_INF("Suspending flash.");
    int rc = 0;
    rc = pm_device_action_run(qspi_dev, PM_DEVICE_ACTION_SUSPEND);
    if (rc != 0)
    {
        LOG_ERR("Failed to suspend P25Q16H (err %d).", rc);
    }
    return rc;
}