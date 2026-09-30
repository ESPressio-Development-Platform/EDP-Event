# EDP-Event Wiki

EDP-Event owns the bounded broadcast Primitive family: compile-time Event deployment, runtime subscription state, retained occurrence admission/lifetime, typed Dedicated-Thread delivery, retention and the narrow typed Transport boundary.

Every Event payload Type is itself an `EDP-Primitives` Primitive and therefore carries stable `EDP-System` Type identity plus a canonical compile-time Field schema. `Event::EventType` adds only the Event-specific retained-value requirements above that generic Primitive contract.

It deliberately does **not** own allocators, native synchronization, threads/stacks, clocks, codecs, wire framing, routing, retry, acknowledgement, Radio, Mesh or Sockets. Those responsibilities remain in their owning EDP domains.

Primary consumer entry point: [`src/ESPressio_Event.hpp`](../src/ESPressio_Event.hpp).

## Navigation

- [Architecture](Architecture)
- [Dependency Contracts](Dependency-Contracts)
- [Public API](Public-API)
- [Internal API](Internal-API)
- [Private Implementation](Private-Implementation)
- [Composition and Providers](Composition-and-Providers)
- [Resources, Lifecycle and Concurrency](Resources-Lifecycle-and-Concurrency)
- [Build, Test and Source Navigation](Build-Test-and-Source-Navigation)
- [Compiler Definitions](Compiler-Definitions)
- [Tooling Reference](Tooling-Reference)
- [Reference Index](Reference-Index)
