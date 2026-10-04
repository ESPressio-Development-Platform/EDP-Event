# Transport Contract and Execution-Domain Scope

## Event remains transport-neutral

Every Event Type is serialisable, but EDP-Event owns no codec, wire bytes, route,
connection, retry, acknowledgement, downstream queue or concrete Transport binding.
A selected adapter performs bounded synchronous encoding before its Mesh admission
returns; no typed Event borrow may survive that acceptance.

## Scoped dispatch

The public scope Types are exact re-exports of
`EDP-Primitives::ExecutionDomain::{LocalOnly,RemoteOnly,LocalAndRemote}`.

- `LocalOnly` performs local Runtime admission only.
- `RemoteOnly` needs no local Runtime. `DispatchScoped(RemoteOnly,...)` applies
  the expiry gate and calls one selected bounded operation. Concurrent ordering for
  this Runtime-free surface belongs to that external binding.
- `LocalAndRemote` admits locally first. Local refusal never suppresses the
  independent remote opportunity. It returns separate local and remote-reservation
  outcomes and has no rollback, fallback, quorum or aggregate-success contract.

## Ordered LocalAndRemote handoff

Each `Deploy` explicitly plans 0..255 same-Type remote handoff slots. Admission
reserves only a local ring position and a borrow of the caller-owned immutable Event;
Event never creates a second payload queue.

`RemoteHandoffReservation::TryCommit` is nonblocking. A later reservation reports
`EarlierPending` until all earlier opportunities are resolved. The head captures its
borrow under the ordinary Event mutex, releases that mutex, invokes the adapter, then
re-enters only to mark the slot terminal and advance across contiguous resolved slots.
Expiry, abort and integration quiesce are terminal skips, so a slot cannot permanently
block its successors. The sequence is local ordering metadata and is not a wire identity.

The caller must keep the Event alive and immutable until the reservation commits or
aborts. Destroying a live reservation aborts it.

## Transactional inbound admission

`PrepareIngress<TEvent>` reserves a physical occurrence plus the worst-case Queue
pending entitlement and default-constructs an unpublished destination. A decoder
populates `Value()` without holding Event synchronization.

`Commit()` revalidates generation, lifecycle and retention, snapshots the current
subscriber set, and atomically publishes Queue/NewestOnly visibility. No-subscriber
commit is `Accepted` and releases backing immediately. Abort, destruction or expiry
releases all backing without listener callbacks or publication. Quiesce cancels
publication but leaves the exclusive decoder destination alive until its owner commits
or aborts, avoiding destruction concurrent with population.

## Remote Event semantics

`RemoteEventOperation<TEvent,TBinding>` is a move-only observation capability over
the adapter-frozen recipient set. The binding owns DeliveryIdentifier duplicate
suppression and bounded records. Per-recipient terminals are `Admitted`,
`AlreadyAdmitted`, `Refused`, `ExpiredNotAdmitted`, `CancelledNotAdmitted`, and
`OutcomeUncertain`.

Remote Event lifecycle ends at destination admission. There is no Response,
listener-completion acknowledgement or aggregate remote success. Cancellation cannot
retract committed destination admission or an active local listener borrow; releasing
the operation abandons source observation only.

## Provider results and quiesce

`OrderedHandoffAttempt<T>` preserves the adapter result only for `Attempted`.
`RemoteDispatchAttempt<T>` remains the direct RemoteOnly wrapper.

`BeginIntegrationQuiesce()` closes new ingress/outbound reservations and
generation-safely cancels every non-in-flight capability. Outbound skips are reclaimed
immediately; an ingress reservation retains its exclusive destination until the decoder
subsequently commits or aborts. `IsIntegrationQuiescent()` reports when no integration
slot remains. Existing local occurrence delivery is independent of this integration gate.
