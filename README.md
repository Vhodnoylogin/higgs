# HIGGS ([Nexus link](https://www.nexusmods.com/skyrimspecialedition/mods/43930))

The additive [nonbutton hand-use proposals](docs/hand-use-proposals.md) collect contact, dwell and posture opportunities into a main-maintenance priority round. Disjoint winners receive ordinary hand-use grants; pending proposals have no authority, and existing uses are never preempted.

The additive [own-item cosmetic contact service](docs/own-item-contacts.md) supplies HIGGS-owned bilateral item identities, explicit collider/UID provenance, bounded policies and paired contact-point evidence forwarded by PLANCK. It preserves existing combat/solver behavior; audio and haptic presentation remain a consumer responsibility and have not been qualified in game.

The additive [controller menu-context service](docs/input-context.md) hands fresh controls to supported UI, drains held sequences across entry/exit without replay, and preserves existing physical use and committed receipt/recovery paths. It does not infer engine pause or change official001.

The separate [hand-use transition table](docs/hand-use-transitions.md) atomically adds or removes hands from a live session while retaining its original grant. Removal waits for dependent physical cleanup; it does not drop held items or preempt another owner.

The additive [owned physical policies](docs/physical-policies.md) cover constrained held-item mass/inertia/COM, per-hand physics-grab preference, HIGGS finger suppression and controller-node culling. Independent secondary-grip transforms extend the negotiated finite operation service. Their native-source/compiler evidence and remaining presentation limits are documented separately.

## Proposed cooperation with VRIK HIGGS and PLANCK

This fork's intended architectural direction is described in [the HIGGS synergy requirements](docs/trinity-synergy.md), linked to the full concept maintained only in Body Pouches. HIGGS owns hand state, hand use, input and physical possession; Body Slots owns all slot mechanics, VRIK owns body/virtual poses, PLANCK owns NPC physics and PPB supplies anatomy. The full concept remains solely in Body Pouches. Fork source APIs are distinct from a released integration or an agreement with framework authors.

The additive [articulated hand-parts API](docs/articulated-hand-parts.md) lets a provider such as PPB submit fitted finger BOXes while HIGGS owns their native bodies, keyframes, filters, lifecycle and canonical contact attribution. It preserves the existing palm and separates proposed geometry, actual applied revision, observed body pose and keyframe target. Offline checks and native compilation do not establish an in-game PPB port.

## Scoped hand and weapon collision restrictions

This development fork implements a separate, negotiated [restriction service](docs/scoped-restrictions.md) for physical left/right hands. Consumers can acquire, renew and release bounded restrictions without clearing another consumer's lease. The existing 001 header and vtable remain unchanged. A scoped hand restriction blocks new interactions while preserving existing possession; collision suppression takes precedence over collision force requests.

The production service and its copied C ABI pass standalone ownership, lifecycle, concurrency and compatibility checks. The complete native plugin builds with explicitly staged external SKSE/Havok declarations and the game's existing allocator bindings; Skyrim behavior remains untested. This restriction service remains independently negotiated; the additive hand broker below supplies input/grant/action contracts without taking over consumer gameplay or inventory policy.

Run the standalone checks with `python tests/run_tests.py --build-dir <external-output-directory>`. The tests compile the actual production core and ABI adapter, including a C consumer, without SKSE or Havok. See the service documentation for its lifetime, renewal and threading contract.

The [native build prerequisites](docs/native-build-prerequisites.md) distinguish the available SKSE/toolchain from legacy project paths, and document reproducible native build commands and their qualification limits.

An internal [controller-motion availability repair](docs/controller-motion.md) clears only affected tracking histories on observed failures, preserves valid five-sample behavior and player/object release motion, and treats unavailable motion separately from stationary motion. It adds no callback-silence freshness guarantee. The standalone command above also runs its actual numeric core and extracted callback/consumer fixtures; `--motion-only` selects the motion checks. Native plugin/game qualification remains pending.

The additive [read-only velocity ABI](docs/velocity-observation.md) copies that existing owner's physical-hand pair, cached velocity/history metrics, private continuity state and producer-computed observation age. Consumers choose an explicit age policy; last-observed availability is not current tracking or palm validity. The separate C header/export preserves official001 and scoped restrictions. `--motion-abi-only` runs its actual core, C/C++ consumers and isolated DLL discovery checks without claiming a full native-plugin build.

The additive [hand services](docs/hand-broker.md) supply coherent occupancy/availability, common controller observations and arbitration, deferred hold/multi-tap recognition, end-user priorities, owned sessions/grants, executor routing and fenced commits. Typed transactions preserve original/proxy/display identities and separate inventory, equipment, physical grip and compensation facts. Finite native paths cover static and moving world support, PLANCK-leased NPC endpoints, owned held-item constraints, physical hand targets, existing secondary support, grip transforms, motor caps, box hand geometry and coordinated haptics. [Motion/contact/release observations](docs/hand-observations.md) preserve source and continuity identities. Production-core/C/DLL checks, actual-source synthetic fixtures and native compilation provide distinct evidence; none establishes running-game collision response, hardware delivery or author agreement. No live installation or game test has occurred.

This mod is complicated.

The optional [Body Slots spatial adapter](docs/native-regions.md) exports actual native mouth/shoulder source samples and accepts one spatial-only detector. HIGGS preserves standalone fallback and its single consume/stash/drop executor, including current hand-authority checks before effects.

The [native player inventory executor](docs/native-inventory.md) supplies observed exact-stack selection tickets and common inventory/world/hand, consumption and hand-equipment transactions. Consumers reuse the existing transaction coordinator; they do not need a bespoke engine remove/pickup/equip executor. Deferred engine postfacts, uncertain mutations and compensation remain explicit. This source is offline tested and native compiled, with no running-game qualification.

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
