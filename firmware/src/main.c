#include <air_quality_monitor.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

int main(void)
{
	LOG_INF("Initializing and starting the air quality monitor.");
	int rc = 0;

	// Initialize the air quality monitor
	rc = init_air_quality_monitor();
	if (rc != 0)
	{
		LOG_ERR("Error while initializing the air quality monitor (err %d).", rc);
		return rc;
	}

	// Start the air quality monitor
	rc = start_air_quality_monitor();
	if (rc != 0)
	{
		LOG_ERR("Error while starting the air quality monitor (err %d).", rc);
		return rc;
	}

	LOG_INF("Initialization and  startup complete, exiting main.");
	return 0;
}