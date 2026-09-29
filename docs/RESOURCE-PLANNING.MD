# Resource Planning

## Semantic dimensions

Event resource planning keeps semantic capacities instead of opaque byte totals. Per deployed Type the planner knows `MaximumInstances`, Queue dedicated pending entitlement, eligible Listener count and timed-retention capability. Family-wide it knows the unique Listener set and optional Shared Pending entitlement.

## Physical occurrence storage

Every locally deployed Type receives one exact dedicated-only EDP-Memory pool over its planner-generated `OccurrenceRecord<TPlan,TEvent>`. There is no shared cross-Type raw Event pool. `MaximumInstances` therefore controls the complete physical simultaneous-live bound for that Type.

## Compact topology state

EDP-BoundedTopology provides:

- strong capacity-derived occurrence and Listener indices;
- one-bit-per-Listener subscription and pending-recipient sets;
- intrusive FIFO head/tail and per-record link for Queue deployments.

Event derives counter/index storage width from compile-time maxima; applications never select bookkeeping integer Types.

## Compile-away behavior

The representation removes state which topology proves unnecessary:

- `UntilHandoffOnly` has no `ExpiresAt` field;
- `NewestOnly` has no FIFO link/Queue entitlement;
- zero eligible Listeners use the zero-capacity membership specialization;
- SharedPending absence has no retained used counter;
- one-Type Listener has no cross-Type round-robin cursor.

`[[no_unique_address]]` is used on empty state where valid so C++ object layout can realize those zero-cost specializations.

## Pending entitlement versus physical capacity

Queue pending entitlement is logical admission capacity, separate from physical occurrence slots. An occurrence with all pending interests claimed may no longer consume pending entitlement while remaining physically live because callbacks still borrow it. Conversely one pending occurrence consumes only one pending entitlement regardless of Listener fan-out.

`SharedPending<N>` is a fungible family-wide overflow counter, not Memory storage. The Type's dedicated entitlement is implicit for the first Queue positions; Event therefore does not retain an origin flag per occurrence.
