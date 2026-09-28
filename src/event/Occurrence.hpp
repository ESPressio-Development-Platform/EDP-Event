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

        template<class TPlan, class TEvent>
        struct OccurrenceIndexSpace final {};

        template<class TPlan, class TEvent>
        struct ListenerIndexSpace final {};

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

        template<bool TTimed>
        struct ExpiryStorage {
            constexpr explicit ExpiryStorage(MonotonicTimestamp) noexcept {}
            [[nodiscard]] constexpr MonotonicTimestamp Deadline() const noexcept { return MonotonicTimestamp{}; }
            constexpr void SetDeadline(MonotonicTimestamp) noexcept {}
        };

        template<>
        struct ExpiryStorage<true> {
            MonotonicTimestamp ExpiresAt{};
            constexpr explicit ExpiryStorage(MonotonicTimestamp deadline) noexcept : ExpiresAt(deadline) {}
            [[nodiscard]] constexpr MonotonicTimestamp Deadline() const noexcept { return ExpiresAt; }
            constexpr void SetDeadline(MonotonicTimestamp deadline) noexcept { ExpiresAt = deadline; }
        };

        template<class TIndex, bool TQueued>
        struct QueueLinkStorage {
            constexpr QueueLinkStorage() noexcept = default;
        };

        template<class TIndex>
        struct QueueLinkStorage<TIndex, true> {
            TIndex Next = TIndex::Invalid();
            constexpr QueueLinkStorage() noexcept = default;
        };

        template<std::size_t TListeners, bool THasListeners = (TListeners > 0U)>
        struct BorrowStorage {
            constexpr BorrowStorage() noexcept = default;
            [[nodiscard]] constexpr std::size_t Count() const noexcept { return 0U; }
            constexpr void Increment() noexcept {}
            constexpr void Decrement() noexcept {}
        };

        template<std::size_t TListeners>
        struct BorrowStorage<TListeners, true> {
            using Storage = CountStorage<TListeners>;
            Storage Active{0U};

            constexpr BorrowStorage() noexcept = default;
            [[nodiscard]] constexpr std::size_t Count() const noexcept { return static_cast<std::size_t>(Active); }
            constexpr void Increment() noexcept { ++Active; }
            constexpr void Decrement() noexcept { --Active; }
        };

    } // Event::Detail

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
        using Event = TEvent;
        using Deployment = typename TPlan::template Deployment<TEvent>;
        static constexpr std::size_t MaximumInstances = Deployment::MaximumInstances;
        static constexpr std::size_t ListenerCount = TPlan::template EligibleListenerCount<TEvent>;
        static constexpr bool Timed = TPlan::template SupportsTimedRetention<TEvent>;
        static constexpr bool Queued = TPlan::template IsQueue<TEvent>;

        using OccurrenceIndex = BoundedTopology::BoundedIndex<
            Detail::OccurrenceIndexSpace<TPlan, TEvent>,
            MaximumInstances
        >;
        using ListenerIndex = BoundedTopology::BoundedIndex<
            Detail::ListenerIndexSpace<TPlan, TEvent>,
            ListenerCount
        >;
        using ListenerSet = BoundedTopology::BoundedIndexSet<
            Detail::ListenerIndexSpace<TPlan, TEvent>,
            ListenerCount
        >;

    private:
        using ExpiryBase = Detail::ExpiryStorage<Timed>;
        using QueueBase = Detail::QueueLinkStorage<OccurrenceIndex, Queued>;
        using BorrowBase = Detail::BorrowStorage<ListenerCount>;

        TEvent _event;
        ListenerSet _pending{};

    public:
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

        OccurrenceRecord(const OccurrenceRecord&) = delete;
        OccurrenceRecord& operator=(const OccurrenceRecord&) = delete;
        OccurrenceRecord(OccurrenceRecord&&) = delete;
        OccurrenceRecord& operator=(OccurrenceRecord&&) = delete;
        ~OccurrenceRecord() noexcept = default;


        /// Replaces one uncommitted occurrence in-place for NewestOnly supersession.
        /// The caller must hold Event synchronization and prove there are no active borrows.
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

        [[nodiscard]] TEvent& Value() noexcept { return _event; }
        [[nodiscard]] const TEvent& Value() const noexcept { return _event; }

        [[nodiscard]] ListenerSet& PendingRecipients() noexcept { return _pending; }
        [[nodiscard]] const ListenerSet& PendingRecipients() const noexcept { return _pending; }

        [[nodiscard]] bool HasPendingRecipients() const noexcept { return _pending.IsAnySet(); }
        [[nodiscard]] std::size_t ActiveBorrowCount() const noexcept { return BorrowBase::Count(); }

        [[nodiscard]] bool Claim(ListenerIndex listener) noexcept {
            if (!_pending.IsSet(listener)) { return false; }
            static_cast<void>(_pending.Clear(listener));
            BorrowBase::Increment();
            return true;
        }

        void ReleaseBorrow() noexcept {
            BorrowBase::Decrement();
        }

        [[nodiscard]] MonotonicTimestamp ExpiresAt() const noexcept {
            return ExpiryBase::Deadline();
        }

        [[nodiscard]] bool IsExpired(MonotonicTimestamp now) const noexcept {
            if constexpr (!Timed) {
                static_cast<void>(now);
                return false;
            } else {
                const auto deadline = ExpiresAt();
                return deadline.Nanoseconds() != 0U && now >= deadline;
            }
        }

        [[nodiscard]] OccurrenceIndex QueueNext() const noexcept requires Queued {
            return QueueBase::Next;
        }

        void SetQueueNext(OccurrenceIndex next) noexcept requires Queued {
            QueueBase::Next = next;
        }
    };

} // ESPressio::Event
