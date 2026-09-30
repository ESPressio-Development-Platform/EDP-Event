#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <tuple>
#include <type_traits>
#include <utility>

#include <ESPressio_Event.hpp>
#include <ESPressio_Platform_FreeRTOS.hpp>
#include <memory/MemoryResourceProvider.hpp>
#include <synchronization/SpinLockProvider.hpp>

namespace Demo {

    namespace Event = ESPressio::Event;
    namespace Memory = ESPressio::Memory;
    namespace Primitives = ESPressio::Primitives;
    namespace Threading = ESPressio::Threading;
    namespace Clock = ESPressio::Clock;
    namespace CF = ESPressio::System::CompositionFramework;

    // Concrete target providers selected by the demonstration.

    using SignalProvider = ESPressio::Platform::FreeRTOS::Synchronization::SignalProvider;
    using ExecutionContextProvider = ESPressio::Platform::FreeRTOS::Execution::ExecutionContextProvider;
    using SpinLockProvider = ESPressio::Platform::ESPIDF::Synchronization::SpinLockProvider;
    using MutexProvider = ESPressio::Platform::FreeRTOS::Synchronization::MutexProvider;
    using MemoryResourceProvider = ESPressio::Platform::Portable::Memory::MemoryResourceProvider;


    /// Outcome of running the NewestOnly + timed-retention demonstration.
    enum class Result : std::uint8_t {
        Succeeded = 0,
        ClockBindingFailed = 1,
        MemoryInitializationFailed = 2,
        ThreadingInitializationFailed = 3,
        ThreadingStartFailed = 4,
        EventInitializationFailed = 5,
        SubscriptionFailed = 6,
        FirstDispatchFailed = 7,
        ReplacementDispatchFailed = 8,
        ListenerStartFailed = 9,
        ReplacementMismatch = 10,
        ExpiryContractFailed = 11,
        ListenerStopFailed = 12,
        ThreadingShutdownFailed = 13,
        ThreadingFinalizationFailed = 14,
        MemoryTeardownFailed = 15
    };


    /// Semantic identity of the Dedicated Thread consuming latest-value Events.
    struct LatestListenerThread final {};


    /// Latest-value Event payload used to demonstrate NewestOnly supersession.
    struct LatestReading final {

        // Primitive identity.

        /// Stable universal TypeIdentifier for this Event Type.
        inline static constexpr ESPressio::System::TypeIdentifier Identifier{
            ESPressio::System::TypeIdentifier::Storage{
                0x45, 0x44, 0x50, 0x00, 0x00, 0x00, 0x00, 0x02
            }
        };

        /// Primitive family owned by this payload Type.
        using Family = Event::Family;

        // Event payload.

        /// Demonstration latest-value payload.
        std::int32_t Value{0};

        /// Canonical schema exposing the payload under one stable Type-local Field identity.
        using Fields = ESPressio::System::FieldSet<
            ESPressio::System::FieldBinding<&LatestReading::Value, 0U>
        >;

    };

} // Demo


namespace ESPressio::Bounded {

    /// Certifies LatestReading as a bounded, self-contained retained value.
    template<>
    struct MemoryBoundedTraits<Demo::LatestReading> :
        MemoryBoundedValueDeclaration<false, std::int32_t> {};

} // ESPressio::Bounded


namespace Demo {

    /// Deterministic one-megahertz monotonic timebase used for expiry validation.
    class Timebase final : public Clock::MonotonicTimebaseProvider<1000000U> {

        private:

            // Simulated monotonic counter.

            /// Current one-megahertz physical counter value.
            std::atomic<std::uint64_t> _count{0U};

        public:

            // Timebase access.

            /// Returns the current physical counter value.
            std::uint64_t CurrentCount() const noexcept {
                return _count.load(
                    std::memory_order_relaxed
                );
            }

            /// Advances the deterministic physical counter.
            /// @param count Number of one-megahertz ticks to add.
            void Advance(
                std::uint64_t count
            ) noexcept {
                _count.fetch_add(
                    count,
                    std::memory_order_relaxed
                );
            }

    };


    // Static Event and Memory topology.

    using PrimitiveTopology = Primitives::Topology<
        Event::Deploy<
            LatestReading,
            2U,
            Event::NewestOnly,
            Event::TimedRetention
        >,
        Event::Observe<
            LatestListenerThread,
            LatestReading
        >
    >;

    using EventPlan = Event::PlanFor<PrimitiveTopology>;
    using PoolSpec = Event::OccurrencePoolSpec<EventPlan, LatestReading>;

    using MemoryTopology = Memory::MemoryTopology<
        MemoryResourceProvider,
        Memory::SharedReserve<0U, MemoryResourceProvider>,
        PoolSpec
    >;

    using MemoryComposition = Memory::MemoryComposition<MemoryResourceProvider>;

