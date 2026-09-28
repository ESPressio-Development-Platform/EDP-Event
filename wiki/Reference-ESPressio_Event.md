# Reference — `src/ESPressio_Event.hpp`

**Classification:** PUBLIC ENTRY POINT  
**Source:** [`src/ESPressio_Event.hpp`](../src/ESPressio_Event.hpp)

Umbrella include for the complete supported EDP-Event consumer surface. It aggregates `event/Event.hpp` and intentionally contains no retained state or runtime logic. Consumers should prefer this header unless they deliberately need one focused module for compile-time hygiene.
