#pragma once

#include <cstddef>
#include <type_traits>

#include <ESPressio_Primitives.hpp>
#include <ESPressio_System.hpp>

#include "Deployment.hpp"

namespace ESPressio::Event {

    namespace Detail {

        template<class T> struct IsDeploy : std::false_type {};
        template<class E, std::size_t N, class A, class R>
        struct IsDeploy<Deploy<E, N, A, R>> : std::true_type {};

        template<class T> struct IsObserve : std::false_type {};
        template<class L, class E>
        struct IsObserve<Observe<L, E>> : std::true_type {};

        template<class T> struct IsSharedPending : std::false_type {};
        template<std::size_t N>
        struct IsSharedPending<SharedPending<N>> : std::true_type {};


        template<class TList, class TType> struct TypeOrdinal;
        template<class TType, class... TRest>
        struct TypeOrdinal<Primitives::TypeList<TType, TRest...>, TType> : std::integral_constant<std::size_t, 0U> {};
        template<class TFirst, class... TRest, class TType>
        struct TypeOrdinal<Primitives::TypeList<TFirst, TRest...>, TType> : std::integral_constant<
            std::size_t,
            1U + TypeOrdinal<Primitives::TypeList<TRest...>, TType>::value
        > {};

        template<class TList, std::size_t TIndex> struct TypeAt;
        template<class TFirst, class... TRest>
        struct TypeAt<Primitives::TypeList<TFirst, TRest...>, 0U> { using Type = TFirst; };
        template<class TFirst, class... TRest, std::size_t TIndex>
        struct TypeAt<Primitives::TypeList<TFirst, TRest...>, TIndex> {
            static_assert(TIndex < sizeof...(TRest) + 1U, "TypeList ordinal is out of range");
            using Type = typename TypeAt<Primitives::TypeList<TRest...>, TIndex - 1U>::Type;
        };

        template<class TList, template<class> class TPredicate>
        struct Filter;

        template<template<class> class P>
        struct Filter<Primitives::TypeList<>, P> { using Type = Primitives::TypeList<>; };

        template<class First, class... Rest, template<class> class P>
        struct Filter<Primitives::TypeList<First, Rest...>, P> {
            using Tail = typename Filter<Primitives::TypeList<Rest...>, P>::Type;
            using Type = std::conditional_t<
                P<First>::value,
                typename Primitives::Detail::ConcatTypeLists<Primitives::TypeList<First>, Tail>::Type,
                Tail
            >;
        };

        template<class TList> struct DeployEvents;
        template<class... D>
        struct DeployEvents<Primitives::TypeList<D...>> {
            using Type = Primitives::TypeList<typename D::Event...>;
        };

        template<class TList> struct ObserveListeners;
        template<class... O>
        struct ObserveListeners<Primitives::TypeList<O...>> {
            using Type = Primitives::TypeList<typename O::ThreadIdentity...>;
        };

        template<class TList, class TEvent> struct CountObservedEvent;
        template<class TEvent, class... O>
        struct CountObservedEvent<Primitives::TypeList<O...>, TEvent> : std::integral_constant<
            std::size_t,
            (0U + ... + (std::is_same_v<typename O::Event, TEvent> ? 1U : 0U))
        > {};

        template<class TList, class TThread> struct CountObservedThread;
        template<class TThread, class... O>
        struct CountObservedThread<Primitives::TypeList<O...>, TThread> : std::integral_constant<
            std::size_t,
            (0U + ... + (std::is_same_v<typename O::ThreadIdentity, TThread> ? 1U : 0U))
        > {};

        template<class TList, class TThread> struct ObservedTypesFor;
        template<class TThread>
        struct ObservedTypesFor<Primitives::TypeList<>, TThread> { using Type = Primitives::TypeList<>; };
        template<class First, class... Rest, class TThread>
        struct ObservedTypesFor<Primitives::TypeList<First, Rest...>, TThread> {
            using Tail = typename ObservedTypesFor<Primitives::TypeList<Rest...>, TThread>::Type;
            using Type = std::conditional_t<
                std::is_same_v<typename First::ThreadIdentity, TThread>,
                typename Primitives::Detail::ConcatTypeLists<Primitives::TypeList<typename First::Event>, Tail>::Type,
                Tail
            >;
        };

