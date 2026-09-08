#ifndef AIR_QUALITY_MAPPER_H
#define AIR_QUALITY_MAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Map the VOC index to a string that describes the air quality
 *
 * @param voc_index Input VOC index value to map
 * @return const char* Excellent, Good, Ok, Poor, Bad or Unknown
 */
const char *air_quality_from_voc_index(int voc_index);

/**
 * @brief Map the IAQ index to a string that describes the air quality
 *
 * @param iaq_index Input IAQ index value to map
 * @return const char* Excellent, Good, Ok, Poor, Bad or Unknown
 */
const char *air_quality_from_iaq_index(int iaq_index);

#ifdef __cplusplus
}
#endif

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