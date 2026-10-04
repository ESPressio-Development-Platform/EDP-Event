#include <array>
#include <cassert>
#include <cstdint>
#include <optional>
#include <tuple>
#include <type_traits>

#include <ESPressio_Event.hpp>

namespace Test {

    namespace Event = ESPressio::Event;

static_assert(std::is_same_v<Event::LocalOnly, ESPressio::Primitives::ExecutionDomain::LocalOnly>);
static_assert(std::is_same_v<Event::RemoteOnly, ESPressio::Primitives::ExecutionDomain::RemoteOnly>);
static_assert(std::is_same_v<Event::LocalAndRemote, ESPressio::Primitives::ExecutionDomain::LocalAndRemote>);
static_assert(Event::ExecutionDomainScope<Event::LocalOnly>);
static_assert(Event::ExecutionDomainScope<const Event::RemoteOnly&>);
static_assert(!Event::ExecutionDomainScope<int>);
    namespace Primitives = ESPressio::Primitives;
    namespace BoundedTopology = ESPressio::BoundedTopology;
    namespace Memory = ESPressio::Memory;
    namespace Threading = ESPressio::Threading;
    namespace Clock = ESPressio::Clock;
    namespace CF = ESPressio::System::CompositionFramework;

    struct ListenerA {};
    struct ListenerB {};

    struct QueueEvent final {
        /// Stable Primitive Type identity used by this test Event.
        inline static constexpr ESPressio::System::TypeIdentifier Identifier{
            ESPressio::System::TypeIdentifier::Storage{0x00,0x00,0x01,0x00,0x00,0x00,0x20,0x01}
        };
        /// Primitive family binding proving this payload is an Event.
        using Family = Event::Family;
        /// Test payload value used to verify delivery semantics.
        int Value{};
        /// Canonical schema exposing the payload under one stable Field identity.
        using Fields = ESPressio::System::FieldSet<
            ESPressio::System::FieldBinding<&QueueEvent::Value, 0U>
        >;
    };

    struct LatestEvent final {
        /// Stable Primitive Type identity used by this test Event.
        inline static constexpr ESPressio::System::TypeIdentifier Identifier{
            ESPressio::System::TypeIdentifier::Storage{0x00,0x00,0x01,0x00,0x00,0x00,0x20,0x02}
        };
        /// Primitive family binding proving this payload is an Event.
        using Family = Event::Family;
        /// Test payload value used to verify delivery semantics.
        int Value{};
        /// Canonical schema exposing the payload under one stable Field identity.
        using Fields = ESPressio::System::FieldSet<
            ESPressio::System::FieldBinding<&LatestEvent::Value, 0U>
        >;
    };

    struct TestClock final {
        /// Mutable canonical time source controlled by the host test.
        mutable std::uint64_t NowNs{100U};

        /// Optional increment applied after each read for deadline-boundary tests.
        mutable std::uint64_t StepNs{0U};

        /// Returns the current test-controlled monotonic timestamp.
        Clock::MonotonicTimestamp Now() const noexcept {
            const auto observed = NowNs;
            NowNs += StepNs;
            return Clock::MonotonicTimestamp::FromNanoseconds(observed);
        }
    };

} // Test

namespace ESPressio::Bounded {

    template<> struct MemoryBoundedTraits<Test::QueueEvent> : MemoryBoundedValueDeclaration<false, int> {};
    template<> struct MemoryBoundedTraits<Test::LatestEvent> : MemoryBoundedValueDeclaration<false, int> {};

} // ESPressio::Bounded

namespace Test {

    using PrimitiveTopology = Primitives::Topology<
        Event::Deploy<QueueEvent, 3U, Event::Queue<1U>, Event::TimedRetention, 3U>,
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

        private:

            /// Internal strong slot identity used by the fake bounded pool.
            using DedicatedIndexType = BoundedTopology::BoundedIndex<PoolIndexSpace<TObject>, TCapacity>;

            /// Fixed-capacity optional storage modelling the Memory-owned object pool.
            std::array<std::optional<TObject>, TCapacity> _objects{};

        public:

