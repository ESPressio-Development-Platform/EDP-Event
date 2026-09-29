# Reference — `src/event/Occurrence.hpp`

**Classification:** INTERNAL RESOURCE/LIFETIME CONTRACT  
**Source:** [`src/event/Occurrence.hpp`](../src/event/Occurrence.hpp)

`Occurrence.hpp` defines the exact Memory-owned object retained for one accepted local Event occurrence. Its layout is planner-derived and aggressively removes state that topology proves unnecessary.

## `Detail::OccurrenceIndexSpace<TPlan,TEvent>`

Private semantic tag for `BoundedIndex`. `TPlan` and `TEvent` prevent physical slot identities from unrelated Event Types/plans being mixed even where their numeric storage widths are identical.

## `Detail::ListenerIndexSpace<TPlan,TEvent>`

Private semantic tag for Type-local dense Listener ordinals. It gives subscription/pending-recipient bitsets strong Type identity.

## `Detail::CountStorage<TMaximum>`

Private alias choosing the minimum unsigned scalar capable of representing the compile-time count range. Used for active-borrow bookkeeping rather than semantic identities.

## `Detail::ExpiryStorage<TTimed>`

Private EBO base controlling deadline storage.

- `ExpiryStorage<false>` retains no deadline; constructor/setter discard the argument and `Deadline()` returns the zero sentinel.
- `ExpiryStorage<true>` retains member `ExpiresAt`, the canonical monotonic absolute deadline. Constructor initializes it; `Deadline()` reads it; `SetDeadline()` updates it during NewestOnly replacement.

## `Detail::QueueLinkStorage<TIndex,TQueued>`

Private EBO base controlling intrusive FIFO linkage.

- nonqueued specialization retains no state;
- queued specialization retains one `Next` strong occurrence index, default Invalid, used by `BoundedTopology::IntrusiveQueue`.

## `Detail::BorrowStorage<TListeners,THasListeners>` — PRIVATE IMPLEMENTATION

Private EBO base controlling active callback borrow count. The zero-listener specialization retains no state; the positive-listener specialization aliases minimum-width `Storage`, retains authoritative member `Active`, returns it through `Count()`, increments after a successful claim, and decrements after callback return.

## `Detail::OccurrenceClaimResult` — PRIVATE OPERATION RESULT

Strong result for `OccurrenceRecord::Claim`. `Claimed` means the pending bit was consumed and an active callback borrow established; `NotPending` means that Listener had no pending interest and no state changed.

# `OccurrenceRecord<TPlan,TEvent>`

**Classification:** INTERNAL CROSS-MODULE RESOURCE CONTRACT.** It is generated into an EDP-Memory Object Pool and consumed by Event Runtime/BoundedTopology Queue mechanics.

Template parameters:
- `TPlan` — normalized Event plan that supplies deployment, Listener count, Queue shape and timed-retention capability.
- `TEvent` — concrete immutable Event payload Type.

Nested metadata:
- `Event` — payload Type;
- `Deployment` — deployment declaration for this Event Type;
- `MaximumInstances` — exact physical occurrence capacity;
- `ListenerCount` — eligible Type-local Listener cardinality;
- `Timed` — whether deadline storage exists;
- `Queued` — whether intrusive Queue linkage exists;
- `OccurrenceIndex` — strong compact slot identity;
- `ListenerIndex` — strong compact Type-local Listener identity;
- `ListenerSet` — one-bit-per-Listener set used for pending recipients.

Private compile-time aliases `DeploymentType`, `MaximumInstancesValue`, `ListenerCountValue`, `OccurrenceIndexType`, `ListenerIndexType`, and `ListenerSetType` derive exact internal layout vocabulary. Private bases `ExpiryBase`, `QueueBase`, and `BorrowBase` select conditional deadline/Queue/borrow state. `_event` is the retained payload and `_pending` is the authoritative pending-recipient snapshot.

## Construction

The templated constructor materializes the payload, recipient snapshot and normalized deadline. Runtime constrains construction to nothrow paths so `DispatchResult` needs no construction-failure state.

## `ReplaceUnborrowed(...)`

NewestOnly-only replacement path used when the old pending occurrence has no active borrow. It destroys and reconstructs the payload in the existing physical slot exclusively through `EDP-Memory::ObjectLifetime`, then replaces recipient/deadline state.

## Payload/recipient accessors

`Value()` returns mutable/internal or immutable payload references. Consumers receive only `const TEvent&` through callbacks. `PendingRecipients()` exposes internal mutable/const recipient sets to Runtime. `HasPendingRecipients()` is the authoritative pending-interest predicate.

## Borrow operations

`ActiveBorrowCount()` reads current active callback borrowers. `Claim(listener)` is a state-changing operation returning `Detail::OccurrenceClaimResult`; it clears that Listener pending bit and increments the borrow count only for `Claimed`. `ReleaseBorrow()` decrements after callback completion.

## Expiry

`ExpiresAt()` returns the normalized deadline (zero for untimed/UntilHandoff). `IsExpired(now)` is true only for a non-zero deadline at or before `now`; active borrows are not invalidated by expiry.

## Intrusive Queue contract

For Queue deployments, `QueueNext()` and `SetQueueNext(index)` satisfy the record contract required by `BoundedTopology::IntrusiveQueue`. These methods are unavailable for NewestOnly via `requires Queued`.

## Template-parameter coverage

`TEventArgument` is the forwarding payload argument accepted by construction/replacement helpers; it is constrained by the selected nothrow construction path and never retained as a reference. The apparent `noexcept` token in mechanical scans is a function qualifier, not a declaration.
