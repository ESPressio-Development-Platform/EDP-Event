#include <array>
#include <cassert>
#include <cstdint>
#include <optional>
#include <tuple>
#include <type_traits>

#include <ESPressio_Event.hpp>

namespace Test {
    namespace Event = ESPressio::Event;
    namespace Primitives = ESPressio::Primitives;
    namespace BoundedTopology = ESPressio::BoundedTopology;
    namespace Memory = ESPressio::Memory;
    namespace Threading = ESPressio::Threading;
    namespace Clock = ESPressio::Clock;
    namespace CF = ESPressio::System::CompositionFramework;

    struct ListenerA {};
    struct ListenerB {};

    struct QueueEvent final {
        inline static constexpr ESPressio::System::TypeIdentifier Identifier{
            ESPressio::System::TypeIdentifier::Storage{0x00,0x00,0x01,0x00,0x00,0x00,0x20,0x01}
        };
        using Family = Event::Family;
        int Value{};
    };

    struct LatestEvent final {
        inline static constexpr ESPressio::System::TypeIdentifier Identifier{
            ESPressio::System::TypeIdentifier::Storage{0x00,0x00,0x01,0x00,0x00,0x00,0x20,0x02}
        };
        using Family = Event::Family;
        int Value{};
    };

    struct TestClock final {
        mutable std::uint64_t NowNs{100U};
        Clock::MonotonicTimestamp Now() const noexcept {
            return Clock::MonotonicTimestamp::FromNanoseconds(NowNs);
        }
    };
}

namespace ESPressio::Bounded {
    template<> struct MemoryBoundedTraits<Test::QueueEvent> : MemoryBoundedValueDeclaration<false, int> {};
    template<> struct MemoryBoundedTraits<Test::LatestEvent> : MemoryBoundedValueDeclaration<false, int> {};
}

namespace Test {
    using PrimitiveTopology = Primitives::Topology<
        Event::Deploy<QueueEvent, 3U, Event::Queue<1U>, Event::TimedRetention>,
        Event::Deploy<LatestEvent, 2U, Event::NewestOnly, Event::UntilHandoffOnly>,
        Event::Observe<ListenerA, QueueEvent>,
        Event::Observe<ListenerB, QueueEvent>,
        Event::Observe<ListenerA, LatestEvent>,
        Event::SharedPending<2U>
    >;

    using FamilyPlan = typename Primitives::Detail::InvokeFamilyPlanner<
        Event::Family,
        typename PrimitiveTopology::Deployments
    >::Type;
    using Plan = typename FamilyPlan::RuntimeProvider::EventPlan;

    template<class TObject>
    struct PoolIndexSpace {};

    template<class TObject, std::size_t TCapacity>
    class FakePool final {
    public:
        using DedicatedIndex = BoundedTopology::BoundedIndex<PoolIndexSpace<TObject>, TCapacity>;
    private:
        std::array<std::optional<TObject>, TCapacity> _objects{};
    public:
        template<class... TArgs>
        Memory::DedicatedObjectPoolAcquisitionResult AcquireDedicated(
            DedicatedIndex& output,
            TArgs&&... args
        ) noexcept {
            if (output.IsValid()) return Memory::DedicatedObjectPoolAcquisitionResult::OutputIndexOccupied;
            for (std::size_t i = 0U; i < TCapacity; ++i) {
                if (!_objects[i].has_value()) {
                    _objects[i].emplace(std::forward<TArgs>(args)...);
                    output = DedicatedIndex::FromUnchecked(i);
                    return Memory::DedicatedObjectPoolAcquisitionResult::Succeeded;
                }
            }
            return Memory::DedicatedObjectPoolAcquisitionResult::CapacityUnavailable;
        }

        TObject& DedicatedObject(DedicatedIndex index) noexcept {
            return *_objects[static_cast<std::size_t>(index.Value())];
        }
        const TObject& DedicatedObject(DedicatedIndex index) const noexcept {
            return *_objects[static_cast<std::size_t>(index.Value())];
        }
        Memory::DedicatedObjectPoolReleaseResult ReleaseDedicated(DedicatedIndex& index) noexcept {
            if (!index.IsValid()) return Memory::DedicatedObjectPoolReleaseResult::InvalidIndex;
            auto& slot = _objects[static_cast<std::size_t>(index.Value())];
            if (!slot.has_value()) return Memory::DedicatedObjectPoolReleaseResult::SlotNotOwned;
            slot.reset();
            index = DedicatedIndex::Invalid();
            return Memory::DedicatedObjectPoolReleaseResult::Released;
        }
    };

