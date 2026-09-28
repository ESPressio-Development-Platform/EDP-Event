#include "../support/EventTestSupport.hpp"
using Invalid = ESPressio::Primitives::Topology<
    ESPressio::Event::Deploy<EventTestSupport::EventValue<1U>, 1U, ESPressio::Event::NewestOnly, ESPressio::Event::UntilHandoffOnly>,
    ESPressio::Event::Observe<EventTestSupport::ListenerA, EventTestSupport::EventValue<2U>>
>;
static_assert(sizeof(Invalid) > 0U);
