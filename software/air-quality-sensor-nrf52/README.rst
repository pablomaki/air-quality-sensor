Air Quality Monitor NRF52840
###########

Overview
********

Software designed to run on the Seeed XIAO NRF52840 board utilizing the Nordic Semiconductor nRF52840 microcontroller.

Configuration options can be found in [config.h](./include/config.h).

Building and Running
********************

To build the project, you will need to have the following tools installed:
- [Visual Studio Code](https://code.visualstudio.com/download)
- [nRF Connect for Visual Studio Code](https://docs.nordicsemi.com/bundle/nrf-connect-vscode/page/get_started/install.html)

The project was built on nRF connect SDK and Toolchain version 2.7. The only modification that needed to be done, was changing SGP40 drivers defined SGP40_TEST_WAIT_MS from 250 to 320.


TODO
********************
* Unify error handling
* Fix possible bug in event handler giving both warning and ok
* Add BLE events to event handler
* Look in to reducing the amount of config variables in configs.h
* Implement EPD output