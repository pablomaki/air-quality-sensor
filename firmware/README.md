# Air Quality Sensor Firmware for NRF52840

## Overview

This firmware is designed to run on the Seeed XIAO NRF52840 board, which utilizes the Nordic Semiconductor nRF52840 microcontroller. It board functions as an air quality sensor by interfacing with various sensors and providing Bluetooth Low Energy (BLE) communication for data transmission.

### Key Features

- **Sensor Support**: The firmware has support for the following sensors as of now:
  - **SHT40/SHT41** temperature and humidity sensor
  - **SGP40** VOC (Volatile Organic Compound) sensor
  - **SCD40/SCD41** Carbon Dioxide (CO2) sensor (includes temperature and humidity reading as well)
  - **BMP390** Pressure sensor
- **BLE Communication**: Wireless data transfer to mobile devices or cloud platforms.
- **Customizable Configuration**: Easily modify settings in [config.h](./include/config.h) to adapt to different use cases.

## Building and Running

### Prerequisites

To build the project, ensure you have the following tools installed:

- [Visual Studio Code](https://code.visualstudio.com/download)
- [nRF Connect for Visual Studio Code](https://docs.nordicsemi.com/bundle/nrf-connect-vscode/page/get_started/install.html)
- nRF Connect SDK and Toolchain version 2.9

### Build Instructions

1. Clone the repository and navigate to the `firmware/` directory.
2. Open the project in Visual Studio Code.
3. Configure the build environment using the nRF Connect extension (v2.9 SDK and toolkit).
4. Build the firmware by selecting the appropriate build target for the Seeed XIAO NRF52840 board.
5. Selecting "No sysbuild" might be necessary to avoid build issues.

## Additional Resources

- [Nordic Semiconductor Documentation](https://infocenter.nordicsemi.com/)
- [Seeed XIAO NRF52840 Documentation](https://wiki.seeedstudio.com/XIAO_nRF52840/)