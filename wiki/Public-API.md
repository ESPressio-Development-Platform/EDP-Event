# Public API

The supported consumer surface is exposed through `ESPressio_Event.hpp`.

## Family/deployment vocabulary

`Family`, `Queue<N>`, `NewestOnly`, `UntilHandoffOnly`, `TimedRetention`, `Deploy<TEvent,N,Admission,Retention,RemoteHandoffs=0>`, `SharedPending<N>` and `Observe<TThread,TEvent>` define static Event-family topology.

`EventType<T>` requires an Event-family `Primitives::PrimitiveType<T>` and then adds the retained-value constraints that are specific to Event: recursively memory-bounded ownership, no external lifetime dependencies and nothrow destruction. Because `PrimitiveType` requires both `System::SchemaType` and `Serialisation::SerialisableType`, every Event already has a stable TypeIdentifier, canonical FieldSet, and recursively serialisable schema before Event-specific qualification is applied. EventType intentionally does not duplicate either generic predicate.

Payload-bearing Event Types expose stable numeric Type-local fields with `System::FieldBinding`; zero-field Event Types use `System::FieldSet<>`.

## Planning/storage helpers

`PlanFor<TTopology>` resolves the normalized Event plan from a complete Primitive topology. `OccurrencePoolSpec<TPlan,TEvent,TMemoryResourceSelection>` derives the exact dedicated-only EDP-Memory pool specification.

## Runtime/lifecycle

`Bootstrap<Architecture,Plan,MemoryRuntime,ThreadingRuntime>` validates external providers/topology and owns the Event Runtime object. `Runtime` exposes `Initialize`, `Subscribe`, `Unsubscribe`, local `Dispatch`, transactional `PrepareIngress`, ordered `PrepareRemoteHandoff`, LocalAndRemote staged `Dispatch`, integration quiesce, and bounded `Drain`.

## Results/scope/retention

`DispatchResult`, `ReservationFailure`, `OrderedHandoffAttemptState`, `RemoteEventTerminal`, subscription/initialization/drain results, retention requests, `RemoteDispatchAttempt`, `OrderedHandoffAttempt` and `LocalAndRemoteDispatchResult` expose strongly typed state. `LocalOnly`, `RemoteOnly`, and `LocalAndRemote` are Event-facing re-exports of the canonical `EDP-Primitives` scope Types.

## Integration helper

`DispatchScoped(RemoteOnly,...)` supports an outbound-only Type without requiring a local Event Runtime/deployment. It applies the common expiry precondition then either returns `SkippedExpired` or invokes the external remote operation exactly once.

`InboundAdmission`, `IngressReservation`, `OutboundHandoff`, and `RemoteHandoffReservation` expose transactional ingress and nonblocking ordered egress. `RemoteEventOperation` observes frozen recipients through destination-admission terminals and deliberately has no Response surface.
