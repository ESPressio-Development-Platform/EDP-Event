# Build, Test and Source Navigation

## Source

`src/ESPressio_Event.hpp` is the umbrella header. `src/event/` contains focused family/deployment, planner, occurrence, runtime, bootstrap, Composition, retention, reservation and integration modules.

## Host validation

Run `python3 tests/run_tests.py`. The runner compiles/executes planner, Runtime, layout and concurrency tests, checks negative/compile-fail contracts and verifies standalone public-header compilability.

## Platform/demo validation

Run `bash tests/run_demo_builds.sh` from the managed sibling-repository workspace. It rewrites only temporary PIOArduino-compatible project configuration so builds consume the coherent local source topology rather than stale GitHub dependency refs.

Three logical demos each provide Arduino IDE, PIOArduino Arduino and PIOArduino ESP-IDF projects: `local-broadcast`, `newest-only-retention`, `transport-boundary`.

The `examples/TemperatureMonitor` PIOArduino-compatible project is the complete example documented step-by-step in the root README. Run `bash tests/run_example_build.sh` to validate that consumer-facing project independently against the same coherent sibling-source topology; the separate gate prevents the example from silently drifting merely because the equivalent local-broadcast demo remains buildable.

## Validation authority

RPI400 is authoritative for executable validation. GitHub Actions is maintained as representative CI but its execution is not the project validation authority.

Run `bash tests/run_arduino_ide_builds.sh` with an installed ESP32 Arduino core to compile all three `.ino` variants as a distinct supported consumption surface.