    template<class TObject>
    struct FakeSpec final {
        using Dedicated = Memory::DedicatedInstances<TObject::MaximumInstances>;
        using Shared = Memory::NoSharedOverflow;
    };

    template<class TPlan, class TList>
    class FakeMemoryRuntimeImpl;

    template<class TPlan, class... TEvents>
    class FakeMemoryRuntimeImpl<TPlan, Primitives::TypeList<TEvents...>> final {
        using Pools = std::tuple<
            FakePool<Event::OccurrenceRecord<TPlan, TEvents>, Event::OccurrenceRecord<TPlan, TEvents>::MaximumInstances>...
        >;
        Pools _pools{};
    public:
        template<class TObject>
        using ObjectPoolType = FakePool<TObject, TObject::MaximumInstances>;

        struct Topology final {
            template<class TObject>
            static constexpr bool ContainsObjectPool = true;
            template<class TObject>
            using ObjectPoolSpecFor = FakeSpec<TObject>;
        };
        bool IsInitialized() const noexcept { return true; }
        template<class TObject>
        auto& ObjectPoolFor() noexcept {
            return std::get<FakePool<TObject, TObject::MaximumInstances>>(_pools);
        }
    };

    using FakeMemoryRuntime = FakeMemoryRuntimeImpl<Plan, typename Plan::PrimitiveTypes>;

    template<class TThread>
    struct WakeCounter final { inline static int Count = 0; };

    template<class TThread>
    struct FakeThread final {
        Threading::ThreadWakeResult Wake() noexcept {
            ++WakeCounter<TThread>::Count;
            return Threading::ThreadWakeResult::Woken;
        }
    };

    struct FakeThreadingRuntime final {
        template<class TThread>
        FakeThread<TThread> ThreadHandle() noexcept { return {}; }
    };

    struct FakeMutex final : CF::Provider<
        Threading::Domain,
        CF::Offers<
            CF::Offer<Threading::OrdinaryMutex<Event::Composition::RuntimeMutexIdentity>>
        >
    > {
        bool Held{false};
        Threading::OrdinaryMutexAcquireResult Acquire() noexcept {
            if (Held) return Threading::OrdinaryMutexAcquireResult::ProviderFailure;
            Held = true;
            return Threading::OrdinaryMutexAcquireResult::Acquired;
        }
        Threading::OrdinaryMutexReleaseResult Release() noexcept {
            if (!Held) return Threading::OrdinaryMutexReleaseResult::ProviderFailure;
            Held = false;
            return Threading::OrdinaryMutexReleaseResult::Released;
        }
    };

    struct ListenerAHandler final : CF::Provider<
        Event::Composition::Domain,
        CF::Offers<
            CF::Offer<Event::Composition::ListenerCallback<ListenerA, QueueEvent>>,
            CF::Offer<Event::Composition::ListenerCallback<ListenerA, LatestEvent>>
        >
    > {
        using QueueHook = void (*)(void*, const QueueEvent&) noexcept;
        using LatestHook = void (*)(void*, const LatestEvent&) noexcept;

        int QueueSum{0};
        int LatestLast{0};
        void* QueueContext{nullptr};
        QueueHook QueueAfter{nullptr};
        void* LatestContext{nullptr};
        LatestHook LatestAfter{nullptr};

        void OnEvent(const QueueEvent& event) noexcept {
            QueueSum += event.Value;
            if (QueueAfter != nullptr) QueueAfter(QueueContext, event);
        }

        void OnEvent(const LatestEvent& event) noexcept {
            LatestLast = event.Value;
            if (LatestAfter != nullptr) LatestAfter(LatestContext, event);
        }
    };

    struct QueueBHandler final : CF::Provider<
        Event::Composition::Domain,
        CF::Offers<
            CF::Offer<Event::Composition::ListenerCallback<ListenerB, QueueEvent>>
        >
    > {
        int Sum{0};
        void OnEvent(const QueueEvent& event) noexcept { Sum += event.Value * 10; }
    };

    using ListenerThreadingTopology = Threading::ThreadingTopology<
        Threading::DedicatedThread<ListenerA>,
        Threading::DedicatedThread<ListenerB>
    >;
    using EventComposition = CF::Composition<
        Event::Composition::Domain,
        ListenerAHandler,
        QueueBHandler
    >;
    using ThreadingComposition = CF::Composition<
        Threading::Domain,
        ListenerThreadingTopology,
        FakeMutex
    >;
    using Architecture = CF::Architecture<EventComposition, ThreadingComposition>;

