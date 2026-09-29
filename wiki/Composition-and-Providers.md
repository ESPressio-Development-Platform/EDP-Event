# Composition and Providers

## Event Composition domain

`Event::Composition::Domain` owns Event-specific capabilities.

`ListenerCallback<TThreadIdentity,TEvent>` is an exclusive capability. Each Observe relation requires exactly one satisfying provider selected from the Event Architecture. Provider validation requires `void OnEvent(const TEvent&) noexcept`.

## Runtime mutex

`RuntimeMutexIdentity` keys one external `EDP-Threading::OrdinaryMutex` capability. Event requires exactly one provider. The concrete Platform mutex remains owned behind Threading; Event has no native synchronization dependency.

## Threading topology

Bootstrap requires exactly one external Threading topology provider and verifies every unique Event Listener identity appears exactly once as a Dedicated Thread. It also checks the supplied Threading Runtime exposes `ThreadHandle<TIdentity>()` for every Listener.

## Provider lifetime

Bootstrap and Runtime retain borrowed pointers/references to application-owned Memory Runtime, Threading Runtime, mutex and callback providers. Those providers must outlive the Event Bootstrap/Runtime. Event does not dynamically allocate provider wrappers.
