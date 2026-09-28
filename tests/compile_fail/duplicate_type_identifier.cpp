#include "../support/EventTestSupport.hpp"

struct EventA final {
    inline static constexpr ESPressio::System::TypeIdentifier Identifier{
        ESPressio::System::TypeIdentifier::Storage{0,0,1,0,0,0,0x32,1}
    };
    using Family = ESPressio::Event::Family;
    int Value{};
};
struct EventB final {
    inline static constexpr ESPressio::System::TypeIdentifier Identifier = EventA::Identifier;
    using Family = ESPressio::Event::Family;
    int Value{};
};
namespace ESPressio::Bounded {
    template<> struct MemoryBoundedTraits<EventA> : MemoryBoundedValueDeclaration<false, int> {};
    template<> struct MemoryBoundedTraits<EventB> : MemoryBoundedValueDeclaration<false, int> {};
}
using Invalid = ESPressio::Primitives::Topology<
    ESPressio::Event::Deploy<EventA, 1U, ESPressio::Event::NewestOnly, ESPressio::Event::UntilHandoffOnly>,
    ESPressio::Event::Deploy<EventB, 1U, ESPressio::Event::NewestOnly, ESPressio::Event::UntilHandoffOnly>
>;
static_assert(sizeof(Invalid) > 0U);
