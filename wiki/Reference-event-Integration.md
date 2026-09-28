# Reference — `src/event/Integration.hpp`

**Classification:** PUBLIC INTEGRATION API  
**Source:** [`src/event/Integration.hpp`](../src/event/Integration.hpp)

## `DispatchScoped(RemoteOnly,TEvent,TRetention,TRemoteOperation)`

Outbound-only coordination for an Event Type with no local Event deployment/Runtime. `TEvent` must satisfy EventType; `TRetention` is one retention request; `TRemoteOperation` must be non-throwing and return an observable, nothrow-movable/destructible provider-defined result.

The helper first normalizes the common retention precondition. If expired it returns `RemoteDispatchAttempt<TRemoteResult>` in `SkippedExpired` state without invoking the remote operation. Otherwise it invokes `remoteOperation(const TEvent&)` exactly once and returns Attempted containing the native result. It owns no binding, codec, queue or downstream work.
