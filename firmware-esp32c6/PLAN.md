# ESP32-C6 port plan

Port of the air quality sensor firmware from the XIAO nRF52840 (nRF Connect SDK)
to the Seeed XIAO ESP32-C6 (ESP-IDF + esp-matter), with Matter OTA updates.
The nRF52840 firmware in `../firmware` stays as it is.

## Decisions

| Topic | Decision |
|---|---|
| Location | `firmware-esp32c6/` next to `firmware/`; portable code copied for now |
| Stack | ESP-IDF v6.0.2 + pinned esp-matter release, built natively with `idf.py` |
| Transport | Matter over Thread (MTD, Wi-Fi disabled), HomePod as border router |
| OTA provider | python-matter-server on the RPi (`--ota-provider-dir`) |
| OTA images | ECDSA-signed apps without secure boot, so no eFuses are burned; key kept outside the repo |
| Rollback | `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`, image confirmed once Matter is up |
| Product IDs | Test VID 0xFFF1, one PID per variant: aqs_001..004 = 0x8001..0x8004 |
| Status LED | Onboard user LED (GPIO15, active low), colors mapped to blink patterns |
| Factory reset | Power-cycle gesture (as on nRF) plus long press of the BOOT button (GPIO9) |
| BSEC | Read from `BSEC_DIR`, default `~/Downloads/bsec2-6-1-0_generic_release_22102024`, `esp32_c2c3` build; fail early if missing |

## Findings behind the decisions

- **BSEC**: the `esp32_c2c3` library is `rv32imc`, ilp32 soft-float, which links
  into the C6 (`rv32imac`, no FPU). About 34 KB of code.
- **Flash**: esp-matter's `light_switch` over Thread on the C6 is about 1.65 MB
  against a 1.875 MB OTA slot. This application is expected around 1.7 MB.
  Measured in milestone 0 before anything else depends on it.
- **Commissioning data survives updates**: fabrics, keys and Thread credentials
  live in `nvs`. OTA writes only the inactive app slot and `otadata`, so the
  partition table below is fixed from the first build and never changed.
- **Carrier PCB** (from the KiCad netlist): sensors on I2C at D4/D5, which are
  GPIO22/GPIO23 on the C6. The WS2812B on D3 and the button on D7 are not
  populated.
- **RF switch**: GPIO3 low powers the antenna switch and GPIO14 low selects the
  onboard ceramic antenna. Both are set at boot, before the radio starts.

## Flash layout (4 MB, fixed)

```
esp_secure_cert  0x00D000  0x002000
nvs              0x010000  0x00C000   Matter fabrics, Thread dataset, app settings
nvs_keys         0x01C000  0x001000
otadata          0x01D000  0x002000
phy_init         0x01F000  0x001000
ota_0            0x020000  0x1E0000
ota_1            0x200000  0x1E0000
fctry            0x3E0000  0x006000   reserved for real factory data later
```

## Port map

| nRF source | ESP32-C6 |
|---|---|
| `utils/variable_buffer`, `utils/air_quality_mapper`, Sensirion gas index | copied unchanged |
| `air_quality_monitor.c` | Zephyr work queue and delayable work become a FreeRTOS task with `esp_timer`; same sample/report clocks and deadline compensation |
| `event_handler.c`, `led_controller.c` | same events; one LED, so red/green/yellow become distinct blink patterns |
| `sensors.c` | same logic and read order; Zephyr sensor API replaced by the ESP-IDF I2C master driver with Sensirion `embedded-i2c-*` drivers (SHT4x, SGP40, SCD4x) and Bosch `BMP3_SensorAPI` |
| Nordic `bme68x_iaq` driver | own component: Bosch `BME68x_SensorAPI` + BSEC, LP rate (3 s) on its own task, state saved to NVS hourly, same temperature offset |
| `matter_handler.cpp` | endpoints 1-4 and device types recreated with the esp-matter data model API; attribute updates via `esp_matter::attribute::update` under the stack lock |
| `device_identity.cpp` | serial number, unique ID and discriminator derived from the eFuse base MAC with the same hashing. If the default commissionable data provider cannot set the discriminator, a small custom provider supplies it |
| `factory_reset.cpp` | boot count in NVS, `esp_matter::factory_reset()`, plus BOOT long press |
| Kconfig + devicetree variant overlays | `Kconfig.projbuild` symbols; one `sdkconfig` defaults fragment per variant selecting sensors, product ID and name |

## Milestones

0. **Toolchain**: install ESP-IDF v6.0.2 and esp-matter under `~/esp`, next to
   the existing v5.4. Build esp-matter's light example for the C6 over Thread
   to confirm the setup and record its size.
1. **Skeleton**: project, partition table, RF switch, LED, identity, factory
   reset, Matter over Thread with all four endpoints reporting fixed values,
   OTA requestor, rollback and signing. Commission with Apple Home, then share
   to the RPi matter-server.
2. **OTA go/no-go**: version 1 to version 2 through the RPi provider over
   Thread, then a rollback test with an image that never confirms itself. If the
   sensor cannot reach the provider through the HomePod, stop and choose between
   Matter over Wi-Fi and BLE OTA before porting further.
3. **Sensors**: Sensirion parts and BMP390 first, then BME680 with BSEC.
4. **Variants and tooling**: `scripts/build_variants.sh` (one build at a time,
   pinned to four cores), `bin/` outputs (merged image for first flash, signed
   app, `.ota`), OTA JSON generator, `docker-compose.yml` OTA directory, README.

## Needs from you during the work

- XIAO ESP32-C6 boards on USB for flashing and log capture.
- Commissioning with Apple Home and sharing to the RPi (milestones 1 and 2).
- Running the update from the RPi in milestone 2, unless it is reachable from here.
