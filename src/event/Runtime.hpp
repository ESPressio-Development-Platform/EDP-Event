#pragma once

#include <cstddef>
#include <cstdint>
#include <exception>
#include <tuple>
#include <type_traits>
#include <utility>

#include <ESPressio_Clock.hpp>
#include <ESPressio_Memory.hpp>
#include <ESPressio_Threading.hpp>

#include "Composition.hpp"
#include "Occurrence.hpp"
#include "Retention.hpp"

namespace ESPressio::Event {

    namespace Detail {

        template<class TPlan, class TEvent, bool TQueue = TPlan::template IsQueue<TEvent>>
        struct AdmissionState;

        template<class TPlan, class TEvent>
        struct AdmissionState<TPlan, TEvent, true> final {
            using Record = OccurrenceRecord<TPlan, TEvent>;
            using Index = typename Record::OccurrenceIndex;
            using Queue = BoundedTopology::IntrusiveQueue<
                typename Index::IndexSpace,
                Record::MaximumInstances
            >;

            Queue Pending{};
        };

        template<class TPlan, class TEvent>
        struct AdmissionState<TPlan, TEvent, false> final {
            using Record = OccurrenceRecord<TPlan, TEvent>;
            using Index = typename Record::OccurrenceIndex;
            Index Pending = Index::Invalid();
        };

        template<class TPlan, class TEvent>
        struct TypeState final {
            using Record = OccurrenceRecord<TPlan, TEvent>;
            [[no_unique_address]] typename Record::ListenerSet Subscriptions{};
            AdmissionState<TPlan, TEvent> Admission{};
        };

        template<class TPlan, class TList>
        struct TypeStateTuple;

        template<class TPlan, class... TEvents>
        struct TypeStateTuple<TPlan, Primitives::TypeList<TEvents...>> final {
            using Type = std::tuple<TypeState<TPlan, TEvents>...>;
        };

        template<class TPlan, class TThread, std::size_t TCount = TPlan::template ObservedEventTypes<TThread>::Count>
        struct ListenerCursor final {
            using Storage = CountStorage<TCount>;
            Storage Value{0U};
        };

        template<class TPlan, class TThread>
        struct ListenerCursor<TPlan, TThread, 0U> final {};

        template<class TPlan, class TThread>
        struct ListenerCursor<TPlan, TThread, 1U> final {};

        template<class TPlan, class TList>
        struct ListenerCursorTuple;

        template<class TPlan, class... TThreads>
        struct ListenerCursorTuple<TPlan, Primitives::TypeList<TThreads...>> final {
            using Type = std::tuple<ListenerCursor<TPlan, TThreads>...>;
        };

        template<std::size_t TCapacity, bool TEnabled = (TCapacity > 0U)>
        struct SharedPendingCounter final {
            using Storage = CountStorage<TCapacity>;
            Storage Used{0U};
            [[nodiscard]] std::size_t Count() const noexcept { return static_cast<std::size_t>(Used); }
            void Increment() noexcept { ++Used; }
            void Decrement() noexcept { --Used; }
        };

        template<std::size_t TCapacity>
        struct SharedPendingCounter<TCapacity, false> final {
            [[nodiscard]] constexpr std::size_t Count() const noexcept { return 0U; }
            constexpr void Increment() noexcept {}
            constexpr void Decrement() noexcept {}
        };

        template<class TPlan, class TEvent, class TPool>
        class RecordView final {
            TPool* _pool;
        public:
            explicit RecordView(TPool& pool) noexcept : _pool(&pool) {}
            OccurrenceRecord<TPlan, TEvent>& operator[](std::size_t index) noexcept {
                return _pool->DedicatedObject(
                    TPool::DedicatedIndex::FromUnchecked(index)
                );
            }
        };

        template<class TList>
        struct AnyTimedDeployment;

        template<class... TEvents>
        struct AnyTimedDeployment<Primitives::TypeList<TEvents...>> {
            template<class TPlan>
            static consteval bool For() {
                return (TPlan::template SupportsTimedRetention<TEvents> || ... || false);
            }
        };

    } // Event::Detail


