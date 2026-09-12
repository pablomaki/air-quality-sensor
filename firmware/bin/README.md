# Firmware binaries

Pre-compiled firmware images for the sensor configurations defined in
`firmware/variants/`. Each image is a Matter over Thread device, commissioned
over Bluetooth LE and reporting over Thread.

Rebuild them with:

```sh
cd firmware
./scripts/build_variants.sh            # all variants
./scripts/build_variants.sh aqs_001    # just one
```

The script writes each image here, named after its variant.

## Files

### aqs_001.uf2 - temperature, humidity and CO2

**Board:** XIAO BLE Sense (`xiao_ble/nrf52840/sense`)

**Fitted sensors:**
- SHT41, temperature and humidity
- SCD41, carbon dioxide

**Reports:** temperature, relative humidity, CO2 concentration, and an air
quality rating derived from the CO2 concentration.

**Configuration:** sampling every 10 s, Matter attributes refreshed every 60 s,
product name `Air Quality Sensor TH-CO2`.

### aqs_002.uf2 - VOC only

**Board:** XIAO BLE Sense (`xiao_ble/nrf52840/sense`)

**Fitted sensors:**
- SGP40, volatile organic compounds

**Reports:** an air quality rating derived from the VOC index.

**Configuration:** sampling every 5 s to suit the Sensirion gas index algorithm,
Matter attributes refreshed every 60 s, product name `Air Quality Sensor VOC`.

Without a temperature and humidity sensor the SGP40 cannot be compensated and
falls back to the driver's defaults, which costs some accuracy.

### aqs_003.uf2 - Bosch IAQ

**Board:** XIAO BLE Sense (`xiao_ble/nrf52840/sense`)

**Fitted sensors:**
- BME680, gas/IAQ and pressure

**Reports:** pressure, and an air quality rating derived from the IAQ index.

**Configuration:** product name `Air Quality Sensor IAQ`. Requires the licensed
Bosch BSEC library, so the `bsec` west manifest group has to be enabled before
building.

## Endpoints without a source

The Matter data model is the same in every image: endpoint 1 temperature,
2 humidity, 3 pressure, 4 air quality and CO2. A quantity that no fitted sensor
measures is reported as `null` rather than as a placeholder value.

Note that the BME680 measures temperature and humidity but the firmware does not
currently publish them, so those endpoints read `null` in `aqs_003`.

## Installation Instructions

1. **Enter Bootloader Mode:**
   - Doubleclick the reset button on the XIAO BLE board
   - Device will mount as a USB mass storage device

2. **Flash Firmware:**
   - Copy the desired `.uf2` file to the mounted device
   - Device will automatically reboot with new firmware

3. **Verify Installation:**
   - Connect to the USB CDC ACM console that enumerates after reboot
     (`/dev/ttyACM*`) and check the sensor initialization and measurement output
