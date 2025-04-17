#ifndef STATE_HANDLER_H
#define STATE_HANDLER_H

/**
 * @brief Enumeration for states
 *
 */
typedef enum
{
    STATE_NOT_SET,
    INITIALIZING,
    ACTIVE,
    IDLE,
    ERROR
} state_t;

/**
 * @brief Set the state object
 *
 * @param new_state New state
 */
void set_state(state_t new_state);

/**
 * @brief Get the state object
 *
 * @return state_t, current state
 */
state_t get_state(void);

void display_state(void);

#endif