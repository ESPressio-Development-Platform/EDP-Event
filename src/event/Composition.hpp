#pragma once

#include <ESPressio_System.hpp>
#include <ESPressio_Threading.hpp>

#include "Deployment.hpp"

namespace ESPressio::Event::Composition {

    /// Local alias for the shared System Composition Framework namespace.
    namespace Framework = System::CompositionFramework;


    /// Event-owned Composition domain for typed Listener callback capabilities.
    struct Domain final : Framework::Domain {};


    /// Exclusive typed Listener callback capability for one Dedicated Thread/Event relation.
    /// @tparam TThreadIdentity Dedicated Thread identity owning the Listener endpoint.
    /// @tparam TEvent Concrete Event payload Type delivered to the callback.
    template<class TThreadIdentity, class TEvent>
    requires EventType<TEvent>
    struct ListenerCallback final : Framework::ExclusiveCapability<Domain> {

        // Capability identity metadata.

        /// Dedicated Thread identity associated with this callback capability.
        using ThreadIdentity = TThreadIdentity;

        /// Event payload Type associated with this callback capability.
        using Event = TEvent;

    };


    /// Exact same-domain requirement for one typed Listener callback provider.
    /// @tparam TThreadIdentity Dedicated Thread identity owning the Listener endpoint.
    /// @tparam TEvent Concrete Event payload Type delivered to the callback.
    template<class TThreadIdentity, class TEvent>
    requires EventType<TEvent>
    using ListenerCallbackRequirement = Framework::Requirement<
        ListenerCallback<TThreadIdentity, TEvent>,
        Framework::RequirementScope::SameDomain,
        Framework::ExactlyProviders<1U>
    >;


    /// Uniquely resolves one typed Listener callback provider from a Composition/Architecture.
    /// @tparam TThreadIdentity Dedicated Thread identity owning the Listener endpoint.
    /// @tparam TEvent Concrete Event payload Type delivered to the callback.
    /// @tparam TComposition Composition/Architecture from which the provider is selected.
    template<class TThreadIdentity, class TEvent, class TComposition>
    using ListenerCallbackProvider = typename TComposition::template Select<
        ListenerCallbackRequirement<TThreadIdentity, TEvent>,
        Framework::SelectUnique
    >;


    /// Semantic identity separating the Event Runtime mutex from unrelated ordinary mutexes.
    struct RuntimeMutexIdentity final {};


    /// External-domain requirement for exactly one Event Runtime ordinary mutex provider.
    using RuntimeMutexRequirement = Framework::Requirement<
        Threading::OrdinaryMutex<RuntimeMutexIdentity>,
        Framework::RequirementScope::ExternalDomain,
        Framework::ExactlyProviders<1U>
    >;


    /// Uniquely resolves the Event Runtime mutex provider from a Composition/Architecture.
    /// @tparam TComposition Composition/Architecture from which the provider is selected.
    template<class TComposition>
    using RuntimeMutexProvider = typename TComposition::template Select<
        RuntimeMutexRequirement,
        Framework::SelectUnique
    >;


    /// External-domain requirement for exactly one Threading topology provider.
    using ThreadingTopologyRequirement = Framework::Requirement<
        Threading::DedicatedThreadExecution,
        Framework::RequirementScope::ExternalDomain,
        Framework::ExactlyProviders<1U>
    >;


    /// Uniquely resolves the Threading topology provider needed for Listener validation.
    /// @tparam TArchitecture Architecture from which the Threading topology is selected.
    template<class TArchitecture>
    using ThreadingTopologyProvider = typename TArchitecture::template Select<
        ThreadingTopologyRequirement,
        Framework::SelectUnique
    >;

} // ESPressio::Event::Composition
