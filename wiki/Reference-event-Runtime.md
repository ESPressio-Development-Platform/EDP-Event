# Reference — `src/event/Runtime.hpp`

**Classification:** PUBLIC RUNTIME API with PRIVATE BOUNDED IMPLEMENTATION  
**Source:** [`src/event/Runtime.hpp`](../src/event/Runtime.hpp)

Runtime realizes one normalized local Event topology over application-owned Memory/Threading/mutex/callback providers. All mutable Event-core state is statically bounded by the plan.

# Private per-Type/family state

`AdmissionState<TPlan,TEvent,true>` owns one `BoundedTopology::IntrusiveQueue` of occurrence indices for Queue deployments. `AdmissionState<...,false>` owns only one pending occurrence index for NewestOnly.

`TypeState<TPlan,TEvent>` retains the authoritative active subscription bitset, admission state, per-physical-slot ingress generations, and the optional per-Type ordered outbound sequencer. Queue state counts exact committed-plus-reserved dedicated entitlement; every queued record remembers whether it consumes dedicated or SharedPending capacity.

`TypeStateTuple<TPlan,PrimitiveTypes>` materializes one TypeState per locally deployed Event Type.

`ListenerCursor<TPlan,TThread>` retains a minimum-width cross-Type round-robin continuation cursor only when the Listener observes more than one Type; zero/one-Type specializations retain no state. `ListenerCursorTuple` materializes these per unique Listener.

`SharedPendingCounter<TCapacity>` retains a minimum-width used count only for positive family SharedPending capacity; zero specialization is stateless. `Count()`, `Increment()`, `Decrement()` are private bookkeeping operations used under the Event mutex.

`RecordView<TPlan,TEvent,TPool>` adapts EDP-Memory dedicated indexed storage to the record-indexing contract required by `BoundedTopology::IntrusiveQueue`; retained member `_pool` is a borrowed pool pointer.

`AnyTimedDeployment<TList>` is a compile-time predicate used to require a bound canonical monotonic Clock only when at least one deployment can retain deadlines. `DeliveryAttemptResult` is a PRIVATE strong operational result: `Delivered` means one callback completed and `NoPendingOccurrence` means no claimable occurrence existed.

# `Runtime<TPlan,TArchitecture,TMemoryRuntime,TThreadingRuntime,TMutexProvider,TCallbackProviders...>`

Template parameters bind the normalized semantic plan, provider-resolution Architecture, already-owned Memory/Threading runtimes, selected Event ordinary mutex and exact typed callback providers. Runtime derives `PrimitiveTypes`, `Listeners`, `Observations`, Type state tuple and Listener cursor tuple.

Private aliases encode normalized topology and exact provider/state tuples. Provider members are non-owning. `_types`, listener cursors, SharedPending use, initialization, and the integration-admission gate are Event-owned bounded state. Outbound slots retain only caller-owned immutable Event pointers, deadlines, generations and state flags; no duplicate payload storage exists.

## Infrastructure failure boundary

`InfrastructureFailure()` terminates for impossible provider-coordination failure after topology is valid. Semantic publication can additionally return `RuntimeUnavailable` for stale generation or closed integration lifecycle.

`Lock()` / `Unlock()` acquire/release the selected Threading ordinary mutex and treat provider failure as infrastructure failure.

## Memory/index helpers

`Record<TEvent>`, `Pool<TEvent>`, `State<TEvent>` are derived aliases. `StateFor`, `PoolFor`, `ToEventIndex`, `ToMemoryIndex`, `RecordAt`, `ReleaseOccurrence`, and `Records` connect semantic occurrence identity to Memory-owned dedicated storage without generic per-occurrence leases.

`MemoryPoolValid<TEvent>()` proves the Memory topology contains exactly one pool for the planner-generated record, with dedicated capacity equal to `MaximumInstances` and no shared raw overflow. `AllMemoryPoolsValid()` checks every local Type and feeds the class-level static assertion.

## Queue/shared bookkeeping

