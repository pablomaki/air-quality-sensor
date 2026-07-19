#include <components/matter_handler.h>
#include <utils/variable_buffer.h>

#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(matter_handler);

int init_matter(void)
{
    int rc = 0;
    return rc;
}

int update_cluster_states(void)
{
    LOG_INF("Updating advertisement data.");

    int rc = 0;
#ifdef CONFIG_ENABLE_SHT4X
    float temperature = get_mean(TEMPERATURE);
    float hum = get_mean(HUMIDITY);
#endif

#if defined(CONFIG_ENABLE_BMP390) || defined(CONFIG_ENABLE_BME680)
    float pressure = get_mean(PRESSURE);
#endif

#ifdef CONFIG_ENABLE_SCD4X
    float co2_concentration = get_mean(CO2_CONCENTRATION);
#endif

#ifdef CONFIG_ENABLE_SGP40
    float voc_index = get_mean(VOC_INDEX);
#endif
#ifdef CONFIG_ENABLE_BME680
    float iaq_index = get_mean(IAQ_INDEX);
#endif
    return rc;
}