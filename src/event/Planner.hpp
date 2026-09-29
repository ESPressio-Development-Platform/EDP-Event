#pragma once

#include <cstddef>
#include <type_traits>

#include <ESPressio_Primitives.hpp>
#include <ESPressio_System.hpp>

#include "Composition.hpp"
#include "Deployment.hpp"

namespace ESPressio::Event {

    namespace Detail {

        /// Compile-time predicate identifying non-Deploy declarations by default.
        /// @tparam TDeclaration Candidate Event-family declaration Type.
        template<class TDeclaration>
        struct IsDeploy : std::false_type {};


        /// Compile-time predicate specialization identifying Deploy declarations.
        /// @tparam TEvent Deployed Event payload Type.
        /// @tparam TMaximumInstances Exact live occurrence capacity.
        /// @tparam TAdmission Admission policy Type.
        /// @tparam TRetention Local retention capability Type.
        template<
            class TEvent,
            std::size_t TMaximumInstances,
            class TAdmission,
            class TRetention
        >
        struct IsDeploy<
            Deploy<TEvent, TMaximumInstances, TAdmission, TRetention>
        > : std::true_type {};


        /// Compile-time predicate identifying non-Observe declarations by default.
        /// @tparam TDeclaration Candidate Event-family declaration Type.
        template<class TDeclaration>
        struct IsObserve : std::false_type {};


        /// Compile-time predicate specialization identifying Observe declarations.
        /// @tparam TThreadIdentity Listener Dedicated Thread identity.
        /// @tparam TEvent Observed Event payload Type.
        template<class TThreadIdentity, class TEvent>
        struct IsObserve<
            Observe<TThreadIdentity, TEvent>
        > : std::true_type {};


        /// Compile-time predicate identifying non-SharedPending declarations by default.
        /// @tparam TDeclaration Candidate Event-family declaration Type.
        template<class TDeclaration>
        struct IsSharedPending : std::false_type {};


        /// Compile-time predicate specialization identifying SharedPending declarations.
        /// @tparam TSlots Exact family-wide shared pending entitlement.
        template<std::size_t TSlots>
        struct IsSharedPending<SharedPending<TSlots>> : std::true_type {};


        /// Resolves the zero-based ordinal of one Type within a compile-time TypeList.
        /// @tparam TList Compile-time TypeList being searched.
        /// @tparam TType Type whose ordinal is required.
        template<class TList, class TType>
        struct TypeOrdinal;


        /// Resolves zero when the requested Type is at the TypeList head.
        /// @tparam TType Requested Type and head Type.
        /// @tparam TRest Remaining TypeList members.
        template<class TType, class... TRest>
        struct TypeOrdinal<
            Primitives::TypeList<TType, TRest...>,
            TType
        > : std::integral_constant<std::size_t, 0U> {};


        /// Recursively advances through the TypeList until the requested Type becomes the head.
        /// @tparam TFirst Current nonmatching head Type.
        /// @tparam TRest Remaining TypeList members.
        /// @tparam TType Requested Type.
        template<class TFirst, class... TRest, class TType>
        struct TypeOrdinal<
            Primitives::TypeList<TFirst, TRest...>,
            TType
        > : std::integral_constant<
            std::size_t,
            1U + TypeOrdinal<
                Primitives::TypeList<TRest...>,
                TType
            >::value
        > {};


        /// Resolves the Type at one compile-time TypeList ordinal.
        /// @tparam TList Compile-time TypeList being indexed.
        /// @tparam TIndex Zero-based ordinal to resolve.
        template<class TList, std::size_t TIndex>
        struct TypeAt;


        /// Resolves the head Type for ordinal zero.
        /// @tparam TFirst Head Type returned by this specialization.
        /// @tparam TRest Remaining TypeList members.
        template<class TFirst, class... TRest>
        struct TypeAt<
            Primitives::TypeList<TFirst, TRest...>,
            0U
        > {

            /// Type stored at ordinal zero.
            using Type = TFirst;

        };


