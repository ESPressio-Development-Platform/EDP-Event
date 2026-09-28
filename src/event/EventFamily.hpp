#pragma once

#include <ESPressio_Primitives.hpp>

namespace ESPressio::Event {

    struct Planner;

    /// Primitive family tag for broadcast typed Events.
    struct Family final {
        /// ESPressio-governed Primitive family identity: Authority 1, Event family 2.
        inline static constexpr Primitives::PrimitiveFamilyIdentifier Identifier{
            System::TypeAuthorityIdentifier{
                System::TypeAuthorityIdentifier::Storage{0x00U, 0x00U, 0x01U}
            },
            0x02U
        };

        using Planner = Event::Planner;
    };

} // ESPressio::Event
