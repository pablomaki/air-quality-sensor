#ifndef AIR_QUALITY_MAPPER_H
#define AIR_QUALITY_MAPPER_H

#ifdef __cplusplus
#include <app/clusters/air-quality-server/air-quality-server.h>

/**
 * @brief Map a 0-500 VOC/IAQ index onto the Matter AirQualityEnum buckets (Bosch BME68x IAQ scale)
 *
 * @param index Input VOC/IAQ index value to map
 * @return chip::app::Clusters::AirQuality::AirQualityEnum
 */
chip::app::Clusters::AirQuality::AirQualityEnum air_quality_enum_from_index(int index);
#endif

#endif // AIR_QUALITY_MAPPER_H