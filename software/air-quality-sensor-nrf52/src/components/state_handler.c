#include <components/state_handler.h>
#include <drivers/led_controller.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(state_handler);

static state_t state = STATE_NOT_SET; // Global bluetooth state variable

int init_state_handler(void)
{
#ifdef ENABLE_STATE_LED
    int err;
    err = init_led_controller();
    if (err)
    {
        LOG_ERR("LED setup failed (err %d)", err);
        return err;
    }
#endif
    return 0;
}

void set_state(state_t new_state)
{
    state = new_state;
#ifdef ENABLE_STATE_LED
    display_state();
#endif
}

state_t get_state(void)
{
    return state;
}

#ifdef ENABLE_STATE_LED
/**
 * @brief Display the current state with LED
 *
 */
static void display_state(void)
{
    switch (state)
    {
    case STATE_NOT_SET:
        // LED off
        set_led(LED_OFF_COLOR);
        break;
    case INITIALIZING:
        // LED off
        set_led(LED_CYAN);
        break;
    case ACTIVE:
        // LED off
        set_led(LED_GREEN);
        break;
    case IDLE:
        // Blinking blue light
        set_led(LED_YELLOW);
        break;
    case ERROR:
        // Blinking blue light
        set_led(LED_RED);
        break;
    default:
        break;
    }
    return;
}
#endif
