#include <cstdint>
#include <type_traits>

#include <ESPressio_Event.hpp>
#include "../support/EventTestSupport.hpp"

namespace LayoutTest {

    namespace Event = ESPressio::Event;
    namespace Primitives = ESPressio::Primitives;
    namespace BoundedTopology = ESPressio::BoundedTopology;

    using BareEvent = EventTestSupport::EventValue<10U>;
    using BareTopology = Primitives::Topology<
        Event::Deploy<BareEvent, 2U, Event::NewestOnly, Event::UntilHandoffOnly>
    >;
    using BareFamilyPlan = typename Primitives::Detail::InvokeFamilyPlanner<
        Event::Family,
        typename BareTopology::Deployments
    >::Type;
    using BarePlan = typename BareFamilyPlan::RuntimeProvider::EventPlan;
    using BareRecord = Event::OccurrenceRecord<BarePlan, BareEvent>;

    static_assert(sizeof(typename BareRecord::OccurrenceIndex) == 1U);
    static_assert(BareRecord::ListenerSet::StorageBytes == 0U);
    static_assert(std::is_empty_v<Event::Detail::ExpiryStorage<false>>);
    static_assert(std::is_empty_v<Event::Detail::BorrowStorage<0U>>);
    static_assert(std::is_empty_v<Event::Detail::QueueLinkStorage<typename BareRecord::OccurrenceIndex, false>>);
    static_assert(sizeof(BareRecord) == sizeof(BareEvent),
        "Untimed zero-listener NewestOnly occurrence must retain no bookkeeping bytes beyond the Event payload");

    using QueueEvent = EventTestSupport::EventValue<11U>;
    using QueueTopology = Primitives::Topology<
        Event::Deploy<QueueEvent, 255U, Event::Queue<1U>, Event::TimedRetention>,
        Event::Observe<EventTestSupport::ListenerA, QueueEvent>
    >;
    using QueueFamilyPlan = typename Primitives::Detail::InvokeFamilyPlanner<
        Event::Family,
        typename QueueTopology::Deployments
    >::Type;
    using QueuePlan = typename QueueFamilyPlan::RuntimeProvider::EventPlan;
    using QueueRecord = Event::OccurrenceRecord<QueuePlan, QueueEvent>;

    static_assert(sizeof(typename QueueRecord::OccurrenceIndex) == 1U,
        "V1 MaximumInstances <=255 must retain a one-byte physical occurrence identity");
    static_assert(QueueRecord::ListenerSet::StorageBytes == 1U);
    static_assert(sizeof(Event::Detail::ExpiryStorage<true>) == sizeof(Event::MonotonicTimestamp));
    static_assert(sizeof(Event::Detail::BorrowStorage<1U>) == 1U);
    static_assert(sizeof(Event::Detail::QueueLinkStorage<typename QueueRecord::OccurrenceIndex, true>) == 1U);
    static_assert(std::is_empty_v<Event::Detail::SharedPendingCounter<0U>>);
    static_assert(std::is_empty_v<Event::Detail::ListenerCursor<QueuePlan, EventTestSupport::ListenerA>>,
        "A Listener observing one Event Type must retain no round-robin cursor state");

    using TwoListenerSet = BoundedTopology::BoundedIndexSet<
        Event::Detail::ListenerIndexSpace<QueuePlan, QueueEvent>,
        2U
    >;
    static_assert(TwoListenerSet::StorageBytes == 1U);

} // LayoutTest

int main() { return 0; }