        /// Recursively resolves a positive TypeList ordinal.
        /// @tparam TFirst Current head Type skipped by this specialization.
        /// @tparam TRest Remaining TypeList members.
        /// @tparam TIndex Positive zero-based ordinal to resolve.
        template<class TFirst, class... TRest, std::size_t TIndex>
        struct TypeAt<
            Primitives::TypeList<TFirst, TRest...>,
            TIndex
        > {

            static_assert(
                TIndex < sizeof...(TRest) + 1U,
                "TypeList ordinal is out of range"
            );

            /// Type stored at the requested ordinal.
            using Type = typename TypeAt<
                Primitives::TypeList<TRest...>,
                TIndex - 1U
            >::Type;

        };


        /// Filters one compile-time TypeList by a unary compile-time predicate.
        /// @tparam TList Source TypeList.
        /// @tparam TPredicate Predicate exposing a Boolean `value` for each Type.
        template<class TList, template<class> class TPredicate>
        struct Filter;


        /// Terminates filtering for an empty TypeList.
        /// @tparam TPredicate Predicate retained only for Type identity.
        template<template<class> class TPredicate>
        struct Filter<
            Primitives::TypeList<>,
            TPredicate
        > {

            /// Empty filtered TypeList.
            using Type = Primitives::TypeList<>;

        };


        /// Recursively filters one nonempty TypeList while preserving declaration order.
        /// @tparam TFirst Current head Type.
        /// @tparam TRest Remaining source Types.
        /// @tparam TPredicate Predicate selecting retained Types.
        template<class TFirst, class... TRest, template<class> class TPredicate>
        struct Filter<
            Primitives::TypeList<TFirst, TRest...>,
            TPredicate
        > {

            /// Filtered result for the remaining source Types.
            using Tail = typename Filter<
                Primitives::TypeList<TRest...>,
                TPredicate
            >::Type;

            /// Filtered TypeList including TFirst only when the predicate selects it.
            using Type = std::conditional_t<
                TPredicate<TFirst>::value,
                typename Primitives::Detail::ConcatTypeLists<
                    Primitives::TypeList<TFirst>,
                    Tail
                >::Type,
                Tail
            >;

        };


        /// Maps one Observe declaration to its uniquely selected callback provider Type.
        /// @tparam TArchitecture Application Composition Architecture.
        /// @tparam TObservation Observe declaration being resolved.
        template<class TArchitecture, class TObservation>
        struct CallbackProviderForObservation;


        /// Observe specialization resolving Listener callback provider identity.
        /// @tparam TArchitecture Application Composition Architecture.
        /// @tparam TThreadIdentity Listener Dedicated Thread identity.
        /// @tparam TEvent Observed Event payload Type.
        template<class TArchitecture, class TThreadIdentity, class TEvent>
        struct CallbackProviderForObservation<
            TArchitecture,
            Observe<TThreadIdentity, TEvent>
        > {

            /// Unique callback provider selected for this Observe relation.
            using Type = Composition::ListenerCallbackProvider<
                TThreadIdentity,
                TEvent,
                TArchitecture
            >;

        };


        /// Derives the unique callback-provider TypeList required by all Observe declarations.
        /// @tparam TArchitecture Application Composition Architecture.
        /// @tparam TObservations Observe declaration TypeList.
        template<class TArchitecture, class TObservations>
        struct RequiredCallbackProviders;


        /// TypeList specialization resolving and deduplicating all callback provider Types.
        /// @tparam TArchitecture Application Composition Architecture.
        /// @tparam TObservations Observe declaration Types.
        template<class TArchitecture, class... TObservations>
        struct RequiredCallbackProviders<
            TArchitecture,
            Primitives::TypeList<TObservations...>
        > {

            /// Unique callback-provider TypeList in deterministic first-use order.
            using Type = typename Primitives::Detail::UniqueTypeList<
                Primitives::TypeList<
                    typename CallbackProviderForObservation<
                        TArchitecture,
                        TObservations
                    >::Type...
                >
            >::Type;

        };


