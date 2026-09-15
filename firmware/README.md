# Air Quality Sensor Firmware for nRF52840

## Overview

This firmware runs on the Seeed XIAO nRF52840 (Sense) board. It reads a set of air
quality sensors over I2C and exposes the readings as a **Matter over Thread** device,
so the node can be commissioned into any Matter ecosystem.

### Key Features

- **Sensor Support**: The firmware supports the following sensors. Which of them are
  present is decided by the board overlay, see [Sensor selection](#sensor-selection):
  - **SHT40/SHT41** temperature and humidity sensor
  - **SGP40** VOC (Volatile Organic Compound) sensor
  - **SCD40/SCD41** Carbon Dioxide (CO2) sensor (also measures temperature and humidity,
    used as a fallback when no dedicated sensor is present)
  - **BMP390** pressure sensor
  - **BME680** gas/IAQ sensor (requires the `bsec` west manifest group, see
    [Prerequisites](#prerequisites))
- **Matter over Thread**: The node is commissioned over Bluetooth LE and then reports
  over Thread. Bluetooth LE is used only for commissioning.
- **Configurable sampling and reporting**: Independent intervals, see
  [Configuration](#configuration).
- **Per-board identity from one binary**: The onboarding values compiled into the
  image are only fallbacks, see [Device identity](#device-identity).

### Matter data model

Each quantity is reported on its own endpoint so that ecosystems surface them as
separate sensors:

| Endpoint | Device type | Reports |
| --- | --- | --- |
| 1 | Temperature Sensor | temperature |
| 2 | Humidity Sensor | relative humidity |
| 3 | Pressure Sensor | pressure |
| 4 | Air Quality Sensor | air quality, CO2 concentration |

A quantity with no valid reading is reported as `null` rather than as a placeholder
value, so an unavailable sensor is distinguishable from a working one.

### Device identity

Factory data is disabled, so serial number, unique ID and setup discriminator would
otherwise come from the image and be identical on every board flashed with it - two
units would then present themselves to a controller as the same device. Instead
`src/components/device_identity.cpp` derives all three from the SoC's factory
programmed device ID on every boot and persists them:

| Value | Derivation |
| --- | --- |
| Serial number | `AQS-` followed by the device ID in hex |
| Unique ID | Salted SHA-256 of the device ID, so it is not the serial number restated |
| Setup discriminator | 12 bits of that same hash |

The derivation is deterministic, so a board keeps its identity across a factory reset
and a controller recognises a recommissioned unit as the same device. The setup
*passcode* is still shared, because a per-board one needs a SPAKE2+ verifier generated
on the device, which belongs with real factory data.

Because the discriminator now differs per board, the pairing code does too. Read it
from the log at startup, where the stack prints the QR code payload and the manual
pairing code - see [Viewing the log output](#viewing-the-log-output).

## Building and Running

### Prerequisites

To build the project, ensure you have the following tools installed:

- [Visual Studio Code](https://code.visualstudio.com/download)
- [nRF Connect for Visual Studio Code](https://docs.nordicsemi.com/bundle/nrf-connect-vscode/page/get_started/install.html)
- nRF Connect SDK and Toolchain version 3.2.4 or newer

  > **Do not use v3.2.1.** Its `i2c_nrfx_twi` driver compares the transfer result
  > against `NRFX_SUCCESS` (0x0BAD0000) while the event handler stores 0 on success,
  > so every I2C transfer reports `-EIO` and no sensor is ever detected. The XIAO BLE
  > uses this driver for `i2c1`, where all the sensors live. Fixed in v3.2.4.

- The `bme68x` and `bsec` Bosch libraries (needed for `CONFIG_BME68X_IAQ`) belong to the
  `bsec` west manifest group, which is disabled by default. Enable it and fetch the
  sources before building:

  ```
  west config manifest.group-filter -- +bsec
  west update bme68x bsec
  ```

### Build Instructions

1. Clone the repository and navigate to the `firmware/` directory.
2. Open the project in Visual Studio Code.
3. Configure the build environment using the nRF Connect extension (v3.2.4 SDK and toolchain).
4. Build for the board target `xiao_ble/nrf52840/sense`.
5. Selecting "No sysbuild" might be necessary to avoid build issues.

Flash by copying the resulting `build/zephyr/zephyr.uf2` onto the mass storage device
that the Adafruit UF2 bootloader presents after double tapping reset. The firmware
deliberately does not use MCUboot, so that bootloader is left untouched.

### Viewing the log output

The console is a USB CDC ACM port provided by the board, not a UART. After flashing,
a second USB device enumerates; connect to it at any baud rate:

```
minicom -D /dev/ttyACM0
```

## Configuration

Settings live in `prj.conf` and are defined in `Kconfig`. There is no `config.h`.

### Sensor selection

The board overlay in `boards/` is the single source of truth for which parts exist on
the board. Adding a node there enables both the Zephyr driver and the application
support for it, because each `CONFIG_ENABLE_*` symbol depends on the corresponding
devicetree node and defaults to `y`. Use `prj.conf` only to turn a present sensor
*off*:

```
CONFIG_ENABLE_SHT4X=n   # part is on the board but not used in this build
```

Flash is very tight on this target, so check the memory report after a build and
enable sensors one at a time.

`CONFIG_BME68X_IAQ` is the exception: it is the licensed Bosch BSEC path, has no
devicetree default, and is mutually exclusive with the plain `CONFIG_BME680` driver.

### Timing

| Symbol | Default | Meaning |
| --- | --- | --- |
| `CONFIG_SAMPLE_INTERVAL_MS` | 5000 | How often every enabled sensor is read. Also the sampling interval given to the Sensirion gas index algorithm. |
| `CONFIG_REPORT_INTERVAL_MS` | 60000 | How often the Matter attributes are refreshed from the buffered samples. |

The two are independent. The number of samples averaged into each reported value is
derived from them - one reporting period's worth - so every sample taken contributes
to exactly one report.

Note that when a controller actually *receives* a value is governed by its own Matter
subscription intervals, not by `CONFIG_REPORT_INTERVAL_MS`, which only controls how
fresh the attributes are.

### Other settings

| Symbol | Meaning |
| --- | --- |
| `CONFIG_ENABLE_EVENT_LED` | Blink the RGB LED on application events |
| `CONFIG_SCD4X_ALTITUDE` | Altitude in metres, used when no pressure sensor supplies compensation |
| `CONFIG_SCD4X_TEMPERATURE_OFFSET_MILLI_C` | Compensates the SCD4x self heating, in milli-degrees Celsius (2.35 degrees = 2350) |

## Additional Resources

- [Nordic Semiconductor Documentation](https://docs.nordicsemi.com/)
- [Seeed XIAO nRF52840 Documentation](https://wiki.seeedstudio.com/XIAO_nRF52840/)
