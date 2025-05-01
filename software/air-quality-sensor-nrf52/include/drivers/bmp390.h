#ifndef BMP390_H
#define BMP390_H

#include <zephyr/device.h>
// #include <zephyr/devicetree.h>
// #include <zephyr/drivers/spi.h>
#include <zephyr/drivers/i2c.h>
// #include <zephyr/drivers/gpio.h>
// #include <zephyr/drivers/sensor.h>
// #include <zephyr/sys/util.h>

/* registers */
#define BMP390_REG_CHIPID 0x00
#define BMP390_REG_ERR_REG 0x02
#define BMP390_REG_STATUS 0x03
#define BMP390_REG_DATA0 0x04
#define BMP390_REG_DATA1 0x05
#define BMP390_REG_DATA2 0x06
#define BMP390_REG_DATA3 0x07
#define BMP390_REG_DATA4 0x08
#define BMP390_REG_DATA5 0x09
#define BMP390_REG_SENSORTIME0 0x0C
#define BMP390_REG_SENSORTIME1 0x0D
#define BMP390_REG_SENSORTIME2 0x0E
#define BMP390_REG_SENSORTIME3 0x0F
#define BMP390_REG_EVENT 0x10
#define BMP390_REG_INT_STATUS 0x11
#define BMP390_REG_FIFO_LENGTH0 0x12
#define BMP390_REG_FIFO_LENGTH1 0x13
#define BMP390_REG_FIFO_DATA 0x14
#define BMP390_REG_FIFO_WTM0 0x15
#define BMP390_REG_FIFO_WTM1 0x16
#define BMP390_REG_FIFO_CONFIG1 0x17
#define BMP390_REG_FIFO_CONFIG2 0x18
#define BMP390_REG_INT_CTRL 0x19
#define BMP390_REG_IF_CONF 0x1A
#define BMP390_REG_PWR_CTRL 0x1B
#define BMP390_REG_OSR 0x1C
#define BMP390_REG_ODR 0x1D
#define BMP390_REG_CONFIG 0x1F
#define BMP390_REG_CALIB0 0x31
#define BMP390_REG_CMD 0x7E

/* BMP390_REG_CHIPID */
#define BMP390_CHIP_ID 0x50

/* BMP390_REG_STATUS */
#define BMP390_STATUS_FATAL_ERR BIT(0)
#define BMP390_STATUS_CMD_ERR BIT(1)
#define BMP390_STATUS_CONF_ERR BIT(2)
#define BMP390_STATUS_CMD_RDY BIT(4)
#define BMP390_STATUS_DRDY_PRESS BIT(5)
#define BMP390_STATUS_DRDY_TEMP BIT(6)

/* BMP390_REG_INT_CTRL */
#define BMP390_INT_CTRL_DRDY_EN_POS 6
#define BMP390_INT_CTRL_DRDY_EN_MASK BIT(6)

/* BMP390_REG_PWR_CTRL */
#define BMP390_PWR_CTRL_PRESS_EN BIT(0)
#define BMP390_PWR_CTRL_TEMP_EN BIT(1)
#define BMP390_PWR_CTRL_MODE_POS 4
#define BMP390_PWR_CTRL_MODE_MASK (0x03 << BMP390_PWR_CTRL_MODE_POS)
#define BMP390_PWR_CTRL_MODE_SLEEP (0x00 << BMP390_PWR_CTRL_MODE_POS)
#define BMP390_PWR_CTRL_MODE_FORCED (0x01 << BMP390_PWR_CTRL_MODE_POS)
#define BMP390_PWR_CTRL_MODE_NORMAL (0x03 << BMP390_PWR_CTRL_MODE_POS)

/* BMP390_REG_OSR */
#define BMP390_ODR_POS 0
#define BMP390_ODR_MASK 0x1F

/* BMP390_REG_ODR */
#define BMP390_OSR_PRESSURE_POS 0
#define BMP390_OSR_PRESSURE_MASK (0x07 << BMP390_OSR_PRESSURE_POS)
#define BMP390_OSR_TEMP_POS 3
#define BMP390_OSR_TEMP_MASK (0x07 << BMP390_OSR_TEMP_POS)

/* BMP390_REG_CONFIG */
#define BMP390_IIR_FILTER_POS 1
#define BMP390_IIR_FILTER_MASK (0x7 << BMP390_IIR_FILTER_POS)

/* BMP390_REG_CMD */
#define BMP390_CMD_FIFO_FLUSH 0xB0
#define BMP390_CMD_SOFT_RESET 0xB6

/* default PWR_CTRL settings */
#define BMP390_PWR_CTRL_ON      \
	(BMP390_PWR_CTRL_PRESS_EN | \
	 BMP390_PWR_CTRL_TEMP_EN |  \
	 BMP390_PWR_CTRL_MODE_NORMAL)
#define BMP390_PWR_CTRL_OFF 0

#define BMP390_SAMPLE_BUFFER_SIZE (6)

#define BMP390_START_UP_WAIT_TIME_MS 2

typedef struct
{
	uint16_t t1;
	uint16_t t2;
	int8_t t3;
	int16_t p1;
	int16_t p2;
	int8_t p3;
	int8_t p4;
	uint16_t p5;
	uint16_t p6;
	int8_t p7;
	int8_t p8;
	int16_t p9;
	int8_t p10;
	int8_t p11;
} __packed bmp390_cal_data_t;

/**
 * @brief BMP390 mode of operation.
 *
 */
typedef enum
{
	BMP390_MODE_NORMAL,
	BMP390_MODE_FORCED,
} bmp390_mode_t;

/**
 * @brief BMP390 configuration structure.
 *
 */
typedef struct
{
	struct i2c_dt_spec bus;
	bmp390_mode_t mode;
	uint8_t iir_filter;
	uint8_t odr;
	bool enable_pressure;
	bool enable_temp;
	uint8_t osr_pressure;
	uint8_t osr_temp;
} bmp390_config_t;

/**
 * @brief BMP390 data structure.
 *
 */
typedef struct
{
	uint32_t p_sample;
	uint32_t t_sample;
	int64_t comp_temp;
	bmp390_cal_data_t cal;
} bmp390_data_t;

typedef enum
{
	SENSOR_ATTR_BMP390_SAMPLING_RATE,
	SENSOR_ATTR_BMP390_MODE,
	SENSOR_ATTR_BMP390_COMPENSATION,
} sensor_attribute_bmp390;

#endif // BMP390_H