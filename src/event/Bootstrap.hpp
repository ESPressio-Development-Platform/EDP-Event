#pragma once

#include <concepts>
#include <type_traits>

#include <ESPressio_Threading.hpp>

#include "Composition.hpp"
#include "Planner.hpp"
#include "Runtime.hpp"

namespace ESPressio::Event {

    namespace Detail {

        template<class TThreadingTopology, class TListeners>
        struct ListenerTopologyValid;

        template<class TThreadingTopology, class... TListeners>
        struct ListenerTopologyValid<TThreadingTopology, Primitives::TypeList<TListeners...>> : std::bool_constant<
            (
                Threading::SatisfiesThreadingRequirement<
                    TThreadingTopology,
                    Threading::DedicatedThreadRequirement<TListeners>
                > &&
                ... &&
                true
            )
        > {};

        template<class TThreadingRuntime, class TListeners>
        struct ListenerRuntimeSurfaceValid;

        template<class TThreadingRuntime, class... TListeners>
        struct ListenerRuntimeSurfaceValid<TThreadingRuntime, Primitives::TypeList<TListeners...>> : std::bool_constant<
            (
                requires(TThreadingRuntime& runtime) {
                    { runtime.template ThreadHandle<TListeners>().Wake() } -> std::same_as<Threading::ThreadWakeResult>;
                } &&
                ... &&
                true
            )
        > {};

        template<class TArchitecture, class TObservations>
        struct CallbackContractsValid;

        template<class TArchitecture, class... TObservations>
        struct CallbackContractsValid<TArchitecture, Primitives::TypeList<TObservations...>> : std::bool_constant<
            (
                requires(
                    Composition::ListenerCallbackProvider<
                        typename TObservations::ThreadIdentity,
                        typename TObservations::Event,
                        TArchitecture
                    >& provider,
                    const typename TObservations::Event& event
                ) {
                    { provider.OnEvent(event) } noexcept -> std::same_as<void>;
                } &&
                ... &&
                true
            )
        > {};

        template<
            class TArchitecture,
            class TPlan,
            class TMemoryRuntime,
            class TThreadingRuntime,
            class TCallbackProviders
        >
        class BootstrapImpl;

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
            using ArchitectureType = TArchitecture;
            using Plan = TPlan;
            using MemoryRuntime = TMemoryRuntime;
            using ThreadingRuntime = TThreadingRuntime;
            using MutexProvider = Composition::RuntimeMutexProvider<TArchitecture>;
            using ThreadingTopology = Composition::ThreadingTopologyProvider<TArchitecture>;
            using CallbackProviders = Primitives::TypeList<TCallbackProviders...>;
            using RuntimeType = Runtime<
                TPlan,
                TArchitecture,
                TMemoryRuntime,
                TThreadingRuntime,
                MutexProvider,
                TCallbackProviders...
            >;

            static_assert(TArchitecture::IsValid,
                "Event Bootstrap requires a valid EDP-System Architecture");
            static_assert(
                ListenerTopologyValid<ThreadingTopology, typename TPlan::Listeners>::value,
                "Every Event Listener identity must resolve to one statically declared Dedicated Thread"
            );
            static_assert(
                ListenerRuntimeSurfaceValid<TThreadingRuntime, typename TPlan::Listeners>::value,
                "Event Bootstrap requires a Threading runtime exposing Wake-capable handles for every Listener Dedicated Thread"
            );
            static_assert(
                CallbackContractsValid<TArchitecture, typename TPlan::Observations>::value,
                "Every Event Observe relation requires one unique void OnEvent(const TEvent&) noexcept provider"
            );

        private:
            TMemoryRuntime* _memory;
            TThreadingRuntime* _threading;
            MutexProvider* _mutex;
            RuntimeType _runtime;

        public:
            BootstrapImpl(
                TMemoryRuntime& memory,
                TThreadingRuntime& threading,
                MutexProvider& mutex,
                TCallbackProviders&... callbacks
            ) noexcept :
                _memory(&memory),
                _threading(&threading),
                _mutex(&mutex),
                _runtime(memory, threading, mutex, callbacks...) {
            }

            BootstrapImpl(const BootstrapImpl&) = delete;
            BootstrapImpl& operator=(const BootstrapImpl&) = delete;
            BootstrapImpl(BootstrapImpl&&) = delete;
            BootstrapImpl& operator=(BootstrapImpl&&) = delete;

            [[nodiscard]] InitializationResult Initialize() noexcept {
                return _runtime.Initialize();
            }

            [[nodiscard]] RuntimeType& RuntimeInstance() noexcept {
                return _runtime;
            }

            [[nodiscard]] const RuntimeType& RuntimeInstance() const noexcept {
                return _runtime;
            }

            [[nodiscard]] TMemoryRuntime& MemoryRuntimeInstance() noexcept {
                return *_memory;
            }

            [[nodiscard]] TThreadingRuntime& ThreadingRuntimeInstance() noexcept {
                return *_threading;
            }

            [[nodiscard]] MutexProvider& MutexProviderInstance() noexcept {
                return *_mutex;
            }
        };

    } // Event::Detail

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
        typename Detail::RequiredCallbackProviders<TArchitecture, typename TPlan::Observations>::Type
    >;

} // ESPressio::Event
