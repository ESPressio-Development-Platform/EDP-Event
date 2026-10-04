#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

#include <ESPressio_Clock.hpp>
#include <ESPressio_Memory.hpp>
#include <ESPressio_Threading.hpp>

#include "Composition.hpp"
#include "Occurrence.hpp"
#include "Reservation.hpp"
#include "Retention.hpp"

namespace ESPressio::Event {

    namespace Detail {

        /// Compact exact-use counter which compiles away for zero capacity.
        template<std::size_t TCapacity, bool TEnabled = (TCapacity > 0U)>
        struct CapacityCounter final {
            using Storage = CountStorage<TCapacity>;
            Storage Used{0U};

            [[nodiscard]] std::size_t Count() const noexcept {
                return static_cast<std::size_t>(Used);
            }

            void Increment() noexcept {
                ++Used;
            }

            void Decrement() noexcept {
                --Used;
            }
        };

        template<std::size_t TCapacity>
        struct CapacityCounter<TCapacity, false> final {
            [[nodiscard]] constexpr std::size_t Count() const noexcept {
                return 0U;
            }

            constexpr void Increment() noexcept {
            }

            constexpr void Decrement() noexcept {
            }
        };


        /// Generation and entitlement state for one physical unpublished ingress slot.
        struct IngressSlotState final {
            std::uint32_t Generation{0U};
            bool Reserved{false};
            bool Cancelled{false};
            bool SharedPending{false};
        };


        /// Exact per-Type ingress reservation metadata indexed by physical occurrence slot.
        template<class TPlan, class TEvent>
        struct IngressState final {
            std::array<
                IngressSlotState,
                TPlan::template Deployment<TEvent>::MaximumInstances
            > Slots{};
        };


        /// One caller-owned Event borrow in the local ordered handoff sequencer.
        template<class TEvent>
        struct OutboundHandoffSlot final {
            const TEvent* EventView{nullptr};
            MonotonicTimestamp Deadline{};
            std::uint32_t Generation{0U};
            bool Occupied{false};
            bool Terminal{false};
            bool InFlight{false};
        };


        /// Positive-capacity ring preserving local handoff-opportunity order without payload copies.
        template<class TPlan, class TEvent, std::size_t TCapacity = TPlan::template RemoteHandoffCapacity<TEvent>>
        struct OutboundSequencer final {
            std::array<OutboundHandoffSlot<TEvent>, TCapacity> Slots{};
            CountStorage<TCapacity> Head{0U};
            CountStorage<TCapacity> Count{0U};
        };


        /// Zero-capacity sequencer has no retained state.
        template<class TPlan, class TEvent>
        struct OutboundSequencer<TPlan, TEvent, 0U> final {};


        /// Admission-state primary declaration selected by one deployment's Queue shape.
        /// @tparam TPlan Normalized Event plan.
        /// @tparam TEvent Locally deployed Event payload Type.
        /// @tparam TQueue Derived flag selecting FIFO Queue or NewestOnly state.
        template<
            class TPlan,
            class TEvent,
            bool TQueue = TPlan::template IsQueue<TEvent>
        >
        struct AdmissionState;


        /// FIFO Queue admission state retaining only the intrusive Queue endpoints.
        /// @tparam TPlan Normalized Event plan.
        /// @tparam TEvent Queue-deployed Event payload Type.
        template<class TPlan, class TEvent>
        struct AdmissionState<TPlan, TEvent, true> final {

            // Queue identity/topology metadata.

            /// Planner-shaped occurrence record Type for this Event Type.
            using Record = OccurrenceRecord<TPlan, TEvent>;

            /// Strong compact occurrence identity used by pending Queue topology.
            using Index = typename Record::OccurrenceIndex;

            /// Bounded non-owning FIFO Queue over Memory-owned occurrence records.
            using Queue = BoundedTopology::IntrusiveQueue<
                typename Index::IndexSpace,
                Record::MaximumInstances
            >;

            // Pending Queue state.

            /// FIFO of occurrences with at least one pending Listener interest.
            Queue Pending{};

            /// Exact committed-plus-reserved use of Type-local dedicated pending entitlement.
            [[no_unique_address]] CapacityCounter<
                TPlan::template DedicatedPendingCapacity<TEvent>
            > DedicatedUsed{};

        };


        /// NewestOnly admission state retaining at most one pending occurrence identity.
        /// @tparam TPlan Normalized Event plan.
        /// @tparam TEvent NewestOnly-deployed Event payload Type.
        template<class TPlan, class TEvent>
        struct AdmissionState<TPlan, TEvent, false> final {

            // Occurrence identity metadata.

            /// Planner-shaped occurrence record Type for this Event Type.
            using Record = OccurrenceRecord<TPlan, TEvent>;

            /// Strong compact occurrence identity used by NewestOnly pending state.
            using Index = typename Record::OccurrenceIndex;

            // Pending latest-value state.

            /// Current pending latest occurrence, or Invalid when no uncommitted occurrence exists.
            Index Pending = Index::Invalid();

        };


        /// Complete mutable Event-core state owned for one locally deployed Event Type.
        /// @tparam TPlan Normalized Event plan.
        /// @tparam TEvent Locally deployed Event payload Type.
        template<class TPlan, class TEvent>
        struct TypeState final {

            // Planner-shaped Type metadata.

            /// Planner-shaped occurrence record Type for this Event Type.
            using Record = OccurrenceRecord<TPlan, TEvent>;

            // Authoritative runtime state.

            /// Active runtime subscription bits for every statically eligible Listener.
            [[no_unique_address]] typename Record::ListenerSet Subscriptions{};

            /// Queue/NewestOnly pending topology state selected by the deployment.
            AdmissionState<TPlan, TEvent> Admission{};

            /// Physical-slot generation and unpublished ingress ownership.
            IngressState<TPlan, TEvent> Ingress{};

            /// Bounded local ordering for outbound handoff opportunities.
            [[no_unique_address]] OutboundSequencer<TPlan, TEvent> Outbound{};

        };


        /// Maps a local Event TypeList to one tuple containing each Type's runtime state.
        /// @tparam TPlan Normalized Event plan.
        /// @tparam TList Locally deployed Event payload TypeList.
        template<class TPlan, class TList>
        struct TypeStateTuple;


        /// TypeList specialization materializing TypeState for every local Event Type.
        /// @tparam TPlan Normalized Event plan.
        /// @tparam TEvents Locally deployed Event payload Types.
        template<class TPlan, class... TEvents>
        struct TypeStateTuple<
            TPlan,
            Primitives::TypeList<TEvents...>
        > final {

            /// Tuple of per-Type mutable Event-core states.
            using Type = std::tuple<TypeState<TPlan, TEvents>...>;

        };


        /// Compact cross-Type round-robin continuation state for a Listener observing multiple Event Types.
        /// @tparam TPlan Normalized Event plan.
        /// @tparam TThreadIdentity Listener Dedicated Thread identity.
        /// @tparam TCount Number of Event Types observed by this Listener.
        template<
            class TPlan,
            class TThreadIdentity,
            std::size_t TCount = TPlan::template ObservedEventTypes<TThreadIdentity>::Count
        >
        struct ListenerCursor final {

            // Compact cursor representation.

            /// Minimum-width scalar capable of representing the observed-Type cardinality.
            using Storage = CountStorage<TCount>;

            /// Next observed-Type ordinal at which bounded drain scanning begins.
            Storage Value{0U};

        };


        /// Stateless Listener cursor when a Listener observes no Event Type.
        /// @tparam TPlan Normalized Event plan.
        /// @tparam TThreadIdentity Listener Dedicated Thread identity.
        template<class TPlan, class TThreadIdentity>
        struct ListenerCursor<TPlan, TThreadIdentity, 0U> final {};


        /// Stateless Listener cursor when a Listener observes exactly one Event Type.
        /// @tparam TPlan Normalized Event plan.
        /// @tparam TThreadIdentity Listener Dedicated Thread identity.
        template<class TPlan, class TThreadIdentity>
        struct ListenerCursor<TPlan, TThreadIdentity, 1U> final {};


        /// Maps the unique Listener TypeList to one tuple of optional round-robin cursors.
        /// @tparam TPlan Normalized Event plan.
        /// @tparam TList Unique Listener Dedicated Thread TypeList.
        template<class TPlan, class TList>
        struct ListenerCursorTuple;


        /// TypeList specialization materializing one cursor representation per Listener.
        /// @tparam TPlan Normalized Event plan.
        /// @tparam TThreadIdentities Unique Listener Thread identities.
        template<class TPlan, class... TThreadIdentities>
        struct ListenerCursorTuple<
            TPlan,
            Primitives::TypeList<TThreadIdentities...>
        > final {

            /// Tuple of per-Listener cross-Type continuation cursors.
            using Type = std::tuple<ListenerCursor<TPlan, TThreadIdentities>...>;

        };


        /// Compact family-wide SharedPending usage counter for positive capacity.
        /// @tparam TCapacity Exact family-wide logical SharedPending capacity.
        /// @tparam TEnabled Derived positive-capacity flag selecting retained state.
        template<std::size_t TCapacity, bool TEnabled = (TCapacity > 0U)>
        struct SharedPendingCounter final {

            // Compact shared-entitlement representation.

            /// Minimum-width scalar capable of representing SharedPending usage.
            using Storage = CountStorage<TCapacity>;

            /// Number of Queue occurrences currently consuming SharedPending entitlement.
            Storage Used{0U};

