# Reference — `src/event/Runtime.hpp`

**Classification:** PUBLIC RUNTIME API with PRIVATE BOUNDED IMPLEMENTATION  
**Source:** [`src/event/Runtime.hpp`](../src/event/Runtime.hpp)

Runtime realizes one normalized local Event topology over application-owned Memory/Threading/mutex/callback providers. All mutable Event-core state is statically bounded by the plan.

# Private per-Type/family state

`AdmissionState<TPlan,TEvent,true>` owns one `BoundedTopology::IntrusiveQueue` of occurrence indices for Queue deployments. `AdmissionState<...,false>` owns only one pending occurrence index for NewestOnly.

`TypeState<TPlan,TEvent>` retains the authoritative active subscription bitset plus that admission state.

`TypeStateTuple<TPlan,PrimitiveTypes>` materializes one TypeState per locally deployed Event Type.

`ListenerCursor<TPlan,TThread>` retains a minimum-width cross-Type round-robin continuation cursor only when the Listener observes more than one Type; zero/one-Type specializations retain no state. `ListenerCursorTuple` materializes these per unique Listener.

`SharedPendingCounter<TCapacity>` retains a minimum-width used count only for positive family SharedPending capacity; zero specialization is stateless. `Count()`, `Increment()`, `Decrement()` are private bookkeeping operations used under the Event mutex.

`RecordView<TPlan,TEvent,TPool>` adapts EDP-Memory dedicated indexed storage to the record-indexing contract required by `BoundedTopology::IntrusiveQueue`; retained member `_pool` is a borrowed pool pointer.

`AnyTimedDeployment<TList>` is a compile-time predicate used to require a bound canonical monotonic Clock only when at least one deployment can retain deadlines.

# `Runtime<TPlan,TArchitecture,TMemoryRuntime,TThreadingRuntime,TMutexProvider,TCallbackProviders...>`

Template parameters bind the normalized semantic plan, provider-resolution Architecture, already-owned Memory/Threading runtimes, selected Event ordinary mutex and exact typed callback providers. Runtime derives `PrimitiveTypes`, `Listeners`, `Observations`, Type state tuple and Listener cursor tuple.

Private retained provider members `_memory`, `_threading`, `_mutex` and `_callbacks` are non-owning. `_types`, `_listenerCursors`, `_sharedPending`, and `_initialized` are Event-owned bounded state.

## Infrastructure failure boundary

`InfrastructureFailure()` terminates for impossible/terminal provider-coordination failure after topology is valid. Such failures are intentionally not folded into D20 `DispatchResult`, whose vocabulary remains `Accepted/NoCapacity/Expired`.

`Lock()` / `Unlock()` acquire/release the selected Threading ordinary mutex and treat provider failure as infrastructure failure.

## Memory/index helpers

`Record<TEvent>`, `Pool<TEvent>`, `State<TEvent>` are derived aliases. `StateFor`, `PoolFor`, `ToEventIndex`, `ToMemoryIndex`, `RecordAt`, `ReleaseOccurrence`, and `Records` connect semantic occurrence identity to Memory-owned dedicated storage without generic per-occurrence leases.

`MemoryPoolValid<TEvent>()` proves the Memory topology contains exactly one pool for the planner-generated record, with dedicated capacity equal to `MaximumInstances` and no shared raw overflow. `AllMemoryPoolsValid()` checks every local Type and feeds the class-level static assertion.

## Queue/shared bookkeeping

`QueuePendingCount<TEvent>()` performs a bounded traversal rather than retaining another count. `RemovePendingQueueOccurrence()` removes one occurrence and updates SharedPending accounting using pre-removal pending cardinality, preserving the fungible dedicated/shared entitlement rule.

## Expiry

`SweepExpired<TEvent>(now)` is opportunistic and bounded. For timed Queue deployments it traverses the pending Queue, clears expired pending recipients, releases logical entitlement, and physically releases only when no active borrow remains. For NewestOnly it clears the one pending occurrence similarly. Untimed deployments compile this work away.

