# Firmware binaries
Pre-compiled firmware binaries for selected sensor configurations

## Files

### aqs_001.uf2

**Board**:
- XIAO BLE Sense (nrf52840)

**Enabled sensors:**
- SHT41, temperature and humidity sensor
- SCD41, carbon dioxide sensor
- SGP40, VOC sensor
- BMP390, pressure sensor

**Configurations:**
- Sensor name: `air_quality_sensor_001`
- Measurement interval: 5 minutes

### aqs_002.uf2

**Board**:
- XIAO BLE (nrf52840)

**Enabled sensors:**
- SHT41, temperature and humidity sensor
- SCD41, carbon dioxide sensor
- BME680, IAQ index and pressure sensor

**Configurations:**
- Sensor name: `air_quality_sensor_002`
- Measurement interval: 5 minutes

## Installation Instructions

1. **Enter Bootloader Mode:**
   - Doubleclick the reset button on the XIAO BLE board
   - Device will mount as a USB mass storage device

2. **Flash Firmware:**
   - Copy the desired `.uf2` file to the mounted device
   - Device will automatically reboot with new firmware

3. **Verify Installation:**
   - Check serial output for sensor initialization and output values