            // Shared-entitlement accounting.

            /// Returns the current number of consumed SharedPending slots.
            [[nodiscard]] std::size_t Count() const noexcept {
                return static_cast<std::size_t>(Used);
            }

            /// Consumes one SharedPending slot after successful Queue admission.
            void Increment() noexcept {
                ++Used;
            }

            /// Releases one SharedPending slot when a pending Queue occurrence leaves overflow territory.
            void Decrement() noexcept {
                --Used;
            }

        };


        /// Stateless SharedPending counter used when family-wide capacity is zero.
        /// @tparam TCapacity Zero capacity retained for specialization identity.
        template<std::size_t TCapacity>
        struct SharedPendingCounter<TCapacity, false> final {

            /// Reports zero SharedPending use without retaining state.
            [[nodiscard]] constexpr std::size_t Count() const noexcept {
                return 0U;
            }

            /// Performs no work because no SharedPending entitlement exists.
            constexpr void Increment() noexcept {
            }

            /// Performs no work because no SharedPending entitlement exists.
            constexpr void Decrement() noexcept {
            }

        };


        /// Adapts one EDP-Memory dedicated occurrence pool to IntrusiveQueue's record-indexing contract.
        /// @tparam TPlan Normalized Event plan.
        /// @tparam TEvent Locally deployed Queue Event payload Type.
        /// @tparam TPool EDP-Memory dedicated Object Pool Type containing occurrence records.
        template<class TPlan, class TEvent, class TPool>
        class RecordView final {

            private:

                // Borrowed pool binding.

                /// Memory-owned pool containing the indexed occurrence records.
                TPool* _pool;

            public:

                /// Binds this non-owning record view to one Memory pool.
                /// @param pool Memory-owned dedicated occurrence pool.
                explicit RecordView(
                    TPool& pool
                ) noexcept :
                    _pool(&pool) {
                }

                /// Resolves one raw Queue ordinal to the corresponding Memory-owned occurrence record.
                /// @param index Zero-based physical occurrence slot ordinal proven valid by Queue topology.
                OccurrenceRecord<TPlan, TEvent>& operator[](
                    std::size_t index
                ) noexcept {
                    return _pool->DedicatedObject(
                        TPool::DedicatedIndex::FromUnchecked(index)
                    );
                }

        };


        /// Compile-time predicate determining whether any local Event deployment can retain a timed deadline.
        /// @tparam TList Locally deployed Event payload TypeList.
        template<class TList>
        struct AnyTimedDeployment;


        /// TypeList specialization folding SupportsTimedRetention across local Event Types.
        /// @tparam TEvents Locally deployed Event payload Types.
        template<class... TEvents>
        struct AnyTimedDeployment<Primitives::TypeList<TEvents...>> {

            /// Evaluates timed-retention support against one normalized Event plan.
            /// @tparam TPlan Normalized Event plan providing per-Type retention capability.
            template<class TPlan>
            static consteval bool For() {
                return (
                    TPlan::template SupportsTimedRetention<TEvents> ||
                    ... ||
                    false
                );
            }

        };


        /// Result of one bounded internal Listener delivery attempt.
        enum class DeliveryAttemptResult : std::uint8_t {
            /// One pending occurrence was claimed and its callback completed.
            Delivered = 0U,

            /// No pending occurrence was available for the selected Listener/Event pair.
            NoPendingOccurrence = 1U
        };

    } // ESPressio::Event::Detail


    /// Bounded Runtime realizing one normalized Event-family plan over application-owned dependencies.
    /// @tparam TPlan Normalized Event plan owning immutable deployment/Listener/resource truth.
    /// @tparam TArchitecture Application Composition Architecture used for typed callback provider resolution.
    /// @tparam TMemoryRuntime Application-owned EDP-Memory Runtime containing exact Event occurrence pools.
    /// @tparam TThreadingRuntime Application-owned EDP-Threading Runtime containing Listener Dedicated Threads.
    /// @tparam TMutexProvider Selected EDP-Threading ordinary mutex provider serializing Event-core mutation.
    /// @tparam TCallbackProviders Unique typed Listener callback provider Types derived from Observe topology.
    template<
        class TPlan,
        class TArchitecture,
        class TMemoryRuntime,
        class TThreadingRuntime,
        class TMutexProvider,
        class... TCallbackProviders
    >
    class Runtime final : public RuntimeProvider<TPlan> {

        private:

            // Planner-derived Type metadata.

            /// Locally deployed Event payload TypeList.
            using PrimitiveTypes = typename TPlan::PrimitiveTypes;

            /// Unique Listener Dedicated Thread identity TypeList.
            using Listeners = typename TPlan::Listeners;

            /// Immutable Observe declaration TypeList.
            using Observations = typename TPlan::Observations;

            /// Tuple containing mutable state for every local Event Type.
            using TypeStates = typename Detail::TypeStateTuple<
                TPlan,
                PrimitiveTypes
            >::Type;

            /// Tuple containing optional cross-Type continuation state for every Listener.
            using ListenerCursors = typename Detail::ListenerCursorTuple<
                TPlan,
                Listeners
            >::Type;

            /// Exact unique callback provider TypeList required by the plan/Architecture.
            using RequiredCallbackProviders = typename Detail::RequiredCallbackProviders<
                TArchitecture,
                Observations
            >::Type;

            /// Callback provider TypeList actually bound to this Runtime instantiation.
            using BoundCallbackProviders = Primitives::TypeList<TCallbackProviders...>;

            static_assert(
                std::is_same_v<RequiredCallbackProviders, BoundCallbackProviders>,
                "Event Runtime callback provider bindings must exactly match the unique providers resolved by Architecture"
            );

            // Borrowed owning-domain/provider bindings.

            /// Application-owned Memory Runtime; must outlive this Event Runtime.
            TMemoryRuntime* _memory;

            /// Application-owned Threading Runtime; must outlive this Event Runtime.
            TThreadingRuntime* _threading;

            /// Application-owned Event ordinary mutex provider; must outlive this Event Runtime.
            TMutexProvider* _mutex;

            /// Unique typed callback providers selected by Architecture; provider objects remain application-owned.
            std::tuple<TCallbackProviders*...> _callbacks;

            // Authoritative bounded Event-core state.

            /// Per-Type subscriptions and Queue/NewestOnly pending topology.
            TypeStates _types{};

            /// Optional per-Listener round-robin cursors; zero/one-Type forms compile away.
            [[no_unique_address]] ListenerCursors _listenerCursors{};

            /// Family-wide SharedPending accounting; zero-capacity form compiles away.
            [[no_unique_address]] Detail::SharedPendingCounter<TPlan::SharedPendingCapacity> _sharedPending{};

            /// Indicates whether required owning-domain preconditions have been established.
            bool _initialized{false};

            /// Admission gate for unpublished ingress and ordered outbound integration reservations.
            bool _integrationOpen{false};

            // Infrastructure failure boundary.

            /// Terminates when an owning-domain provider violates an invariant that Event cannot represent as a semantic Dispatch result.
            [[noreturn]] static void InfrastructureFailure() noexcept {
                std::terminate();
            }

            // Event-core synchronization.

            /// Acquires the one Event Runtime ordinary-context mutex indefinitely.
            void Lock() noexcept {
                if (_mutex->Acquire() != Threading::OrdinaryMutexAcquireResult::Acquired) {
                    InfrastructureFailure();
                }
            }

            /// Releases the one Event Runtime ordinary-context mutex.
            void Unlock() noexcept {
                if (_mutex->Release() != Threading::OrdinaryMutexReleaseResult::Released) {
                    InfrastructureFailure();
                }
            }

            // Per-Type Memory/state aliases.

            /// Planner-shaped occurrence record Type for one local Event Type.
            /// @tparam TEvent Locally deployed Event payload Type.
            template<class TEvent>
            using Record = OccurrenceRecord<TPlan, TEvent>;

            /// EDP-Memory Object Pool Type containing one local Event Type's occurrence records.
            /// @tparam TEvent Locally deployed Event payload Type.
            template<class TEvent>
            using Pool = typename TMemoryRuntime::template ObjectPoolType<Record<TEvent>>;

            /// Mutable per-Type Event state Type.
            /// @tparam TEvent Locally deployed Event payload Type.
            template<class TEvent>
            using State = Detail::TypeState<TPlan, TEvent>;

            // Per-Type state and Memory access.

            /// Returns mutable Event-core state for one local Event Type.
            /// @tparam TEvent Locally deployed Event payload Type.
            template<class TEvent>
            State<TEvent>& StateFor() noexcept {
                return std::get<State<TEvent>>(_types);
            }

            /// Returns the Memory-owned dedicated occurrence pool for one local Event Type.
            /// @tparam TEvent Locally deployed Event payload Type.
            template<class TEvent>
            auto& PoolFor() noexcept {
                return _memory->template ObjectPoolFor<Record<TEvent>>();
            }

            /// Converts Memory's pool-local strong index into Event's semantically tagged occurrence index.
            /// @tparam TEvent Locally deployed Event payload Type.
            /// @param index Valid compact Memory dedicated-slot identity.
            template<class TEvent>
            typename Record<TEvent>::OccurrenceIndex ToEventIndex(
                typename Pool<TEvent>::DedicatedIndex index
            ) const noexcept {
                return Record<TEvent>::OccurrenceIndex::FromUnchecked(
                    static_cast<std::size_t>(index.Value())
                );
            }

