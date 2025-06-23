# Scripts

This directory contains utility scripts for interacting with the air quality sensor. Each advertised service is read by the script and published to a MQTT topic as is.

## Files

- **aqs_ble_client.py**: A Python script for communicating with the air quality sensor over BLE (Bluetooth Low Energy) and publishing the data to MQTT topics.

## Usage
To use the script, ensure you have Python installed along with the required libraries. You can run the script with the following command:

```bash
python aqs_ble_client.py
```

Make sure to configure the MQTT settings in the script before running it. The script will scan for BLE devices, connect to the air quality sensor, and publish the sensor data to the specified MQTT topics.

### Published Topics
The script publishes the following topics:
- `{SENSOR_NAME}/battery_level`: Battery level of the sensor (%).
- `{SENSOR_NAME}/pressure`: Pressure readings from the sensor (hPa).
- `{SENSOR_NAME}/temperature`: Temperature readings from the sensor (°C).
- `{SENSOR_NAME}/humidity`: Humidity readings from the sensor (%RH).
- `{SENSOR_NAME}/co2_concentration`: CO2 concentration readings from the sensor (ppm).
- `{SENSOR_NAME}/voc_index`: VOC index readings from the sensor (0-500).