# Tooling Reference

EDP-Event contains maintained validation tooling rather than a consumer CLI/compiler.

## `tests/run_tests.py`

Discovers the managed sibling-repository topology, compiles and executes the positive host tests, verifies each `tests/compile_fail/*.cpp` source fails compilation as required, and compiles each public Event header independently. A successful run establishes host/compile-contract evidence only.

## `tests/run_demo_builds.sh`

Validates PIOArduino-compatible Arduino and ESP-IDF forms of all three Event demos against the coherent local sibling-source topology. It creates temporary project configuration, injects sibling include paths, removes stale build output and invokes the PlatformIO-compatible CLI against PIOArduino `stable`. Temporary configuration is removed after each successful project.

## `tests/run_example_build.sh`

Validates the complete `examples/TemperatureMonitor` PIOArduino-compatible project independently from the demo matrix. It uses the same coherent sibling-source strategy, removes stale example build output, creates only a temporary local PIOArduino-compatible configuration, and deletes that configuration on exit. This runner exists because the README example is a maintained consumer-facing integration surface in its own right.

None of these tools publishes code or mutates release versions.

## `tests/run_arduino_ide_builds.sh`

Validates all three Arduino IDE sketch surfaces with `arduino-cli`. `ARDUINO_CLI` may select a non-default executable and `EDP_EVENT_ARDUINO_FQBN` may select a board FQBN; the default is `esp32:esp32:esp32`. The runner supplies each sibling EDP repository as an Arduino library and injects each sibling `src` root so nested EDP public headers resolve consistently, while explicitly compiling C++ sources with `-std=gnu++20`.

## `tests/run_policy_checks.py`

Mechanical regression gate for the V2 compliance defects discovered after initial completion: direct production lifetime/move primitives, Boolean operation results, unsafe remote-result access, audited visibility ordering, focused-document reachability, Wiki reference presence, PIOArduino stable/C++20 baseline, stale identifiers and namespace spacing. `tests/run_tests.py` executes this gate before compiler contracts.
