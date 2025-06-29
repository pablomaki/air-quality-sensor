#ifndef VARIABLE_BUFFER_H
#define VARIABLE_BUFFER_H

#include <stdint.h>
#include <stddef.h>

// Define the variables that will be stored in buffers
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

// Define the buffer structure for each variable
typedef struct
{
    float *data;
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

/**
 * @brief Get the latest value in the buffer
 *
 * @param variable The variable to get (e.g., TEMPERATURE)
 * @return The latest value
 */
float get_latest(variable_t variable);

#endif // VARIABLE_BUFFERS_H