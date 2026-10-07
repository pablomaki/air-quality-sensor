#include <air_quality_monitor.h>

#include <esp_log.h>
#include <nvs_flash.h>

static const char *TAG = "app";

/**
 * @brief Bring up storage, then initialize and start the air quality monitor
 */
void app_main(void)
{
    ESP_LOGI(TAG, "Initializing and starting the air quality monitor.");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "NVS needs to be erased (err %d), the device loses its commissioning.", err);
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    int rc = init_air_quality_monitor();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Air quality monitor initialized with errors (err %d), continuing.", rc);
    }

    ESP_LOGI(TAG, "Initialization complete, starting the monitoring.");
    rc = start_air_quality_monitor();
    if (rc != 0)
    {
        ESP_LOGE(TAG, "Error while starting the air quality monitor (err %d).", rc);
    }
}
