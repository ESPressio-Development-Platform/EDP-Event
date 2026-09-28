#pragma once

#include <concepts>
#include <type_traits>

#include <ESPressio_Threading.hpp>

#include "Composition.hpp"
#include "Planner.hpp"
#include "Runtime.hpp"

namespace ESPressio::Event {

    namespace Detail {

        /// Compile-time predicate proving every Event Listener identity exists as one Dedicated Thread.
        /// @tparam TThreadingTopology Selected Threading topology provider Type.
        /// @tparam TListeners Unique Event Listener identity TypeList.
        template<class TThreadingTopology, class TListeners>
        struct ListenerTopologyValid;


        /// TypeList specialization evaluating every Listener DedicatedThreadRequirement.
        /// @tparam TThreadingTopology Selected Threading topology provider Type.
        /// @tparam TListeners Event Listener identity Types.
        template<class TThreadingTopology, class... TListeners>
        struct ListenerTopologyValid<
            TThreadingTopology,
            Primitives::TypeList<TListeners...>
        > : std::bool_constant<
            (
                Threading::SatisfiesThreadingRequirement<
                    TThreadingTopology,
                    Threading::DedicatedThreadRequirement<TListeners>
                > &&
                ... &&
                true
            )
        > {};


        /// Compile-time predicate proving the supplied Threading Runtime exposes wake-capable Listener handles.
        /// @tparam TThreadingRuntime Concrete Threading Runtime Type bound by the application.
        /// @tparam TListeners Unique Event Listener identity TypeList.
        template<class TThreadingRuntime, class TListeners>
        struct ListenerRuntimeSurfaceValid;


        /// TypeList specialization validating every Listener handle's Wake operation.
        /// @tparam TThreadingRuntime Concrete Threading Runtime Type bound by the application.
        /// @tparam TListeners Event Listener identity Types.
        template<class TThreadingRuntime, class... TListeners>
        struct ListenerRuntimeSurfaceValid<
            TThreadingRuntime,
            Primitives::TypeList<TListeners...>
        > : std::bool_constant<
            (
                requires(TThreadingRuntime& runtime) {
                    {
                        runtime.template ThreadHandle<TListeners>().Wake()
                    } -> std::same_as<Threading::ThreadWakeResult>;
                } &&
                ... &&
                true
            )
        > {};


        /// Compile-time predicate proving every Observe relation resolves to the required typed noexcept callback.
        /// @tparam TArchitecture Application Composition Architecture containing Event callback providers.
        /// @tparam TObservations Immutable Event Observe declaration TypeList.
        template<class TArchitecture, class TObservations>
        struct CallbackContractsValid;


        /// TypeList specialization validating every typed callback provider contract.
        /// @tparam TArchitecture Application Composition Architecture containing Event callback providers.
        /// @tparam TObservations Event Observe declaration Types.
        template<class TArchitecture, class... TObservations>
        struct CallbackContractsValid<
            TArchitecture,
            Primitives::TypeList<TObservations...>
        > : std::bool_constant<
            (
                requires(
                    Composition::ListenerCallbackProvider<
                        typename TObservations::ThreadIdentity,
                        typename TObservations::Event,
                        TArchitecture
                    >& provider,
                    const typename TObservations::Event& event
                ) {
                    {
                        provider.OnEvent(event)
                    } noexcept -> std::same_as<void>;
                } &&
                ... &&
                true
            )
        > {};


        /// Primary declaration for the provider-pack-specialized Event Bootstrap implementation.
        /// @tparam TArchitecture Application Composition Architecture.
        /// @tparam TPlan Normalized Event plan.
        /// @tparam TMemoryRuntime Application-owned EDP-Memory Runtime Type.
        /// @tparam TThreadingRuntime Application-owned EDP-Threading Runtime Type.
        /// @tparam TCallbackProviders TypeList of callback providers derived from Observe relations.
        template<
            class TArchitecture,
            class TPlan,
            class TMemoryRuntime,
            class TThreadingRuntime,
            class TCallbackProviders
        >
        class BootstrapImpl;