    /// Bounded runtime realizing one normalized Event family plan over application-owned dependencies.
    template<
        class TPlan,
        class TArchitecture,
        class TMemoryRuntime,
        class TThreadingRuntime,
        class TMutexProvider,
        class... TCallbackProviders
    >
    class Runtime final : public RuntimeProvider<TPlan> {

        using PrimitiveTypes = typename TPlan::PrimitiveTypes;
        using Listeners = typename TPlan::Listeners;
        using Observations = typename TPlan::Observations;
        using TypeStates = typename Detail::TypeStateTuple<TPlan, PrimitiveTypes>::Type;
        using ListenerCursors = typename Detail::ListenerCursorTuple<TPlan, Listeners>::Type;

        using RequiredCallbackProviders = typename Detail::RequiredCallbackProviders<
            TArchitecture,
            Observations
        >::Type;
        using BoundCallbackProviders = Primitives::TypeList<TCallbackProviders...>;

        static_assert(
            std::is_same_v<RequiredCallbackProviders, BoundCallbackProviders>,
            "Event Runtime callback provider bindings must exactly match the unique providers resolved by Architecture"
        );

        TMemoryRuntime* _memory;
        TThreadingRuntime* _threading;
        TMutexProvider* _mutex;
        std::tuple<TCallbackProviders*...> _callbacks;
        TypeStates _types{};
        [[no_unique_address]] ListenerCursors _listenerCursors{};
        [[no_unique_address]] Detail::SharedPendingCounter<TPlan::SharedPendingCapacity> _sharedPending{};
        bool _initialized{false};

        [[noreturn]] static void InfrastructureFailure() noexcept {
            std::terminate();
        }

        void Lock() noexcept {
            if (_mutex->Acquire() != Threading::OrdinaryMutexAcquireResult::Acquired) {
                InfrastructureFailure();
            }
        }

        void Unlock() noexcept {
            if (_mutex->Release() != Threading::OrdinaryMutexReleaseResult::Released) {
                InfrastructureFailure();
            }
        }

        template<class TEvent>
        using Record = OccurrenceRecord<TPlan, TEvent>;

        template<class TEvent>
        using Pool = typename TMemoryRuntime::template ObjectPoolType<Record<TEvent>>;

        template<class TEvent>
        using State = Detail::TypeState<TPlan, TEvent>;

        template<class TEvent>
        State<TEvent>& StateFor() noexcept {
            return std::get<State<TEvent>>(_types);
        }

        template<class TEvent>
        auto& PoolFor() noexcept {
            return _memory->template ObjectPoolFor<Record<TEvent>>();
        }

        template<class TEvent>
        typename Record<TEvent>::OccurrenceIndex ToEventIndex(
            typename Pool<TEvent>::DedicatedIndex index
        ) const noexcept {
            return Record<TEvent>::OccurrenceIndex::FromUnchecked(
                static_cast<std::size_t>(index.Value())
            );
        }

        template<class TEvent>
        auto ToMemoryIndex(typename Record<TEvent>::OccurrenceIndex index) noexcept {
            return Pool<TEvent>::DedicatedIndex::FromUnchecked(
                static_cast<std::size_t>(index.Value())
            );
        }

        template<class TEvent>
        Record<TEvent>& RecordAt(typename Record<TEvent>::OccurrenceIndex index) noexcept {
            auto memoryIndex = ToMemoryIndex<TEvent>(index);
            return PoolFor<TEvent>().DedicatedObject(memoryIndex);
        }

        template<class TEvent>
        void ReleaseOccurrence(typename Record<TEvent>::OccurrenceIndex index) noexcept {
            auto memoryIndex = ToMemoryIndex<TEvent>(index);
            const auto result = PoolFor<TEvent>().ReleaseDedicated(memoryIndex);
            if (result != Memory::DedicatedObjectPoolReleaseResult::Released) {
                InfrastructureFailure();
            }
        }

