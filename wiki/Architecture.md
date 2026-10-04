# Architecture

## Domain model

One accepted local Dispatch represents one immutable occurrence of a concrete Event payload Type. The payload Type is itself an Event-family Primitive. Consequently every Event inherits the generic `Primitives::PrimitiveType` contract, including stable `System::TypeIdentifier` identity, `System::SchemaType` metadata, and `Serialisation::SerialisableType` qualification. Payload fields use explicit stable numeric Type-local `System::FieldIdentifier` bindings; a zero-field Event uses `System::FieldSet<>`.

`EventType<T>` deliberately adds only Event-specific retained-value constraints above the generic Primitive contract: exact Event-family identity, recursively bounded owned memory, no external lifetime dependencies and nothrow destruction. Schema qualification is not duplicated locally in Event.

At admission, Event snapshots the active subscription bits for that Type. Each eligible Listener is an application-owned `EDP-Threading::DedicatedThread` identity; fan-out is represented as one bit per Type-local Listener ordinal rather than payload copies.

Local occurrence storage is exact and per Type. `MaximumInstances<TEvent>` is the maximum simultaneous live occurrence count, including borrow-only occurrences whose pending recipients have already been exhausted. `EDP-Memory` owns physical storage/lifetime; Event owns semantic admission/reclamation decisions.

`Queue<D>` provides FIFO pending ordering plus `D` dedicated pending entitlements. Optional family-wide `SharedPending<S>` supplies logical overflow entitlement only. `NewestOnly` has no Queue and supersedes pending interest in the older occurrence without revoking an active callback borrow.

## Delivery

A Listener claim clears its pending bit and increments the occurrence active-borrow count while the one Event Runtime mutex is held. The mutex is released before invoking `OnEvent(const TEvent&) noexcept`. Callback return reacquires the mutex, releases the borrow and reclaims the occurrence when no pending interest/borrow remains.

## Execution context

Event never creates a worker. Event signals the existing Threading wake path of a Listener Dedicated Thread. A multipurpose Dedicated Thread drains a bounded amount of Event work, services its other responsibilities and may then wait indefinitely through `ThreadContext::Wait()` because every work/control path—including stop/termination—signals that same Threading wake path.

## Transport boundary

Outbound integration reserves a planned same-Type ordering slot and borrows `const TEvent&` only until one bounded adapter call returns. The adapter encodes synchronously outside Event synchronization and may not retain the typed borrow after Mesh admission. Inbound integration first reserves a constructed unpublished occurrence, decodes directly into its exclusive destination outside Event synchronization, then commits against current subscribers. Event does not own wire bytes, codec, route, retry, connection, acknowledgement or a duplicate payload queue. Whether a deployment currently uses Transport does not alter the Event Type's intrinsic schema contract.