        template<class TList, class TEvent> struct ObserversFor;
        template<class TEvent>
        struct ObserversFor<Primitives::TypeList<>, TEvent> { using Type = Primitives::TypeList<>; };
        template<class First, class... Rest, class TEvent>
        struct ObserversFor<Primitives::TypeList<First, Rest...>, TEvent> {
            using Tail = typename ObserversFor<Primitives::TypeList<Rest...>, TEvent>::Type;
            using Type = std::conditional_t<
                std::is_same_v<typename First::Event, TEvent>,
                typename Primitives::Detail::ConcatTypeLists<Primitives::TypeList<typename First::ThreadIdentity>, Tail>::Type,
                Tail
            >;
        };

        template<class TList, class TEvent> struct FindDeployment;
        template<class TEvent>
        struct FindDeployment<Primitives::TypeList<>, TEvent> { using Type = void; };
        template<class First, class... Rest, class TEvent>
        struct FindDeployment<Primitives::TypeList<First, Rest...>, TEvent> {
            using Type = std::conditional_t<
                std::is_same_v<typename First::Event, TEvent>,
                First,
                typename FindDeployment<Primitives::TypeList<Rest...>, TEvent>::Type
            >;
        };

        template<class TList> struct SharedPendingValue;
        template<> struct SharedPendingValue<Primitives::TypeList<>> : std::integral_constant<std::size_t, 0U> {};
        template<class First> struct SharedPendingValue<Primitives::TypeList<First>> : std::integral_constant<std::size_t, First::Slots> {};

        template<class TList> struct MaximumUsefulShared;
        template<class... D>
        struct MaximumUsefulShared<Primitives::TypeList<D...>> : std::integral_constant<
            std::size_t,
            (0U + ... + (
                Detail::AdmissionTraits<typename D::Admission>::IsQueue
                    ? (D::MaximumInstances - Detail::AdmissionTraits<typename D::Admission>::DedicatedPending)
                    : 0U
            ))
        > {};

        template<class TList> struct ValidateObservedDeployed;
        template<class... O>
        struct ValidateObservedDeployed<Primitives::TypeList<O...>> {
            template<class TDeployedEvents>
            static consteval bool Against() {
                return (TDeployedEvents::template Contains<typename O::Event> && ...);
            }
        };

        template<class TList> struct HasDuplicateEventDeployments;
        template<class... D>
        struct HasDuplicateEventDeployments<Primitives::TypeList<D...>> {
            using Events = Primitives::TypeList<typename D::Event...>;
            static constexpr bool Value = Primitives::Detail::HasDuplicateTypes<Events>::Value;
        };

        template<class TList> struct MaxInstancesSum;
        template<class... D>
        struct MaxInstancesSum<Primitives::TypeList<D...>> : std::integral_constant<std::size_t, (0U + ... + D::MaximumInstances)> {};

        template<class TEvent> struct OccurrenceInstances final {};
        template<class TEvent> struct DedicatedPendingSlots final {};
        template<class TEvent> struct EligibleListeners final {};
        struct SharedPendingSlots final {};
        struct ListenerCount final {};

        template<class TDeployList, class TObserveList, std::size_t TShared>
        struct MakeResourcePlan;

        template<class... D, class TObserveList, std::size_t TShared>
        struct MakeResourcePlan<Primitives::TypeList<D...>, TObserveList, TShared> {
            template<class X>
            using Occ = Primitives::ResourceRequirement<OccurrenceInstances<typename X::Event>, X::MaximumInstances>;
            template<class X>
            using Pending = Primitives::ResourceRequirement<
                DedicatedPendingSlots<typename X::Event>,
                AdmissionTraits<typename X::Admission>::DedicatedPending
            >;
            template<class X>
            using Listeners = Primitives::ResourceRequirement<
                EligibleListeners<typename X::Event>,
                CountObservedEvent<TObserveList, typename X::Event>::value
            >;

            template<class TList> struct Expand;
            template<class... X>
            struct Expand<Primitives::TypeList<X...>> {
                using Type = Primitives::ResourcePlan<
                    Occ<X>..., Pending<X>..., Listeners<X>...,
                    Primitives::ResourceRequirement<SharedPendingSlots, TShared>,
                    Primitives::ResourceRequirement<ListenerCount, Primitives::Detail::UniqueTypeList<typename ObserveListeners<TObserveList>::Type>::Type::Count>
                >;
            };
            using Type = typename Expand<Primitives::TypeList<D...>>::Type;
        };