            /// Converts Event's semantically tagged occurrence index into Memory's pool-local strong index.
            /// @tparam TEvent Locally deployed Event payload Type.
            /// @param index Valid Event occurrence identity.
            template<class TEvent>
            auto ToMemoryIndex(
                typename Record<TEvent>::OccurrenceIndex index
            ) noexcept {
                return Pool<TEvent>::DedicatedIndex::FromUnchecked(
                    static_cast<std::size_t>(index.Value())
                );
            }

            /// Resolves one Event occurrence identity to its Memory-owned record.
            /// @tparam TEvent Locally deployed Event payload Type.
            /// @param index Valid Event occurrence identity.
            template<class TEvent>
            Record<TEvent>& RecordAt(
                typename Record<TEvent>::OccurrenceIndex index
            ) noexcept {
                auto memoryIndex = ToMemoryIndex<TEvent>(index);
                return PoolFor<TEvent>().DedicatedObject(memoryIndex);
            }

            /// Releases one occurrence's physical Memory slot after pending interests and active borrows are exhausted.
            /// @tparam TEvent Locally deployed Event payload Type.
            /// @param index Valid Event occurrence identity being reclaimed.
            template<class TEvent>
            void ReleaseOccurrence(
                typename Record<TEvent>::OccurrenceIndex index
            ) noexcept {
                auto memoryIndex = ToMemoryIndex<TEvent>(index);
                const auto result = PoolFor<TEvent>().ReleaseDedicated(memoryIndex);

                if (result != Memory::DedicatedObjectPoolReleaseResult::Released) {
                    InfrastructureFailure();
                }
            }

            /// Creates a non-owning record view satisfying IntrusiveQueue indexing requirements.
            /// @tparam TEvent Queue-deployed Event payload Type.
            template<class TEvent>
            Detail::RecordView<TPlan, TEvent, Pool<TEvent>> Records() noexcept {
                return Detail::RecordView<TPlan, TEvent, Pool<TEvent>>(
                    PoolFor<TEvent>()
                );
            }

            // Queue/shared-pending accounting.

            /// Removes one Queue occurrence and releases its exact retained pending entitlement.
            /// @tparam TEvent Queue-deployed Event payload Type.
            /// @param index Pending occurrence identity being removed.
            template<class TEvent>
            void RemovePendingQueueOccurrence(
                typename Record<TEvent>::OccurrenceIndex index
            ) noexcept requires (TPlan::template IsQueue<TEvent>) {
                auto& record = RecordAt<TEvent>(index);
                const bool shared = record.ConsumesSharedPending();
                auto& admission = StateFor<TEvent>().Admission;
                auto records = Records<TEvent>();
                const auto removeResult = admission.Pending.Remove(records, index);

                if (removeResult != BoundedTopology::IntrusiveQueueRemoveResult::Removed) {
                    InfrastructureFailure();
                }

                if (shared) {
                    _sharedPending.Decrement();
                } else {
                    admission.DedicatedUsed.Decrement();
                }
            }

            // Opportunistic expiry.

            /// Removes expired pending interests for one Event Type without invalidating active callback borrows.
            /// @tparam TEvent Locally deployed Event payload Type.
            /// @param now Current canonical monotonic timestamp.
            template<class TEvent>
            void SweepExpired(
                MonotonicTimestamp now
            ) noexcept {
                if constexpr (!TPlan::template SupportsTimedRetention<TEvent>) {
                    static_cast<void>(now);
                } else if constexpr (TPlan::template IsQueue<TEvent>) {
                    auto& queue = StateFor<TEvent>().Admission.Pending;
                    auto current = queue.Head();
                    std::size_t visited = 0U;

                    while (
                        current.IsValid() &&
                        visited++ < Record<TEvent>::MaximumInstances
                    ) {
                        const auto next = RecordAt<TEvent>(current).QueueNext();
                        auto& record = RecordAt<TEvent>(current);

                        if (record.IsExpired(now)) {
                            record.PendingRecipients().ClearAll();
                            RemovePendingQueueOccurrence<TEvent>(current);

                            if (record.ActiveBorrowCount() == 0U) {
                                ReleaseOccurrence<TEvent>(current);
                            }
                        }

                        current = next;
                    }
                } else {
                    auto& pending = StateFor<TEvent>().Admission.Pending;

                    if (pending.IsValid()) {
                        auto& record = RecordAt<TEvent>(pending);

                        if (record.IsExpired(now)) {
                            const auto expired = pending;
                            record.PendingRecipients().ClearAll();
                            pending = Record<TEvent>::OccurrenceIndex::Invalid();

                            if (record.ActiveBorrowCount() == 0U) {
                                ReleaseOccurrence<TEvent>(expired);
                            }
                        }
                    }
                }
            }

            // Subscription snapshot and Listener wake publication.

            /// Returns one admission-time copy of active subscription bits for an Event Type.
            /// @tparam TEvent Locally deployed Event payload Type.
            template<class TEvent>
            typename Record<TEvent>::ListenerSet SnapshotSubscriptions() noexcept {
                return StateFor<TEvent>().Subscriptions;
            }

            /// Resolves one planned Listener Thread identity to its dense Type-local Listener index.
            /// @tparam TThreadIdentity Eligible Listener Dedicated Thread identity.
            /// @tparam TEvent Locally deployed Event payload Type.
            template<class TThreadIdentity, class TEvent>
            static constexpr auto ListenerIndexFor() noexcept {
                using Index = typename Record<TEvent>::ListenerIndex;

                return Index::FromUnchecked(
                    TPlan::template ListenerOrdinal<TEvent, TThreadIdentity>
                );
            }

            /// Signals every Listener whose bit belongs to one newly admitted recipient snapshot.
            /// @tparam TEvent Locally deployed Event payload Type.
            /// @tparam TIndex Current compile-time Listener ordinal visited by the recursion.
            /// @param recipients Admission-time pending recipient snapshot.
            template<class TEvent, std::size_t TIndex = 0U>
            void WakeRecipients(
                const typename Record<TEvent>::ListenerSet& recipients
            ) noexcept {
                using ListenerTypes = typename TPlan::template EligibleListenerTypes<TEvent>;

                if constexpr (TIndex < ListenerTypes::Count) {
                    using ThreadIdentity = typename Detail::TypeAt<
                        ListenerTypes,
                        TIndex
                    >::Type;

                    const auto listener = Record<TEvent>::ListenerIndex::FromUnchecked(
                        TIndex
                    );

                    if (recipients.IsSet(listener)) {
                        auto thread = _threading->template ThreadHandle<ThreadIdentity>();
                        static_cast<void>(
                            thread.Wake()
                        );
                    }

                    WakeRecipients<TEvent, TIndex + 1U>(recipients);
                }
            }

            // Local admission core.

            /// Performs local admission while the Event Runtime mutex is already held.
            /// @tparam TEventArgument Concrete Event lvalue/rvalue argument preserving the deployed Event Type.
            /// @param event Event payload source; moved only after all applicable admission viability checks.
            /// @param retention Already-normalized common retention decision.
            template<class TEventArgument>
            DispatchResult DispatchLocalLocked(
                TEventArgument&& event,
                Detail::NormalizedRetention retention
            ) noexcept {
                using TEvent = std::remove_cvref_t<TEventArgument>;

                static_assert(
                    TPlan::template IsDeployed<TEvent>,
                    "Local Event Dispatch requires a local deployment"
                );

                static_assert(
                    std::is_nothrow_constructible_v<TEvent, TEventArgument&&>,
                    "Local Event Dispatch requires nothrow occurrence construction"
                );

                if (retention.Expired) {
                    return DispatchResult::Expired;
                }

                const auto deadline = retention.Deadline;
                const auto now = TPlan::template SupportsTimedRetention<TEvent>
                    ? Clock::MonotonicNow()
                    : MonotonicTimestamp{};

                if (
                    deadline.Nanoseconds() != 0U &&
                    now >= deadline
                ) {
                    return DispatchResult::Expired;
                }

                SweepExpired<TEvent>(now);
                auto recipients = SnapshotSubscriptions<TEvent>();

                if (!recipients.IsAnySet()) {
                    return DispatchResult::Accepted;
                }

                auto& pool = PoolFor<TEvent>();
                auto& state = StateFor<TEvent>();

                if constexpr (TPlan::template IsQueue<TEvent>) {
                    auto& admission = state.Admission;
                    const bool needsShared =
                        admission.DedicatedUsed.Count() >=
                        TPlan::template DedicatedPendingCapacity<TEvent>;

                    if (
                        needsShared &&
                        _sharedPending.Count() >= TPlan::SharedPendingCapacity
                    ) {
                        return DispatchResult::NoCapacity;
                    }

                    typename std::remove_reference_t<decltype(pool)>::DedicatedIndex memoryIndex;
                    const auto acquisitionResult = pool.AcquireDedicated(
                        memoryIndex,
                        std::forward<TEventArgument>(event),
                        recipients,
                        deadline
                    );

                    if (acquisitionResult == Memory::DedicatedObjectPoolAcquisitionResult::CapacityUnavailable) {
                        return DispatchResult::NoCapacity;
                    }

                    if (acquisitionResult != Memory::DedicatedObjectPoolAcquisitionResult::Succeeded) {
                        InfrastructureFailure();
                    }

                    const auto eventIndex = ToEventIndex<TEvent>(memoryIndex);
                    auto& record = RecordAt<TEvent>(eventIndex);
                    record.SetQueueEntitlement(needsShared);
                    auto records = Records<TEvent>();
                    const auto queueResult = admission.Pending.Push(records, eventIndex);

                    if (queueResult != BoundedTopology::IntrusiveQueuePushResult::Succeeded) {
                        InfrastructureFailure();
                    }

                    if (needsShared) {
                        _sharedPending.Increment();
                    } else {
                        admission.DedicatedUsed.Increment();
                    }

                    WakeRecipients<TEvent>(recipients);
                    return DispatchResult::Accepted;
                } else {
                    auto& pendingIndex = state.Admission.Pending;

                    if (pendingIndex.IsValid()) {
                        auto& old = RecordAt<TEvent>(pendingIndex);

                        if (old.ActiveBorrowCount() == 0U) {
                            old.ReplaceUnborrowed(
                                std::forward<TEventArgument>(event),
                                recipients,
                                deadline
                            );
                            WakeRecipients<TEvent>(recipients);
                            return DispatchResult::Accepted;
                        }
                    }

                    typename std::remove_reference_t<decltype(pool)>::DedicatedIndex memoryIndex;
                    const auto acquisitionResult = pool.AcquireDedicated(
                        memoryIndex,
                        std::forward<TEventArgument>(event),
                        recipients,
                        deadline
                    );

                    if (acquisitionResult == Memory::DedicatedObjectPoolAcquisitionResult::CapacityUnavailable) {
                        return DispatchResult::NoCapacity;
                    }

                    if (acquisitionResult != Memory::DedicatedObjectPoolAcquisitionResult::Succeeded) {
                        InfrastructureFailure();
                    }

                    const auto newIndex = ToEventIndex<TEvent>(memoryIndex);

                    if (pendingIndex.IsValid()) {
                        auto& old = RecordAt<TEvent>(pendingIndex);
                        old.PendingRecipients().ClearAll();
                        const auto oldIndex = pendingIndex;

                        if (old.ActiveBorrowCount() == 0U) {
                            ReleaseOccurrence<TEvent>(oldIndex);
                        }
                    }

                    pendingIndex = newIndex;
                    WakeRecipients<TEvent>(recipients);
                    return DispatchResult::Accepted;
                }
            }

