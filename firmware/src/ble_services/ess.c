#include <ble_services/ess.h>

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ess);

// Declaration and initialization of sensor reading values
static int16_t temperature = 0;
static int16_t humidity = 0;
static int16_t pressure = 0;
static int16_t co2_concentration = 0;
static int16_t voc_index = 0;

/** @brief Generic Read Attribute value helper.
 *
 *  @param conn Connection object.
 *  @param attr Attribute to read.
 *  @param buf Buffer to store the value.
 *  @param buf_len Buffer length.
 *  @param offset Start offset.
 *  @param value Attribute value.
 *  @param value_len Length of the attribute value.
 *
 *  @return number of bytes read in case of success or negative values in case of error.
 */
static ssize_t read_data(struct bt_conn *conn,
                         const struct bt_gatt_attr *attr, void *buf,
                         uint16_t len, uint16_t offset)
{
    const uint16_t *sensor_value_ptr = (const uint16_t *)attr->user_data;
    return bt_gatt_attr_read(conn, attr, buf, len, offset, sensor_value_ptr, sizeof(*sensor_value_ptr));
}

// Custom VOC index UUID
#define BT_UUID_VOC_INDEX_VAL BT_UUID_128_ENCODE(0x8caa4e2a, 0x31ef, 0x4e50, 0xa19d, 0xbdfd38918119)
#define BT_UUID_VOC_INDEX BT_UUID_DECLARE_128(BT_UUID_VOC_INDEX_VAL)

// Create service
BT_GATT_SERVICE_DEFINE(ess, BT_GATT_PRIMARY_SERVICE(BT_UUID_ESS),
#ifdef CONFIG_ENABLE_SHT4X
                       BT_GATT_CHARACTERISTIC(BT_UUID_TEMPERATURE, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ_ENCRYPT, read_data, NULL, &temperature),
                       BT_GATT_CHARACTERISTIC(BT_UUID_HUMIDITY, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ_ENCRYPT, read_data, NULL, &humidity),
#endif

#ifdef CONFIG_ENABLE_BMP390
                       BT_GATT_CHARACTERISTIC(BT_UUID_PRESSURE, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ_ENCRYPT, read_data, NULL, &pressure),
#endif

#ifdef CONFIG_ENABLE_SCD4X
                       BT_GATT_CHARACTERISTIC(BT_UUID_GATT_CO2CONC, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ_ENCRYPT, read_data, NULL, &co2_concentration),
#endif

#ifdef CONFIG_ENABLE_SGP40
                       BT_GATT_CHARACTERISTIC(BT_UUID_VOC_INDEX, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ_ENCRYPT, read_data, NULL, &voc_index),
#endif
);

float bt_ess_get_temperature(void)
{
    return (float)temperature / CONFIG_TEMPERATURE_SCALE;
}

float bt_ess_get_humidity(void)
{
    return (float)humidity / CONFIG_HUMIDITY_SCALE;
}

float bt_ess_get_pressure(void)
{
    return (float)pressure / ((float)CONFIG_PRESSURE_SCALE / 10.0f);
}

float bt_ess_get_co2_concentration(void)
{
    return (float)co2_concentration / CONFIG_CO2_CONCENTRATION_SCALE;
}

float bt_ess_get_voc_index(void)
{
    return (float)voc_index / CONFIG_VOC_INDEX_SCALE;
}

int bt_ess_set_temperature(float new_temperature)
{
    int ret = 0;
    if (new_temperature > 1000 || new_temperature < 0)
    {
        ret = -EINVAL;
    }
    temperature = (uint16_t)(new_temperature * CONFIG_TEMPERATURE_SCALE);
    return ret;
}

int bt_ess_set_humidity(float new_humidity)
{
    int ret = 0;
    if (new_humidity > 100 || new_humidity < 0)
    {
        ret = -EINVAL;
    }
    humidity = (uint16_t)(new_humidity * CONFIG_HUMIDITY_SCALE);
    return ret;
}

int bt_ess_set_pressure(float new_pressure)
{
    int ret = 0;
    if (new_pressure > 200000 || new_pressure < 0)
    {
        ret = -EINVAL;
    }
    pressure = (uint16_t)(new_pressure * ((float)CONFIG_PRESSURE_SCALE / 10.0f));
    return ret;
}

int bt_ess_set_co2_concentration(float new_co2_concentration)
{
    int ret = 0;
    if (new_co2_concentration > 10000 || new_co2_concentration < 0)
    {
        ret = -EINVAL;
    }
    co2_concentration = (uint16_t)(new_co2_concentration * CONFIG_CO2_CONCENTRATION_SCALE);
    return ret;
}

int bt_ess_set_voc_index(float new_voc_index)
{
    int ret = 0;
    if (new_voc_index > 500 || new_voc_index < 0)
    {
        ret = -EINVAL;
    }
    voc_index = (uint16_t)(new_voc_index * CONFIG_VOC_INDEX_SCALE);
    return ret;
}