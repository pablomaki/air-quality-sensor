#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(i2c_probe);

#define I2C_NODE DT_NODELABEL(i2c1)

/** @brief Addresses this project expects to find, probed individually */
static const struct {
	uint16_t addr;
	const char *name;
} expected[] = {
	{0x44, "sht4x"},
	{0x59, "sgp40"},
	{0x62, "scd41"},
	{0x77, "bmp390/bme680"},
};

/**
 * @brief Address one device and report whether it acknowledges
 *
 * Uses the zero length write that i2c scan uses, which the nRF TWI/TWIM
 * peripherals support.
 *
 * @param bus I2C controller
 * @param addr 7 bit address to probe
 * @return 0 if the device acknowledged, negative errno otherwise
 */
static int probe(const struct device *bus, uint16_t addr)
{
	uint8_t dummy;
	struct i2c_msg msg = {.buf = &dummy, .len = 0U, .flags = I2C_MSG_WRITE | I2C_MSG_STOP};

	return i2c_transfer(bus, &msg, 1, addr);
}

int main(void)
{
	const struct device *bus = DEVICE_DT_GET(I2C_NODE);

	if (!device_is_ready(bus)) {
		LOG_ERR("%s is not ready, the controller itself failed to initialize.", bus->name);
		return 0;
	}

	LOG_INF("Probing %s at %u Hz on SDA P0.04 / SCL P0.05.", bus->name,
		DT_PROP(I2C_NODE, clock_frequency));

	while (1) {
		int found = 0;

		for (uint16_t addr = 0x08; addr <= 0x77; addr++) {
			if (probe(bus, addr) == 0) {
				LOG_INF("  0x%02x acknowledged", addr);
				found++;
			}
		}

		LOG_INF("%d device(s) responded on the full scan.", found);

		ARRAY_FOR_EACH(expected, i) {
			int rc = probe(bus, expected[i].addr);

			LOG_INF("  %-14s 0x%02x -> %s (err %d)", expected[i].name, expected[i].addr,
				rc == 0 ? "ACK" : "no ACK", rc);
		}

		k_sleep(K_SECONDS(5));
	}

	return 0;
}
