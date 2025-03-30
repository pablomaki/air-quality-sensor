#include <battery.h>
#include <variables.h>

int read_battery_level(void)
{
	set_battery_level(100.0);
	return 0;
}