#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <ESPressio_Clock.hpp>

namespace ESPressio::Event {

    using Duration = Clock::Duration;
    using MonotonicTimestamp = Clock::MonotonicTimestamp;

    enum class DispatchResult : std::uint8_t {
        Accepted = 0U,
        NoCapacity = 1U,
        Expired = 2U
    };

    enum class SubscribeResult : std::uint8_t {
        Subscribed = 0U,
        AlreadySubscribed = 1U
    };

    enum class UnsubscribeResult : std::uint8_t {
        Unsubscribed = 0U,
        NotSubscribed = 1U
    };

    enum class InitializationResult : std::uint8_t {
        Initialized = 0U,
        AlreadyInitialized = 1U,
        ProviderFailure = 2U
    };

    struct LocalOnly final {};
    struct RemoteOnly final {};
    struct LocalAndRemote final {};

    template<class TScope>
    concept ExecutionDomainScope =
        std::is_same_v<std::remove_cvref_t<TScope>, LocalOnly> ||
        std::is_same_v<std::remove_cvref_t<TScope>, RemoteOnly> ||
        std::is_same_v<std::remove_cvref_t<TScope>, LocalAndRemote>;

    struct UntilHandoff final {};

    struct ForDuration final {
        Duration Value;
    };

    struct UntilDeadline final {
        MonotonicTimestamp Value;
    };

    template<class TRetention>
    concept RetentionRequest =
        std::is_same_v<std::remove_cvref_t<TRetention>, UntilHandoff> ||
        std::is_same_v<std::remove_cvref_t<TRetention>, ForDuration> ||
        std::is_same_v<std::remove_cvref_t<TRetention>, UntilDeadline>;

    template<class TRemoteResult>
    class RemoteDispatchAttempt final {
    public:
        enum class State : std::uint8_t {
            SkippedExpired = 0U,
            Attempted = 1U
        };

    private:
        State _state{State::SkippedExpired};
        union Storage {
            char Empty;
            TRemoteResult Result;

            constexpr Storage() noexcept : Empty{} {}
            ~Storage() noexcept {}
        } _storage{};

    public:
        static_assert(!std::is_void_v<TRemoteResult>, "Remote Dispatch result must be observable");
        static_assert(std::is_nothrow_move_constructible_v<TRemoteResult>, "Remote Dispatch result must be nothrow move constructible");
        static_assert(std::is_nothrow_destructible_v<TRemoteResult>, "Remote Dispatch result must be nothrow destructible");

        RemoteDispatchAttempt() noexcept = default;

        explicit RemoteDispatchAttempt(TRemoteResult result) noexcept :
            _state(State::Attempted) {
            ::new (static_cast<void*>(&_storage.Result)) TRemoteResult(std::move(result));
        }

        RemoteDispatchAttempt(const RemoteDispatchAttempt&) = delete;
        RemoteDispatchAttempt& operator=(const RemoteDispatchAttempt&) = delete;

        RemoteDispatchAttempt(RemoteDispatchAttempt&& other) noexcept :
            _state(other._state) {
            if (_state == State::Attempted) {
                ::new (static_cast<void*>(&_storage.Result)) TRemoteResult(std::move(other._storage.Result));
                other._storage.Result.~TRemoteResult();
                other._state = State::SkippedExpired;
            }
        }

        RemoteDispatchAttempt& operator=(RemoteDispatchAttempt&&) = delete;

        ~RemoteDispatchAttempt() noexcept {
            if (_state == State::Attempted) {
                _storage.Result.~TRemoteResult();
            }
        }

        [[nodiscard]] State GetState() const noexcept { return _state; }
        [[nodiscard]] bool WasAttempted() const noexcept { return _state == State::Attempted; }
        [[nodiscard]] bool WasSkippedExpired() const noexcept { return _state == State::SkippedExpired; }

        [[nodiscard]] TRemoteResult& Result() noexcept { return _storage.Result; }
        [[nodiscard]] const TRemoteResult& Result() const noexcept { return _storage.Result; }
    };

    template<class TLocalResult, class TRemoteResult>
    class LocalAndRemoteDispatchResult final {
        TLocalResult _local;
        RemoteDispatchAttempt<TRemoteResult> _remote;

    public:
        LocalAndRemoteDispatchResult(
            TLocalResult local,
            RemoteDispatchAttempt<TRemoteResult> remote
        ) noexcept :
            _local(std::move(local)),
            _remote(std::move(remote)) {
        }

        LocalAndRemoteDispatchResult(const LocalAndRemoteDispatchResult&) = delete;
        LocalAndRemoteDispatchResult& operator=(const LocalAndRemoteDispatchResult&) = delete;
        LocalAndRemoteDispatchResult(LocalAndRemoteDispatchResult&&) noexcept = default;
        LocalAndRemoteDispatchResult& operator=(LocalAndRemoteDispatchResult&&) = delete;

        [[nodiscard]] TLocalResult& Local() noexcept { return _local; }
        [[nodiscard]] const TLocalResult& Local() const noexcept { return _local; }
        [[nodiscard]] RemoteDispatchAttempt<TRemoteResult>& Remote() noexcept { return _remote; }
        [[nodiscard]] const RemoteDispatchAttempt<TRemoteResult>& Remote() const noexcept { return _remote; }
    };

    struct DrainResult final {
        std::size_t Delivered{0U};
        bool WorkRemaining{false};
    };

} // ESPressio::Event
