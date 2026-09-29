# Building and Validation

EDP-Event is a C++20 header-oriented library. Host contract tests are executed with `python3 tests/run_tests.py` from a workspace containing the required sibling EDP repositories.

The current `event_v1` development branch depends on prerequisite Event-integration APIs which are also still carried on `EDP-Memory/event_v1` and `EDP-Threading/event_v1`. PlatformIO metadata and the checked-in demo/example projects therefore pin those two repositories to `event_v1`; Arduino IDE users must likewise clone/check out those two prerequisite branches alongside EDP-Event until the prerequisite APIs are promoted to their respective `main` branches. All other mandatory EDP dependencies use `main`.

`tests/run_demo_builds.sh` validates the PlatformIO Arduino and PlatformIO ESP-IDF demo surfaces against the coherent local sibling repository topology rather than accidentally cloning stale dependency revisions.

`tests/run_example_build.sh` independently builds the complete `examples/TemperatureMonitor` PlatformIO project against the same coherent sibling topology. The separate runner is intentional: the README example is a maintained consumer-facing project and must not silently rot merely because the equivalent local-broadcast demo still compiles.

Every logical demo additionally includes an Arduino IDE sketch under `demos/<name>/Arduino_IDE`.

The authoritative project validation appliance is RPI400. GitHub Actions remains a maintained representative CI definition but does not replace RPI400 executable validation.

For hardware validation, the local-broadcast/TemperatureMonitor path is suitable for the connected ESP32-WROOM-32 board because it requires no external peripheral beyond serial output.