    using Allocator = typename MemoryComposition::template Select<
        Memory::SharedReserveAllocationRequirement,
        CF::SelectUnique
    >;

    using MemoryRuntime = Memory::MemoryRuntime<
        MemoryTopology,
        MemoryComposition,
        MutexProvider,
        SignalProvider
    >;

    using ThreadingTopology = Threading::ThreadingTopology<
        Threading::DedicatedThread<
            LatestListenerThread,
            Threading::StackCapacity<4096U>
        >
    >;


    /// Late-bound Event Runtime bridge used by the Dedicated Thread callable.
    struct Control final {

        /// Erased bounded-drain operation.
        using DrainFunction = Event::DrainResult (*)(void*, std::size_t) noexcept;

        // Borrowed runtime state.

        /// Event Runtime instance owned by Event Bootstrap.
        void* Runtime{nullptr};

        /// Typed bounded-drain adapter corresponding to Runtime.
        DrainFunction Drain{nullptr};

    };


    /// Multi-purpose Dedicated Thread callable which drains Event work and then waits.
    struct Loop final {

        // Borrowed application bridge.

        /// Event Runtime bridge populated after Event Bootstrap.
        Control* RuntimeControl{nullptr};

        // Dedicated Thread execution.

        /// Services bounded Event work until idle, then waits indefinitely for advisory wake.
        /// @param context Threading-owned Dedicated Thread context.
        void operator ()(
            Threading::ThreadContext& context
        ) noexcept {
            while (!context.IsStopRequested()) {
                if (
                    RuntimeControl != nullptr &&
                    RuntimeControl->Runtime != nullptr &&
                    RuntimeControl->Drain != nullptr
                ) {
                    const auto drained = RuntimeControl->Drain(
                        RuntimeControl->Runtime,
                        8U
                    );

                    if (drained.WorkRemaining) {
                        continue;
                    }
                }

                if (
                    context.Wait() !=
                    Threading::ThreadWaitResult::Woken
                ) {
                    return;
                }
            }
        }

    };


    /// Typed Listener callback provider for the latest-reading Event.
    struct Handler final : CF::Provider<
        Event::Composition::Domain,
        CF::Offers<
            CF::Offer<
                Event::Composition::ListenerCallback<
                    LatestListenerThread,
                    LatestReading
                >
            >
        >
    > {

        // Observable demonstration state.

        /// Last delivered latest-reading value.
        std::atomic<std::int32_t> Last{0};

        /// Number of callback deliveries observed.
        std::atomic<std::uint32_t> Count{0U};

        // Listener callback.

        /// Records one delivered latest-reading occurrence.
        /// @param event Immutable Event occurrence borrowed for the callback duration.
        void OnEvent(
            const LatestReading& event
        ) noexcept {
            Last.store(
                event.Value,
                std::memory_order_relaxed
            );
            Count.fetch_add(
                1U,
                std::memory_order_release
            );
        }

    };


    // Event Composition and Architecture.

    using EventMutex = Threading::OrdinaryMutexProvider<
        Event::Composition::RuntimeMutexIdentity,
        MutexProvider
    >;

    using EventComposition = CF::Composition<
        Event::Composition::Domain,
        Handler
    >;

    using ThreadingComposition = CF::Composition<
        Threading::Domain,
        ThreadingTopology,
        EventMutex
    >;

    using Architecture = CF::Architecture<
        EventComposition,
        ThreadingComposition
    >;


    /// Erases the concrete Event Runtime Type for the Dedicated Thread control bridge.
    /// @tparam TEventRuntime Concrete Event Runtime produced by Event Bootstrap.
    /// @param runtime Borrowed Event Runtime instance.
    /// @param maximum Maximum callbacks permitted during this bounded drain.
    template<class TEventRuntime>
    Event::DrainResult DrainEvents(
        void* runtime,
        std::size_t maximum
    ) noexcept {
        return static_cast<TEventRuntime*>(runtime)->template Drain<
            LatestListenerThread
        >(
            maximum
        );
    }


    /// Writes one demonstration status line through the target console.
    /// @param text Null-terminated status message.
    void Print(
        const char* text
    ) noexcept {
        std::printf(
            "%s\n",
            text
        );
    }


