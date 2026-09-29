#pragma once

#include <ESPressio_Primitives.hpp>

namespace ESPressio::Event {

    /// Event-family planner defined by Planner.hpp.
    struct Planner;


    /// Primitive family tag for broadcast typed Events.
    struct Family final {

        // Primitive-family identity.

        /// ESPressio-governed Primitive family identity: Authority 1, Event family 2.
        inline static constexpr Primitives::PrimitiveFamilyIdentifier Identifier{
            System::TypeAuthorityIdentifier{
                System::TypeAuthorityIdentifier::Storage{0x00U, 0x00U, 0x01U}
            },
            0x02U
        };

        /// Canonical family planner responsible for normalizing Event declarations.
        using Planner = Event::Planner;

    };

} // ESPressio::Event