        /// Extracts concrete Event payload Types from one Deploy declaration TypeList.
        /// @tparam TList Deploy declaration TypeList.
        template<class TList>
        struct DeployEvents;


        /// TypeList specialization mapping each Deploy declaration to its Event payload Type.
        /// @tparam TDeployments Deploy declaration Types.
        template<class... TDeployments>
        struct DeployEvents<Primitives::TypeList<TDeployments...>> {

            /// Locally deployed Event payload TypeList.
            using Type = Primitives::TypeList<typename TDeployments::Event...>;

        };


        /// Extracts Dedicated Thread identities from one Observe declaration TypeList.
        /// @tparam TList Observe declaration TypeList.
        template<class TList>
        struct ObserveListeners;


        /// TypeList specialization mapping each Observe declaration to its Thread identity.
        /// @tparam TObservations Observe declaration Types.
        template<class... TObservations>
        struct ObserveListeners<Primitives::TypeList<TObservations...>> {

            /// Listener Thread identity TypeList, including repeat identities across observed Types.
            using Type = Primitives::TypeList<typename TObservations::ThreadIdentity...>;

        };


        /// Counts Observe declarations targeting one Event Type.
        /// @tparam TList Observe declaration TypeList.
        /// @tparam TEvent Event payload Type being counted.
        template<class TList, class TEvent>
        struct CountObservedEvent;


        /// TypeList specialization folding one count across all Observe declarations.
        /// @tparam TEvent Event payload Type being counted.
        /// @tparam TObservations Observe declaration Types.
        template<class TEvent, class... TObservations>
        struct CountObservedEvent<
            Primitives::TypeList<TObservations...>,
            TEvent
        > : std::integral_constant<
            std::size_t,
            (
                0U + ... +
                (std::is_same_v<typename TObservations::Event, TEvent> ? 1U : 0U)
            )
        > {};


        /// Counts Observe declarations owned by one Listener Thread identity.
        /// @tparam TList Observe declaration TypeList.
        /// @tparam TThreadIdentity Listener Thread identity being counted.
        template<class TList, class TThreadIdentity>
        struct CountObservedThread;


        /// TypeList specialization folding one count across all Observe declarations.
        /// @tparam TThreadIdentity Listener Thread identity being counted.
        /// @tparam TObservations Observe declaration Types.
        template<class TThreadIdentity, class... TObservations>
        struct CountObservedThread<
            Primitives::TypeList<TObservations...>,
            TThreadIdentity
        > : std::integral_constant<
            std::size_t,
            (
                0U + ... +
                (std::is_same_v<typename TObservations::ThreadIdentity, TThreadIdentity> ? 1U : 0U)
            )
        > {};


        /// Derives the Event TypeList observed by one Listener Thread identity.
        /// @tparam TList Observe declaration TypeList.
        /// @tparam TThreadIdentity Listener Thread identity being resolved.
        template<class TList, class TThreadIdentity>
        struct ObservedTypesFor;


        /// Terminates observed-Type derivation for an empty Observe list.
        /// @tparam TThreadIdentity Listener Thread identity retained only for Type identity.
        template<class TThreadIdentity>
        struct ObservedTypesFor<
            Primitives::TypeList<>,
            TThreadIdentity
        > {

            /// Empty observed Event TypeList.
            using Type = Primitives::TypeList<>;

        };


