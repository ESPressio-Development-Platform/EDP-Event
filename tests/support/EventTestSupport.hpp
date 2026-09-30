#pragma once

#include <cstdint>

#include <ESPressio_Event.hpp>

namespace EventTestSupport {

    namespace Event = ESPressio::Event;
    namespace System = ESPressio::System;

    struct ListenerA final {};
    struct ListenerB final {};

    template<std::uint8_t TOrdinal>
    struct EventValue final {
        /// Stable test Primitive Type identity derived from the template ordinal.
        inline static constexpr System::TypeIdentifier Identifier{
            System::TypeIdentifier::Storage{
                0x00U, 0x00U, 0x01U, 0x00U,
                0x00U, 0x00U, 0x30U,
                static_cast<std::uint8_t>(TOrdinal + 1U)
            }
        };

        /// Primitive family binding proving this test payload is an Event.
        using Family = Event::Family;

        /// Integer payload used by host and compile-fail Event tests.
        int Value{};

        /// Canonical schema exposing the payload under one stable Type-local Field identity.
        using Fields = System::FieldSet<
            System::FieldBinding<&EventValue::Value, 0U>
        >;
    };

    static_assert(System::SchemaType<EventValue<0U>>);
    static_assert(Event::EventType<EventValue<0U>>);

} // EventTestSupport

namespace ESPressio::Bounded {

    template<std::uint8_t TOrdinal>
    struct MemoryBoundedTraits<EventTestSupport::EventValue<TOrdinal>> :
        MemoryBoundedValueDeclaration<false, int> {};

} // ESPressio::Bounded
