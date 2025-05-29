#include <components/e_paper_display.h>
#include <utils/variables.h>
#include <utils/lv_font_montserrat_64.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>
#include <stdint.h>
#include <lvgl.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(e_paper_display);
LV_FONT_DECLARE(lv_font_montserrat_64);
#define EPD_WIDTH 250
#define EPD_HEIGHT 136
#define EPD_BUF_SIZE (EPD_WIDTH * EPD_HEIGHT / 8)
struct display_buffer_descriptor desc = {
    .buf_size = EPD_BUF_SIZE,
    .width = EPD_WIDTH,
    .height = EPD_HEIGHT,
    .pitch = EPD_WIDTH,
};
// static uint8_t screen_buffer[EPD_BUF_SIZE];
static const struct device *epd_dev;
static lv_obj_t *temp_tag_label;
static lv_obj_t *temp_ms_label;
static lv_obj_t *temp_ls_label;
static lv_obj_t *temp_unit_label;
static lv_obj_t *hum_tag_label;
static lv_obj_t *hum_val_label;
static lv_obj_t *hum_unit1_label;
static lv_obj_t *hum_unit2_label;
static lv_obj_t *co2_tag1_label;
static lv_obj_t *co2_tag2_label;
static lv_obj_t *co2_val_label;
// static lv_obj_t *co2_unit1_label;
// static lv_obj_t *co2_unit2_label;
static lv_obj_t *voc_val_label;
static lv_obj_t *voc_tag_label;
// static lv_obj_t *voc_unit1_label;
// static lv_obj_t *voc_unit2_label;
// static lv_obj_t *pressure_label;
static lv_obj_t *battery_percentage_label;

