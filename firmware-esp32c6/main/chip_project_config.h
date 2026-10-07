#pragma once

#include <sdkconfig.h>

// The ESP32 platform has no Kconfig option for the product name, so the
// variant's AQS_PRODUCT_NAME reaches the Basic Information cluster through here.
#define CHIP_DEVICE_CONFIG_DEVICE_PRODUCT_NAME CONFIG_AQS_PRODUCT_NAME
