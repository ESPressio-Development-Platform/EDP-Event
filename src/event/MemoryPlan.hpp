#pragma once

#include <ESPressio_Memory.hpp>

#include "Occurrence.hpp"

namespace ESPressio::Event {

    /// Planner-derived EDP-Memory Object Pool declaration for one locally deployed Event Type.
    ///
    /// The application chooses only the Memory resource selection. Event owns the occurrence
    /// record Type, exact MaximumInstances capacity, and the no-shared-overflow policy.
    template<
        class TPlan,
        class TEvent,
        class TMemoryResourceSelection = Memory::UseDefaultMemoryResource
    >
    requires EventType<TEvent> && TPlan::template IsDeployed<TEvent>
    using OccurrencePoolSpec = Memory::ObjectPoolSpec<
        OccurrenceRecord<TPlan, TEvent>,
        Memory::DedicatedInstances<
            TPlan::template Deployment<TEvent>::MaximumInstances
        >,
        Memory::NoSharedOverflow,
        TMemoryResourceSelection
    >;

} // ESPressio::Event
