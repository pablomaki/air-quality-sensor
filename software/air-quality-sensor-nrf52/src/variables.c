#include <variables.h>

static float battery_level = 42.0;

void set_battery_level(float new_battery_level)
{
	battery_level = new_battery_level;
}

float get_battery_level(void)
{
	return battery_level;
}