int init_e_paper_display(void)
{
    epd_dev = DEVICE_DT_GET(DT_ALIAS(ssd1680));
    if (!device_is_ready(epd_dev))
    {
        LOG_ERR("Display device not ready");
        return -ENXIO;
    }

    temp_tag_label = lv_label_create(lv_scr_act());
    temp_ms_label = lv_label_create(lv_scr_act());
    temp_ls_label = lv_label_create(lv_scr_act());
    temp_unit_label = lv_label_create(lv_scr_act());
    hum_tag_label = lv_label_create(lv_scr_act());
    hum_val_label = lv_label_create(lv_scr_act());
    hum_unit1_label = lv_label_create(lv_scr_act());
    hum_unit2_label = lv_label_create(lv_scr_act());
    co2_tag1_label = lv_label_create(lv_scr_act());
    co2_tag2_label = lv_label_create(lv_scr_act());
    co2_val_label = lv_label_create(lv_scr_act());
    // co2_unit1_label = lv_label_create(lv_scr_act());
    // co2_unit2_label = lv_label_create(lv_scr_act());
    voc_val_label = lv_label_create(lv_scr_act());
    voc_tag_label = lv_label_create(lv_scr_act());
    // voc_unit1_label = lv_label_create(lv_scr_act());
    // voc_unit2_label = lv_label_create(lv_scr_act());
    battery_percentage_label = lv_label_create(lv_scr_act());

    lv_obj_set_style_text_font(temp_tag_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_font(temp_ms_label, &lv_font_montserrat_64, 0);
    lv_obj_set_style_text_font(temp_ls_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_font(temp_unit_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_font(hum_tag_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_font(hum_val_label, &lv_font_montserrat_64, 0);
    lv_obj_set_style_text_font(hum_unit1_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_font(hum_unit2_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(co2_tag1_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_font(co2_tag2_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_font(co2_val_label, &lv_font_montserrat_48, 0);
    // lv_obj_set_style_text_font(co2_unit1_label, &lv_font_montserrat_16, 0);
    // lv_obj_set_style_text_font(co2_unit2_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_font(voc_tag_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_font(voc_val_label, &lv_font_montserrat_48, 0);
    // lv_obj_set_style_text_font(voc_unit1_label, &lv_font_montserrat_16, 0);
    // lv_obj_set_style_text_font(voc_unit2_label, &lv_font_montserrat_16, 0);
    // lv_obj_set_style_text_font(pressure_label, &lv_font_montserrat_26, 0);
    lv_obj_set_style_text_font(battery_percentage_label, &lv_font_montserrat_18, 0);

    lv_obj_align(temp_tag_label, LV_ALIGN_BOTTOM_RIGHT, -137, -115);
    lv_obj_align(temp_ms_label, LV_ALIGN_BOTTOM_RIGHT, -165, -59);
    lv_obj_align(temp_ls_label, LV_ALIGN_BOTTOM_LEFT, 85, -87);
    lv_obj_align(temp_unit_label, LV_ALIGN_BOTTOM_LEFT, 85, -66);

    lv_obj_align(hum_tag_label, LV_ALIGN_BOTTOM_RIGHT, -137, -54);
    lv_obj_align(hum_val_label, LV_ALIGN_BOTTOM_RIGHT, -165, 3);
    lv_obj_align(hum_unit1_label, LV_ALIGN_BOTTOM_LEFT, 85, -23);
    lv_obj_align(hum_unit2_label, LV_ALIGN_BOTTOM_LEFT, 82, -6);

    // lv_obj_align(co2_val_label, LV_ALIGN_BOTTOM_RIGHT, -40, -91);
    // lv_obj_align(co2_unit1_label, LV_ALIGN_BOTTOM_LEFT, 210, -108);
    // lv_obj_align(co2_unit2_label, LV_ALIGN_BOTTOM_LEFT, 210, -95);
    lv_obj_align(co2_tag1_label, LV_ALIGN_BOTTOM_RIGHT, 0, -115);
    lv_obj_align(co2_tag2_label, LV_ALIGN_BOTTOM_RIGHT, -98, -114);
    lv_obj_align(co2_val_label, LV_ALIGN_BOTTOM_RIGHT, 0, -72);
    // lv_obj_align(co2_unit1_label, LV_ALIGN_BOTTOM_LEFT, 210, -108);
    // lv_obj_align(co2_unit2_label, LV_ALIGN_BOTTOM_LEFT, 210, -95);

    // lv_obj_align(voc_val_label, LV_ALIGN_BOTTOM_RIGHT, -40, -56);
    // lv_obj_align(voc_unit1_label, LV_ALIGN_BOTTOM_LEFT, 210, -73);
    // lv_obj_align(voc_unit2_label, LV_ALIGN_BOTTOM_LEFT, 210, -60);
    lv_obj_align(voc_tag_label, LV_ALIGN_BOTTOM_RIGHT, 0, -62);
    lv_obj_align(voc_val_label, LV_ALIGN_BOTTOM_RIGHT, 0, -19);

    // lv_obj_align(pressure_label, LV_ALIGN_BOTTOM_RIGHT, 0, -31);

    lv_obj_align(battery_percentage_label, LV_ALIGN_BOTTOM_RIGHT, 0, -8);

    display_blanking_off(epd_dev);
    return 0;
}

int update_e_paper_display(void)
{
    int err;
    float temp = get_mean(TEMPERATURE);
    float hum = get_mean(HUMIDITY);
    float press = get_mean(PRESSURE);
    float co2 = get_mean(CO2_CONCENTRATION);
    float voc = get_mean(VOC_INDEX);
    float bat = get_mean(BATTERY_LEVEL);

    char buf[32];
    snprintf(buf, sizeof(buf), "Temperature");
    lv_label_set_text(temp_tag_label, buf);
    snprintf(buf, sizeof(buf), "%d", (int)temp);
    lv_label_set_text(temp_ms_label, buf);
    snprintf(buf, sizeof(buf), ".%01d", (int)((temp - (int)temp) * 10));
    lv_label_set_text(temp_ls_label, buf);
    snprintf(buf, sizeof(buf), "\xB0"
                               "C");
    lv_label_set_text(temp_unit_label, buf);

    snprintf(buf, sizeof(buf), "Humidity");
    lv_label_set_text(hum_tag_label, buf);
    snprintf(buf, sizeof(buf), "%d", (int)hum);
    lv_label_set_text(hum_val_label, buf);
    snprintf(buf, sizeof(buf), "%%");
    lv_label_set_text(hum_unit1_label, buf);
    snprintf(buf, sizeof(buf), "RH");
    lv_label_set_text(hum_unit2_label, buf);

    // static lv_point_t line_points[] = {{5, 5}, {70, 70}, {120, 10}, {180, 60}, {240, 10}};
    // /*Create style*/
    // static lv_style_t style_line;
    // lv_style_init(&style_line);
    // lv_style_set_line_width(&style_line, 8);
    // lv_style_set_line_rounded(&style_line, true);

    // /*Create a line and apply the new style*/
    // lv_obj_t *line1;
    // line1 = lv_line_create(lv_scr_act());
    // lv_line_set_points(line1, line_points, 5); /*Set the points*/
    // lv_obj_add_style(line1, &style_line, 0);
    // lv_obj_center(line1);

    // snprintf(buf, sizeof(buf), "%dhPa", (int)press / 100);
    // lv_label_set_text(pressure_label, buf);

    // snprintf(buf, sizeof(buf), "%dppm", (int)co2);
    // lv_label_set_text(co2_concentration_label, buf);
    snprintf(buf, sizeof(buf), "CO  conc. (ppm)");
    lv_label_set_text(co2_tag1_label, buf);
    snprintf(buf, sizeof(buf), "2");
    lv_label_set_text(co2_tag2_label, buf);
    snprintf(buf, sizeof(buf), "%d", (int)co2);
    lv_label_set_text(co2_val_label, buf);
    // snprintf(buf, sizeof(buf), "CO2");
    // lv_label_set_text(co2_unit1_label, buf);
    // snprintf(buf, sizeof(buf), "ppm");
    // lv_label_set_text(co2_unit2_label, buf);

    // snprintf(buf, sizeof(buf), "%dVOC", (int)voc);
    // lv_label_set_text(voc_index_label, buf);
    snprintf(buf, sizeof(buf), "VOC index", (int)voc);
    lv_label_set_text(voc_tag_label, buf);
    snprintf(buf, sizeof(buf), "%d", (int)voc);
    lv_label_set_text(voc_val_label, buf);
    // snprintf(buf, sizeof(buf), "VOC");
    // lv_label_set_text(voc_unit1_label, buf);
    // snprintf(buf, sizeof(buf), "IDX");
    // lv_label_set_text(voc_unit2_label, buf);

    snprintf(buf, sizeof(buf), "Battery: %d%%", (int)bat);
    lv_label_set_text(battery_percentage_label, buf);

    lv_task_handler();

    return 0;
}

int activate_epd(void)
{
    LOG_INF("Activating EPD");
    int err;
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
    int err;
    err = pm_device_action_run(epd_dev, PM_DEVICE_ACTION_SUSPEND);
    if (err)
    {
        LOG_ERR("Failed to suspend EPD device (err, %d)", err);
    }
    return err;
}