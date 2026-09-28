# Reference — `src/event/Retention.hpp`

**Classification:** INTERNAL SEMANTIC HELPER  
**Source:** [`src/event/Retention.hpp`](../src/event/Retention.hpp)

## `NormalizedRetention`

Structured normalized retention state. `Deadline` is the canonical absolute monotonic deadline (zero sentinel for UntilHandoff); `IsExpired` records whether the common dispatch precondition is already expired at normalization time.

## `NormalizeRetention(TRetention)`

Non-throwing helper shared by local and remote-scoped dispatch. `UntilHandoff` returns zero/nonexpired. `UntilDeadline` compares supplied deadline against canonical monotonic now. `ForDuration` resolves now plus positive duration to an absolute deadline with saturation at maximum timestamp and treats zero/nonpositive duration as expired. This ensures LocalOnly/RemoteOnly/LocalAndRemote share one expiry interpretation.
