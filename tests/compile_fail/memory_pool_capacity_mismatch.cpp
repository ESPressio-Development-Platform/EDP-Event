#include "../support/BootstrapTestSupport.hpp"
namespace S = EventBootstrapSupport;

struct BadMemoryRuntime final {
    template<class TObject>
    /// Maps requested occurrence Types to the deliberately mismatched fake pool.
    using ObjectPoolType = S::DummyPool<TObject>;
    template<class TObject>
    struct BadSpec final {
        /// Deliberately wrong dedicated capacity used to trigger Runtime validation.
        using Dedicated = ESPressio::Memory::DedicatedInstances<1U>;
        /// Confirms the mismatch test does not rely on raw shared overflow.
        using Shared = ESPressio::Memory::NoSharedOverflow;
    };
    struct Topology final {
        template<class TObject>
        /// Advertises presence so validation reaches the capacity mismatch.
        static constexpr bool ContainsObjectPool = true;
        template<class TObject>
        /// Returns the deliberately invalid pool specification.
        using ObjectPoolSpecFor = BadSpec<TObject>;
    };
    /// Reports initialized so only the compile-time pool shape can fail.
    bool IsInitialized() const noexcept { return true; }
};

using Invalid = ESPressio::Event::Bootstrap<
    S::GoodArchitecture,
    S::Plan,
    BadMemoryRuntime,
    S::FakeThreadingRuntime
>;
static_assert(sizeof(Invalid) > 0U);
