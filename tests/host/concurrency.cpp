#include <array>
#include <atomic>
#include <barrier>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>
#include <tuple>

#include <ESPressio_Event.hpp>

namespace Test {

    namespace Event = ESPressio::Event;
    namespace Primitives = ESPressio::Primitives;
    namespace BoundedTopology = ESPressio::BoundedTopology;
    namespace Memory = ESPressio::Memory;
    namespace Threading = ESPressio::Threading;
    namespace CF = ESPressio::System::CompositionFramework;

    struct Listener final {};

    struct QueueEvent final {
        /// Stable Primitive Type identity used by this test Event.
        inline static constexpr ESPressio::System::TypeIdentifier Identifier{
            ESPressio::System::TypeIdentifier::Storage{
                0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x21, 0x01
            }
        };
        /// Primitive family binding proving this payload is an Event.
        using Family = Event::Family;
        /// Test payload value used to verify delivery semantics.
        std::uint16_t Value{};
    };

} // Test

namespace ESPressio::Bounded {

    template<>
    struct MemoryBoundedTraits<Test::QueueEvent> :
        MemoryBoundedValueDeclaration<false, std::uint16_t> {};

} // ESPressio::Bounded

namespace Test {

    inline constexpr std::size_t EventCount = 64U;

    using PrimitiveTopology = Primitives::Topology<
        Event::Deploy<QueueEvent, EventCount, Event::Queue<EventCount>, Event::UntilHandoffOnly>,
        Event::Observe<Listener, QueueEvent>
    >;
    using Plan = Event::PlanFor<PrimitiveTopology>;