    using Runtime = Event::Runtime<
        Plan,
        Architecture,
        FakeMemoryRuntime,
        FakeThreadingRuntime,
        FakeMutex,
        ListenerAHandler,
        QueueBHandler
    >;
    using Bootstrap = Event::Bootstrap<
        Architecture,
        Plan,
        FakeMemoryRuntime,
        FakeThreadingRuntime
    >;

    void ReentrantQueueDispatch(void* context, const QueueEvent& event) noexcept {
        if (event.Value == 50) {
            auto& runtime = *static_cast<Runtime*>(context);
            assert(runtime.Dispatch(QueueEvent{51}) == Event::DispatchResult::Accepted);
        }
    }

    void ReentrantLatestDispatch(void* context, const LatestEvent& event) noexcept {
        if (event.Value == 30) {
            auto& runtime = *static_cast<Runtime*>(context);
            assert(runtime.Dispatch(LatestEvent{40}) == Event::DispatchResult::Accepted);
        }
    }

    struct RemoteOperation final {
        int Calls{0};
        int operator()(const QueueEvent& event) noexcept {
            ++Calls;
            return event.Value * 100;
        }
    };
}

int main() {
    using namespace Test;

    TestClock clock;
    assert(Clock::BindMonotonicClock(clock));

    FakeMemoryRuntime memory;
    FakeThreadingRuntime threading;
    FakeMutex mutex;
    ListenerAHandler listenerA;
    QueueBHandler queueB;
    Bootstrap bootstrap(memory, threading, mutex, listenerA, queueB);
    assert(bootstrap.Initialize() == Event::InitializationResult::Initialized);
    auto& runtime = bootstrap.RuntimeInstance();
    assert((runtime.template Subscribe<ListenerA, QueueEvent>() == Event::SubscribeResult::Subscribed));
    assert((runtime.template Subscribe<ListenerB, QueueEvent>() == Event::SubscribeResult::Subscribed));
    assert((runtime.template Subscribe<ListenerA, LatestEvent>() == Event::SubscribeResult::Subscribed));

    assert(runtime.Dispatch(QueueEvent{1}) == Event::DispatchResult::Accepted);
    assert(runtime.Dispatch(QueueEvent{2}) == Event::DispatchResult::Accepted);
    assert(runtime.Dispatch(QueueEvent{3}) == Event::DispatchResult::Accepted);
    assert(runtime.Dispatch(QueueEvent{4}) == Event::DispatchResult::NoCapacity);

    auto drainedA = runtime.template Drain<ListenerA>(2U);
    assert(drainedA.Delivered == 2U);
    assert(drainedA.WorkRemaining);
    assert(listenerA.QueueSum == 3);

    auto drainedB = runtime.template Drain<ListenerB>(8U);
    assert(drainedB.Delivered == 3U);
    assert(!drainedB.WorkRemaining);
    assert(queueB.Sum == 60);

    drainedA = runtime.template Drain<ListenerA>(8U);
    assert(drainedA.Delivered == 1U);
    assert(!drainedA.WorkRemaining);
    assert(listenerA.QueueSum == 6);

    assert(runtime.Dispatch(LatestEvent{10}) == Event::DispatchResult::Accepted);
    assert(runtime.Dispatch(LatestEvent{20}) == Event::DispatchResult::Accepted);
    const auto latestDrain = runtime.template Drain<ListenerA>(8U);
    assert(latestDrain.Delivered == 1U);
    assert(listenerA.LatestLast == 20);

    clock.NowNs = 1000U;
    assert(runtime.Dispatch(QueueEvent{9}, Event::UntilDeadline{Clock::MonotonicTimestamp::FromNanoseconds(999U)}) == Event::DispatchResult::Expired);
    assert(runtime.Dispatch(QueueEvent{9}, Event::ForDuration{Clock::Duration::FromNanoseconds(10)}) == Event::DispatchResult::Accepted);
    clock.NowNs = 1011U;
    const auto expiredDrain = runtime.template Drain<ListenerA>(8U);
    assert(expiredDrain.Delivered == 0U);

    assert((runtime.template Unsubscribe<ListenerA, QueueEvent>() == Event::UnsubscribeResult::Unsubscribed));
    assert((runtime.template Unsubscribe<ListenerA, QueueEvent>() == Event::UnsubscribeResult::NotSubscribed));

    // Unsubscribe cancels pending interest that has not yet been claimed.
    assert((runtime.template Subscribe<ListenerA, QueueEvent>() == Event::SubscribeResult::Subscribed));
    assert(runtime.Dispatch(QueueEvent{70}) == Event::DispatchResult::Accepted);
    assert((runtime.template Unsubscribe<ListenerA, QueueEvent>() == Event::UnsubscribeResult::Unsubscribed));
    const auto cancelledA = runtime.template Drain<ListenerA>(8U);
    assert(cancelledA.Delivered == 0U);
    const auto retainedB = runtime.template Drain<ListenerB>(8U);
    assert(retainedB.Delivered == 1U);
    assert(queueB.Sum == 760);
    assert((runtime.template Subscribe<ListenerA, QueueEvent>() == Event::SubscribeResult::Subscribed));

    // Callback execution is outside the Event lock: reentrant Dispatch must succeed.
    listenerA.QueueContext = &runtime;
    listenerA.QueueAfter = &ReentrantQueueDispatch;
    assert(runtime.Dispatch(QueueEvent{50}) == Event::DispatchResult::Accepted);
    const auto reentrantQueue = runtime.template Drain<ListenerA>(1U);
    assert(reentrantQueue.Delivered == 1U);
    assert(reentrantQueue.WorkRemaining);
    listenerA.QueueAfter = nullptr;
    const auto reentrantQueueTail = runtime.template Drain<ListenerA>(8U);
    assert(reentrantQueueTail.Delivered == 1U);

    // NewestOnly may admit a replacement while the previous occurrence remains actively borrowed.
    listenerA.LatestContext = &runtime;
    listenerA.LatestAfter = &ReentrantLatestDispatch;
    assert(runtime.Dispatch(LatestEvent{30}) == Event::DispatchResult::Accepted);
    const auto latestBorrowed = runtime.template Drain<ListenerA>(1U);
    assert(latestBorrowed.Delivered == 1U);
    assert(latestBorrowed.WorkRemaining);
    listenerA.LatestAfter = nullptr;
    const auto latestReplacement = runtime.template Drain<ListenerA>(1U);
    assert(latestReplacement.Delivered == 1U);
    assert(listenerA.LatestLast == 40);

    // Common expiry gate prevents both local admission and remote handoff.
    RemoteOperation remote;
    QueueEvent remoteEvent{8};
    clock.NowNs = 2000U;
    auto expiredBoth = runtime.Dispatch(
        Event::LocalAndRemote{},
        remoteEvent,
        Event::UntilDeadline{Clock::MonotonicTimestamp::FromNanoseconds(1999U)},
        remote
    );
    assert(expiredBoth.Local() == Event::DispatchResult::Expired);
    assert(expiredBoth.Remote().WasSkippedExpired());
    assert(remote.Calls == 0);

    auto validBoth = runtime.Dispatch(
        Event::LocalAndRemote{},
        remoteEvent,
        Event::UntilHandoff{},
        remote
    );
    assert(validBoth.Local() == Event::DispatchResult::Accepted);
    assert(validBoth.Remote().WasAttempted());
    assert(validBoth.Remote().Result() == 800);
    assert(remote.Calls == 1);

    auto skippedRemote = Event::DispatchScoped(
        Event::RemoteOnly{},
        remoteEvent,
        Event::UntilDeadline{Clock::MonotonicTimestamp::FromNanoseconds(1999U)},
        remote
    );
    assert(skippedRemote.WasSkippedExpired());
    assert(remote.Calls == 1);

    auto attemptedRemote = Event::DispatchScoped(
        Event::RemoteOnly{},
        remoteEvent,
        Event::UntilHandoff{},
        remote
    );
    assert(attemptedRemote.WasAttempted());
    assert(attemptedRemote.Result() == 800);
    assert(remote.Calls == 2);

    // Domain attempts remain independent: local capacity failure does not suppress remote handoff.
    QueueEvent overflowEvent{9};
    auto capacityAndRemote = runtime.Dispatch(
        Event::LocalAndRemote{},
        overflowEvent,
        Event::UntilHandoff{},
        remote
    );
    assert(capacityAndRemote.Local() == Event::DispatchResult::NoCapacity);
    assert(capacityAndRemote.Remote().WasAttempted());
    assert(capacityAndRemote.Remote().Result() == 900);
    assert(remote.Calls == 3);

    assert(!mutex.Held);
    return 0;
}
