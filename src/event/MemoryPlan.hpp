#pragma once

#include <ESPressio_Memory.hpp>

#include "Occurrence.hpp"

namespace ESPressio::Event {

    /// Planner-derived EDP-Memory Object Pool declaration for one locally deployed Event Type.
    ///
    /// The application chooses only the Memory resource selection. Event owns the occurrence
    /// record Type, exact MaximumInstances capacity, and the no-shared-overflow policy.
    /// @tparam TPlan Normalized Event plan defining the local deployment.
    /// @tparam TEvent Locally deployed Event Type whose occurrence records are pooled.
    /// @tparam TMemoryResourceSelection EDP-Memory resource placement selection for the dedicated pool.
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
