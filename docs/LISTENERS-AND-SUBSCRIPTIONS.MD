# Listeners and Subscriptions

## Listener identity

A Listener is not an Event-owned worker. It is an application-owned `EDP-Threading::DedicatedThread<TIdentity,...>` whose identity is reused by Event. One Dedicated Thread may observe many Event Types and will normally own other application work as well.

`Observe<TThreadIdentity,TEvent>` creates immutable eligibility. Event's planner assigns a dense Type-local Listener ordinal and uses it to dimension the subscription and pending-recipient bitsets. The application never configures or stores ordinals.

## Callback provider

For each Observe relation, Architecture must resolve exactly one `Event::Composition::ListenerCallback<TThreadIdentity,TEvent>` capability. The selected provider exposes:

```cpp
void OnEvent(const TEvent&) noexcept;
```

The Event reference is immutable and valid only for the callback duration. Retaining it or deriving an asynchronous pointer/reference from it violates the contract.

A single provider may offer callbacks for multiple Event Types consumed by one Thread.

## Subscribe and Unsubscribe

`Subscribe<Thread,Event>()` and `Unsubscribe<Thread,Event>()` are strongly typed and operate only on statically declared Observe relationships. Repeated Subscribe returns `AlreadySubscribed`; repeated Unsubscribe returns `NotSubscribed`.

Dispatch snapshots the active subscription bitset at its admission linearization point. A later subscription never replays an older occurrence. Unsubscribe clears only still-pending interest; an Event already claimed by a callback is not revoked. Re-subscribe does not resurrect cancelled interest.

## Bounded drain

`Drain<TThread>(maximumDeliveries)` performs at most the supplied finite number of callbacks. Its result reports the number delivered and whether Event work remains for that Listener. If work remains, Event publishes another advisory wake so a multi-purpose Thread can continue servicing work without losing progress.

Same-Listener callbacks are serialized by the fact that only that Listener's Dedicated Thread calls its Drain path. Cross-Type selection is bounded round-robin where more than one Event Type is observed.
