#include <cstdint>
#include <utility>

#include <ESPressio_Event.hpp>

namespace Event = ESPressio::Event;
namespace System = ESPressio::System;
namespace Bounded = ESPressio::Bounded;
namespace Serialisation = ESPressio::Serialisation;

using UnsupportedValue = std::pair<std::uint32_t, std::uint32_t>;

struct NonSerialisableEvent final {

    /// Stable semantic identity for the serialisability-rejection fixture.
    static constexpr System::TypeIdentifier Identifier{
        System::TypeIdentifier::Storage{0x00U, 0xFEU, 0x08U, 0x00U, 0x00U, 0x00U, 0x00U, 0x01U}
    };

    /// Deliberately bounded but unsupported V1 Serialisation Field value.
    UnsupportedValue Value{};

    /// Canonical schema binding for the deliberately unsupported Field.
    using Fields = System::FieldSet<System::FieldBinding<&NonSerialisableEvent::Value, 1U>>;

    /// Associates the payload with the Event Primitive family.
    using Family = Event::Family;

};

namespace ESPressio::Bounded {

    template<>
    struct MemoryBoundedTraits<NonSerialisableEvent> :
        MemoryBoundedValueDeclaration<false, UnsupportedValue> {};

} // ESPressio::Bounded

static_assert(Bounded::IsMemoryBoundedValue<UnsupportedValue>);
static_assert(Bounded::IsMemoryBoundedValue<NonSerialisableEvent>);
static_assert(!Bounded::MemoryBoundedTraits<NonSerialisableEvent>::HasExternalLifetimeDependencies);
static_assert(System::SchemaType<NonSerialisableEvent>);
static_assert(!Serialisation::SerialisableType<UnsupportedValue>);
static_assert(!Serialisation::SerialisableType<NonSerialisableEvent>);
static_assert(
    Event::EventType<NonSerialisableEvent>,
    "A bounded schema-bearing Event with a non-serialisable Field Type must be rejected"
);

int main() { return 0; }
