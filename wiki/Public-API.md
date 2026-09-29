# Public API

The supported consumer surface is exposed through `ESPressio_Event.hpp`.

## Family/deployment vocabulary

`Family`, `Queue<N>`, `NewestOnly`, `UntilHandoffOnly`, `TimedRetention`, `Deploy<TEvent,N,Admission,Retention>`, `SharedPending<N>` and `Observe<TThread,TEvent>` define static Event-family topology.

`EventType<T>` constrains retained payloads to Event-family Primitive Types that are recursively memory-bounded and self-contained.

## Planning/storage helpers

`PlanFor<TTopology>` resolves the normalized Event plan from a complete Primitive topology. `OccurrencePoolSpec<TPlan,TEvent,TMemoryResourceSelection>` derives the exact dedicated-only EDP-Memory pool specification.

## Runtime/lifecycle

`Bootstrap<Architecture,Plan,MemoryRuntime,ThreadingRuntime>` validates external providers/topology and owns the Event Runtime object. `Runtime` exposes `Initialize`, `Subscribe`, `Unsubscribe`, local `Dispatch`, `Ingress`, LocalAndRemote `Dispatch`, and bounded `Drain`.

## Results/scope/retention

`DispatchResult`, `SubscribeResult`, `UnsubscribeResult`, `InitializationResult`, `DrainResult`, `UntilHandoff`, `ForDuration`, `UntilDeadline`, `RemoteDispatchAttempt` and `LocalAndRemoteDispatchResult` expose strongly typed Event operational state. `LocalOnly`, `RemoteOnly`, and `LocalAndRemote` are Event-facing re-exports of the canonical `EDP-Primitives` execution-domain scope Types.

## Integration helper

`DispatchScoped(RemoteOnly,...)` supports an outbound-only Type without requiring a local Event Runtime/deployment. It applies the common expiry precondition then either returns `SkippedExpired` or invokes the external remote operation exactly once.

`RemoteDispatchAttempt::ResultIfPresent()` returns a pointer only when the remote operation was attempted and `nullptr` for `SkippedExpired`, structurally preserving payload absence.
