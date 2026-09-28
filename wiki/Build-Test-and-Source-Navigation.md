# Build, Test and Source Navigation

## Source

`src/ESPressio_Event.hpp` is the umbrella header. `src/event/` contains focused family/deployment, planner, occurrence, runtime, bootstrap, Composition, retention and integration modules.

## Host validation

Run `python3 tests/run_tests.py`. The runner compiles/executes planner, Runtime and layout tests, checks negative/compile-fail contracts and verifies standalone public-header compilability.

## Platform/demo validation

Run `bash tests/run_demo_builds.sh` from the managed sibling-repository workspace. It rewrites only temporary PlatformIO project configuration so builds consume the coherent local source topology rather than stale GitHub dependency refs.

Three logical demos each provide Arduino IDE, PlatformIO Arduino and PlatformIO ESP-IDF projects: `local-broadcast`, `newest-only-retention`, `transport-boundary`.

The `examples/TemperatureMonitor` PlatformIO project is the complete example documented step-by-step in the root README.

## Validation authority

RPI400 is authoritative for executable validation. GitHub Actions is maintained as representative CI but its execution is not the project validation authority.
