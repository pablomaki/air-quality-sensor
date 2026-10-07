#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Status colors
 *
 * The board has a single LED, so a color is rendered as a blink length rather
 * than a hue: green is a short flash, yellow a medium one and red a long one.
 */
typedef enum
{
    LED_OFF_COLOR = 0,
    LED_RED = 1 << 0,
    LED_GREEN = 1 << 1,
    LED_YELLOW = LED_RED | LED_GREEN,
} led_color_t;

/**
 * @brief Initialize the user LED and its blink timer
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int init_led_controller(void);

/**
 * @brief Blink the LED count times with the pattern of a color
 *
 * Replaces any blink sequence still in progress.
 *
 * @param color Color whose pattern to blink
 * @param count Count of times to blink
 */
void blink_led(led_color_t color, int count);

/**
 * @brief Switch the LED on for any color, off for LED_OFF_COLOR
 *
 * @param color Color to set LED to
 */
void set_led(led_color_t color);

#ifdef __cplusplus
}
#endif

#endif // LED_CONTROLLER_H
