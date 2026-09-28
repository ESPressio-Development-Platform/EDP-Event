#include "../support/EventTestSupport.hpp"
namespace T = EventTestSupport;
using E = T::EventValue<1U>;
using Invalid = ESPressio::Primitives::Topology<
    ESPressio::Event::Deploy<E, 2U, ESPressio::Event::Queue<1U>, ESPressio::Event::UntilHandoffOnly>,
    ESPressio::Event::Observe<T::ListenerA, E>,
    ESPressio::Event::Observe<T::ListenerA, E>
>;
static_assert(sizeof(Invalid) > 0U);
