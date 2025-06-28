#ifndef CONFIG_H
#define CONFIG_H

// Measurement and advertising intervals and timing
#define ADVERTISEMENT_INTERVAL 300000 // milliseconds
#define BLE_TIMEOUT 10000             // milliseconds
#define MEASUREMENTS_PER_INTERVAL 5   // Number of measurements per interval
#define SENSOR_WARMUP_TIME_MS 5000    // Warmup time for sensors (required by at least SGP40 for more consistemt readings.)

// Bluetooth
// #define ENABLE_CONN_FILTER_LIST    // Use filter list for connections (not finished)
#define TEMPERATURE_SCALE 100      // 100 for 2 decimal places
#define HUMIDITY_SCALE 100         // 100 for 2 decimal places
#define PRESSURE_SCALE 0.1f        // 0.1 for -1 decimal place
#define CO2_CONCENTRATION_SCALE 10 // 10 for 1 decimal place
#define VOC_INDEX_SCALE 10         // 10 for 1 decimal place

// Peripherals
#define ENABLE_EVENT_LED       // Enable or disable event LED
#define ENABLE_SHT4X           // Enable or disable SHT4X sensor
#define ENABLE_SGP40           // Enable or disable SGP40 sensor
#define ENABLE_SCD4X           // Enable or disable SCD4X sensor
#define ENABLE_BMP390          // Enable or disable BMP390 sensor
#define ENABLE_EPD             // Enable or disable the screen
#define ENABLE_BATTERY_MONITOR // Enable or disable battery monitor

// SCD41 configs 
#define SCD4X_ALTITUDE 10          // Meters. Only used if pressure is not provided by other sensors.
#define SCD4X_TEMPERATURE_OFFSET 0 // Celcius (The reported temperature is measured value - offset). SCD41 temperature readings are not currently used.

// Battery monitor config
#define USE_FAST_CHARGING 1        // 0 for slow (50 mA) charging, 1 for fast (100 mA) charging
#define VOLTAGE_DIVIDER_R1 1031    // 1Mohm (needs to be calibrated manually for your board for accurate readings)
#define VOLTAGE_DIVIDER_R2 510     // 510kohm (needs to be calibrated manually for your board for accurate readings)

#endif