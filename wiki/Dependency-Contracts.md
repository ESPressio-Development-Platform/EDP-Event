# Dependency Contracts

Dependency analysis was performed before this Wiki baseline. All listed package edges are mandatory for EDP-Event production headers.

## EDP-System

Consumed for stable `TypeIdentifier`, `FieldIdentifier`, `FieldBinding`, `FieldSet`, `SchemaType`, schema traversal, and the Composition Framework. Event defines its own Composition domain/capabilities and resolves exact providers through System Architecture selection.

The Serialisation-prerequisite tranche also exposed and corrected a substitution-safety defect in `System::IdentifiedType`: non-schema Types probed during constrained overload resolution must make the concept evaluate `false`, not hard-error. The matching Event prerequisite branch is therefore validated against `EDP-System/serialisation_prerequisites` until that correction is integrated upstream.

## EDP-Primitives

Event consumes the canonical Primitive Family/Topology/FamilyPlan framework and the shared `ExecutionDomain::{LocalOnly, RemoteOnly, LocalAndRemote}` scope vocabulary. Event re-exports the scope Types for namespace consistency but does not own duplicate definitions.

Every Event first satisfies `Primitives::PrimitiveType`, which now requires `System::SchemaType`. Event therefore inherits universal Type identity and canonical Field schema through Primitives rather than defining a duplicate Event-local schema rule. The Event Planner continues to normalize opaque Event-family declarations into the common Family Plan.

On `EDP-Event/serialisation_prerequisites`, CI intentionally consumes `EDP-Primitives/serialisation_prerequisites`; `main` continues to consume Primitives `main`.

## EDP-Memory

Consumed for exact physical occurrence storage. `OccurrencePoolSpec<TPlan,TEvent>` generates a dedicated-only `ObjectPoolSpec` over the planner-generated occurrence record. Runtime requires exactly matching capacity and uses compact dedicated indices for acquire/address/release. EDP-Event pins EDP-Memory to `main`, where this compact indexed-pool seam is integrated.

## EDP-Threading

Consumed for Dedicated Thread topology/handles/wake semantics and `OrdinaryMutex<RuntimeMutexIdentity>`. Event validates every Listener identity resolves to a declared Dedicated Thread and signals that Thread's existing managed wake path. EDP-Event pins EDP-Threading to `main`, where the public Dedicated-Thread wake/wait and keyed ordinary-mutex seam is integrated.

## EDP-Clock

Consumed for canonical monotonic `Duration`/`MonotonicTimestamp`, clock binding and `MonotonicNow()` used by timed retention. Untimed deployments compile the deadline field out, though Event still depends on Clock at package level for the public retention vocabulary.

## EDP-BoundedTopology

Consumed directly for `BoundedIndex`, `BoundedIndexSet` and `IntrusiveQueue`. These supply occurrence/Listener identity, compact recipient/subscription membership and FIFO topology without taking payload ownership.

## EDP-BoundedTypes

Consumed for recursive memory-boundedness and external-lifetime certification of locally retained Event payload Types. These are Event-specific retained-value constraints layered on top of the generic Primitive schema contract.

## Deliberate non-dependencies

Serialisation implementation, concrete Transport subsystems, Security, Radio, Mesh and Sockets are external. Platform concrete providers are selected through Memory/Threading and are not direct EDP-Event dependencies.

The schema migration introduces no new runtime dependency edge.
