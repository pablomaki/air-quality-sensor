/* SKETCH — src/sensing/sensor_hub.c
 * Registry iteration + per-sensor state machine on a dedicated workqueue.
 * Deliberately has no knowledge of any specific sensor, and none of Matter.
 */
#include <aqs/sensor.h>
#include <aqs/events.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(aqs_hub, CONFIG_AQS_SENSING_LOG_LEVEL);

K_THREAD_STACK_DEFINE(hub_stack, CONFIG_AQS_SENSOR_WQ_STACK_SIZE);
static struct k_work_q hub_wq;

#define BACKOFF_MIN_MS 1000U
#define BACKOFF_MAX_MS 60000U
#define FAIL_GRACE     3U      /* failures tolerated before values go AQS_ERROR */

static uint32_t period_of(const struct aqs_sensor *s)
{
	/* Configured cadence, floored by what the part can physically do. */
	return MAX(s->min_period_ms, aqs_config_get()->sample_period_ms);
}

static void mark_all(const struct aqs_sensor *s, enum aqs_status st)
{
	for (uint8_t i = 0; i < s->map_count; i++) {
		struct aqs_value v = { .status = st, .timestamp = k_uptime_get() };
		/* published, not written directly: the hub does not own the state */
		aqs_publish_value(s->maps[i].quantity, &v, s);
	}
}

static void sensor_work(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	const struct aqs_sensor *s = sensor_from_dwork(dwork);
	struct aqs_sensor_rt *rt = s->rt;
	int err;

	switch (rt->state) {
	case AQS_S_UNINIT:
		if (!device_is_ready(s->dev)) {
			LOG_ERR("%s not ready", s->name);
			rt->state = AQS_S_ERROR;
			break;
		}
		err = s->ops && s->ops->init ? s->ops->init(s) : 0;
		if (!err && s->ops && s->ops->apply_config) {
			err = s->ops->apply_config(s);   /* altitude, T offset, ASC... */
		}
		if (err) { rt->state = AQS_S_ERROR; break; }
		rt->next_due = k_uptime_get();
		rt->state = AQS_S_IDLE;
		__fallthrough;

	case AQS_S_IDLE:
		if (s->warmup_ms && s->ops && s->ops->start) {
			(void)s->ops->start(s);          /* power/fan on */
			mark_all(s, AQS_WARMING_UP);
			rt->state = AQS_S_WARMUP;
			k_work_reschedule_for_queue(&hub_wq, dwork, K_MSEC(s->warmup_ms));
			return;
		}
		rt->state = AQS_S_FETCH;
		__fallthrough;

	case AQS_S_WARMUP:
		rt->state = AQS_S_FETCH;
		__fallthrough;

	case AQS_S_FETCH: {
		struct aqs_sample_msg msg = { .source = s };

		err = s->ops && s->ops->decode ? s->ops->decode(s, &msg)
					       : aqs_generic_read(s, &msg);
		if (s->warmup_ms && s->ops && s->ops->stop) {
			(void)s->ops->stop(s);           /* duty cycle: power/fan off */
		}

		if (err) {
			if (++rt->fail_count == FAIL_GRACE) {
				mark_all(s, AQS_ERROR);
			}
			rt->backoff_ms = rt->backoff_ms
				? MIN(rt->backoff_ms * 2U, BACKOFF_MAX_MS)
				: BACKOFF_MIN_MS;
			LOG_WRN("%s read failed (%d), retry in %u ms",
				s->name, err, rt->backoff_ms);
			rt->state = AQS_S_ERROR;
			k_work_reschedule_for_queue(&hub_wq, dwork, K_MSEC(rt->backoff_ms));
			return;
		}

		rt->fail_count = 0;
		rt->backoff_ms = 0;
		rt->sample_count++;
		zbus_chan_pub(&chan_sensor_sample, &msg, K_MSEC(50));

		/* Absolute rescheduling: cadence does not drift by the read duration. */
		rt->next_due += period_of(s);
		int64_t delay = rt->next_due - k_uptime_get();
		if (delay < 0) { rt->next_due = k_uptime_get() + period_of(s); delay = period_of(s); }
		rt->state = AQS_S_IDLE;
		k_work_reschedule_for_queue(&hub_wq, dwork, K_MSEC(delay));
		return;
	}

	case AQS_S_ERROR:
		/* Recovery ladder: retry -> i2c_recover_bus() -> power-cycle the rail. */
		rt->state = (rt->fail_count > 10U) ? AQS_S_UNINIT : AQS_S_FETCH;
		k_work_reschedule_for_queue(&hub_wq, dwork, K_MSEC(rt->backoff_ms));
		return;
	}
}

int aqs_sensor_hub_init(void)
{
	k_work_queue_init(&hub_wq);
	k_work_queue_start(&hub_wq, hub_stack, K_THREAD_STACK_SIZEOF(hub_stack),
			   CONFIG_AQS_SENSOR_WQ_PRIO, NULL);
	k_thread_name_set(&hub_wq.thread, "aqs_sensor_wq");

	AQS_SENSOR_FOREACH(s) {
		k_work_init_delayable(&s->rt->work, sensor_work);
		s->rt->state = AQS_S_UNINIT;
		/* Stagger startup so N sensors don't hit the bus in the same tick. */
		k_work_reschedule_for_queue(&hub_wq, &s->rt->work,
					    K_MSEC(50 * aqs_sensor_index(s)));
		LOG_INF("registered %s (period >= %u ms, warmup %u ms)",
			s->name, s->min_period_ms, s->warmup_ms);
	}
	return 0;
}
