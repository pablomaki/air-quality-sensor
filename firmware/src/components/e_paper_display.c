#include <components/e_paper_display.h>
#include <utils/variables.h>
#include <utils/air_quality_mapper.h>
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
static lv_obj_t *voc_val_label;
static lv_obj_t *voc_tag_label;
static lv_obj_t *battery_percentage_label;

static char label_buffer[32];

int init_e_paper_display(void)
{
    epd_dev = DEVICE_DT_GET(DT_ALIAS(ssd1680));
    if (!device_is_ready(epd_dev))
    {
        LOG_ERR("Display device not ready");
        return -ENXIO;
    }

    // Set up temperature labels
    temp_tag_label = lv_label_create(lv_scr_act());
    temp_ms_label = lv_label_create(lv_scr_act());
    temp_ls_label = lv_label_create(lv_scr_act());
    temp_unit_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(temp_tag_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_font(temp_ms_label, &lv_font_montserrat_64, 0);
    lv_obj_set_style_text_font(temp_ls_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_font(temp_unit_label, &lv_font_montserrat_24, 0);
    lv_obj_align(temp_tag_label, LV_ALIGN_BOTTOM_RIGHT, -137, -115);
    lv_obj_align(temp_ms_label, LV_ALIGN_BOTTOM_RIGHT, -165, -59);
    lv_obj_align(temp_ls_label, LV_ALIGN_BOTTOM_LEFT, 85, -87);
    lv_obj_align(temp_unit_label, LV_ALIGN_BOTTOM_LEFT, 85, -66);
    snprintf(label_buffer, sizeof(label_buffer), "Temperature");
    lv_label_set_text(temp_tag_label, label_buffer);
    snprintf(label_buffer, sizeof(label_buffer), "\xB0"
                                                 "C");
    lv_label_set_text(temp_unit_label, label_buffer);

    // Set up humidity labels
    hum_tag_label = lv_label_create(lv_scr_act());
    hum_val_label = lv_label_create(lv_scr_act());
    hum_unit1_label = lv_label_create(lv_scr_act());
    hum_unit2_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(hum_tag_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_font(hum_val_label, &lv_font_montserrat_64, 0);
    lv_obj_set_style_text_font(hum_unit1_label, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_font(hum_unit2_label, &lv_font_montserrat_20, 0);
    lv_obj_align(hum_tag_label, LV_ALIGN_BOTTOM_RIGHT, -137, -54);
    lv_obj_align(hum_val_label, LV_ALIGN_BOTTOM_RIGHT, -165, 3);
    lv_obj_align(hum_unit1_label, LV_ALIGN_BOTTOM_LEFT, 85, -23);
    lv_obj_align(hum_unit2_label, LV_ALIGN_BOTTOM_LEFT, 82, -6);
    snprintf(label_buffer, sizeof(label_buffer), "Humidity");
    lv_label_set_text(hum_tag_label, label_buffer);
    snprintf(label_buffer, sizeof(label_buffer), "%%");
    lv_label_set_text(hum_unit1_label, label_buffer);
    snprintf(label_buffer, sizeof(label_buffer), "RH");
    lv_label_set_text(hum_unit2_label, label_buffer);

    // Set up CO2 labels
    co2_tag1_label = lv_label_create(lv_scr_act());
    co2_tag2_label = lv_label_create(lv_scr_act());
    co2_val_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(co2_tag1_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_font(co2_tag2_label, &lv_font_montserrat_10, 0);
    lv_obj_set_style_text_font(co2_val_label, &lv_font_montserrat_48, 0);
    lv_obj_align(co2_tag1_label, LV_ALIGN_BOTTOM_RIGHT, 0, -115);
    lv_obj_align(co2_tag2_label, LV_ALIGN_BOTTOM_RIGHT, -50, -114);
    lv_obj_align(co2_val_label, LV_ALIGN_BOTTOM_RIGHT, 0, -68);
    snprintf(label_buffer, sizeof(label_buffer), "CO  (ppm)");
    lv_label_set_text(co2_tag1_label, label_buffer);
    snprintf(label_buffer, sizeof(label_buffer), "2");
    lv_label_set_text(co2_tag2_label, label_buffer);

    // Set up VOC labels
    voc_val_label = lv_label_create(lv_scr_act());
    voc_tag_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(voc_tag_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_font(voc_val_label, &lv_font_montserrat_28, 0);
    lv_obj_align(voc_tag_label, LV_ALIGN_BOTTOM_RIGHT, 0, -55);
    lv_obj_align(voc_val_label, LV_ALIGN_BOTTOM_RIGHT, 0, -28);
    snprintf(label_buffer, sizeof(label_buffer), "Air quality");
    lv_label_set_text(voc_tag_label, label_buffer);

    // Set up battery percentage label
    battery_percentage_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(battery_percentage_label, &lv_font_montserrat_20, 0);
    lv_obj_align(battery_percentage_label, LV_ALIGN_BOTTOM_RIGHT, -3, -6);

    // Turn off display blanking for ???
    display_blanking_off(epd_dev);
    return 0;
}

int update_e_paper_display(void)
{
    float temp = get_mean(TEMPERATURE);
    float hum = get_mean(HUMIDITY);
    float co2 = get_mean(CO2_CONCENTRATION);
    float voc = get_mean(VOC_INDEX);
    float bat = get_mean(BATTERY_LEVEL);

    snprintf(label_buffer, sizeof(label_buffer), "%d", (int)temp);
    lv_label_set_text(temp_ms_label, label_buffer);
    snprintf(label_buffer, sizeof(label_buffer), ".%01d", (int)((temp - (int)temp) * 10));
    lv_label_set_text(temp_ls_label, label_buffer);

    snprintf(label_buffer, sizeof(label_buffer), "%d", (int)hum);
    lv_label_set_text(hum_val_label, label_buffer);

    snprintf(label_buffer, sizeof(label_buffer), "%d", (int)co2);
    lv_label_set_text(co2_val_label, label_buffer);

    snprintf(label_buffer, sizeof(label_buffer), "%s", air_quality_from_voc_index((int)voc));
    lv_label_set_text(voc_val_label, label_buffer);

    snprintf(label_buffer, sizeof(label_buffer), "BATT: %d%%", (int)bat);
    lv_label_set_text(battery_percentage_label, label_buffer);

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