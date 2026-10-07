/* SKETCH — include/aqs/state.h
 * The single aggregated snapshot. Copy-out only: no consumer ever holds the lock.
 */
#ifndef AQS_STATE_H_
#define AQS_STATE_H_

#include <aqs/value.h>

struct aqs_state {
	struct aqs_value q[AQS_Q_COUNT];
	uint8_t          air_quality;   /* aq_index output, maps to Matter AirQualityEnum */
	int64_t          updated;       /* k_uptime_get() of last change */
};

/* Producer side — telemetry thread only. */
void aqs_state_update(enum aqs_quantity q, const struct aqs_value *v);
void aqs_state_expire(void);   /* VALID -> STALE -> ERROR by age; called on each tick */

/* Consumer side — any thread. Short mutex hold, then memcpy. */
void aqs_state_snapshot(struct aqs_state *out);

#endif /* AQS_STATE_H_ */
