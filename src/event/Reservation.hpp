#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <type_traits>
#include <utility>

#include "Deployment.hpp"
#include "EventTypes.hpp"

namespace ESPressio::Event {

    /// Optional provider result from one bounded ordered handoff attempt.
    template<class TRemoteResult>
    class OrderedHandoffAttempt final {
    private:
        OrderedHandoffAttemptState _state;
        std::optional<TRemoteResult> _result{};

    public:
        static_assert(!std::is_void_v<TRemoteResult>);
        static_assert(std::is_nothrow_move_constructible_v<TRemoteResult>);
        static_assert(std::is_nothrow_destructible_v<TRemoteResult>);

        explicit OrderedHandoffAttempt(OrderedHandoffAttemptState state) noexcept :
            _state(state) {
            if (state == OrderedHandoffAttemptState::Attempted) {
                std::terminate();
            }
        }

        explicit OrderedHandoffAttempt(TRemoteResult result) noexcept :
            _state(OrderedHandoffAttemptState::Attempted),
            _result(std::in_place, Memory::OwnershipTransfer::Move(result)) {
        }

        OrderedHandoffAttempt(const OrderedHandoffAttempt&) = delete;
        OrderedHandoffAttempt& operator=(const OrderedHandoffAttempt&) = delete;
        OrderedHandoffAttempt(OrderedHandoffAttempt&&) noexcept = default;
        OrderedHandoffAttempt& operator=(OrderedHandoffAttempt&&) = delete;

        [[nodiscard]] OrderedHandoffAttemptState GetState() const noexcept {
            return _state;
        }

        [[nodiscard]] TRemoteResult* ResultIfPresent() noexcept {
            return _result ? &*_result : nullptr;
        }

        [[nodiscard]] const TRemoteResult* ResultIfPresent() const noexcept {
            return _result ? &*_result : nullptr;
        }
    };


    template<EventType TEvent, class TRuntime>
    class IngressReservationResult;

    template<EventType TEvent, class TRuntime>
    class RemoteHandoffReservationResult;


    /// Move-only owner of one constructed but unpublished inbound Event occurrence.
    template<EventType TEvent, class TRuntime>
    class IngressReservation final {
    private:
        TRuntime* _runtime{nullptr};
        std::size_t _index{0U};
        std::uint32_t _generation{0U};

        friend class IngressReservationResult<TEvent, TRuntime>;

        IngressReservation(
            TRuntime& runtime,
            std::size_t index,
            std::uint32_t generation
        ) noexcept :
            _runtime(&runtime),
            _index(index),
            _generation(generation) {
        }

    public:
        IngressReservation() noexcept = default;
        IngressReservation(const IngressReservation&) = delete;
        IngressReservation& operator=(const IngressReservation&) = delete;

        IngressReservation(IngressReservation&& other) noexcept :
            _runtime(other._runtime),
            _index(other._index),
            _generation(other._generation) {
            other._runtime = nullptr;
        }

        IngressReservation& operator=(IngressReservation&& other) noexcept {
            if (this != &other) {
                Abort();
                _runtime = other._runtime;
                _index = other._index;
                _generation = other._generation;
                other._runtime = nullptr;
            }
            return *this;
        }

        ~IngressReservation() noexcept {
            Abort();
        }

        [[nodiscard]] bool IsValid() const noexcept {
            return _runtime != nullptr;
        }

        [[nodiscard]] TEvent& Value() noexcept {
            if (_runtime == nullptr) {
                std::terminate();
            }
            return _runtime->template IngressValue<TEvent>(_index, _generation);
        }

        [[nodiscard]] DispatchResult Commit() noexcept {
            if (_runtime == nullptr) {
                return DispatchResult::RuntimeUnavailable;
            }
            auto* runtime = _runtime;
            _runtime = nullptr;
            return runtime->template CommitIngress<TEvent>(_index, _generation);
        }

        void Abort() noexcept {
            if (_runtime != nullptr) {
                _runtime->template AbortIngress<TEvent>(_index, _generation);
                _runtime = nullptr;
            }
        }
    };


    /// Result of reserving exact inbound Event admission backing.
    template<EventType TEvent, class TRuntime>
    class IngressReservationResult final {
    private:
        bool _accepted{false};
        ReservationFailure _failure{ReservationFailure::NoCapacity};
        IngressReservation<TEvent, TRuntime> _reservation{};

    public:
        explicit IngressReservationResult(ReservationFailure failure) noexcept :
            _failure(failure) {
        }

        IngressReservationResult(
            TRuntime& runtime,
            std::size_t index,
            std::uint32_t generation
        ) noexcept :
            _accepted(true),
            _reservation(runtime, index, generation) {
        }

