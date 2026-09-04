#include <air_quality_monitor.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app);

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
	LOG_INF("Initialization complete, starting the monitoring.");
	rc = start_air_quality_monitor();
	if (rc != 0)
	{
		LOG_ERR("Error while starting the air quality monitor (err %d).", rc);
		return rc;
	}

	return 0;
}