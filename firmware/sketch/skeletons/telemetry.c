/* SKETCH — src/aq/telemetry.c
 * The one thread that owns the two clocks: consume samples continuously,
 * publish to Matter every X seconds (X from settings, not from Kconfig).
 */
#include <aqs/state.h>
#include <aqs/config.h>
#include <aqs/events.h>
#include <zephyr/zbus/zbus.h>

ZBUS_SUBSCRIBER_DEFINE(telemetry_sub, 8);

static void report_tick(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(report_work, report_tick);

static void report_tick(struct k_work *work)
{
	struct aqs_state snap;

	aqs_state_expire();                 /* VALID -> STALE -> ERROR by age */
	aqs_state_snapshot(&snap);
	aqs_matter_publish(&snap);          /* hands off to the CHIP thread */
	zbus_chan_pub(&chan_aq_state, &snap, K_NO_WAIT);  /* shell, LEDs, future consumers */

	k_work_reschedule(&report_work, K_SECONDS(aqs_config_get()->report_interval_s));
}

static void telemetry_thread(void *a, void *b, void *c)
{
	const struct zbus_channel *chan;

	k_work_reschedule(&report_work, K_SECONDS(aqs_config_get()->report_interval_s));

	while (!zbus_sub_wait(&telemetry_sub, &chan, K_FOREVER)) {
		if (chan == &chan_sensor_sample) {
			struct aqs_sample_msg msg;
			zbus_chan_read(chan, &msg, K_MSEC(50));

			for (uint8_t i = 0; i < msg.count; i++) {
				struct aqs_value v = msg.values[i];
				aqs_filter_apply(msg.quantities[i], &v);   /* EMA + limits */
				aqs_state_update(msg.quantities[i], &v);
				if (IS_ENABLED(CONFIG_AQS_REPORT_ON_CHANGE) &&
				    aqs_delta_exceeded(msg.quantities[i], &v)) {
					k_work_reschedule(&report_work, K_NO_WAIT);
				}
			}
			aqs_index_recompute();     /* composite AirQuality enum */

		} else if (chan == &chan_config) {
			/* Interval changed at runtime -> re-arm without waiting out the
			 * old period. Same path serves a future Matter-side config write. */
			k_work_reschedule(&report_work,
				K_SECONDS(aqs_config_get()->report_interval_s));
		}
	}
}

K_THREAD_DEFINE(aqs_telemetry, CONFIG_AQS_TELEMETRY_STACK_SIZE, telemetry_thread,
		NULL, NULL, NULL, CONFIG_AQS_TELEMETRY_PRIO, 0, 0);
