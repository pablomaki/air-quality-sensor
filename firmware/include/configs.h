#ifndef CONFIG_H
#define CONFIG_H

// System
// #define ENABLE_SYSTEM_OFF // Enable to allow SYSTEM_OFF sleep, otherwise SYSTEM_ON IDLE loop is used

// Bluetooth
// #define ENABLE_CONN_FILTER_LIST // Use filter list for connections (not finished)

// Data config
#define FIRST_TASK_DELAY 1000                                                   // milliseconds
#define ADVERTISEMENT_INTERVAL 300000                                           // milliseconds
#define BLE_TIMEOUT 10000                                                       // milliseconds
#define MEASUREMENTS_PER_INTERVAL 5                                             // Number of measurements per interval
#define MEASUREMENT_INTERVAL ADVERTISEMENT_INTERVAL / MEASUREMENTS_PER_INTERVAL // milliseconds

// LED config
#define LED_BLINK_INTERVAL 50 // milliseconds
#define ENABLE_EVENT_LED      // Enable or disable event LED

// Sensors
#define ENABLE_SHT4X  // Enable or disable SHT4X sensor
#define ENABLE_SGP40  // Enable or disable SGP40 sensor
#define ENABLE_SCD4X  // Enable or disable SCD4X sensor
#define ENABLE_BMP390 // Enable or disable BMP390 sensor

// Environment
#define ALTITUDE 10          // meters
#define TEMPERATURE_OFFSET 0 // meters

// Screen
#define ENABLE_EPD // Enable or disable the screen

// Battery monitor
#define ENABLE_BATTERY_MONITOR     // Enable or disable battery monitor
#define BATTERY_TYPE_SAMSUNG_Q30   // Used battery type currently supported choices: BATTERY_TYPE_SAMSUNG_Q30
#define BM_ADC_ID 0                // ADC channel for battery monitor
#define USE_FAST_CHARGING 1        // 0 for slow (50 mA) charging, 1 for fast (100 mA) charging
#define ADC_SAMPLES_TOTAL 10       // Number of samples to take for averaging
#define ADC_SAMPLE_INTERVAL_US 500 // Microseconds
#define BM_ADC_RESOLUTION 12       // ADC resolution in bits
#define VOLTAGE_DIVIDER_R1 1031    // 1Mohm
#define VOLTAGE_DIVIDER_R2 510     // 510kohm

// Value scaling and min max for warning
#define TEMPERATURE_SCALE 100 // 100 for 2 decimal places
#define TEMPERATURE_MIN -100  // Celsius
#define TEMPERATURE_MAX 100   // Celsius

#define HUMIDITY_SCALE 100 // 100 for 2 decimal places
#define HUMIDITY_MIN 0     // Percentage
#define HUMIDITY_MAX 100   // Percentage

#define PRESSURE_SCALE 0.1f // 0.1 for -1 decimal place
#define PRESSURE_MIN 0      // Pascals
#define PRESSURE_MAX 200000 // Pascals

#define CO2_CONCENTRATION_SCALE 10  // 10 for 1 decimal place
#define CO2_CONCENTRATION_MIN 0     // ppm
#define CO2_CONCENTRATION_MAX 10000 // ppm

#define VOC_INDEX_SCALE 10 // 10 for 1 decimal place
#define VOC_INDEX_MIN 0    // index value (0-500)
#define VOC_INDEX_MAX 500  // index value (0-500)

#endif