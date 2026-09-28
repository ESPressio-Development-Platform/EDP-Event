# Tooling Reference

EDP-Event contains maintained validation tooling rather than a consumer CLI/compiler.

## `tests/run_tests.py`

Discovers the managed sibling-repository topology, compiles and executes the positive host tests, verifies each `tests/compile_fail/*.cpp` source fails compilation as required, and compiles each public Event header independently. A successful run establishes host/compile-contract evidence only.

## `tests/run_demo_builds.sh`

Validates PlatformIO Arduino and PlatformIO ESP-IDF forms of all three Event demos against the coherent local sibling-source topology. It creates temporary project configuration, injects sibling include paths, removes stale build output and invokes PlatformIO. Temporary configuration is removed after each successful project.

Neither tool publishes code or mutates release versions.
