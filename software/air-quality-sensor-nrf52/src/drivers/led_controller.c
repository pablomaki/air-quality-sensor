#include <drivers/led_controller.h>
#include <configs.h>

#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

#define LED_RED_NODE DT_ALIAS(led0)
#define LED_GREEN_NODE DT_ALIAS(led1)
#define LED_BLUE_NODE DT_ALIAS(led2)

LOG_MODULE_REGISTER(led_controller);

/**
 * @brief Struct for containing led related information
 *
 */
typedef struct
{
    led_color_t color;
    int remaining_toggles;
    bool is_on;
    struct k_timer timer;
} led_blink_ctx_t;

static led_blink_ctx_t blink_ctx;

// GPIO specs from devicetree
static const struct gpio_dt_spec led_red_spec = GPIO_DT_SPEC_GET(LED_RED_NODE, gpios);
static const struct gpio_dt_spec led_green_spec = GPIO_DT_SPEC_GET(LED_GREEN_NODE, gpios);
static const struct gpio_dt_spec led_blue_spec = GPIO_DT_SPEC_GET(LED_BLUE_NODE, gpios);

/**
 * @brief Handles LED blinking
 *
 * @param timer
 */
static void blink_handler(struct k_timer *timer)
{
    if (blink_ctx.remaining_toggles <= 0)
    {
        set_led(LED_OFF_COLOR);
        k_timer_stop(&blink_ctx.timer);
        return;
    }

    set_led((blink_ctx.is_on) ? LED_OFF_COLOR : blink_ctx.color);
    blink_ctx.remaining_toggles--;

    blink_ctx.is_on = !blink_ctx.is_on;
}

int init_led_controller(void)
{
    if (!gpio_is_ready_dt(&led_red_spec) || !gpio_is_ready_dt(&led_green_spec) || !gpio_is_ready_dt(&led_blue_spec))
    {
        LOG_ERR("LED devices not ready");
        return -ENXIO;
    }

    int err, err2, err3;
    err = gpio_pin_configure_dt(&led_red_spec, GPIO_OUTPUT_INACTIVE);
    err2 = gpio_pin_configure_dt(&led_green_spec, GPIO_OUTPUT_INACTIVE);
    err3 = gpio_pin_configure_dt(&led_blue_spec, GPIO_OUTPUT_INACTIVE);
    if (err || err2 || err3)
    {
        LOG_ERR("GPIO configuration failed (err %d, %d, %d)", err, err2, err3);
        return err ? err : (err2 ? err2 : err3);
    }
    k_timer_init(&blink_ctx.timer, blink_handler, NULL);
    return 0;
}

void blink_led(led_color_t color, int count)
{
    // Stop current blinking before starting new one
    k_timer_stop(&blink_ctx.timer);

    blink_ctx.color = color;
    blink_ctx.remaining_toggles = count * 2; // on + off
    blink_ctx.is_on = false;

    k_timer_start(&blink_ctx.timer, K_NO_WAIT, K_MSEC(LED_BLINK_INTERVAL));
}

void set_led(led_color_t color)
{
    gpio_pin_set_dt(&led_red_spec, (color & LED_RED) ? LED_ON : LED_OFF);
    gpio_pin_set_dt(&led_green_spec, (color & LED_GREEN) ? LED_ON : LED_OFF);
    gpio_pin_set_dt(&led_blue_spec, (color & LED_BLUE) ? LED_ON : LED_OFF);
}