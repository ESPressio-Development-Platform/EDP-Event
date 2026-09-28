#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

#include <ESPressio_BoundedTopology.hpp>

#include "EventTypes.hpp"
#include "Planner.hpp"

namespace ESPressio::Event {

    namespace Detail {

        /// Semantic tag separating one Event Type's occurrence-slot index space from all others.
        /// @tparam TPlan Normalized Event plan owning the deployment.
        /// @tparam TEvent Concrete deployed Event Type.
        template<class TPlan, class TEvent>
        struct OccurrenceIndexSpace final {};


        /// Semantic tag separating one Event Type's dense Listener index space from all others.
        /// @tparam TPlan Normalized Event plan owning the observation topology.
        /// @tparam TEvent Concrete deployed Event Type.
        template<class TPlan, class TEvent>
        struct ListenerIndexSpace final {};


        /// Smallest unsigned scalar capable of representing a bounded count up to TMaximum.
        /// @tparam TMaximum Maximum representable count derived from immutable topology.
        template<std::size_t TMaximum>
        using CountStorage = std::conditional_t<
            (TMaximum <= static_cast<std::size_t>(std::numeric_limits<std::uint8_t>::max())),
            std::uint8_t,
            std::conditional_t<
                (TMaximum <= static_cast<std::size_t>(std::numeric_limits<std::uint16_t>::max())),
                std::uint16_t,
                std::conditional_t<
                    (TMaximum <= static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())),
                    std::uint32_t,
                    std::uint64_t
                >
            >
        >;


        /// Stateless expiry storage used when a deployment cannot use timed retention.
        /// @tparam TTimed Compile-time timed-retention capability; false selects this primary form.
        template<bool TTimed>
        struct ExpiryStorage {

            // Construction.

            /// Accepts the normalized deadline without retaining it for an untimed deployment.
            /// @param deadline Ignored normalized deadline value.
            constexpr explicit ExpiryStorage(
                MonotonicTimestamp deadline
            ) noexcept {
                static_cast<void>(deadline);
            }

            // Deadline access.

            /// Returns the zero sentinel because this deployment retains no deadline.
            [[nodiscard]] constexpr MonotonicTimestamp Deadline() const noexcept {
                return MonotonicTimestamp{};
            }

            /// Ignores replacement deadline because this deployment retains no deadline.
            /// @param deadline Ignored normalized deadline value.
            constexpr void SetDeadline(
                MonotonicTimestamp deadline
            ) noexcept {
                static_cast<void>(deadline);
            }

        };


        /// Deadline-bearing expiry storage used by timed-retention deployments.
        template<>
        struct ExpiryStorage<true> {

            // Retention state.

            /// Absolute canonical monotonic expiry deadline, or zero for UntilHandoff.
            MonotonicTimestamp ExpiresAt{};

            // Construction.

            /// Retains one normalized absolute deadline.
            /// @param deadline Canonical monotonic deadline, with zero meaning UntilHandoff.
            constexpr explicit ExpiryStorage(
                MonotonicTimestamp deadline
            ) noexcept :
                ExpiresAt(deadline) {
            }

            // Deadline access and replacement.

            /// Returns the retained canonical monotonic deadline.
            [[nodiscard]] constexpr MonotonicTimestamp Deadline() const noexcept {
                return ExpiresAt;
            }

            /// Replaces the retained deadline during unborrowed NewestOnly supersession.
            /// @param deadline New normalized canonical monotonic deadline.
            constexpr void SetDeadline(
                MonotonicTimestamp deadline
            ) noexcept {
                ExpiresAt = deadline;
            }

        };


        /// Stateless Queue-link storage used by non-Queue admission policies.
        /// @tparam TIndex Strong occurrence index Type.
        /// @tparam TQueued Compile-time Queue topology flag; false selects this primary form.
        template<class TIndex, bool TQueued>
        struct QueueLinkStorage {

            /// Creates the stateless non-Queue storage form.
            constexpr QueueLinkStorage() noexcept = default;

        };


        /// Intrusive Queue-link storage used by Queue admission policies.
        /// @tparam TIndex Strong occurrence index Type used by the owning Event Type.
        template<class TIndex>
        struct QueueLinkStorage<TIndex, true> {

            // Intrusive Queue state.

            /// Strong index of the next pending occurrence, or Invalid at the Queue tail.
            TIndex Next = TIndex::Invalid();

            /// Creates an unlinked Queue record.
            constexpr QueueLinkStorage() noexcept = default;

        };


        /// Stateless active-borrow storage used when no Listener can observe the Event Type.
        /// @tparam TListeners Eligible Listener count.
        /// @tparam THasListeners Derived flag selecting stateful borrow storage for positive count.
        template<std::size_t TListeners, bool THasListeners = (TListeners > 0U)>
        struct BorrowStorage {

            /// Creates the stateless no-Listener borrow representation.
            constexpr BorrowStorage() noexcept = default;

            /// Reports zero because no callback borrow can exist without an eligible Listener.
            [[nodiscard]] constexpr std::size_t Count() const noexcept {
                return 0U;
            }