            // Transactional ingress reservation state.

            template<class TEvent>
            [[nodiscard]] Detail::IngressSlotState& IngressSlotAt(
                std::size_t index
            ) noexcept {
                return StateFor<TEvent>().Ingress.Slots[index];
            }

            template<class TEvent>
            [[nodiscard]] bool MatchesIngressLocked(
                std::size_t index,
                std::uint32_t generation
            ) noexcept {
                return index < Record<TEvent>::MaximumInstances &&
                    IngressSlotAt<TEvent>(index).Reserved &&
                    IngressSlotAt<TEvent>(index).Generation == generation;
            }

            template<class TEvent>
            void ReleaseIngressEntitlementLocked(
                Detail::IngressSlotState& slot
            ) noexcept {
                if constexpr (TPlan::template IsQueue<TEvent>) {
                    if (slot.SharedPending) {
                        _sharedPending.Decrement();
                    } else {
                        StateFor<TEvent>().Admission.DedicatedUsed.Decrement();
                    }
                }
            }

            template<class TEvent>
            void AbortIngressLocked(
                std::size_t index
            ) noexcept {
                auto& slot = IngressSlotAt<TEvent>(index);
                ReleaseIngressEntitlementLocked<TEvent>(slot);
                slot.Reserved = false;
                slot.Cancelled = false;
                slot.SharedPending = false;
                ReleaseOccurrence<TEvent>(
                    Record<TEvent>::OccurrenceIndex::FromUnchecked(index)
                );
            }

            // Ordered outbound handoff sequencer.

            template<class TEvent>
            [[nodiscard]] bool MatchesRemoteHandoffLocked(
                std::size_t index,
                std::uint32_t generation
            ) noexcept {
                if constexpr (TPlan::template RemoteHandoffCapacity<TEvent> == 0U) {
                    static_cast<void>(index);
                    static_cast<void>(generation);
                    return false;
                } else {
                    auto& sequence = StateFor<TEvent>().Outbound;
                    return index < TPlan::template RemoteHandoffCapacity<TEvent> &&
                        sequence.Slots[index].Occupied &&
                        sequence.Slots[index].Generation == generation;
                }
            }

            template<class TEvent>
            void AdvanceRemoteHandoffHeadLocked() noexcept {
                if constexpr (TPlan::template RemoteHandoffCapacity<TEvent> != 0U) {
                    constexpr auto capacity = TPlan::template RemoteHandoffCapacity<TEvent>;
                    auto& sequence = StateFor<TEvent>().Outbound;

                    while (sequence.Count != 0U) {
                        const auto head = static_cast<std::size_t>(sequence.Head);
                        auto& slot = sequence.Slots[head];

                        if (!slot.Occupied || !slot.Terminal || slot.InFlight) {
                            break;
                        }

                        slot.EventView = nullptr;
                        slot.Deadline = MonotonicTimestamp{};
                        slot.Occupied = false;
                        slot.Terminal = false;
                        sequence.Head = static_cast<Detail::CountStorage<capacity>>(
                            (head + 1U) % capacity
                        );
                        --sequence.Count;
                    }
                }
            }

            template<class TEvent>
            void SweepRemoteHandoffExpiryLocked(
                MonotonicTimestamp now
            ) noexcept {
                if constexpr (
                    TPlan::template RemoteHandoffCapacity<TEvent> != 0U &&
                    TPlan::template SupportsTimedRetention<TEvent>
                ) {
                    constexpr auto capacity = TPlan::template RemoteHandoffCapacity<TEvent>;
                    auto& sequence = StateFor<TEvent>().Outbound;
                    const auto count = static_cast<std::size_t>(sequence.Count);
                    const auto head = static_cast<std::size_t>(sequence.Head);

                    for (std::size_t offset = 0U; offset < count; ++offset) {
                        auto& slot = sequence.Slots[(head + offset) % capacity];
                        if (
                            slot.Occupied &&
                            !slot.InFlight &&
                            slot.Deadline.Nanoseconds() != 0U &&
                            now >= slot.Deadline
                        ) {
                            slot.Terminal = true;
                        }
                    }
                    AdvanceRemoteHandoffHeadLocked<TEvent>();
                } else {
                    static_cast<void>(now);
                }
            }

            template<class TEvent>
            [[nodiscard]] std::size_t ReserveRemoteHandoffLocked(
                const TEvent& event,
                MonotonicTimestamp deadline
            ) noexcept {
                constexpr auto capacity = TPlan::template RemoteHandoffCapacity<TEvent>;

                if constexpr (capacity == 0U) {
                    static_cast<void>(event);
                    static_cast<void>(deadline);
                    return 0U;
                } else {
                    auto& sequence = StateFor<TEvent>().Outbound;
                    if (static_cast<std::size_t>(sequence.Count) >= capacity) {
                        return capacity;
                    }

                    const auto index = (
                        static_cast<std::size_t>(sequence.Head) +
                        static_cast<std::size_t>(sequence.Count)
                    ) % capacity;
                    auto& slot = sequence.Slots[index];

                    if (
                        slot.Occupied ||
                        slot.Generation == std::numeric_limits<std::uint32_t>::max()
                    ) {
                        return capacity;
                    }

                    ++slot.Generation;
                    slot.EventView = &event;
                    slot.Deadline = deadline;
                    slot.Occupied = true;
                    slot.Terminal = false;
                    slot.InFlight = false;
                    ++sequence.Count;
                    return index;
                }
            }

            // Integration quiesce.

            template<std::size_t TIndex = 0U>
            void CancelIntegrationReservationsLocked() noexcept {
                if constexpr (TIndex < PrimitiveTypes::Count) {
                    using TEvent = typename Detail::TypeAt<
                        PrimitiveTypes,
                        TIndex
                    >::Type;

                    for (
                        std::size_t index = 0U;
                        index < Record<TEvent>::MaximumInstances;
                        ++index
                    ) {
                        auto& ingress = IngressSlotAt<TEvent>(index);
                        if (ingress.Reserved) {
                            // The decoder may be populating its exclusive destination
                            // outside this mutex. Defer destruction to its Commit/Abort.
                            ingress.Cancelled = true;
                        }
                    }

                    if constexpr (TPlan::template RemoteHandoffCapacity<TEvent> != 0U) {
                        auto& sequence = StateFor<TEvent>().Outbound;
                        constexpr auto capacity =
                            TPlan::template RemoteHandoffCapacity<TEvent>;
                        const auto count = static_cast<std::size_t>(sequence.Count);
                        const auto head = static_cast<std::size_t>(sequence.Head);

                        for (std::size_t offset = 0U; offset < count; ++offset) {
                            auto& slot = sequence.Slots[(head + offset) % capacity];
                            if (slot.Occupied && !slot.InFlight) {
                                slot.Terminal = true;
                            }
                        }
                        AdvanceRemoteHandoffHeadLocked<TEvent>();
                    }

                    CancelIntegrationReservationsLocked<TIndex + 1U>();
                }
            }

