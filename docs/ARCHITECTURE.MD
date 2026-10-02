# Architecture

## Purpose

`EDP-Event` is the broadcast Primitive family. One accepted dispatch represents one immutable occurrence that may be delivered to zero or more statically eligible Listeners whose runtime subscriptions were active at admission.

## Type and schema foundation

The Event payload Type is the Event Primitive itself. Generic semantic identity and schema are inherited from `EDP-Primitives`: every Event must satisfy `Primitives::PrimitiveType`, and therefore both `System::SchemaType` and `Serialisation::SerialisableType`, before Event-specific qualification is considered. Payload-bearing Events expose stable Type-local numeric Field identifiers through `System::FieldSet`; zero-field Events use `System::FieldSet<>`.

`Event::EventType` intentionally adds only family-specific retained-value constraints: exact `Event::Family`, recursively bounded memory, no external lifetime dependencies and nothrow destruction. It does not duplicate the generic SchemaType or SerialisableType predicates. Stage C inherits both through PrimitiveType rather than defining a second Event-local serialisability rule.

## Ownership boundaries

Event owns family declarations, local admission, subscription state, retained occurrence semantics, bounded pending entitlement, expiry, Listener claims and callback handoff. `EDP-Memory` owns physical occurrence storage/lifetime. `EDP-BoundedTopology` owns compact finite identity/membership/FIFO mechanics. `EDP-Threading` owns Dedicated Threads, their wake path and ordinary-context mutex provider. `EDP-Clock` owns canonical monotonic time.

Event owns no native thread, stack, allocator, codec, route, retry, connection, acknowledgement, transport queue or serialized representation. Schema identity is intrinsic to the Primitive Type; encoding remains outside Event.

## Compile-time family plan

Applications declare local Event deployments, immutable Observe relations and an optional family-wide Shared Pending entitlement. `Event::Planner` normalizes these declarations into the canonical `EDP-Primitives` Family Plan. The Planner derives locally deployed Primitive Types, Listener sets/ordinals, resource dimensions and Composition requirements.

Outbound-only Event Types referenced only by an external Transport binding are not local deployments and consume no Event occurrence storage. They remain schema-bearing Event Primitive Types because schema qualification is not deployment-dependent.

## Runtime synchronization

One local Event Runtime uses one non-recursive ordinary-context mutex. Admission, subscription mutation, pending-recipient mutation, Queue/NewestOnly topology, expiry cleanup, active-borrow accounting and Listener claims linearize through this mutex. Listener callbacks always execute outside the mutex.

## Delivery model

Each retained occurrence contains one immutable Event value and a compact pending-recipient snapshot. Handoff clears the claiming Listener's pending bit and increments an active-borrow count before releasing the mutex. Callback return decrements that borrow. Physical storage is released only when no pending recipient and no active borrow remain.

See also [Delivery and Lifetime](./DELIVERY-AND-LIFETIME.MD) and [Concurrency and ISR](./CONCURRENCY-AND-ISR.MD).
