#ifndef MATTER_HANDLER_H
#define MATTER_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Build the data model, apply the device identity and start Matter over Thread
 *
 * Creates an endpoint for each quantity the fitted sensors measure, plus the
 * Air Quality Sensor endpoint. Also arms the OTA image confirmation: an image
 * booted for the first time after an update is marked valid once Matter is
 * reachable again, and rolled back if that does not happen in time.
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

#ifdef __cplusplus
}
#endif

#endif // MATTER_HANDLER_H