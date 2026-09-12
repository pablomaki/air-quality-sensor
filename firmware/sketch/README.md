# `sketch/` — proposed firmware architecture

Design-only material. Nothing here is built or linked; `sketch/` is excluded from the
build (no `CMakeLists.txt`, and it should be added to `.gitignore`-adjacent tooling
only if you don't want it committed — otherwise keep it as living design docs).

- [`REVIEW.md`](REVIEW.md) — how the firmware on `feat-redesign` actually stands against
  the plan below: what already aligns, 14 findings worst-first, and a suggested order of work.
- [`ARCHITECTURE.md`](ARCHITECTURE.md) — the plan: layers, modules, file layout,
  RTOS usage, Matter mapping, configuration tiers, power, testing, migration steps.

Rough interface sketches carrying the load-bearing decisions — illustrative, not
compilable:

| File | Decision it pins down |
|---|---|
| [`skeletons/aqs_sensor.h`](skeletons/aqs_sensor.h) | The sensor plug-in contract: self-registering descriptors, optional per-part ops, warm-up/duty-cycle fields |
| [`skeletons/aqs_state.h`](skeletons/aqs_state.h) | One aggregated snapshot, copy-out access, explicit per-value status |
| [`skeletons/sensor_hub.c`](skeletons/sensor_hub.c) | Per-sensor delayable work + state machine, drift-free cadence, backoff/recovery |
| [`skeletons/telemetry.c`](skeletons/telemetry.c) | The two clocks: sample consumption vs. the configurable report interval |
| [`skeletons/matter_reporting.cpp`](skeletons/matter_reporting.cpp) | The C++/CHIP wall: binding table, cluster unit scaling, NULL for absent values |
| [`skeletons/Kconfig.sketch`](skeletons/Kconfig.sketch) | Config shape: DT-gated sensor symbols, cadence defaults, RTOS resources in one auditable place |

`ARCHITECTURE.md` was written before reading the current sources; `REVIEW.md` is the
reconciliation with what is actually in `src/` today.
