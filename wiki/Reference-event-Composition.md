# Reference — `src/event/Composition.hpp`

**Classification:** PUBLIC/INTERNAL COMPOSITION API  
**Source:** [`src/event/Composition.hpp`](../src/event/Composition.hpp)

## `Domain`

Event-specific Composition domain separating Event callback capabilities from Threading/System capabilities.

## `ListenerCallback<TThreadIdentity,TEvent>`

Exclusive typed callback capability. Template parameters identify the Listener Dedicated Thread and concrete Event Type. Nested aliases expose both identities for validation. Exactly one provider is required for each Observe relation.

`ListenerCallbackRequirement<TThreadIdentity,TEvent>` is SameDomain with `ExactlyProviders<1>`. `ListenerCallbackProvider<...,TComposition>` performs unique provider selection from a Composition/Architecture.

## `RuntimeMutexIdentity`

Semantic key for the one ordinary-context Event Runtime mutex. It prevents accidental aliasing with unrelated Threading mutex consumers.

`RuntimeMutexRequirement` requires exactly one external-domain `Threading::OrdinaryMutex<RuntimeMutexIdentity>`. `RuntimeMutexProvider<TComposition>` selects it uniquely.

## Threading topology requirement

`ThreadingTopologyRequirement` requires exactly one external Threading topology capability. `ThreadingTopologyProvider<TArchitecture>` uniquely selects it so Bootstrap can prove every Event Listener identity is a declared Dedicated Thread.