        /// Recursively derives observed Event Types while preserving Observe declaration order.
        /// @tparam TFirst Current Observe declaration.
        /// @tparam TRest Remaining Observe declarations.
        /// @tparam TThreadIdentity Listener Thread identity being resolved.
        template<class TFirst, class... TRest, class TThreadIdentity>
        struct ObservedTypesFor<
            Primitives::TypeList<TFirst, TRest...>,
            TThreadIdentity
        > {

            /// Derived observed Types from remaining Observe declarations.
            using Tail = typename ObservedTypesFor<
                Primitives::TypeList<TRest...>,
                TThreadIdentity
            >::Type;

            /// Event TypeList observed by the requested Thread identity.
            using Type = std::conditional_t<
                std::is_same_v<typename TFirst::ThreadIdentity, TThreadIdentity>,
                typename Primitives::Detail::ConcatTypeLists<
                    Primitives::TypeList<typename TFirst::Event>,
                    Tail
                >::Type,
                Tail
            >;

        };


        /// Derives the Listener Thread TypeList eligible for one Event Type.
        /// @tparam TList Observe declaration TypeList.
        /// @tparam TEvent Event payload Type being resolved.
        template<class TList, class TEvent>
        struct ObserversFor;


        /// Terminates Listener derivation for an empty Observe list.
        /// @tparam TEvent Event payload Type retained only for Type identity.
        template<class TEvent>
        struct ObserversFor<
            Primitives::TypeList<>,
            TEvent
        > {

            /// Empty eligible Listener TypeList.
            using Type = Primitives::TypeList<>;

        };


        /// Recursively derives eligible Listener identities while preserving Observe order.
        /// @tparam TFirst Current Observe declaration.
        /// @tparam TRest Remaining Observe declarations.
        /// @tparam TEvent Event payload Type being resolved.
        template<class TFirst, class... TRest, class TEvent>
        struct ObserversFor<
            Primitives::TypeList<TFirst, TRest...>,
            TEvent
        > {

            /// Derived Listener identities from remaining Observe declarations.
            using Tail = typename ObserversFor<
                Primitives::TypeList<TRest...>,
                TEvent
            >::Type;

            /// Listener TypeList eligible for the requested Event Type.
            using Type = std::conditional_t<
                std::is_same_v<typename TFirst::Event, TEvent>,
                typename Primitives::Detail::ConcatTypeLists<
                    Primitives::TypeList<typename TFirst::ThreadIdentity>,
                    Tail
                >::Type,
                Tail
            >;

        };


        /// Resolves the unique local Deploy declaration for one Event Type.
        /// @tparam TList Deploy declaration TypeList.
        /// @tparam TEvent Event payload Type being resolved.
        template<class TList, class TEvent>
        struct FindDeployment;


        /// Returns void when no deployment remains in the search list.
        /// @tparam TEvent Event payload Type being resolved.
        template<class TEvent>
        struct FindDeployment<
            Primitives::TypeList<>,
            TEvent
        > {

            /// Sentinel Type representing an absent deployment.
            using Type = void;

        };


        /// Recursively resolves the deployment matching one Event Type.
        /// @tparam TFirst Current Deploy declaration.
        /// @tparam TRest Remaining Deploy declarations.
        /// @tparam TEvent Event payload Type being resolved.
        template<class TFirst, class... TRest, class TEvent>
        struct FindDeployment<
            Primitives::TypeList<TFirst, TRest...>,
            TEvent
        > {

            /// Matching deployment Type, or recursive result from remaining declarations.
            using Type = std::conditional_t<
                std::is_same_v<typename TFirst::Event, TEvent>,
                TFirst,
                typename FindDeployment<
                    Primitives::TypeList<TRest...>,
                    TEvent
                >::Type
            >;

        };


        /// Resolves the optional family-wide SharedPending capacity from its declaration list.
        /// @tparam TList Zero-or-one SharedPending declaration TypeList.
        template<class TList>
        struct SharedPendingValue;


        /// Maps absence of SharedPending declaration to zero capacity.
        template<>
        struct SharedPendingValue<Primitives::TypeList<>> :
            std::integral_constant<std::size_t, 0U> {};


        /// Maps one SharedPending declaration to its exact capacity.
        /// @tparam TSharedPending Sole SharedPending declaration Type.
        template<class TSharedPending>
        struct SharedPendingValue<Primitives::TypeList<TSharedPending>> :
            std::integral_constant<std::size_t, TSharedPending::Slots> {};


