#pragma once

#include <cstddef>

#include "EventTestSupport.hpp"

namespace EventBootstrapSupport {

    namespace Event = ESPressio::Event;
    namespace Memory = ESPressio::Memory;
    namespace Primitives = ESPressio::Primitives;
    namespace Threading = ESPressio::Threading;
    namespace BoundedTopology = ESPressio::BoundedTopology;
    namespace CF = ESPressio::System::CompositionFramework;

    using ListenerA = EventTestSupport::ListenerA;
    using ListenerB = EventTestSupport::ListenerB;
    using TestEvent = EventTestSupport::EventValue<1U>;

    using PrimitiveTopology = Primitives::Topology<
        Event::Deploy<TestEvent, 2U, Event::Queue<1U>, Event::UntilHandoffOnly>,
        Event::Observe<ListenerA, TestEvent>,
        Event::SharedPending<1U>
    >;
    using FamilyPlan = typename Primitives::Detail::InvokeFamilyPlanner<
        Event::Family,
        typename PrimitiveTopology::Deployments
    >::Type;
    using Plan = typename FamilyPlan::RuntimeProvider::EventPlan;

    template<class TObject>
    struct PoolIndexSpace final {};

    template<class TObject>
    struct DummyPool final {
        /// Strong fake pool slot identity required by the Memory contract.
        using DedicatedIndex = BoundedTopology::BoundedIndex<
            PoolIndexSpace<TObject>,
            TObject::MaximumInstances
        >;
    };

    template<class TObject>
    struct CorrectSpec final {
        /// Exact dedicated instance capacity matching the occurrence record.
        using Dedicated = Memory::DedicatedInstances<TObject::MaximumInstances>;
        /// Confirms no raw shared overflow is available to Event pools.
        using Shared = Memory::NoSharedOverflow;
    };

    struct FakeMemoryRuntime final {
        template<class TObject>
        /// Maps requested occurrence Types to the fake pool contract.
        using ObjectPoolType = DummyPool<TObject>;

        struct Topology final {
            template<class TObject>
            /// Advertises the requested Event occurrence pool to Runtime validation.
            static constexpr bool ContainsObjectPool = true;
            template<class TObject>
            /// Exposes the exact fake pool specification for compile-time validation.
            using ObjectPoolSpecFor = CorrectSpec<TObject>;
        };

        /// Reports the fake Memory Runtime as initialized.
        bool IsInitialized() const noexcept { return true; }
    };

    template<class TThread>
    struct FakeThread final {
        /// Simulates a successful Dedicated Thread wake.
        Threading::ThreadWakeResult Wake() noexcept {
            return Threading::ThreadWakeResult::Woken;
        }
    };

    struct FakeThreadingRuntime final {
        template<class TThread>
        /// Returns a fake wake-capable Thread handle for the requested identity.
        FakeThread<TThread> ThreadHandle() noexcept { return {}; }
    };

    struct MutexProvider final : CF::Provider<
        Threading::Domain,
        CF::Offers<
            CF::Offer<Threading::OrdinaryMutex<Event::Composition::RuntimeMutexIdentity>>
        >
    > {
        /// Simulates successful acquisition of the Event ordinary mutex.
        Threading::OrdinaryMutexAcquireResult Acquire() noexcept {
            return Threading::OrdinaryMutexAcquireResult::Acquired;
        }
        /// Simulates successful release of the Event ordinary mutex.
        Threading::OrdinaryMutexReleaseResult Release() noexcept {
            return Threading::OrdinaryMutexReleaseResult::Released;
        }
    };

    struct GoodHandler final : CF::Provider<
        Event::Composition::Domain,
        CF::Offers<
            CF::Offer<Event::Composition::ListenerCallback<ListenerA, TestEvent>>
        >
    > {
        /// Satisfies the typed Event Listener callback contract for Bootstrap compile tests.
        void OnEvent(const TestEvent&) noexcept {}
    };

    using GoodThreadingTopology = Threading::ThreadingTopology<
        Threading::DedicatedThread<ListenerA>
    >;
    using GoodEventComposition = CF::Composition<
        Event::Composition::Domain,
        GoodHandler
    >;
    using GoodThreadingComposition = CF::Composition<
        Threading::Domain,
        GoodThreadingTopology,
        MutexProvider
    >;
    using GoodArchitecture = CF::Architecture<
        GoodEventComposition,
        GoodThreadingComposition
    >;
    using GoodBootstrap = Event::Bootstrap<
        GoodArchitecture,
        Plan,
        FakeMemoryRuntime,
        FakeThreadingRuntime
    >;

} // EventBootstrapSupport
