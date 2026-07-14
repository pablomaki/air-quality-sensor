#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>

#include <drivers/scd4x.h>

LOG_MODULE_REGISTER(SCD4X, CONFIG_SENSOR_LOG_LEVEL);

static uint8_t scd4x_calc_crc(uint16_t value)
{
    uint8_t buf[2];
    sys_put_be16(value, buf);
    return crc8(buf, 2, SCD4X_CRC_POLY, SCD4X_CRC_INIT, false);
}

static int scd4x_read_reg(const struct device *dev, uint8_t *rx_buf, uint8_t rx_buf_size)
{
    const scd4x_config_t *cfg = dev->config;
    int rc = 0;

    rc = i2c_read_dt(&cfg->bus, rx_buf, rx_buf_size);
    if (rc < 0)
    {
        LOG_ERR("Failed to read i2c data (err %d).", rc);
        return rc;
    }

    for (uint8_t i = 0; i < (rx_buf_size / 3); i++)
    {
        rc = scd4x_calc_crc(sys_get_be16(&rx_buf[i * 3]));
        if (rc != rx_buf[(i * 3) + 2])
        {
            LOG_ERR("Invalid CRC (err %d).", rc);
            return -EIO;
        }
    }

    return 0;
}

static int scd4x_write_reg(const struct device *dev, uint16_t cmd, uint16_t *data, uint8_t data_size)
{
    const scd4x_config_t *cfg = dev->config;
    uint8_t tx_buf[((data_size * 3) + 2)];

    sys_put_be16(cmd, tx_buf);
    uint8_t tx_buf_pos = 2;

    for (uint8_t i = 0; i < data_size; i++)
    {
        sys_put_be16(data[i], &tx_buf[tx_buf_pos]);
        tx_buf_pos += 2;
        tx_buf[tx_buf_pos++] = scd4x_calc_crc(data[i]);
    }

    return i2c_write_dt(&cfg->bus, tx_buf, sizeof(tx_buf));
}

static int scd4x_read_sample(const struct device *dev, uint16_t *co2_sample, uint16_t *t_sample, uint16_t *rh_sample)
{
    int rc = 0;

    rc = scd4x_write_reg(dev, SCD4X_CMD_READ_MEASUREMENT, NULL, 0);
    if (rc < 0)
    {
        LOG_ERR("Failed to start measurement (err %d).", rc);
        return rc;
    }
    k_sleep(K_MSEC(SCD4X_DEFAULT_WAIT_MS));

    uint8_t rx_buf[9];
    rc = scd4x_read_reg(dev, rx_buf, sizeof(rx_buf));
    if (rc < 0)
    {
        LOG_ERR("Failed to read data from device (err %d).", rc);
        return rc;
    }

    *co2_sample = sys_get_be16(rx_buf);
    *t_sample = sys_get_be16(&rx_buf[3]);
    *rh_sample = sys_get_be16(&rx_buf[6]);

    return 0;
}

static int scd4x_set_idle_mode(const struct device *dev)
{
    const scd4x_config_t *cfg = dev->config;
    int rc = 0;

    if (cfg->mode == SCD4X_MODE_SINGLE_SHOT)
    {
        return 0;
    }
    else if (cfg->mode == SCD4X_MODE_POWER_CYCLED_SINGLE_SHOT)
    {
        /*send wake up command twice because of an expected nack return in power down mode*/
        rc = scd4x_write_reg(dev, SCD4X_CMD_WAKE_UP, NULL, 0);
        if (rc < 0)
        {
            LOG_ERR("Failed write wake_up command (err %d).", rc);
            return rc;
        }
        k_sleep(K_MSEC(SCD4X_WAKE_UP_WAIT_MS));
    }
    else
    {
        rc = scd4x_write_reg(dev, SCD4X_CMD_STOP_PERIODIC_MEASUREMENT, NULL, 0);
        if (rc < 0)
        {
            LOG_ERR("Failed to write stop_periodic_measurement command (err %d).", rc);
            return rc;
        }
        k_sleep(K_MSEC(SCD4X_STOP_PERIODIC_MEASUREMENT_WAIT_MS));
    }

    return 0;
}

