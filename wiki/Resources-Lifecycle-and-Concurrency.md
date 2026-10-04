# Resources, Lifecycle and Concurrency

## Resource dimensions

Per deployed Type: exact `MaximumInstances`, Queue dedicated pending entitlement, eligible Listener count, admission shape, timed-retention capability and 0..255 ordered remote handoff slots. Family-wide: unique Listener count and optional Shared Pending capacity.

The Planner derives strong index widths, recipient/subscription bitset width, Queue linkage, active-borrow width, unpublished-ingress generations, optional outbound sequencer state and Listener round-robin cursors. Zero remote capacity and other unneeded state compile away where topology proves it unnecessary.

## Initialization

Memory and Threading are initialized by their owning domains first. Event Bootstrap then validates/resolves the already-static Composition and initializes its Runtime. A timed deployment additionally requires the canonical monotonic Clock to be bound.

## Concurrency

One Event Runtime ordinary-context non-recursive mutex protects subscriptions, admission/reclamation coordination, pending topology, recipient mutation, active borrows, Shared Pending accounting, ingress reservation metadata, outbound sequencing, expiry and Listener claims. Listener callbacks, decoder population and outbound adapter calls execute outside the lock and may re-enter Event APIs.

## ISR

No EDP-Event API is ISR-safe in V1. ISR code must defer Event dispatch to ordinary context.

## Shutdown

Event owns no Thread lifecycle. Before integration teardown, the application calls `BeginIntegrationQuiesce()`, stops new decoder/adapter work, releases remaining reservation owners, and waits until `IsIntegrationQuiescent()`. The application then stops/shuts down Threading through Threading APIs. A blocked Dedicated Thread is woken by stop/termination through Threading's managed wake path. Memory teardown occurs only after Event/Threading activity using its occurrence pools has ceased.
