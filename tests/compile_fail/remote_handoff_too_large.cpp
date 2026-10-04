#include <ESPressio_Event.hpp>
#include "../support/EventTestSupport.hpp"

using Invalid = ESPressio::Event::Deploy<
    EventTestSupport::EventValue<20U>,
    1U,
    ESPressio::Event::NewestOnly,
    ESPressio::Event::UntilHandoffOnly,
    256U
>;

int main() {
    return static_cast<int>(Invalid::RemoteHandoffCapacity);
}
