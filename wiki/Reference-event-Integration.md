# Reference — `src/event/Integration.hpp`

**Classification:** PUBLIC INTEGRATION API  
**Source:** [`src/event/Integration.hpp`](../src/event/Integration.hpp)

## `DispatchScoped<TEvent,TRetention,TRemoteOperation>(RemoteOnly,...)`

Outbound-only coordination when no local Event Runtime/deployment is required. `TEvent` is constrained by `EventType`; `TRetention` must satisfy `RetentionRequest`; `TRemoteOperation` is the selected external bounded operation and must be non-throwing.

The helper derives `RemoteResult` from `remoteOperation(const TEvent&)` and requires a non-void, nothrow move-constructible and nothrow destructible result. It normalizes retention through `Detail::NormalizeRetention`. Expired input returns `RemoteDispatchAttempt<RemoteResult>` in `SkippedExpired` state without invoking the operation; otherwise it invokes exactly once and returns `Attempted` with the native result.

Runtime-backed LocalAndRemote ordering and transactional inbound capabilities live in `Reservation.hpp`, which this header re-exports. RemoteOnly remains binding-ordered because no local Event Runtime is required to exist.
