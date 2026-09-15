#ifndef FACTORY_RESET_H
#define FACTORY_RESET_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Count this boot towards the power cycle factory reset gesture
 *
 * The final board carries no button, so a factory reset has to be reachable
 * through the only control a sealed unit offers: its power. A counter in
 * settings survives the power cycle and is cleared once the device has stayed up
 * past the window, so only cycles in quick succession accumulate.
 *
 * Must be called before init_matter(), so that a requested reset is known before
 * the Matter server starts, and after init_event_handler(), so the count can be
 * blinked back to the user.
 *
 * @return int, 0 if ok, non-zero if an error occured
 */
int init_factory_reset(void);

/**
 * @brief Perform the factory reset when the power cycle gesture asked for one
 *
 * Separate from init_factory_reset() because the erase runs through the Matter
 * server, which is only up once init_matter() has returned. Does nothing when no
 * reset was requested.
 */
void run_pending_factory_reset(void);

#ifdef __cplusplus
}
#endif

#endif // FACTORY_RESET_H
