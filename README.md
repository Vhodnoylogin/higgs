# HIGGS ([Nexus link](https://www.nexusmods.com/skyrimspecialedition/mods/43930))

## Proposed cooperation with VRIK HIGGS and PLANCK

This fork's intended architectural direction is described in [the HIGGS synergy requirements](docs/trinity-synergy.md), linked to the full concept maintained only in Body Pouches. HIGGS owns input and physical possession; a separate slot service owns offline authoring and saved definitions, while VRIK handles their runtime binding and interactions. These are proposed services, not a released API or an agreement with the framework authors.

## Scoped hand and weapon collision restrictions

This development fork implements a separate, negotiated [restriction service](docs/scoped-restrictions.md) for physical left/right hands. Consumers can acquire, renew and release bounded restrictions without clearing another consumer's lease. The existing 001 header and vtable remain unchanged. A scoped hand restriction blocks new interactions while preserving existing possession; collision suppression takes precedence over collision force requests.

The production service and its copied C ABI pass standalone ownership, lifecycle, concurrency and compatibility checks. Full native-plugin integration has not been built or tested in Skyrim: the existing project requires external SKSE/Havok dependencies. This is an admission and collision restriction service; input arbitration, committed action reservations, climbing/harvesting gameplay and transfer APIs remain proposals.

Run the standalone checks with `python tests/run_tests.py --build-dir <external-output-directory>`. The tests compile the actual production core and ABI adapter, including a C consumer, without SKSE or Havok. See the service documentation for its lifetime, renewal and threading contract.

The [native build prerequisites](docs/native-build-prerequisites.md) distinguish the available SKSE/toolchain from nonportable project paths and the remaining compatible Havok headers.

An internal [controller-motion availability repair](docs/controller-motion.md) clears only affected tracking histories on observed failures, preserves valid five-sample behavior and player/object release motion, and treats unavailable motion separately from stationary motion. It adds no motion ABI or callback-silence freshness guarantee. The standalone command above also runs its actual numeric core and extracted callback/consumer fixtures; `--motion-only` selects those checks alone. Native plugin/game qualification remains pending.

This mod is complicated.

To build from source, it requires object definitions / headers similar to those belonging to havok 2010.2. I will not be providing these, nor do I claim to have them myself.


HIGGS has a C++ API that can be used by other mods.\
To use it, copy `src/higgsinterface001.cpp` and `include/higgsinterface001.h` into your project.\
Then, do something like this in your SKSE plugin in `PostPostLoad` or later (this is important - if you try and get the interface before `PostPostLoad`, such as in `PostLoad`, it will not work).

```cpp
#include "higgsinterface001.h"

...

void OnSKSEMessage(SKSEMessagingInterface::Message* msg)
{
  if (msg) {
    if (msg->type == SKSEMessagingInterface::kMessage_PostPostLoad) {
      // Get the HIGGS plugin API
      HiggsPluginAPI::GetHiggsInterface001(g_pluginHandle, g_messaging);
      if (g_higgsInterface) {
        _MESSAGE("Got higgs interface!");
        unsigned int higgsVersion = g_higgsInterface->GetBuildNumber();
      }
    }
  }
}
        
```
