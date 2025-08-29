#include <components/e_paper_display.h>
#include <utils/variable_buffer.h>
#include <utils/air_quality_mapper.h>

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

static lv_obj_t *notif_label;
static lv_obj_t *temp_tag_label;
static lv_obj_t *temp_val_label;
static lv_obj_t *hum_tag_label;
static lv_obj_t *hum_val_label;
static lv_obj_t *co2_tag_label;
static lv_obj_t *co2_val_label;
static lv_obj_t *voc_tag_label;
static lv_obj_t *voc_val_label;
static lv_obj_t *batt_tag_label;
static lv_obj_t *batt_val_label;

static char label_buffer[32];

int init_e_paper_display(void)
{
    epd_dev = DEVICE_DT_GET(DT_ALIAS(ssd1680));
    if (!device_is_ready(epd_dev))
    {
        LOG_ERR("Display device not ready.");
        return -ENXIO;
    }

    // Configure LVGL for rotated display
    lv_disp_t *disp = lv_disp_get_default();
    if (disp)
    {
        lv_disp_set_rotation(disp, LV_DISP_ROT_90);
    }

    // Setup notification label
    notif_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(notif_label, &lv_font_montserrat_24, 0);
    lv_obj_align(notif_label, LV_ALIGN_CENTER, 0, 0);

    // Set up temperature labels
    temp_tag_label = lv_label_create(lv_scr_act());
    temp_val_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(temp_tag_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_font(temp_val_label, &lv_font_montserrat_32, 0);
    lv_obj_align(temp_tag_label, LV_ALIGN_TOP_LEFT, 10, 0);
    lv_obj_align(temp_val_label, LV_ALIGN_TOP_RIGHT, -12, 18);

    // Set up humidity labels
    hum_tag_label = lv_label_create(lv_scr_act());
    hum_val_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(hum_tag_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_font(hum_val_label, &lv_font_montserrat_32, 0);
    lv_obj_align(hum_tag_label, LV_ALIGN_TOP_LEFT, 10, 52);
    lv_obj_align(hum_val_label, LV_ALIGN_TOP_RIGHT, -12, 70);

    // Set up CO2 labels
    co2_tag_label = lv_label_create(lv_scr_act());
    co2_val_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(co2_tag_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_font(co2_val_label, &lv_font_montserrat_32, 0);
    lv_obj_align(co2_tag_label, LV_ALIGN_TOP_LEFT, 10, 104);
    lv_obj_align(co2_val_label, LV_ALIGN_TOP_RIGHT, -12, 122);

    // Set up VOC labels
    voc_val_label = lv_label_create(lv_scr_act());
    voc_tag_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(voc_tag_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_font(voc_val_label, &lv_font_montserrat_24, 0);
    lv_obj_align(voc_tag_label, LV_ALIGN_TOP_LEFT, 10, 156);
    lv_obj_align(voc_val_label, LV_ALIGN_TOP_RIGHT, -12, 173);

    // Set up battery percentage label
    batt_tag_label = lv_label_create(lv_scr_act());
    batt_val_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(batt_tag_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_font(batt_val_label, &lv_font_montserrat_32, 0);
    lv_obj_align(batt_tag_label, LV_ALIGN_TOP_LEFT, 10, 198);
    lv_obj_align(batt_val_label, LV_ALIGN_TOP_RIGHT, -12, 215);

    // Turn off display blanking to activate partial updates
    display_blanking_off(epd_dev);

    return 0;
}

int display_notification(const char *message)
{
    lv_label_set_text(notif_label, "");
    lv_label_set_text(temp_tag_label, "");
    lv_label_set_text(temp_val_label, "");
    lv_label_set_text(hum_tag_label, "");
    lv_label_set_text(hum_val_label, "");
    lv_label_set_text(co2_tag_label, "");
    lv_label_set_text(co2_val_label, "");
    lv_label_set_text(voc_val_label, "");
    lv_label_set_text(voc_tag_label, "");
    lv_label_set_text(batt_tag_label, "");
    lv_label_set_text(batt_val_label, "");
    lv_label_set_text(notif_label, message);
    lv_task_handler();
    return 0;
}

int update_e_paper_display(void)
{
    static int refresh_count = 0;

    lv_label_set_text(notif_label, "");
    snprintf(label_buffer, sizeof(label_buffer), "Temperature");
    lv_label_set_text(temp_tag_label, label_buffer);
    snprintf(label_buffer, sizeof(label_buffer), "Humidity");
    lv_label_set_text(hum_tag_label, label_buffer);
    snprintf(label_buffer, sizeof(label_buffer), "CO2 (ppm)");
    lv_label_set_text(co2_tag_label, label_buffer);
    snprintf(label_buffer, sizeof(label_buffer), "Air quality");
    lv_label_set_text(voc_tag_label, label_buffer);
    snprintf(label_buffer, sizeof(label_buffer), "Battery");
    lv_label_set_text(batt_tag_label, label_buffer);

    // Set temperature
    float temp = get_latest(TEMPERATURE);
    if (temp == -1)
    {
        lv_label_set_text(temp_val_label, "n/a");
    }
    else
    {
        snprintf(label_buffer, sizeof(label_buffer), "%d.%01d"
                                                     "\xB0"
                                                     "C",
                 (int)temp, (int)((temp - (int)temp) * 10));
        lv_label_set_text(temp_val_label, label_buffer);
    }

    // Set humidity
    float hum = get_latest(HUMIDITY);
    if (hum == -1)
    {
        lv_label_set_text(hum_val_label, "n/a");
    }
    else
    {
        snprintf(label_buffer, sizeof(label_buffer), "%d%%", (int)hum);
        lv_label_set_text(hum_val_label, label_buffer);
    }

    // Set CO2 concentration
    float co2 = get_latest(CO2_CONCENTRATION);
    if (co2 == -1)
    {
        lv_label_set_text(co2_val_label, "n/a");
    }
    else
    {
        snprintf(label_buffer, sizeof(label_buffer), "%d", (int)co2);
        lv_label_set_text(co2_val_label, label_buffer);
    }

    // Set VOC index air quality label
    float voc = get_latest(VOC_INDEX);
    snprintf(label_buffer, sizeof(label_buffer), "%s", air_quality_from_voc_index((int)voc));
    lv_label_set_text(voc_val_label, label_buffer);

    // Set battery percentage
    float bat = get_latest(BATTERY_LEVEL);
    if (bat == -1)
    {
        lv_label_set_text(batt_val_label, "n/a");
    }
    else
    {
        snprintf(label_buffer, sizeof(label_buffer), "%d%%", (int)bat);
        lv_label_set_text(batt_val_label, label_buffer);
    }

    refresh_count++;
    if (refresh_count >= 10)
    {
        // Full refresh every 10th update to avoid ghosting
        display_blanking_on(epd_dev);
        display_blanking_off(epd_dev);
        refresh_count = 0;
    }

    lv_task_handler();

    return 0;
}

int activate_epd(void)
{
    LOG_INF("Activating EPD.");
    int rc = 0;
    rc = pm_device_action_run(epd_dev, PM_DEVICE_ACTION_RESUME);
    if (rc != 0)
    {
        LOG_ERR("Failed to activate EPD device (err %d).", rc);
    }
    return rc;
}

int suspend_epd(void)
{
    LOG_INF("Suspending EPD.");
    int rc = 0;
    rc = pm_device_action_run(epd_dev, PM_DEVICE_ACTION_SUSPEND);
    if (rc != 0)
    {
        LOG_ERR("Failed to suspend EPD device (err %d).", rc);
    }
    return rc;
}