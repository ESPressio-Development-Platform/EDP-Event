#include "../support/EventTestSupport.hpp"

struct EventA final {
    /// Stable identifier intentionally duplicated by EventB.
    inline static constexpr ESPressio::System::TypeIdentifier Identifier{
        ESPressio::System::TypeIdentifier::Storage{0,0,1,0,0,0,0x32,1}
    };
    /// Binds EventA to the Event Primitive family.
    using Family = ESPressio::Event::Family;
    /// EventA payload value used only to form a complete bounded Event.
    int Value{};
};
struct EventB final {
    /// Deliberately duplicates EventA identity to exercise planner rejection.
    inline static constexpr ESPressio::System::TypeIdentifier Identifier = EventA::Identifier;
    /// Binds EventB to the Event Primitive family.
    using Family = ESPressio::Event::Family;
    /// EventB payload value used only to form a complete bounded Event.
    int Value{};
};
namespace ESPressio::Bounded {

    template<> struct MemoryBoundedTraits<EventA> : MemoryBoundedValueDeclaration<false, int> {};
    template<> struct MemoryBoundedTraits<EventB> : MemoryBoundedValueDeclaration<false, int> {};

} // ESPressio::Bounded
using Invalid = ESPressio::Primitives::Topology<
    ESPressio::Event::Deploy<EventA, 1U, ESPressio::Event::NewestOnly, ESPressio::Event::UntilHandoffOnly>,
    ESPressio::Event::Deploy<EventB, 1U, ESPressio::Event::NewestOnly, ESPressio::Event::UntilHandoffOnly>
>;
static_assert(sizeof(Invalid) > 0U);
