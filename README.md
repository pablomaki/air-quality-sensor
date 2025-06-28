# Air Quality Sensor

This repository contains the firmware, hardware design, and scripts for an indoor air quality sensor.

## Features

- **Supported Sensors**: The system supports a variety of sensors for measuring air quality parameters such as:
  - **SHT40/SHT41** temperature and humidity sensor
  - **SGP40** VOC (Volatile Organic Compound) sensor
  - **SCD40/SCD41** Carbon Dioxide (CO2) sensor (includes temperature and humidity reading as well)
  - **BMP390** Pressure sensor
  - **Coming soon: BME680** Gas, humidity, pressure and temperature sensor
- **Electronic paper display**: Support for an Electronic paper display for displaying the measured values.
- **Wireless Communication**: Bluetooth Low Energy (BLE) for data transmission to mobile devices or cloud platforms.
- **Battery powered**: Battery life sensor and measurement/advertisement frequency dependent, but the default configurations aim for 1+ month up time.
- **Dockerized data receiver**: Simple script for BLE sensor reading and publishing the data to MQTT serverfor smart home integration ([MQTTThing](https://github.com/arachnetech/homebridge-mqttthing)).

## Structure

- **docker/**: Contains Docker configuration files for setting up the BLE sensor reader.
- **firmware/**: Contains the firmware source code and build files for the air quality sensor.
- **pcb/**: Contains the PCB design files for the air quality sensor.
- **scripts/**: Contains utility scripts for interacting with the air quality sensor used by the docker image for reading the sensor data output.

Refer to the README files in each subdirectory for more details.

## TODO
- **Firmware**
    - Fix a potential bug in the event handler that may trigger both warning and OK states simultaneously.
    - Migrate configurations to Kconfig for better management.
    - Try restricting BLE connections with a whitelist to improve security.
    - Add support for BME680 sensor.
- **PCB**
    - Add support for BME680 sensor.
- **Scripts**
    - Add logging the data to a database for grafana dashboard.
- **Other**
    - Design a custom enclosure for the sensor.

## License

This project is licensed under the terms specified in the `LICENSE` file.