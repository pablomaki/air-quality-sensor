#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <zephyr/kernel.h>

#include <variables.h>

/**
 * @brief Initialize all LED devices
 * 
 * @return int, 0 if ok, non-zero if an error occured
 */
int init_led_controller(void);

/**
 * @brief Setup the rgb led object
 * 
 * @return int, 0 if ok, non-zero if an error occured
 */
int setup_rgb_led(void);

/**
 * @brief Set the RGB led values
 * 
 * @param rgb_arr Array for enabling individual R, G, B leds
 */
void set_led_rgb(const int rgb_arr[3]);

/**
 * @brief Handle blinking functionality (periodically called function)
 * 
 * @param timer timer object pointer
 */
void blink_timer_handler(struct k_timer *timer);

/**
 * @brief Callback for when BLE state changes
 * 
 * @param new_state new state
 */
void state_change_callback(state_t new_state);

#endif // LED_CONTROLLER_H