# Transport Contract and Execution-Domain Scope

## Event remains transport-neutral

Every Event Type is semantically serialisable and transmissible, but EDP-Event neither serializes bytes nor owns transport bindings. Concrete integration code owns codec choice, framing, addressing, connection state, retry, downstream buffering, acknowledgement and delivery semantics.

## Scoped dispatch

The public scope vocabulary is `LocalOnly`, `RemoteOnly` and `LocalAndRemote`.

- `LocalOnly` performs local Runtime admission only. Typed inbound Transport integration enters Event through `Ingress`, which is LocalOnly by construction and cannot automatically re-egress.
- `RemoteOnly` needs no local Event Runtime. `DispatchScoped(RemoteOnly,...)` performs the common expiry gate and, if valid, invokes exactly one already-selected bounded outbound operation.
- `LocalAndRemote` requires a local deployment. The Runtime performs the common expiry gate once, then attempts local and remote domains independently. Local failure such as `NoCapacity` does not suppress a valid remote attempt and there is no rollback/fallback/quorum contract.

## Remote result preservation

Event never interprets the concrete Transport/provider result. `RemoteDispatchAttempt<TRemoteResult>` adds only Event's own two-state fact: `SkippedExpired` or `Attempted`. In the attempted case it owns the provider-defined result unchanged.

`LocalAndRemoteDispatchResult` retains the local `DispatchResult` and the remote attempt separately. It deliberately exposes no aggregate success state.

## Ordering boundary

For LocalAndRemote, the bounded outbound call occurs while the Event Runtime's linearization mutex remains held so concurrent same-Type scoped Dispatch cannot reverse the Event-level handoff opportunity order. The concrete outbound operation must therefore be bounded and must not synchronously re-enter the same Event Runtime while that protected handoff is active.

For RemoteOnly, where no local Runtime need exist, concurrent same-Type serialization is the responsibility of the external binding/integration implementation.
