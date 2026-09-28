#include <string>
#include "../support/EventTestSupport.hpp"

struct UnboundedEvent final {
    inline static constexpr ESPressio::System::TypeIdentifier Identifier{
        ESPressio::System::TypeIdentifier::Storage{0,0,1,0,0,0,0x31,1}
    };
    using Family = ESPressio::Event::Family;
    std::string Value;
};

using Invalid = ESPressio::Event::Deploy<
    UnboundedEvent, 1U, ESPressio::Event::NewestOnly, ESPressio::Event::UntilHandoffOnly
>;
static_assert(sizeof(Invalid) > 0U);
