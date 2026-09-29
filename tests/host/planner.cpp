#include <type_traits>
#include <ESPressio_Event.hpp>

namespace Test {

    struct ListenerA {};
    struct ListenerB {};

    struct EventA final {
        /// Stable Primitive Type identity used by this test Event.
        inline static constexpr ESPressio::System::TypeIdentifier Identifier{
            ESPressio::System::TypeIdentifier::Storage{0x00,0x00,0x01,0x00,0x00,0x00,0x10,0x01}
        };
        /// Primitive family binding proving this payload is an Event.
        using Family = ESPressio::Event::Family;
        /// Test payload value used to verify delivery semantics.
        int Value{};
    };

    struct EventB final {
        /// Stable Primitive Type identity used by this test Event.
        inline static constexpr ESPressio::System::TypeIdentifier Identifier{
            ESPressio::System::TypeIdentifier::Storage{0x00,0x00,0x01,0x00,0x00,0x00,0x10,0x02}
        };
        /// Primitive family binding proving this payload is an Event.
        using Family = ESPressio::Event::Family;
        /// Test payload value used to verify delivery semantics.
        std::uint16_t Value{};
    };

} // Test

namespace ESPressio::Bounded {

    template<> struct MemoryBoundedTraits<Test::EventA> : MemoryBoundedValueDeclaration<false, int> {};
    template<> struct MemoryBoundedTraits<Test::EventB> : MemoryBoundedValueDeclaration<false, std::uint16_t> {};

} // ESPressio::Bounded

using Topology = ESPressio::Primitives::Topology<
    ESPressio::Event::Deploy<Test::EventA, 4U, ESPressio::Event::Queue<2U>, ESPressio::Event::TimedRetention>,
    ESPressio::Event::Deploy<Test::EventB, 2U, ESPressio::Event::NewestOnly, ESPressio::Event::UntilHandoffOnly>,
    ESPressio::Event::Observe<Test::ListenerA, Test::EventA>,
    ESPressio::Event::Observe<Test::ListenerB, Test::EventA>,
    ESPressio::Event::Observe<Test::ListenerA, Test::EventB>,
    ESPressio::Event::SharedPending<2U>
>;

using Plan = ESPressio::Event::PlanFor<Topology>;

static_assert(Topology::PrimitiveTypes::Count == 2U);
static_assert(Plan::Listeners::Count == 2U);
static_assert(Plan::template EligibleListenerCount<Test::EventA> == 2U);
static_assert(Plan::template EligibleListenerCount<Test::EventB> == 1U);
static_assert(Plan::SharedPendingCapacity == 2U);
static_assert(std::is_same_v<typename Plan::template Deployment<Test::EventA>::Admission, ESPressio::Event::Queue<2U>>);

int main() { return 0; }
