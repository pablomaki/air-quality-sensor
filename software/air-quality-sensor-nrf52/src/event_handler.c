#include <event_handler.h>
#include <led_controller.h>

int init_event_handler(void)
{
    return 0;
}

void record_event(event_t event)
{
    switch (event)
    {
    case INITIALIZATION_SUCCESS:
        // LED off
        blink_led(GREEN, 2)
        break;
    case INITIALIZATION_FAILURE:
        // LED off
        blink_led(RED, 2)
        break;
    case PERIODIC_TASK_SUCCESS:
        // LED off
        blink_led(GREEN, 1)
        break;
    case PERIODIC_TASK_FAILURE:
        // Blinking blue light
        blink_led(YELLOW, 1)
        break;
    default:
        break;
    }
    return;
}