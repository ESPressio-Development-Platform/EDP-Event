# Building and Validation

EDP-Event is a C++20 header-oriented library. C++20 is an absolute project baseline, not an optional build mode.

## Recommended ESP32 toolchain — PIOArduino

For VS Code ESP32 development, install and use **PIOArduino** rather than the legacy PlatformIO IDE extension. EDP public examples retain `platformio.ini` metadata because PIOArduino is PlatformIO-project compatible, but the ESP32 platform is selected from PIOArduino's current stable release:

```ini
[env:esp32dev]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/stable/platform-espressif32.zip
board = esp32dev
framework = arduino
build_unflags =
    -std=gnu++11
    -std=gnu++14
    -std=gnu++17
build_flags =
    -std=gnu++20
```

For ESP-IDF use the same `platform` and `board`, set `framework = espidf`, and retain the C++20 unflags/flags. The checked-in demo configurations intentionally follow the moving PIOArduino `stable` release rather than an obsolete platform pin.

## Repository dependency baseline

The current `event_v1` branch depends on prerequisite Event-integration APIs on `EDP-Memory/event_v1` and `EDP-Threading/event_v1`. All other mandatory EDP dependencies use their normal current baseline. Arduino IDE validation therefore uses the coherent sibling-repository workspace rather than independently resolving arbitrary versions.

## Validation commands

The mechanical V2 regression gate can be run independently:

```text
python3 tests/run_policy_checks.py
```

The normal host runner invokes that policy gate first, then executes positive, compile-fail and public-header contracts:

```text
python3 tests/run_tests.py
```

PIOArduino demo and complete-example surfaces are validated with:

```text
bash tests/run_demo_builds.sh
bash tests/run_example_build.sh
```

Every logical demo also includes an Arduino IDE sketch under `demos/<name>/Arduino_IDE`. With `arduino-cli` and the ESP32 Arduino core installed, validate all three with:

```text
bash tests/run_arduino_ide_builds.sh
```

The Arduino runner defaults to FQBN `esp32:esp32:esp32`, supplies the sibling EDP libraries plus their `src` roots for nested EDP header resolution, and explicitly compiles C++ sources with `-std=gnu++20`.

RPI400 is the authoritative executable-validation appliance. GitHub Actions remains representative CI and does not replace appliance validation or physical HIL where technically applicable.
