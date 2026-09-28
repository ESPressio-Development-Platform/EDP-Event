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
        using DedicatedIndex = BoundedTopology::BoundedIndex<
            PoolIndexSpace<TObject>,
            TObject::MaximumInstances
        >;
    };

    template<class TObject>
    struct CorrectSpec final {
        using Dedicated = Memory::DedicatedInstances<TObject::MaximumInstances>;
        using Shared = Memory::NoSharedOverflow;
    };

    struct FakeMemoryRuntime final {
        template<class TObject>
        using ObjectPoolType = DummyPool<TObject>;

        struct Topology final {
            template<class TObject>
            static constexpr bool ContainsObjectPool = true;
            template<class TObject>
            using ObjectPoolSpecFor = CorrectSpec<TObject>;
        };

        bool IsInitialized() const noexcept { return true; }
    };

    template<class TThread>
    struct FakeThread final {
        Threading::ThreadWakeResult Wake() noexcept {
            return Threading::ThreadWakeResult::Woken;
        }
    };

    struct FakeThreadingRuntime final {
        template<class TThread>
        FakeThread<TThread> ThreadHandle() noexcept { return {}; }
    };

    struct MutexProvider final : CF::Provider<
        Threading::Domain,
        CF::Offers<
            CF::Offer<Threading::OrdinaryMutex<Event::Composition::RuntimeMutexIdentity>>
        >
    > {
        Threading::OrdinaryMutexAcquireResult Acquire() noexcept {
            return Threading::OrdinaryMutexAcquireResult::Acquired;
        }
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
