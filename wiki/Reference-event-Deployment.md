# Reference — `src/event/Deployment.hpp`

**Classification:** PUBLIC DECLARATION API with private compile-time traits  
**Source:** [`src/event/Deployment.hpp`](../src/event/Deployment.hpp)

## Public declarations

### `EventType<TEvent>`

Concept accepting only Event-family Primitive Types certified by `EDP-BoundedTypes` as recursively memory-bounded and free from external lifetime dependencies. This is the retained-payload safety gate.

### `Queue<TDedicatedPending>`

Admission policy. Template parameter is the exact Type-local dedicated pending entitlement. Member `DedicatedPending` exposes that compile-time value. V1 ordering is FIFO.

### `NewestOnly`

Admission policy with at most one uncommitted pending occurrence. It carries no Queue entitlement.

### `UntilHandoffOnly`

Local deployment retention capability that structurally forbids timed local retention and permits deadline storage to compile away.

### `TimedRetention`

Local deployment retention capability enabling UntilHandoff, duration and absolute-deadline requests.

### `Deploy<TEvent,TMaximumInstances,TAdmission,TRetention>`

One local Event deployment. `TEvent` is the semantic payload Type; `TMaximumInstances` is exact simultaneous live-occurrence capacity (1..255); `TAdmission` is `Queue<N>` or `NewestOnly`; `TRetention` is one of the two retention capabilities. Nested aliases `Family`, `Event`, `Admission`, `Retention` and constant `MaximumInstances` are consumed by Planner. Static assertions reject invalid capacities/policies and Queue entitlement above physical capacity.

### `SharedPending<TSlots>`

One optional family-wide Queue overflow entitlement declaration. `Slots` is the exact logical shared pending capacity. Zero and provably unused declarations are rejected by Planner rather than retaining useless state.

### `Observe<TThreadIdentity,TEvent>`

Immutable eligibility declaration. `TThreadIdentity` identifies one application Dedicated Thread; `TEvent` must be locally deployed. Nested aliases allow Planner/Bootstrap to derive Listener topology/callback requirements.

## Private `Detail` traits

`AdmissionTraits<TAdmission>` classifies supported admission policy, Queue-ness and dedicated pending capacity. Unsupported policies expose `IsValid=false`. `RetentionValid<TRetention>` recognizes the two supported local retention capability Types. They are compile-time validation helpers and carry no runtime state.
