#ifndef VARIABLE_BUFFER_H
#define VARIABLE_BUFFER_H

#include <stdint.h>
#include <stddef.h>

typedef struct
{
    float *data;
    size_t size;
    size_t index;
} variable_buffer_t;

/**
 * @brief Initialize a buffer
 *
 * @param buffer Pointer to the buffer
 * @param size Size of the buffer
 * @return 0 on success, -1 on failure
 */
int buffer_init(variable_buffer_t *buffer, size_t size);

/**
 * @brief Add a value to the buffer
 *
 * @param buffer Pointer to the buffer
 * @param value Value to add
 */
void buffer_add(variable_buffer_t *buffer, float value);

/**
 * @brief Get the mean of the buffer
 *
 * @param buffer Pointer to the buffer
 * @return Mean of the buffer values
 */
float buffer_get_mean(const variable_buffer_t *buffer);

/**
 * @brief Free the buffer memory
 *
 * @param buffer Pointer to the buffer
 */
void buffer_free(variable_buffer_t *buffer);

#endif // VARIABLE_BUFFER_H