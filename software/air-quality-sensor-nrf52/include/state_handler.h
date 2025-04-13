#ifndef STATE_HANDLER_H
#define STATE_HANDLER_H

int init_state_handler(void);

void set_state(state_t new_state);

state_t get_state(void);

#endif