static int scd4x_setup_measurement(const struct device *dev)
{
    const scd4x_config_t *cfg = dev->config;
    int rc = 0;

    switch ((scd4x_mode_t)cfg->mode)
    {
    case SCD4X_MODE_NORMAL:
        rc = scd4x_write_reg(dev, SCD4X_CMD_START_PERIODIC_MEASUREMENT, NULL, 0);
        if (rc < 0)
        {
            LOG_ERR("Failed to write start_periodic_measurement command (err %d).", rc);
            return rc;
        }
        break;
    case SCD4X_MODE_LOW_POWER:
        rc = scd4x_write_reg(dev, SCD4X_CMD_START_LOW_POWER_PERIODIC_MEASUREMENT, NULL, 0);
        if (rc < 0)
        {
            LOG_ERR("Failed to write start_low_power_periodic_measurement command (err %d).", rc);
            return rc;
        }
        break;
    case SCD4X_MODE_SINGLE_SHOT:
        break;
    case SCD4X_MODE_POWER_CYCLED_SINGLE_SHOT:
        break;
    default:
        return -EINVAL;
    }
    return 0;
}

static int scd4x_attr_set(const struct device *dev,
                          enum sensor_channel chan,
                          enum sensor_attribute attr,
                          const struct sensor_value *val)
{
    const scd4x_config_t *cfg = dev->config;
    int rc = 0;
    bool idle_mode = false;

    if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_AMBIENT_TEMP &&
        chan != SENSOR_CHAN_HUMIDITY && chan != SENSOR_CHAN_CO2)
    {
        return -ENOTSUP;
    }

    // Make sure the sensor is in idle mode when setting anythign else but ambient pressure
    if ((sensor_attribute_scd4x)attr != SENSOR_ATTR_SCD4X_AMBIENT_PRESSURE)
    {
        rc = scd4x_set_idle_mode(dev);
        if (rc < 0)
        {
            LOG_ERR("Failed to set idle mode (err %d).", rc);
            return rc;
        }
        idle_mode = true;
    }

    if (val->val1 < 0 || val->val2 < 0)
    {
        return -EINVAL;
    }

    uint16_t ticks;
    switch ((sensor_attribute_scd4x)attr)
    {
    case SENSOR_ATTR_SCD4X_TEMPERATURE_OFFSET:
        if (val->val1 < SCD4X_OFFSET_TEMP_MIN || val->val1 > SCD4X_OFFSET_TEMP_MAX)
        {
            return -EINVAL;
        }
        ticks = (float)(val->val1 + (val->val2 / 1000000.0)) * 0xFFFF / 175;
        rc = scd4x_write_reg(dev, SCD4X_CMD_SET_TEMPERATURE_OFFSET, &ticks, 1);
        break;

    case SENSOR_ATTR_SCD4X_ALTITUDE:
        if (val->val1 < SCD4X_COMP_MIN_ALT || val->val1 > SCD4X_COMP_MAX_ALT)
        {
            return -EINVAL;
        }
        ticks = val->val1;
        rc = scd4x_write_reg(dev, SCD4X_CMD_SET_SENSOR_ALTITUDE, &ticks, 1);
        break;

    case SENSOR_ATTR_SCD4X_AMBIENT_PRESSURE:
        if (val->val1 < SCD4X_COMP_MIN_AP || val->val1 > SCD4X_COMP_MAX_AP)
        {
            return -EINVAL;
        }
        ticks = (uint16_t)((float)val->val1 / 100.0f + 0.5f); // round to nearest
        rc = scd4x_write_reg(dev, SCD4X_CMD_SET_AMBIENT_PRESSURE, &ticks, 1);
        break;

    case SENSOR_ATTR_SCD4X_AUTOMATIC_CALIB_ENABLE:
        if (val->val1 != 0 && val->val1 != 1)
        {
            return -EINVAL;
        }

        ticks = val->val1;
        rc = scd4x_write_reg(dev, SCD4X_CMD_SET_AUTOMATIC_SELF_CALIBRATION_ENABLED, &ticks, 1);
        break;

    case SENSOR_ATTR_SCD4X_SELF_CALIB_INITIAL_PERIOD:
        if (val->val1 % 4)
        {
            return -EINVAL;
        }
        if (cfg->model == SCD4X_MODEL_SCD40)
        {
            LOG_ERR("SELF_CALIB_INITIAL_PERIOD not available for SCD40.");
            return -ENOTSUP;
        }

        ticks = val->val1;
        rc = scd4x_write_reg(dev, SCD4X_CMD_SET_AUTOMATIC_SELF_CALIBRATION_INITIAL_PERIOD, &ticks, 1);
        break;

    case SENSOR_ATTR_SCD4X_SELF_CALIB_STANDARD_PERIOD:
        if (val->val1 % 4)
        {
            return -EINVAL;
        }
        if (cfg->model == SCD4X_MODEL_SCD40)
        {
            LOG_ERR("SELF_CALIB_STANDARD_PERIOD not available for SCD40.");
            return -ENOTSUP;
        }

        ticks = val->val1;
        rc = scd4x_write_reg(dev, SCD4X_CMD_SET_AUTOMATIC_SELF_CALIBRATION_STANDARD_PERIOD, &ticks, 1);
        break;

    default:
        return -ENOTSUP;
    }

    if (rc < 0)
    {
        LOG_ERR("Failed to set attribute (err %d).", rc);
        return rc;
    }
    k_sleep(K_MSEC(SCD4X_DEFAULT_WAIT_MS));

    if (idle_mode)
    {
        rc = scd4x_setup_measurement(dev);
        if (rc < 0)
        {
            LOG_ERR("Failed to setup measurement (err %d).", rc);
            return rc;
        }
    }

    return 0;
}

