# Air Quality Sensor firmware — proposed architecture

Target: nRF52840 (Cortex-M4F, 1 MB flash / 256 KB RAM), nRF Connect SDK v3.2.1,
Matter over Thread, USB powered today, battery-capable later.

---

## 1. Requirements that shape the design

| # | Requirement | Architectural consequence |
|---|---|---|
| R1 | Many possible sensors, selected per build/board | Sensors must be **plug-ins**: adding one touches only its own files + a devicetree node. No `switch` statements in core code. |
| R2 | Report every X seconds, X configurable | Sampling cadence and *reporting* cadence are **separate clocks**. X lives in a runtime-settable, persisted setting — not only in Kconfig. |
| R3 | Matter over Thread | The CHIP stack owns its own thread and its own locking. Everything C++ stays behind one wall. |
| R4 | USB now, battery maybe | No busy-waiting, no polling loops, all work is timer/event driven, sensor power gating modelled from day one even if unused. |
| R5 | Maintainable | One responsibility per module, one direction of dependency, logic testable on `native_sim` without hardware. |

## 2. Design principles

1. **Devicetree says what exists; Kconfig says what to do with it; Settings says what the user changed.** Three tiers, never duplicated.
2. **Sensors are Zephyr `sensor` devices.** If a sensor has no upstream driver, write an out-of-tree driver implementing `sensor_driver_api` in `drivers/sensor/` — do *not* put I²C transactions in application code. The application then knows exactly one sensor API forever.
3. **One direction of dependency.** `platform → sensing → aq → matter`. Nothing below reaches up. The Matter layer is the only consumer of the aggregated state; the sensing layer has never heard of Matter.
4. **C++ is quarantined.** Only `src/matter/` is C++. Everything else is C with `extern "C"` headers. This keeps the reusable parts testable and the CHIP include-storm contained.
5. **Blocking I/O never runs on the system workqueue.** Sensor reads take tens of milliseconds (and seconds of warm-up); they get their own workqueue with its own stack.
6. **A missing value is `null`, not `0`.** Every value carries a status; unavailable Matter attributes are written as NULL.

## 3. Layered view

```
                +-----------------------------------------------+
                |  Matter / CHIP (C++)                          |
   endpoints,   |  matter_app  matter_reporting  matter_ota     |
   ZAP, OTA     |  matter_endpoints  matter_identify            |
                +------------------------^----------------------+
                                         | aqs_state snapshot (read-only)
                +------------------------+----------------------+
   domain       |  aq: aggregation, filtering, indices          |
   logic (C)    |  aq_state  aq_index  gas_index                |
                +------------------------^----------------------+
                                         | zbus: CHAN_SENSOR_SAMPLE
                +------------------------+----------------------+
   acquisition  |  sensing: sensor_hub (scheduler + registry)   |
   (C)          |  per-sensor glue modules (warm-up, quirks)    |
                +------------------------^----------------------+
                                         | Zephyr sensor API
                +------------------------+----------------------+
   drivers      |  upstream drivers  |  out-of-tree drivers/    |
                +------------------------^----------------------+
                                         | devicetree
                +------------------------+----------------------+
   platform     |  settings  watchdog  leds  buttons  power     |
                |  fault_handler  shell/diag                    |
                +-----------------------------------------------+
```

## 4. File layout