            /// Performs no work because this specialization cannot own a borrow.
            constexpr void Increment() noexcept {
            }

            /// Performs no work because this specialization cannot own a borrow.
            constexpr void Decrement() noexcept {
            }

        };


        /// Compact active-borrow storage used when one or more Listeners can observe the Event Type.
        /// @tparam TListeners Eligible Listener count defining the maximum simultaneous borrow count.
        template<std::size_t TListeners>
        struct BorrowStorage<TListeners, true> {

            // Compact borrow representation.

            /// Minimum-width scalar capable of representing every active Listener borrow.
            using Storage = CountStorage<TListeners>;

            /// Current number of callbacks holding the occurrence immutable borrow.
            Storage Active{0U};

            /// Creates the borrow state with no active callback.
            constexpr BorrowStorage() noexcept = default;

            /// Returns the current active callback borrow count.
            [[nodiscard]] constexpr std::size_t Count() const noexcept {
                return static_cast<std::size_t>(Active);
            }

            /// Adds one active callback borrow after a successful Listener claim.
            constexpr void Increment() noexcept {
                ++Active;
            }

            /// Removes one active callback borrow after the callback returns.
            constexpr void Decrement() noexcept {
                --Active;
            }

        };

    } // ESPressio::Event::Detail


    /// Planner-shaped Memory-owned retained representation for one Event occurrence.
    /// @tparam TPlan Normalized Event plan defining capacities and Listener topology.
    /// @tparam TEvent Concrete locally deployed Event payload Type.
    template<class TPlan, class TEvent>
    requires EventType<TEvent> && TPlan::template IsDeployed<TEvent>
    class OccurrenceRecord final :
        private Detail::ExpiryStorage<TPlan::template SupportsTimedRetention<TEvent>>,
        private Detail::QueueLinkStorage<
            BoundedTopology::BoundedIndex<
                Detail::OccurrenceIndexSpace<TPlan, TEvent>,
                TPlan::template Deployment<TEvent>::MaximumInstances
            >,
            TPlan::template IsQueue<TEvent>
        >,
        private Detail::BorrowStorage<TPlan::template EligibleListenerCount<TEvent>> {

        public:

            // Planner-derived public metadata.

            /// Concrete Event payload Type retained in this occurrence record.
            using Event = TEvent;

            /// Local deployment declaration defining this record's hard policy/resource choices.
            using Deployment = typename TPlan::template Deployment<TEvent>;

            /// Exact simultaneous live occurrence capacity for this Event Type.
            static constexpr std::size_t MaximumInstances = Deployment::MaximumInstances;

            /// Exact number of statically eligible Listeners for this Event Type.
            static constexpr std::size_t ListenerCount = TPlan::template EligibleListenerCount<TEvent>;

            /// Indicates whether this record physically retains a timed deadline.
            static constexpr bool Timed = TPlan::template SupportsTimedRetention<TEvent>;

            /// Indicates whether this record participates in intrusive FIFO Queue topology.
            static constexpr bool Queued = TPlan::template IsQueue<TEvent>;

            /// Strong compact physical occurrence-slot identity.
            using OccurrenceIndex = BoundedTopology::BoundedIndex<
                Detail::OccurrenceIndexSpace<TPlan, TEvent>,
                MaximumInstances
            >;

            /// Strong compact Type-local Listener identity.
            using ListenerIndex = BoundedTopology::BoundedIndex<
                Detail::ListenerIndexSpace<TPlan, TEvent>,
                ListenerCount
            >;

            /// Compact one-bit-per-Listener pending-recipient set.
            using ListenerSet = BoundedTopology::BoundedIndexSet<
                Detail::ListenerIndexSpace<TPlan, TEvent>,
                ListenerCount
            >;

        private:

            // Conditional storage-base aliases.

            /// Expiry storage selected from local retention capability.
            using ExpiryBase = Detail::ExpiryStorage<Timed>;

            /// Queue-link storage selected from admission shape.
            using QueueBase = Detail::QueueLinkStorage<OccurrenceIndex, Queued>;

            /// Active-borrow storage selected from Listener cardinality.
            using BorrowBase = Detail::BorrowStorage<ListenerCount>;

            // Authoritative occurrence payload and pending-recipient state.

            /// Immutable-by-contract Event payload retained for the lifetime of this occurrence.
            TEvent _event;

            /// Snapshot of Listener interests still awaiting handoff; zero-capacity form occupies no unique state.
            [[no_unique_address]] ListenerSet _pending{};

        public:

            // Construction and lifetime.

            /// Materializes one admitted occurrence from a non-throwing Event construction path.
            /// @tparam TEventArgument Source argument preserving the concrete Event Type.
            /// @param event Event payload source consumed only after admission viability is established.
            /// @param pending Snapshot of active Listener subscriptions at admission linearization.
            /// @param deadline Normalized canonical deadline, or zero for UntilHandoff.
            template<class TEventArgument>
            requires std::is_same_v<std::remove_cvref_t<TEventArgument>, TEvent>
            explicit OccurrenceRecord(
                TEventArgument&& event,
                const ListenerSet& pending,
                MonotonicTimestamp deadline
            ) noexcept(std::is_nothrow_constructible_v<TEvent, TEventArgument&&>) :
                ExpiryBase(deadline),
                QueueBase(),
                BorrowBase(),
                _event(std::forward<TEventArgument>(event)),
                _pending(pending) {
            }

            /// Occurrences have stable Memory-pool identity and therefore cannot be copied.
            OccurrenceRecord(const OccurrenceRecord&) = delete;

            /// Occurrences have stable Memory-pool identity and therefore cannot be copy-assigned.
            OccurrenceRecord& operator=(const OccurrenceRecord&) = delete;

            /// Occurrences have stable Memory-pool identity and therefore cannot be moved.
            OccurrenceRecord(OccurrenceRecord&&) = delete;

            /// Occurrences have stable Memory-pool identity and therefore cannot be move-assigned.
            OccurrenceRecord& operator=(OccurrenceRecord&&) = delete;

            /// Destroys the retained payload when EDP-Memory releases the physical occurrence slot.
            ~OccurrenceRecord() noexcept = default;

            // NewestOnly supersession.

            /// Replaces one uncommitted occurrence in-place for NewestOnly supersession.
            /// The caller must hold Event synchronization and prove there are no active borrows.
            /// @tparam TEventArgument Source argument preserving the concrete Event Type.
            /// @param event Replacement Event payload.
            /// @param pending Fresh subscription snapshot for the replacement occurrence.
            /// @param deadline Replacement normalized canonical deadline.
            template<class TEventArgument>
            requires std::is_same_v<std::remove_cvref_t<TEventArgument>, TEvent> &&
                std::is_nothrow_constructible_v<TEvent, TEventArgument&&>
            void ReplaceUnborrowed(
                TEventArgument&& event,
                const ListenerSet& pending,
                MonotonicTimestamp deadline
            ) noexcept {
                _event.~TEvent();
                ::new (static_cast<void*>(&_event)) TEvent(
                    std::forward<TEventArgument>(event)
                );
                _pending = pending;
                ExpiryBase::SetDeadline(deadline);

                if constexpr (Queued) {
                    QueueBase::Next = OccurrenceIndex::Invalid();
                }
            }

            // Payload and recipient access.

            /// Returns the mutable payload reference used only by Event-core implementation.
            [[nodiscard]] TEvent& Value() noexcept {
                return _event;
            }

            /// Returns the immutable payload reference borrowed by Listener callbacks.
            [[nodiscard]] const TEvent& Value() const noexcept {
                return _event;
            }

            /// Returns mutable pending-recipient state for Event-core bookkeeping.
            [[nodiscard]] ListenerSet& PendingRecipients() noexcept {
                return _pending;
            }

            /// Returns immutable pending-recipient state for inspection.
            [[nodiscard]] const ListenerSet& PendingRecipients() const noexcept {
                return _pending;
            }

            /// Indicates whether any Listener interest remains pending.
            [[nodiscard]] bool HasPendingRecipients() const noexcept {
                return _pending.IsAnySet();
            }

            /// Returns the number of callbacks currently borrowing the immutable payload.
            [[nodiscard]] std::size_t ActiveBorrowCount() const noexcept {
                return BorrowBase::Count();
            }

            // Claim and borrow lifecycle.

            /// Claims this occurrence for one pending Listener and establishes an active borrow.
            /// @param listener Type-local Listener identity attempting the claim.
            /// @return True only when this Listener had pending interest and the claim was established.
            [[nodiscard]] bool Claim(
                ListenerIndex listener
            ) noexcept {
                if (!_pending.IsSet(listener)) {
                    return false;
                }

                static_cast<void>(
                    _pending.Clear(listener)
                );
                BorrowBase::Increment();
                return true;
            }

            /// Releases one active callback borrow after callback return.
            void ReleaseBorrow() noexcept {
                BorrowBase::Decrement();
            }

            // Retention inspection.

            /// Returns the retained absolute deadline, or zero for UntilHandoff/untimed deployment.
            [[nodiscard]] MonotonicTimestamp ExpiresAt() const noexcept {
                return ExpiryBase::Deadline();
            }

            /// Indicates whether still-pending interests have expired at the supplied canonical time.
            /// @param now Current canonical monotonic timestamp.
            [[nodiscard]] bool IsExpired(
                MonotonicTimestamp now
            ) const noexcept {
                if constexpr (!Timed) {
                    static_cast<void>(now);
                    return false;
                } else {
                    const auto deadline = ExpiresAt();
                    return deadline.Nanoseconds() != 0U && now >= deadline;
                }
            }

            // Intrusive Queue contract.

            /// Returns the next pending occurrence index for Queue deployments.
            [[nodiscard]] OccurrenceIndex QueueNext() const noexcept requires Queued {
                return QueueBase::Next;
            }

            /// Replaces the next pending occurrence index for Queue deployments.
            /// @param next Next occurrence index, or Invalid for Queue tail.
            void SetQueueNext(
                OccurrenceIndex next
            ) noexcept requires Queued {
                QueueBase::Next = next;
            }

    };

} // ESPressio::Event
