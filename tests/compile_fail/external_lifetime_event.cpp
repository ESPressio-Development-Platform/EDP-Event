#include "../support/EventTestSupport.hpp"

struct ExternalEvent final {
    /// Stable Event identity for the external-lifetime rejection test.
    inline static constexpr ESPressio::System::TypeIdentifier Identifier{
        ESPressio::System::TypeIdentifier::Storage{0,0,1,0,0,0,0x31,2}
    };

    /// Binds the rejected payload to the Event Primitive family.
    using Family = ESPressio::Event::Family;

    /// Trivial payload retained alongside the deliberately invalid lifetime trait.
    int Value{};

    /// Keeps the fixture schema-valid so rejection is specifically due to lifetime dependence.
    using Fields = ESPressio::System::FieldSet<
        ESPressio::System::FieldBinding<&ExternalEvent::Value, 0U>
    >;
};

static_assert(ESPressio::System::SchemaType<ExternalEvent>);

namespace ESPressio::Bounded {

    template<>
    struct MemoryBoundedTraits<ExternalEvent> : MemoryBoundedValueDeclaration<true, int> {};

} // ESPressio::Bounded

using Invalid = ESPressio::Event::Deploy<
    ExternalEvent, 1U, ESPressio::Event::NewestOnly, ESPressio::Event::UntilHandoffOnly
>;
static_assert(sizeof(Invalid) > 0U);
