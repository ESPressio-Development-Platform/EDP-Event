#include "../support/EventTestSupport.hpp"

struct ExternalEvent final {
    inline static constexpr ESPressio::System::TypeIdentifier Identifier{
        ESPressio::System::TypeIdentifier::Storage{0,0,1,0,0,0,0x31,2}
    };
    using Family = ESPressio::Event::Family;
    int Value{};
};

namespace ESPressio::Bounded {
    template<>
    struct MemoryBoundedTraits<ExternalEvent> : MemoryBoundedValueDeclaration<true, int> {};
}

using Invalid = ESPressio::Event::Deploy<
    ExternalEvent, 1U, ESPressio::Event::NewestOnly, ESPressio::Event::UntilHandoffOnly
>;
static_assert(sizeof(Invalid) > 0U);
