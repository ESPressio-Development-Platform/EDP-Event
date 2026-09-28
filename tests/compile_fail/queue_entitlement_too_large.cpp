#include "../support/EventTestSupport.hpp"
using Invalid = ESPressio::Event::Deploy<
    EventTestSupport::EventValue<1U>, 2U,
    ESPressio::Event::Queue<3U>, ESPressio::Event::UntilHandoffOnly
>;
static_assert(sizeof(Invalid) > 0U);
