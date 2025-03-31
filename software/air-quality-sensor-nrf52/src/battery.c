#include <battery.h>
#include <variables.h>

float battery_lvl = 100;

int read_battery_level(void)
{
	set_battery_level(battery_lvl);
	battery_lvl = battery_lvl - 1;
	if (battery_lvl <= 0)
	{
		battery_lvl = 100;
	}
	return 0;
}