#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

/**
 * @brief Enumeration for events
 *
 */
typedef enum
{
    INITIALIZATION_SUCCESS,
    INITIALIZATION_FAILURE,
    PERIODIC_TASK_SUCCESS,
    PERIODIC_TASK_FAILURE
} event_t;

int init_event_handler(void);

void record_event(event_t event);

#endif // EVENT_HANDLER_H