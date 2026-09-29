# Admission and Retention

## Local Dispatch result

Local admission returns exactly:

- `DispatchResult::Accepted` — the local occurrence committed, or there were no active local interests and no record was required;
- `DispatchResult::NoCapacity` — bounded physical or pending entitlement was unavailable;
- `DispatchResult::Expired` — the retention request was already expired before admission.

There is no capacity wait, cancellation handle, completion handle or fourth infrastructure-failure result. Event provider/infrastructure failure violates the established Bootstrap/runtime invariant and is not reclassified as a semantic dispatch outcome.

## Queue admission

A Queue Type has one same-Type FIFO. One pending occurrence consumes exactly one pending entitlement regardless of fan-out count. The first `Queue<D>` pending occurrences are covered by the Type's dedicated entitlement; later pending occurrences consume the family-wide Shared Pending counter. When an occurrence stops being pending, shared usage is decremented if the pre-removal Queue depth exceeded the dedicated entitlement.

Before returning `NoCapacity`, timed deployments opportunistically reclaim expired pending interests for that Type. Event never evicts a valid FIFO occurrence merely to admit a newer one.

## NewestOnly

NewestOnly has no pending Queue entitlement. If the current pending occurrence is not actively borrowed, Event may replace its payload in place. If the old occurrence is already borrowed by a callback, a new physical slot is acquired; all old pending interests are superseded but the borrowed immutable payload remains alive until callback return.

## Retention normalization

`UntilHandoff` stores no deadline. `ForDuration` is normalized at Dispatch to a canonical absolute monotonic deadline. `UntilDeadline` uses the supplied canonical monotonic timestamp. Zero remains the untimed sentinel; an already-expired timed request is rejected before any local or remote domain attempt.

Timed expiry is semantically immediate but physical cleanup is opportunistic: before same-Type admission, before Listener handoff, and during bounded pressure/sweeps. Expiry removes pending interests only and never revokes an active callback borrow.

## Move/copy guarantees

A local Dispatch path only participates when retained occurrence construction is non-throwing. Capacity/expiry viability is established before destructive move construction, preserving a rejected rvalue source. `LocalAndRemote` currently takes an immutable Event reference and requires nothrow copying for the local retained occurrence because the same Event is also borrowed by the bounded outbound operation.
