#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <ESPressio_Clock.hpp>
#include <ESPressio_Memory.hpp>

namespace ESPressio::Event {

    /// Canonical Event duration Type owned by EDP-Clock.
    using Duration = Clock::Duration;

    /// Canonical Event monotonic timestamp Type owned by EDP-Clock.
    using MonotonicTimestamp = Clock::MonotonicTimestamp;


    /// Result of one local Event admission attempt.
    enum class DispatchResult : std::uint8_t {
        /// Admission committed successfully.
        Accepted = 0U,

        /// Required bounded physical or pending capacity was unavailable.
        NoCapacity = 1U,

        /// The common retention precondition was already expired.
        Expired = 2U
    };


    /// Result of activating one statically planned Listener/Event relationship.
    enum class SubscribeResult : std::uint8_t {
        /// The previously inactive relationship became subscribed.
        Subscribed = 0U,

        /// The relationship was already subscribed and remained unchanged.
        AlreadySubscribed = 1U
    };


    /// Result of deactivating one statically planned Listener/Event relationship.
    enum class UnsubscribeResult : std::uint8_t {
        /// The previously active relationship became unsubscribed.
        Unsubscribed = 0U,

        /// The relationship was already unsubscribed and remained unchanged.
        NotSubscribed = 1U
    };


    /// Result of initializing one Event Runtime.
    enum class InitializationResult : std::uint8_t {
        /// Event Runtime initialization completed successfully.
        Initialized = 0U,

        /// The Event Runtime had already been initialized.
        AlreadyInitialized = 1U,

        /// A required owning-domain provider was not ready or available.
        ProviderFailure = 2U
    };


    /// Observable Event-level state of one external remote Dispatch attempt.
    enum class RemoteDispatchAttemptState : std::uint8_t {
        /// The common expiry gate prevented the remote operation from being called.
        SkippedExpired = 0U,

        /// The remote operation was called exactly once and produced a native result.
        Attempted = 1U
    };


    /// Selects local Event admission only.
    struct LocalOnly final {};


    /// Selects an external remote Event handoff only.
    struct RemoteOnly final {};


    /// Selects independent local admission and external remote Event handoff.
    struct LocalAndRemote final {};


    /// Identifies one supported Event execution-domain scope tag.
    /// @tparam TScope Candidate execution-domain scope Type.
    template<class TScope>
    concept ExecutionDomainScope =
        std::is_same_v<std::remove_cvref_t<TScope>, LocalOnly> ||
        std::is_same_v<std::remove_cvref_t<TScope>, RemoteOnly> ||
        std::is_same_v<std::remove_cvref_t<TScope>, LocalAndRemote>;


    /// Requests retention until every pending local handoff has occurred or been cancelled.
    struct UntilHandoff final {};


    /// Requests retention for one duration measured from Dispatch normalization time.
    struct ForDuration final {

        /// Requested retention duration in canonical Clock units.
        Duration Value;

    };


    /// Requests retention until one absolute canonical monotonic deadline.
    struct UntilDeadline final {

        /// Requested absolute canonical monotonic deadline.
        MonotonicTimestamp Value;

    };


    /// Identifies one supported per-Dispatch retention request Type.
    /// @tparam TRetention Candidate retention request Type.
    template<class TRetention>
    concept RetentionRequest =
        std::is_same_v<std::remove_cvref_t<TRetention>, UntilHandoff> ||
        std::is_same_v<std::remove_cvref_t<TRetention>, ForDuration> ||
        std::is_same_v<std::remove_cvref_t<TRetention>, UntilDeadline>;


    /// Event-owned wrapper describing whether an external remote operation was attempted.
    /// @tparam TRemoteResult Provider-defined non-void result Type returned by the remote operation.
    template<class TRemoteResult>
    class RemoteDispatchAttempt final {

        private:

            // Remote attempt state and conditional result storage.

            /// Authoritative presence/state indicator for the union-stored remote result.
            RemoteDispatchAttemptState _state{RemoteDispatchAttemptState::SkippedExpired};

            /// Manual storage allowing the provider-defined result to be absent when expiry skips the operation.
            union Storage {
                /// Inactive storage byte used while no remote result exists.
                char Empty;

                /// Provider-defined result present only while state is Attempted.
                TRemoteResult Result;

                /// Creates inactive storage without constructing a remote result.
                constexpr Storage() noexcept : Empty{} {
                }

                /// Leaves active-member destruction to RemoteDispatchAttempt through EDP-Memory.
                ~Storage() noexcept {
                }
            } _storage{};

        public:

            /// Compatibility alias exposing the Event-level attempt state Type through the wrapper.
            using State = RemoteDispatchAttemptState;

            static_assert(
                !std::is_void_v<TRemoteResult>,
                "Remote Dispatch result must be observable"
            );

            static_assert(
                std::is_nothrow_move_constructible_v<TRemoteResult>,
                "Remote Dispatch result must be nothrow move constructible"
            );

            static_assert(
                std::is_nothrow_destructible_v<TRemoteResult>,
                "Remote Dispatch result must be nothrow destructible"
            );

            // Construction and ownership.

            /// Creates a remote attempt which was skipped because expiry prevented invocation.
            RemoteDispatchAttempt() noexcept = default;

            /// Creates an attempted remote result by transferring the provider-defined result through EDP-Memory.
            /// @param result Provider-defined result produced by the remote operation.
            explicit RemoteDispatchAttempt(
                TRemoteResult result
            ) noexcept :
                _state(State::Attempted) {
                static_cast<void>(
                    Memory::ObjectLifetime::MoveConstruct<TRemoteResult>(
                        static_cast<void*>(&_storage.Result),
                        result
                    )
                );
            }

