#include <components/factory_reset.h>
#include <drivers/led_controller.h>
#include <board.h>

#include <esp_log.h>
#include <esp_matter.h>
#include <esp_timer.h>
#include <nvs.h>

static const char *TAG = "factory_reset";

// The application namespace, holding the boot count and the BSEC state. The
// Matter reset clears only the stack's own namespaces, so this one is erased
// alongside it, the way the nRF build's reset wipes its whole settings area.
#define APP_NVS_NAMESPACE "aqs"
#define BOOT_COUNT_KEY "boot_count"

#define BUTTON_POLL_INTERVAL_MS 50
#define BUTTON_PRESSED_LEVEL 0

static esp_timer_handle_t clear_count_timer;
static esp_timer_handle_t button_timer;
static bool reset_requested;
static bool matter_running;
static uint32_t button_held_ms;

/**
 * @brief Blink the LED, when there is one to blink
 *
 * Mirrors the guard in event_handler.c: the LED controller is only initialized
 * under CONFIG_ENABLE_EVENT_LED, so nothing may drive it when that is off.
 *
 * @param color Color to blink
 * @param count Count of times to blink
 */
static void signal(led_color_t color, int count)
{
#ifdef CONFIG_ENABLE_EVENT_LED
    blink_led(color, count);
#else
    (void)color;
    (void)count;
#endif
}

/**
 * @brief Erase the application namespace and hand over to the Matter factory reset
 *
 * The Matter reset withdraws the SRP records from the border router, erases
 * the stack's storage and restarts the device.
 */
static void factory_reset(void)
{
    ESP_LOGW(TAG, "Factory resetting, the device will reboot uncommissioned.");
    signal(LED_RED, CONFIG_FACTORY_RESET_POWER_CYCLES);

    nvs_handle_t handle;
    if (nvs_open(APP_NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK)
    {
        nvs_erase_all(handle);
        nvs_commit(handle);
        nvs_close(handle);
    }

    esp_err_t err = esp_matter::factory_reset();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Matter factory reset failed (err %d).", err);
    }
}

/**
 * @brief Drop the boot count once the device has stayed up past the window
 *
 * @param arg Unused
 */
static void clear_boot_count(void *arg)
{
    (void)arg;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(APP_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK)
    {
        err = nvs_erase_key(handle, BOOT_COUNT_KEY);
        if (err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND)
        {
            err = nvs_commit(handle);
        }
        nvs_close(handle);
    }
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Failed to clear the boot count (err %d).", err);
    }
}

/**
 * @brief Factory reset once the BOOT button has been held long enough
 *
 * @param arg Unused
 */
static void poll_button(void *arg)
{
    (void)arg;

    if (gpio_get_level(BOARD_BOOT_BUTTON_GPIO) != BUTTON_PRESSED_LEVEL)
    {
        button_held_ms = 0;
        return;
    }

    button_held_ms += BUTTON_POLL_INTERVAL_MS;
    if (button_held_ms == BUTTON_POLL_INTERVAL_MS)
    {
        signal(LED_YELLOW, 1);
    }
    if (button_held_ms >= CONFIG_FACTORY_RESET_BUTTON_HOLD_MS && matter_running)
    {
        esp_timer_stop(button_timer);
        factory_reset();
    }
}

/**
 * @brief Start polling the BOOT button
 *
 * @return esp_err_t, ESP_OK if ok
 */
static esp_err_t init_button(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = BIT64(BOARD_BOOT_BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK)
    {
        return err;
    }

    const esp_timer_create_args_t timer_args = {
        .callback = poll_button,
        .name = "reset_button",
    };
    err = esp_timer_create(&timer_args, &button_timer);
    if (err != ESP_OK)
    {
        return err;
    }
    return esp_timer_start_periodic(button_timer, BUTTON_POLL_INTERVAL_MS * 1000ULL);
}

int init_factory_reset(void)
{
    esp_err_t err = init_button();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set up the BOOT button (err %d).", err);
    }

    nvs_handle_t handle;
    err = nvs_open(APP_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to open the application storage (err %d).", err);
        return err;
    }

    uint8_t count = 0;
    err = nvs_get_u8(handle, BOOT_COUNT_KEY, &count);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGW(TAG, "Failed to read the boot count (err %d), starting from zero.", err);
    }

    count++;
    reset_requested = count >= CONFIG_FACTORY_RESET_POWER_CYCLES;

    if (reset_requested)
    {
        // Deliberately not stored: the erase clears it anyway, and leaving the
        // previous count in place keeps the gesture armed if the reset fails.
        nvs_close(handle);
        ESP_LOGW(TAG, "Power cycled %u times in succession, a factory reset will run.", count);
        return 0;
    }

    err = nvs_set_u8(handle, BOOT_COUNT_KEY, count);
    if (err == ESP_OK)
    {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to store the boot count (err %d).", err);
        return err;
    }

    signal(LED_YELLOW, count);

    const esp_timer_create_args_t timer_args = {
        .callback = clear_boot_count,
        .name = "clear_boot_count",
    };
    err = esp_timer_create(&timer_args, &clear_count_timer);
    if (err == ESP_OK)
    {
        err = esp_timer_start_once(clear_count_timer, CONFIG_FACTORY_RESET_POWER_CYCLE_WINDOW_MS * 1000ULL);
    }
    return err;
}

void run_pending_factory_reset(void)
{
    matter_running = true;

    if (reset_requested)
    {
        factory_reset();
    }
}
