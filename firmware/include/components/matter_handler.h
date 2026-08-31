#ifndef MATTER_HANDLER_H
#define MATTER_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enable BLE
 *
 * @return int Zero for success, non-zero otherwise.
 */
int init_matter(void);

/**
 * @brief Update cluster states
 * @return int Zero for success, non-zero otherwise.
 */
int update_cluster_states(void);

/**
 * @brief Pump Nordic's Matter common library task queue (board/LED/watchdog
 * tasks posted via Nrf::PostTask). Never returns - call from the main thread.
 */
void matter_dispatch_tasks(void);

#ifdef __cplusplus
}
#endif

#endif // MATTER_HANDLER_H