        /// Calculates the maximum family-wide SharedPending capacity that could ever be useful.
        /// @tparam TList Deploy declaration TypeList.
        template<class TList>
        struct MaximumUsefulShared;


        /// Sums per-Queue physical capacity beyond each Type's dedicated pending entitlement.
        /// @tparam TDeployments Deploy declaration Types.
        template<class... TDeployments>
        struct MaximumUsefulShared<Primitives::TypeList<TDeployments...>> :
            std::integral_constant<
                std::size_t,
                (
                    0U + ... +
                    (
                        AdmissionTraits<typename TDeployments::Admission>::IsQueue
                            ? (
                                TDeployments::MaximumInstances -
                                AdmissionTraits<typename TDeployments::Admission>::DedicatedPending
                            )
                            : 0U
                    )
                )
            > {};


        /// Validates that every Observe relation names a locally deployed Event Type.
        /// @tparam TList Observe declaration TypeList.
        template<class TList>
        struct ValidateObservedDeployed;


        /// TypeList specialization evaluating deployment membership for every Observe relation.
        /// @tparam TObservations Observe declaration Types.
        template<class... TObservations>
        struct ValidateObservedDeployed<Primitives::TypeList<TObservations...>> {

            /// Tests all observed Event Types against the local deployed Event TypeList.
            /// @tparam TDeployedEvents Local deployed Event TypeList.
            template<class TDeployedEvents>
            static consteval bool Against() {
                return (
                    TDeployedEvents::template Contains<typename TObservations::Event> &&
                    ...
                );
            }

        };


        /// Detects duplicate local deployment declarations by concrete Event Type.
        /// @tparam TList Deploy declaration TypeList.
        template<class TList>
        struct HasDuplicateEventDeployments;


        /// TypeList specialization performing duplicate Event Type detection.
        /// @tparam TDeployments Deploy declaration Types.
        template<class... TDeployments>
        struct HasDuplicateEventDeployments<Primitives::TypeList<TDeployments...>> {

            /// Event payload TypeList extracted from deployments.
            using Events = Primitives::TypeList<typename TDeployments::Event...>;

            /// Indicates whether any Event payload Type appears in more than one deployment.
            static constexpr bool Value = Primitives::Detail::HasDuplicateTypes<Events>::Value;

        };


        /// Sums exact MaximumInstances across all local deployments.
        /// @tparam TList Deploy declaration TypeList.
        template<class TList>
        struct MaxInstancesSum;


        /// TypeList specialization folding MaximumInstances across deployments.
        /// @tparam TDeployments Deploy declaration Types.
        template<class... TDeployments>
        struct MaxInstancesSum<Primitives::TypeList<TDeployments...>> :
            std::integral_constant<
                std::size_t,
                (0U + ... + TDeployments::MaximumInstances)
            > {};


        /// Resource-plan tag identifying physical occurrence instances for one Event Type.
        /// @tparam TEvent Event payload Type owning the occurrence capacity.
        template<class TEvent>
        struct OccurrenceInstances final {};


        /// Resource-plan tag identifying dedicated pending Queue entitlement for one Event Type.
        /// @tparam TEvent Event payload Type owning the pending entitlement.
        template<class TEvent>
        struct DedicatedPendingSlots final {};


        /// Resource-plan tag identifying eligible Listener count for one Event Type.
        /// @tparam TEvent Event payload Type whose Listener cardinality is represented.
        template<class TEvent>
        struct EligibleListeners final {};


        /// Resource-plan tag identifying family-wide logical SharedPending capacity.
        struct SharedPendingSlots final {};


        /// Resource-plan tag identifying the family-wide unique Listener count.
        struct ListenerCount final {};


