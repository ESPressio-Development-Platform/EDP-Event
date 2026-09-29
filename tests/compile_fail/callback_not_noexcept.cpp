#include "../support/BootstrapTestSupport.hpp"
namespace S = EventBootstrapSupport;
struct BadHandler final : S::CF::Provider<
    ESPressio::Event::Composition::Domain,
    S::CF::Offers<S::CF::Offer<ESPressio::Event::Composition::ListenerCallback<S::ListenerA, S::TestEvent>>>
> {
    /// Deliberately omits noexcept so Bootstrap callback validation must reject this provider.
    void OnEvent(const S::TestEvent&) {}
};
using EventComposition = S::CF::Composition<ESPressio::Event::Composition::Domain, BadHandler>;
using Architecture = S::CF::Architecture<EventComposition, S::GoodThreadingComposition>;
using Invalid = ESPressio::Event::Bootstrap<Architecture, S::Plan, S::FakeMemoryRuntime, S::FakeThreadingRuntime>;
static_assert(sizeof(Invalid) > 0U);
