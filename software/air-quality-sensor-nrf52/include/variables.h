#ifndef VARIABLES_H
#define VARIABLES_H

#include <stdint.h>
#include <stddef.h>

typedef enum
{
    BATTERY_LEVEL,
    TEMPERATURE,
    HUMIDITY,
    PRESSURE,
    CO2_CONCENTRATION,
    VOC_INDEX,
    NUM_VARIABLES // Total number of variables
} variable_t;

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
 * @brief Set a value in the buffer
 *
 * @param variable The variable to set (e.g., TEMPERATURE)
 * @param value The value to set
 */
void set_value(variable_t variable, float value);

/**
 * @brief Get the mean value of a buffer
 *
 * @param variable The variable to get (e.g., TEMPERATURE)
 * @return The mean value of the buffer
 */
float get_mean(variable_t variable);

#endif // VARIABLES_H