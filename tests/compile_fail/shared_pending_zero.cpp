#include "../support/EventTestSupport.hpp"
using E = EventTestSupport::EventValue<1U>;
using Invalid = ESPressio::Primitives::Topology<
    ESPressio::Event::Deploy<E, 2U, ESPressio::Event::Queue<1U>, ESPressio::Event::UntilHandoffOnly>,
    ESPressio::Event::SharedPending<0U>
>;
static_assert(sizeof(Invalid) > 0U);
