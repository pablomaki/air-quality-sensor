#include <drivers/bmp390.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/pm/device.h>
#include <zephyr/drivers/sensor.h>

LOG_MODULE_REGISTER(BMP390, CONFIG_SENSOR_LOG_LEVEL);

static inline int bmp390_read_reg(const struct device *dev, uint8_t start, uint8_t *buf, int size)
{
    const bmp390_config_t *cfg = dev->config;
    return i2c_burst_read_dt(&cfg->bus, start, buf, size);
}

static inline int bmp390_write_reg(const struct device *dev, uint8_t reg, uint8_t val)
{
    const bmp390_config_t *cfg = dev->config;
    return i2c_reg_write_byte_dt(&cfg->bus, reg, val);
}

static int bmp390_reg_field_update(const struct device *dev, uint8_t reg, uint8_t mask, uint8_t val)
{
    int rc = 0;
    uint8_t current_value, updated_value;

    rc = bmp390_read_reg(dev, reg, &current_value, 1);
    if (rc != 0)
    {
        return rc;
    }

    updated_value = (current_value & ~mask) | (val & mask);
    if (updated_value == current_value)
    {
        return 0;
    }

    return bmp390_write_reg(dev, reg, updated_value);
}

static uint32_t get_conversion_time(const struct device *dev)
{
    const bmp390_config_t *cfg = dev->config;
    uint32_t time = 234;
    time += cfg->enable_pressure ? (392 + (1 << cfg->osr_pressure) * 2020) : 0;
    time += cfg->enable_temp ? (163 + (1 << cfg->osr_temp) * 2020) : 0;
    return 1.2 * time;
}

static int bmp390_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    bmp390_data_t *data = dev->data;
    const bmp390_config_t *cfg = dev->config;
    uint8_t raw[BMP390_SAMPLE_BUFFER_SIZE];
    int rc = 0;

    if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_AMBIENT_TEMP && chan != SENSOR_CHAN_PRESS)
    {
        return -ENOTSUP;
    }

    if (cfg->mode == BMP390_MODE_FORCED)
    {
        rc = bmp390_reg_field_update(dev, BMP390_REG_PWR_CTRL, BMP390_PWR_CTRL_MODE_MASK, BMP390_PWR_CTRL_MODE_FORCED);
        if (rc < 0)
        {
            return rc;
        }
        k_sleep(K_USEC(get_conversion_time(dev)));
    }
    else
    {
        /* Wait for status to indicate that data is ready. */
        raw[0] = 0U;
        while ((raw[0] & BMP390_STATUS_DRDY_PRESS) == 0U)
        {
            rc = bmp390_read_reg(dev, BMP390_REG_STATUS, raw, 1);
            if (rc < 0)
            {
                return rc;
            }
        }
    }

    rc = bmp390_read_reg(dev, BMP390_REG_DATA0, raw, BMP390_SAMPLE_BUFFER_SIZE);
    if (rc < 0)
    {
        return rc;
    }

    data->p_sample = sys_get_le24(&raw[0]);
    data->t_sample = sys_get_le24(&raw[3]);
    data->comp_temp = 0;

    return rc;
}

static void bmp390_compensate_temp(const struct device *dev)
{
    /* Adapted from:
     * https://github.com/BoschSensortec/BMP3-Sensor-API/blob/master/bmp3.c
     */
    bmp390_data_t *data = dev->data;
    bmp390_cal_data_t *cal = &data->cal;

    int64_t partial_data1;
    int64_t partial_data2;
    int64_t partial_data3;
    int64_t partial_data4;
    int64_t partial_data5;

    partial_data1 = ((int64_t)data->t_sample - (256 * cal->t1));
    partial_data2 = cal->t2 * partial_data1;
    partial_data3 = (partial_data1 * partial_data1);
    partial_data4 = (int64_t)partial_data3 * cal->t3;
    partial_data5 = ((int64_t)(partial_data2 * 262144) + partial_data4);

    /* Store for pressure calculation */
    data->comp_temp = partial_data5 / 4294967296;
}

static int bmp390_temp_channel_get(const struct device *dev, struct sensor_value *val)
{
    bmp390_data_t *data = dev->data;

    if (data->comp_temp == 0)
    {
        bmp390_compensate_temp(dev);
    }

    int64_t tmp = (data->comp_temp * 250000) / 16384;

    val->val1 = tmp / 1000000;
    val->val2 = tmp % 1000000;

    return 0;
}

