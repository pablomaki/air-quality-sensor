#ifndef SCD4X_H
#define SCD4X_H

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>

#define SCD4X_CMD_START_PERIODIC_MEASUREMENT 0x21B1
#define SCD4X_CMD_READ_MEASUREMENT 0xEC05
#define SCD4X_CMD_STOP_PERIODIC_MEASUREMENT 0x3F86
#define SCD4X_CMD_SET_TEMPERATURE_OFFSET 0x241d
#define SCD4X_CMD_GET_TEMPERATURE_OFFSET 0x2318
#define SCD4X_CMD_SET_SENSOR_ALTITUDE 0x2427
#define SCD4X_CMD_GET_SENSOR_ALTITUDE 0x2322
#define SCD4X_CMD_SET_AMBIENT_PRESSURE 0xE000
#define SCD4X_CMD_GET_AMBIENT_PRESSURE 0xE000
#define SCD4X_CMD_PERFORM_FORCED_RECALIBRATION 0x362F
#define SCD4X_CMD_SET_AUTOMATIC_SELF_CALIBRATION_ENABLED 0x2416
#define SCD4X_CMD_GET_AUTOMATIC_SELF_CALIBRATION_ENABLED 0x2313
#define SCD4X_CMD_SET_AUTOMATIC_SELF_CALIBRATION_TARGET 0x243A
#define SCD4X_CMD_GET_AUTOMATIC_SELF_CALIBRATION_TARGET 0x233F
#define SCD4X_CMD_START_LOW_POWER_PERIODIC_MEASUREMENT 0x21AC
#define SCD4X_CMD_GET_DATA_READY_STATUS 0xE4B8
#define SCD4X_CMD_PERSIST_SETTINGS 0x3615
#define SCD4X_CMD_GET_SERIAL_NUMBER 0x3682
#define SCD4X_CMD_PERFORM_SELF_TEST 0x3639
#define SCD4X_CMD_PERFORM_FACTORY_RESET 0x3632
#define SCD4X_CMD_REINIT 0x3646
#define SCD4X_CMD_GET_SENSOR_VARIANT 0x202F
#define SCD4X_CMD_MEASURE_SINGLE_SHOT 0x219D
#define SCD4X_CMD_MEASURE_SINGLE_SHOT_RHT_ONLY 0x2196
#define SCD4X_CMD_POWER_DOWN 0x36E0
#define SCD4X_CMD_WAKE_UP 0x36F6
#define SCD4X_CMD_SET_AUTOMATIC_SELF_CALIBRATION_INITIAL_PERIOD 0x2445
#define SCD4X_CMD_GET_AUTOMATIC_SELF_CALIBRATION_INITIAL_PERIOD 0x2340
#define SCD4X_CMD_SET_AUTOMATIC_SELF_CALIBRATION_STANDARD_PERIOD 0x244E
#define SCD4X_CMD_GET_AUTOMATIC_SELF_CALIBRATION_STANDARD_PERIOD 0x234B

#define SCD4X_TEST_OK 0x0000

#define SCD4X_STOP_PERIODIC_MEASUREMENT_WAIT_MS 500
#define SCD4X_FORCED_CALIBRATION_WAIT_MS 400
#define SCD4X_PERSIST_SETTINGS_WAIT_MS 800
#define SCD4X_TEST_WAIT_MS 10000
#define SCD4X_FACTORY_RESET_WAIT_MS 1200
#define SCD4X_REINIT_WAIT_MS 30
#define SCD4X_MEASURE_SINGLE_SHOT_WAIT_MS 5000
#define SCD4X_MEASURE_SINGLE_SHOT_RHT_ONLY_WAIT_MS 50
#define SCD4X_WAKE_UP_WAIT_MS 30
#define SCD4X_DEFAULT_WAIT_MS 1

/*
 * CRC parameters were taken from the
 * "Checksum Calculation" section of the datasheet.
 */
#define SCD4X_CRC_POLY 0x31
#define SCD4X_CRC_INIT 0xFF

/*
 * Value range of compensation data parameters
 */
#define SCD4X_COMP_MIN_ALT 0	 // meters
#define SCD4X_COMP_MAX_ALT 3000	 // meters
#define SCD4X_OFFSET_TEMP_MIN 0	 // meters
#define SCD4X_OFFSET_TEMP_MAX 20 // meters
#define SCD4X_COMP_MIN_AP 70000	 // pascals
#define SCD4X_COMP_MAX_AP 120000 // pascals
#define SCD4X_COMP_DEFAULT_ALT 0
#define SCD4X_COMP_DEFAULT_AP 101300

typedef enum
{
	SCD4X_MODE_NORMAL,
	SCD4X_MODE_LOW_POWER,
	SCD4X_MODE_SINGLE_SHOT,
	SCD4X_MODE_POWER_CYCLED_SINGLE_SHOT,
} scd4x_mode_t;

typedef enum
{
	SCD4X_MODEL_SCD40,
	SCD4X_MODEL_SCD41,
} scd4x_model_t;

typedef struct
{
	struct i2c_dt_spec bus;
	scd4x_mode_t mode;
	scd4x_model_t model;
	bool selftest;
} scd4x_config_t;

typedef struct
{
	uint16_t co2_sample;
	uint16_t t_sample;
	uint16_t rh_sample;
} scd4x_data_t;

typedef enum
{
	/* Offset temperature: Toffset_actual = Tscd4x – Treference + Toffset_previous
	 * Allowed values: 0 - 20°C
	 * Default: 4°C
	 */
	SENSOR_ATTR_SCD4X_TEMPERATURE_OFFSET = SENSOR_ATTR_PRIV_START,
	/* Altidude of the sensor;
	 * Allowed values: 0 - 3000m
	 * Default: 0m
	 */
	SENSOR_ATTR_SCD4X_ALTITUDE,
	/* Ambient pressure in hPa
	 * Allowed values: 700 - 1200hPa
	 * Default: 1013hPa
	 */
	SENSOR_ATTR_SCD4X_AMBIENT_PRESSURE,
	/* Set the current state (enabled: 1 / disabled: 0).
	 * Default: enabled.
	 */
	SENSOR_ATTR_SCD4X_AUTOMATIC_CALIB_ENABLE,
	/* Set the initial period for automatic self calibration correction in hours.
	 * Allowed values are integer multiples of 4 hours.
	 * Default: 44
	 */
	SENSOR_ATTR_SCD4X_SELF_CALIB_INITIAL_PERIOD,
	/* Set the standard period for automatic self calibration correction in hours.
	 * Allowed values are integer multiples of 4 hours.
	 * Default: 156
	 */
	SENSOR_ATTR_SCD4X_SELF_CALIB_STANDARD_PERIOD,
} sensor_attribute_scd4x;

#endif // SCD4X_H