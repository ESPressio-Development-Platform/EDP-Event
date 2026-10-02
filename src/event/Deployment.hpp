#pragma once

#include <cstddef>
#include <type_traits>

#include <bounded/MemoryBoundedTraits.hpp>
#include <ESPressio_Primitives.hpp>

#include "EventFamily.hpp"

namespace ESPressio::Event {

    /// Identifies a serialisable concrete Primitive payload which can be retained safely as an Event occurrence.
    /// `Primitives::PrimitiveType` supplies the universal System schema and Serialisation qualification; Event adds only
    /// bounded retained-memory, self-contained lifetime, family, and nothrow-destruction constraints.
    /// @tparam TEvent Candidate Event payload Type.
    template<class TEvent>
    concept EventType =
        Primitives::PrimitiveType<TEvent> &&
        std::is_same_v<Primitives::PrimitiveFamilyOf<TEvent>, Family> &&
        Bounded::IsMemoryBoundedValue<TEvent> &&
        !Bounded::MemoryBoundedTraits<std::remove_cv_t<TEvent>>::HasExternalLifetimeDependencies &&
        std::is_nothrow_destructible_v<TEvent>;


    /// Declares FIFO Queue admission and the exact dedicated pending entitlement for one Event Type.
    /// @tparam TDedicatedPending Exact number of pending Queue occurrences entitled without shared overflow.
    template<std::size_t TDedicatedPending>
    struct Queue final {

        // Queue resource policy.

        /// Exact Type-local dedicated pending entitlement.
        static constexpr std::size_t DedicatedPending = TDedicatedPending;

    };


    /// Declares latest-value admission with at most one uncommitted pending occurrence.
    struct NewestOnly final {};


    /// Declares that local retained occurrences never require a timed deadline.
    struct UntilHandoffOnly final {};


    /// Declares that local retained occurrences may use timed retention.
    struct TimedRetention final {};


    namespace Detail {

        /// Describes unsupported admission policy by default.
        /// @tparam TAdmission Candidate admission policy Type.
        template<class TAdmission>
        struct AdmissionTraits final {

            // Admission classification.

            /// Indicates whether the policy is one of the supported Event admission policies.
            static constexpr bool IsValid = false;

            /// Indicates whether the policy requires FIFO Queue topology.
            static constexpr bool IsQueue = false;

            /// Exact dedicated pending entitlement exposed by the policy.
            static constexpr std::size_t DedicatedPending = 0U;

        };


        /// Describes Queue admission policy.
        /// @tparam TCapacity Exact dedicated pending entitlement carried by Queue.
        template<std::size_t TCapacity>
        struct AdmissionTraits<Queue<TCapacity>> final {

            // Admission classification.

            /// Queue is a supported admission policy.
            static constexpr bool IsValid = true;

            /// Queue requires FIFO Queue topology.
            static constexpr bool IsQueue = true;

            /// Exact Queue dedicated pending entitlement.
            static constexpr std::size_t DedicatedPending = TCapacity;

        };


        /// Describes NewestOnly admission policy.
        template<>
        struct AdmissionTraits<NewestOnly> final {

            // Admission classification.

            /// NewestOnly is a supported admission policy.
            static constexpr bool IsValid = true;

            /// NewestOnly has no FIFO Queue topology.
            static constexpr bool IsQueue = false;

            /// NewestOnly has no Queue pending entitlement.
            static constexpr std::size_t DedicatedPending = 0U;

        };


        /// Indicates whether one Type is a supported local retention capability declaration.
        /// @tparam TRetention Candidate retention capability Type.
        template<class TRetention>
        inline constexpr bool IsRetentionPolicyV =
            std::is_same_v<TRetention, UntilHandoffOnly> ||
            std::is_same_v<TRetention, TimedRetention>;

    } // ESPressio::Event::Detail


    /// Declares one locally deployed Event Type and its hard resource/policy choices.
    /// @tparam TEvent Concrete retained Event payload Type.
    /// @tparam TMaximumInstances Exact simultaneous live occurrence capacity in the V1 range 1..255.
    /// @tparam TAdmission Queue<N> or NewestOnly admission policy.
    /// @tparam TRetention UntilHandoffOnly or TimedRetention local retention capability.
    template<
        class TEvent,
        std::size_t TMaximumInstances,
        class TAdmission,
        class TRetention
    >
    struct Deploy final {

        static_assert(
            EventType<TEvent>,
            "Deploy requires a bounded, self-contained Event Type"
        );

        static_assert(
            TMaximumInstances > 0U && TMaximumInstances <= 255U,
            "Event MaximumInstances must be in the V1 range 1..255"
        );

        static_assert(
            Detail::AdmissionTraits<TAdmission>::IsValid,
            "Event admission must be Queue<N> or NewestOnly"
        );

        static_assert(
            Detail::IsRetentionPolicyV<TRetention>,
            "Event retention must be UntilHandoffOnly or TimedRetention"
        );

        static_assert(
            Detail::AdmissionTraits<TAdmission>::DedicatedPending <= TMaximumInstances,
            "Queue dedicated pending entitlement must not exceed MaximumInstances"
        );

        // Family and deployment metadata.

        /// Primitive family owning this declaration.
        using Family = Event::Family;

        /// Locally deployed Event payload Type.
        using Event = TEvent;

        /// Selected admission policy Type.
        using Admission = TAdmission;

        /// Selected local retention capability Type.
        using Retention = TRetention;

        /// Exact simultaneous live occurrence capacity.
        static constexpr std::size_t MaximumInstances = TMaximumInstances;

    };


    /// Declares the one family-wide logical shared pending overflow entitlement.
    /// @tparam TSlots Exact number of fungible shared pending Queue slots.
    template<std::size_t TSlots>
    struct SharedPending final {

        // Family and resource metadata.

        /// Primitive family owning this declaration.
        using Family = Event::Family;

        /// Exact family-wide shared pending entitlement.
        static constexpr std::size_t Slots = TSlots;

    };


    /// Declares immutable Listener eligibility for one Event Type.
    /// @tparam TThreadIdentity Dedicated Thread identity serving as the Listener endpoint.
    /// @tparam TEvent Locally deployed Event Type eligible for delivery to the Listener.
    template<class TThreadIdentity, class TEvent>
    requires EventType<TEvent>
    struct Observe final {

        // Family and relationship metadata.

        /// Primitive family owning this declaration.
        using Family = Event::Family;

        /// Dedicated Thread identity serving as this Listener endpoint.
        using ThreadIdentity = TThreadIdentity;

        /// Event payload Type this Listener is statically eligible to receive.
        using Event = TEvent;

    };

} // ESPressio::Event
