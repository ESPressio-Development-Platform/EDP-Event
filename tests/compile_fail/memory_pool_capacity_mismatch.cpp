#include "../support/BootstrapTestSupport.hpp"
namespace S = EventBootstrapSupport;

struct BadMemoryRuntime final {
    template<class TObject>
    using ObjectPoolType = S::DummyPool<TObject>;
    template<class TObject>
    struct BadSpec final {
        using Dedicated = ESPressio::Memory::DedicatedInstances<1U>;
        using Shared = ESPressio::Memory::NoSharedOverflow;
    };
    struct Topology final {
        template<class TObject>
        static constexpr bool ContainsObjectPool = true;
        template<class TObject>
        using ObjectPoolSpecFor = BadSpec<TObject>;
    };
    bool IsInitialized() const noexcept { return true; }
};

using Invalid = ESPressio::Event::Bootstrap<
    S::GoodArchitecture,
    S::Plan,
    BadMemoryRuntime,
    S::FakeThreadingRuntime
>;
static_assert(sizeof(Invalid) > 0U);