`RemovePendingQueueOccurrence()` removes one occurrence and releases the exact entitlement recorded in that occurrence, so out-of-order removal cannot misattribute dedicated versus SharedPending use.

## Expiry

`SweepExpired<TEvent>(now)` is opportunistic and bounded. For timed Queue deployments it traverses the pending Queue, clears expired pending recipients, releases logical entitlement, and physically releases only when no active borrow remains. For NewestOnly it clears the one pending occurrence similarly. Untimed deployments compile this work away.

## Subscription snapshot/wake

`SnapshotSubscriptions<TEvent>()` copies the Type-local active subscription bitset for admission. `ListenerIndexFor<TThread,TEvent>()` resolves the compile-time dense ordinal. `WakeRecipients<TEvent>()` recursively inspects the snapshot and signals each affected Dedicated Thread via `ThreadHandle<T>().Wake()`. Wake failure does not roll back admitted Event state.

## Local admission

Private local admission validates retention, opportunistically sweeps expiry, snapshots subscriptions, elides allocation for zero recipients, then applies Queue or NewestOnly policy transactionally.

Queue path enforces dedicated/shared logical entitlement before Memory acquisition, acquires one exact physical occurrence, appends its strong index to the FIFO and wakes snapshot recipients.

NewestOnly replaces an unborrowed pending occurrence in place when possible. If the old occurrence is actively borrowed it acquires a new physical slot first, then clears old pending interest without revoking the borrow. Capacity failure leaves the old pending occurrence unchanged.

## Listener claim/callback

`TryDeliverOne<TThread,TEvent>()` performs bounded claim/delivery and returns `DeliveryAttemptResult::Delivered` or `NoPendingOccurrence`; claim itself uses `OccurrenceClaimResult`. `TryDeliverOrdinal<TThread,TIndex>(ordinal)` recursively selects an observed Type and propagates the same strong result. `SetCursor`, `HasPendingForType`, and `HasAnyPending` provide bounded fairness and genuine pending-work predicates.

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

## `Dispatch(LocalAndRemote,event,retention)`

Applies one common expiry decision, performs local admission first, and independently reserves the same-Type outbound opportunity at that linearization. The result contains local `DispatchResult` and `RemoteHandoffReservationResult` separately. Local `NoCapacity` does not suppress remote reservation. The caller-owned Event must remain immutable until `TryCommit` or abort.

## `PrepareRemoteHandoff` / `TryCommitRemoteHandoff`

Preparation reserves one bounded ring slot. TryCommit fails fast with `EarlierPending` unless the generation-safe slot owns the head; the adapter always runs after the Event mutex is released. Completion, expiry and abort advance across contiguous terminal slots.

## `PrepareIngress<TEvent>`

Reserves one physical occurrence plus worst-case pending entitlement and returns an unpublished default-constructed destination. Commit revalidates integration lifecycle, generation and expiry, snapshots current subscribers, and atomically publishes Queue/NewestOnly state. Abort and empty-subscriber commit release all backing.

`Ingress(event,retention)` remains the concise already-materialized LocalOnly facade.

## Integration quiesce

`BeginIntegrationQuiesce()` closes new reservations and cancels every non-in-flight ingress/outbound capability. Outbound skips are reclaimed immediately. An ingress reservation remains the exclusive owner of its unpublished destination until the decoder calls `Commit` or `Abort`; a cancelled commit returns `RuntimeUnavailable` and then releases backing. `IsIntegrationQuiescent()` reports when the bounded integration state is empty. Local pending delivery remains independent.

## `Drain<TThread>(maximumDeliveries)`

Services at most the explicit callback budget. For multi-Type Listeners it begins at the retained round-robin cursor and advances after each delivery. After the budget/no-work condition it reports pending work under the mutex and advisorially wakes the Thread again when WorkRemaining is true. It never blocks; the Dedicated Thread decides when to call ThreadContext::Wait().

## Additional private declaration coverage

Internal templates and aliases select exact listener, occurrence, pool, counter, reservation, sequencer and provider-result Types. They are bounded implementation details and create no hidden allocator, payload queue or Transport ownership.