static uint64_t bmp390_compensate_press(const struct device *dev)
{
    /* Adapted from:
     * https://github.com/BoschSensortec/BMP3-Sensor-API/blob/master/bmp3.c
     */
    bmp390_data_t *data = dev->data;
    bmp390_cal_data_t *cal = &data->cal;

    int64_t partial_data1;
    int64_t partial_data2;
    int64_t partial_data3;
    int64_t partial_data4;
    int64_t partial_data5;
    int64_t partial_data6;
    int64_t offset;
    int64_t sensitivity;
    uint64_t comp_press;

    int64_t t_lin = data->comp_temp;
    uint32_t raw_pressure = data->p_sample;

    partial_data1 = t_lin * t_lin;
    partial_data2 = partial_data1 / 64;
    partial_data3 = (partial_data2 * t_lin) / 256;
    partial_data4 = (cal->p8 * partial_data3) / 32;
    partial_data5 = (cal->p7 * partial_data1) * 16;
    partial_data6 = (cal->p6 * t_lin) * 4194304;
    offset = (cal->p5 * 140737488355328) + partial_data4 + partial_data5 +
             partial_data6;
    partial_data2 = (cal->p4 * partial_data3) / 32;
    partial_data4 = (cal->p3 * partial_data1) * 4;
    partial_data5 = (cal->p2 - 16384) * t_lin * 2097152;
    sensitivity = ((cal->p1 - 16384) * 70368744177664) + partial_data2 +
                  partial_data4 + partial_data5;
    partial_data1 = (sensitivity / 16777216) * raw_pressure;
    partial_data2 = cal->p10 * t_lin;
    partial_data3 = partial_data2 + (65536 * cal->p9);
    partial_data4 = (partial_data3 * raw_pressure) / 8192;
    /* Dividing by 10 followed by multiplying by 10 to avoid overflow caused
     * (raw_pressure * partial_data4)
     */
    partial_data5 = (raw_pressure * (partial_data4 / 10)) / 512;
    partial_data5 = partial_data5 * 10;
    partial_data6 = ((int64_t)raw_pressure * (int64_t)raw_pressure);
    partial_data2 = (cal->p11 * partial_data6) / 65536;
    partial_data3 = (partial_data2 * raw_pressure) / 128;
    partial_data4 = (offset / 4) + partial_data1 + partial_data5 +
                    partial_data3;

    comp_press = (((uint64_t)partial_data4 * 25) / (uint64_t)1099511627776);

    /* returned value is in hundredths of Pa. */
    return comp_press;
}

static int bmp390_press_channel_get(const struct device *dev, struct sensor_value *val)
{
    bmp390_data_t *data = dev->data;

    if (data->comp_temp == 0)
    {
        bmp390_compensate_temp(dev);
    }

    uint64_t tmp = bmp390_compensate_press(dev);

    // tmp is in hundredths of Pa. Convert to Pa
    val->val1 = tmp / 100;
    val->val2 = (tmp % 100);

    return 0;
}

static int bmp390_channel_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val)
{
    switch (chan)
    {
    case SENSOR_CHAN_PRESS:
        bmp390_press_channel_get(dev, val);
        break;
    case SENSOR_CHAN_AMBIENT_TEMP:
        bmp390_temp_channel_get(dev, val);
        break;

    default:
        LOG_DBG("Channel not supported.");
        return -ENOTSUP;
    }

    return 0;
}
static int bmp390_get_calibration_data(const struct device *dev)
{
    bmp390_data_t *data = dev->data;
    bmp390_cal_data_t *cal = &data->cal;
    int rc = 0;

    rc = bmp390_read_reg(dev, BMP390_REG_CALIB0, (uint8_t *)cal, sizeof(*cal));
    if (rc < 0)
    {
        return -EIO;
    }

    cal->t1 = sys_le16_to_cpu(cal->t1);
    cal->t2 = sys_le16_to_cpu(cal->t2);
    cal->p1 = (int16_t)sys_le16_to_cpu(cal->p1);
    cal->p2 = (int16_t)sys_le16_to_cpu(cal->p2);
    cal->p5 = sys_le16_to_cpu(cal->p5);
    cal->p6 = sys_le16_to_cpu(cal->p6);
    cal->p9 = (int16_t)sys_le16_to_cpu(cal->p9);

    return 0;
}

#ifdef CONFIG_PM_DEVICE
static int bmp390_pm_action(const struct device *dev, enum pm_device_action action)
{
    const bmp390_config_t *cfg = dev->config;
    uint8_t reg_val;
    int rc = 0;

    switch (action)
    {
    case PM_DEVICE_ACTION_RESUME:
        if (cfg->mode == BMP390_MODE_FORCED)
        {
            reg_val = BMP390_PWR_CTRL_MODE_FORCED;
            break;
        }
        else
        {
            reg_val = BMP390_PWR_CTRL_MODE_NORMAL;
            break;
        }
    case PM_DEVICE_ACTION_SUSPEND:
        reg_val = BMP390_PWR_CTRL_MODE_SLEEP;
        break;
    default:
        return -ENOTSUP;
    }

    rc = bmp390_reg_field_update(dev, BMP390_REG_PWR_CTRL, BMP390_PWR_CTRL_MODE_MASK, reg_val);
    if (rc < 0)
    {
        LOG_DBG("Failed to set power mode.");
        return -EIO;
    }

    return 0;
}
#endif /* CONFIG_PM_DEVICE */

