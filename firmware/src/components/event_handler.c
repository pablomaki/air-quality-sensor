#include <components/event_handler.h>
#include <drivers/led_controller.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(event_handler);

int init_event_handler(void)
{
    int rc = 0;
#ifdef CONFIG_ENABLE_EVENT_LED
    rc = init_led_controller();
    if (rc != 0)
    {
        LOG_ERR("LED setup failed (err %d).", rc);
        return rc;
    }
#endif
    return 0;
}

void dispatch_event(event_t event)
{
#ifdef CONFIG_ENABLE_EVENT_LED
    switch (event)
    {
    case INITIALIZATION_SUCCESS:
        // LED off
        blink_led(LED_GREEN, 3);
        break;
    case INITIALIZATION_ERROR:
        // LED off
        blink_led(LED_RED, 3);
        break;
    case STARTUP_SUCCESS:
        // LED off
        blink_led(LED_GREEN, 2);
        break;
    case STARTUP_ERROR:
        // LED off
        blink_led(LED_RED, 2);
        break;
    case PERIODIC_TASK_SUCCESS:
        // LED off
        blink_led(LED_GREEN, 1);
        break;
    case BLE_CONNECTION_SUCCESS:
        // LED off
        blink_led(LED_BLUE, 1);
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