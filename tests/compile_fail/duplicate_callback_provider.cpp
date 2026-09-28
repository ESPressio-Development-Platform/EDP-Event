#include "../support/BootstrapTestSupport.hpp"
namespace S = EventBootstrapSupport;
struct SecondHandler final : S::CF::Provider<
    ESPressio::Event::Composition::Domain,
    S::CF::Offers<S::CF::Offer<ESPressio::Event::Composition::ListenerCallback<S::ListenerA, S::TestEvent>>>
> {
    void OnEvent(const S::TestEvent&) noexcept {}
};
using InvalidComposition = S::CF::Composition<
    ESPressio::Event::Composition::Domain,
    S::GoodHandler,
    SecondHandler
>;
static_assert(sizeof(InvalidComposition) > 0U);
