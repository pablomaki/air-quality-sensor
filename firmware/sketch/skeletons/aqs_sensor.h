/* SKETCH — include/aqs/sensor.h
 * The plug-in contract. Core code depends on this; nothing here names a part number.
 */
#ifndef AQS_SENSOR_H_
#define AQS_SENSOR_H_

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <aqs/quantity.h>

/* Zephyr sensor channel -> our quantity, plus optional linear correction. */
struct aqs_channel_map {
	enum sensor_channel chan;
	enum aqs_quantity    quantity;
	float                scale;   /* 0 => 1.0 */
	float                offset;
};

struct aqs_sensor;

/* Optional per-part hooks. All may be NULL; sensor_hub handles the common case
 * (sensor_sample_fetch + sensor_channel_get) with no ops at all. */
struct aqs_sensor_ops {
	int (*init)(const struct aqs_sensor *s);
	int (*start)(const struct aqs_sensor *s);   /* power on / begin measurement */
	int (*stop)(const struct aqs_sensor *s);    /* for duty cycling + battery */
	int (*apply_config)(const struct aqs_sensor *s);  /* altitude, T offset, ASC */
	int (*decode)(const struct aqs_sensor *s, struct aqs_sample_msg *out);
	int (*self_test)(const struct aqs_sensor *s);
};

enum aqs_sensor_state { AQS_S_UNINIT, AQS_S_IDLE, AQS_S_WARMUP, AQS_S_FETCH, AQS_S_ERROR };

/* Mutable per-sensor runtime, kept out of the const descriptor. */
struct aqs_sensor_rt {
	struct k_work_delayable work;
	enum aqs_sensor_state   state;
	int64_t                 next_due;      /* absolute, so cadence cannot drift */
	uint32_t                backoff_ms;
	uint16_t                fail_count;
	uint32_t                sample_count;
};

struct aqs_sensor {
	const char                   *name;
	const struct device          *dev;
	const struct aqs_sensor_ops  *ops;
	const struct aqs_channel_map *maps;
	uint8_t                       map_count;
	uint32_t                      min_period_ms; /* device physics: lower bound */
	uint32_t                      warmup_ms;     /* 0 for always-on sensors */
	struct aqs_sensor_rt         *rt;
};

/* Self-registration into an iterable linker section. CMakeLists needs:
 *   zephyr_iterable_section(NAME aqs_sensor GROUP RODATA_REGION)          */
#define AQS_SENSOR_DEFINE(_id, ...)                                             \
	static struct aqs_sensor_rt _CONCAT(_id, _rt);                         \
	STRUCT_SECTION_ITERABLE(aqs_sensor, _id) = {                           \
		.name = STRINGIFY(_id),                                        \
		.rt   = &_CONCAT(_id, _rt),                                    \
		__VA_ARGS__                                                    \
	}

/* Iteration for sensor_hub, shell, and matter_endpoints (which endpoints to enable). */
#define AQS_SENSOR_FOREACH(_p) STRUCT_SECTION_FOREACH(aqs_sensor, _p)

bool aqs_sensor_provides(enum aqs_quantity q);  /* any registered sensor -> quantity? */

#endif /* AQS_SENSOR_H_ */
