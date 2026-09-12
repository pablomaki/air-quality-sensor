# Current firmware vs. the proposed architecture

Review of the tree as of branch `feat-redesign` (commit `f01761f`), against
[`ARCHITECTURE.md`](ARCHITECTURE.md). ~1500 lines of application code.

## Verdict

The **shape** is closer to the plan than I expected: module boundaries are real, the
C/C++ wall is in the right place, the Matter endpoint split matches, and sensors are
already Zephyr `sensor` devices rather than hand-rolled I²C. What is missing is
everything that makes the structure *hold* as sensors are added: the sensor layer is a
hand-maintained `#ifdef` matrix with cross-sensor coupling through file-scope globals,
there is one global cadence for parts with very different physics, and error states are
encoded as in-band magic values that reach the Matter attributes as plausible readings.

Roughly: good decomposition, no isolation.

## Already aligned with the plan

| Plan item | Status |
|---|---|
| Sensors via Zephyr `sensor_driver_api`, no raw I²C in app | Done — all five parts go through `sensor_sample_fetch`/`channel_get`/`attr_set` |
| C++ quarantined to the Matter boundary | Done — only `matter_handler.cpp` + `air_quality_mapper.cpp`, C headers use `extern "C"` |
| Separate endpoints per sensor device type | Done — T=1, RH=2, P=3, AirQuality=4; matches the proposal exactly |
| Delegate `Instance` for AirQuality / concentration clusters | Done, and correctly — the comments show this was learned the hard way |
| Module layout (`components/`, `utils/`, `drivers/`) with headers under `include/` | Done, close enough to the proposed `platform/sensing/aq/matter` split |
| Timed work on a dedicated workqueue, not the system WQ | Done |
| Deadline-aware rescheduling | Partially — `calculate_task_delay()` compensates for work duration |
| PowerSource cluster on EP0, `vbatt` divider + ADC channel in DT | Present in the data model and devicetree already; no code behind either |
| `CONFIG_PM_DEVICE=y` | Set, but nothing calls `pm_device_action_run()` |

## Findings, worst first

### 1. Matter attributes are written without holding the CHIP stack lock

