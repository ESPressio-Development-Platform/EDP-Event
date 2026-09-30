# EDP-Event dependency graph

`EDP-Event` owns bounded Event-family planning, admission, subscription, retained occurrence lifetime and typed Listener delivery. It deliberately delegates generic schema identity, storage, execution, time, finite topology and payload-boundedness mechanics to their owning EDP domains.

```text
EDP-Event
├── EDP-System             mandatory — TypeIdentifier/Field schema and Composition framework
├── EDP-Primitives         mandatory — Primitive SchemaType invariant, Family/Topology/FamilyPlan framework and canonical execution-domain scope vocabulary
├── EDP-Memory             mandatory — physical occurrence storage and lifetime
├── EDP-Threading          mandatory — Dedicated Threads, wake path and Event runtime mutex provider
├── EDP-Clock              mandatory — canonical monotonic retention deadlines
├── EDP-BoundedTopology    mandatory — strong indices, recipient sets and intrusive Queue topology
└── EDP-BoundedTypes       mandatory — recursive finite/self-contained Event payload qualification
```

The Serialisation-prerequisite schema migration introduces **no new Event dependency edge**. Event already depends on EDP-Primitives and EDP-System; the strengthened Primitive contract simply means every Event now consumes the existing System `SchemaType`/`FieldSet` foundation through that established dependency direction.

The dependency directions above are architectural. `EDP-Memory` and `EDP-Threading` remain consumed from `main`. On `EDP-Event/serialisation_prerequisites`, validation intentionally consumes matching `EDP-System/serialisation_prerequisites` and `EDP-Primitives/serialisation_prerequisites`; Event `main` continues to consume their `main` branches. The temporary branch coherence is a workstream validation requirement, not a new runtime dependency.

Concrete Platform providers are selected by the application through `EDP-Memory` and `EDP-Threading`; they are not direct `EDP-Event` dependencies.

Serialisation codecs, Transport implementations, Security, Radio, Mesh and Sockets remain external integration domains and are not dependencies of `EDP-Event`.
