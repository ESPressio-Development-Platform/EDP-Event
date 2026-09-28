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
        inline static constexpr System::TypeIdentifier Identifier{
            System::TypeIdentifier::Storage{
                0x00U, 0x00U, 0x01U, 0x00U,
                0x00U, 0x00U, 0x30U,
                static_cast<std::uint8_t>(TOrdinal + 1U)
            }
        };
        using Family = Event::Family;
        int Value{};
    };

} // EventTestSupport

namespace ESPressio::Bounded {

    template<std::uint8_t TOrdinal>
    struct MemoryBoundedTraits<EventTestSupport::EventValue<TOrdinal>> :
        MemoryBoundedValueDeclaration<false, int> {};

} // ESPressio::Bounded
