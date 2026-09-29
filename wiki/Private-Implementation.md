# Private Implementation

`Event::Detail` contains compile-time filtering/planning machinery and bounded Runtime bookkeeping. It is private implementation unless a symbol is explicitly described as an internal provider contract elsewhere.

Key private mechanisms include declaration predicates/filters, Type ordinal/list utilities, deployment lookup, observation/listener derivation, ResourcePlan construction, `NormalizedEventPlan`, per-Type `AdmissionState`/`TypeState`, zero-state listener cursors, Shared Pending accounting, Memory-backed `RecordView`, and compile-time topology validation.

These helpers deliberately derive bookkeeping from semantic declarations rather than making widths/ordinals/layout configurable. Their observable contract is the normalized public/runtime behavior, not their exact helper spelling.