        template<class TEvent>
        Detail::RecordView<TPlan, TEvent, Pool<TEvent>> Records() noexcept {
            return Detail::RecordView<TPlan, TEvent, Pool<TEvent>>(
                PoolFor<TEvent>()
            );
        }

        template<class TEvent>
        std::size_t QueuePendingCount() noexcept requires (TPlan::template IsQueue<TEvent>) {
            auto& state = StateFor<TEvent>().Admission;
            auto current = state.Pending.Head();
            std::size_t count = 0U;
            for (; count < Record<TEvent>::MaximumInstances && current.IsValid(); ++count) {
                current = RecordAt<TEvent>(current).QueueNext();
            }
            return count;
        }

        template<class TEvent>
        void RemovePendingQueueOccurrence(
            typename Record<TEvent>::OccurrenceIndex index,
            std::size_t pendingBefore
        ) noexcept requires (TPlan::template IsQueue<TEvent>) {
            auto records = Records<TEvent>();
            if (StateFor<TEvent>().Admission.Pending.Remove(records, index) != BoundedTopology::IntrusiveQueueRemoveResult::Removed) {
                InfrastructureFailure();
            }
            if (pendingBefore > TPlan::template DedicatedPendingCapacity<TEvent>) {
                _sharedPending.Decrement();
            }
        }

