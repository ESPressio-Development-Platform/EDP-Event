# Concurrency and ISR Boundary

## Event Runtime synchronization domain

One local Event Runtime has one topology-selected, ordinary-context, non-recursive mutex supplied through EDP-Threading. It protects all mutable Event-core state: subscriptions, occurrence claim/reclamation coordination, Queue linkage, pending-recipient changes, NewestOnly supersession, active-borrow transitions, shared-pending accounting and expiry cleanup.

Concurrent same-Type Dispatch calls are ordered by their mutex linearization. The implementation does not thereby promise semantic ordering between different Event Types.

## Callbacks and reentrancy

Callbacks execute outside the Event mutex. A callback may Dispatch, Subscribe or Unsubscribe and the host tests deliberately verify reentrant Dispatch with a mutex implementation that rejects recursive acquisition.

## Dedicated Thread wake semantics

Event uses the existing Threading-owned Dedicated Thread wake path. A wake means only that the Thread should recheck its work sources and lifecycle state; it is not an Event count or Event-specific condition variable. Event stores no wake slot or wake counter.

A multi-purpose Dedicated Thread may block indefinitely through `ThreadContext::Wait()`. Every path that can require that Thread's attention—including Event admission, other application work, cooperative stop and infrastructure termination—must publish the same advisory wake. On return from Wait, the Thread rechecks all responsibilities.

## ISR boundary

EDP-Event is categorically ordinary-context only. ISR code must not Dispatch, Subscribe, Unsubscribe, Drain or touch Event Runtime synchronization. An ISR may perform immediate peripheral/timestamp/reset work permitted by its owning domain, then hand control to ordinary context which emits the Event.

This avoids hidden ISR-safe allocation/synchronization requirements and keeps one deterministic Event state machine.
