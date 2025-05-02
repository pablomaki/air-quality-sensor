#include <drivers/battery.h>
#include <variables.h>

// Placeholder
static float battery_lvl = 100;

int read_battery_level(uint8_t index)
{
	set_battery_level(battery_lvl, index);
	battery_lvl = battery_lvl - 1;
	if (battery_lvl <= 0)
	{
		battery_lvl = 100;
	}
	return 0;
}