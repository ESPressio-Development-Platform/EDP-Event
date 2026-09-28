#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>

#include <ESPressio_Clock.hpp>

#include "EventTypes.hpp"

namespace ESPressio::Event::Detail {

    /// Canonical retention decision used by local and scoped Dispatch paths.
    struct NormalizedRetention final {
        MonotonicTimestamp Deadline{};
        bool Expired{false};
    };

    /// Normalizes one Event retention request against one canonical monotonic observation.
    template<RetentionRequest TRetention>
    [[nodiscard]] inline NormalizedRetention NormalizeRetention(
        TRetention retention
    ) noexcept {
        if constexpr (std::is_same_v<std::remove_cvref_t<TRetention>, UntilHandoff>) {
            return {};
        } else if constexpr (std::is_same_v<std::remove_cvref_t<TRetention>, UntilDeadline>) {
            const auto now = Clock::MonotonicNow();
            return NormalizedRetention{
                retention.Value,
                now >= retention.Value
            };
        } else {
            const auto now = Clock::MonotonicNow();
            const auto duration = retention.Value.Nanoseconds();

            if (duration <= 0) {
                return NormalizedRetention{now, true};
            }

            const auto nowNanoseconds = now.Nanoseconds();
            const auto delta = static_cast<std::uint64_t>(duration);
            const auto maximum = std::numeric_limits<std::uint64_t>::max();
            const auto deadlineNanoseconds = delta > maximum - nowNanoseconds
                ? maximum
                : nowNanoseconds + delta;
            const auto deadline = MonotonicTimestamp::FromNanoseconds(
                deadlineNanoseconds
            );

            return NormalizedRetention{
                deadline,
                now >= deadline
            };
        }
    }

} // ESPressio::Event::Detail
