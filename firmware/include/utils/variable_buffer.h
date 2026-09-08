#ifndef VARIABLE_BUFFER_H
#define VARIABLE_BUFFER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Define the variables that will be stored in buffers
typedef enum
{
    TEMPERATURE,
    HUMIDITY,
    PRESSURE,
    CO2_CONCENTRATION,
    VOC_INDEX,
    IAQ_INDEX,
    NUM_VARIABLES // Total number of variables
} variable_t;

// Define the buffer structure for each variable
typedef struct
{
    float *data;
    bool *valid; // Per-slot validity, so an unread or failed sample is not averaged
    size_t size;
    size_t index;
} variable_buffer_t;

/**
 * @brief Initialize all buffers
 *
 * @param size Size of each buffer
 * @return 0 on success, -1 on failure
 */
int init_buffers(size_t size);

/**
 * @brief Free all buffers
 */
void free_buffers(void);

/**
 * @brief Record a successfully measured value in the buffer
 *
 * @param variable The variable to set (e.g., TEMPERATURE)
 * @param value The value to set
 */
void set_value(variable_t variable, float value);

/**
 * @brief Record a failed measurement in the buffer
 *
 * Occupies a slot so that the sample window still advances in step with the
 * measurement cadence, but marks it invalid so it is excluded from the mean.
 * Validity is tracked out of band because every quantity measured here has
 * plausible values that cannot be distinguished from an in-band error marker -
 * -1 degC is a real temperature.
 *
 * @param variable The variable to mark as failed (e.g., TEMPERATURE)
 */
void set_invalid(variable_t variable);

/**
 * @brief Get the mean of the valid values in a buffer
 *
 * @param variable The variable to get (e.g., TEMPERATURE)
 * @param mean Set to the mean of the valid samples if any exist, untouched otherwise
 * @return true if at least one valid sample was available, false otherwise
 */
bool get_mean(variable_t variable, float *mean);

/**
 * @brief Get the latest value in the buffer
 *
 * @param variable The variable to get (e.g., TEMPERATURE)
 * @param latest Set to the most recent sample if it is valid, untouched otherwise
 * @return true if the most recent sample was valid, false otherwise
 */
bool get_latest(variable_t variable, float *latest);

#ifdef __cplusplus
}
#endif

#endif // VARIABLE_BUFFERS_H