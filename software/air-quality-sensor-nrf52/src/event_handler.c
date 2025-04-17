#include <event_handler.h>
#include <configs.h>
#include <drivers/led_controller.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(event_handler);

int init_event_handler(void)
{
    int err;
#ifdef ENABLE_EVENT_LED
    err = init_led_controller();
    if (err)
    {
        LOG_ERR("LED setup failed (err %d)", err);
        return err;
    }
#endif
    return 0;
}

void dispatch_event(event_t event)
{
#ifdef ENABLE_EVENT_LED
    switch (event)
    {
    case INITIALIZATION_SUCCESS:
        // LED off
        blink_led(LED_GREEN, 2);
        break;
    case INITIALIZATION_ERROR:
        // LED off
        blink_led(LED_RED, 2);
        break;
    case PERIODIC_TASK_SUCCESS:
        // LED off
        blink_led(LED_GREEN, 1);
        break;
    case PERIODIC_TASK_WARNING:
        // Blinking blue light
        blink_led(LED_YELLOW, 1);
        break;
    case PERIODIC_TASK_ERROR:
        // Blinking blue light
        blink_led(LED_RED, 1);
        break;
    default:
        break;
    }
#endif
    return;
}