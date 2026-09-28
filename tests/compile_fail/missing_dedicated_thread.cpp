#include "../support/BootstrapTestSupport.hpp"
namespace S = EventBootstrapSupport;
using WrongTopology = ESPressio::Threading::ThreadingTopology<
    ESPressio::Threading::DedicatedThread<S::ListenerB>
>;
using ThreadingComposition = S::CF::Composition<ESPressio::Threading::Domain, WrongTopology, S::MutexProvider>;
using Architecture = S::CF::Architecture<S::GoodEventComposition, ThreadingComposition>;
using Invalid = ESPressio::Event::Bootstrap<Architecture, S::Plan, S::FakeMemoryRuntime, S::FakeThreadingRuntime>;
static_assert(sizeof(Invalid) > 0U);
