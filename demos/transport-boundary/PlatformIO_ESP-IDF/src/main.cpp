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
    namespace CF = ESPressio::System::CompositionFramework;

    // Concrete target providers selected by the demonstration.

    using SignalProvider = ESPressio::Platform::FreeRTOS::Synchronization::SignalProvider;
    using ExecutionContextProvider = ESPressio::Platform::FreeRTOS::Execution::ExecutionContextProvider;
    using SpinLockProvider = ESPressio::Platform::ESPIDF::Synchronization::SpinLockProvider;
    using MutexProvider = ESPressio::Platform::FreeRTOS::Synchronization::MutexProvider;
    using MemoryResourceProvider = ESPressio::Platform::Portable::Memory::MemoryResourceProvider;


    /// Provider-defined outcome returned by the fake outbound Transport operation.
    enum class TransportResult : std::uint8_t {
        Accepted = 0
    };


    /// Outcome of running the Transport-boundary demonstration.
    enum class Result : std::uint8_t {
        Succeeded = 0,
        MemoryInitializationFailed = 1,
        ThreadingInitializationFailed = 2,
        ThreadingStartFailed = 3,
        EventInitializationFailed = 4,
        SubscriptionFailed = 5,
        ListenerStartFailed = 6,
        LocalAndRemoteFailed = 7,
        LocalDeliveryFailed = 8,
        RemoteOnlyFailed = 9,
        IngressFailed = 10,
        IngressDeliveryFailed = 11,
        ListenerStopFailed = 12,
        ThreadingShutdownFailed = 13,
        ThreadingFinalizationFailed = 14,
        MemoryTeardownFailed = 15
    };


    /// Semantic identity of the Dedicated Thread consuming local Event deliveries.
    struct ListenerThread final {};


    /// Event payload used to demonstrate the typed Transport boundary.
    struct BoundaryEvent final {

        // Primitive identity.

        /// Stable universal TypeIdentifier for this Event Type.
        inline static constexpr ESPressio::System::TypeIdentifier Identifier{
            ESPressio::System::TypeIdentifier::Storage{
                0x45, 0x44, 0x50, 0x00, 0x00, 0x00, 0x00, 0x03
            }
        };

        /// Primitive family owned by this payload Type.
        using Family = Event::Family;

        // Event payload.

        /// Demonstration value transported locally and remotely.
        std::int32_t Value{0};

        /// Canonical schema exposing the payload under one stable Type-local Field identity.
        using Fields = ESPressio::System::FieldSet<
            ESPressio::System::FieldBinding<&BoundaryEvent::Value, 0U>
        >;

    };

} // Demo


namespace ESPressio::Bounded {

    /// Certifies BoundaryEvent as a bounded, self-contained retained value.
    template<>
    struct MemoryBoundedTraits<Demo::BoundaryEvent> :
        MemoryBoundedValueDeclaration<false, std::int32_t> {};

} // ESPressio::Bounded


namespace Demo {

    // Static Event and Memory topology.

    using PrimitiveTopology = Primitives::Topology<
        Event::Deploy<
            BoundaryEvent,
            2U,
            Event::Queue<2U>,
            Event::UntilHandoffOnly,
            1U
        >,
        Event::Observe<
            ListenerThread,
            BoundaryEvent
        >
    >;

    using EventPlan = Event::PlanFor<PrimitiveTopology>;
    using PoolSpec = Event::OccurrencePoolSpec<EventPlan, BoundaryEvent>;

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
            ListenerThread,
            Threading::StackCapacity<4096U>
        >
    >;


    /// Late-bound Event Runtime bridge used by the Dedicated Thread callable.
    struct Control final {

        /// Erased bounded-drain operation.
        using DrainFunction = Event::DrainResult (*)(void*, std::size_t) noexcept;

        // Borrowed Event Runtime state.

        /// Event Runtime instance owned by Event Bootstrap.
        void* Runtime{nullptr};

        /// Typed drain adapter corresponding to Runtime.
        DrainFunction Drain{nullptr};

    };


    /// Multi-purpose Dedicated Thread callable which services local Event deliveries.
    struct Loop final {

        // Borrowed application bridge.

        /// Event Runtime bridge populated after Event Bootstrap.
        Control* RuntimeControl{nullptr};

        // Dedicated Thread execution.

        /// Drains Event work until idle, then waits indefinitely for advisory wake.
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