```
firmware/
├── CMakeLists.txt                 # thin: adds subdirs, iterable sections, ZAP glue
├── Kconfig                        # menu "Air Quality Sensor", sources module Kconfigs
├── prj.conf                       # baseline that every build needs
├── VERSION                        # single source of version for MCUboot + Matter
├── sysbuild.conf                  # MCUboot on/off
├── west.yml (or rely on parent)   # pin NCS v3.2.1
│
├── configs/                       # composable conf fragments, selected on the CLI
│   ├── matter-thread.conf
│   ├── dfu-ota.conf               # Matter OTA requestor + MCUboot
│   ├── dfu-smp.conf               # BLE SMP fallback if OTA doesn't fit
│   ├── debug.conf                 # shell, RTT, asserts, verbose logs
│   ├── release.conf               # log minimal, no shell, watchdog on
│   └── lowpower.conf              # future: MTD/SED, PM_DEVICE_RUNTIME
│
├── boards/                        # overlays: which sensors are populated
│   ├── nrf52840dk_nrf52840.overlay
│   └── aqs_v1_nrf52840.overlay
├── boards/arm/aqs_v1/             # the custom board, once the PCB is real
│
├── dts/bindings/sensor/           # bindings for out-of-tree drivers
├── drivers/sensor/<part>/         # out-of-tree Zephyr sensor drivers
│
├── include/aqs/                   # the only headers other modules may include
│   ├── quantity.h                 # enum aqs_quantity + units + names
│   ├── value.h                    # struct aqs_value (value + status + timestamp)
│   ├── state.h                    # aggregated snapshot API
│   ├── sensor.h                   # the sensor plug-in contract
│   ├── config.h                   # runtime config get/set (report interval, offsets)
│   └── events.h                   # zbus channel declarations
│
├── src/
│   ├── main.c                     # ~40 lines: init platform, sensing, matter, done
│   ├── platform/
│   │   ├── settings_store.c       # NVS-backed runtime config, settings handlers
│   │   ├── watchdog.c             # WDT + per-thread heartbeat supervisor
│   │   ├── led_indication.c       # status pattern from app state
│   │   ├── button_handler.c       # factory reset, commissioning window
│   │   ├── power.c               # PM policy, sensor rail gating, battery hook
│   │   └── fault_handler.c        # fatal error capture, retained reboot reason
│   ├── sensing/
│   │   ├── sensor_hub.c           # registry iteration + per-sensor state machine
│   │   ├── sensor_wq.c            # dedicated workqueue definition
│   │   └── glue/
│   │       ├── scd4x.c            # CO2: 5 s cadence, ASC, pressure/temp offset
│   │       ├── sen5x.c            # PM/VOC/NOx: fan warm-up, duty cycling
│   │       ├── bme68x.c           # T/RH/P (+ optional BSEC)
│   │       ├── sht4x.c
│   │       ├── sgp4x.c            # raw ticks -> gas index algorithm
│   │       └── sim.c              # synthetic sensor: full pipeline, no hardware
│   ├── aq/
│   │   ├── aq_state.c             # owns the snapshot, mutex, staleness expiry
│   │   ├── aq_filter.c            # per-quantity median/EMA, spike rejection
│   │   ├── aq_index.c             # composite air quality -> Matter AirQualityEnum
│   │   └── gas_index.c            # wrapper around Sensirion gas index algorithm
│   ├── matter/
│   │   ├── matter_app.cpp         # stack init, event handler, commissioning
│   │   ├── matter_endpoints.cpp   # endpoint enable/disable from sensor set
│   │   ├── matter_reporting.cpp   # snapshot -> attributes, on the CHIP thread
│   │   ├── matter_identify.cpp
│   │   ├── matter_ota.cpp
│   │   └── default_zap/           # existing generated data model
│   └── diag/
│       └── shell_cmds.c           # `aqs sensors`, `aqs read`, `aqs interval 30`
│
├── tests/                         # ztest, run with twister on native_sim
│   ├── aq_index/
│   ├── aq_filter/
│   ├── aq_state/
│   └── sensor_hub/                # against a fake sensor device
└── sketch/                        # this design material
```

## 5. Module catalogue

| Module | Owns | Never does |
|---|---|---|
| `platform/settings_store` | Persisted runtime config, change notification | Know what the settings mean |
| `platform/watchdog` | Hardware WDT, heartbeat registration | Decide what a fault means |
| `platform/power` | PM policy, sensor rail gating, battery voltage | Touch sensors directly (goes via regulator DT) |
| `sensing/sensor_hub` | Iterate the sensor registry, run each sensor's state machine, publish samples | Know any specific part number |
| `sensing/glue/*` | One part's quirks: warm-up, calibration, channel→quantity mapping | Know about Matter, zbus, or other sensors |
| `aq/aq_state` | The single aggregated snapshot + validity/staleness | Format or transmit anything |
| `aq/aq_filter` | Smoothing, plausibility limits | Persist anything |
| `aq/aq_index` | Composite index from available quantities | Know cluster IDs |
| `matter/matter_reporting` | Snapshot → attribute writes, cadence, null handling | Compute values |
| `matter/matter_endpoints` | Which endpoints/clusters are live for this hardware set | Sample anything |
| `diag/shell_cmds` | Human diagnostics | Exist in release builds |

## 6. Core data model

