#ifndef MATTER_HANDLER_H
#define MATTER_HANDLER_H

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

#endif // MATTER_HANDLER_H