        IngressReservationResult(const IngressReservationResult&) = delete;
        IngressReservationResult& operator=(const IngressReservationResult&) = delete;
        IngressReservationResult(IngressReservationResult&&) noexcept = default;
        IngressReservationResult& operator=(IngressReservationResult&&) = delete;

        [[nodiscard]] bool Accepted() const noexcept {
            return _accepted;
        }

        [[nodiscard]] ReservationFailure Failure() const noexcept {
            return _failure;
        }

        [[nodiscard]] IngressReservation<TEvent, TRuntime> TakeReservation() && noexcept {
            return Memory::OwnershipTransfer::Move(_reservation);
        }
    };


    /// Move-only owner of one per-Type ordered outbound Event handoff opportunity.
    ///
    /// Only a caller-owned immutable Event borrow is retained. TryCommit never blocks and the
    /// reservation remains valid when an earlier opportunity still owns the sequencer head.
    template<EventType TEvent, class TRuntime>
    class RemoteHandoffReservation final {
    private:
        TRuntime* _runtime{nullptr};
        std::size_t _index{0U};
        std::uint32_t _generation{0U};

        friend class RemoteHandoffReservationResult<TEvent, TRuntime>;

        RemoteHandoffReservation(
            TRuntime& runtime,
            std::size_t index,
            std::uint32_t generation
        ) noexcept :
            _runtime(&runtime),
            _index(index),
            _generation(generation) {
        }

    public:
        RemoteHandoffReservation() noexcept = default;
        RemoteHandoffReservation(const RemoteHandoffReservation&) = delete;
        RemoteHandoffReservation& operator=(const RemoteHandoffReservation&) = delete;

        RemoteHandoffReservation(RemoteHandoffReservation&& other) noexcept :
            _runtime(other._runtime),
            _index(other._index),
            _generation(other._generation) {
            other._runtime = nullptr;
        }

        RemoteHandoffReservation& operator=(RemoteHandoffReservation&& other) noexcept {
            if (this != &other) {
                Abort();
                _runtime = other._runtime;
                _index = other._index;
                _generation = other._generation;
                other._runtime = nullptr;
            }
            return *this;
        }

        ~RemoteHandoffReservation() noexcept {
            Abort();
        }

        [[nodiscard]] bool IsValid() const noexcept {
            return _runtime != nullptr;
        }

        template<class TRemoteOperation>
        [[nodiscard]] auto TryCommit(TRemoteOperation& remoteOperation) noexcept {
            using RemoteResult = decltype(remoteOperation(std::declval<const TEvent&>()));
            if (_runtime == nullptr) {
                return OrderedHandoffAttempt<RemoteResult>(
                    OrderedHandoffAttemptState::RuntimeUnavailable
                );
            }

            auto result = _runtime->template TryCommitRemoteHandoff<TEvent>(
                _index,
                _generation,
                remoteOperation
            );
            if (result.GetState() != OrderedHandoffAttemptState::EarlierPending) {
                _runtime = nullptr;
            }
            return result;
        }

        void Abort() noexcept {
            if (_runtime != nullptr) {
                _runtime->template AbortRemoteHandoff<TEvent>(_index, _generation);
                _runtime = nullptr;
            }
        }
    };


    /// Result of reserving one ordered outbound handoff slot.
    template<EventType TEvent, class TRuntime>
    class RemoteHandoffReservationResult final {
    private:
        bool _accepted{false};
        ReservationFailure _failure{ReservationFailure::NoCapacity};
        RemoteHandoffReservation<TEvent, TRuntime> _reservation{};

    public:
        explicit RemoteHandoffReservationResult(ReservationFailure failure) noexcept :
            _failure(failure) {
        }

        RemoteHandoffReservationResult(
            TRuntime& runtime,
            std::size_t index,
            std::uint32_t generation
        ) noexcept :
            _accepted(true),
            _reservation(runtime, index, generation) {
        }

        RemoteHandoffReservationResult(const RemoteHandoffReservationResult&) = delete;
        RemoteHandoffReservationResult& operator=(const RemoteHandoffReservationResult&) = delete;
        RemoteHandoffReservationResult(RemoteHandoffReservationResult&&) noexcept = default;
        RemoteHandoffReservationResult& operator=(RemoteHandoffReservationResult&&) = delete;

        [[nodiscard]] bool Accepted() const noexcept {
            return _accepted;
        }

        [[nodiscard]] ReservationFailure Failure() const noexcept {
            return _failure;
        }

