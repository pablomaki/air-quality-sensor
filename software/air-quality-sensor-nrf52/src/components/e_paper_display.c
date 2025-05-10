#include <components/e_paper_display.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <stdint.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(e_paper_display);

#define EPD_WIDTH 250
#define EPD_HEIGHT 128
#define EPD_BUF_SIZE (EPD_WIDTH * EPD_HEIGHT / 8)
struct display_buffer_descriptor desc = {
    .buf_size = EPD_BUF_SIZE,
    .width = EPD_WIDTH,
    .height = EPD_HEIGHT,
    .pitch = EPD_WIDTH,
};
static bool white = true;
static uint8_t screen_buffer[EPD_BUF_SIZE];

// const struct device *const epd_dev_p = DEVICE_DT_GET_ANY(weact_zjy1222);
static const struct device *epd_dev = DEVICE_DT_GET(DT_NODELABEL(ssd1680));

int init_e_paper_display(void)
{
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
    // Fill with 0xFF for white, 0x00 for black
    memset(screen_buffer, 0xF0, sizeof(screen_buffer));

    display_write(epd_dev, 0, 0, &desc, screen_buffer);
    // display_blanking_on(epd_dev);
    white = !white;
    return 0;
}
