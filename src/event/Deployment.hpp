#pragma once

#include <cstddef>
#include <type_traits>

#include <bounded/MemoryBoundedTraits.hpp>
#include <ESPressio_Primitives.hpp>

#include "EventFamily.hpp"

namespace ESPressio::Event {

    template<class TEvent>
    concept EventType =
        Primitives::PrimitiveType<TEvent> &&
        std::is_same_v<Primitives::PrimitiveFamilyOf<TEvent>, Family> &&
        Bounded::IsMemoryBoundedValue<TEvent> &&
        !Bounded::MemoryBoundedTraits<std::remove_cv_t<TEvent>>::HasExternalLifetimeDependencies &&
        std::is_nothrow_destructible_v<TEvent>;

    template<std::size_t TDedicatedPending>
    struct Queue final {
        static constexpr std::size_t DedicatedPending = TDedicatedPending;
    };

    struct NewestOnly final {};
    struct UntilHandoffOnly final {};
    struct TimedRetention final {};

    namespace Detail {
        template<class TAdmission>
        struct AdmissionTraits final {
            static constexpr bool IsValid = false;
            static constexpr bool IsQueue = false;
            static constexpr std::size_t DedicatedPending = 0U;
        };

        template<std::size_t TCapacity>
        struct AdmissionTraits<Queue<TCapacity>> final {
            static constexpr bool IsValid = true;
            static constexpr bool IsQueue = true;
            static constexpr std::size_t DedicatedPending = TCapacity;
        };

        template<>
        struct AdmissionTraits<NewestOnly> final {
            static constexpr bool IsValid = true;
            static constexpr bool IsQueue = false;
            static constexpr std::size_t DedicatedPending = 0U;
        };

        template<class TRetention>
        inline constexpr bool IsRetentionPolicyV =
            std::is_same_v<TRetention, UntilHandoffOnly> ||
            std::is_same_v<TRetention, TimedRetention>;
    }

    template<
        class TEvent,
        std::size_t TMaximumInstances,
        class TAdmission,
        class TRetention
    >
    struct Deploy final {
        static_assert(EventType<TEvent>, "Deploy requires a bounded, self-contained Event Type");
        static_assert(TMaximumInstances > 0U && TMaximumInstances <= 255U, "Event MaximumInstances must be in the V1 range 1..255");
        static_assert(Detail::AdmissionTraits<TAdmission>::IsValid, "Event admission must be Queue<N> or NewestOnly");
        static_assert(Detail::IsRetentionPolicyV<TRetention>, "Event retention must be UntilHandoffOnly or TimedRetention");
        static_assert(
            Detail::AdmissionTraits<TAdmission>::DedicatedPending <= TMaximumInstances,
            "Queue dedicated pending entitlement must not exceed MaximumInstances"
        );

        using Family = Event::Family;
        using Event = TEvent;
        using Admission = TAdmission;
        using Retention = TRetention;
        static constexpr std::size_t MaximumInstances = TMaximumInstances;
    };

    template<std::size_t TSlots>
    struct SharedPending final {
        using Family = Event::Family;
        static constexpr std::size_t Slots = TSlots;
    };

    template<class TThreadIdentity, class TEvent>
    requires EventType<TEvent>
    struct Observe final {
        using Family = Event::Family;
        using ThreadIdentity = TThreadIdentity;
        using Event = TEvent;
    };

} // ESPressio::Event
