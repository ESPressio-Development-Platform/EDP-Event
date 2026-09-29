# Reference — `src/event/Retention.hpp`

**Classification:** INTERNAL SEMANTIC HELPER  
**Source:** [`src/event/Retention.hpp`](../src/event/Retention.hpp)

## `Detail::NormalizedRetention` — PRIVATE IMPLEMENTATION VALUE

`Deadline` is the canonical absolute monotonic deadline. Zero is the structural sentinel for `UntilHandoff`. `Expired` records whether the request had already expired at normalization time.

## `Detail::NormalizeRetention<TRetention>(retention)` — INTERNAL HELPER

`TRetention` must satisfy public `RetentionRequest`. The function is non-throwing, allocates nothing, and produces one `NormalizedRetention` shared by local and remote-scoped Dispatch paths. It uses canonical `Clock::MonotonicNow()`, handles zero/non-positive duration as expired, and saturates duration addition at the maximum representable timestamp.