        template<class TEvent>
        void SweepExpired(MonotonicTimestamp now) noexcept {
            if constexpr (!TPlan::template SupportsTimedRetention<TEvent>) {
                static_cast<void>(now);
            } else if constexpr (TPlan::template IsQueue<TEvent>) {
                auto& queue = StateFor<TEvent>().Admission.Pending;
                auto current = queue.Head();
                std::size_t pending = QueuePendingCount<TEvent>();
                std::size_t visited = 0U;
                while (current.IsValid() && visited++ < Record<TEvent>::MaximumInstances) {
                    auto next = RecordAt<TEvent>(current).QueueNext();
                    auto& record = RecordAt<TEvent>(current);
                    if (record.IsExpired(now)) {
                        record.PendingRecipients().ClearAll();
                        RemovePendingQueueOccurrence<TEvent>(current, pending);
                        --pending;
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

        template<class TEvent>
        typename Record<TEvent>::ListenerSet SnapshotSubscriptions() noexcept {
            return StateFor<TEvent>().Subscriptions;
        }

        template<class TThread, class TEvent>
        static constexpr auto ListenerIndexFor() noexcept {
            using Index = typename Record<TEvent>::ListenerIndex;
            return Index::FromUnchecked(TPlan::template ListenerOrdinal<TEvent, TThread>);
        }

        template<class TEvent, std::size_t TIndex = 0U>
        void WakeRecipients(const typename Record<TEvent>::ListenerSet& recipients) noexcept {
            using ListenerTypes = typename TPlan::template EligibleListenerTypes<TEvent>;
            if constexpr (TIndex < ListenerTypes::Count) {
                using ThreadIdentity = typename Detail::TypeAt<ListenerTypes, TIndex>::Type;
                if (recipients.IsSet(Record<TEvent>::ListenerIndex::FromUnchecked(TIndex))) {
                    auto thread = _threading->template ThreadHandle<ThreadIdentity>();
                    static_cast<void>(thread.Wake());
                }
                WakeRecipients<TEvent, TIndex + 1U>(recipients);
            }
        }

        template<class TEventArgument>
        DispatchResult DispatchLocalLocked(
            TEventArgument&& event,
            Detail::NormalizedRetention retention
        ) noexcept {
            using TEvent = std::remove_cvref_t<TEventArgument>;
            static_assert(TPlan::template IsDeployed<TEvent>, "Local Event Dispatch requires a local deployment");
            static_assert(std::is_nothrow_constructible_v<TEvent, TEventArgument&&>,
                "Local Event Dispatch requires nothrow occurrence construction");

            if (retention.Expired) return DispatchResult::Expired;
            const auto deadline = retention.Deadline;
            const auto now = TPlan::template SupportsTimedRetention<TEvent>
                ? Clock::MonotonicNow()
                : MonotonicTimestamp{};
            if (deadline.Nanoseconds() != 0U && now >= deadline) return DispatchResult::Expired;

            SweepExpired<TEvent>(now);
            auto recipients = SnapshotSubscriptions<TEvent>();
            if (!recipients.IsAnySet()) return DispatchResult::Accepted;

            auto& pool = PoolFor<TEvent>();
            auto& state = StateFor<TEvent>();

            if constexpr (TPlan::template IsQueue<TEvent>) {
                const auto pending = QueuePendingCount<TEvent>();
                const bool needsShared = pending >= TPlan::template DedicatedPendingCapacity<TEvent>;
                if (needsShared && _sharedPending.Count() >= TPlan::SharedPendingCapacity) {
                    return DispatchResult::NoCapacity;
                }

                typename std::remove_reference_t<decltype(pool)>::DedicatedIndex memoryIndex;
                const auto result = pool.AcquireDedicated(
                    memoryIndex,
                    std::forward<TEventArgument>(event),
                    recipients,
                    deadline
                );
                if (result == Memory::DedicatedObjectPoolAcquisitionResult::CapacityUnavailable) {
                    return DispatchResult::NoCapacity;
                }
                if (result != Memory::DedicatedObjectPoolAcquisitionResult::Succeeded) {
                    InfrastructureFailure();
                }

                const auto eventIndex = ToEventIndex<TEvent>(memoryIndex);
                auto records = Records<TEvent>();
                if (state.Admission.Pending.Push(records, eventIndex) != BoundedTopology::IntrusiveQueuePushResult::Succeeded) {
                    InfrastructureFailure();
                }
                if (needsShared) _sharedPending.Increment();
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
                const auto result = pool.AcquireDedicated(
                    memoryIndex,
                    std::forward<TEventArgument>(event),
                    recipients,
                    deadline
                );
                if (result == Memory::DedicatedObjectPoolAcquisitionResult::CapacityUnavailable) {
                    return DispatchResult::NoCapacity;
                }
                if (result != Memory::DedicatedObjectPoolAcquisitionResult::Succeeded) {
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

        template<class TThread, class TEvent>
        bool TryDeliverOne() noexcept {
            using EventRecord = Record<TEvent>;
            using Index = typename EventRecord::OccurrenceIndex;
            const auto listener = ListenerIndexFor<TThread, TEvent>();
            Index selected = Index::Invalid();

            Lock();
            const auto now = TPlan::template SupportsTimedRetention<TEvent>
                ? Clock::MonotonicNow()
                : MonotonicTimestamp{};
            SweepExpired<TEvent>(now);

            auto& state = StateFor<TEvent>();
            if constexpr (TPlan::template IsQueue<TEvent>) {
                auto current = state.Admission.Pending.Head();
                for (std::size_t visited = 0U; visited < EventRecord::MaximumInstances && current.IsValid(); ++visited) {
                    auto& record = RecordAt<TEvent>(current);
                    if (record.PendingRecipients().IsSet(listener)) {
                        selected = current;
                        break;
                    }
                    current = record.QueueNext();
                }
            } else {
                if (state.Admission.Pending.IsValid() &&
                    RecordAt<TEvent>(state.Admission.Pending).PendingRecipients().IsSet(listener)) {
                    selected = state.Admission.Pending;
                }
            }

            if (!selected.IsValid()) {
                Unlock();
                return false;
            }

            auto& record = RecordAt<TEvent>(selected);
            if (!record.Claim(listener)) InfrastructureFailure();

            if (!record.HasPendingRecipients()) {
                if constexpr (TPlan::template IsQueue<TEvent>) {
                    const auto pendingBefore = QueuePendingCount<TEvent>();
                    RemovePendingQueueOccurrence<TEvent>(selected, pendingBefore);
                } else {
                    state.Admission.Pending = Index::Invalid();
                }
            }

            const TEvent* borrowed = &record.Value();
            Unlock();

            using CallbackProvider = Composition::ListenerCallbackProvider<
                TThread,
                TEvent,
                TArchitecture
            >;
            auto* callback = std::get<CallbackProvider*>(_callbacks);
            static_assert(
                requires(decltype(*callback)& provider, const TEvent& value) {
                    { provider.OnEvent(value) } noexcept -> std::same_as<void>;
                },
                "Event Listener callback provider must expose void OnEvent(const TEvent&) noexcept"
            );
            callback->OnEvent(*borrowed);

            Lock();
            auto& after = RecordAt<TEvent>(selected);
            after.ReleaseBorrow();
            if (!after.HasPendingRecipients() && after.ActiveBorrowCount() == 0U) {
                ReleaseOccurrence<TEvent>(selected);
            }
            Unlock();
            return true;
        }

        template<class TThread, std::size_t TIndex = 0U>
        bool TryDeliverOrdinal(std::size_t ordinal) noexcept {
            using Types = typename TPlan::template ObservedEventTypes<TThread>;
            if constexpr (TIndex >= Types::Count) {
                return false;
            } else {
                if (ordinal == TIndex) {
                    using TEvent = typename Detail::TypeAt<Types, TIndex>::Type;
                    return TryDeliverOne<TThread, TEvent>();
                }
                return TryDeliverOrdinal<TThread, TIndex + 1U>(ordinal);
            }
        }

        template<class TThread>
        void SetCursor(std::size_t value) noexcept {
            constexpr auto count = TPlan::template ObservedEventTypes<TThread>::Count;
            if constexpr (count > 1U) {
                std::get<Detail::ListenerCursor<TPlan, TThread>>(_listenerCursors).Value =
                    static_cast<typename Detail::ListenerCursor<TPlan, TThread>::Storage>(value % count);
            } else {
                static_cast<void>(value);
            }
        }

        template<class TThread, class TEvent>
        bool HasPendingForType() noexcept {
            const auto listener = ListenerIndexFor<TThread, TEvent>();
            auto& state = StateFor<TEvent>();
            if constexpr (TPlan::template IsQueue<TEvent>) {
                auto current = state.Admission.Pending.Head();
                for (std::size_t visited = 0U; visited < Record<TEvent>::MaximumInstances && current.IsValid(); ++visited) {
                    auto& record = RecordAt<TEvent>(current);
                    if (record.PendingRecipients().IsSet(listener)) return true;
                    current = record.QueueNext();
                }
                return false;
            } else {
                return state.Admission.Pending.IsValid() &&
                    RecordAt<TEvent>(state.Admission.Pending).PendingRecipients().IsSet(listener);
            }
        }

        template<class TThread, std::size_t TIndex = 0U>
        bool HasAnyPending() noexcept {
            using Types = typename TPlan::template ObservedEventTypes<TThread>;
            if constexpr (TIndex >= Types::Count) {
                return false;
            } else {
                using TEvent = typename Detail::TypeAt<Types, TIndex>::Type;
                return HasPendingForType<TThread, TEvent>() || HasAnyPending<TThread, TIndex + 1U>();
            }
        }

        template<class TEvent>
        static consteval bool MemoryPoolValid() {
            using R = Record<TEvent>;
            if constexpr (!TMemoryRuntime::Topology::template ContainsObjectPool<R>) {
                return false;
            } else {
                using Spec = typename TMemoryRuntime::Topology::template ObjectPoolSpecFor<R>;
                return Spec::Dedicated::Value == TPlan::template Deployment<TEvent>::MaximumInstances &&
                    !Spec::Shared::IsEnabled;
            }
        }

        template<std::size_t TIndex = 0U>
        static consteval bool AllMemoryPoolsValid() {
            if constexpr (TIndex >= PrimitiveTypes::Count) {
                return true;
            } else {
                using TEvent = typename Detail::TypeAt<PrimitiveTypes, TIndex>::Type;
                return MemoryPoolValid<TEvent>() && AllMemoryPoolsValid<TIndex + 1U>();
            }
        }

    public:
        static_assert(AllMemoryPoolsValid(),
            "Event Runtime requires an exact dedicated-only EDP-Memory occurrence pool for every local Event Type");

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

        Runtime(const Runtime&) = delete;
        Runtime& operator=(const Runtime&) = delete;
        Runtime(Runtime&&) = delete;
        Runtime& operator=(Runtime&&) = delete;

        [[nodiscard]] InitializationResult Initialize() noexcept {
            if (_initialized) return InitializationResult::AlreadyInitialized;
            if (!_memory->IsInitialized()) return InitializationResult::ProviderFailure;
            if constexpr (Detail::AnyTimedDeployment<PrimitiveTypes>::template For<TPlan>()) {
                if (!Clock::IsMonotonicClockBound()) return InitializationResult::ProviderFailure;
            }
            _initialized = true;
            return InitializationResult::Initialized;
        }

        [[nodiscard]] bool IsInitialized() const noexcept { return _initialized; }

        template<class TThread, class TEvent>
        requires EventType<TEvent> && TPlan::template IsDeployed<TEvent>
        [[nodiscard]] SubscribeResult Subscribe() noexcept {
            constexpr auto ordinal = TPlan::template ListenerOrdinal<TEvent, TThread>;
            static_assert(ordinal < TPlan::template EligibleListenerCount<TEvent>,
                "Subscribe requires a statically declared Observe relation");
            const auto listener = ListenerIndexFor<TThread, TEvent>();
            Lock();
            auto& subscriptions = StateFor<TEvent>().Subscriptions;
            if (subscriptions.IsSet(listener)) {
                Unlock();
                return SubscribeResult::AlreadySubscribed;
            }
            static_cast<void>(subscriptions.Set(listener));
            Unlock();
            return SubscribeResult::Subscribed;
        }

        template<class TThread, class TEvent>
        requires EventType<TEvent> && TPlan::template IsDeployed<TEvent>
        [[nodiscard]] UnsubscribeResult Unsubscribe() noexcept {
            constexpr auto ordinal = TPlan::template ListenerOrdinal<TEvent, TThread>;
            static_assert(ordinal < TPlan::template EligibleListenerCount<TEvent>,
                "Unsubscribe requires a statically declared Observe relation");
            const auto listener = ListenerIndexFor<TThread, TEvent>();
            Lock();
            auto& state = StateFor<TEvent>();
            if (!state.Subscriptions.IsSet(listener)) {
                Unlock();
                return UnsubscribeResult::NotSubscribed;
            }
            static_cast<void>(state.Subscriptions.Clear(listener));

            if constexpr (TPlan::template IsQueue<TEvent>) {
                auto current = state.Admission.Pending.Head();
                std::size_t pending = QueuePendingCount<TEvent>();
                std::size_t visited = 0U;
                while (current.IsValid() && visited++ < Record<TEvent>::MaximumInstances) {
                    auto& record = RecordAt<TEvent>(current);
                    const auto next = record.QueueNext();
                    if (record.PendingRecipients().IsSet(listener)) {
                        static_cast<void>(record.PendingRecipients().Clear(listener));
                        if (!record.HasPendingRecipients()) {
                            RemovePendingQueueOccurrence<TEvent>(current, pending);
                            --pending;
                            if (record.ActiveBorrowCount() == 0U) ReleaseOccurrence<TEvent>(current);
                        }
                    }
                    current = next;
                }
            } else {
                auto& pending = state.Admission.Pending;
                if (pending.IsValid()) {
                    auto& record = RecordAt<TEvent>(pending);
                    if (record.PendingRecipients().IsSet(listener)) {
                        static_cast<void>(record.PendingRecipients().Clear(listener));
                        if (!record.HasPendingRecipients()) {
                            const auto old = pending;
                            pending = Record<TEvent>::OccurrenceIndex::Invalid();
                            if (record.ActiveBorrowCount() == 0U) ReleaseOccurrence<TEvent>(old);
                        }
                    }
                }
            }

            Unlock();
            return UnsubscribeResult::Unsubscribed;
        }

        template<class TEventArgument, class TRetention = UntilHandoff>
        requires EventType<std::remove_cvref_t<TEventArgument>> && RetentionRequest<TRetention> &&
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
            if (!_initialized) InfrastructureFailure();
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

        template<class TEvent, class TRetention = UntilHandoff, class TRemoteOperation>
        requires EventType<TEvent> && RetentionRequest<TRetention> && TPlan::template IsDeployed<TEvent> &&
            (
                TPlan::template SupportsTimedRetention<TEvent> ||
                std::is_same_v<std::remove_cvref_t<TRetention>, UntilHandoff>
            )
        [[nodiscard]] auto Dispatch(
            LocalAndRemote,
            const TEvent& event,
            TRetention retention,
            TRemoteOperation& remoteOperation
        ) noexcept {
            using RemoteResult = decltype(remoteOperation(event));
            static_assert(noexcept(remoteOperation(event)),
                "LocalAndRemote Event handoff must be non-throwing");
            static_assert(!std::is_void_v<RemoteResult>,
                "LocalAndRemote Event handoff requires an observable remote provider result");
            static_assert(std::is_nothrow_copy_constructible_v<TEvent>,
                "LocalAndRemote Event Dispatch requires nothrow local occurrence copying");
            static_assert(
                TPlan::template SupportsTimedRetention<TEvent> ||
                std::is_same_v<std::remove_cvref_t<TRetention>, UntilHandoff>,
                "Timed retention is unavailable for an UntilHandoffOnly local Event deployment"
            );

            if (!_initialized) InfrastructureFailure();
            Lock();
            const auto normalized = Detail::NormalizeRetention(retention);
            if (normalized.Expired) {
                Unlock();
                return LocalAndRemoteDispatchResult<DispatchResult, RemoteResult>(
                    DispatchResult::Expired,
                    RemoteDispatchAttempt<RemoteResult>{}
                );
            }

            const auto local = DispatchLocalLocked(event, normalized);
            auto remote = RemoteDispatchAttempt<RemoteResult>{remoteOperation(event)};
            Unlock();
            return LocalAndRemoteDispatchResult<DispatchResult, RemoteResult>(
                local,
                std::move(remote)
            );
        }

        template<class TEventArgument, class TRetention = UntilHandoff>
        requires EventType<std::remove_cvref_t<TEventArgument>> && RetentionRequest<TRetention> &&
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

        template<class TEventArgument, class TRetention = UntilHandoff>
        requires EventType<std::remove_cvref_t<TEventArgument>> && RetentionRequest<TRetention> &&
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

        template<class TThread>
        [[nodiscard]] DrainResult Drain(std::size_t maximumDeliveries) noexcept {
            constexpr auto typeCount = TPlan::template ObservedEventTypes<TThread>::Count;
            static_assert(typeCount > 0U, "Drain requires a statically declared Event Listener");
            DrainResult result{};
            std::size_t cursor = 0U;
            if constexpr (typeCount > 1U) {
                cursor = static_cast<std::size_t>(
                    std::get<Detail::ListenerCursor<TPlan, TThread>>(_listenerCursors).Value
                );
            }

            while (result.Delivered < maximumDeliveries) {
                bool delivered = false;
                for (std::size_t scan = 0U; scan < typeCount; ++scan) {
                    const auto ordinal = (cursor + scan) % typeCount;
                    if (TryDeliverOrdinal<TThread>(ordinal)) {
                        ++result.Delivered;
                        cursor = (ordinal + 1U) % typeCount;
                        SetCursor<TThread>(cursor);
                        delivered = true;
                        break;
                    }
                }
                if (!delivered) break;
            }

            Lock();
            result.WorkRemaining = HasAnyPending<TThread>();
            Unlock();
            if (result.WorkRemaining) {
                auto thread = _threading->template ThreadHandle<TThread>();
                static_cast<void>(thread.Wake());
            }
            return result;
        }
    };

} // ESPressio::Event