    template<class TObject>
    struct PoolIndexSpace final {};

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
            if (output.IsValid()) {
                return Memory::DedicatedObjectPoolAcquisitionResult::OutputIndexOccupied;
            }
            for (std::size_t index = 0U; index < TCapacity; ++index) {
                if (!_objects[index].has_value()) {
                    _objects[index].emplace(std::forward<TArgs>(args)...);
                    output = DedicatedIndex::FromUnchecked(index);
                    return Memory::DedicatedObjectPoolAcquisitionResult::Succeeded;
                }
            }
            return Memory::DedicatedObjectPoolAcquisitionResult::CapacityUnavailable;
        }

        /// Resolves one valid fake slot index to its retained test object.
        TObject& DedicatedObject(DedicatedIndex index) noexcept {
            return *_objects[static_cast<std::size_t>(index.Value())];
        }

        /// Releases one owned fake dedicated slot and invalidates the caller index.
        Memory::DedicatedObjectPoolReleaseResult ReleaseDedicated(DedicatedIndex& index) noexcept {
            if (!index.IsValid()) {
                return Memory::DedicatedObjectPoolReleaseResult::InvalidIndex;
            }
            auto& slot = _objects[static_cast<std::size_t>(index.Value())];
            if (!slot.has_value()) {
                return Memory::DedicatedObjectPoolReleaseResult::SlotNotOwned;
            }
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

    class FakeMemoryRuntime final {
        /// Event occurrence record Type stored by this fake Memory Runtime.
        using Record = Event::OccurrenceRecord<Plan, QueueEvent>;
        /// Fixed fake pool backing the single deployed Event Type.
        FakePool<Record, Record::MaximumInstances> _pool{};

    public:
        template<class TObject>
        /// Maps an occurrence object Type to its fake bounded pool Type.
        using ObjectPoolType = FakePool<TObject, TObject::MaximumInstances>;

        struct Topology final {
            template<class TObject>
            /// Test topology advertises the required occurrence pool.
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
            static_assert(std::is_same_v<TObject, Record>);
            return _pool;
        }
    };

    template<class TThread>
    struct FakeThread final {
        /// Simulates a successful advisory Dedicated Thread wake.
        Threading::ThreadWakeResult Wake() noexcept {
            return Threading::ThreadWakeResult::Woken;
        }
    };

    struct FakeThreadingRuntime final {
        template<class TThread>
        /// Returns a wake-capable fake Thread handle for the requested identity.
        FakeThread<TThread> ThreadHandle() noexcept { return {}; }
    };

    enum class OperationKind : std::uint8_t {
        None = 0U,
        Dispatch = 1U,
        Subscribe = 2U
    };

    struct OperationMarker final {
        /// Operation category recorded at one synchronization point.
        OperationKind Kind{OperationKind::None};

        /// Payload value associated with the recorded operation.
        std::uint16_t Value{0U};
    };

    inline thread_local OperationMarker CurrentOperation{};

    struct LinearizationEntry final {
        /// Operation category stored in one linearization log entry.
        OperationKind Kind{OperationKind::None};

        /// Payload value stored in one linearization log entry.
        std::uint16_t Value{0U};
    };

    class ConcurrentMutex final : public CF::Provider<
        Threading::Domain,
        CF::Offers<
            CF::Offer<Threading::OrdinaryMutex<Event::Composition::RuntimeMutexIdentity>>
        >
    > {

        // Mutex and linearization-log state.

        /// Native host mutex used to serialize the Event Runtime under real contention.
        std::mutex _mutex;

        /// Bounded record of observed protected-operation linearization order.
        std::array<LinearizationEntry, EventCount + 2U> _linearized{};

        /// Number of valid entries currently retained in the bounded linearization log.
        std::size_t _count{0U};

    public:

        // Ordinary mutex provider contract.

        /// Acquires the host mutex and records the current test operation at the linearization point.
        Threading::OrdinaryMutexAcquireResult Acquire() noexcept {
            _mutex.lock();
            if (CurrentOperation.Kind != OperationKind::None) {
                assert(_count < _linearized.size());
                _linearized[_count++] = LinearizationEntry{
                    CurrentOperation.Kind,
                    CurrentOperation.Value
                };
            }
            return Threading::OrdinaryMutexAcquireResult::Acquired;
        }

        /// Releases the host mutex after one Event protected operation.
        Threading::OrdinaryMutexReleaseResult Release() noexcept {
            _mutex.unlock();
            return Threading::OrdinaryMutexReleaseResult::Released;
        }

        // Linearization-log inspection.

        /// Clears all retained operation-order evidence before the next race scenario.
        void ClearLinearizationLog() noexcept {
            std::scoped_lock lock(_mutex);
            _count = 0U;
        }

        /// Returns the number of valid linearization entries recorded so far.
        std::size_t LinearizationCount() noexcept {
            std::scoped_lock lock(_mutex);
            return _count;
        }

        /// Returns one recorded linearization entry by bounded host-test ordinal.
        LinearizationEntry LinearizationAt(std::size_t index) noexcept {
            std::scoped_lock lock(_mutex);
            assert(index < _count);
            return _linearized[index];
        }
    };

    class ListenerHandler final : public CF::Provider<
        Event::Composition::Domain,
        CF::Offers<
            CF::Offer<Event::Composition::ListenerCallback<Listener, QueueEvent>>
        >
    > {

        // Delivered-value evidence.

        /// Bounded sequence of delivered payload values in callback order.
        std::array<std::uint16_t, EventCount + 1U> _delivered{};

        /// Number of delivered values currently retained in the evidence array.
        std::size_t _count{0U};

    public:

        // Listener callback and inspection surface.

        /// Records one delivered Queue Event value without allocating.
        void OnEvent(const QueueEvent& event) noexcept {
            assert(_count < _delivered.size());
            _delivered[_count++] = event.Value;
        }

        /// Returns the number of callback values recorded by this handler.
        std::size_t Count() const noexcept { return _count; }
        /// Returns one recorded callback payload by bounded host-test ordinal.
        std::uint16_t At(std::size_t index) const noexcept {
            assert(index < _count);
            return _delivered[index];
        }
        /// Clears recorded callback evidence before the next test scenario.
        void Clear() noexcept { _count = 0U; }
    };

    using ListenerThreadingTopology = Threading::ThreadingTopology<
        Threading::DedicatedThread<Listener>
    >;
    using EventComposition = CF::Composition<
        Event::Composition::Domain,
        ListenerHandler
    >;
    using ThreadingComposition = CF::Composition<
        Threading::Domain,
        ListenerThreadingTopology,
        ConcurrentMutex
    >;
    using Architecture = CF::Architecture<EventComposition, ThreadingComposition>;
    using Bootstrap = Event::Bootstrap<
        Architecture,
        Plan,
        FakeMemoryRuntime,
        FakeThreadingRuntime
    >;

    void VerifyConcurrentProducerLinearization() {
        FakeMemoryRuntime memory;
        FakeThreadingRuntime threading;
        ConcurrentMutex mutex;
        ListenerHandler handler;
        Bootstrap bootstrap(memory, threading, mutex, handler);
        assert(bootstrap.Initialize() == Event::InitializationResult::Initialized);
        auto& runtime = bootstrap.RuntimeInstance();
        assert((runtime.template Subscribe<Listener, QueueEvent>() == Event::SubscribeResult::Subscribed));
        mutex.ClearLinearizationLog();

        std::barrier start(3);
        std::atomic<std::size_t> accepted{0U};

        auto producer = [&](std::uint16_t base) {
            start.arrive_and_wait();
            for (std::uint16_t offset = 0U; offset < EventCount / 2U; ++offset) {
                const auto value = static_cast<std::uint16_t>(base + offset);
                CurrentOperation = OperationMarker{OperationKind::Dispatch, value};
                const auto result = runtime.Dispatch(QueueEvent{value});
                CurrentOperation = {};
                assert(result == Event::DispatchResult::Accepted);
                accepted.fetch_add(1U, std::memory_order_relaxed);
            }
        };

        std::thread first(producer, 1U);
        std::thread second(producer, 1001U);
        start.arrive_and_wait();
        first.join();
        second.join();

        assert(accepted.load(std::memory_order_relaxed) == EventCount);
        assert(mutex.LinearizationCount() == EventCount);

        const auto drained = runtime.template Drain<Listener>(EventCount);
        assert(drained.Delivered == EventCount);
        assert(!drained.WorkRemaining);
        assert(handler.Count() == EventCount);

        for (std::size_t index = 0U; index < EventCount; ++index) {
            const auto linearized = mutex.LinearizationAt(index);
            assert(linearized.Kind == OperationKind::Dispatch);
            assert(handler.At(index) == linearized.Value);
        }
    }

    void VerifyConcurrentSubscribeSnapshot() {
        FakeMemoryRuntime memory;
        FakeThreadingRuntime threading;
        ConcurrentMutex mutex;
        ListenerHandler handler;
        Bootstrap bootstrap(memory, threading, mutex, handler);
        assert(bootstrap.Initialize() == Event::InitializationResult::Initialized);
        auto& runtime = bootstrap.RuntimeInstance();
        mutex.ClearLinearizationLog();

        std::barrier start(3);
        std::atomic<bool> dispatchAccepted{false};

        std::thread subscriber([&] {
            start.arrive_and_wait();
            CurrentOperation = OperationMarker{OperationKind::Subscribe, 0U};
            const auto result = runtime.template Subscribe<Listener, QueueEvent>();
            CurrentOperation = {};
            assert(result == Event::SubscribeResult::Subscribed);
        });

        std::thread dispatcher([&] {
            start.arrive_and_wait();
            CurrentOperation = OperationMarker{OperationKind::Dispatch, 77U};
            dispatchAccepted.store(
                runtime.Dispatch(QueueEvent{77U}) == Event::DispatchResult::Accepted,
                std::memory_order_relaxed
            );
            CurrentOperation = {};
        });

        start.arrive_and_wait();
        subscriber.join();
        dispatcher.join();
        assert(dispatchAccepted.load(std::memory_order_relaxed));
        assert(mutex.LinearizationCount() == 2U);

        const auto first = mutex.LinearizationAt(0U);
        const auto second = mutex.LinearizationAt(1U);
        assert(first.Kind != second.Kind);

        const auto drained = runtime.template Drain<Listener>(1U);
        if (first.Kind == OperationKind::Subscribe) {
            assert(second.Kind == OperationKind::Dispatch);
            assert(drained.Delivered == 1U);
            assert(handler.Count() == 1U);
            assert(handler.At(0U) == 77U);
        } else {
            assert(first.Kind == OperationKind::Dispatch);
            assert(second.Kind == OperationKind::Subscribe);
            assert(drained.Delivered == 0U);
            assert(handler.Count() == 0U);
        }
    }

} // Test

int main() {
    Test::VerifyConcurrentProducerLinearization();
    Test::VerifyConcurrentSubscribeSnapshot();
    return 0;
}