        /// Builds the decomposed Event ResourcePlan from deployments, observations and SharedPending.
        /// @tparam TDeployList Deploy declaration TypeList.
        /// @tparam TObserveList Observe declaration TypeList.
        /// @tparam TShared Exact family-wide SharedPending capacity.
        template<class TDeployList, class TObserveList, std::size_t TShared>
        struct MakeResourcePlan;


        /// Deploy TypeList specialization expanding per-Type and family-wide resource dimensions.
        /// @tparam TDeployments Deploy declaration Types.
        /// @tparam TObserveList Observe declaration TypeList.
        /// @tparam TShared Exact family-wide SharedPending capacity.
        template<class... TDeployments, class TObserveList, std::size_t TShared>
        struct MakeResourcePlan<
            Primitives::TypeList<TDeployments...>,
            TObserveList,
            TShared
        > {

            /// Per-deployment physical occurrence capacity requirement.
            /// @tparam TDeployment Deploy declaration being represented.
            template<class TDeployment>
            using OccurrenceRequirement = Primitives::ResourceRequirement<
                OccurrenceInstances<typename TDeployment::Event>,
                TDeployment::MaximumInstances
            >;

            /// Per-deployment dedicated Queue pending entitlement requirement.
            /// @tparam TDeployment Deploy declaration being represented.
            template<class TDeployment>
            using PendingRequirement = Primitives::ResourceRequirement<
                DedicatedPendingSlots<typename TDeployment::Event>,
                AdmissionTraits<typename TDeployment::Admission>::DedicatedPending
            >;

            /// Per-deployment eligible Listener cardinality requirement.
            /// @tparam TDeployment Deploy declaration being represented.
            template<class TDeployment>
            using ListenerRequirement = Primitives::ResourceRequirement<
                EligibleListeners<typename TDeployment::Event>,
                CountObservedEvent<
                    TObserveList,
                    typename TDeployment::Event
                >::value
            >;

            /// Expands a deployment TypeList into one complete Primitives ResourcePlan.
            /// @tparam TList Deployment TypeList being expanded.
            template<class TList>
            struct Expand;

            /// TypeList specialization producing all decomposed requirements.
            /// @tparam TExpandedDeployments Deployment Types being expanded.
            template<class... TExpandedDeployments>
            struct Expand<Primitives::TypeList<TExpandedDeployments...>> {

                /// Complete Event family semantic ResourcePlan.
                using Type = Primitives::ResourcePlan<
                    OccurrenceRequirement<TExpandedDeployments>...,
                    PendingRequirement<TExpandedDeployments>...,
                    ListenerRequirement<TExpandedDeployments>...,
                    Primitives::ResourceRequirement<
                        SharedPendingSlots,
                        TShared
                    >,
                    Primitives::ResourceRequirement<
                        ListenerCount,
                        Primitives::Detail::UniqueTypeList<
                            typename ObserveListeners<TObserveList>::Type
                        >::Type::Count
                    >
                >;

            };

            /// Complete Event family semantic ResourcePlan for the supplied deployments.
            using Type = typename Expand<
                Primitives::TypeList<TDeployments...>
            >::Type;

        };


        /// Canonical normalized immutable Event-family plan consumed by Runtime and integration helpers.
        /// @tparam TDeclarations Complete Event family declaration TypeList supplied by EDP-Primitives.
        template<class TDeclarations>
        struct NormalizedEventPlan final {

            // Normalized declaration sets.

            /// Complete source Event-family declaration TypeList.
            using Declarations = TDeclarations;

            /// Filtered local Deploy declaration TypeList.
            using Deployments = typename Filter<TDeclarations, IsDeploy>::Type;

            /// Filtered Observe declaration TypeList.
            using Observations = typename Filter<TDeclarations, IsObserve>::Type;

            /// Zero-or-one SharedPending declaration TypeList.
            using SharedDeclarations = typename Filter<TDeclarations, IsSharedPending>::Type;

            /// Locally deployed concrete Event payload TypeList.
            using PrimitiveTypes = typename DeployEvents<Deployments>::Type;

