#include <ESPressio_Event.hpp>

namespace Test {

    struct RemoteOperation final {

        /// Echoes a non-Event payload so DispatchScoped must reject the call at compile time.
        [[nodiscard]] int operator()(
            const int& value
        ) noexcept {
            return value;
        }

    };

} // Test

int main() {
    Test::RemoteOperation remote;
    const int payload = 42;
    const auto result = ESPressio::Event::DispatchScoped(
        ESPressio::Event::RemoteOnly{},
        payload,
        ESPressio::Event::UntilHandoff{},
        remote
    );
    static_cast<void>(result);
    return 0;

} // Test
