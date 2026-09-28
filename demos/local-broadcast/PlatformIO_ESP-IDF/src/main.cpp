#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <tuple>
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

using SignalProvider = ESPressio::Platform::FreeRTOS::Synchronization::SignalProvider;
using ExecutionContextProvider = ESPressio::Platform::FreeRTOS::Execution::ExecutionContextProvider;
using SpinLockProvider = ESPressio::Platform::ESPIDF::Synchronization::SpinLockProvider;
using MutexProvider = ESPressio::Platform::FreeRTOS::Synchronization::MutexProvider;
using MemoryResourceProvider = ESPressio::Platform::Portable::Memory::MemoryResourceProvider;

struct TemperatureListenerThread final {};

struct TemperatureChanged final {
    inline static constexpr ESPressio::System::TypeIdentifier Identifier{
        ESPressio::System::TypeIdentifier::Storage{0x45,0x44,0x50,0x00,0x00,0x00,0x00,0x01}
    };
    using Family = Event::Family;
    std::int16_t CelsiusX100{0};
};

} // namespace Demo

namespace ESPressio::Bounded {
template<>
struct MemoryBoundedTraits<Demo::TemperatureChanged> :
    MemoryBoundedValueDeclaration<false, std::int16_t> {};
} // namespace ESPressio::Bounded

