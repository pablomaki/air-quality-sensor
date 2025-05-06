#include <variables.h>
#include <utils/variable_buffer.h>

#include <stdlib.h>

static variable_buffer_t buffers[NUM_VARIABLES];

int init_buffers(size_t size)
{
	for (int i = 0; i < NUM_VARIABLES; i++)
	{
		buffers[i].data = (float *)malloc(size * sizeof(float));
		if (!buffers[i].data)
		{
			// Free already allocated buffers on failure
			for (int j = 0; j < i; j++)
			{
				free(buffers[j].data);
			}
			return -1;
		}
		buffers[i].size = size;
		buffers[i].index = 0;
	}
	return 0;
}

void free_buffers(void)
{
	for (int i = 0; i < NUM_VARIABLES; i++)
	{
		free(buffers[i].data);
		buffers[i].data = NULL;
		buffers[i].size = 0;
		buffers[i].index = 0;
	}
}

void set_value(variable_t variable, float value)
{
	variable_buffer_t *buffer = &buffers[variable];
	buffer->data[buffer->index] = value;
	buffer->index = (buffer->index + 1) % buffer->size; // Circular buffer
}

float get_mean(variable_t variable)
{
	variable_buffer_t *buffer = &buffers[variable];
	float sum = 0.0f;
	for (size_t i = 0; i < buffer->size; i++)
	{
		sum += buffer->data[i];
	}
	return sum / buffer->size;
}