# Reference — `src/event/Reservation.hpp`

**Classification:** PUBLIC MESH/TRANSPORT INTEGRATION API
**Source:** [`src/event/Reservation.hpp`](../src/event/Reservation.hpp)

## Transactional inbound admission

`IngressReservation<TEvent,TRuntime>` is the move-only owner of one constructed but
unpublished occurrence, its physical slot and its worst-case pending entitlement.
`Value()` is the decoder's exclusive destination. `Commit()` revalidates generation,
integration lifecycle and retention, snapshots current subscribers, and atomically
publishes. `Abort()` and destruction release backing without visibility.

`IngressReservationResult` separates `Accepted()` from `ReservationFailure` and
transfers ownership through rvalue-only `TakeReservation()`.
`InboundAdmission<TEvent,TRuntime>` is the typed facade over `PrepareIngress`.

## Ordered outbound handoff

`RemoteHandoffReservation<TEvent,TRuntime>` owns one per-Type ordered opportunity and
only borrows the caller-owned immutable Event. `TryCommit(adapter)` never waits:
`EarlierPending` leaves the reservation live for retry; head ownership invokes the
adapter outside Event synchronization and preserves its native result. Expiry,
generation loss and lifecycle closure are explicit states. `Abort()` and destruction
resolve a terminal skip.

`RemoteHandoffReservationResult` and `OutboundHandoff<TEvent,TRuntime>` provide the
bounded preparation surface. Rvalue Events are rejected because the borrow must outlive
commit or abort.

`OrderedHandoffAttempt<T>` owns an optional adapter result. `ResultIfPresent()` is
non-null only in `Attempted`.

## Remote Event operation

`RemoteEventOperation<TEvent,TBinding>` is a move-only source observation capability
over a binding-frozen bounded recipient set. It exposes `RecipientCount`, `Recipient`,
`Observe`, finite `WaitFor`/`WaitUntil`, cooperative `RequestCancellation`, and
idempotent `Release`.

The complete family terminal vocabulary is `Admitted`, `AlreadyAdmitted`, `Refused`,
`ExpiredNotAdmitted`, `CancelledNotAdmitted`, and `OutcomeUncertain`. There is no
Response extraction or listener-completion acknowledgement. The binding owns
DeliveryIdentifier duplicate suppression and bounded record lifetime; wrapper
abandonment releases observation only.