```c
/* include/aqs/quantity.h */
enum aqs_quantity {
    AQS_Q_TEMPERATURE,      /* degC      */
    AQS_Q_HUMIDITY,         /* %RH       */
    AQS_Q_PRESSURE,         /* hPa       */
    AQS_Q_CO2,              /* ppm       */
    AQS_Q_TVOC,             /* ppb       */
    AQS_Q_VOC_INDEX,        /* 1..500    */
    AQS_Q_NOX_INDEX,        /* 1..500    */
    AQS_Q_PM1_0,            /* ug/m3     */
    AQS_Q_PM2_5,
    AQS_Q_PM4_0,
    AQS_Q_PM10,
    AQS_Q_COUNT
};

/* include/aqs/value.h */
enum aqs_status { AQS_ABSENT, AQS_WARMING_UP, AQS_VALID, AQS_STALE, AQS_ERROR };

struct aqs_value {
    float          value;       /* canonical unit per quantity, above */
    enum aqs_status status;
    int64_t        timestamp;   /* k_uptime_get() of acquisition */
    uint8_t        source;      /* sensor index, for provenance/debug */
};
```

`float` is deliberate: the M4F has an FPU, the Matter concentration clusters carry
`single`, and fixed-point scaling per quantity is the kind of detail that produces
unit bugs. Floats stay out of ISRs and out of log hot paths.

The aggregated state is one array plus a mutex, accessed only by copy-out:

```c
struct aqs_state { struct aqs_value q[AQS_Q_COUNT]; uint8_t air_quality_index; };
void aqs_state_snapshot(struct aqs_state *out);   /* short mutex hold, memcpy */
```

Consumers never hold a pointer into the live state, so no consumer can stall a
producer, and the Matter layer can never read a half-updated set.

## 7. The sensor plug-in contract (R1)

A sensor glue module declares itself into an iterable linker section; `sensor_hub`
iterates and knows nothing about it. Presence is driven by devicetree so a board
overlay is the single source of truth:

```c
/* src/sensing/glue/scd4x.c */
#define DT_DRV_COMPAT sensirion_scd4x

#if DT_HAS_DRV_INST(0)
static const struct aqs_channel_map scd4x_maps[] = {
    { SENSOR_CHAN_CO2,         AQS_Q_CO2 },
    { SENSOR_CHAN_AMBIENT_TEMP, AQS_Q_TEMPERATURE },
    { SENSOR_CHAN_HUMIDITY,    AQS_Q_HUMIDITY },
};

AQS_SENSOR_DEFINE(scd4x,
    .dev          = DEVICE_DT_GET(DT_DRV_INST(0)),
    .maps         = scd4x_maps,
    .map_count    = ARRAY_SIZE(scd4x_maps),
    .min_period_ms = 5000,      /* device physics */
    .warmup_ms     = 0,
    .ops           = &scd4x_ops /* optional: post-init calibration, suspend */
);
#endif
```

**Adding a sensor is four mechanical steps** — driver (or reuse upstream), DT node in
the board overlay, one glue file, one `Kconfig` symbol + `zephyr_library_sources_ifdef`
line. Core files are untouched. That is the property worth protecting in review.

`ops` is a small optional vtable for the ~20 % of sensors that need more than
fetch/get: `init()`, `start_measurement()`, `stop_measurement()`, `apply_config()`
(altitude, temperature offset), `self_test()`.

## 8. RTOS usage

### 8.1 Execution contexts

| Context | Kind | Prio | Stack | Why |
|---|---|---|---|---|
| CHIP event loop | thread (NCS) | NCS default | `CONFIG_CHIP_TASK_STACK_SIZE` | Owns the data model; only place attributes are written |
| OpenThread | thread (NCS) | NCS default | NCS | Radio/stack |
| `aqs_sensor_wq` | dedicated workqueue | preemptible, ~10 | 2–3 KB | Blocking I²C, warm-up delays; must never sit on the system WQ |
| `aqs_telemetry` | thread (zbus subscriber) | ~7 | 1.5–2 KB | Consumes samples, filters, updates state, drives the report cadence |
| system workqueue | Zephyr | default | default | LEDs, buttons, settings commits — short, non-blocking work only |
| logging | Zephyr (deferred) | low | small | `CONFIG_LOG_MODE_DEFERRED` so logging never blocks a sensor read |
| shell | Zephyr | low | 2 KB | Debug builds only |

Three application threads total. On a part where Matter + Thread already claim most
of the 256 KB, thread count is a RAM budget line item — resist the temptation to give
every sensor a thread.

### 8.2 Sampling: one delayable work item per sensor

