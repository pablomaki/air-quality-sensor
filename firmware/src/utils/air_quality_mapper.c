#include <utils/air_quality_mapper.h>

const char *air_quality_from_voc_index(int voc_index)
{
    if (voc_index >= 0 && voc_index < 80)
    {
        return "Excellent";
    }
    else if (voc_index >= 80 && voc_index < 120)
    {
        return "Good";
    }
    else if (voc_index >= 120 && voc_index < 200)
    {
        return "Fair";
    }
    else if (voc_index >= 200 && voc_index < 300)
    {
        return "Inferior";
    }
    else if (voc_index >= 300 && voc_index <= 500)
    {
        return "Poor";
    }
    else
    {
        return "Unknown";
    }
}
