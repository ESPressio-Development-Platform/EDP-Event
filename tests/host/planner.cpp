#include <type_traits>
#include <ESPressio_Event.hpp>

namespace Test {
    struct ListenerA {};
    struct ListenerB {};

    struct EventA final {
        inline static constexpr ESPressio::System::TypeIdentifier Identifier{
            ESPressio::System::TypeIdentifier::Storage{0x00,0x00,0x01,0x00,0x00,0x00,0x10,0x01}
        };
        using Family = ESPressio::Event::Family;
        int Value{};
    };

    struct EventB final {
        inline static constexpr ESPressio::System::TypeIdentifier Identifier{
            ESPressio::System::TypeIdentifier::Storage{0x00,0x00,0x01,0x00,0x00,0x00,0x10,0x02}
        };
        using Family = ESPressio::Event::Family;
        std::uint16_t Value{};
    };
}

namespace ESPressio::Bounded {
    template<> struct MemoryBoundedTraits<Test::EventA> : MemoryBoundedValueDeclaration<false, int> {};
    template<> struct MemoryBoundedTraits<Test::EventB> : MemoryBoundedValueDeclaration<false, std::uint16_t> {};
}

using Topology = ESPressio::Primitives::Topology<
    ESPressio::Event::Deploy<Test::EventA, 4U, ESPressio::Event::Queue<2U>, ESPressio::Event::TimedRetention>,
    ESPressio::Event::Deploy<Test::EventB, 2U, ESPressio::Event::NewestOnly, ESPressio::Event::UntilHandoffOnly>,
    ESPressio::Event::Observe<Test::ListenerA, Test::EventA>,
    ESPressio::Event::Observe<Test::ListenerB, Test::EventA>,
    ESPressio::Event::Observe<Test::ListenerA, Test::EventB>,
    ESPressio::Event::SharedPending<2U>
>;

using FamilyPlan = typename ESPressio::Primitives::Detail::InvokeFamilyPlanner<
    ESPressio::Event::Family,
    typename Topology::Deployments
>::Type;
using Plan = typename FamilyPlan::RuntimeProvider::EventPlan;

static_assert(Topology::PrimitiveTypes::Count == 2U);
static_assert(Plan::Listeners::Count == 2U);
static_assert(Plan::template EligibleListenerCount<Test::EventA> == 2U);
static_assert(Plan::template EligibleListenerCount<Test::EventB> == 1U);
static_assert(Plan::SharedPendingCapacity == 2U);
static_assert(std::is_same_v<typename Plan::template Deployment<Test::EventA>::Admission, ESPressio::Event::Queue<2U>>);

int main() { return 0; }