Each registered sensor gets a `struct k_work_delayable` submitted to `aqs_sensor_wq`,
plus a small state machine:

```
UNINIT ──probe ok──> IDLE ──due──> WARMUP ──warmup_ms──> FETCH ──ok──> publish ──> IDLE
   │                                                        │
   └──probe fail──> ERROR <──── N consecutive failures ──────┘
                      │
                      └── exponential backoff (1s → 60s cap), bus/power recovery,
                          quantities marked AQS_ERROR after grace period
```

Why work items rather than a thread per sensor, or one thread polling all sensors:

- Heterogeneous cadences (SCD4x 5 s, SEN5x 1 s, BME680 on demand) fall out naturally
  from per-sensor `k_work_reschedule`, with no lowest-common-denominator tick.
- One stack for all sensors.
- A slow or wedged sensor delays only its own next sample; it cannot stall the report
  cadence, because reporting reads the snapshot, not the sensors.
- Warm-up and duty cycling are just extra states — the same mechanism that will power
  gate a fan on battery.

Rescheduling is computed from an absolute base (`next += period`), not
`k_work_reschedule(period)` from completion time, so cadence does not drift by the
read duration.

### 8.3 Decoupling: zbus

```c
ZBUS_CHAN_DEFINE(chan_sensor_sample, struct aqs_sample_msg, ...);  /* hub -> telemetry */
ZBUS_CHAN_DEFINE(chan_aq_state,      struct aqs_state,      ...);  /* telemetry -> N   */
ZBUS_CHAN_DEFINE(chan_config,        struct aqs_config,     ...);  /* settings -> N    */
ZBUS_CHAN_DEFINE(chan_net_status,    struct aqs_net_msg,    ...);  /* matter -> LEDs   */
```

zbus buys the thing that matters for maintainability: **new consumers cost nothing**.
A local display, a BLE debug service, a threshold alarm, an SD logger — each is a new
observer, with no edit to the producer. Discipline: listeners run in the *publisher's*
context, so only trivial listeners (LED state, log) may be listeners; anything that
blocks (Matter, filtering) must be a subscriber with its own thread.

If you would rather not take the zbus dependency, the fallback is a single
`k_msgq` from hub to telemetry and a direct call into `aqs_state`. Same shape, less
extensibility; the interfaces above do not change.

### 8.4 The two clocks (R2)

- **Sample interval** — per sensor, bounded below by device physics.
- **Report interval `X`** — one `k_work_delayable` on the telemetry thread:
  snapshot → hand to Matter. Default from
  `CONFIG_AQS_REPORT_INTERVAL_S` (60), overridden by the persisted setting
  `aqs/report_interval`, changeable at runtime via shell today.

Important Matter nuance to document in the code: the app controls how fresh the
*attributes* are; when a controller actually receives data is governed by its own
**subscription** min/max intervals. So `X` is "how often the data model is refreshed",
and the app additionally calls the attribute-change reporting path so a subscribed
controller sees a change promptly. Optional `CONFIG_AQS_REPORT_ON_CHANGE` with
per-quantity deltas gives event-driven publishing between ticks; combined with a
maximum silence interval it is also the power-friendly mode for a battery build.

### 8.5 Crossing into CHIP

`matter_reporting` is the only place that touches the data model, and it does so
from the CHIP context:

```cpp
/* called from telemetry thread */
void aqs_matter_publish(const struct aqs_state *s) {
    auto *copy = new aqs_state(*s);                     /* or a small static queue */
    chip::DeviceLayer::PlatformMgr().ScheduleWork(WriteAll, reinterpret_cast<intptr_t>(copy));
}
```

Never `LockChipStack()` from the sensor path — `ScheduleWork` avoids priority
inversion against the radio stack entirely.

### 8.6 Synchronization inventory

Deliberately small: one mutex inside `aq_state`, zbus channel locks, `ScheduleWork`
for the CHIP boundary, `k_sem` only in driver code. No shared globals, no spinlocks in
application code, no dynamic allocation after init.

## 9. Configuration (three tiers)

| Tier | Answers | Where | Example |
|---|---|---|---|
| Devicetree | "What hardware is on this board?" | `boards/*.overlay`, board files | I²C address, sensor power rail, interrupt GPIO |
| Kconfig | "How does this build behave?" | `Kconfig`, `configs/*.conf` | `AQS_SENSOR_SCD4X`, `AQS_REPORT_INTERVAL_S`, stack sizes, log levels |
| Settings/NVS | "What did the user change?" | `platform/settings_store` | report interval, temperature offset, altitude, sensor enable at runtime |

