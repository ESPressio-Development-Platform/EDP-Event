# Dependency Contracts

Dependency analysis was performed before this Wiki baseline. All listed package edges are mandatory for EDP-Event production headers.

## EDP-System

Consumed for stable `TypeIdentifier` and the Composition Framework. Event defines its own Composition domain/capabilities and resolves exact providers through System Architecture selection.

## EDP-Primitives

Consumed for `Family`, `Topology`, declaration filtering, `FamilyPlan`, `ResourcePlan` and family-planner integration. Event declarations are opaque family declarations to Primitives; the Event Planner normalizes them.

## EDP-Memory

Consumed for exact physical occurrence storage. `OccurrencePoolSpec<TPlan,TEvent>` generates a dedicated-only `ObjectPoolSpec` over the planner-generated occurrence record. Runtime requires exactly matching capacity and uses compact dedicated indices for acquire/address/release.

## EDP-Threading

Consumed for Dedicated Thread topology/handles/wake semantics and `OrdinaryMutex<RuntimeMutexIdentity>`. Event validates every Listener identity resolves to a declared Dedicated Thread and signals that Thread's existing managed wake path.

## EDP-Clock

Consumed for canonical monotonic `Duration`/`MonotonicTimestamp`, clock binding and `MonotonicNow()` used by timed retention. Untimed deployments compile the deadline field out, though Event still depends on Clock at package level for the public retention vocabulary.

## EDP-BoundedTopology

Consumed directly for `BoundedIndex`, `BoundedIndexSet` and `IntrusiveQueue`. These supply occurrence/Listener identity, compact recipient/subscription membership and FIFO topology without taking payload ownership.

## EDP-BoundedTypes

Consumed for recursive memory-boundedness and external-lifetime certification of locally retained Event payload Types.

## Deliberate non-dependencies

Serialization implementation, concrete Transport subsystems, Security, Radio, Mesh and Sockets are external. Platform concrete providers are selected through Memory/Threading and are not direct EDP-Event dependencies.