    /// Runs NewestOnly supersession followed by an already-expired timed admission.
    Result Run() {
        Timebase timebase;
        Clock::MonotonicClockProvider<Timebase> clock(timebase);

        if (!Clock::BindMonotonicClock(clock)) {
            return Result::ClockBindingFailed;
        }

        MemoryResourceProvider resource;
        MutexProvider memoryMutex;
        Allocator allocator;
        MemoryRuntime memory(
            memoryMutex,
            allocator,
            resource
        );
        Memory::MemoryTopologyInitializationFailure memoryFailure{};

        if (
            memory.Initialize(
                memoryFailure
            ) != Memory::MemoryTopologyInitializationResult::Succeeded
        ) {
            return Result::MemoryInitializationFailed;
        }

        Control control{};
        auto bindings = std::make_tuple(
            Threading::BindDedicatedThread<LatestListenerThread>(
                Loop{&control}
            )
        );
        using Bindings = decltype(bindings);
        using ThreadingRuntime = Threading::StaticThreadingRuntime<
            ThreadingTopology,
            Bindings,
            SignalProvider,
            ExecutionContextProvider,
            SpinLockProvider,
            MutexProvider
        >;
        static ThreadingRuntime threading(
            std::move(
                bindings
            )
        );

        if (
            threading.Initialize() !=
            Threading::ThreadingInitializationResult::Succeeded
        ) {
            return Result::ThreadingInitializationFailed;
        }

        if (
            threading.Start() !=
            Threading::ThreadingStartResult::Succeeded
        ) {
            return Result::ThreadingStartFailed;
        }

        EventMutex eventMutex;
        Handler handler;
        using EventBootstrap = Event::Bootstrap<
            Architecture,
            EventPlan,
            MemoryRuntime,
            ThreadingRuntime
        >;
        EventBootstrap bootstrap(
            memory,
            threading,
            eventMutex,
            handler
        );

        if (
            bootstrap.Initialize() !=
            Event::InitializationResult::Initialized
        ) {
            return Result::EventInitializationFailed;
        }

        auto& events = bootstrap.RuntimeInstance();
        using EventRuntime = std::remove_reference_t<decltype(events)>;
        control.Runtime = &events;
        control.Drain = &DrainEvents<EventRuntime>;

        if (
            events.Subscribe<
                LatestListenerThread,
                LatestReading
            >() != Event::SubscribeResult::Subscribed
        ) {
            return Result::SubscriptionFailed;
        }

        // Keep the Listener stopped so the second occurrence supersedes the first pending value.
        if (
            events.Dispatch(
                LatestReading{10}
            ) != Event::DispatchResult::Accepted
        ) {
            return Result::FirstDispatchFailed;
        }

        if (
            events.Dispatch(
                LatestReading{20}
            ) != Event::DispatchResult::Accepted
        ) {
            return Result::ReplacementDispatchFailed;
        }

        auto listener = threading.ThreadHandle<LatestListenerThread>();

        if (
            listener.Start() !=
            Threading::ThreadStartResult::Started
        ) {
            return Result::ListenerStartFailed;
        }

        for (
            std::size_t spin = 0U;
            spin < 200000U;
            ++spin
        ) {
            if (
                handler.Count.load(
                    std::memory_order_acquire
                ) != 0U
            ) {
                break;
            }

            ExecutionContextProvider::Yield();
        }

        if (
            handler.Count.load(
                std::memory_order_acquire
            ) != 1U ||
            handler.Last.load(
                std::memory_order_relaxed
            ) != 20
        ) {
            return Result::ReplacementMismatch;
        }

        // Two thousand 1 MHz ticks move the canonical clock two milliseconds forward.
        timebase.Advance(2000U);

        if (
            events.Dispatch(
                LatestReading{30},
                Event::UntilDeadline{
                    Clock::MonotonicTimestamp::FromNanoseconds(
                        1000000U
                    )
                }
            ) != Event::DispatchResult::Expired
        ) {
            return Result::ExpiryContractFailed;
        }

        if (
            listener.RequestStop() !=
            Threading::ThreadStopRequestResult::Accepted
        ) {
            return Result::ListenerStopFailed;
        }

        while (
            listener.State() ==
            Threading::ThreadState::Running
        ) {
            ExecutionContextProvider::Yield();
        }

        if (
            threading.BeginShutdown() !=
            Threading::ThreadingShutdownResult::Accepted
        ) {
            return Result::ThreadingShutdownFailed;
        }

        while (!threading.IsExecutionQuiescent()) {
            ExecutionContextProvider::Yield();
        }

        if (
            threading.FinalizeShutdown() !=
            Threading::ThreadingFinalizationResult::Completed
        ) {
            return Result::ThreadingFinalizationFailed;
        }

        if (
            memory.TearDown() !=
            Memory::MemoryTopologyTeardownResult::Succeeded
        ) {
            return Result::MemoryTeardownFailed;
        }

        Print("EDP-Event newest-only-retention: PASS");
        return Result::Succeeded;
    }

} // Demo


#ifdef ARDUINO

/// Runs the NewestOnly + retention demonstration once after Arduino startup.
void setup() {
    static_cast<void>(
        Demo::Run()
    );
}

/// Leaves the target idle after the one-shot demonstration completes.
void loop() {
}

#else

/// Runs the NewestOnly + retention demonstration once from the ESP-IDF entry point.
extern "C" void app_main() {
    static_cast<void>(
        Demo::Run()
    );
}

#endif
