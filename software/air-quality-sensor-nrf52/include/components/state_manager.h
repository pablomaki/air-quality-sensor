#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

/**
 * @brief Enumeration for states
 *
 */
typedef enum
{
    STATE_NOT_SET,
    INITIALIZING,
    MEASURING,
    UPDATING,
    ADVERTISING,
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

#endif // STATE_MANAGER_H