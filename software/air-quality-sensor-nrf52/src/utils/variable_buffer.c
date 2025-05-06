#include "utils/variable_buffer.h"
#include <stdlib.h>

int buffer_init(variable_buffer_t *buffer, size_t size)
{
    buffer->data = (float *)malloc(size * sizeof(float));
    if (!buffer->data)
    {
        return -1; // Memory allocation failed
    }
    buffer->size = size;
    buffer->index = 0;
    return 0;
}

void buffer_add(variable_buffer_t *buffer, float value)
{
    buffer->data[buffer->index] = value;
    buffer->index = (buffer->index + 1) % buffer->size; // Circular buffer
}

float buffer_get_mean(const variable_buffer_t *buffer)
{
    float sum = 0.0f;
    for (size_t i = 0; i < buffer->size; i++)
    {
        sum += buffer->data[i];
    }
    return sum / buffer->size;
}

void buffer_free(variable_buffer_t *buffer)
{
    free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0;
    buffer->index = 0;
}