#pragma once

#include <type_traits>
#include <utility>

#include "Deployment.hpp"
#include "EventTypes.hpp"
#include "Reservation.hpp"
#include "Retention.hpp"

namespace ESPressio::Event {

    /// Executes one already-selected outbound-only Event handoff after the common expiry gate.
    ///
    /// Ordering between concurrent RemoteOnly calls remains a responsibility of the selected
    /// binding/Transport implementation because no local Event Runtime is required to exist.
    /// @tparam TEvent Concrete Event payload Type presented to the remote operation.
    /// @tparam TRetention Per-Dispatch retention request Type.
    /// @tparam TRemoteOperation External bounded remote-domain operation Type.
    template<EventType TEvent, RetentionRequest TRetention, class TRemoteOperation>
    [[nodiscard]] auto DispatchScoped(
        RemoteOnly,
        const TEvent& event,
        TRetention retention,
        TRemoteOperation& remoteOperation
    ) noexcept {
        using RemoteResult = decltype(remoteOperation(event));

        static_assert(
            noexcept(remoteOperation(event)),
            "RemoteOnly Event handoff must be non-throwing"
        );

        static_assert(
            !std::is_void_v<RemoteResult>,
            "RemoteOnly Event handoff requires an observable provider result"
        );

        static_assert(
            std::is_nothrow_move_constructible_v<RemoteResult>,
            "Remote Event result must be nothrow move constructible"
        );

        static_assert(
            std::is_nothrow_destructible_v<RemoteResult>,
            "Remote Event result must be nothrow destructible"
        );

        const auto normalized = Detail::NormalizeRetention(retention);

        if (normalized.Expired) {
            return RemoteDispatchAttempt<RemoteResult>{};
        }

        return RemoteDispatchAttempt<RemoteResult>{
            remoteOperation(event)
        };
    }

} // ESPressio::Event
