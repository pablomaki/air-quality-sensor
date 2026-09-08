#include <utils/air_quality_mapper.h>

chip::app::Clusters::AirQuality::AirQualityEnum air_quality_enum_from_index(int index)
{
    using chip::app::Clusters::AirQuality::AirQualityEnum;
    if (index < 0 || index > 500)
    {
        return AirQualityEnum::kUnknown;
    }
    if (index <= 100)
    {
        return AirQualityEnum::kGood;
    }
    if (index <= 150)
    {
        return AirQualityEnum::kFair;
    }
    if (index <= 200)
    {
        return AirQualityEnum::kModerate;
    }
    if (index <= 250)
    {
        return AirQualityEnum::kPoor;
    }
    if (index <= 350)
    {
        return AirQualityEnum::kVeryPoor;
    }
    return AirQualityEnum::kExtremelyPoor;
}
