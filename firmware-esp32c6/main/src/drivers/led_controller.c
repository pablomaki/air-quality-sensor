#include <drivers/led_controller.h>
#include <board.h>

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>

static const char *TAG = "led_controller";

#define LED_ACTIVE_LEVEL 0

#define LED_GREEN_HALF_PERIOD_MS 50
#define LED_YELLOW_HALF_PERIOD_MS 150
#define LED_RED_HALF_PERIOD_MS 400

/**
 * @brief State of the blink sequence in progress
 */
typedef struct
{
    int remaining_toggles;
    bool is_on;
    esp_timer_handle_t timer;
} led_blink_ctx_t;

static led_blink_ctx_t blink_ctx;
static portMUX_TYPE blink_lock = portMUX_INITIALIZER_UNLOCKED;

/**
 * @brief Half period of the blink pattern that represents a color
 *
 * @param color Color to look up
 * @return uint32_t, milliseconds the LED stays on and then off
 */
static uint32_t half_period_ms(led_color_t color)
{
    switch (color)
    {
    case LED_GREEN:
        return LED_GREEN_HALF_PERIOD_MS;
    case LED_YELLOW:
        return LED_YELLOW_HALF_PERIOD_MS;
    default:
        return LED_RED_HALF_PERIOD_MS;
    }
}

/**
 * @brief Toggle the LED until the sequence is exhausted
 *
 * @param arg Unused
 */
static void blink_handler(void *arg)
{
    (void)arg;
    bool done;
    bool turn_on;

    taskENTER_CRITICAL(&blink_lock);
    done = blink_ctx.remaining_toggles <= 0;
    if (!done)
    {
        blink_ctx.is_on = !blink_ctx.is_on;
        blink_ctx.remaining_toggles--;
    }
    turn_on = !done && blink_ctx.is_on;
    taskEXIT_CRITICAL(&blink_lock);

    set_led(turn_on ? LED_GREEN : LED_OFF_COLOR);
    if (done)
    {
        esp_timer_stop(blink_ctx.timer);
    }
}

int init_led_controller(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = BIT64(BOARD_USER_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
    };

    esp_err_t err = gpio_config(&config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "GPIO configuration for the LED failed (err %d).", err);
        return err;
    }
    set_led(LED_OFF_COLOR);

    const esp_timer_create_args_t timer_args = {
        .callback = blink_handler,
        .name = "led_blink",
    };
    err = esp_timer_create(&timer_args, &blink_ctx.timer);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create the blink timer (err %d).", err);
        return err;
    }
    return 0;
}

void blink_led(led_color_t color, int count)
{
    if (blink_ctx.timer == NULL)
    {
        return;
    }

    esp_timer_stop(blink_ctx.timer);

    taskENTER_CRITICAL(&blink_lock);
    blink_ctx.remaining_toggles = count * 2;
    blink_ctx.is_on = false;
    taskEXIT_CRITICAL(&blink_lock);

    set_led(LED_OFF_COLOR);
    esp_timer_start_periodic(blink_ctx.timer, half_period_ms(color) * 1000ULL);
}

void set_led(led_color_t color)
{
    gpio_set_level(BOARD_USER_LED_GPIO, color != LED_OFF_COLOR ? LED_ACTIVE_LEVEL : !LED_ACTIVE_LEVEL);
}
