#pragma once

#include <ESPressio_System.hpp>
#include <ESPressio_Threading.hpp>

#include "Deployment.hpp"

namespace ESPressio::Event::Composition {

    namespace Framework = System::CompositionFramework;

    struct Domain final : Framework::Domain {};

    template<class TThreadIdentity, class TEvent>
    requires EventType<TEvent>
    struct ListenerCallback final : Framework::ExclusiveCapability<Domain> {
        using ThreadIdentity = TThreadIdentity;
        using Event = TEvent;
    };

    template<class TThreadIdentity, class TEvent>
    requires EventType<TEvent>
    using ListenerCallbackRequirement = Framework::Requirement<
        ListenerCallback<TThreadIdentity, TEvent>,
        Framework::RequirementScope::SameDomain,
        Framework::ExactlyProviders<1U>
    >;

    template<class TThreadIdentity, class TEvent, class TComposition>
    using ListenerCallbackProvider = typename TComposition::template Select<
        ListenerCallbackRequirement<TThreadIdentity, TEvent>,
        Framework::SelectUnique
    >;

    struct RuntimeMutexIdentity final {};

    using RuntimeMutexRequirement = Framework::Requirement<
        Threading::OrdinaryMutex<RuntimeMutexIdentity>,
        Framework::RequirementScope::ExternalDomain,
        Framework::ExactlyProviders<1U>
    >;

    template<class TComposition>
    using RuntimeMutexProvider = typename TComposition::template Select<
        RuntimeMutexRequirement,
        Framework::SelectUnique
    >;

} // ESPressio::Event::Composition
