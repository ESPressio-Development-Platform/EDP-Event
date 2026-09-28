#include "../support/BootstrapTestSupport.hpp"
namespace S = EventBootstrapSupport;
using EmptyEventComposition = S::CF::Composition<ESPressio::Event::Composition::Domain>;
using Architecture = S::CF::Architecture<EmptyEventComposition, S::GoodThreadingComposition>;
using Invalid = ESPressio::Event::Bootstrap<Architecture, S::Plan, S::FakeMemoryRuntime, S::FakeThreadingRuntime>;
static_assert(sizeof(Invalid) > 0U);
