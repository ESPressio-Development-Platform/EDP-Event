# ESPressio Event

`EDP-Event` is the ESPressio Development Platform's deterministic bounded broadcast Primitive. One Dispatch represents one immutable occurrence of a concrete Event Type and may be delivered to zero or more currently subscribed Listener Dedicated Threads.

Event owns admission, retention, subscription and delivery semantics. EDP-Memory owns occurrence storage and object lifetime; EDP-BoundedTopology supplies compact finite identity/membership/Queue mechanics; EDP-Threading owns Listener execution and advisory wake delivery; EDP-Clock supplies canonical monotonic deadlines. Encoding and Transport remain outside Event.

## Public facilities

Include:

```cpp
#include <ESPressio_Event.hpp>
```

The core vocabulary is:

- `Deploy<TEvent, MaximumInstances, Admission, Retention>` — declares one locally admissible Event Type.
- `Queue<DedicatedPending>` — FIFO pending delivery with exact dedicated pending entitlement.
- `NewestOnly` — retains only the newest still-pending occurrence while preserving active callback borrows.
- `UntilHandoffOnly` / `TimedRetention` — selects whether local occurrence records can retain deadlines.
- `Observe<TThreadIdentity,TEvent>` — immutable Listener/Event eligibility.
- `SharedPending<N>` — optional family-wide Queue overflow entitlement; it is not a physical storage pool.
- `PlanFor<TTopology>` — resolves the normalized Event family plan from a Primitives topology.
- `OccurrencePoolSpec<TPlan,TEvent>` — derives the exact EDP-Memory pool required by one deployed Event Type.
- `Bootstrap<...>` / `Runtime<...>` — compile-time validated construction and bounded runtime operations.
- `Subscribe`, `Unsubscribe`, `Dispatch`, `Ingress` and `Drain` — runtime Event operations.
- `LocalOnly`, `RemoteOnly`, `LocalAndRemote` — execution-domain scope vocabulary.

## Documentation

- [Architecture](./docs/ARCHITECTURE.MD) — ownership boundaries, family planning, synchronization and ordering.
- [Event Types and Deployment](./docs/EVENT-TYPES-AND-DEPLOYMENT.MD) — payload qualification, deployment, Listener eligibility and Memory planning.
- [Admission and Retention](./docs/ADMISSION-AND-RETENTION.MD) — Queue/NewestOnly capacity, results, expiry and move/copy guarantees.
- [Listeners and Subscriptions](./docs/LISTENERS-AND-SUBSCRIPTIONS.MD) — Dedicated Thread identity, callback providers, runtime subscription and bounded drain.
- [Delivery and Lifetime](./docs/DELIVERY-AND-LIFETIME.MD) — immutable borrow, active-borrow lifetime and occurrence reclamation.
- [Concurrency and ISR Boundary](./docs/CONCURRENCY-AND-ISR.MD) — the one Event mutex, callback reentrancy, advisory wake and ordinary-context-only rule.
- [Transport Contract](./docs/TRANSPORT-CONTRACT.MD) — Local/Remote scopes, inbound handoff and provider-result preservation.
- [Resource Planning](./docs/RESOURCE-PLANNING.MD) — exact physical pools, pending entitlements and compile-away representation.
- [Building and Validation](./docs/BUILDING.MD) — C++20, PIOArduino setup, dependency baseline and executable validation commands.
- [Dependency graph](./DEPENDENCY_GRAPH.MD) — mandatory dependency directions and deliberate non-dependencies.

## Complete example — TemperatureMonitor

The walkthrough below constructs a complete application which emits `TemperatureChanged{2345}`, wakes a multi-purpose Dedicated Thread, drains Event work there, and observes `23.45 C`. The exact buildable source is maintained at [`examples/TemperatureMonitor`](./examples/TemperatureMonitor); the snippets below are taken from that same application rather than from an unrelated toy API.

### 1. Define the Event Type

An Event Type is its own payload. Give it a stable Primitive identifier and identify the Event family:

```cpp
struct TemperatureChanged final {
    inline static constexpr ESPressio::System::TypeIdentifier Identifier{
        ESPressio::System::TypeIdentifier::Storage{
            0x45, 0x44, 0x50, 0x00, 0x00, 0x00, 0x00, 0x01
        }
    };

    using Family = ESPressio::Event::Family;
    std::int16_t CelsiusX100{0};
};
```

Because Event may outlive the producer until every Listener callback has returned, retained payloads must be recursively memory-bounded and self-contained. Certify the owned member graph with EDP-BoundedTypes:

```cpp
namespace ESPressio::Bounded {

    template<>
    struct MemoryBoundedTraits<Demo::TemperatureChanged> :
        MemoryBoundedValueDeclaration<false, std::int16_t> {};

} // ESPressio::Bounded
```