## Subscription snapshot/wake

`SnapshotSubscriptions<TEvent>()` copies the Type-local active subscription bitset for admission. `ListenerIndexFor<TThread,TEvent>()` resolves the compile-time dense ordinal. `WakeRecipients<TEvent>()` recursively inspects the snapshot and signals each affected Dedicated Thread via `ThreadHandle<T>().Wake()`. Wake failure does not roll back admitted Event state.

## Local admission

Private local admission validates retention, opportunistically sweeps expiry, snapshots subscriptions, elides allocation for zero recipients, then applies Queue or NewestOnly policy transactionally.

Queue path enforces dedicated/shared logical entitlement before Memory acquisition, acquires one exact physical occurrence, appends its strong index to the FIFO and wakes snapshot recipients.

NewestOnly replaces an unborrowed pending occurrence in place when possible. If the old occurrence is actively borrowed it acquires a new physical slot first, then clears old pending interest without revoking the borrow. Capacity failure leaves the old pending occurrence unchanged.

## Listener claim/callback

`TryDeliverOne<TThread,TEvent>()` locks, sweeps expiry, finds the oldest Queue occurrence (or NewestOnly occurrence) whose recipient bit includes the Listener, claims it, removes exhausted pending topology, captures `const TEvent*`, then unlocks. The typed callback provider is selected by Architecture capability, invoked `noexcept` outside the mutex, then Runtime reacquires the mutex to release the borrow/reclaim storage.

`TryDeliverOrdinal`, `SetCursor`, `HasPendingForType`, and `HasAnyPending` provide bounded cross-Type drain/fairness and WorkRemaining calculation.

# Public operations

## Constructor

Binds already-owned Memory Runtime, Threading Runtime, Event mutex and callback provider objects. Runtime is noncopyable/nonmovable so provider bindings and retained topology state have stable identity.

## `Initialize()`

Returns `AlreadyInitialized` when repeated; returns ProviderFailure if Memory is not initialized or a timed deployment exists without canonical monotonic Clock; otherwise marks Runtime initialized. It does not own provider initialization.

## `IsInitialized()`

Predicate exposing Runtime initialization state.

## `Subscribe<TThread,TEvent>()`

Compile-time requires a locally deployed Event and statically declared Observe relation. Under the mutex, sets the Type-local subscription bit. Returns `Subscribed` or `AlreadySubscribed`. It allocates no runtime subscription object.

## `Unsubscribe<TThread,TEvent>()`

Clears active subscription and cancels that Listener's still-pending interests across bounded occurrence topology. It never revokes an already-claimed active borrow. Returns `Unsubscribed` or `NotSubscribed`.

## `Dispatch(LocalOnly,event,retention)` and shorthand `Dispatch(event,retention)`

Require local deployment and typed retention compatibility. Execute common local admission under the Event mutex. Rvalue materialization is constrained to nonthrowing construction and only occurs after semantic/capacity viability is established.

## `Dispatch(LocalAndRemote,event,retention,remoteOperation)`

Applies one common expiry precondition. If already expired, returns local `Expired` and remote `SkippedExpired` without entering either domain. Otherwise holds the Event linearization mutex while performing local admission and exactly one bounded external remote handoff, preserving independent local/native remote results. Local `NoCapacity` does not suppress a valid remote attempt and there is no rollback/fallback/quorum semantics.

## `Ingress(event,retention)`

Typed inbound facade equivalent to LocalOnly admission after an external Transport has already deserialized/validated the Event. It never automatically re-egresses inbound data.

## `Drain<TThread>(maximumDeliveries)`

Services at most the explicit callback budget. For multi-Type Listeners it begins at the retained round-robin cursor and advances after each delivery. After the budget/no-work condition it reports pending work under the mutex and advisorially wakes the Thread again when WorkRemaining is true. It never blocks; the Dedicated Thread decides when to call ThreadContext::Wait().
