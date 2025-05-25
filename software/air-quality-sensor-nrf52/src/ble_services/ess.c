#include <ble_services/ess.h>
#include <configs.h>

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

// Create service
BT_GATT_SERVICE_DEFINE(ess, BT_GATT_PRIMARY_SERVICE(BT_UUID_ESS),
#ifdef ENABLE_SHT4X
                       BT_GATT_CHARACTERISTIC(BT_UUID_TEMPERATURE, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ, read_data, NULL, &temperature),
                       BT_GATT_CHARACTERISTIC(BT_UUID_HUMIDITY, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ, read_data, NULL, &humidity),
#endif

#ifdef ENABLE_BMP390
                       BT_GATT_CHARACTERISTIC(BT_UUID_PRESSURE, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ, read_data, NULL, &pressure),
#endif

#ifdef ENABLE_SCD4X
                       BT_GATT_CHARACTERISTIC(BT_UUID_GATT_CO2CONC, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ, read_data, NULL, &co2_concentration),
#endif

#ifdef ENABLE_SGP40
                       BT_GATT_CHARACTERISTIC(BT_UUID_GATT_VOCCONC, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ, read_data, NULL, &voc_index),
#endif
);

float bt_ess_get_temperature(void)
{
    return (float)temperature / TEMPERATURE_SCALE;
}

float bt_ess_get_humidity(void)
{
    return (float)humidity / HUMIDITY_SCALE;
}

float bt_ess_get_pressure(void)
{
    return (float)pressure / PRESSURE_SCALE;
}

float bt_ess_get_co2_concentration(void)
{
    return (float)co2_concentration / CO2_CONCENTRATION_SCALE;
}

float bt_ess_get_voc_index(void)
{
    return (float)voc_index / VOC_INDEX_SCALE;
}

int bt_ess_set_temperature(float new_temperature)
{
    int ret = 0;
    if (new_temperature >= TEMPERATURE_MAX || new_temperature < TEMPERATURE_MIN)
    {
        ret = -EINVAL;
    }
    temperature = (uint16_t)(new_temperature * TEMPERATURE_SCALE);
    return ret;
}

int bt_ess_set_humidity(float new_humidity)
{
    int ret = 0;
    if (new_humidity > HUMIDITY_MAX || new_humidity < HUMIDITY_MIN)
    {
        ret = -EINVAL;
    }
    humidity = (uint16_t)(new_humidity * HUMIDITY_SCALE);
    return ret;
}

int bt_ess_set_pressure(float new_pressure)
{
    int ret = 0;
    if (new_pressure > PRESSURE_MAX || new_pressure < PRESSURE_MIN)
    {
        ret = -EINVAL;
    }
    pressure = (uint16_t)(new_pressure * PRESSURE_SCALE);
    return ret;
}

int bt_ess_set_co2_concentration(float new_co2_concentration)
{
    int ret = 0;
    if (new_co2_concentration > CO2_CONCENTRATION_MAX || new_co2_concentration < CO2_CONCENTRATION_MIN)
    {
        ret = -EINVAL;
    }
    co2_concentration = (uint16_t)(new_co2_concentration * CO2_CONCENTRATION_SCALE);
    return ret;
}

int bt_ess_set_voc_index(float new_voc_index)
{
    int ret = 0;
    if (new_voc_index > VOC_INDEX_MAX || new_voc_index < VOC_INDEX_MIN)
    {
        ret = -EINVAL;
    }
    voc_index = (uint16_t)(new_voc_index * VOC_INDEX_SCALE);
    return ret;
}