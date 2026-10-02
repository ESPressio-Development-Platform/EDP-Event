# EDP-Event dependency graph

`EDP-Event` owns bounded Event-family planning, admission, subscription, retained occurrence lifetime and typed Listener delivery. It deliberately delegates generic schema identity, storage, execution, time, finite topology and payload-boundedness mechanics to their owning EDP domains.

```text
EDP-Event
├── EDP-System             mandatory — TypeIdentifier/Field schema and Composition framework
├── EDP-Primitives         mandatory — Primitive SchemaType + SerialisableType invariants, Family/Topology/FamilyPlan framework and canonical execution-domain scope vocabulary
├── EDP-Memory             mandatory — physical occurrence storage and lifetime
├── EDP-Threading          mandatory — Dedicated Threads, wake path and Event runtime mutex provider
├── EDP-Clock              mandatory — canonical monotonic retention deadlines
├── EDP-BoundedTopology    mandatory — strong indices, recipient sets and intrusive Queue topology
└── EDP-BoundedTypes       mandatory — recursive finite/self-contained Event payload qualification
```

Stage C introduces **no new direct Event dependency edge**. Event already depends on EDP-Primitives; the strengthened Primitive contract means every Event now inherits both System schema and Serialisation qualification through that established direction. EDP-Serialisation is therefore transitive from Event, not directly consumed by Event source.

The dependency directions above are architectural. All Stage-C work is now on `main`; the former prerequisite branches have been fast-forwarded into their owning `main` branches.

Concrete Platform providers are selected by the application through `EDP-Memory` and `EDP-Threading`; they are not direct `EDP-Event` dependencies.

Serialisation codecs, Transport implementations, Security, Radio, Mesh and Sockets remain external integration domains and are not direct dependencies of `EDP-Event`; only compile-time serialisability qualification arrives transitively through EDP-Primitives.
