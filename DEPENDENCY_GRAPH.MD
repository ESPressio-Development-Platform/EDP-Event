# EDP-Event dependency graph

`EDP-Event` owns bounded Event-family planning, admission, subscription, retained occurrence lifetime and typed Listener delivery. It deliberately delegates generic storage, execution, time, finite topology and payload-boundedness mechanics to their owning EDP domains.

```text
EDP-Event
├── EDP-System             mandatory — TypeIdentifier and Composition framework
├── EDP-Primitives         mandatory — Primitive Family/Topology/FamilyPlan framework
├── EDP-Memory             mandatory — physical occurrence storage and lifetime
├── EDP-Threading          mandatory — Dedicated Threads, wake path and Event runtime mutex provider
├── EDP-Clock              mandatory — canonical monotonic retention deadlines
├── EDP-BoundedTopology    mandatory — strong indices, recipient sets and intrusive Queue topology
└── EDP-BoundedTypes       mandatory — recursive finite/self-contained Event payload qualification
```

The dependency directions above are architectural. The concrete package refs for `EDP-Memory` and `EDP-Threading` are pinned to `main`. Event requires the compact dedicated-index Object Pool seam and the Dedicated-Thread wake/keyed ordinary-mutex seam; those prerequisites are now integrated into the respective repositories' `main` branches. This remains a dependency-baseline requirement, not an additional dependency edge.

Concrete Platform providers are selected by the application through `EDP-Memory` and `EDP-Threading`; they are not direct `EDP-Event` dependencies.

Serialization codecs, Transport implementations, Security, Radio, Mesh and Sockets remain external integration domains and are not dependencies of `EDP-Event`.
