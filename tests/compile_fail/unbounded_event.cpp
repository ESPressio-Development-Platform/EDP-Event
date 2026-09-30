#include <string>
#include "../support/EventTestSupport.hpp"

struct UnboundedEvent final {
    /// Stable Event identity for the unbounded-payload rejection test.
    inline static constexpr ESPressio::System::TypeIdentifier Identifier{
        ESPressio::System::TypeIdentifier::Storage{0,0,1,0,0,0,0x31,1}
    };

    /// Binds the rejected payload to the Event Primitive family.
    using Family = ESPressio::Event::Family;

    /// Deliberately unbounded payload member which must fail EventType.
    std::string Value;

    /// Keeps the fixture schema-valid so rejection is specifically due to boundedness.
    using Fields = ESPressio::System::FieldSet<
        ESPressio::System::FieldBinding<&UnboundedEvent::Value, 0U>
    >;
};

static_assert(ESPressio::System::SchemaType<UnboundedEvent>);

using Invalid = ESPressio::Event::Deploy<
    UnboundedEvent, 1U, ESPressio::Event::NewestOnly, ESPressio::Event::UntilHandoffOnly
>;
static_assert(sizeof(Invalid) > 0U);