            template<std::size_t TIndex = 0U>
            [[nodiscard]] bool IntegrationQuiescentLocked() noexcept {
                if constexpr (TIndex >= PrimitiveTypes::Count) {
                    return true;
                } else {
                    using TEvent = typename Detail::TypeAt<
                        PrimitiveTypes,
                        TIndex
                    >::Type;
                    bool quiet = true;

                    for (
                        std::size_t index = 0U;
                        index < Record<TEvent>::MaximumInstances;
                        ++index
                    ) {
                        quiet = quiet && !IngressSlotAt<TEvent>(index).Reserved;
                    }

                    if constexpr (TPlan::template RemoteHandoffCapacity<TEvent> != 0U) {
                        quiet = quiet &&
                            StateFor<TEvent>().Outbound.Count == 0U;
                    }

                    return quiet &&
                        IntegrationQuiescentLocked<TIndex + 1U>();
                }
            }

            // Listener claim/callback delivery.

            /// Attempts one callback delivery for one Listener/Event pair without blocking.
            /// @tparam TThreadIdentity Listener Dedicated Thread identity.
            /// @tparam TEvent Observed Event payload Type.
            /// @return Strongly typed delivery outcome for the selected Listener/Event pair.
            template<class TThreadIdentity, class TEvent>
            Detail::DeliveryAttemptResult TryDeliverOne() noexcept {
                using EventRecord = Record<TEvent>;
                using Index = typename EventRecord::OccurrenceIndex;

                const auto listener = ListenerIndexFor<TThreadIdentity, TEvent>();
                Index selected = Index::Invalid();

                Lock();
                const auto now = TPlan::template SupportsTimedRetention<TEvent>
                    ? Clock::MonotonicNow()
                    : MonotonicTimestamp{};
                SweepExpired<TEvent>(now);

                auto& state = StateFor<TEvent>();

                if constexpr (TPlan::template IsQueue<TEvent>) {
                    auto current = state.Admission.Pending.Head();

                    for (
                        std::size_t visited = 0U;
                        visited < EventRecord::MaximumInstances && current.IsValid();
                        ++visited
                    ) {
                        auto& record = RecordAt<TEvent>(current);

                        if (record.PendingRecipients().IsSet(listener)) {
                            selected = current;
                            break;
                        }

                        current = record.QueueNext();
                    }
                } else {
                    if (
                        state.Admission.Pending.IsValid() &&
                        RecordAt<TEvent>(state.Admission.Pending).PendingRecipients().IsSet(listener)
                    ) {
                        selected = state.Admission.Pending;
                    }
                }

                if (!selected.IsValid()) {
                    Unlock();
                    return Detail::DeliveryAttemptResult::NoPendingOccurrence;
                }

                auto& record = RecordAt<TEvent>(selected);

                if (record.Claim(listener) != Detail::OccurrenceClaimResult::Claimed) {
                    InfrastructureFailure();
                }

                if (!record.HasPendingRecipients()) {
                    if constexpr (TPlan::template IsQueue<TEvent>) {
                        RemovePendingQueueOccurrence<TEvent>(selected);
                    } else {
                        state.Admission.Pending = Index::Invalid();
                    }
                }

                const TEvent* borrowed = &record.Value();
                Unlock();

                using CallbackProvider = Composition::ListenerCallbackProvider<
                    TThreadIdentity,
                    TEvent,
                    TArchitecture
                >;

                auto* callback = std::get<CallbackProvider*>(_callbacks);

                static_assert(
                    requires(decltype(*callback)& provider, const TEvent& value) {
                        {
                            provider.OnEvent(value)
                        } noexcept -> std::same_as<void>;
                    },
                    "Event Listener callback provider must expose void OnEvent(const TEvent&) noexcept"
                );

                callback->OnEvent(*borrowed);

                Lock();
                auto& after = RecordAt<TEvent>(selected);
                after.ReleaseBorrow();

                if (
                    !after.HasPendingRecipients() &&
                    after.ActiveBorrowCount() == 0U
                ) {
                    ReleaseOccurrence<TEvent>(selected);
                }

                Unlock();
                return Detail::DeliveryAttemptResult::Delivered;
            }

            /// Attempts delivery for the observed Event Type at one runtime-selected ordinal.
            /// @tparam TThreadIdentity Listener Dedicated Thread identity.
            /// @tparam TIndex Current compile-time observed-Type ordinal in the recursive dispatcher.
            /// @param ordinal Runtime ordinal selected by the Listener round-robin scan.
            template<class TThreadIdentity, std::size_t TIndex = 0U>
            Detail::DeliveryAttemptResult TryDeliverOrdinal(
                std::size_t ordinal
            ) noexcept {
                using Types = typename TPlan::template ObservedEventTypes<TThreadIdentity>;

                if constexpr (TIndex >= Types::Count) {
                    return Detail::DeliveryAttemptResult::NoPendingOccurrence;
                } else {
                    if (ordinal == TIndex) {
                        using TEvent = typename Detail::TypeAt<Types, TIndex>::Type;
                        return TryDeliverOne<TThreadIdentity, TEvent>();
                    }

                    return TryDeliverOrdinal<TThreadIdentity, TIndex + 1U>(ordinal);
                }
            }

            /// Updates the cross-Type continuation cursor when one is required by topology.
            /// @tparam TThreadIdentity Listener Dedicated Thread identity.
            /// @param value Next observed-Type ordinal at which scanning should begin.
            template<class TThreadIdentity>
            void SetCursor(
                std::size_t value
            ) noexcept {
                constexpr auto count = TPlan::template ObservedEventTypes<TThreadIdentity>::Count;

                if constexpr (count > 1U) {
                    std::get<Detail::ListenerCursor<TPlan, TThreadIdentity>>(_listenerCursors).Value =
                        static_cast<typename Detail::ListenerCursor<TPlan, TThreadIdentity>::Storage>(
                            value % count
                        );
                } else {
                    static_cast<void>(value);
                }
            }

            /// Indicates whether one Listener still has pending interest in one Event Type.
            /// @tparam TThreadIdentity Listener Dedicated Thread identity.
            /// @tparam TEvent Observed Event payload Type.
            template<class TThreadIdentity, class TEvent>
            bool HasPendingForType() noexcept {
                const auto listener = ListenerIndexFor<TThreadIdentity, TEvent>();
                auto& state = StateFor<TEvent>();

                if constexpr (TPlan::template IsQueue<TEvent>) {
                    auto current = state.Admission.Pending.Head();

                    for (
                        std::size_t visited = 0U;
                        visited < Record<TEvent>::MaximumInstances && current.IsValid();
                        ++visited
                    ) {
                        auto& record = RecordAt<TEvent>(current);

                        if (record.PendingRecipients().IsSet(listener)) {
                            return true;
                        }

                        current = record.QueueNext();
                    }

                    return false;
                } else {
                    return state.Admission.Pending.IsValid() &&
                        RecordAt<TEvent>(state.Admission.Pending).PendingRecipients().IsSet(listener);
                }
            }

            /// Indicates whether one Listener has pending interest in any observed Event Type.
            /// @tparam TThreadIdentity Listener Dedicated Thread identity.
            /// @tparam TIndex Current compile-time observed-Type ordinal in recursive scanning.
            template<class TThreadIdentity, std::size_t TIndex = 0U>
            bool HasAnyPending() noexcept {
                using Types = typename TPlan::template ObservedEventTypes<TThreadIdentity>;

                if constexpr (TIndex >= Types::Count) {
                    return false;
                } else {
                    using TEvent = typename Detail::TypeAt<Types, TIndex>::Type;

                    return HasPendingForType<TThreadIdentity, TEvent>() ||
                        HasAnyPending<TThreadIdentity, TIndex + 1U>();
                }
            }

            // Compile-time Memory topology validation.

            /// Proves one local Event Type has the exact dedicated-only occurrence pool required by its plan.
            /// @tparam TEvent Locally deployed Event payload Type.
            template<class TEvent>
            static consteval bool MemoryPoolValid() {
                using EventRecord = Record<TEvent>;

                if constexpr (!TMemoryRuntime::Topology::template ContainsObjectPool<EventRecord>) {
                    return false;
                } else {
                    using PoolSpec = typename TMemoryRuntime::Topology::template ObjectPoolSpecFor<EventRecord>;

                    return PoolSpec::Dedicated::Value == TPlan::template Deployment<TEvent>::MaximumInstances &&
                        !PoolSpec::Shared::IsEnabled;
                }
            }

            /// Proves exact occurrence-pool validity recursively across all locally deployed Event Types.
            /// @tparam TIndex Current compile-time deployed-Type ordinal.
            template<std::size_t TIndex = 0U>
            static consteval bool AllMemoryPoolsValid() {
                if constexpr (TIndex >= PrimitiveTypes::Count) {
                    return true;
                } else {
                    using TEvent = typename Detail::TypeAt<
                        PrimitiveTypes,
                        TIndex
                    >::Type;

                    return MemoryPoolValid<TEvent>() &&
                        AllMemoryPoolsValid<TIndex + 1U>();
                }
            }

        public:

            static_assert(
                AllMemoryPoolsValid(),
                "Event Runtime requires an exact dedicated-only EDP-Memory occurrence pool for every local Event Type"
            );

            // Construction and stable provider binding.

            /// Binds already-owned Memory, Threading, mutex and callback providers to one Event Runtime.
            /// @param memory Application Memory Runtime containing exact Event occurrence pools.
            /// @param threading Application Threading Runtime containing every Listener Dedicated Thread.
            /// @param mutex Event Runtime ordinary-context mutex provider.
            /// @param callbacks Unique typed callback provider objects selected by Architecture.
            Runtime(
                TMemoryRuntime& memory,
                TThreadingRuntime& threading,
                TMutexProvider& mutex,
                TCallbackProviders&... callbacks
            ) noexcept :
                _memory(&memory),
                _threading(&threading),
                _mutex(&mutex),
                _callbacks(&callbacks...) {
            }