Kconfig symbols are *defaults* for settings-backed values, so a factory reset gives a
known-good configuration. `AQS_SENSOR_*` symbols `depends on DT_HAS_<compat>_ENABLED`,
so you cannot enable a sensor that is not on the board.

Remote configuration of `X`: the honest options are (a) shell/UART today,
(b) a manufacturer-specific Matter cluster attribute — flexible, but most commercial
ecosystems ignore custom clusters, (c) leave cadence to the controller's subscription
parameters, which is the spec-blessed answer. Recommendation: build (a) now, keep the
setting plumbing generic, add (b) only if a specific controller needs it.

## 10. Matter data model mapping

Proposed endpoint layout (IDs to be confirmed against the spec revision your ZAP
targets — verify before generating):

| EP | Device type | Clusters | Populated when |
|---|---|---|---|
| 0 | Root Node | Basic Info, General/Thread Diagnostics, OTA Requestor, (Power Source, later) | always |
| 1 | Air Quality Sensor (0x002C) | Air Quality (0x005B), CO₂ (0x040D), PM1 (0x042C), PM2.5 (0x042A), PM10 (0x042D), TVOC (0x042E), + NO₂/O₃/CH₂O as needed | any AQ sensor |
| 2 | Temperature Sensor (0x0302) | Temperature Measurement (0x0402) | T available |
| 3 | Humidity Sensor (0x0307) | Relative Humidity Measurement (0x0405) | RH available |
| 4 | Pressure Sensor (0x0305) | Pressure Measurement (0x0403) | P available |

T/RH/P get their own endpoints so ecosystems surface them as separate sensor tiles;
the concentration clusters share endpoint 1 because they are distinct cluster IDs
under one device type.

**Variant handling** — ZAP is static, hardware is not. Two workable strategies:

1. *Superset ZAP + runtime pruning*: generate every cluster, then disable unpopulated
   endpoints at boot with `emberAfEndpointEnableDisable()` driven by the registry.
   One binary covers all variants; cost is a little flash and coarse granularity
   (endpoint, not individual cluster).
2. *Per-variant ZAP*: exact data model, but N ZAP files to keep in sync.

Recommendation: (1) while the sensor set is still moving, with the endpoint→quantity
table derived from the same registry the hub iterates, so the data model can never
disagree with the hardware. Revisit if certification pushes back on unpopulated
clusters on endpoint 1.

**Null handling**: measured-value attributes are nullable. `matter_reporting` writes
NULL for any quantity whose status is not `AQS_VALID`/`AQS_STALE`, and applies each
cluster's own unit and scaling in exactly one place per cluster (temperature is
0.01 °C int16, humidity 0.01 % uint16, pressure kPa-ish int16, concentrations are
`single` float with a `MeasurementUnit`) — the unit conversion table belongs to the
Matter layer, never to a sensor glue file.

Also worth wiring on endpoint 1: `Air Quality` enum from `aq_index`, plus the
Peak/Average measurement attributes only if you actually want to maintain the
windows they imply.

## 11. Power (R4)

Not optimized now, but nothing done now should have to be undone later:

- **Already free**: every thread blocks on a queue or timer, so the idle thread runs
  `WFI` and the SoC drops to System ON idle between samples. No `k_busy_wait`, no
  polling loops, no 1 Hz reporting by default.
- **Structural**: sensor power gating expressed in devicetree (`vin-supply` →
  `regulator-fixed`), and every glue module implements `stop_measurement()`. The
  `warmup_ms` field in the descriptor exists precisely so the duty-cycled path is the
  same code path as the always-on one. A SEN5x fan at ~80 mA is the whole battery
  story; getting it right is a config change, not a rewrite.
- **Config-only later**: `CONFIG_PM_DEVICE` + `PM_DEVICE_RUNTIME` so drivers suspend
  themselves between samples; `lowpower.conf` flips OpenThread from FTD to
  MTD/SED with a poll period; longer `X` and report-on-change.
- **Stub now, fill later**: battery voltage via ADC (`voltage-divider`) and the
  Power Source cluster on EP0. Leave the module and the endpoint slot, empty.
- **Anti-goals**: do not add a tickless/PM hack, per-sensor threads, or logging in the
  sample path — all three are hard to remove later.

## 12. Reliability

