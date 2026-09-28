#include "../support/EventTestSupport.hpp"
using Invalid = ESPressio::Event::Deploy<
    EventTestSupport::EventValue<1U>, 0U,
    ESPressio::Event::NewestOnly, ESPressio::Event::UntilHandoffOnly
>;
static_assert(sizeof(Invalid) > 0U);