            /// Runtime owns stable topology state/provider bindings and therefore cannot be copied.
            Runtime(const Runtime&) = delete;

            /// Runtime owns stable topology state/provider bindings and therefore cannot be copy-assigned.
            Runtime& operator=(const Runtime&) = delete;

            /// Runtime owns stable topology state/provider bindings and therefore cannot be moved.
            Runtime(Runtime&&) = delete;

            /// Runtime owns stable topology state/provider bindings and therefore cannot be move-assigned.
            Runtime& operator=(Runtime&&) = delete;

            // Runtime initialization.

            /// Initializes Event after required owning-domain resources have been established.
            [[nodiscard]] InitializationResult Initialize() noexcept {
                if (_initialized) {
                    return InitializationResult::AlreadyInitialized;
                }

                if (!_memory->IsInitialized()) {
                    return InitializationResult::ProviderFailure;
                }

                if constexpr (Detail::AnyTimedDeployment<PrimitiveTypes>::template For<TPlan>()) {
                    if (!Clock::IsMonotonicClockBound()) {
                        return InitializationResult::ProviderFailure;
                    }
                }

                _integrationOpen = true;
                _initialized = true;
                return InitializationResult::Initialized;
            }

            /// Indicates whether Event Runtime initialization has completed successfully.
            [[nodiscard]] bool IsInitialized() const noexcept {
                return _initialized;
            }

            /// Stops new integration reservations and cancels every non-in-flight capability.
            /// Ingress storage remains owned until its decoder subsequently commits or aborts.
            void BeginIntegrationQuiesce() noexcept {
                Lock();
                _integrationOpen = false;
                CancelIntegrationReservationsLocked();
                Unlock();
            }

            /// Reports whether all unpublished ingress and ordered outbound slots are released.
            [[nodiscard]] bool IsIntegrationQuiescent() noexcept {
                Lock();
                const bool result = IntegrationQuiescentLocked();
                Unlock();
                return result;
            }

            // Runtime subscription control.

            /// Activates one statically planned Listener/Event relationship.
            /// @tparam TThreadIdentity Eligible Listener Dedicated Thread identity.
            /// @tparam TEvent Locally deployed Event payload Type.
            template<class TThreadIdentity, class TEvent>
            requires EventType<TEvent> && TPlan::template IsDeployed<TEvent>
            [[nodiscard]] SubscribeResult Subscribe() noexcept {
                constexpr auto ordinal = TPlan::template ListenerOrdinal<
                    TEvent,
                    TThreadIdentity
                >;

                static_assert(
                    ordinal < TPlan::template EligibleListenerCount<TEvent>,
                    "Subscribe requires a statically declared Observe relation"
                );

                const auto listener = ListenerIndexFor<TThreadIdentity, TEvent>();
                Lock();
                auto& subscriptions = StateFor<TEvent>().Subscriptions;

                if (subscriptions.IsSet(listener)) {
                    Unlock();
                    return SubscribeResult::AlreadySubscribed;
                }

                static_cast<void>(
                    subscriptions.Set(listener)
                );
                Unlock();
                return SubscribeResult::Subscribed;
            }

            /// Deactivates one statically planned Listener/Event relationship and cancels only still-pending interest.
            /// @tparam TThreadIdentity Eligible Listener Dedicated Thread identity.
            /// @tparam TEvent Locally deployed Event payload Type.
            template<class TThreadIdentity, class TEvent>
            requires EventType<TEvent> && TPlan::template IsDeployed<TEvent>
            [[nodiscard]] UnsubscribeResult Unsubscribe() noexcept {
                constexpr auto ordinal = TPlan::template ListenerOrdinal<
                    TEvent,
                    TThreadIdentity
                >;

                static_assert(
                    ordinal < TPlan::template EligibleListenerCount<TEvent>,
                    "Unsubscribe requires a statically declared Observe relation"
                );

                const auto listener = ListenerIndexFor<TThreadIdentity, TEvent>();
                Lock();
                auto& state = StateFor<TEvent>();

                if (!state.Subscriptions.IsSet(listener)) {
                    Unlock();
                    return UnsubscribeResult::NotSubscribed;
                }

                static_cast<void>(
                    state.Subscriptions.Clear(listener)
                );

                if constexpr (TPlan::template IsQueue<TEvent>) {
                    auto current = state.Admission.Pending.Head();
                    std::size_t visited = 0U;

                    while (
                        current.IsValid() &&
                        visited++ < Record<TEvent>::MaximumInstances
                    ) {
                        auto& record = RecordAt<TEvent>(current);
                        const auto next = record.QueueNext();

                        if (record.PendingRecipients().IsSet(listener)) {
                            static_cast<void>(
                                record.PendingRecipients().Clear(listener)
                            );

                            if (!record.HasPendingRecipients()) {
                                RemovePendingQueueOccurrence<TEvent>(current);

                                if (record.ActiveBorrowCount() == 0U) {
                                    ReleaseOccurrence<TEvent>(current);
                                }
                            }
                        }

                        current = next;
                    }
                } else {
                    auto& pending = state.Admission.Pending;

                    if (pending.IsValid()) {
                        auto& record = RecordAt<TEvent>(pending);

                        if (record.PendingRecipients().IsSet(listener)) {
                            static_cast<void>(
                                record.PendingRecipients().Clear(listener)
                            );

                            if (!record.HasPendingRecipients()) {
                                const auto old = pending;
                                pending = Record<TEvent>::OccurrenceIndex::Invalid();

                                if (record.ActiveBorrowCount() == 0U) {
                                    ReleaseOccurrence<TEvent>(old);
                                }
                            }
                        }
                    }
                }

                Unlock();
                return UnsubscribeResult::Unsubscribed;
            }

            // Local Event admission.

            /// Performs explicit LocalOnly Event admission.
            /// @tparam TEventArgument Concrete locally deployed Event argument Type.
            /// @tparam TRetention Supported retention request Type permitted by the deployment.
            /// @param event Event payload source.
            /// @param retention Per-Dispatch retention request.
            template<class TEventArgument, class TRetention = UntilHandoff>
            requires EventType<std::remove_cvref_t<TEventArgument>> &&
                RetentionRequest<TRetention> &&
                TPlan::template IsDeployed<std::remove_cvref_t<TEventArgument>> &&
                (
                    TPlan::template SupportsTimedRetention<std::remove_cvref_t<TEventArgument>> ||
                    std::is_same_v<std::remove_cvref_t<TRetention>, UntilHandoff>
                )
            [[nodiscard]] DispatchResult Dispatch(
                LocalOnly,
                TEventArgument&& event,
                TRetention retention = {}
            ) noexcept {
                if (!_initialized) {
                    InfrastructureFailure();
                }

                static_assert(
                    TPlan::template SupportsTimedRetention<std::remove_cvref_t<TEventArgument>> ||
                    std::is_same_v<std::remove_cvref_t<TRetention>, UntilHandoff>,
                    "Timed retention is unavailable for an UntilHandoffOnly Event deployment"
                );

                Lock();
                const auto normalized = Detail::NormalizeRetention(retention);
                const auto result = DispatchLocalLocked(
                    std::forward<TEventArgument>(event),
                    normalized
                );
                Unlock();
                return result;
            }

            // Transactional unpublished remote ingress.

            template<class TEvent, class TRetention = UntilHandoff>
            requires EventType<TEvent> &&
                TPlan::template IsDeployed<TEvent> &&
                RetentionRequest<TRetention> &&
                std::is_nothrow_default_constructible_v<TEvent> &&
                (
                    TPlan::template SupportsTimedRetention<TEvent> ||
                    std::is_same_v<std::remove_cvref_t<TRetention>, UntilHandoff>
                )
            [[nodiscard]] IngressReservationResult<TEvent, Runtime> PrepareIngress(
                TRetention retention = {}
            ) noexcept {
                if (!_initialized) {
                    return IngressReservationResult<TEvent, Runtime>(
                        ReservationFailure::RuntimeUnavailable
                    );
                }

                const auto normalized = Detail::NormalizeRetention(retention);
                if (normalized.Expired) {
                    return IngressReservationResult<TEvent, Runtime>(
                        ReservationFailure::Expired
                    );
                }

                Lock();
                if (!_integrationOpen) {
                    Unlock();
                    return IngressReservationResult<TEvent, Runtime>(
                        ReservationFailure::RuntimeUnavailable
                    );
                }

                const auto now = TPlan::template SupportsTimedRetention<TEvent>
                    ? Clock::MonotonicNow()
                    : MonotonicTimestamp{};

                if (
                    normalized.Deadline.Nanoseconds() != 0U &&
                    now >= normalized.Deadline
                ) {
                    Unlock();
                    return IngressReservationResult<TEvent, Runtime>(
                        ReservationFailure::Expired
                    );
                }

                SweepExpired<TEvent>(now);
                bool shared = false;

                if constexpr (TPlan::template IsQueue<TEvent>) {
                    auto& admission = StateFor<TEvent>().Admission;
                    shared =
                        admission.DedicatedUsed.Count() >=
                        TPlan::template DedicatedPendingCapacity<TEvent>;

                    if (
                        shared &&
                        _sharedPending.Count() >= TPlan::SharedPendingCapacity
                    ) {
                        Unlock();
                        return IngressReservationResult<TEvent, Runtime>(
                            ReservationFailure::NoCapacity
                        );
                    }
                }

                auto& pool = PoolFor<TEvent>();
                typename std::remove_reference_t<decltype(pool)>::DedicatedIndex memoryIndex;
                const auto acquired = pool.AcquireDedicated(
                    memoryIndex,
                    Detail::UnpublishedOccurrenceTag{},
                    normalized.Deadline
                );

                if (acquired == Memory::DedicatedObjectPoolAcquisitionResult::CapacityUnavailable) {
                    Unlock();
                    return IngressReservationResult<TEvent, Runtime>(
                        ReservationFailure::NoCapacity
                    );
                }

                if (acquired != Memory::DedicatedObjectPoolAcquisitionResult::Succeeded) {
                    InfrastructureFailure();
                }

                const auto index = static_cast<std::size_t>(memoryIndex.Value());
                auto& slot = IngressSlotAt<TEvent>(index);

                if (
                    slot.Reserved ||
                    slot.Generation == std::numeric_limits<std::uint32_t>::max()
                ) {
                    const auto released = pool.ReleaseDedicated(memoryIndex);
                    if (released != Memory::DedicatedObjectPoolReleaseResult::Released) {
                        InfrastructureFailure();
                    }
                    Unlock();
                    return IngressReservationResult<TEvent, Runtime>(
                        ReservationFailure::NoCapacity
                    );
                }

                ++slot.Generation;
                slot.Reserved = true;
                slot.Cancelled = false;
                slot.SharedPending = shared;

                if constexpr (TPlan::template IsQueue<TEvent>) {
                    if (shared) {
                        _sharedPending.Increment();
                    } else {
                        StateFor<TEvent>().Admission.DedicatedUsed.Increment();
                    }
                }

                const auto generation = slot.Generation;
                Unlock();
                return IngressReservationResult<TEvent, Runtime>(
                    *this,
                    index,
                    generation
                );
            }

