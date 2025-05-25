#include <components/e_paper_display.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>
#include <stdint.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(e_paper_display);

#define EPD_WIDTH 250
#define EPD_HEIGHT 136
#define EPD_BUF_SIZE (EPD_WIDTH * EPD_HEIGHT / 8)
struct display_buffer_descriptor desc = {
    .buf_size = EPD_BUF_SIZE,
    .width = EPD_WIDTH,
    .height = EPD_HEIGHT,
    .pitch = EPD_WIDTH,
};
static uint8_t screen_buffer[EPD_BUF_SIZE];
static const struct device *epd_dev;

int init_e_paper_display(void)
{
    epd_dev = DEVICE_DT_GET(DT_ALIAS(ssd1680));
    if (!device_is_ready(epd_dev))
    {
        LOG_ERR("Display device not ready");
        return -ENXIO;
    }
    display_blanking_off(epd_dev);
    return 0;
}

int update_e_paper_display(void)
{
    int err;
    // Fill with 0xFF for white, 0x00 for black
    memset(screen_buffer, 0xF0, sizeof(screen_buffer));

    err = display_write(epd_dev, 0, 0, &desc, screen_buffer);
    if (err)
    {
        LOG_ERR("Failed to write to display (err %d)", err);
        return err;
    }
    return 0;
}

int activate_epd(void)
{
    LOG_INF("Activating EPD");
    int err = 0;
    err = pm_device_action_run(epd_dev, PM_DEVICE_ACTION_RESUME);
    if (err)
    {
        LOG_ERR("Failed to activate EPD device (err, %d)", err);
    }
    return err;
}

int suspend_epd(void)
{
    LOG_INF("Suspending EPD");
    int err = 0;
    err = pm_device_action_run(epd_dev, PM_DEVICE_ACTION_SUSPEND);
    if (err)
    {
        LOG_ERR("Failed to suspend EPD device (err, %d)", err);
    }
    return err;
}