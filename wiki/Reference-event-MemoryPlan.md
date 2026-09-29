# Reference — `src/event/MemoryPlan.hpp`

**Classification:** PUBLIC RESOURCE-PLANNING API  
**Source:** [`src/event/MemoryPlan.hpp`](../src/event/MemoryPlan.hpp)

## `OccurrencePoolSpec<TPlan,TEvent,TMemoryResourceSelection>`

Alias generating the exact EDP-Memory `ObjectPoolSpec` required for one local Event Type.

- `TPlan` — normalized Event plan that owns deployment/resource truth.
- `TEvent` — locally deployed Event Type whose planner-shaped occurrence record is pooled.
- `TMemoryResourceSelection` — optional EDP-Memory resource selection; defaults to topology default. This is the only application-selected Memory placement dimension.

The generated pool object Type is `OccurrenceRecord<TPlan,TEvent>`, dedicated capacity equals the deployment `MaximumInstances`, and shared raw Memory overflow is always disabled. This preserves Event's exact per-Type physical-capacity contract.