            template<class TEvent>
            [[nodiscard]] TEvent& IngressValue(
                std::size_t index,
                std::uint32_t generation
            ) noexcept {
                Lock();
                if (!MatchesIngressLocked<TEvent>(index, generation)) {
                    InfrastructureFailure();
                }
                auto* value = &RecordAt<TEvent>(
                    Record<TEvent>::OccurrenceIndex::FromUnchecked(index)
                ).Value();
                Unlock();
                return *value;
            }

            template<class TEvent>
            [[nodiscard]] DispatchResult CommitIngress(
                std::size_t index,
                std::uint32_t generation
            ) noexcept {
                Lock();
                if (!MatchesIngressLocked<TEvent>(index, generation)) {
                    Unlock();
                    return DispatchResult::RuntimeUnavailable;
                }

                if (!_integrationOpen || IngressSlotAt<TEvent>(index).Cancelled) {
                    AbortIngressLocked<TEvent>(index);
                    Unlock();
                    return DispatchResult::RuntimeUnavailable;
                }

                const auto eventIndex =
                    Record<TEvent>::OccurrenceIndex::FromUnchecked(index);
                auto& slot = IngressSlotAt<TEvent>(index);
                auto& record = RecordAt<TEvent>(eventIndex);
                const auto now = TPlan::template SupportsTimedRetention<TEvent>
                    ? Clock::MonotonicNow()
                    : MonotonicTimestamp{};

                if (record.IsExpired(now)) {
                    AbortIngressLocked<TEvent>(index);
                    Unlock();
                    return DispatchResult::Expired;
                }

                SweepExpired<TEvent>(now);
                auto recipients = SnapshotSubscriptions<TEvent>();

                if (!recipients.IsAnySet()) {
                    AbortIngressLocked<TEvent>(index);
                    Unlock();
                    return DispatchResult::Accepted;
                }

                record.PublishUnpublished(
                    recipients,
                    record.ExpiresAt(),
                    slot.SharedPending
                );
                auto& state = StateFor<TEvent>();

                if constexpr (TPlan::template IsQueue<TEvent>) {
                    auto records = Records<TEvent>();
                    const auto pushed = state.Admission.Pending.Push(
                        records,
                        eventIndex
                    );
                    if (pushed != BoundedTopology::IntrusiveQueuePushResult::Succeeded) {
                        InfrastructureFailure();
                    }
                } else {
                    auto& pending = state.Admission.Pending;
                    if (pending.IsValid()) {
                        auto& old = RecordAt<TEvent>(pending);
                        old.PendingRecipients().ClearAll();
                        const auto oldIndex = pending;
                        if (old.ActiveBorrowCount() == 0U) {
                            ReleaseOccurrence<TEvent>(oldIndex);
                        }
                    }
                    pending = eventIndex;
                }

                slot.Reserved = false;
                slot.Cancelled = false;
                slot.SharedPending = false;
                Unlock();
                WakeRecipients<TEvent>(recipients);
                return DispatchResult::Accepted;
            }

            template<class TEvent>
            void AbortIngress(
                std::size_t index,
                std::uint32_t generation
            ) noexcept {
                Lock();
                if (MatchesIngressLocked<TEvent>(index, generation)) {
                    AbortIngressLocked<TEvent>(index);
                }
                Unlock();
            }

            // Combined local/remote scoped dispatch.

            /// Reserves one bounded ordered outbound handoff opportunity.
            template<class TEvent, class TRetention = UntilHandoff>
            requires EventType<TEvent> &&
                TPlan::template IsDeployed<TEvent> &&
                RetentionRequest<TRetention> &&
                (
                    TPlan::template SupportsTimedRetention<TEvent> ||
                    std::is_same_v<std::remove_cvref_t<TRetention>, UntilHandoff>
                )
            [[nodiscard]] RemoteHandoffReservationResult<TEvent, Runtime>
            PrepareRemoteHandoff(
                const TEvent& event,
                TRetention retention = {}
            ) noexcept {
                if (!_initialized) {
                    return RemoteHandoffReservationResult<TEvent, Runtime>(
                        ReservationFailure::RuntimeUnavailable
                    );
                }

                const auto normalized = Detail::NormalizeRetention(retention);
                if (normalized.Expired) {
                    return RemoteHandoffReservationResult<TEvent, Runtime>(
                        ReservationFailure::Expired
                    );
                }

                Lock();
                if (!_integrationOpen) {
                    Unlock();
                    return RemoteHandoffReservationResult<TEvent, Runtime>(
                        ReservationFailure::RuntimeUnavailable
                    );
                }

                const auto now = TPlan::template SupportsTimedRetention<TEvent>
                    ? Clock::MonotonicNow()
                    : MonotonicTimestamp{};

                if (
                    normalized.Deadline.Nanoseconds() != 0U &&
                    now >= normalized.Deadline
                ) {
                    Unlock();
                    return RemoteHandoffReservationResult<TEvent, Runtime>(
                        ReservationFailure::Expired
                    );
                }

                SweepRemoteHandoffExpiryLocked<TEvent>(now);
                constexpr auto capacity =
                    TPlan::template RemoteHandoffCapacity<TEvent>;

                if constexpr (capacity == 0U) {
                    Unlock();
                    return RemoteHandoffReservationResult<TEvent, Runtime>(
                        ReservationFailure::NoCapacity
                    );
                } else {
                    const auto index = ReserveRemoteHandoffLocked(
                        event,
                        normalized.Deadline
                    );
                    if (index == capacity) {
                        Unlock();
                        return RemoteHandoffReservationResult<TEvent, Runtime>(
                            ReservationFailure::NoCapacity
                        );
                    }

                    const auto generation =
                        StateFor<TEvent>().Outbound.Slots[index].Generation;
                    Unlock();
                    return RemoteHandoffReservationResult<TEvent, Runtime>(
                        *this,
                        index,
                        generation
                    );
                }
            }

            template<class TEvent, class TRetention = UntilHandoff>
            requires (!std::is_lvalue_reference_v<TEvent>)
            auto PrepareRemoteHandoff(
                TEvent&&,
                TRetention = {}
            ) noexcept = delete;

            /// Attempts the sequencer head and invokes the adapter outside Event synchronization.
            template<class TEvent, class TRemoteOperation>
            requires (TPlan::template RemoteHandoffCapacity<TEvent> > 0U)
            [[nodiscard]] auto TryCommitRemoteHandoff(
                std::size_t index,
                std::uint32_t generation,
                TRemoteOperation& remoteOperation
            ) noexcept {
                using RemoteResult = decltype(
                    remoteOperation(std::declval<const TEvent&>())
                );
                static_assert(
                    noexcept(remoteOperation(std::declval<const TEvent&>())),
                    "Event remote handoff must be non-throwing"
                );
                static_assert(
                    !std::is_void_v<RemoteResult> &&
                    std::is_nothrow_move_constructible_v<RemoteResult> &&
                    std::is_nothrow_destructible_v<RemoteResult>,
                    "Event remote handoff requires a non-throwing movable result"
                );

                Lock();
                if (!MatchesRemoteHandoffLocked<TEvent>(index, generation)) {
                    Unlock();
                    return OrderedHandoffAttempt<RemoteResult>(
                        OrderedHandoffAttemptState::RuntimeUnavailable
                    );
                }

                auto& sequence = StateFor<TEvent>().Outbound;
                auto& slot = sequence.Slots[index];
                const auto now = TPlan::template SupportsTimedRetention<TEvent>
                    ? Clock::MonotonicNow()
                    : MonotonicTimestamp{};

                if (
                    slot.Deadline.Nanoseconds() != 0U &&
                    now >= slot.Deadline
                ) {
                    slot.Terminal = true;
                    AdvanceRemoteHandoffHeadLocked<TEvent>();
                    Unlock();
                    return OrderedHandoffAttempt<RemoteResult>(
                        OrderedHandoffAttemptState::Expired
                    );
                }

                SweepRemoteHandoffExpiryLocked<TEvent>(now);
                const auto head = static_cast<std::size_t>(sequence.Head);
                if (index != head || slot.InFlight) {
                    Unlock();
                    return OrderedHandoffAttempt<RemoteResult>(
                        OrderedHandoffAttemptState::EarlierPending
                    );
                }

                slot.InFlight = true;
                const auto* event = slot.EventView;
                Unlock();

                auto result = remoteOperation(*event);

                Lock();
                if (!MatchesRemoteHandoffLocked<TEvent>(index, generation)) {
                    InfrastructureFailure();
                }
                auto& completed = StateFor<TEvent>().Outbound.Slots[index];
                completed.InFlight = false;
                completed.Terminal = true;
                AdvanceRemoteHandoffHeadLocked<TEvent>();
                Unlock();

                return OrderedHandoffAttempt<RemoteResult>(
                    Memory::OwnershipTransfer::Move(result)
                );
            }

