# Reference — `src/event/Bootstrap.hpp`

**Classification:** PUBLIC BOOTSTRAP API with PRIVATE COMPILE-TIME VALIDATION  
**Source:** [`src/event/Bootstrap.hpp`](../src/event/Bootstrap.hpp)

Bootstrap bridges a normalized Event plan to already-owned Memory, Threading, mutex and typed callback providers. It performs no dynamic topology discovery and owns no external provider lifetime.

# Private validation traits

`ListenerTopologyValid<TThreadingTopology,TListeners>` proves each unique Event Listener identity appears exactly once as a `Threading::DedicatedThread` in the selected Threading topology.

`ListenerRuntimeSurfaceValid<TThreadingRuntime,TListeners>` proves the supplied Threading Runtime exposes `ThreadHandle<TListener>()` for every planned Listener.

`CallbackContractsValid<TArchitecture,TObservations>` resolves each typed callback capability and verifies the selected provider exposes exactly `void OnEvent(const TEvent&) noexcept`.

`BootstrapImpl<TArchitecture,TPlan,TMemoryRuntime,TThreadingRuntime,TCallbackProviders>` is specialized after deriving the exact callback-provider Type list from the plan/Architecture.

# `Detail::BootstrapImpl`

Nested aliases expose `ArchitectureType`, `Plan`, `MemoryRuntime`, `ThreadingRuntime`, selected `MutexProvider`, selected `ThreadingTopology`, `CallbackProviders`, and concrete `RuntimeType`.

Retained state is application-owned dependency references plus one owned Runtime object. No provider is heap-allocated or copied.

## Construction

Constructor arguments are the application Memory Runtime, Threading Runtime, selected Event mutex provider and exact callback providers. Compile-time validation rejects missing/duplicate providers, missing Dedicated Threads, wrong callback signature/noexcept contract and incompatible Threading Runtime surface.

## `Initialize()`

Delegates to Runtime initialization after all compile-time structural checks have succeeded. Returns Event `InitializationResult`; it does not initialize Memory or Threading on behalf of those domains.

## Runtime/provider accessors

`RuntimeInstance()` mutable/const returns the owned Event Runtime. `MemoryRuntimeInstance()`, `ThreadingRuntimeInstance()` and `MutexProviderInstance()` expose the bound owning-domain providers for infrastructure/diagnostic integration without transferring ownership.

# `Bootstrap<TArchitecture,TPlan,TMemoryRuntime,TThreadingRuntime>`

**Classification:** PUBLIC ALIAS.** Derives the callback-provider parameter pack from Architecture and plan, selecting the correctly specialized `BootstrapImpl`. Application construction therefore supplies providers, but does not repeat their Types/order manually.