`update_cluster_states()` ([matter_handler.cpp:112](../src/components/matter_handler.cpp#L112))
calls `TemperatureMeasurement::Attributes::MeasuredValue::Set()`,
`sCarbonDioxideInstance.SetMeasuredValue()` and `sAirQualityInstance.UpdateAirQuality()`
directly from the `periodic_task_work_q` thread. Those touch the attribute store and
the reporting engine, which the CHIP thread is also using. No `LockChipStack()`, no
`ScheduleWork()`.

This is the failure mode that shows up as rare, unreproducible faults once
subscriptions are active — exactly when it is hardest to debug. Fix:

```cpp
chip::DeviceLayer::PlatformMgr().ScheduleWork(WriteAll, reinterpret_cast<intptr_t>(copy));
/* or, if you must stay synchronous: */
chip::DeviceLayer::PlatformMgr().LockChipStack(); /* ... */ UnlockChipStack();
```

`ScheduleWork` is preferable — it also removes any chance of the sensor path blocking
behind the radio stack.

### 2. One failed sensor at boot silently disables the device

`init_air_quality_monitor()` returns on the first sensor that is not ready, so
`main()` returns before `matter_dispatch_tasks()`
([air_quality_monitor.c:161](../src/air_quality_monitor.c#L161),
[main.c:22](../src/main.c#L22)). Matter and Thread were already started, so the node
still joins the network and answers — but nothing ever pumps `Nrf::DispatchNextTask()`
and no measurement is ever taken. A single loose I²C part produces a device that looks
healthy and reports nothing, forever.

Per the plan, sensor readiness should be per-sensor state, not a boot gate: log it,
mark that sensor's quantities unavailable, carry on.

### 3. Sensor combinations that do not compile

The `#ifdef` matrix has cross-dependencies that are not expressed:

- `read_sgp40_data()` uses `temperature` and `humidity` for compensation
  ([sensors.c:187](../src/components/sensors.c#L187)), but those are declared inside
  `#ifdef CONFIG_ENABLE_SHT4X` ([sensors.c:18](../src/components/sensors.c#L18)).
  **SGP40 without SHT4X does not build.**
- In `update_cluster_states()`, `status` is declared under
  `#if defined(SHT4X) || defined(SCD4X)` ([matter_handler.cpp:119](../src/components/matter_handler.cpp#L119))
  and used under `#if defined(BMP390) || defined(BME680)` ([matter_handler.cpp:139](../src/components/matter_handler.cpp#L139)).
  **A pressure-only or BME680-only build does not build.**
- `asc_initial_period` / `asc_standard_period`
  ([sensors.c:32](../src/components/sensors.c#L32)) divide by
  `CONFIG_ADVERTISEMENT_INTERVAL / CONFIG_MEASUREMENTS_PER_INTERVAL / 1000`. Any
  configuration where the per-sample interval is under 1000 ms is a **compile-time
  division by zero**.

Five independent sensor flags is a 32-way build matrix that nobody will test by hand.
This is the concrete argument for the registry approach: with self-registering
descriptors there is no combinatorial surface, and cross-sensor compensation becomes
explicit data (`AQS_Q_TEMPERATURE` from *whichever* sensor provides it) instead of a
file-scope variable that may or may not exist.

### 4. Error states are published as plausible measurements

`-1.0f` is the in-band error sentinel, and `get_mean()` returns `-1.0f` if *any*
sample in the window is negative ([variable_buffer.c:53](../src/utils/variable_buffer.c#L53)).
Consequences:

- **-1 °C is a real temperature.** One sub-zero reading poisons the whole window, and a
  genuine error is indistinguishable from a cold room.
- `-1.0f` flows straight through to Matter: temperature reports as `-100` (i.e. -1 °C),
  and CO₂ as `MakeNullable(-1.0f)` — nullable, but populated with -1 ppm rather than
  actually null.
- Every measured-value attribute in use here is nullable. Null is the correct signal.

### 5. Uninitialized heap can be published as a reading

`init_buffers()` mallocs but never zeroes, and `get_mean()` always averages the full
window regardless of how many samples were written
([variable_buffer.c:13](../src/utils/variable_buffer.c#L13)). Meanwhile
`update_cluster_states()` publishes temperature and humidity whenever
`CONFIG_ENABLE_SHT4X || CONFIG_ENABLE_SCD4X` — but `read_scd4x_data()` only *logs* its
temperature and humidity; it never calls `set_value()`. So in an **SCD4X-only build the
device publishes uninitialized heap as temperature and humidity**.

Secondary: the window size is a compile-time constant, so `malloc` buys nothing over a
static array — and no malloc arena is configured in `prj.conf`, so the allocation's
success depends on libc defaults.

### 6. Pressure scaling looks 10× off

Zephyr's `SENSOR_CHAN_PRESS` is kPa; Matter's `PressureMeasurement::MeasuredValue` is
kPa × 10 (0.1 kPa resolution). The code does `pressure * 100`
([matter_handler.cpp:140](../src/components/matter_handler.cpp#L140)), which reports
~10132 for standard atmosphere instead of ~1013. The log line in `sensors.c` formats
the same value as if it were Pa (`val1 / 100` → "1.x hPa"), which suggests the unit was
genuinely ambiguous at the time. Worth confirming against the BMP390 driver and the
spec, then putting the conversion in exactly one place.

Also minor: `RelativeHumidityMeasurement::MeasuredValue` is `uint16`, cast here to
`int16_t`.

### 7. The ICD configuration contradicts the use case and the data model

`prj.conf` selects `OPENTHREAD_MTD` + `CHIP_ENABLE_ICD_SUPPORT` + `CHIP_ICD_LIT_SUPPORT`
with a 300 s slow poll and a 900 s idle mode — a long-idle, battery-style sleepy device.
The application meanwhile refreshes attributes every 5 s
(`ADVERTISEMENT_INTERVAL=5000`). Two problems:

- A 5 s refresh behind a 5-minute poll interval means most updates never leave the
  device promptly. The cadence you configure and the cadence a controller observes are
  unrelated right now.
- **The ZAP has no `IcdManagement` cluster on endpoint 0**, which a LIT ICD is required
  to expose. Enabled in Kconfig, absent from the data model.

For a USB-powered device, FTD (or at minimum a non-LIT MED with a short poll) is the
straightforward choice, with the ICD/LIT setup moved to the future `lowpower.conf`
fragment. That is precisely the split the plan proposes; here both configurations are
half-applied at once.

### 8. One global cadence for parts with different physics

`ADVERTISEMENT_INTERVAL / MEASUREMENTS_PER_INTERVAL` is *the* sampling interval for
everything. But the SCD4x has a 5 s measurement period, and Sensirion's gas index
algorithm is specified around ~1 s sampling with only a narrow supported range —
`GasIndexAlgorithm_init_with_sampling_interval()` is handed
`ADVERTISEMENT_INTERVAL / MEASUREMENTS_PER_INTERVAL / 1000`
([sensors.c:69](../src/components/sensors.c#L69)), which at the Kconfig default of
300000/5 is **60 s**. At that interval the VOC index output is not meaningful. Worth
checking against the algorithm documentation, but the structural point stands: sampling
cadence belongs to the sensor, reporting cadence belongs to the application.

### 9. Cross-sensor coupling is implicit and order-dependent

SGP40 compensation needs SHT4X's values; SCD4x pressure compensation needs BMP390's.
Both work only because of the order of the `#ifdef` blocks in `read_sensors()` and
shared file-scope `sensor_value` variables. Nothing declares the dependency, and
reordering the blocks silently degrades accuracy rather than failing. A shared state
keyed by *quantity* rather than by *sensor* makes this explicit and order-independent.

### 10. "X seconds" is build-time only

`CONFIG_ADVERTISEMENT_INTERVAL` is a Kconfig integer baked into the image — and used at
compile time in static initializers, so it cannot easily become runtime-settable
without unpicking those. `CONFIG_SETTINGS`/`SETTINGS_RUNTIME` are already enabled in
`prj.conf` but unused by the application. The requirement "configurable reporting
interval" is not met yet in any operational sense.

### 11. Configuration source of truth is split three ways

Every sensor has an app flag (`CONFIG_ENABLE_SHT4X`), a driver flag (`CONFIG_SHT4X`),
and a devicetree `status`, all maintained by hand:

- Right now the overlay marks sht4x/sgp40/scd41/bme680 `okay` and the drivers are built,
  while **all five `CONFIG_ENABLE_*` are `n`** — so drivers initialize at boot, consume
  RAM and bus time, and are never read.
- `CONFIG_ENABLE_BMP390` gates a driver whose actual symbol is `CONFIG_BMP388`.
- Nothing prevents `CONFIG_ENABLE_SHT4X=y` with `CONFIG_SHT4X=n`.
- `CONFIG_BME680=n` appears twice, with different explanatory comments.

The plan's rule — devicetree decides existence, `depends on DT_HAS_<compat>_ENABLED`,
`select` the driver symbol — collapses this to one knob per sensor.

### 12. No `IcdManagement`, no `ThreadNetworkDiagnostics`

Worth adding the latter regardless; it is the cheapest field-debugging tool for a Thread
node, and the ZAP is where it has to happen.

### 13. Robustness gaps

- No watchdog (`CONFIG_WATCHDOG` absent), no `CONFIG_RESET_ON_FATAL_ERROR`.
- No retry, backoff, or bus recovery: a sensor that starts failing simply fails on every
  cycle, at full rate, forever.
- No timestamps or staleness anywhere, so a wedged sensor's last good value is
  republished indefinitely as current.
- `uart0` is `status = "disabled"` in the overlay while `CONFIG_UART_CONSOLE=y` and
  `CONFIG_LOG=y` — worth confirming the log output actually goes somewhere, because all
  the diagnostics above depend on it.

### 14. Build and hygiene

- `FILE(GLOB_RECURSE ...)` over `src/**` ([CMakeLists.txt:14](../CMakeLists.txt#L14))
  means anything dropped into the tree is compiled, and new files need a manual CMake
  re-run. It currently sweeps in both `air_quality_mapper.c` and
  `air_quality_mapper.cpp`, plus the `zap-generated/*.cpp` that
  `ncs_configure_data_model()` also adds. Explicit `zephyr_library_sources_ifdef()` per
  module is both safer and how per-sensor conditional compilation should be expressed.
- `include_directories()` is global; prefer `target_include_directories(app PRIVATE …)`.
- `include(zap_helpers.cmake)` appears twice (lines 30 and 32).
- Dead code: `warm_up_sgp40()` (static, never called — should be warning), all of
  `air_quality_mapper.c`, `get_latest()`, `free_buffers()`, `CONFIG_SENSOR_WARMUP_TIME_MS`.
  Note that the *warm-up* concept was clearly intended and then dropped; in the plan it
  is a first-class field on the sensor descriptor.
- `src/matter/default_zap/aqs.zap~` (editor backup) and `bin/*.uf2` are in the tree.
- `read_sensors()` is declared `int read_sensors();` — unprototyped in C.
- `int success = true;` in `read_sensors()`.
- README still describes BLE data transfer, NCS 2.9, and an `include/config.h` that does
  not exist.
- `NCS_SAMPLE_MATTER_LEDS=y` + `DK_LIBRARY=y` gives Nordic's board module ownership of
  the same LED GPIOs as `led_controller.c`. Dormant only because
  `CONFIG_ENABLE_EVENT_LED=n`.

## Structural differences worth deciding on

| Dimension | Now | Plan |
|---|---|---|
| Sensor addition cost | edit `sensors.c` (init + read + dispatch), `matter_handler.cpp`, `variable_buffer.h` enum, Kconfig, prj.conf, overlay | one glue file + DT node + one Kconfig symbol |
| Build matrix | 2⁵ hand-tested combinations, several known-broken | combinations are data, not preprocessor branches |
| Cadence | one interval for all sensors and for reporting | per-sensor sampling, independent reporting interval |
| Value representation | bare `float`, `-1.0f` = error | value + status + timestamp + provenance |
| Cross-sensor compensation | order-dependent file-scope globals | shared state keyed by quantity |
| Matter thread safety | unlocked writes from a workqueue | `ScheduleWork` only |
| Unit conversion | inline at each call site | one table at the Matter boundary |
| New consumers (the e-paper display already in DT!) | must edit the periodic task | add a zbus observer |
| Testability | none possible without the CHIP stack | logic layer runs on `native_sim` |

Note the `ssd1680` e-paper display and the `vbatt` divider are already in the
devicetree with no code behind them. Both are exactly the "second consumer" and "future
power concern" the decoupled state layer is meant to accommodate.

## Suggested order of work

Ordered by risk removed per unit of effort, not by architectural purity:

1. **`ScheduleWork` for all Matter writes** (§1). Small, self-contained, removes a
   real concurrency hazard.
2. **Don't abort boot on sensor init failure** (§2). A few lines.
3. **Fix the SCD4X-only uninitialized-heap publish and the `-1.0f` sentinel** (§4, §5) —
   introduce `struct aqs_value { float; status; timestamp; }` and write NULL for
   anything not valid. This is the first real step of the redesign and it pays for
   itself immediately.
4. **Decide FTD vs. ICD** (§7) and make `prj.conf` internally consistent; add
   `IcdManagement` to the ZAP if LIT stays.
5. **Verify the pressure scaling** (§6).
6. **Introduce the sensor registry** (§3, §8, §9) and migrate parts one at a time,
   starting with the simulated sensor so the pipeline is testable with no hardware.
7. **Move the reporting interval into settings** (§10), which requires removing
   `CONFIG_ADVERTISEMENT_INTERVAL` from static initializers first.
8. **Collapse the three-way sensor configuration** to devicetree-driven (§11).
9. Watchdog, backoff, staleness (§13); CMake and hygiene cleanup (§14).

Steps 1–5 are bug fixes worth doing whether or not the redesign happens.