        template<class TDeclarations>
        struct NormalizedEventPlan final {
            using Declarations = TDeclarations;
            using Deployments = typename Filter<TDeclarations, IsDeploy>::Type;
            using Observations = typename Filter<TDeclarations, IsObserve>::Type;
            using SharedDeclarations = typename Filter<TDeclarations, IsSharedPending>::Type;
            using PrimitiveTypes = typename DeployEvents<Deployments>::Type;
            using ListenerDeclarations = typename ObserveListeners<Observations>::Type;
            using Listeners = typename Primitives::Detail::UniqueTypeList<ListenerDeclarations>::Type;

            static_assert(!HasDuplicateEventDeployments<Deployments>::Value, "Event family may deploy each Event Type at most once");
            static_assert(!Primitives::Detail::HasDuplicateTypes<Observations>::Value, "Event family may declare each (Listener, Event) observation at most once");
            static_assert(SharedDeclarations::Count <= 1U, "Event family may declare SharedPending at most once");
            static_assert(ValidateObservedDeployed<Observations>::template Against<PrimitiveTypes>(), "Observe requires the Event Type to be locally deployed");

            static constexpr std::size_t SharedPendingCapacity = SharedPendingValue<SharedDeclarations>::value;
            static constexpr std::size_t MaximumUsefulSharedPending = MaximumUsefulShared<Deployments>::value;
            static_assert(SharedPendingCapacity <= MaximumUsefulSharedPending, "SharedPending exceeds the maximum useful Queue overflow entitlement");

            using Resources = typename MakeResourcePlan<Deployments, Observations, SharedPendingCapacity>::Type;

            template<class TEvent>
            static constexpr bool IsDeployed = PrimitiveTypes::template Contains<TEvent>;

            template<class TEvent>
            using Deployment = typename FindDeployment<Deployments, TEvent>::Type;

            template<class TEvent>
            static constexpr std::size_t EligibleListenerCount = CountObservedEvent<Observations, TEvent>::value;

            template<class TEvent>
            using EligibleListenerTypes = typename ObserversFor<Observations, TEvent>::Type;

            template<class TThread>
            using ObservedEventTypes = typename ObservedTypesFor<Observations, TThread>::Type;

            template<class TEvent, class TThread>
            static constexpr std::size_t ListenerOrdinal = TypeOrdinal<EligibleListenerTypes<TEvent>, TThread>::value;

            template<class TThread>
            static constexpr std::size_t ListenerGlobalOrdinal = TypeOrdinal<Listeners, TThread>::value;

            template<class TThread, class TEvent>
            static constexpr std::size_t ObservationOrdinal = TypeOrdinal<
                Observations,
                Observe<TThread, TEvent>
            >::value;

            template<class TEvent>
            static constexpr bool IsQueue = AdmissionTraits<typename Deployment<TEvent>::Admission>::IsQueue;

            template<class TEvent>
            static constexpr std::size_t DedicatedPendingCapacity = AdmissionTraits<typename Deployment<TEvent>::Admission>::DedicatedPending;

            template<class TEvent>
            static constexpr bool SupportsTimedRetention = std::is_same_v<typename Deployment<TEvent>::Retention, TimedRetention>;
        };

    } // Event::Detail

    template<class TEventPlan>
    class RuntimeProvider : public System::CompositionFramework::Provider<
        Primitives::Composition::Domain,
        System::CompositionFramework::Offers<
            System::CompositionFramework::Offer<Primitives::Composition::FamilyRuntime<Family>>
        >
    > {
    public:
        using EventPlan = TEventPlan;
    };

    struct Planner final {
        template<class TFamily, class TDeclarations>
        struct MakePlan {
            static_assert(std::is_same_v<TFamily, Family>, "Event Planner may only plan Event::Family");
            using EventPlan = Detail::NormalizedEventPlan<TDeclarations>;
            using Type = Primitives::FamilyPlan<
                Family,
                RuntimeProvider<EventPlan>,
                typename EventPlan::PrimitiveTypes,
                typename EventPlan::Resources
            >;
        };

        template<class TFamily, class TDeclarations>
        using Plan = typename MakePlan<TFamily, TDeclarations>::Type;
    };

} // ESPressio::Event