    /// Typed local Listener callback provider used by the boundary demonstration.
    struct Handler final : CF::Provider<
        Event::Composition::Domain,
        CF::Offers<
            CF::Offer<
                Event::Composition::ListenerCallback<
                    ListenerThread,
                    BoundaryEvent
                >
            >
        >
    > {

        // Observable local-delivery state.

        /// Most recently delivered local Event value.
        std::atomic<std::int32_t> Last{0};

        /// Number of local callbacks completed.
        std::atomic<std::uint32_t> Count{0U};

        // Listener callback.

        /// Records one immutable borrowed local occurrence.
        /// @param event Event payload borrowed only during this callback.
        void OnEvent(
            const BoundaryEvent& event
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


    /// Fake concrete outbound Transport boundary retaining only test-observable provider state.
    struct FakeTransport final {

        // Provider-owned observations.

        /// Number of outbound typed handoffs performed.
        std::atomic<std::uint32_t> Calls{0U};

        /// Payload value observed by the most recent outbound handoff.
        std::atomic<std::int32_t> Last{0};

        // Outbound typed handoff.

        /// Accepts one borrowed Event and returns the provider's own result vocabulary.
        /// @param event Immutable Event borrowed only for the bounded call duration.
        TransportResult operator ()(
            const BoundaryEvent& event
        ) noexcept {
            Last.store(
                event.Value,
                std::memory_order_relaxed
            );
            Calls.fetch_add(
                1U,
                std::memory_order_relaxed
            );
            return TransportResult::Accepted;
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


    /// Erases the concrete Event Runtime Type for the Dedicated Thread bridge.
    /// @tparam TEventRuntime Concrete Event Runtime produced by Event Bootstrap.
    /// @param runtime Borrowed Event Runtime instance.
    /// @param maximum Maximum callbacks permitted during this bounded drain.
    template<class TEventRuntime>
    Event::DrainResult DrainEvents(
        void* runtime,
        std::size_t maximum
    ) noexcept {
        return static_cast<TEventRuntime*>(runtime)->template Drain<ListenerThread>(
            maximum
        );
    }


    /// Runs staged local/remote handoff plus transactional inbound admission.
    Result Run() {
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
            Threading::BindDedicatedThread<ListenerThread>(
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
                ListenerThread,
                BoundaryEvent
            >() != Event::SubscribeResult::Subscribed
        ) {
            return Result::SubscriptionFailed;
        }

        auto listener = threading.ThreadHandle<ListenerThread>();

        if (
            listener.Start() !=
            Threading::ThreadStartResult::Started
        ) {
            return Result::ListenerStartFailed;
        }

        FakeTransport transport;
        BoundaryEvent event{77};
        auto both = events.Dispatch(
            Event::LocalAndRemote{},
            event,
            Event::UntilHandoff{}
        );

        if (
            both.Local() != Event::DispatchResult::Accepted ||
            !both.Remote().Accepted()
        ) {
            return Result::LocalAndRemoteFailed;
        }

        auto reservation =
            std::move(both.Remote()).TakeReservation();
        auto remoteAttempt = reservation.TryCommit(transport);
        const auto* bothRemoteResult = remoteAttempt.ResultIfPresent();

        if (
            remoteAttempt.GetState() != Event::OrderedHandoffAttemptState::Attempted ||
            bothRemoteResult == nullptr ||
            *bothRemoteResult != TransportResult::Accepted ||
            transport.Calls.load(
                std::memory_order_relaxed
            ) != 1U ||
            transport.Last.load(
                std::memory_order_relaxed
            ) != 77
        ) {
            return Result::LocalAndRemoteFailed;
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
            ) != 77
        ) {
            return Result::LocalDeliveryFailed;
        }

        auto remoteOnly = Event::DispatchScoped(
            Event::RemoteOnly{},
            BoundaryEvent{88},
            Event::UntilHandoff{},
            transport
        );

        const auto* remoteOnlyResult = remoteOnly.ResultIfPresent();

        if (
            !remoteOnly.WasAttempted() ||
            remoteOnlyResult == nullptr ||
            *remoteOnlyResult != TransportResult::Accepted ||
            transport.Calls.load(
                std::memory_order_relaxed
            ) != 2U ||
            transport.Last.load(
                std::memory_order_relaxed
            ) != 88
        ) {
            return Result::RemoteOnlyFailed;
        }

        Event::InboundAdmission<
            BoundaryEvent,
            EventRuntime
        > inbound(events);
        auto ingressResult = inbound.Prepare();

        if (!ingressResult.Accepted()) {
            return Result::IngressFailed;
        }

        auto ingress =
            std::move(ingressResult).TakeReservation();
        ingress.Value().Value = 99;

        if (ingress.Commit() != Event::DispatchResult::Accepted) {
            return Result::IngressFailed;
        }

        for (
            std::size_t spin = 0U;
            spin < 200000U;
            ++spin
        ) {
            if (
                handler.Count.load(
                    std::memory_order_acquire
                ) >= 2U
            ) {
                break;
            }

            ExecutionContextProvider::Yield();
        }

        if (
            handler.Count.load(
                std::memory_order_acquire
            ) != 2U ||
            handler.Last.load(
                std::memory_order_relaxed
            ) != 99
        ) {
            return Result::IngressDeliveryFailed;
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

        std::printf("EDP-Event transport-boundary: PASS\n");
        return Result::Succeeded;
    }

} // Demo


#ifdef ARDUINO

/// Runs the Transport-boundary demonstration once after Arduino startup.
void setup() {
    static_cast<void>(
        Demo::Run()
    );
}

/// Leaves the target idle after the one-shot demonstration completes.
void loop() {
}

#else

/// Runs the Transport-boundary demonstration once from the ESP-IDF application entry.
extern "C" void app_main() {
    static_cast<void>(
        Demo::Run()
    );
}

#endif
