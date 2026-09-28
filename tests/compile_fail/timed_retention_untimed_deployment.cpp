#include "../support/BootstrapTestSupport.hpp"

void Invalid(
    EventBootstrapSupport::GoodBootstrap& bootstrap,
    EventBootstrapSupport::TestEvent event
) {
    bootstrap.RuntimeInstance().Dispatch(
        event,
        ESPressio::Event::ForDuration{ESPressio::Clock::Duration::FromNanoseconds(1)}
    );
}
