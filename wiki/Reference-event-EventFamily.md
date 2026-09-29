# Reference — `src/event/EventFamily.hpp`

**Source:** [`src/event/EventFamily.hpp`](../src/event/EventFamily.hpp)

## `Planner` — PUBLIC FORWARD DECLARATION

Forward declaration of Event's canonical family planner, allowing `Family::Planner` to name the planner without pulling the implementation into this foundational header.

## `Family` — PUBLIC PRIMITIVE FAMILY TAG

`Family` identifies Event to EDP-Primitives. `Identifier` is the authoritative stable `PrimitiveFamilyIdentifier`; `Planner` aliases `Event::Planner`.
