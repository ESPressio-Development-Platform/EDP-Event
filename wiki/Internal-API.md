# Internal API

EDP-Event intentionally exposes several reusable cross-module contracts which are not ordinary application-facing concepts.

- `Event::Composition::ListenerCallback<TThread,TEvent>` — exclusive typed callback capability used by Bootstrap/Architecture provider resolution.
- `Event::Composition::RuntimeMutexIdentity` and its requirement/provider aliases — identify the one Event Runtime ordinary mutex.
- `Event::Composition::ThreadingTopologyRequirement` — external-domain requirement allowing Bootstrap to prove Listener identities exist in Threading topology.
- `RuntimeProvider<TEventPlan>` — EDP-Primitives family-plan runtime-provider marker carrying `EventPlan`.
- `OccurrenceRecord<TPlan,TEvent>` — planner-shaped Memory-owned occurrence object consumed internally by Runtime and by the generated Memory pool specification.

These contracts may be consumed across EDP implementation modules but should not be treated as unconstrained compatibility points independent of the documented Event architecture.