static int bmp390_init(const struct device *dev)
{
    const bmp390_config_t *cfg = dev->config;
    uint8_t val = 0U;
    int rc = 0;

    if (!device_is_ready(cfg->bus.bus))
    {
        LOG_ERR("Device not ready.");
        return -ENODEV;
    }

    /* reboot the chip */
    rc = bmp390_write_reg(dev, BMP390_REG_CMD, BMP390_CMD_SOFT_RESET);
    if (rc < 0)
    {
        LOG_ERR("Cannot reboot chip (err %d).", rc);
        return -EIO;
    }
    k_busy_wait(BMP390_START_UP_WAIT_TIME_MS * 1000);

    /* Read calibration data */
    rc = bmp390_get_calibration_data(dev);
    if (rc < 0)
    {
        LOG_ERR("Failed to read calibration data (err %d).", rc);
        return -EIO;
    }

    if (cfg->mode == BMP390_MODE_NORMAL)
    {
        /* Set ODR */
        rc = bmp390_reg_field_update(dev, BMP390_REG_ODR, BMP390_ODR_MASK, cfg->odr);
        if (rc < 0)
        {
            LOG_ERR("Failed to set ODR.");
            return -EIO;
        }
    }

    /* Set OSR */
    val = (cfg->osr_pressure << BMP390_OSR_PRESSURE_POS);
    val |= (cfg->osr_temp << BMP390_OSR_TEMP_POS);
    rc = bmp390_write_reg(dev, BMP390_REG_OSR, val);
    if (rc < 0)
    {
        LOG_ERR("Failed to set OSR.");
        return -EIO;
    }

    /* Set IIR filter coefficient */
    val = (cfg->iir_filter << BMP390_IIR_FILTER_POS) & BMP390_IIR_FILTER_MASK;
    rc = bmp390_write_reg(dev, BMP390_REG_CONFIG, val);
    if (rc < 0)
    {
        LOG_ERR("Failed to set IIR coefficient.");
        return -EIO;
    }

    /* Enable sensors and set normal mode if applicable*/
    val = (cfg->enable_pressure ? BMP390_PWR_CTRL_PRESS_EN : 0) |
          (cfg->enable_temp ? BMP390_PWR_CTRL_TEMP_EN : 0) |
          (cfg->mode == BMP390_MODE_NORMAL ? BMP390_PWR_CTRL_MODE_NORMAL : BMP390_PWR_CTRL_MODE_SLEEP);
    rc = bmp390_write_reg(dev, BMP390_REG_PWR_CTRL, val);
    if (rc < 0)
    {
        LOG_ERR("Failed to enable sensors.");
        return -EIO;
    }

    /* Read error register */
    rc = bmp390_read_reg(dev, BMP390_REG_ERR_REG, &val, 1);
    if (rc < 0)
    {
        LOG_ERR("Failed get sensors error register.");
        return -EIO;
    }

    /* OSR and ODR config not proper */
    if (val & BMP390_STATUS_CONF_ERR)
    {
        LOG_ERR("OSR and ODR configuration is not proper.");
        return -EINVAL;
    }

    return 0;
}

static const struct sensor_driver_api bmp390_api = {
    .sample_fetch = bmp390_sample_fetch,
    .channel_get = bmp390_channel_get,
};

#define BMP390_INST(inst)                                       \
    static bmp390_data_t bmp390_data_##inst;                    \
                                                                \
    static const bmp390_config_t bmp390_config_##inst = {       \
        .bus = I2C_DT_SPEC_INST_GET(inst),                      \
        .mode = DT_INST_ENUM_IDX(inst, mode),                   \
        .iir_filter = DT_INST_ENUM_IDX(inst, iir_filter),       \
        .odr = DT_INST_ENUM_IDX(inst, odr),                     \
        .osr_pressure = DT_INST_ENUM_IDX(inst, osr_press),      \
        .osr_temp = DT_INST_ENUM_IDX(inst, osr_temp),           \
        .enable_pressure = DT_INST_PROP(inst, enable_pressure), \
        .enable_temp = DT_INST_PROP(inst, enable_temp),         \
    };                                                          \
    PM_DEVICE_DT_INST_DEFINE(inst, bmp390_pm_action);           \
                                                                \
    SENSOR_DEVICE_DT_INST_DEFINE(inst,                          \
                                 bmp390_init,                   \
                                 PM_DEVICE_DT_INST_GET(inst),   \
                                 &bmp390_data_##inst,           \
                                 &bmp390_config_##inst,         \
                                 POST_KERNEL,                   \
                                 CONFIG_SENSOR_INIT_PRIORITY,   \
                                 &bmp390_api);

#define DT_DRV_COMPAT bosch_bmp390
DT_INST_FOREACH_STATUS_OKAY(BMP390_INST)
#undef DT_DRV_COMPAT