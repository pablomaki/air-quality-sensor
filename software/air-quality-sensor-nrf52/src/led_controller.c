#include <led_controller.h>
#include <configs.h>

#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

#define LED_RED_NODE DT_ALIAS(led0)
#define LED_GREEN_NODE DT_ALIAS(led1)
#define LED_BLUE_NODE DT_ALIAS(led2)

LOG_MODULE_REGISTER(led_controller);

// GPIO specs from devicetree
static const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(LED_RED_NODE, gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(LED_GREEN_NODE, gpios);
static const struct gpio_dt_spec led_blue = GPIO_DT_SPEC_GET(LED_BLUE_NODE, gpios);

// Color definitions
static const int red_rgb_arr[] = {1, 0, 0};     /// Color red
static const int green_rgb_arr[] = {0, 1, 0};   /// Color green
static const int blue_rgb_arr[] = {0, 0, 1};    /// Color blue
static const int yellow_rgb_arr[] = {1, 1, 0};  /// Color yellow
static const int magenta_rgb_arr[] = {1, 0, 1}; /// Color magenta
static const int cyan_rgb_arr[] = {0, 1, 1};    /// Color cyan
static const int white_rgb_arr[] = {1, 1, 1};   /// Color white
static const int off_rgb_arr[] = {0, 0, 0};     /// Color off

// Blink timer
static struct k_timer blink_timer;

int init_led_controller(void)
{
    int err;
#ifdef ENABLE_DEBUG_LED
    err = setup_rgb_led();
    if (err)
    {
        LOG_ERR("LED setup failed (err %d)", err);
        return err;
    }
#endif
    return 0;
}

int setup_rgb_led(void)
{
    if (!gpio_is_ready_dt(&led_red) && !gpio_is_ready_dt(&led_green) && !gpio_is_ready_dt(&led_blue))
    {
        LOG_ERR("GPIO devices not ready");
        return -ENXIO;
    }
    int err, err2, err3;
    err = gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);
    err2 = gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);
    err3 = gpio_pin_configure_dt(&led_blue, GPIO_OUTPUT_INACTIVE);
    if (err || err2 || err3)
    {
        LOG_ERR("GPIO configuration failed (err %d, %d, %d)", err, err2, err3);
        return err ? err : (err2 ? err2 : err3);
    }

    k_timer_init(&blink_timer, blink_timer_handler, NULL);
    register_state_callback(state_change_callback);
    return 0;
}

void set_led_rgb(const int rgb_arr[3])
{
    int err, err2, err3;
    err = gpio_pin_set_dt(&led_red, rgb_arr[0]);
    err2 = gpio_pin_set_dt(&led_green, rgb_arr[1]);
    err3 = gpio_pin_set_dt(&led_blue, rgb_arr[2]);
    if (err || err2 || err3)
    {
        LOG_ERR("Failed to set GPIO levels (err %d, %d, %d)", err, err2, err3);
    }
}

void blink_timer_handler(struct k_timer *timer)
{
    int *rgb_val = (int *)k_timer_user_data_get(timer);
    int err, err2, err3;

    // Toggle or turn off the individual leds based on the given rgb value
    err = rgb_val[0] ? gpio_pin_toggle_dt(&led_red) : gpio_pin_set_dt(&led_red, 0);
    err2 = rgb_val[1] ? gpio_pin_toggle_dt(&led_green) : gpio_pin_set_dt(&led_green, 0);
    err3 = rgb_val[2] ? gpio_pin_toggle_dt(&led_blue) : gpio_pin_set_dt(&led_blue, 0);

    if (err || err2 || err3)
    {
        LOG_ERR("Failed to set GPIO levels (err %d, %d, %d)", err, err2, err3);
    }
}

void state_change_callback(state_t new_state)
{
    switch (new_state)
    {
    case INITIALIZING:
        // LED off
        k_timer_stop(&blink_timer);
        set_led_rgb(red_rgb_arr);
        break;
    case STANDBY:
        // LED off
        k_timer_stop(&blink_timer);
        set_led_rgb(yellow_rgb_arr);
        break;
    case MEASURING:
        // Blinking blue light
        k_timer_user_data_set(&blink_timer, (void *)green_rgb_arr);
        k_timer_start(&blink_timer, K_NO_WAIT, K_MSEC(LED_BLINK_INTERVAL / 2));
        break;
    case ADVERTISING:
        // Blinking blue light when advertising
        k_timer_user_data_set(&blink_timer, (void *)blue_rgb_arr);
        k_timer_start(&blink_timer, K_NO_WAIT, K_MSEC(LED_BLINK_INTERVAL / 2));
        break;
    default:
        break;
    }
}