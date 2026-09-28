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
namespace Clock = ESPressio::Clock;
namespace CF = ESPressio::System::CompositionFramework;
using SignalProvider = ESPressio::Platform::FreeRTOS::Synchronization::SignalProvider;
using ExecutionContextProvider = ESPressio::Platform::FreeRTOS::Execution::ExecutionContextProvider;
using SpinLockProvider = ESPressio::Platform::ESPIDF::Synchronization::SpinLockProvider;
using MutexProvider = ESPressio::Platform::FreeRTOS::Synchronization::MutexProvider;
using MemoryResourceProvider = ESPressio::Platform::Portable::Memory::MemoryResourceProvider;

struct LatestListenerThread final {};
struct LatestReading final {
    inline static constexpr ESPressio::System::TypeIdentifier Identifier{
        ESPressio::System::TypeIdentifier::Storage{0x45,0x44,0x50,0x00,0x00,0x00,0x00,0x02}
    };
    using Family = Event::Family;
    std::int32_t Value{0};
};
}
namespace ESPressio::Bounded {
template<> struct MemoryBoundedTraits<Demo::LatestReading> : MemoryBoundedValueDeclaration<false, std::int32_t> {};
}
namespace Demo {
class Timebase final : public Clock::MonotonicTimebaseProvider<1000000U> {
    std::atomic<std::uint64_t> _count{0U};
public:
    std::uint64_t CurrentCount() const noexcept { return _count.load(std::memory_order_relaxed); }
    void Advance(std::uint64_t count) noexcept { _count.fetch_add(count, std::memory_order_relaxed); }
};
using PrimitiveTopology = Primitives::Topology<
    Event::Deploy<LatestReading, 2U, Event::NewestOnly, Event::TimedRetention>,
    Event::Observe<LatestListenerThread, LatestReading>
>;
using EventPlan = Event::PlanFor<PrimitiveTopology>;
using PoolSpec = Event::OccurrencePoolSpec<EventPlan, LatestReading>;
using MemoryTopology = Memory::MemoryTopology<MemoryResourceProvider, Memory::SharedReserve<0U, MemoryResourceProvider>, PoolSpec>;
using MemoryComposition = Memory::MemoryComposition<MemoryResourceProvider>;
using Allocator = typename MemoryComposition::template Select<Memory::SharedReserveAllocationRequirement, CF::SelectUnique>;
using MemoryRuntime = Memory::MemoryRuntime<MemoryTopology, MemoryComposition, MutexProvider, SignalProvider>;
using ThreadingTopology = Threading::ThreadingTopology<Threading::DedicatedThread<LatestListenerThread, Threading::StackCapacity<4096U>>>;
struct Control final { using DrainFn=Event::DrainResult(*)(void*,std::size_t) noexcept; void* Runtime{}; DrainFn Drain{}; };
struct Loop final { Control* C{}; void operator()(Threading::ThreadContext& ctx) noexcept { while(!ctx.IsStopRequested()){ if(C&&C->Runtime&&C->Drain){ auto d=C->Drain(C->Runtime,8U); if(d.WorkRemaining) continue; } if(ctx.Wait()!=Threading::ThreadWaitResult::Woken) return; } } };
struct Handler final : CF::Provider<Event::Composition::Domain,CF::Offers<CF::Offer<Event::Composition::ListenerCallback<LatestListenerThread,LatestReading>>>> {
    std::atomic<std::int32_t> Last{0}; std::atomic<std::uint32_t> Count{0};
    void OnEvent(const LatestReading& e) noexcept { Last.store(e.Value,std::memory_order_relaxed); Count.fetch_add(1U,std::memory_order_release); }
};
using EventMutex = Threading::OrdinaryMutexProvider<Event::Composition::RuntimeMutexIdentity,MutexProvider>;
using EventComposition = CF::Composition<Event::Composition::Domain,Handler>;
using ThreadingComposition = CF::Composition<Threading::Domain,ThreadingTopology,EventMutex>;
using Architecture = CF::Architecture<EventComposition,ThreadingComposition>;
template<class R> Event::DrainResult Drain(void* p,std::size_t n) noexcept { return static_cast<R*>(p)->template Drain<LatestListenerThread>(n); }
void Print(const char* s) noexcept { std::printf("%s\n",s); }
int Run(){
    Timebase timebase; Clock::MonotonicClockProvider<Timebase> clock(timebase); if(!Clock::BindMonotonicClock(clock)) return 1;
    MemoryResourceProvider resource; MutexProvider mm; Allocator allocator; MemoryRuntime memory(mm,allocator,resource); Memory::MemoryTopologyInitializationFailure mf{}; if(memory.Initialize(mf)!=Memory::MemoryTopologyInitializationResult::Succeeded) return 2;
    Control control{}; auto bindings=std::make_tuple(Threading::BindDedicatedThread<LatestListenerThread>(Loop{&control})); using Bindings=decltype(bindings);
    using TR=Threading::StaticThreadingRuntime<ThreadingTopology,Bindings,SignalProvider,ExecutionContextProvider,SpinLockProvider,MutexProvider>; static TR threading(std::move(bindings)); if(threading.Initialize()!=Threading::ThreadingInitializationResult::Succeeded||threading.Start()!=Threading::ThreadingStartResult::Succeeded) return 3;
    EventMutex em; Handler handler; using EB=Event::Bootstrap<Architecture,EventPlan,MemoryRuntime,TR>; EB bootstrap(memory,threading,em,handler); if(bootstrap.Initialize()!=Event::InitializationResult::Initialized) return 4; auto& events=bootstrap.RuntimeInstance(); using ER=std::remove_reference_t<decltype(events)>; control.Runtime=&events; control.Drain=&Drain<ER>; if(events.Subscribe<LatestListenerThread,LatestReading>()!=Event::SubscribeResult::Subscribed) return 5;
    // Listener is deliberately not started yet: the second NewestOnly dispatch supersedes the first pending value.
    if(events.Dispatch(LatestReading{10})!=Event::DispatchResult::Accepted) return 6; if(events.Dispatch(LatestReading{20})!=Event::DispatchResult::Accepted) return 7;
    auto listener=threading.ThreadHandle<LatestListenerThread>(); if(listener.Start()!=Threading::ThreadStartResult::Started) return 8;
    for(std::size_t i=0;i<200000U && handler.Count.load(std::memory_order_acquire)==0U;++i) ExecutionContextProvider::Yield();
    if(handler.Count.load(std::memory_order_acquire)!=1U||handler.Last.load(std::memory_order_relaxed)!=20) return 9;
    timebase.Advance(2000U); // 2 ms at 1 MHz.
    if(events.Dispatch(LatestReading{30},Event::UntilDeadline{Clock::MonotonicTimestamp::FromNanoseconds(1000000U)})!=Event::DispatchResult::Expired) return 10;
    if(listener.RequestStop()!=Threading::ThreadStopRequestResult::Accepted) return 11; while(listener.State()==Threading::ThreadState::Running) ExecutionContextProvider::Yield(); if(threading.BeginShutdown()!=Threading::ThreadingShutdownResult::Accepted) return 12; while(!threading.IsExecutionQuiescent()) ExecutionContextProvider::Yield(); if(threading.FinalizeShutdown()!=Threading::ThreadingFinalizationResult::Completed) return 13; if(memory.TearDown()!=Memory::MemoryTopologyTeardownResult::Succeeded) return 14;
    Print("EDP-Event newest-only-retention: PASS"); return 0;
}
}
#ifdef ARDUINO
void setup(){static_cast<void>(Demo::Run());} void loop(){}
#else
extern "C" void app_main(){static_cast<void>(Demo::Run());}
#endif
