#ifndef CONFIG_H
#define CONFIG_H

// System
// #define ENABLE_SYSTEM_OFF // Enable to allow SYSTEM_OFF sleep, otherwise SYSTEM_ON IDLE loop is used

// Bluetooth
// #define ENABLE_CONN_FILTER_LIST // Use filter list for connections (not finished)

// Data config
#define FIRST_TASK_DELAY 1000                                                   // milliseconds
#define ADVERTISEMENT_INTERVAL 60000                                            // milliseconds
#define BLE_TIMEOUT 10000                                                       // milliseconds
#define MEASUREMENTS_PER_INTERVAL 3                                             // Number of measurements per interval
#define MEASUREMENT_INTERVAL ADVERTISEMENT_INTERVAL / MEASUREMENTS_PER_INTERVAL // milliseconds

// LED config
#define LED_BLINK_INTERVAL 50 // milliseconds
#define ENABLE_EVENT_LED      // Enable or disable event LED
// #define ENABLE_STATE_LED     // Enable or disable LED for showing the current status

// Sensors
#define ENABLE_SHT4X
#define ENABLE_SGP40
#define ENABLE_SCD4X
#define ENABLE_BMP390

// Environment
#define ALTITUDE 10          // meters
#define TEMPERATURE_OFFSET 0 // meters

// Screen
// #define ENABLE_EPD //Enable or disable the screen

// Battery monitor
#define ENABLE_BATTERY_MONITOR
#define BATTERY_TYPE_SAMSUNG_Q30 // Used battery type currently supported choices: BATTERY_TYPE_SAMSUNG_Q30
#define BM_ADC_ID 0
#define USE_FAST_CHARGING 1 // 0 for slow (50 mA) charging, 1 for fast (100 mA) charging
#define ADC_SAMPLES_TOTAL 10
#define ADC_SAMPLE_INTERVAL_US 500
#define BM_ADC_RESOLUTION 12
#define VOLTAGE_DIVIDER_R1 1031 // 1Mohm
#define VOLTAGE_DIVIDER_R2 510  // 510kohm

// Value scaling and min max for warning
#define TEMPERATURE_SCALE 100
#define TEMPERATURE_MIN -100
#define TEMPERATURE_MAX 100

#define HUMIDITY_SCALE 100
#define HUMIDITY_MIN 0
#define HUMIDITY_MAX 100

#define PRESSURE_SCALE 0.1
#define PRESSURE_MIN 0
#define PRESSURE_MAX 200000

#define CO2_CONCENTRATION_SCALE 10
#define CO2_CONCENTRATION_MIN 0
#define CO2_CONCENTRATION_MAX 10000

#define VOC_INDEX_SCALE 10
#define VOC_INDEX_MIN 0
#define VOC_INDEX_MAX 500

#endif