            /// Remote attempt wrappers cannot be copied because their provider result may own exclusive state.
            RemoteDispatchAttempt(const RemoteDispatchAttempt&) = delete;

            /// Remote attempt wrappers cannot be copy-assigned because their provider result may own exclusive state.
            RemoteDispatchAttempt& operator=(const RemoteDispatchAttempt&) = delete;

            /// Moves the optional provider result through EDP-Memory and leaves the source in SkippedExpired state.
            /// @param other Source wrapper whose owned result, if present, is transferred.
            RemoteDispatchAttempt(
                RemoteDispatchAttempt&& other
            ) noexcept :
                _state(other._state) {
                if (_state == State::Attempted) {
                    static_cast<void>(
                        Memory::ObjectLifetime::MoveConstruct<TRemoteResult>(
                            static_cast<void*>(&_storage.Result),
                            other._storage.Result
                        )
                    );
                    Memory::ObjectLifetime::Destroy(other._storage.Result);
                    other._state = State::SkippedExpired;
                }
            }

            /// Move assignment is deliberately unavailable so active union state cannot be overwritten ambiguously.
            RemoteDispatchAttempt& operator=(RemoteDispatchAttempt&&) = delete;

            /// Destroys the provider-defined result through EDP-Memory only when an attempted result is present.
            ~RemoteDispatchAttempt() noexcept {
                if (_state == State::Attempted) {
                    Memory::ObjectLifetime::Destroy(_storage.Result);
                }
            }

            // State inspection.

            /// Returns the authoritative Event-level remote-attempt state.
            [[nodiscard]] State GetState() const noexcept {
                return _state;
            }

            /// Indicates whether the remote operation was invoked.
            [[nodiscard]] bool WasAttempted() const noexcept {
                return _state == State::Attempted;
            }

            /// Indicates whether expiry prevented the remote operation from being invoked.
            [[nodiscard]] bool WasSkippedExpired() const noexcept {
                return _state == State::SkippedExpired;
            }

            // Provider result access.

            /// Returns the mutable provider-defined result when present, otherwise nullptr.
            [[nodiscard]] TRemoteResult* ResultIfPresent() noexcept {
                return _state == State::Attempted ? &_storage.Result : nullptr;
            }

            /// Returns the immutable provider-defined result when present, otherwise nullptr.
            [[nodiscard]] const TRemoteResult* ResultIfPresent() const noexcept {
                return _state == State::Attempted ? &_storage.Result : nullptr;
            }

    };


    /// Structurally separates independent local and remote results from LocalAndRemote Dispatch.
    /// @tparam TLocalResult Result Type produced by local Event admission.
    /// @tparam TRemoteResult Provider-defined result Type produced by the remote operation when attempted.
    template<class TLocalResult, class TRemoteResult>
    class LocalAndRemoteDispatchResult final {

        private:

            // Independent domain outcomes.

            /// Result produced by local Event admission.
            TLocalResult _local;

            /// Event-level remote attempt wrapper preserving the provider-defined result when present.
            RemoteDispatchAttempt<TRemoteResult> _remote;

        public:

            // Construction and ownership.

            /// Creates one combined structural result from independently produced domain outcomes.
            /// @param local Local admission result.
            /// @param remote Remote attempt/result wrapper.
            LocalAndRemoteDispatchResult(
                TLocalResult local,
                RemoteDispatchAttempt<TRemoteResult> remote
            ) noexcept :
                _local(Memory::OwnershipTransfer::Move(local)),
                _remote(Memory::OwnershipTransfer::Move(remote)) {
            }

            /// Combined results cannot be copied because either domain result may own exclusive state.
            LocalAndRemoteDispatchResult(const LocalAndRemoteDispatchResult&) = delete;

            /// Combined results cannot be copy-assigned because either domain result may own exclusive state.
            LocalAndRemoteDispatchResult& operator=(const LocalAndRemoteDispatchResult&) = delete;

            /// Transfers both independent domain results through the EDP-Memory ownership abstraction.
            /// @param other Source combined result whose independently owned outcomes are transferred.
            LocalAndRemoteDispatchResult(
                LocalAndRemoteDispatchResult&& other
            ) noexcept :
                _local(Memory::OwnershipTransfer::Move(other._local)),
                _remote(Memory::OwnershipTransfer::Move(other._remote)) {
            }

            /// Move assignment is deliberately unavailable to preserve simple single-construction result ownership.
            LocalAndRemoteDispatchResult& operator=(LocalAndRemoteDispatchResult&&) = delete;

            // Domain-result access.

            /// Returns the mutable local-domain result.
            [[nodiscard]] TLocalResult& Local() noexcept {
                return _local;
            }

            /// Returns the immutable local-domain result.
            [[nodiscard]] const TLocalResult& Local() const noexcept {
                return _local;
            }

            /// Returns the mutable remote-attempt wrapper.
            [[nodiscard]] RemoteDispatchAttempt<TRemoteResult>& Remote() noexcept {
                return _remote;
            }

            /// Returns the immutable remote-attempt wrapper.
            [[nodiscard]] const RemoteDispatchAttempt<TRemoteResult>& Remote() const noexcept {
                return _remote;
            }

    };


    /// Result of one bounded Listener drain operation.
    struct DrainResult final {

        // Drain outcome.

        /// Number of callbacks completed during this bounded drain call.
        std::size_t Delivered{0U};

        /// Indicates whether this Listener still has pending Event work after the drain call.
        bool WorkRemaining{false};

    };

} // ESPressio::Event
