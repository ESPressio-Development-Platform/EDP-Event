# Event Types and Deployment

## Event Type contract

A concrete Event Type is the payload itself; there is no wrapper object. It must expose a stable `System::TypeIdentifier`, declare `using Family = Event::Family`, and satisfy `Event::EventType`.

Retained Event values must be recursively memory-bounded and self-contained according to EDP-BoundedTypes. Dynamic owners such as `std::string`/`std::vector`, and payloads with external lifetime dependencies, are rejected because `MaximumInstances` must bound the complete retained owned-memory graph rather than only `sizeof(TEvent)`.

## Local deployment

`Deploy<TEvent, MaximumInstances, Admission, Retention>` declares one locally admissible Event Type. `MaximumInstances` is the exact maximum number of simultaneously live occurrences, including records retained only because a callback still borrows them. V1 maps this directly to EDP-Memory's compact dedicated capacity and therefore accepts 1..255.

Admission is either:

- `Queue<D>` — FIFO pending occurrences with `D` dedicated pending entitlements; or
- `NewestOnly` — no pending Queue entitlement; a new admitted occurrence supersedes all still-pending interests in the prior occurrence.

Retention capability is either `UntilHandoffOnly` or `TimedRetention`. The former removes the per-occurrence deadline field entirely and rejects timed local Dispatch at compile time. `TimedRetention` permits `UntilHandoff`, `ForDuration` and `UntilDeadline` on each Dispatch.

## Listener eligibility

`Observe<TThreadIdentity,TEvent>` declares that one topology-owned Dedicated Thread is eligible to subscribe to one Event Type and identifies the typed callback capability that Architecture must supply. Eligibility is immutable. Runtime `Subscribe`/`Unsubscribe` only toggle already-planned relationships; no subscription object or heap node is created.

## Shared pending entitlement

One optional `SharedPending<N>` declaration supplies a family-wide logical overflow entitlement for Queue deployments. It does not create a shared physical occurrence pool. Each Event Type always retains its own exact physical EDP-Memory pool. The planner rejects zero or provably useless shared capacity.

## Memory topology

`OccurrencePoolSpec<TPlan,TEvent,TMemoryResourceSelection>` derives the exact EDP-Memory `ObjectPoolSpec` for one deployed Event Type. The application chooses only the Memory resource selection; Event determines the occurrence-record shape, `MaximumInstances` and `NoSharedOverflow` policy.
