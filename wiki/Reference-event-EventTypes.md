# Reference — `src/event/EventTypes.hpp`

**Source:** [`src/event/EventTypes.hpp`](../src/event/EventTypes.hpp)

This header owns Event's public value, scope, retention-request and result vocabulary. It also owns the structured result Types used to preserve external remote-domain outcomes without interpreting them.

## Canonical Clock aliases — PUBLIC API

- `Duration` aliases `Clock::Duration`; all relative Event retention uses this canonical Clock representation.
- `MonotonicTimestamp` aliases `Clock::MonotonicTimestamp`; all absolute Event deadlines use this canonical monotonic representation.

## Operation result enums — PUBLIC API

- `DispatchResult`: `Accepted` means local admission committed; `NoCapacity` means bounded local capacity was unavailable; `Expired` means the common retention precondition had already expired.
- `SubscribeResult`: `Subscribed` means an inactive planned relation became active; `AlreadySubscribed` means it was already active.
- `UnsubscribeResult`: `Unsubscribed` means an active relation became inactive; `NotSubscribed` means it was already inactive.
- `InitializationResult`: `Initialized`, `AlreadyInitialized`, and `ProviderFailure` expose Runtime initialization outcomes.
- `RemoteDispatchAttemptState`: `SkippedExpired` means Event did not call the remote operation because the common expiry gate failed; `Attempted` means the remote operation ran exactly once and produced a retained native result.

## Execution-domain scope vocabulary — PUBLIC API

`LocalOnly`, `RemoteOnly`, and `LocalAndRemote` are Event-facing aliases of the canonical zero-state `EDP-Primitives` execution-domain control Types. `ExecutionDomainScope<TScope>` delegates to the canonical Primitive scope concept.

## Retention request vocabulary — PUBLIC API

- `UntilHandoff` is a zero-state request retaining local work until pending handoff completes/cancels.
- `ForDuration::Value` owns the requested canonical `Duration` relative to normalization time.
- `UntilDeadline::Value` owns an absolute canonical `MonotonicTimestamp` deadline.
- `RetentionRequest<TRetention>` accepts exactly `UntilHandoff`, `ForDuration`, and `UntilDeadline`.

## `RemoteDispatchAttempt<TRemoteResult>` — PUBLIC STRUCTURED RESULT

`TRemoteResult` is the provider-defined, non-void, nothrow-move-constructible and nothrow-destructible native result returned by the external remote operation. Event preserves this Type without translating provider semantics.

### Private implementation state

- `_state` is the authoritative presence state. It is `SkippedExpired` when no result object exists and `Attempted` while `Storage::Result` is live.
- `Storage` is a private union. `Empty` is active in the absent state; `Result` is active only in `Attempted` state. `Storage()` activates `Empty`; `~Storage()` deliberately leaves active-result destruction to the wrapper.
- Explicit result construction, move construction and destruction pass through `EDP-Memory::ObjectLifetime`.

### Public Type and lifecycle surface

- `State` aliases `RemoteDispatchAttemptState`.
- The default constructor creates `SkippedExpired` with no result payload.
- `RemoteDispatchAttempt(TRemoteResult)` establishes `Attempted` and transfers the provider result through EDP-Memory.
- Copy construction/assignment are deleted.
- Move construction transfers any live provider result through EDP-Memory and leaves the source `SkippedExpired`.
- Move assignment is deleted.
- The destructor destroys a live provider result through EDP-Memory only in `Attempted` state.

### State and payload access

- `GetState()` returns the authoritative `State`.
- `WasAttempted()` and `WasSkippedExpired()` are Boolean predicates over that state.
- mutable/const `ResultIfPresent()` return a pointer to the provider result only in `Attempted` state and `nullptr` otherwise. This prevents a caller from obtaining a reference to an inactive union member.

## `LocalAndRemoteDispatchResult<TLocalResult,TRemoteResult>` — PUBLIC STRUCTURED RESULT

Private member `_local` owns the local outcome and `_remote` owns the remote attempt wrapper. Construction and move construction transfer both values through EDP-Memory ownership-transfer semantics. Copy operations and move assignment are deleted. mutable/const `Local()` expose the local result and mutable/const `Remote()` expose the remote wrapper.

## `DrainResult` — PUBLIC STRUCTURED RESULT

- `Delivered` is the exact number of callbacks completed by one bounded `Drain` call.
- `WorkRemaining` states whether that Listener still has pending Event work after the bounded call returns.