            /// Public slot identity required by the EDP-Memory pool contract.
            using DedicatedIndex = DedicatedIndexType;

        /// @tparam TArgs Constructor argument Types forwarded into the retained test object.
        template<class... TArgs>
        /// Acquires the first free fake dedicated slot and constructs the requested test object.
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

        /// Resolves one valid fake slot index to its mutable retained object.
        TObject& DedicatedObject(DedicatedIndex index) noexcept {
            return *_objects[static_cast<std::size_t>(index.Value())];
        }
        /// Resolves one valid fake slot index to its immutable retained object.
        const TObject& DedicatedObject(DedicatedIndex index) const noexcept {
            return *_objects[static_cast<std::size_t>(index.Value())];
        }
        /// Releases one owned fake dedicated slot and invalidates the caller index.
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
        /// Exact dedicated capacity required by the Event occurrence record.
        using Dedicated = Memory::DedicatedInstances<TObject::MaximumInstances>;
        /// Confirms the fake Event pool exposes no raw shared overflow.
        using Shared = Memory::NoSharedOverflow;
    };

    template<class TPlan, class TList>
    class FakeMemoryRuntimeImpl;

    template<class TPlan, class... TEvents>
    class FakeMemoryRuntimeImpl<TPlan, Primitives::TypeList<TEvents...>> final {
        /// Tuple containing one fake occurrence pool for every deployed Event Type.
        using Pools = std::tuple<
            FakePool<Event::OccurrenceRecord<TPlan, TEvents>, Event::OccurrenceRecord<TPlan, TEvents>::MaximumInstances>...
        >;
        /// Owned fake pool tuple used by the Runtime host test.
        Pools _pools{};
    public:
        template<class TObject>
        /// Maps an occurrence object Type to its fake bounded pool Type.
        using ObjectPoolType = FakePool<TObject, TObject::MaximumInstances>;

        struct Topology final {
            template<class TObject>
            /// Test topology advertises every requested occurrence pool.
            static constexpr bool ContainsObjectPool = true;
            template<class TObject>
            /// Returns the exact fake pool specification for the requested object Type.
            using ObjectPoolSpecFor = FakeSpec<TObject>;
        };
        /// Reports the fake Memory Runtime as initialized for Event tests.
        bool IsInitialized() const noexcept { return true; }
        template<class TObject>
        /// Returns the fake pool bound to the requested occurrence object Type.
        auto& ObjectPoolFor() noexcept {
            return std::get<FakePool<TObject, TObject::MaximumInstances>>(_pools);
        }
    };

    using FakeMemoryRuntime = FakeMemoryRuntimeImpl<Plan, typename Plan::PrimitiveTypes>;

    /// Per-Thread wake evidence retained by the fake Threading Runtime.
    /// @tparam TThread Semantic Dedicated Thread identity whose wake count is recorded.
    template<class TThread>
    struct WakeCounter final {

        /// Number of advisory wake requests observed for this Thread identity.
        inline static int Count = 0;

    };

    template<class TThread>
    struct FakeThread final {
        /// Simulates a successful advisory Dedicated Thread wake.
        Threading::ThreadWakeResult Wake() noexcept {
            ++WakeCounter<TThread>::Count;
            return Threading::ThreadWakeResult::Woken;
        }
    };

    struct FakeThreadingRuntime final {
        template<class TThread>
        /// Returns a wake-capable fake Thread handle for the requested identity.
        FakeThread<TThread> ThreadHandle() noexcept { return {}; }
    };

    struct FakeMutex final : CF::Provider<
        Threading::Domain,
        CF::Offers<
            CF::Offer<Threading::OrdinaryMutex<Event::Composition::RuntimeMutexIdentity>>
        >
    > {

        // Fake mutex state.

        /// Indicates whether the fake ordinary mutex is currently held.
        bool Held{false};

        // Ordinary mutex provider contract.

        /// Acquires the fake mutex or reports provider failure for invalid nested acquisition.
        Threading::OrdinaryMutexAcquireResult Acquire() noexcept {
            if (Held) return Threading::OrdinaryMutexAcquireResult::ProviderFailure;
            Held = true;
            return Threading::OrdinaryMutexAcquireResult::Acquired;
        }
        /// Releases the fake mutex or reports provider failure when it was not held.
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

        // Optional post-callback hook Types.

        /// Queue callback hook used to exercise callback-outside-lock reentrancy.
        using QueueHook = void (*)(void*, const QueueEvent&) noexcept;

        /// NewestOnly callback hook used to exercise callback-outside-lock reentrancy.
        using LatestHook = void (*)(void*, const LatestEvent&) noexcept;

        // Callback evidence and hook state.

        /// Sum of Queue Event values observed by Listener A.
        int QueueSum{0};

        /// Most recent NewestOnly Event value observed by Listener A.
        int LatestLast{0};

        /// Optional opaque context supplied to the Queue post-callback hook.
        void* QueueContext{nullptr};

        /// Optional Queue post-callback hook.
        QueueHook QueueAfter{nullptr};

        /// Optional opaque context supplied to the NewestOnly post-callback hook.
        void* LatestContext{nullptr};

        /// Optional NewestOnly post-callback hook.
        LatestHook LatestAfter{nullptr};

        // Typed Listener callback provider contract.

        /// Records Queue delivery and invokes the optional reentrancy hook.
        void OnEvent(const QueueEvent& event) noexcept {
            QueueSum += event.Value;
            if (QueueAfter != nullptr) QueueAfter(QueueContext, event);
        }

        /// Records NewestOnly delivery and invokes the optional reentrancy hook.
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

        // Callback evidence.

        /// Weighted sum of Queue values observed by Listener B.
        int Sum{0};

        // Typed Listener callback provider contract.

        /// Accumulates one Queue Event using a distinguishable weighting factor.
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
        /// Number of remote handoff invocations observed by the test.
        int Calls{0};

        /// Adapter-observed values proving sequencer entry order.
        std::array<int, 16U> Values{};

        /// Event mutex evidence proving adapters run outside family synchronization.
        FakeMutex* Mutex{nullptr};

        /// Returns a deterministic provider result derived from the Event payload.
        int operator()(const QueueEvent& event) noexcept {
            assert(Mutex == nullptr || !Mutex->Held);
            Values[static_cast<std::size_t>(Calls)] = event.Value;
            ++Calls;
            return event.Value * 100;
        }
    };


    struct RemoteEventBinding final {
        bool* Released;

        [[nodiscard]] std::size_t RecipientCount() const noexcept {
            return 1U;
        }

        [[nodiscard]] int Recipient(std::size_t index) const noexcept {
            return index == 0U ? 17 : -1;
        }

        [[nodiscard]] Event::RemoteEventTerminal Observe(std::size_t) const noexcept {
            return Event::RemoteEventTerminal::Admitted;
        }

        [[nodiscard]] Event::RemoteEventTerminal WaitFor(
            std::size_t,
            Event::Duration
        ) noexcept {
            return Event::RemoteEventTerminal::Admitted;
        }

        [[nodiscard]] Event::RemoteEventTerminal WaitUntil(
            std::size_t,
            Event::MonotonicTimestamp
        ) noexcept {
            return Event::RemoteEventTerminal::Admitted;
        }

        [[nodiscard]] bool RequestCancellation(std::size_t) noexcept {
            return false;
        }

        void Release() noexcept {
            *Released = true;
        }
    };


    template<class TOperation>
    concept HasTakeResponse = requires(TOperation& operation) {
        operation.TakeResponse(0U);
    };

    using RemoteEventSurface =
        Event::RemoteEventOperation<QueueEvent, RemoteEventBinding>;

    static_assert(!std::is_copy_constructible_v<RemoteEventSurface>);
    static_assert(std::is_nothrow_move_constructible_v<RemoteEventSurface>);
    static_assert(!HasTakeResponse<RemoteEventSurface>);

} // Test

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

    // Transactional ingress owns physical and pending backing before publication.
    auto abortedIngressResult = runtime.template PrepareIngress<QueueEvent>();
    assert(abortedIngressResult.Accepted());
    auto abortedIngress = std::move(abortedIngressResult).TakeReservation();
    abortedIngress.Value().Value = 90;
    abortedIngress.Abort();
    assert(!abortedIngress.IsValid());

    auto expiredIngressResult = runtime.template PrepareIngress<QueueEvent>(
        Event::UntilDeadline{Clock::MonotonicTimestamp::FromNanoseconds(101U)}
    );
    assert(expiredIngressResult.Accepted());
    auto expiredIngress = std::move(expiredIngressResult).TakeReservation();
    expiredIngress.Value().Value = 91;
    clock.NowNs = 102U;
    assert(expiredIngress.Commit() == Event::DispatchResult::Expired);
    clock.NowNs = 100U;

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

    // Ingress snapshots current subscribers only at commit, never at reservation time.
    auto unpublishedResult = runtime.template PrepareIngress<QueueEvent>();
    assert(unpublishedResult.Accepted());
    auto unpublished = std::move(unpublishedResult).TakeReservation();
    unpublished.Value().Value = 77;
    assert((runtime.template Unsubscribe<ListenerA, QueueEvent>() == Event::UnsubscribeResult::Unsubscribed));
    assert((runtime.template Unsubscribe<ListenerB, QueueEvent>() == Event::UnsubscribeResult::Unsubscribed));
    assert(unpublished.Commit() == Event::DispatchResult::Accepted);
    assert(runtime.template Drain<ListenerA>(1U).Delivered == 0U);
    assert(runtime.template Drain<ListenerB>(1U).Delivered == 0U);
    assert((runtime.template Subscribe<ListenerA, QueueEvent>() == Event::SubscribeResult::Subscribed));
    assert((runtime.template Subscribe<ListenerB, QueueEvent>() == Event::SubscribeResult::Subscribed));

    auto latestIngressResult = runtime.template PrepareIngress<LatestEvent>();
    assert(latestIngressResult.Accepted());
    auto latestIngress = std::move(latestIngressResult).TakeReservation();
    latestIngress.Value().Value = 15;
    assert(latestIngress.Commit() == Event::DispatchResult::Accepted);
    assert(runtime.template Drain<ListenerA>(1U).Delivered == 1U);
    assert(listenerA.LatestLast == 15);

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
    remote.Mutex = &mutex;
    QueueEvent remoteEvent{8};
    clock.NowNs = 2000U;
    auto expiredBoth = runtime.Dispatch(
        Event::LocalAndRemote{},
        remoteEvent,
        Event::UntilDeadline{Clock::MonotonicTimestamp::FromNanoseconds(1999U)}
    );
    assert(expiredBoth.Local() == Event::DispatchResult::Expired);
    assert(!expiredBoth.Remote().Accepted());
    assert(expiredBoth.Remote().Failure() == Event::ReservationFailure::Expired);
    assert(remote.Calls == 0);

    auto validBoth = runtime.Dispatch(
        Event::LocalAndRemote{},
        remoteEvent,
        Event::UntilHandoff{}
    );
    assert(validBoth.Local() == Event::DispatchResult::Accepted);
    assert(validBoth.Remote().Accepted());
    auto validReservation =
        std::move(validBoth.Remote()).TakeReservation();
    auto validAttempt = validReservation.TryCommit(remote);
    assert(validAttempt.GetState() == Event::OrderedHandoffAttemptState::Attempted);
    const auto* validBothResult = validAttempt.ResultIfPresent();
    assert(validBothResult != nullptr);
    assert(*validBothResult == 800);
    assert(remote.Calls == 1);

    auto skippedRemote = Event::DispatchScoped(
        Event::RemoteOnly{},
        remoteEvent,
        Event::UntilDeadline{Clock::MonotonicTimestamp::FromNanoseconds(1999U)},
        remote
    );
    assert(skippedRemote.WasSkippedExpired());
    assert(skippedRemote.GetState() == Event::RemoteDispatchAttemptState::SkippedExpired);
    assert(skippedRemote.ResultIfPresent() == nullptr);
    assert(remote.Calls == 1);

    auto attemptedRemote = Event::DispatchScoped(
        Event::RemoteOnly{},
        remoteEvent,
        Event::UntilHandoff{},
        remote
    );
    assert(attemptedRemote.WasAttempted());
    const auto* attemptedRemoteResult = attemptedRemote.ResultIfPresent();
    assert(attemptedRemoteResult != nullptr);
    assert(*attemptedRemoteResult == 800);
    assert(remote.Calls == 2);

    // Domain attempts remain independent: local capacity failure does not suppress remote handoff.
    QueueEvent overflowEvent{9};
    auto capacityAndRemote = runtime.Dispatch(
        Event::LocalAndRemote{},
        overflowEvent,
        Event::UntilHandoff{}
    );
    assert(capacityAndRemote.Local() == Event::DispatchResult::NoCapacity);
    assert(capacityAndRemote.Remote().Accepted());
    auto capacityReservation =
        std::move(capacityAndRemote.Remote()).TakeReservation();
    auto capacityAttempt = capacityReservation.TryCommit(remote);
    assert(capacityAttempt.GetState() == Event::OrderedHandoffAttemptState::Attempted);
    const auto* capacityRemoteResult = capacityAttempt.ResultIfPresent();
    assert(capacityRemoteResult != nullptr);
    assert(*capacityRemoteResult == 900);
    assert(remote.Calls == 3);

    // A common deadline may expire after local admission but before remote reservation.
    clock.NowNs = 3000U;
    clock.StepNs = 1U;
    QueueEvent crossedDeadline{10};
    auto localThenExpiredRemote = runtime.Dispatch(
        Event::LocalAndRemote{},
        crossedDeadline,
        Event::UntilDeadline{Clock::MonotonicTimestamp::FromNanoseconds(3002U)}
    );
    clock.StepNs = 0U;
    assert(localThenExpiredRemote.Local() == Event::DispatchResult::NoCapacity);
    assert(!localThenExpiredRemote.Remote().Accepted());
    assert(
        localThenExpiredRemote.Remote().Failure() ==
        Event::ReservationFailure::Expired
    );
    assert(remote.Calls == 3);

    // Later slots fail fast until the earlier opportunity is resolved.
    QueueEvent orderedFirst{31};
    QueueEvent orderedSecond{32};
    auto orderedFirstResult = runtime.PrepareRemoteHandoff(orderedFirst);
    auto orderedSecondResult = runtime.PrepareRemoteHandoff(orderedSecond);
    assert(orderedFirstResult.Accepted());
    assert(orderedSecondResult.Accepted());
    auto orderedFirstReservation =
        std::move(orderedFirstResult).TakeReservation();
    auto orderedSecondReservation =
        std::move(orderedSecondResult).TakeReservation();

    auto blockedSecond = orderedSecondReservation.TryCommit(remote);
    assert(blockedSecond.GetState() == Event::OrderedHandoffAttemptState::EarlierPending);
    assert(orderedSecondReservation.IsValid());
    assert(remote.Calls == 3);

    auto committedFirst = orderedFirstReservation.TryCommit(remote);
    assert(committedFirst.GetState() == Event::OrderedHandoffAttemptState::Attempted);
    auto committedSecond = orderedSecondReservation.TryCommit(remote);
    assert(committedSecond.GetState() == Event::OrderedHandoffAttemptState::Attempted);
    assert(remote.Values[3U] == 31);
    assert(remote.Values[4U] == 32);
    assert(remote.Calls == 5);

    // Aborting the head is a terminal skip and immediately unblocks its successor.
    QueueEvent skippedHead{33};
    QueueEvent afterSkip{34};
    auto skippedHeadResult = runtime.PrepareRemoteHandoff(skippedHead);
    auto afterSkipResult = runtime.PrepareRemoteHandoff(afterSkip);
    assert(skippedHeadResult.Accepted());
    assert(afterSkipResult.Accepted());
    auto skippedHeadReservation =
        std::move(skippedHeadResult).TakeReservation();
    auto afterSkipReservation =
        std::move(afterSkipResult).TakeReservation();
    skippedHeadReservation.Abort();
    auto afterSkipAttempt = afterSkipReservation.TryCommit(remote);
    assert(afterSkipAttempt.GetState() == Event::OrderedHandoffAttemptState::Attempted);
    assert(remote.Values[5U] == 34);
    assert(remote.Calls == 6);

    // Capacity is exact and plans with zero slots reject without retaining a borrow.
    QueueEvent heldA{41};
    QueueEvent heldB{42};
    QueueEvent heldC{43};
    QueueEvent refused{44};
    auto heldAResult = runtime.PrepareRemoteHandoff(heldA);
    auto heldBResult = runtime.PrepareRemoteHandoff(heldB);
    auto heldCResult = runtime.PrepareRemoteHandoff(heldC);
    auto refusedResult = runtime.PrepareRemoteHandoff(refused);
    assert(heldAResult.Accepted());
    assert(heldBResult.Accepted());
    assert(heldCResult.Accepted());
    assert(!refusedResult.Accepted());
    assert(refusedResult.Failure() == Event::ReservationFailure::NoCapacity);

    auto heldAReservation = std::move(heldAResult).TakeReservation();
    auto heldBReservation = std::move(heldBResult).TakeReservation();
    auto heldCReservation = std::move(heldCResult).TakeReservation();
    heldAReservation.Abort();
    heldBReservation.Abort();
    heldCReservation.Abort();

    LatestEvent noRemoteCapacity{55};
    auto noRemoteResult = runtime.PrepareRemoteHandoff(noRemoteCapacity);
    assert(!noRemoteResult.Accepted());
    assert(noRemoteResult.Failure() == Event::ReservationFailure::NoCapacity);

    // Remote Event semantics expose destination admission only, never a Response.
    bool remoteEventReleased = false;
    {
        RemoteEventSurface operation(RemoteEventBinding{&remoteEventReleased});
        assert(operation.RecipientCount() == 1U);
        assert(operation.Recipient(0U) == 17);
        assert(operation.Observe(0U) == Event::RemoteEventTerminal::Admitted);
        assert(
            operation.WaitFor(0U, Event::Duration::FromNanoseconds(1)) ==
            Event::RemoteEventTerminal::Admitted
        );
        assert(!operation.RequestCancellation(0U));
        RemoteEventSurface moved(std::move(operation));
        assert(!operation.IsValid());
        assert(moved.IsValid());
    }
    assert(remoteEventReleased);

    // Integration quiesce generation-safely cancels unpublished and ordered reservations.
    QueueEvent quiescedRemoteEvent{61};
    auto quiescedRemoteResult = runtime.PrepareRemoteHandoff(quiescedRemoteEvent);
    auto quiescedIngressResult = runtime.template PrepareIngress<LatestEvent>();
    assert(quiescedRemoteResult.Accepted());
    assert(quiescedIngressResult.Accepted());
    auto quiescedRemote =
        std::move(quiescedRemoteResult).TakeReservation();
    auto quiescedIngress =
        std::move(quiescedIngressResult).TakeReservation();
    quiescedIngress.Value().Value = 62;

    runtime.BeginIntegrationQuiesce();
    // Quiesce cannot destroy a destination while its decoder owns the
    // unpublished reservation outside the Event mutex.
    assert(!runtime.IsIntegrationQuiescent());
    assert(quiescedIngress.Commit() == Event::DispatchResult::RuntimeUnavailable);
    assert(runtime.IsIntegrationQuiescent());
    auto quiescedAttempt = quiescedRemote.TryCommit(remote);
    assert(quiescedAttempt.GetState() == Event::OrderedHandoffAttemptState::RuntimeUnavailable);
    assert(remote.Calls == 6);

    auto closedIngress = runtime.template PrepareIngress<LatestEvent>();
    assert(!closedIngress.Accepted());
    assert(closedIngress.Failure() == Event::ReservationFailure::RuntimeUnavailable);
    auto closedRemote = runtime.PrepareRemoteHandoff(quiescedRemoteEvent);
    assert(!closedRemote.Accepted());
    assert(closedRemote.Failure() == Event::ReservationFailure::RuntimeUnavailable);

    assert(!mutex.Held);
    return 0;
}
