#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

#include <zephyr/kernel.h>

#include <utils/variables.h>

/**
 * @brief Enumeration for events
 *
 */
typedef enum
{
    INITIALIZATION_SUCCESS,
    INITIALIZATION_ERROR,
    PERIODIC_TASK_SUCCESS,
    PERIODIC_TASK_WARNING,
    PERIODIC_TASK_ERROR
} event_t;

/**
 * @brief Initialize event handler
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int init_event_handler(void);

/**
 * @brief Handle event by blinking LED if ENABLE_EVENT_LED is defined
 *
 * @param event Event received
 */
void dispatch_event(event_t event);

/**
 * @brief Wake up from SYSTEM_OFF sleep
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int wake_up(void);

#endif // EVENT_HANDLER_H