The `false` states that the value has no external lifetime dependency.

### 2. Give the Listener a Dedicated Thread identity

Event does not create a worker. The Listener is an application-owned persistent Threading context:

```cpp
struct TemperatureListenerThread final {};
```

### 3. Declare the Event topology

This application allows four simultaneously live `TemperatureChanged` occurrences, two dedicated FIFO pending entitlements, two family-wide shared pending slots, and one eligible Listener:

```cpp
using PrimitiveTopology = ESPressio::Primitives::Topology<
    ESPressio::Event::Deploy<
        TemperatureChanged,
        4U,
        ESPressio::Event::Queue<2U>,
        ESPressio::Event::UntilHandoffOnly
    >,
    ESPressio::Event::Observe<
        TemperatureListenerThread,
        TemperatureChanged
    >,
    ESPressio::Event::SharedPending<2U>
>;

using EventPlan = ESPressio::Event::PlanFor<PrimitiveTopology>;
```

`UntilHandoffOnly` means this Type has no retained deadline field. Use `TimedRetention` when individual Dispatch calls need duration/deadline expiry.

### 4. Derive the exact Memory pool

The application chooses the Memory provider; Event derives occurrence shape, exact physical capacity and `NoSharedOverflow`:

```cpp
using TemperaturePool = ESPressio::Event::OccurrencePoolSpec<
    EventPlan,
    TemperatureChanged
>;

using MemoryTopology = ESPressio::Memory::MemoryTopology<
    MemoryResourceProvider,
    ESPressio::Memory::SharedReserve<0U, MemoryResourceProvider>,
    TemperaturePool
>;
```

The complete example then creates a normal `EDP-Memory::MemoryRuntime` over that topology and initializes it before Event.

### 5. Declare the Dedicated Thread

```cpp
using ThreadingTopology = ESPressio::Threading::ThreadingTopology<
    ESPressio::Threading::DedicatedThread<
        TemperatureListenerThread,
        ESPressio::Threading::StackCapacity<4096U>,
        ESPressio::Threading::Priority<ESPressio::Threading::ThreadPriority::Normal>,
        ESPressio::Threading::AnyAffinity
    >
>;
```

The Thread is deliberately multi-purpose. Its loop drains a bounded amount of Event work, can service any other application responsibilities, and then waits indefinitely when no immediate work remains:

```cpp
void operator ()(
    ESPressio::Threading::ThreadContext& context
) noexcept {
    while (!context.IsStopRequested()) {
        if (Control->Runtime != nullptr) {
            const auto drained = Control->Drain(
                Control->Runtime,
                8U
            );

            if (drained.WorkRemaining) {
                continue;
            }
        }

        if (
            context.Wait() !=
            ESPressio::Threading::ThreadWaitResult::Woken
        ) {
            return;
        }
    }
}
```

Waiting forever is valid: Event work, other application work and Threading lifecycle/termination paths all use the same Dedicated Thread advisory wake mechanism.

### 6. Provide the typed callback capability

The callback provider is selected statically by `(Thread identity, Event Type)`:

```cpp
struct TemperatureHandler final : ESPressio::System::CompositionFramework::Provider<
    ESPressio::Event::Composition::Domain,
    ESPressio::System::CompositionFramework::Offers<
        ESPressio::System::CompositionFramework::Offer<
            ESPressio::Event::Composition::ListenerCallback<
                TemperatureListenerThread,
                TemperatureChanged
            >
        >
    >
> {
    void OnEvent(
        const TemperatureChanged& event
    ) noexcept {
        Last.store(
            event.CelsiusX100,
            std::memory_order_relaxed
        );
        Count.fetch_add(
            1U,
            std::memory_order_release
        );
    }
};
```

`event` is an immutable borrow valid only for the callback. Event releases its internal mutex before calling this function, so the callback may safely Dispatch/Subscribe/Unsubscribe again.

### 7. Supply Event's one ordinary-context mutex and Architecture

```cpp
using EventMutex = ESPressio::Threading::OrdinaryMutexProvider<
    ESPressio::Event::Composition::RuntimeMutexIdentity,
    MutexProvider
>;

using EventComposition = ESPressio::System::CompositionFramework::Composition<
    ESPressio::Event::Composition::Domain,
    TemperatureHandler
>;

using ThreadingComposition = ESPressio::System::CompositionFramework::Composition<
    ESPressio::Threading::Domain,
    ThreadingTopology,
    EventMutex
>;

using Architecture = ESPressio::System::CompositionFramework::Architecture<
    EventComposition,
    ThreadingComposition
>;
```

Bootstrap now has enough compile-time information to prove the Listener Thread, callback and mutex requirements.

### 8. Initialize the owning runtimes, then Event

The complete example constructs and initializes Memory first, then the Threading infrastructure, then Event:

