#include <air_quality_monitor.h>
#include <components/matter_handler.h>

#include <zephyr/logging/log.h>

// Nordic's Matter common library (board.cpp, matter_init.cpp, etc. pulled in
// via source_common.cmake) declares LOG_MODULE_DECLARE(app, ...) and expects
// exactly one translation unit to register that module - normally their own
// main.cpp/AppTask.cpp, which this project doesn't use.
LOG_MODULE_REGISTER(app, CONFIG_CHIP_APP_LOG_LEVEL);

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

	LOG_INF("Initialization and startup complete, dispatching Matter tasks.");

	// Never returns - pumps board/LED/watchdog tasks posted via Nrf::PostTask.
	matter_dispatch_tasks();

	return 0;
}