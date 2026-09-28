# Building and Validation

EDP-Event is a C++20 header-oriented library. Host contract tests are executed with `python3 tests/run_tests.py` from a workspace containing the required sibling EDP repositories.

`tests/run_demo_builds.sh` validates the PlatformIO Arduino and PlatformIO ESP-IDF demo surfaces against the coherent local sibling repository topology rather than accidentally cloning stale dependency revisions.

Every logical demo additionally includes an Arduino IDE sketch under `demos/<name>/Arduino_IDE`.

The authoritative project validation appliance is RPI400. GitHub Actions remains a maintained representative CI definition but does not replace RPI400 executable validation.

For hardware validation, the local-broadcast/TemperatureMonitor path is suitable for the connected ESP32-WROOM-32 board because it requires no external peripheral beyond serial output.
