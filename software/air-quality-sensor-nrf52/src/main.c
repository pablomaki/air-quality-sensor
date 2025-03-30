
#include <zephyr/logging/log.h>

#include <air_quality_monitor.h>

LOG_MODULE_REGISTER(main);

int main(void)
{
	LOG_INF("Initializing the air quality monitor.");
	int err;

	// Initialize the air quality monitor
	err = init_air_quality_monitor();
	if (err)
	{
		LOG_ERR("Error while initializing the air quality monitor (err %d)", err);
		return err;
	}

	LOG_INF("Initialization complete, exiting main.");
	return 0;
}