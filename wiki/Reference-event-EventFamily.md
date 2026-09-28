# Reference — `src/event/EventFamily.hpp`

**Classification:** PUBLIC API  
**Source:** [`src/event/EventFamily.hpp`](../src/event/EventFamily.hpp)

## `Planner`

Forward declaration of the Event family planner so `Family` can identify its canonical planner without introducing include cycles.

## `Family`

Strong Event Primitive-family identity. Its nested `Planner` alias tells `EDP-Primitives` which family-specific planner must normalize opaque Event declarations. It has no runtime state and exists only in type space.
