# Delivery and Lifetime

## One immutable occurrence

Every accepted retained Dispatch constructs one physical occurrence in the Type's exact EDP-Memory pool. The occurrence owns one immutable `TEvent`, a compact pending-recipient set, optional FIFO linkage, optional expiry deadline and a minimum-width active-borrow count.

`EDP-BoundedTopology::BoundedIndex` supplies the strong physical slot identity. The index is converted directly to EDP-Memory's dedicated-pool index without retaining a generic pointer-bearing lease.

## Claim and borrow

Under the Event mutex, Listener delivery finds the oldest eligible same-Type occurrence (or the current NewestOnly occurrence), clears that Listener's pending bit, increments the active-borrow count, and removes the occurrence from pending topology when no pending recipients remain. The mutex is then released.

The callback receives `const TEvent&`. After callback return, Event reacquires the mutex, decrements the borrow count, and returns the physical slot to Memory if neither pending recipients nor active borrows remain.

This separation is why callbacks may safely re-enter Event operations: Event never executes application callback code while holding its own non-recursive mutex.

## Supersession and expiry

NewestOnly supersession may clear all pending interests from an old occurrence but cannot invalidate an active borrow. Likewise, expiry cancels only pending interests. These rules keep the callback reference stable for its entire lexical callback lifetime.

## Zero-interest admission

An otherwise-valid local Dispatch whose subscription snapshot is empty returns `Accepted` without allocating an occurrence. This is intentional: admission succeeded, but there was no local delivery work to retain.