static int scd4x_data_ready(const struct device *dev, bool *is_data_ready)
{
    uint8_t rx_buf[3];
    int rc = 0;

    *is_data_ready = false;

    rc = scd4x_write_reg(dev, SCD4X_CMD_GET_DATA_READY_STATUS, NULL, 0);
    if (rc < 0)
    {
        LOG_ERR("Failed to write get_data_ready_status command (err %d).", rc);
        return rc;
    }
    k_sleep(K_MSEC(SCD4X_DEFAULT_WAIT_MS));

    rc = scd4x_read_reg(dev, rx_buf, sizeof(rx_buf));
    if (rc < 0)
    {
        LOG_ERR("Failed to read get_data_ready_status register (err %d).", rc);
        return rc;
    }

    uint16_t word = sys_get_be16(rx_buf);
    if ((word & 0x07FF) > 0)
    {
        *is_data_ready = true;
    }

    return 0;
}

static int scd4x_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    const scd4x_config_t *cfg = dev->config;
    scd4x_data_t *data = dev->data;
    int rc = 0;

    if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_CO2 && chan != SENSOR_CHAN_AMBIENT_TEMP &&
        chan != SENSOR_CHAN_HUMIDITY)
    {
        return -ENOTSUP;
    }

    if (cfg->mode == SCD4X_MODE_SINGLE_SHOT || cfg->mode == SCD4X_MODE_POWER_CYCLED_SINGLE_SHOT)
    {
        rc = scd4x_write_reg(dev, SCD4X_CMD_MEASURE_SINGLE_SHOT, NULL, 0);
        if (rc < 0)
        {
            LOG_ERR("Failed to start measurement (err %d).", rc);
            return rc;
        }
        k_sleep(K_MSEC(SCD4X_MEASURE_SINGLE_SHOT_WAIT_MS));
    }
    else
    {
        bool is_data_ready;
        rc = scd4x_data_ready(dev, &is_data_ready);
        if (rc < 0)
        {
            LOG_ERR("Failed to check data ready (err %d).", rc);
            return rc;
        }
        if (!is_data_ready)
        {
            LOG_WRN("Data not ready yet.");
            return -EIO;
        }
    }

    rc = scd4x_read_sample(dev, &data->co2_sample, &data->t_sample, &data->rh_sample);
    if (rc < 0)
    {
        LOG_ERR("Failed to fetch data (err %d).", rc);
        return rc;
    }

    return 0;
}

static int scd4x_channel_get(const struct device *dev,
                             enum sensor_channel chan,
                             struct sensor_value *val)
{
    const scd4x_data_t *data = dev->data;

    if (chan == SENSOR_CHAN_CO2)
    {
        val->val1 = data->co2_sample;
        val->val2 = 0;
    }
    else if (chan == SENSOR_CHAN_AMBIENT_TEMP)
    {
        int64_t tmp;
        tmp = data->t_sample * 175;
        val->val1 = (int32_t)(tmp / 0xFFFF) - 45;
        val->val2 = ((tmp % 0xFFFF) * 1000000) / 0xFFFF;
    }
    else if (chan == SENSOR_CHAN_HUMIDITY)
    {
        uint64_t tmp;
        tmp = data->rh_sample * 100U;
        val->val1 = (int32_t)(tmp / 0xFFFF);
        val->val2 = (tmp % 0xFFFF) * 1000000 / 0xFFFF;
    }
    else
    {
        return -ENOTSUP;
    }

