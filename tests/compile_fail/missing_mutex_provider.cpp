#include "../support/BootstrapTestSupport.hpp"
namespace S = EventBootstrapSupport;
using ThreadingComposition = S::CF::Composition<ESPressio::Threading::Domain, S::GoodThreadingTopology>;
using Architecture = S::CF::Architecture<S::GoodEventComposition, ThreadingComposition>;
using Invalid = ESPressio::Event::Bootstrap<Architecture, S::Plan, S::FakeMemoryRuntime, S::FakeThreadingRuntime>;
static_assert(sizeof(Invalid) > 0U);