```cpp
using EventBootstrap = ESPressio::Event::Bootstrap<
    Architecture,
    EventPlan,
    MemoryRuntime,
    ThreadingRuntime
>;

EventMutex eventMutex;
TemperatureHandler handler;
EventBootstrap eventBootstrap(
    memory,
    threading,
    eventMutex,
    handler
);

if (eventBootstrap.Initialize() != ESPressio::Event::InitializationResult::Initialized) {
    // Bootstrap failed.
}

auto& events = eventBootstrap.RuntimeInstance();
```

For a `TimedRetention` deployment, bind the application-wide EDP-Clock monotonic clock before Event initialization.

### 9. Subscribe and start the Listener

```cpp
events.Subscribe<TemperatureListenerThread, TemperatureChanged>();
auto listener = threading.ThreadHandle<TemperatureListenerThread>();
listener.Start();
```

Subscription is a runtime toggle over the already-declared `Observe` relationship. It allocates nothing and cannot create a new relationship.

### 10. Emit the Event

```cpp
const auto result = events.Dispatch(
    TemperatureChanged{2345}
);

if (result != ESPressio::Event::DispatchResult::Accepted) {
    // Handle NoCapacity / Expired as applicable.
}
```

Dispatch snapshots current subscriptions, admits the occurrence into its bounded Type-local Memory pool, records the Listener's pending bit, and publishes an advisory wake to `TemperatureListenerThread`.

The Listener wakes, calls `Drain<TemperatureListenerThread>(8)`, and the callback sees `2345` (`23.45 C`).

### 11. Stop and shut down cooperatively

```cpp
listener.RequestStop();
while (listener.State() == ESPressio::Threading::ThreadState::Running) {
    ExecutionContextProvider::Yield();
}

threading.BeginShutdown();
while (!threading.IsExecutionQuiescent()) {
    ExecutionContextProvider::Yield();
}
threading.FinalizeShutdown();
memory.TearDown();
```

`RequestStop()` wakes an indefinitely waiting Dedicated Thread, so it can observe the stop request and return. Memory teardown occurs only after Event/Threading work no longer owns live occurrence state.

### Build the complete example

With PIOArduino's PlatformIO-compatible CLI:

```text
cd examples/TemperatureMonitor
pio run
```

For repository development, `tests/run_demo_builds.sh` uses coherent sibling-source includes so the demos build against the exact checked-out EDP repositories rather than mixing branch work with remote `main` packages.

## Admission modes at a glance

```text
Queue<D>
    same-Type FIFO pending order
    D dedicated pending entitlements
    optional family SharedPending overflow

NewestOnly
    no Queue entitlement
    at most one unclaimed pending occurrence
    new admission supersedes prior pending interests
    active callback borrows remain valid
```

## Transport boundary

Inbound concrete Transport implementations deserialize a valid typed Event and call `Ingress`, which is LocalOnly. Outbound integration supplies a bounded typed callable. Event never sees bytes or defines codec/retry/acknowledgement semantics.

`RemoteOnly` and `LocalAndRemote` retain the provider-defined remote result inside `RemoteDispatchAttempt<T>`. Event adds only whether the remote operation was `Attempted` or `SkippedExpired`.

See [`demos/transport-boundary`](./demos/transport-boundary) for an explicit fake-Transport example without introducing a dependency on a concrete Transport subsystem.

## Validation

The repository host runner validates planner/runtime/layout behavior, compile-fail contracts and standalone public headers:

```text
python3 tests/run_tests.py
```

The PIOArduino demo matrix is:

```text
bash tests/run_demo_builds.sh
```

The complete README example is validated independently with:

```text
bash tests/run_example_build.sh
```

The implementation is ordinary-context only; no Event Runtime API is ISR-safe.

## Dependencies

```text
EDP-Event
├── EDP-System
├── EDP-Primitives
├── EDP-Memory
├── EDP-Threading
├── EDP-Clock
├── EDP-BoundedTopology
└── EDP-BoundedTypes
```

On the current `event_v1` development branch, `EDP-Memory` and `EDP-Threading` must also be consumed from their `event_v1` branches because the compact indexed-pool and Dedicated-Thread wake/mutex prerequisites have not yet landed on those repositories' `main` branches. `library.json`, the PIOArduino demo projects and `examples/TemperatureMonitor` pin those prerequisite refs explicitly; Arduino IDE users must check out the same branches manually.

See [`DEPENDENCY_GRAPH.MD`](./DEPENDENCY_GRAPH.MD) for ownership and direction details.

## C++ language baseline

ESPressio uses C++20. For VS Code ESP32 development, use PIOArduino rather than the legacy PlatformIO IDE extension; checked-in project files select PIOArduino `stable` and explicitly enable `-std=gnu++20`. See [Building and Validation](./docs/BUILDING.MD) for the exact configuration and validation commands.
