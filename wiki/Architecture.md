# Architecture

## Domain model

One accepted local Dispatch represents one immutable occurrence of a concrete Event payload Type. At admission, Event snapshots the active subscription bits for that Type. Each eligible Listener is an application-owned `EDP-Threading::DedicatedThread` identity; fan-out is represented as one bit per Type-local Listener ordinal rather than payload copies.

Local occurrence storage is exact and per Type. `MaximumInstances<TEvent>` is the maximum simultaneous live occurrence count, including borrow-only occurrences whose pending recipients have already been exhausted. `EDP-Memory` owns physical storage/lifetime; Event owns semantic admission/reclamation decisions.

`Queue<D>` provides FIFO pending ordering plus `D` dedicated pending entitlements. Optional family-wide `SharedPending<S>` supplies logical overflow entitlement only. `NewestOnly` has no Queue and supersedes pending interest in the older occurrence without revoking an active callback borrow.

## Delivery

A Listener claim clears its pending bit and increments the occurrence active-borrow count while the one Event Runtime mutex is held. The mutex is released before invoking `OnEvent(const TEvent&) noexcept`. Callback return reacquires the mutex, releases the borrow and reclaims the occurrence when no pending interest/borrow remains.

## Execution context

Event never creates a worker. Event signals the existing Threading wake path of a Listener Dedicated Thread. A multipurpose Dedicated Thread drains a bounded amount of Event work, services its other responsibilities and may then wait indefinitely through `ThreadContext::Wait()` because every work/control path—including stop/termination—signals that same Threading wake path.

## Transport boundary

Outbound Transport is an external bounded typed handoff of `const TEvent&`; inbound Transport deserializes before calling Event ingress. Event does not own wire bytes, codec, route, buffering, retry, connection or acknowledgement semantics.
