# Reference — `src/event/EventFamily.hpp`

**Source:** [`src/event/EventFamily.hpp`](../src/event/EventFamily.hpp)


## `WireOperationVersion` / `WireOperation` — PUBLIC WIRE VOCABULARY

`WireOperationVersion` is `1`. `WireOperation::Occurrence` is the stable non-zero family-owned semantic operation code used when an Event occurrence is carried by Mesh. The version/code belong to Event, not to Mesh transport, routing or security.

## `Planner` — PUBLIC FORWARD DECLARATION

Forward declaration of Event's canonical family planner, allowing `Family::Planner` to name the planner without pulling the implementation into this foundational header.

## `Family` — PUBLIC PRIMITIVE FAMILY TAG

`Family` identifies Event to EDP-Primitives. `Identifier` is the authoritative stable `PrimitiveFamilyIdentifier`; `Planner` aliases `Event::Planner`.
