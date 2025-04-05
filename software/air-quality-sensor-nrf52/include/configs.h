#ifndef CONFIG_H
#define CONFIG_H

// System
// #define ENABLE_SYSTEM_OFF // Enable to allow SYSTEM_OFF sleep, otherwise SYSTEM_ON IDLE loop is used

// Bluetooth
// #define ENABLE_CONN_FILTER_LIST // Use filter list for connections (not finished)

// Data config
#define DATA_INTERVAL 20000 // milliseconds
#define ADV_TIMEOUT 10000   // milliseconds

// LED config
#define LED_BLINK_INTERVAL 200 // milliseconds
#define ENABLE_DEBUG_LED       // Enable or disable debug LED

// Sensors
#define ENABLE_SHT4X
// #define ENABLE_SGP40
// #define ENABLE_SCD4X
// #define ENABLE_BMP280

// Value scaling and min max for warning
#define TEMPERATURE_SCALE 100
#define TEMPERATURE_MIN -100
#define TEMPERATURE_MAX 100

#define HUMIDITY_SCALE 100
#define HUMIDITY_MIN 0
#define HUMIDITY_MAX 100

#define PRESSURE_SCALE 10
#define PRESSURE_MIN 0
#define PRESSURE_MAX 2000

#define CO2_CONCENTRATION_SCALE 10
#define CO2_CONCENTRATION_MIN 0
#define CO2_CONCENTRATION_MAX 10000

#define VOC_INDEX_SCALE 10
#define VOC_INDEX_MIN 0
#define VOC_INDEX_MAX 10000

#endif