            /// Resolves one uncommitted handoff opportunity as a terminal skip.
            template<class TEvent>
            void AbortRemoteHandoff(
                std::size_t index,
                std::uint32_t generation
            ) noexcept {
                if constexpr (TPlan::template RemoteHandoffCapacity<TEvent> == 0U) {
                    static_cast<void>(index);
                    static_cast<void>(generation);
                } else {
                    Lock();
                    if (MatchesRemoteHandoffLocked<TEvent>(index, generation)) {
                        auto& slot = StateFor<TEvent>().Outbound.Slots[index];
                        if (!slot.InFlight) {
                            slot.Terminal = true;
                            AdvanceRemoteHandoffHeadLocked<TEvent>();
                        }
                    }
                    Unlock();
                }
            }

            /// Admits locally first and reserves the independent remote opportunity at the same linearization.
            template<class TEvent, class TRetention = UntilHandoff>
            requires EventType<TEvent> &&
                TPlan::template IsDeployed<TEvent> &&
                RetentionRequest<TRetention> &&
                (
                    TPlan::template SupportsTimedRetention<TEvent> ||
                    std::is_same_v<std::remove_cvref_t<TRetention>, UntilHandoff>
                )
            [[nodiscard]] auto Dispatch(
                LocalAndRemote,
                const TEvent& event,
                TRetention retention = {}
            ) noexcept {
                using RemoteResult = RemoteHandoffReservationResult<TEvent, Runtime>;
                static_assert(
                    std::is_nothrow_copy_constructible_v<TEvent>,
                    "LocalAndRemote Event Dispatch requires nothrow local occurrence copying"
                );

                if (!_initialized) {
                    InfrastructureFailure();
                }

                Lock();
                const auto normalized = Detail::NormalizeRetention(retention);
                if (normalized.Expired) {
                    Unlock();
                    return LocalAndRemoteDispatchResult<DispatchResult, RemoteResult>(
                        DispatchResult::Expired,
                        RemoteResult(ReservationFailure::Expired)
                    );
                }

                const auto local = DispatchLocalLocked(event, normalized);
                if (!_integrationOpen) {
                    Unlock();
                    return LocalAndRemoteDispatchResult<DispatchResult, RemoteResult>(
                        local,
                        RemoteResult(ReservationFailure::RuntimeUnavailable)
                    );
                }

                const auto now = TPlan::template SupportsTimedRetention<TEvent>
                    ? Clock::MonotonicNow()
                    : MonotonicTimestamp{};
                if (
                    normalized.Deadline.Nanoseconds() != 0U &&
                    now >= normalized.Deadline
                ) {
                    Unlock();
                    return LocalAndRemoteDispatchResult<DispatchResult, RemoteResult>(
                        local,
                        RemoteResult(ReservationFailure::Expired)
                    );
                }

                SweepRemoteHandoffExpiryLocked<TEvent>(now);
                constexpr auto capacity =
                    TPlan::template RemoteHandoffCapacity<TEvent>;

                if constexpr (capacity == 0U) {
                    Unlock();
                    return LocalAndRemoteDispatchResult<DispatchResult, RemoteResult>(
                        local,
                        RemoteResult(ReservationFailure::NoCapacity)
                    );
                } else {
                    const auto index = ReserveRemoteHandoffLocked(
                        event,
                        normalized.Deadline
                    );
                    if (index == capacity) {
                        Unlock();
                        return LocalAndRemoteDispatchResult<DispatchResult, RemoteResult>(
                            local,
                            RemoteResult(ReservationFailure::NoCapacity)
                        );
                    }

                    const auto generation =
                        StateFor<TEvent>().Outbound.Slots[index].Generation;
                    Unlock();
                    return LocalAndRemoteDispatchResult<DispatchResult, RemoteResult>(
                        local,
                        RemoteResult(*this, index, generation)
                    );
                }
            }

            template<class TEvent, class TRetention = UntilHandoff>
            requires (!std::is_lvalue_reference_v<TEvent>)
            auto Dispatch(
                LocalAndRemote,
                TEvent&&,
                TRetention = {}
            ) noexcept = delete;

            // Local shorthand and inbound facade.

            /// Performs LocalOnly admission using the concise default Event Dispatch surface.
            /// @tparam TEventArgument Concrete locally deployed Event argument Type.
            /// @tparam TRetention Supported retention request Type permitted by the deployment.
            /// @param event Event payload source.
            /// @param retention Per-Dispatch retention request.
            template<class TEventArgument, class TRetention = UntilHandoff>
            requires EventType<std::remove_cvref_t<TEventArgument>> &&
                RetentionRequest<TRetention> &&
                TPlan::template IsDeployed<std::remove_cvref_t<TEventArgument>> &&
                (
                    TPlan::template SupportsTimedRetention<std::remove_cvref_t<TEventArgument>> ||
                    std::is_same_v<std::remove_cvref_t<TRetention>, UntilHandoff>
                )
            [[nodiscard]] DispatchResult Dispatch(
                TEventArgument&& event,
                TRetention retention = {}
            ) noexcept {
                return Dispatch(
                    LocalOnly{},
                    std::forward<TEventArgument>(event),
                    retention
                );
            }

            /// Admits one already-deserialized inbound typed Event locally without automatic re-egress.
            /// @tparam TEventArgument Concrete locally deployed Event argument Type.
            /// @tparam TRetention Supported local retention request Type.
            /// @param event Typed Event payload supplied by an external inbound integration.
            /// @param retention Local ingress retention policy.
            template<class TEventArgument, class TRetention = UntilHandoff>
            requires EventType<std::remove_cvref_t<TEventArgument>> &&
                RetentionRequest<TRetention> &&
                TPlan::template IsDeployed<std::remove_cvref_t<TEventArgument>> &&
                (
                    TPlan::template SupportsTimedRetention<std::remove_cvref_t<TEventArgument>> ||
                    std::is_same_v<std::remove_cvref_t<TRetention>, UntilHandoff>
                )
            [[nodiscard]] DispatchResult Ingress(
                TEventArgument&& event,
                TRetention retention = {}
            ) noexcept {
                return Dispatch(
                    LocalOnly{},
                    std::forward<TEventArgument>(event),
                    retention
                );
            }

            // Bounded Listener servicing.

            /// Delivers at most the requested number of callbacks for one statically declared Listener.
            /// @tparam TThreadIdentity Listener Dedicated Thread identity.
            /// @param maximumDeliveries Explicit callback budget for this drain call.
            template<class TThreadIdentity>
            [[nodiscard]] DrainResult Drain(
                std::size_t maximumDeliveries
            ) noexcept {
                constexpr auto typeCount = TPlan::template ObservedEventTypes<TThreadIdentity>::Count;

                static_assert(
                    typeCount > 0U,
                    "Drain requires a statically declared Event Listener"
                );

                DrainResult result{};
                std::size_t cursor = 0U;

                if constexpr (typeCount > 1U) {
                    cursor = static_cast<std::size_t>(
                        std::get<
                            Detail::ListenerCursor<TPlan, TThreadIdentity>
                        >(_listenerCursors).Value
                    );
                }

                while (result.Delivered < maximumDeliveries) {
                    bool delivered = false;

                    for (
                        std::size_t scan = 0U;
                        scan < typeCount;
                        ++scan
                    ) {
                        const auto ordinal = (cursor + scan) % typeCount;

                        if (
                            TryDeliverOrdinal<TThreadIdentity>(ordinal) ==
                            Detail::DeliveryAttemptResult::Delivered
                        ) {
                            ++result.Delivered;
                            cursor = (ordinal + 1U) % typeCount;
                            SetCursor<TThreadIdentity>(cursor);
                            delivered = true;
                            break;
                        }
                    }

                    if (!delivered) {
                        break;
                    }
                }

                Lock();
                result.WorkRemaining = HasAnyPending<TThreadIdentity>();
                Unlock();

                if (result.WorkRemaining) {
                    auto thread = _threading->template ThreadHandle<TThreadIdentity>();
                    static_cast<void>(
                        thread.Wake()
                    );
                }

                return result;
            }

    };

} // ESPressio::Event
