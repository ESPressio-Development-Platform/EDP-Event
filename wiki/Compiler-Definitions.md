# Compiler Definitions

EDP-Event production headers define no repository-owned preprocessor configuration switches. Topology/policy configuration is expressed through C++ Types and non-type template parameters rather than global compiler defines.

The demo/example source uses the externally supplied `ARDUINO` framework macro only to select Arduino `setup()/loop()` versus ESP-IDF `app_main()` entry points. This macro is owned by the framework/toolchain, not EDP-Event.

Platform/SDK macros may indirectly control concrete provider availability in EDP-Platform repositories; they do not alter Event-family semantics.
