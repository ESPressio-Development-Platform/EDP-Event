# Reference — `src/event/EventTypes.hpp`

**Classification:** PUBLIC VALUE/RESULT API  
**Source:** [`src/event/EventTypes.hpp`](../src/event/EventTypes.hpp)

## Aliases

`Duration` and `MonotonicTimestamp` expose the canonical EDP-Clock value Types used by Event retention.

## Result enums

### `DispatchResult`
- `Accepted` — local admission committed, including valid zero-recipient elision.
- `NoCapacity` — required bounded local physical/pending capacity unavailable.
- `Expired` — retention invalid at the common precondition; no local occurrence admitted.

### `SubscribeResult`
- `Subscribed` — previously inactive planned relation activated.
- `AlreadySubscribed` — relation was already active; state unchanged.

### `UnsubscribeResult`
- `Unsubscribed` — active relation disabled and pending interests cancelled where applicable.
- `NotSubscribed` — relation already inactive.

### `InitializationResult`
- `Initialized` — Event Runtime initialized successfully.
- `AlreadyInitialized` — initialization had already completed.
- `ProviderFailure` — required external runtime precondition/provider unavailable (for example Memory not initialized or timed deployment without bound monotonic Clock).

## Execution scope Types

`LocalOnly`, `RemoteOnly`, `LocalAndRemote` are zero-state dispatch control tags. `ExecutionDomainScope<T>` recognizes them. Scope is control metadata only and is not Event payload/wire identity.

## Retention request Types

`UntilHandoff` is zero-state. `ForDuration` owns one canonical `Duration` as member `Value`. `UntilDeadline` owns one canonical `MonotonicTimestamp` as `Value`. `RetentionRequest<T>` recognizes exactly these request Types.

## `RemoteDispatchAttempt<TRemoteResult>`

Move-only structured result preserving the external remote provider's native result without Event interpreting it. Template parameter is the non-void nothrow-movable/destructible provider result Type.

Nested `State` values:
- `SkippedExpired` — Event did not invoke remote handoff because the common expiry precondition failed.
- `Attempted` — remote operation invoked exactly once and `Result()` contains its native result.

Private union `Storage` retains either an inactive byte or the remote result. `_state` is authoritative presence state. Move construction transfers the remote result and clears the source to SkippedExpired. `WasAttempted()` / `WasSkippedExpired()` are predicates. `Result()` is valid only in Attempted state; callers must inspect state first.

## `LocalAndRemoteDispatchResult<TLocalResult,TRemoteResult>`

Move-only pair of independent domain outcomes. `_local` is the local semantic result; `_remote` is `RemoteDispatchAttempt<TRemoteResult>`. `Local()` / `Remote()` expose mutable/const references. The structure makes no aggregate success claim and encodes no rollback relationship.

## `DrainResult`

`Delivered` is the number of callbacks completed by one bounded Drain call. `WorkRemaining` states whether that Listener still has pending Event work after the budget was exhausted/completed.