        [[nodiscard]] RemoteHandoffReservation<TEvent, TRuntime> TakeReservation() && noexcept {
            return Memory::OwnershipTransfer::Move(_reservation);
        }
    };


    /// Typed facade for transactional inbound population.
    template<EventType TEvent, class TRuntime>
    class InboundAdmission final {
    private:
        TRuntime* _runtime;

    public:
        explicit InboundAdmission(TRuntime& runtime) noexcept :
            _runtime(&runtime) {
        }

        template<RetentionRequest TRetention = UntilHandoff>
        [[nodiscard]] auto Prepare(TRetention retention = {}) noexcept
        requires std::is_nothrow_default_constructible_v<TEvent> {
            return _runtime->template PrepareIngress<TEvent>(retention);
        }
    };


    /// Typed facade for per-Type ordered outbound handoff reservation.
    template<EventType TEvent, class TRuntime>
    class OutboundHandoff final {
    private:
        TRuntime* _runtime;

    public:
        explicit OutboundHandoff(TRuntime& runtime) noexcept :
            _runtime(&runtime) {
        }

        template<RetentionRequest TRetention = UntilHandoff>
        [[nodiscard]] auto Prepare(
            const TEvent& event,
            TRetention retention = {}
        ) noexcept {
            return _runtime->template PrepareRemoteHandoff<TEvent>(event, retention);
        }

        template<RetentionRequest TRetention = UntilHandoff>
        auto Prepare(TEvent&&, TRetention = {}) noexcept = delete;
    };


    /// Move-only source-facing semantic operation over a bounded frozen remote recipient set.
    ///
    /// The adapter binding enforces duplicate DeliveryIdentifier handling, finite observation,
    /// cooperative cancellation and bounded record lifetime. Event exposes no response or
    /// listener-completion acknowledgement because remote lifecycle ends at destination admission.
    template<EventType TEvent, class TBinding>
    requires std::is_nothrow_move_constructible_v<TBinding> &&
        std::is_nothrow_destructible_v<TBinding>
    class RemoteEventOperation final {
    private:
        TBinding _binding;
        bool _valid{true};

        void EnsureValid() const noexcept {
            if (!_valid) {
                std::terminate();
            }
        }

    public:
        using Event = TEvent;
        using Binding = TBinding;

        static_assert(
            requires(TBinding& binding) {
                { binding.Release() } noexcept -> std::same_as<void>;
            },
            "Remote Event binding must provide deterministic non-throwing release"
        );

        explicit RemoteEventOperation(TBinding binding) noexcept :
            _binding(Memory::OwnershipTransfer::Move(binding)) {
        }

        RemoteEventOperation(const RemoteEventOperation&) = delete;
        RemoteEventOperation& operator=(const RemoteEventOperation&) = delete;

        RemoteEventOperation(RemoteEventOperation&& other) noexcept :
            _binding(Memory::OwnershipTransfer::Move(other._binding)),
            _valid(other._valid) {
            other._valid = false;
        }

        RemoteEventOperation& operator=(RemoteEventOperation&&) = delete;

        ~RemoteEventOperation() noexcept {
            Release();
        }

        [[nodiscard]] bool IsValid() const noexcept {
            return _valid;
        }

        [[nodiscard]] std::size_t RecipientCount() const noexcept {
            EnsureValid();
            static_assert(noexcept(_binding.RecipientCount()));
            return _binding.RecipientCount();
        }

        [[nodiscard]] decltype(auto) Recipient(std::size_t index) const noexcept {
            EnsureValid();
            static_assert(noexcept(_binding.Recipient(index)));
            return _binding.Recipient(index);
        }

        [[nodiscard]] auto Observe(std::size_t index) const noexcept {
            EnsureValid();
            static_assert(noexcept(_binding.Observe(index)));
            return _binding.Observe(index);
        }

        [[nodiscard]] auto WaitFor(std::size_t index, Duration duration) noexcept {
            EnsureValid();
            static_assert(noexcept(_binding.WaitFor(index, duration)));
            return _binding.WaitFor(index, duration);
        }

        [[nodiscard]] auto WaitUntil(
            std::size_t index,
            MonotonicTimestamp deadline
        ) noexcept {
            EnsureValid();
            static_assert(noexcept(_binding.WaitUntil(index, deadline)));
            return _binding.WaitUntil(index, deadline);
        }

        [[nodiscard]] auto RequestCancellation(std::size_t index) noexcept {
            EnsureValid();
            static_assert(noexcept(_binding.RequestCancellation(index)));
            return _binding.RequestCancellation(index);
        }

        void Release() noexcept {
            if (_valid) {
                _binding.Release();
                _valid = false;
            }
        }
    };

} // ESPressio::Event
