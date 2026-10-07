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
 * @brief Update the Matter cluster attributes from the buffered sensor values
 *
 * Publishes the mean of the valid samples of each measured quantity. A quantity
 * with no valid samples is reported as null, except AirQuality, which has no
 * null state and is reported as kUnknown.
 *
 * Safe to call from any thread: the CHIP stack lock is held for the duration of
 * the data model access.
 *
 * @note When both the SGP40 and the BME680 are enabled they drive the same
 *       AirQuality attribute and the BME680 wins, as it is written last.
 *
 * @return int Zero for success, a bitmask of the failed updates otherwise.
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