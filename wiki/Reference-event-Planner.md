# Reference — `src/event/Planner.hpp`

**Classification:** PUBLIC FAMILY PLANNER plus PRIVATE COMPILE-TIME NORMALIZATION  
**Source:** [`src/event/Planner.hpp`](../src/event/Planner.hpp)

The Planner converts opaque Event-family declarations supplied through `EDP-Primitives::Topology` into one immutable normalized Event plan and canonical `Primitives::FamilyPlan`.

# Private declaration/list utilities

`Detail::IsDeploy`, `IsObserve`, `IsSharedPending` classify family declarations for filtering.

`TypeOrdinal<TList,TType>` computes a dense zero-based ordinal within a compile-time `TypeList`; missing Types intentionally fail instantiation rather than producing a runtime sentinel.

`TypeAt<TList,TIndex>` selects the Type at one compile-time ordinal.

`Filter<TList,TPredicate>` recursively builds a TypeList containing declarations satisfying the predicate.

`DeployEvents<TList>` extracts deployed Event Types. `ObserveListeners<TList>` extracts Listener Thread identities. `ObservedTypesFor<TObservations,TThread>` and `ObserversFor<TObservations,TEvent>` derive the many-to-many Observe topology. `CountObservedEvent` / `CountObservedThread` derive cardinalities.

`FindDeployment<TDeployments,TEvent>` resolves the unique local deployment declaration for one Event Type.

`SharedPendingValue<TList>` maps absence to zero or returns the sole SharedPending capacity. `MaximumUsefulShared<TDeployments>` computes the compile-time maximum Queue overflow that could ever be consumed so provably useless SharedPending declarations can be rejected.

`HasDuplicateEventDeployments`, Primitives duplicate-Type helpers and observation duplicate validation reject ambiguous family topology. `ValidateObservedDeployed` proves every observed Event is locally deployed.

# Composition derivation

`CallbackProviderForObservation<TArchitecture,TObservation>` maps an Observe relation to the exact typed Listener callback provider selected from Architecture.

`RequiredCallbackProviders<TArchitecture,TObservations>` produces the deduplicated callback-provider Type list Runtime/Bootstrap must bind. Provider Type identity rather than positional ordering is used for correctness.

# Resource-plan vocabulary

`OccurrenceInstances<TEvent>`, `DedicatedPendingSlots<TEvent>`, `EligibleListeners<TEvent>`, `SharedPendingSlots`, and `ListenerCount` are semantic resource-dimension tags.

`MakeResourcePlan<TDeployList,TObserveList,TShared>` expands each deployment into decomposed `Primitives::ResourceRequirement` entries plus family-wide Shared Pending and unique Listener count. The plan records semantic dimensions rather than opaque byte totals.

# `Detail::NormalizedEventPlan<TDeclarations>`

**Classification:** INTERNAL PLAN CONTRACT consumed by Runtime/Bootstrap/MemoryPlan.**

Aliases:
- `Declarations` — complete Event family declarations;
- `Deployments`, `Observations`, `SharedDeclarations` — filtered subsets;
- `PrimitiveTypes` — local deployed Event Types only;
- `ListenerDeclarations` — Listener identities including duplicates across observed Types;
- `Listeners` — unique Listener identity set;
- `Resources` — decomposed resource plan.

Constants and variable templates:
- `SharedPendingCapacity` — family-wide logical overflow capacity;
- `MaximumUsefulSharedPending` — static upper bound used to reject wasteful SharedPending;
- `IsDeployed<TEvent>` — local-deployment predicate;
- `EligibleListenerCount<TEvent>` — Type-local Listener cardinality;
- `ListenerOrdinal<TEvent,TThread>` — dense Type-local ordinal used by bitsets;
- `ListenerGlobalOrdinal<TThread>` — stable unique Listener ordinal for family-wide structural use;
- `ObservationOrdinal<TThread,TEvent>` — declaration/provider binding ordinal;
- `IsQueue<TEvent>` — admission-shape predicate;
- `DedicatedPendingCapacity<TEvent>` — Queue dedicated entitlement, zero for NewestOnly;
- `SupportsTimedRetention<TEvent>` — local timed-retention capability.

Aliases `Deployment<TEvent>`, `EligibleListenerTypes<TEvent>`, `ObservedEventTypes<TThread>` expose normalized relationships without runtime tables.

Static assertions enforce unique deployment/Observe/TypeIdentifier contracts, one SharedPending declaration at most, nonzero/useful SharedPending, observed-local deployment and other impossible/wasteful topology constraints.

# `PlanFor<TTopology>`

**Classification:** PUBLIC CONVENIENCE ALIAS.** Resolves Event's normalized runtime plan from a complete `Primitives::Topology` through the canonical family-planner invocation. Applications use this instead of depending on `Primitives::Detail` spelling.

# `RuntimeProvider<TEventPlan>`

**Classification:** INTERNAL FAMILY-PLAN PROVIDER MARKER.** A System Composition provider in the Primitives domain whose nested `EventPlan` carries the normalized Event plan into the generic FamilyPlan.

# `Planner`

**Classification:** PUBLIC FAMILY PLANNER.**

`MakePlan<TFamily,TDeclarations>` validates that `TFamily` is Event::Family, constructs `EventPlan`, and emits `Primitives::FamilyPlan<Family,RuntimeProvider<EventPlan>,PrimitiveTypes,Resources>`. `Plan<TFamily,TDeclarations>` is the canonical alias consumed by EDP-Primitives.