            /// Listener Thread identities in Observe declaration order, including duplicates across Event Types.
            using ListenerDeclarations = typename ObserveListeners<Observations>::Type;

            /// Unique Listener Thread identities in deterministic first-declaration order.
            using Listeners = typename Primitives::Detail::UniqueTypeList<ListenerDeclarations>::Type;

            static_assert(
                !HasDuplicateEventDeployments<Deployments>::Value,
                "Event family may deploy each Event Type at most once"
            );

            static_assert(
                Primitives::Detail::UniquePrimitiveIdentifiers<PrimitiveTypes>::value,
                "Event family deployed Event Types must have distinct universal TypeIdentifier values"
            );

            static_assert(
                !Primitives::Detail::HasDuplicateTypes<Observations>::Value,
                "Event family may declare each (Listener, Event) observation at most once"
            );

            static_assert(
                SharedDeclarations::Count <= 1U,
                "Event family may declare SharedPending at most once"
            );

            static_assert(
                ValidateObservedDeployed<Observations>::template Against<PrimitiveTypes>(),
                "Observe requires the Event Type to be locally deployed"
            );

            // Family-wide pending resource dimensions.

            /// Exact configured family-wide SharedPending capacity, or zero when absent.
            static constexpr std::size_t SharedPendingCapacity = SharedPendingValue<SharedDeclarations>::value;

            static_assert(
                SharedDeclarations::Count == 0U || SharedPendingCapacity > 0U,
                "SharedPending<0> is redundant; omit the family-wide declaration when no shared entitlement is required"
            );

            /// Maximum Queue overflow entitlement that could ever be consumed by this topology.
            static constexpr std::size_t MaximumUsefulSharedPending = MaximumUsefulShared<Deployments>::value;

            static_assert(
                SharedPendingCapacity <= MaximumUsefulSharedPending,
                "SharedPending exceeds the maximum useful Queue overflow entitlement"
            );

            /// Decomposed semantic Event resource plan exported through EDP-Primitives FamilyPlan.
            using Resources = typename MakeResourcePlan<
                Deployments,
                Observations,
                SharedPendingCapacity
            >::Type;

            // Per-Type deployment/Listener queries.

            /// Indicates whether one concrete Event Type is locally deployed.
            /// @tparam TEvent Candidate Event payload Type.
            template<class TEvent>
            static constexpr bool IsDeployed = PrimitiveTypes::template Contains<TEvent>;

            /// Resolves the local deployment declaration for one Event Type.
            /// @tparam TEvent Locally deployed Event payload Type.
            template<class TEvent>
            using Deployment = typename FindDeployment<Deployments, TEvent>::Type;

            /// Exact Type-local eligible Listener count.
            /// @tparam TEvent Locally deployed Event payload Type.
            template<class TEvent>
            static constexpr std::size_t EligibleListenerCount = CountObservedEvent<Observations, TEvent>::value;

            /// TypeList of Listener Thread identities eligible for one Event Type.
            /// @tparam TEvent Locally deployed Event payload Type.
            template<class TEvent>
            using EligibleListenerTypes = typename ObserversFor<Observations, TEvent>::Type;

            /// TypeList of Event Types observed by one Listener Thread identity.
            /// @tparam TThreadIdentity Listener Dedicated Thread identity.
            template<class TThreadIdentity>
            using ObservedEventTypes = typename ObservedTypesFor<Observations, TThreadIdentity>::Type;

            /// Dense Type-local Listener ordinal used by recipient/subscription bitsets.
            /// @tparam TEvent Locally deployed Event payload Type.
            /// @tparam TThreadIdentity Eligible Listener Thread identity.
            template<class TEvent, class TThreadIdentity>
            static constexpr std::size_t ListenerOrdinal = TypeOrdinal<
                EligibleListenerTypes<TEvent>,
                TThreadIdentity
            >::value;