namespace Demo {

using PrimitiveTopology = Primitives::Topology<
    Event::Deploy<TemperatureChanged, 4U, Event::Queue<2U>, Event::UntilHandoffOnly>,
    Event::Observe<TemperatureListenerThread, TemperatureChanged>,
    Event::SharedPending<2U>
>;
using EventPlan = Event::PlanFor<PrimitiveTopology>;
using TemperaturePool = Event::OccurrencePoolSpec<EventPlan, TemperatureChanged>;
using MemoryTopology = Memory::MemoryTopology<
    MemoryResourceProvider,
    Memory::SharedReserve<0U, MemoryResourceProvider>,
    TemperaturePool
>;
using MemoryComposition = Memory::MemoryComposition<MemoryResourceProvider>;
using SharedAllocator = typename MemoryComposition::template Select<
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
        TemperatureListenerThread,
        Threading::StackCapacity<4096U>,
        Threading::Priority<Threading::ThreadPriority::Normal>,
        Threading::AnyAffinity
    >
>;

struct ListenerControl final {
    using DrainFunction = Event::DrainResult (*)(void*, std::size_t) noexcept;
    void* Runtime{nullptr};
    DrainFunction Drain{nullptr};
};

struct ListenerLoop final {
    ListenerControl* Control{nullptr};
    void operator()(Threading::ThreadContext& context) noexcept {
        while (!context.IsStopRequested()) {
            if (Control != nullptr && Control->Runtime != nullptr && Control->Drain != nullptr) {
                const auto drained = Control->Drain(Control->Runtime, 8U);
                if (drained.WorkRemaining) {
                    continue;
                }
            }
            if (context.Wait() != Threading::ThreadWaitResult::Woken) {
                return;
            }
        }
    }
};

struct TemperatureHandler final : CF::Provider<
    Event::Composition::Domain,
    CF::Offers<CF::Offer<Event::Composition::ListenerCallback<TemperatureListenerThread, TemperatureChanged>>>
> {
    std::atomic<std::int16_t> Last{0};
    std::atomic<std::uint32_t> Count{0U};
    void OnEvent(const TemperatureChanged& event) noexcept {
        Last.store(event.CelsiusX100, std::memory_order_relaxed);
        Count.fetch_add(1U, std::memory_order_release);
    }
};

using EventMutex = Threading::OrdinaryMutexProvider<
    Event::Composition::RuntimeMutexIdentity,
    MutexProvider
>;
using EventComposition = CF::Composition<Event::Composition::Domain, TemperatureHandler>;
using ThreadingComposition = CF::Composition<Threading::Domain, ThreadingTopology, EventMutex>;
using Architecture = CF::Architecture<EventComposition, ThreadingComposition>;

template<class TEventRuntime>
Event::DrainResult DrainEvents(void* runtime, std::size_t maximum) noexcept {
    return static_cast<TEventRuntime*>(runtime)->template Drain<TemperatureListenerThread>(maximum);
}

enum class Result : std::uint8_t {
    Succeeded = 0,
    MemoryInitializationFailed,
    ThreadingInitializationFailed,
    ThreadingStartFailed,
    EventInitializationFailed,
    SubscriptionFailed,
    ListenerStartFailed,
    DispatchFailed,
    DeliveryTimeout,
    DeliveryMismatch,
    ListenerStopFailed,
    ThreadingShutdownFailed,
    ThreadingFinalizationFailed,
    MemoryTeardownFailed
};

void Print(const char* text) noexcept { std::printf("%s\n", text); }

Result Run() {
    MemoryResourceProvider resource;
    MutexProvider memoryMutex;
    SharedAllocator allocator;
    MemoryRuntime memory(memoryMutex, allocator, resource);
    Memory::MemoryTopologyInitializationFailure memoryFailure{};
    if (memory.Initialize(memoryFailure) != Memory::MemoryTopologyInitializationResult::Succeeded) {
        Print("TemperatureMonitor: memory initialization failed");
        return Result::MemoryInitializationFailed;
    }

    ListenerControl control{};
    auto bindings = std::make_tuple(
        Threading::BindDedicatedThread<TemperatureListenerThread>(ListenerLoop{&control})
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
    static ThreadingRuntime threading(std::move(bindings));
    if (threading.Initialize() != Threading::ThreadingInitializationResult::Succeeded) {
        return Result::ThreadingInitializationFailed;
    }
    if (threading.Start() != Threading::ThreadingStartResult::Succeeded) {
        return Result::ThreadingStartFailed;
    }

    EventMutex eventMutex;
    TemperatureHandler handler;
    using EventBootstrap = Event::Bootstrap<Architecture, EventPlan, MemoryRuntime, ThreadingRuntime>;
    EventBootstrap eventBootstrap(memory, threading, eventMutex, handler);
    if (eventBootstrap.Initialize() != Event::InitializationResult::Initialized) {
        return Result::EventInitializationFailed;
    }
    auto& events = eventBootstrap.RuntimeInstance();
    using EventRuntime = std::remove_reference_t<decltype(events)>;
    control.Runtime = &events;
    control.Drain = &DrainEvents<EventRuntime>;

    if (events.Subscribe<TemperatureListenerThread, TemperatureChanged>() != Event::SubscribeResult::Subscribed) {
        return Result::SubscriptionFailed;
    }
    auto listener = threading.ThreadHandle<TemperatureListenerThread>();
    if (listener.Start() != Threading::ThreadStartResult::Started) {
        return Result::ListenerStartFailed;
    }

    if (events.Dispatch(TemperatureChanged{2345}) != Event::DispatchResult::Accepted) {
        return Result::DispatchFailed;
    }
    for (std::size_t spin = 0U; spin < 200000U; ++spin) {
        if (handler.Count.load(std::memory_order_acquire) != 0U) break;
        ExecutionContextProvider::Yield();
    }
    if (handler.Count.load(std::memory_order_acquire) == 0U) {
        return Result::DeliveryTimeout;
    }
    if (handler.Last.load(std::memory_order_relaxed) != 2345) {
        return Result::DeliveryMismatch;
    }

    if (listener.RequestStop() != Threading::ThreadStopRequestResult::Accepted) {
        return Result::ListenerStopFailed;
    }
    while (listener.State() == Threading::ThreadState::Running) {
        ExecutionContextProvider::Yield();
    }
    if (threading.BeginShutdown() != Threading::ThreadingShutdownResult::Accepted) {
        return Result::ThreadingShutdownFailed;
    }
    while (!threading.IsExecutionQuiescent()) {
        ExecutionContextProvider::Yield();
    }
    if (threading.FinalizeShutdown() != Threading::ThreadingFinalizationResult::Completed) {
        return Result::ThreadingFinalizationFailed;
    }
    if (memory.TearDown() != Memory::MemoryTopologyTeardownResult::Succeeded) {
        return Result::MemoryTeardownFailed;
    }
    Print("TemperatureMonitor: received 23.45 C");
    Print("TemperatureMonitor: PASS");
    return Result::Succeeded;
}

} // namespace Demo

#ifdef ARDUINO
void setup() { static_cast<void>(Demo::Run()); }
void loop() {}
#else
extern "C" void app_main() { static_cast<void>(Demo::Run()); }
#endif
