# Reference Index

Every maintained production header under `./src` is represented below. Reference pages classify public/internal/private contracts and explain the state/lifetime/invariants that must remain true when the implementation changes.

- [`src/ESPressio_Event.hpp`](Reference-ESPressio_Event) — Public umbrella entry point.
- [`src/event/Bootstrap.hpp`](Reference-event-Bootstrap) — Bootstrap/Composition validation and Runtime ownership.
- [`src/event/Composition.hpp`](Reference-event-Composition) — Event Composition capabilities and requirements.
- [`src/event/Deployment.hpp`](Reference-event-Deployment) — Event Type/deployment/Observe/SharedPending declarations.
- [`src/event/Event.hpp`](Reference-event-Event) — Focused Event aggregation header.
- [`src/event/EventFamily.hpp`](Reference-event-EventFamily) — Primitive-family identity and planner binding.
- [`src/event/EventTypes.hpp`](Reference-event-EventTypes) — Results, scopes, retention requests and scoped result wrappers.
- [`src/event/Integration.hpp`](Reference-event-Integration) — Outbound-only scoped integration helper.
- [`src/event/MemoryPlan.hpp`](Reference-event-MemoryPlan) — Planner-derived Memory pool specification.
- [`src/event/Occurrence.hpp`](Reference-event-Occurrence) — Occurrence layout/lifetime/recipient/borrow contract.
- [`src/event/Planner.hpp`](Reference-event-Planner) — Family planner, normalized topology and resources.
- [`src/event/Reservation.hpp`](Reference-event-Reservation) — Transactional ingress, ordered egress and remote Event operation capabilities.
- [`src/event/Retention.hpp`](Reference-event-Retention) — Common retention normalization.
- [`src/event/Runtime.hpp`](Reference-event-Runtime) — Admission, subscriptions, delivery, expiry and scoped Runtime.