    return 0;
}

int scd4x_forced_recalibration(const struct device *dev, uint16_t target_concentration_ticks,
                               uint16_t *frc_correction)
{
    uint8_t rx_buf[3];
    int rc = 0;

    rc = scd4x_set_idle_mode(dev);
    if (rc < 0)
    {
        LOG_ERR("Failed to set idle mode (err %d).", rc);
        return rc;
    }

    rc = scd4x_write_reg(dev, SCD4X_CMD_PERFORM_FORCED_RECALIBRATION, &target_concentration_ticks, 1);
    if (rc < 0)
    {
        LOG_ERR("Failed to write perform_forced_recalibration register (err %d).", rc);
        return rc;
    }
    k_sleep(K_MSEC(SCD4X_FORCED_CALIBRATION_WAIT_MS));

    rc = scd4x_read_reg(dev, rx_buf, sizeof(rx_buf));
    if (rc < 0)
    {
        LOG_ERR("Failed to read perform_forced_recalibration register (err %d).", rc);
        return rc;
    }

    *frc_correction = sys_get_be16(rx_buf);

    /*from datasheet*/
    if (*frc_correction == 0xFFFF)
    {
        LOG_ERR("FRC failed. Returned 0xFFFF (err %d).", rc);
        return -EIO;
    }

    *frc_correction -= 0x8000;

    rc = scd4x_setup_measurement(dev);
    if (rc < 0)
    {
        LOG_ERR("Failed to setup measurement (err %d).", rc);
        return rc;
    }

    return 0;
}

int scd4x_factory_reset(const struct device *dev)
{
    int rc = 0;

    rc = scd4x_set_idle_mode(dev);
    if (rc < 0)
    {
        LOG_ERR("Failed to set idle mode (err %d).", rc);
        return rc;
    }

    rc = scd4x_write_reg(dev, SCD4X_CMD_PERFORM_FACTORY_RESET, NULL, 0);
    if (rc < 0)
    {
        LOG_ERR("Failed to write perfom_factory_reset command (err %d).", rc);
        return rc;
    }
    k_sleep(K_MSEC(SCD4X_FACTORY_RESET_WAIT_MS));

    rc = scd4x_setup_measurement(dev);
    if (rc < 0)
    {
        LOG_ERR("Failed to setup measurement (err %d).", rc);
        return rc;
    }
    return 0;
}

int scd4x_persist_settings(const struct device *dev)
{
    int rc = 0;

    rc = scd4x_set_idle_mode(dev);
    if (rc < 0)
    {
        LOG_ERR("Failed to set idle mode (err %d).", rc);
        return rc;
    }

    rc = scd4x_write_reg(dev, SCD4X_CMD_PERSIST_SETTINGS, NULL, 0);
    if (rc < 0)
    {
        LOG_ERR("Failed to write persist_settings command (err %d).", rc);
        return rc;
    }
    k_sleep(K_MSEC(SCD4X_PERSIST_SETTINGS_WAIT_MS));

    rc = scd4x_setup_measurement(dev);
    if (rc < 0)
    {
        LOG_ERR("Failed to setup measurement (err %d).", rc);
        return rc;
    }

    return 0;
}

static int scd4x_selftest(const struct device *dev)
{
    uint8_t rx_buf[3];
    uint16_t return_value;
    int rc = 0;

    rc = scd4x_set_idle_mode(dev);
    if (rc < 0)
    {
        LOG_ERR("Failed to set idle mode (err %d).", rc);
        return rc;
    }

    rc = scd4x_write_reg(dev, SCD4X_CMD_PERFORM_SELF_TEST, NULL, 0);
    if (rc < 0)
    {
        LOG_ERR("Failed to start selftest (err %d).", rc);
        return rc;
    }
    k_sleep(K_MSEC(SCD4X_TEST_WAIT_MS));

    rc = scd4x_read_reg(dev, rx_buf, sizeof(rx_buf));
    if (rc < 0)
    {
        LOG_ERR("Failed to read data sample (err %d).", rc);
        return rc;
    }

    return_value = sys_get_be16(rx_buf);
    if (return_value != SCD4X_TEST_OK)
    {
        LOG_ERR("Selftest failed (return value %d != 0x0000).", return_value);
        return -EIO;
    }

    return 0;
}

#ifdef CONFIG_PM_DEVICE
static int scd4x_pm_action(const struct device *dev,
                           enum pm_device_action action)
{
    const scd4x_config_t *cfg = dev->config;

