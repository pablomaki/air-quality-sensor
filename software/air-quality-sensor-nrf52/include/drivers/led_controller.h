#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <zephyr/kernel.h>

#define LED_OFF 0
#define LED_ON 1

/**
 * @brief Enum for RGB colors
 * 
 */
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
 * @brief Initialize all LED devices
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int init_led_controller(void);

/**
 * @brief Blink specific LED color X times
 * 
 * @param color Color to blink
 * @param count Count of times to blink
 */
void blink_led(led_color_t color, int count);

/**
 * @brief Set LED color
 * 
 * @param color Color to set LED to
 */
void set_led(led_color_t color);

#endif // LED_CONTROLLER_H