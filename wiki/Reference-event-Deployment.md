# Reference — `src/event/Deployment.hpp`

**Source:** [`src/event/Deployment.hpp`](../src/event/Deployment.hpp)

This header defines the public immutable Event deployment vocabulary plus private compile-time traits used to validate it.

## Public declaration API

### `EventType<TEvent>` — PUBLIC CONCEPT

`TEvent` is the candidate retained Event payload Type. The concept requires an EDP Primitive whose family is `Event::Family`, recursively memory-bounded under EDP-BoundedTypes, free of external lifetime dependencies, and nothrow destructible.

### Admission and retention policy Types — PUBLIC API

- `Queue<TDedicatedPending>` selects FIFO admission; `DedicatedPending` exposes its exact logical entitlement.
- `NewestOnly` selects latest-value admission.
- `UntilHandoffOnly` declares that the local deployment never needs deadline storage.
- `TimedRetention` declares duration/deadline support.

### `Deploy<TEvent,TMaximumInstances,TAdmission,TRetention>` — PUBLIC DECLARATION

`TEvent` is the retained payload; `TMaximumInstances` is exact simultaneous physical capacity; `TAdmission` is `Queue<N>` or `NewestOnly`; `TRetention` is `UntilHandoffOnly` or `TimedRetention`. Members are `Family`, `Event`, `Admission`, `Retention`, and `MaximumInstances`.

### `SharedPending<TSlots>` — PUBLIC DECLARATION

`TSlots` is the exact family-wide fungible Queue-overflow entitlement. `Family` identifies Event and `Slots` exposes the capacity.

### `Observe<TThreadIdentity,TEvent>` — PUBLIC DECLARATION

`TThreadIdentity` identifies the Dedicated Thread Listener endpoint; `TEvent` must satisfy `EventType`. Members are `Family`, `ThreadIdentity`, and `Event`.

## `Detail::AdmissionTraits<TAdmission>` — PRIVATE IMPLEMENTATION

The primary template classifies an unsupported admission policy with `IsValid=false`, `IsQueue=false`, and `DedicatedPending=0`. `Queue<TCapacity>` and `NewestOnly` specializations expose valid compile-time characteristics used by planning only.

## `Detail::IsRetentionPolicyV<TRetention>` — PRIVATE IMPLEMENTATION

Variable template accepting exactly `UntilHandoffOnly` and `TimedRetention`. `Deploy` uses it to reject unsupported local retention capability declarations.