- **Watchdog**: `platform/watchdog` owns the WDT channel; each long-lived thread
  registers and checks in. A supervisor work item feeds the dog only if every
  registered participant has checked in within its window.
- **Sensor faults**: bounded retries with exponential backoff, I²C bus recovery, then
  power-cycle the rail if one exists; quantities degrade `VALID → STALE → ERROR` and
  the corresponding attributes go NULL. One dead sensor must never affect the others
  or the Thread connection.
- **Staleness is explicit**: `aq_state` expires values older than
  `k * sample_period`, so a silently wedged sensor cannot report a stale value forever.
- **Fatal errors**: `CONFIG_RESET_ON_FATAL_ERROR`, reboot reason and boot counter in
  retained RAM/settings, surfaced over shell and optionally Matter diagnostics.

## 13. Build system, variants, DFU

- Root `CMakeLists.txt` stays thin: `add_subdirectory` per module,
  `zephyr_iterable_section(NAME aqs_sensor ...)` for the registry, ZAP glue include.
- Per-module `CMakeLists.txt` using `zephyr_library_sources_ifdef(CONFIG_AQS_SENSOR_X ...)`
  so an unselected sensor contributes zero bytes.
- Variants by fragment, not by branch:
  `west build -b aqs_v1_nrf52840 -- -DEXTRA_CONF_FILE="configs/release.conf;configs/dfu-ota.conf"`
- **Flash budget is the real risk on nRF52840**: MCUboot + Matter + OpenThread + OTA
  secondary slot is tight. Measure early, with `release.conf`. If OTA does not fit,
  `dfu-smp.conf` (BLE SMP) is the fallback, and the decision stays a build variant
  rather than an architectural change.
- `VERSION` feeds both MCUboot image version and Matter software version — one source.

## 14. Testing

| Level | Where | How |
|---|---|---|
| Pure logic — `aq_index`, `aq_filter`, unit conversion, staleness | `tests/*` | ztest on `native_sim`, no hardware, runs in CI in seconds |
| Sensor scheduling | `tests/sensor_hub` | fake `sensor_driver_api` device that returns scripted values and injectable errors; asserts backoff, cadence, status transitions |
| Full pipeline without sensors | `glue/sim.c` | `CONFIG_AQS_SENSOR_SIM=y` synthesizes plausible drifting values, so the whole path incl. Matter can be exercised on a bare DK. Worth building first, before any real sensor. |
| Integration | on target | shell `aqs read` / `aqs sensors`, `chip-tool` reads and subscriptions |

The reason the logic layer is C and Matter-free is exactly this table — anything that
needs the CHIP stack to test is effectively untested.

## 15. Suggested migration path from the current tree

Written without having read `src/` yet, so treat as a proposal to reconcile:

1. Land `include/aqs/{quantity,value,state}.h` and `aq/aq_state.c`. Nothing else
   changes yet.
2. Introduce `sensor_hub` + the `AQS_SENSOR_DEFINE` registry with `sim.c` as the only
   sensor. Prove sample → state → Matter attribute end to end.
3. Move existing sensor handling from `components/sensors.c` into one glue file per
   part, one at a time, deleting from the old file as you go.
4. Reduce the existing Matter mapping (`air_quality_mapper`, `matter_handler`) to
   `matter_reporting` + `matter_endpoints`, with the unit conversion table in one place.
5. Add `settings_store` and move the report interval from Kconfig-only to
   Kconfig-default + persisted setting; add the shell command.
6. Split `prj.conf` into `prj.conf` + `configs/*.conf`; add `release.conf` and measure
   the flash budget.
7. Add `watchdog` and per-sensor backoff last, once the happy path is stable.

Each step is independently shippable and leaves the firmware working.

## 16. Open questions for you

1. Which sensors are actually in play (SCD4x? SEN5x? BME680/688 with BSEC? SGP4x?)? BSEC
   in particular is a licensed blob with its own state persistence needs and would get
   its own glue module and a settings key.
2. nRF52840 specifically, or could a 53/54 be on the table? It changes the flash budget
   conversation and whether Matter OTA is comfortable.
3. Is remote (Matter-side) configuration of `X` a requirement, or is
   commissioning-time + subscription control enough?
4. FTD or MTD for the Thread role on the USB-powered version? FTD is friendlier to the
   mesh; MTD is the battery path, and it is cheap to keep both as fragments.
5. Does one binary need to cover several hardware variants, or is a build per product?
   That decides §10's superset-ZAP question.