            /// Dense family-wide Listener ordinal used only for structural normalization.
            /// @tparam TThreadIdentity Unique Listener Thread identity.
            template<class TThreadIdentity>
            static constexpr std::size_t ListenerGlobalOrdinal = TypeOrdinal<
                Listeners,
                TThreadIdentity
            >::value;

            /// Original Observe declaration ordinal used to bind its typed callback provider deterministically.
            /// @tparam TThreadIdentity Listener Thread identity.
            /// @tparam TEvent Observed Event payload Type.
            template<class TThreadIdentity, class TEvent>
            static constexpr std::size_t ObservationOrdinal = TypeOrdinal<
                Observations,
                Observe<TThreadIdentity, TEvent>
            >::value;

            /// Indicates whether one Event deployment uses FIFO Queue admission.
            /// @tparam TEvent Locally deployed Event payload Type.
            template<class TEvent>
            static constexpr bool IsQueue = AdmissionTraits<
                typename Deployment<TEvent>::Admission
            >::IsQueue;

            /// Exact Type-local dedicated pending Queue entitlement.
            /// @tparam TEvent Locally deployed Event payload Type.
            template<class TEvent>
            static constexpr std::size_t DedicatedPendingCapacity = AdmissionTraits<
                typename Deployment<TEvent>::Admission
            >::DedicatedPending;

            /// Indicates whether one Event deployment supports timed local retention.
            /// @tparam TEvent Locally deployed Event payload Type.
            template<class TEvent>
            static constexpr bool SupportsTimedRetention = std::is_same_v<
                typename Deployment<TEvent>::Retention,
                TimedRetention
            >;

        };

    } // ESPressio::Event::Detail


    /// Resolves the normalized Event-family runtime plan from one complete Primitive topology.
    /// Applications use this public alias instead of reaching into Primitives planner internals.
    /// @tparam TTopology Complete immutable Primitives topology containing Event family declarations.
    template<class TTopology>
    using PlanFor = typename Primitives::Detail::InvokeFamilyPlanner<
        Family,
        typename TTopology::Deployments
    >::Type::RuntimeProvider::EventPlan;


    /// Event family runtime-provider marker exported through EDP-Primitives FamilyPlan.
    /// @tparam TEventPlan Canonical normalized Event plan carried by this provider Type.
    template<class TEventPlan>
    class RuntimeProvider : public System::CompositionFramework::Provider<
        Primitives::Composition::Domain,
        System::CompositionFramework::Offers<
            System::CompositionFramework::Offer<
                Primitives::Composition::FamilyRuntime<Family>
            >
        >
    > {

        public:

            /// Canonical normalized Event plan carried by this runtime-provider marker.
            using EventPlan = TEventPlan;

    };


    /// Canonical Event family planner invoked by EDP-Primitives.
    struct Planner final {

        /// Constructs one canonical Primitives FamilyPlan from Event family declarations.
        /// @tparam TFamily Primitive family being planned; must be Event::Family.
        /// @tparam TDeclarations Complete Event-family declaration TypeList.
        template<class TFamily, class TDeclarations>
        struct MakePlan {

            static_assert(
                std::is_same_v<TFamily, Family>,
                "Event Planner may only plan Event::Family"
            );

            /// Canonical normalized Event runtime plan.
            using EventPlan = Detail::NormalizedEventPlan<TDeclarations>;

            /// Canonical EDP-Primitives FamilyPlan exported for Event::Family.
            using Type = Primitives::FamilyPlan<
                Family,
                RuntimeProvider<EventPlan>,
                typename EventPlan::PrimitiveTypes,
                typename EventPlan::Resources
            >;

        };

        /// Canonical Event family plan alias consumed by EDP-Primitives family-planner dispatch.
        /// @tparam TFamily Primitive family being planned; must be Event::Family.
        /// @tparam TDeclarations Complete Event-family declaration TypeList.
        template<class TFamily, class TDeclarations>
        using Plan = typename MakePlan<TFamily, TDeclarations>::Type;

    };

} // ESPressio::Event
