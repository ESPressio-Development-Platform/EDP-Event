# Tooling Reference

EDP-Event contains maintained validation tooling rather than a consumer CLI/compiler.

## `tests/run_tests.py`

Discovers the managed sibling-repository topology, compiles and executes the positive host tests, verifies each `tests/compile_fail/*.cpp` source fails compilation as required, and compiles each public Event header independently. A successful run establishes host/compile-contract evidence only.

## `tests/run_demo_builds.sh`

Validates PlatformIO Arduino and PlatformIO ESP-IDF forms of all three Event demos against the coherent local sibling-source topology. It creates temporary project configuration, injects sibling include paths, removes stale build output and invokes PlatformIO. Temporary configuration is removed after each successful project.

## `tests/run_example_build.sh`

Validates the complete `examples/TemperatureMonitor` PlatformIO project independently from the demo matrix. It uses the same coherent sibling-source strategy, removes stale example build output, creates only a temporary local PlatformIO configuration, and deletes that configuration on exit. This runner exists because the README example is a maintained consumer-facing integration surface in its own right.

None of these tools publishes code or mutates release versions.