        /// Binds all planner-derived Event providers and owns the one Event Runtime object.
        /// @tparam TArchitecture Application Composition Architecture.
        /// @tparam TPlan Normalized Event plan.
        /// @tparam TMemoryRuntime Application-owned EDP-Memory Runtime Type.
        /// @tparam TThreadingRuntime Application-owned EDP-Threading Runtime Type.
        /// @tparam TCallbackProviders Callback provider Types resolved from the Architecture.
        template<
            class TArchitecture,
            class TPlan,
            class TMemoryRuntime,
            class TThreadingRuntime,
            class... TCallbackProviders
        >
        class BootstrapImpl<
            TArchitecture,
            TPlan,
            TMemoryRuntime,
            TThreadingRuntime,
            Primitives::TypeList<TCallbackProviders...>
        > final {

            public:

                // Public topology/provider metadata.

                /// Application Composition Architecture Type used for all provider resolution.
                using ArchitectureType = TArchitecture;

                /// Normalized Event plan owned by this Bootstrap.
                using Plan = TPlan;

                /// Application-owned Memory Runtime Type borrowed by Event.
                using MemoryRuntime = TMemoryRuntime;

                /// Application-owned Threading Runtime Type borrowed by Event.
                using ThreadingRuntime = TThreadingRuntime;

                /// Uniquely selected Threading ordinary-mutex provider for Event Runtime synchronization.
                using MutexProvider = Composition::RuntimeMutexProvider<TArchitecture>;

                /// Uniquely selected Threading topology provider used to validate Listener identities.
                using ThreadingTopology = Composition::ThreadingTopologyProvider<TArchitecture>;

                /// Exact callback provider TypeList derived from Observe declarations.
                using CallbackProviders = Primitives::TypeList<TCallbackProviders...>;

                /// Concrete Event Runtime Type bound to all selected providers.
                using RuntimeType = Runtime<
                    TPlan,
                    TArchitecture,
                    TMemoryRuntime,
                    TThreadingRuntime,
                    MutexProvider,
                    TCallbackProviders...
                >;

                static_assert(
                    TArchitecture::IsValid,
                    "Event Bootstrap requires a valid EDP-System Architecture"
                );

                static_assert(
                    ListenerTopologyValid<
                        ThreadingTopology,
                        typename TPlan::Listeners
                    >::value,
                    "Every Event Listener identity must resolve to one statically declared Dedicated Thread"
                );

                static_assert(
                    ListenerRuntimeSurfaceValid<
                        TThreadingRuntime,
                        typename TPlan::Listeners
                    >::value,
                    "Event Bootstrap requires a Threading runtime exposing Wake-capable handles for every Listener Dedicated Thread"
                );

                static_assert(
                    CallbackContractsValid<
                        TArchitecture,
                        typename TPlan::Observations
                    >::value,
                    "Every Event Observe relation requires one unique void OnEvent(const TEvent&) noexcept provider"
                );

            private:

                // Borrowed owning-domain providers and owned Event Runtime.

                /// Application-owned Memory Runtime; must outlive this Bootstrap.
                TMemoryRuntime* _memory;

                /// Application-owned Threading Runtime; must outlive this Bootstrap.
                TThreadingRuntime* _threading;

                /// Application-owned Event Runtime mutex provider; must outlive this Bootstrap.
                MutexProvider* _mutex;

                /// Event-owned bounded Runtime retaining subscriptions/admission/delivery state.
                RuntimeType _runtime;

            public:

                // Construction and stable ownership.

                /// Binds already-owned infrastructure/callback providers and constructs the Event Runtime.
                /// @param memory Initialized-or-initializable Memory Runtime containing exact Event occurrence pools.
                /// @param threading Threading Runtime containing every planned Listener Dedicated Thread.
                /// @param mutex Selected ordinary-context Event Runtime mutex provider.
                /// @param callbacks Exact typed callback provider objects derived from Observe relations.
                BootstrapImpl(
                    TMemoryRuntime& memory,
                    TThreadingRuntime& threading,
                    MutexProvider& mutex,
                    TCallbackProviders&... callbacks
                ) noexcept :
                    _memory(&memory),
                    _threading(&threading),
                    _mutex(&mutex),
                    _runtime(
                        memory,
                        threading,
                        mutex,
                        callbacks...
                    ) {
                }

                /// Bootstrap retains stable provider bindings and therefore cannot be copied.
                BootstrapImpl(const BootstrapImpl&) = delete;

                /// Bootstrap retains stable provider bindings and therefore cannot be copy-assigned.
                BootstrapImpl& operator=(const BootstrapImpl&) = delete;

                /// Bootstrap retains stable provider bindings and therefore cannot be moved.
                BootstrapImpl(BootstrapImpl&&) = delete;

                /// Bootstrap retains stable provider bindings and therefore cannot be move-assigned.
                BootstrapImpl& operator=(BootstrapImpl&&) = delete;

                // Runtime lifecycle.

                /// Initializes Event over already-initialized owning-domain providers.
                [[nodiscard]] InitializationResult Initialize() noexcept {
                    return _runtime.Initialize();
                }

                // Bound-provider access.

                /// Returns the mutable Event Runtime owned by this Bootstrap.
                [[nodiscard]] RuntimeType& RuntimeInstance() noexcept {
                    return _runtime;
                }

                /// Returns the immutable Event Runtime owned by this Bootstrap.
                [[nodiscard]] const RuntimeType& RuntimeInstance() const noexcept {
                    return _runtime;
                }

                /// Returns the borrowed application Memory Runtime.
                [[nodiscard]] TMemoryRuntime& MemoryRuntimeInstance() noexcept {
                    return *_memory;
                }

                /// Returns the borrowed application Threading Runtime.
                [[nodiscard]] TThreadingRuntime& ThreadingRuntimeInstance() noexcept {
                    return *_threading;
                }

                /// Returns the borrowed Event Runtime ordinary-mutex provider.
                [[nodiscard]] MutexProvider& MutexProviderInstance() noexcept {
                    return *_mutex;
                }

        };

    } // ESPressio::Event::Detail


    /// Public Event Bootstrap alias deriving the exact callback-provider pack from Architecture and Event plan.
    /// @tparam TArchitecture Application Composition Architecture.
    /// @tparam TPlan Normalized Event plan.
    /// @tparam TMemoryRuntime Application-owned Memory Runtime Type.
    /// @tparam TThreadingRuntime Application-owned Threading Runtime Type.
    template<
        class TArchitecture,
        class TPlan,
        class TMemoryRuntime,
        class TThreadingRuntime
    >
    using Bootstrap = Detail::BootstrapImpl<
        TArchitecture,
        TPlan,
        TMemoryRuntime,
        TThreadingRuntime,
        typename Detail::RequiredCallbackProviders<
            TArchitecture,
            typename TPlan::Observations
        >::Type
    >;

} // ESPressio::Event
