#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include <variables.h>

#define LED_OFF 0
#define LED_ON 1

typedef enum
{
    LED_OFF_COLOR = 0,
    LED_RED = BIT(0),
    LED_GREEN = BIT(1),
    LED_BLUE = BIT(2),
    LED_YELLOW = LED_RED | LED_GREEN,
    LED_CYAN = LED_GREEN | LED_BLUE,
    LED_MAGENTA = LED_RED | LED_BLUE,
    LED_WHITE = LED_RED | LED_GREEN | LED_BLUE
} led_color_t;

/**
 * @brief Struct for containing led related information
 *
 */
typedef struct {
    led_color_t color;
    int remaining_toggles;
    bool is_on;
    struct k_timer timer;
} led_blink_ctx_t;

/**
 * @brief Initialize all LED devices
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int init_led_controller(void);

void blink_led(led_color_t color, int count);

void set_led(led_color_t color);

void blink_handler(struct k_timer *timer);

#endif // LED_CONTROLLER_H