    if (cfg->mode != SCD4X_MODE_POWER_CYCLED_SINGLE_SHOT)
    {
        return 0;
    }

    uint16_t cmd;
    switch (action)
    {
    case PM_DEVICE_ACTION_RESUME:
        /* activate the hotplate by sending a measure command */
        cmd = SCD4X_CMD_WAKE_UP;
        break;
    case PM_DEVICE_ACTION_SUSPEND:
        cmd = SCD4X_CMD_POWER_DOWN;
        break;
    default:
        return -ENOTSUP;
    }

    return scd4x_write_reg(dev, cmd, NULL, 0);
}
#endif /* CONFIG_PM_DEVICE */

static int scd4x_init(const struct device *dev)
{
    const scd4x_config_t *cfg = dev->config;
    int rc = 0;

    if (!device_is_ready(cfg->bus.bus))
    {
        LOG_ERR("Device not ready.");
        return -ENODEV;
    }

    // Wait for device wake up, and make sure it is woken up
    rc = scd4x_write_reg(dev, SCD4X_CMD_WAKE_UP, NULL, 0);
    if (rc < 0)
    {
        LOG_ERR("Failed to wake up the device (err %d).", rc);
        return rc;
    }
    k_sleep(K_MSEC(SCD4X_WAKE_UP_WAIT_MS));

    rc = scd4x_write_reg(dev, SCD4X_CMD_STOP_PERIODIC_MEASUREMENT, NULL, 0);
    if (rc < 0)
    {
        LOG_ERR("Failed to put the device to idle mode (err %d).", rc);
        return rc;
    }
    k_sleep(K_MSEC(SCD4X_STOP_PERIODIC_MEASUREMENT_WAIT_MS));

    rc = scd4x_write_reg(dev, SCD4X_CMD_REINIT, NULL, 0);
    if (rc < 0)
    {
        LOG_ERR("Failed to reinitialize the device (err %d).", rc);
        return rc;
    }
    k_sleep(K_MSEC(SCD4X_REINIT_WAIT_MS));

    if (cfg->selftest)
    {
        rc = scd4x_selftest(dev);

        if (rc < 0)
        {
            LOG_ERR("Selftest failed (err %d).", rc);
            return rc;
        }
        LOG_DBG("Selftest succeeded.");
    }

    rc = scd4x_setup_measurement(dev);
    if (rc < 0)
    {
        LOG_ERR("Failed to setup measurement (err %d).", rc);
        return rc;
    }

    return 0;
}

static const struct sensor_driver_api scd4x_api = {
    .sample_fetch = scd4x_sample_fetch,
    .channel_get = scd4x_channel_get,
    .attr_set = scd4x_attr_set,
};

#define SCD4X_INIT(inst, scd4x_model)                                    \
    static scd4x_data_t scd4x_data_t_##scd4x_model##_##inst;             \
                                                                         \
    scd4x_config_t scd4x_config_t_##scd4x_model##_##inst = {             \
        .bus = I2C_DT_SPEC_INST_GET(inst),                               \
        .model = scd4x_model,                                            \
        .mode = DT_INST_ENUM_IDX_OR(inst, mode, SCD4X_MODE_NORMAL),      \
        .selftest = DT_INST_PROP(inst, enable_selftest),                 \
    };                                                                   \
                                                                         \
    PM_DEVICE_DT_INST_DEFINE(inst, scd4x_pm_action);                     \
                                                                         \
    SENSOR_DEVICE_DT_INST_DEFINE(inst,                                   \
                                 scd4x_init,                             \
                                 PM_DEVICE_DT_INST_GET(inst),            \
                                 &scd4x_data_t_##scd4x_model##_##inst,   \
                                 &scd4x_config_t_##scd4x_model##_##inst, \
                                 POST_KERNEL,                            \
                                 CONFIG_SENSOR_INIT_PRIORITY,            \
                                 &scd4x_api);

#define DT_DRV_COMPAT sensirion_scd40
DT_INST_FOREACH_STATUS_OKAY_VARGS(SCD4X_INIT, SCD4X_MODEL_SCD40);
#undef DT_DRV_COMPAT

#define DT_DRV_COMPAT sensirion_scd41
DT_INST_FOREACH_STATUS_OKAY_VARGS(SCD4X_INIT, SCD4X_MODEL_SCD41);
#undef DT_DRV_COMPAT