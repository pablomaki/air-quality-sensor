#ifndef AIR_QUALITY_MAPPER_H
#define AIR_QUALITY_MAPPER_H

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

#endif // AIR_QUALITY_MAPPER_H