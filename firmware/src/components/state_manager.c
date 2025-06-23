#include <components/state_manager.h>
#include <components/flash_manager.h>
#include <components/sensors.h>
#include <components/e_paper_display.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(state_manager);

typedef int (*action_fn_t)(void);

typedef struct
{
    const action_fn_t *actions;
    size_t count;
} action_array_t;

typedef struct
{
    const action_array_t on_enter;
    const action_array_t on_exit;
} state_actions_t;

// For each state, definitions of actions to take on entering and exiting the state

// By default suspend everything at the end
action_fn_t initialize_enter[] = {};
action_fn_t initialize_exit[] = {suspend_flash, suspend_sensors, suspend_epd};
state_actions_t initialize_actions = {
    .on_enter = {.actions = initialize_enter, .count = sizeof(initialize_enter) / sizeof(initialize_enter[0])},
    .on_exit = {.actions = initialize_exit, .count = sizeof(initialize_exit) / sizeof(initialize_enter[0])},
};

// Activate sensors and suspend when done
action_fn_t measuring_enter[] = {activate_sensors};
action_fn_t measuring_exit[] = {suspend_sensors};
state_actions_t measuring_actions = {
    .on_enter = {.actions = measuring_enter, .count = sizeof(measuring_enter) / sizeof(measuring_enter[0])},
    .on_exit = {.actions = measuring_exit, .count = sizeof(measuring_exit) / sizeof(measuring_exit[0])},
};

// Activate e paper display and suspend when done
action_fn_t updating_enter[] = {activate_epd};
action_fn_t updating_exit[] = {suspend_epd};
state_actions_t updating_actions = {
    .on_enter = {.actions = updating_enter, .count = sizeof(updating_enter) / sizeof(updating_enter[0])},
    .on_exit = {.actions = updating_exit, .count = sizeof(updating_exit) / sizeof(updating_exit[0])},
};

// Nothing to activate or suspend
action_fn_t advertising_enter[] = {};
action_fn_t advertising_exit[] = {};
state_actions_t advertising_actions = {
    .on_enter = {.actions = advertising_enter, .count = sizeof(advertising_enter) / sizeof(advertising_enter[0])},
    .on_exit = {.actions = advertising_exit, .count = sizeof(advertising_exit) / sizeof(advertising_exit[0])},
};

// Suspend peripherals and activate on exit
action_fn_t idle_enter[] = {};
action_fn_t idle_exit[] = {};
state_actions_t idle_actions = {
    .on_enter = {.actions = idle_enter, .count = sizeof(idle_enter) / sizeof(idle_enter[0])},
    .on_exit = {.actions = idle_exit, .count = sizeof(idle_exit) / sizeof(idle_exit[0])},
};

// Suspend everything on error, no recovery
action_fn_t error_enter[] = {suspend_flash, suspend_sensors, suspend_epd};
action_fn_t error_exit[] = {};
state_actions_t error_actions = {
    .on_enter = {.actions = error_enter, .count = sizeof(error_enter) / sizeof(error_enter[0])},
    .on_exit = {.actions = error_exit, .count = sizeof(error_exit) / sizeof(error_exit[0])},
};

static state_t current_state = STATE_NOT_SET;

static int execute(action_array_t action_arr)
{
    int err, ret = 0;
    for (int i = 0; i < action_arr.count; i++)
    {
        err = action_arr.actions[i]();
        if (err)
        {
            ret = err;
        }
    }
    return ret;
}
/**
 * @brief Setup the state
 *
 * @param state State to display
 */
static int enter_state(state_t state)
{
    int err = 0;
    switch (state)
    {
    case STATE_NOT_SET:
        break;
    case INITIALIZING:
        err = execute(initialize_actions.on_enter);
        break;
    case MEASURING:
        err = execute(measuring_actions.on_enter);
        break;
    case UPDATING:
        err = execute(updating_actions.on_enter);
        break;
    case ADVERTISING:
        err = execute(advertising_actions.on_enter);
        break;
    case IDLE:
        err = execute(idle_actions.on_enter);
        break;
    case ERROR:
        err = execute(error_actions.on_enter);
        break;
    default:
        err = -EINVAL;
        break;
    }
    return err;
}

/**
 * @brief Cleanup the state
 *
 * @param state State to display
 */
static int exit_state(state_t state)
{
    int err = 0;
    switch (state)
    {
    case STATE_NOT_SET:
        break;
    case INITIALIZING:
        err = execute(initialize_actions.on_exit);
        break;
    case MEASURING:
        err = execute(measuring_actions.on_exit);
        break;
    case UPDATING:
        err = execute(updating_actions.on_exit);
        break;
    case ADVERTISING:
        err = execute(advertising_actions.on_exit);
        break;
    case IDLE:
        err = execute(idle_actions.on_exit);
        break;
    case ERROR:
        err = execute(error_actions.on_exit);
        break;
    default:
        err = -EINVAL;
        break;
    }
    return err;
}

const char *state_to_string(state_t state)
{
    switch (state)
    {
    case STATE_NOT_SET:
        return "STATE_NOT_SET";
    case INITIALIZING:
        return "INITIALIZING";
    case MEASURING:
        return "MEASURING";
    case UPDATING:
        return "UPDATING";
    case ADVERTISING:
        return "ADVERTISING";
    case IDLE:
        return "IDLE";
    case ERROR:
        return "ERROR";
    default:
        return "UNKNOWN_STATE";
    }
}

void set_state(state_t new_state)
{
    LOG_INF("Changing state from %s to %s", state_to_string(current_state), state_to_string(new_state));
    int err;
    err = exit_state(current_state);
    if (err)
    {
        LOG_ERR("Failed to exit state %s (err %d)", state_to_string(current_state), err);
    }
    err = enter_state(new_state);
    if (err)
    {
        LOG_ERR("Failed to enter state %s (err %d)", state_to_string(new_state), err);
    }
    current_state = new_state;
}

state_t get_system_state(void)
{
    return current_state;
}