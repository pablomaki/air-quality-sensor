#include <utils/variable_buffer.h>

# include <zephyr/kernel.h>

#include <stdlib.h>

static variable_buffer_t buffers[NUM_VARIABLES];

/**
 * @brief Release the storage of the first `count` buffers
 */
static void free_first(int count)
{
	for (int i = 0; i < count; i++)
	{
		free(buffers[i].data);
		free(buffers[i].valid);
		buffers[i].data = NULL;
		buffers[i].valid = NULL;
	}
}

int init_buffers(size_t size)
{
	for (int i = 0; i < NUM_VARIABLES; i++)
	{
		buffers[i].data = (float *)calloc(size, sizeof(float));
		buffers[i].valid = (bool *)calloc(size, sizeof(bool));
		if (!buffers[i].data || !buffers[i].valid)
		{
			// Free already allocated buffers on failure
			free(buffers[i].data);
			free(buffers[i].valid);
			buffers[i].data = NULL;
			buffers[i].valid = NULL;
			free_first(i);
			return -ENOMEM;
		}
		buffers[i].size = size;
		buffers[i].index = 0;
	}
	return 0;
}

void free_buffers(void)
{
	free_first(NUM_VARIABLES);
	for (int i = 0; i < NUM_VARIABLES; i++)
	{
		buffers[i].size = 0;
		buffers[i].index = 0;
	}
}

/**
 * @brief Advance a buffer by one slot, recording the value and its validity
 */
static void push(variable_t variable, float value, bool valid)
{
	variable_buffer_t *buffer = &buffers[variable];

	if (!buffer->data || !buffer->valid)
	{
		return;
	}

	buffer->data[buffer->index] = value;
	buffer->valid[buffer->index] = valid;
	buffer->index = (buffer->index + 1) % buffer->size; // Circular buffer
}

void set_value(variable_t variable, float value)
{
	push(variable, value, true);
}

void set_invalid(variable_t variable)
{
	push(variable, 0.0f, false);
}

bool get_mean(variable_t variable, float *mean)
{
	variable_buffer_t *buffer = &buffers[variable];
	float sum = 0.0f;
	size_t valid_count = 0;

	if (!buffer->data || !buffer->valid || mean == NULL)
	{
		return false;
	}

	for (size_t i = 0; i < buffer->size; i++)
	{
		if (buffer->valid[i])
		{
			sum += buffer->data[i];
			valid_count++;
		}
	}

	if (valid_count == 0)
	{
		return false;
	}

	*mean = sum / valid_count;
	return true;
}

bool get_latest(variable_t variable, float *latest)
{
	variable_buffer_t *buffer = &buffers[variable];

	if (!buffer->data || !buffer->valid || latest == NULL)
	{
		return false;
	}

	// Index points at the next slot to be written, so the newest sample is behind it
	size_t newest = (buffer->index - 1 + buffer->size) % buffer->size;
	if (!buffer->valid[newest])
	{
		return false;
	}

	*latest = buffer->data[newest];
	return true;
}
