#include "../support/EventTestSupport.hpp"
namespace T = EventTestSupport;
using E = T::EventValue<1U>;
using Invalid = ESPressio::Primitives::Topology<
    ESPressio::Event::Deploy<E, 1U, ESPressio::Event::Queue<1U>, ESPressio::Event::UntilHandoffOnly>,
    ESPressio::Event::Deploy<E, 2U, ESPressio::Event::Queue<1U>, ESPressio::Event::UntilHandoffOnly>
>;
static_assert(sizeof(Invalid) > 0U);
