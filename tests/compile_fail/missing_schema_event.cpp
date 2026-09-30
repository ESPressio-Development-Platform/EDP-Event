#include "../support/EventTestSupport.hpp"

struct MissingSchemaEvent final {
    /// Stable Event identity proving universal Type identity alone is insufficient.
    inline static constexpr ESPressio::System::TypeIdentifier Identifier{
        ESPressio::System::TypeIdentifier::Storage{0,0,1,0,0,0,0x31,3}
    };

    /// Correct Event family classification without the required System schema metadata.
    using Family = ESPressio::Event::Family;

    int Value{};
};

namespace ESPressio::Bounded {

    template<>
    struct MemoryBoundedTraits<MissingSchemaEvent> : MemoryBoundedValueDeclaration<false, int> {};

} // ESPressio::Bounded

using Invalid = ESPressio::Event::Deploy<
    MissingSchemaEvent, 1U, ESPressio::Event::NewestOnly, ESPressio::Event::UntilHandoffOnly
>;
static_assert(sizeof(Invalid) > 0U);
