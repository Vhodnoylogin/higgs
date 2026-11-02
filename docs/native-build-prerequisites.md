# Native build prerequisites

Checked on 11 October 2026. The production plugin remains unbuilt and unqualified in Skyrim. Standalone source/ABI fixtures exercise the delivered repairs without establishing native hook or Havok layout compatibility. The current [implementation ledger](https://github.com/Vhodnoylogin/body-pouches/blob/main/docs/trinity-synergy/implementation-status.md) records that boundary.

## Available dependencies and configuration work

MSVC toolset `v145` and Windows SDK `10.0.26100.0` are installed. Original SKSE VR `2.00.12` source, `common/IPrefix.h`, SKSE support sources, Xbyak and OpenVR headers also exist outside this checkout. The earlier missing-prefix/SKSE-project failures are caused by the authored project layout; they do not establish that the SDK is unavailable.

The [native project](../higgs_vr.vcxproj) targets Release/x64, C++20 and the static CRT. It expects sibling `../skse64/skse64.vcxproj` and `../skse64_common/skse64_common.vcxproj`, a parent include root containing `common/`, and an absolute author-specific Havok include directory. These paths do not resolve in the current independent checkout. The original SDK requires both its `src` and `src/sksevr` include roots. Its default SKSE Release target is a DLL; static support uses `Release_Lib` variants or separately compiled required SDK units. A future build definition must reconcile those targets, runtime/toolset settings, libraries and external paths. Disabling project-reference builds alone does not supply their link symbols.

## Remaining header prerequisite

The source directly includes 37 distinct Havok-style paths. Mandatory examples include `Common/Base/hkBase.h`, `Physics/Dynamics/Entity/hkpRigidBody.h`, `Physics/Collide/Shape/Convex/Box/hkpBoxShape.h` and `Physics/Utilities/Collide/TriggerVolume/hkpTriggerVolume.h`. HIGGS also retains an author-specific SKSE library directory that must be replaced with a verified external support-library output.

The required compatible include tree is absent from the audited workspace and the project's authored directory. A bounded workspace search found no `hkBase.h`; rigid-body matches were CommonLib-modeled `RE/...` headers, rather than the paths requested by this project. This is a bounded location check, not a claim about every possible source or every file on the machine. The exact definitions and layout assertions must be qualified before accepting a complete native build. The project does not explicitly name a Havok library; further linker requirements remain unproven until compilation can proceed.

## Actual compiler probe on 2 November 2026

Compiling the actual new `src/hand_native.cpp` translation unit with MSVC v145, C++20, this repository's `include`, both original SKSE VR 2.00.12 include roots (`src` and `src/sksevr`) and forced `common/IPrefix.h` reaches `include/RE/havok.h:3`. The compiler returns C1083 for the exact missing path `Common/Base/hkBase.h`. This is an actual native compilation attempt, not a failure to find the available SKSE prefix. No linker, deployment hook or game start ran. The required header tree also includes `Physics/Dynamics/Entity/hkpRigidBody.h`, `Physics/Collide/Shape/Convex/Box/hkpBoxShape.h`, `Physics/Collide/Query/CastUtil/hkpWorldRayCastInput.h`, `Physics/Collide/Shape/Query/hkpRayHitCollector.h` and the constraint/motor paths already included by HIGGS. Their `hkpRigidBody`, `hkpCdBody/hkpCollidable`, `hkpWorldRayCastInput`, ray collector/output, `hkpBoxShape` and motor definitions and layouts must match the Skyrim VR runtime before accepting a native build.

The available CommonLibVR tree has modeled `RE/hkpRigidBody.h`, `RE/bhkRigidBody.h` and `RE/hkBaseTypes.h`; CommonLibSSE-NG also has `RE/H/hkpBoxShape.h` and ray collector declarations. These are different paths, namespaced types and API subsets. Adding a CommonLib include directory does not satisfy HIGGS's Havok 2010-style include tree or prove binary compatibility. There is no CommonLib defect established by this probe, and no CommonLib shim, wrapper or migration is implemented here. A future second-stage port must map each required operation/layout explicitly and then compile/link/test it.

## Isolated build precautions

The plugin has a post-build DLL copy into the game directory. SKSE dependency projects also have copy hooks, including some static-library configurations. Any future isolated build must disable post-build events globally with `/p:PostBuildEventUseInBuild=false`, set external `OutDir` and `IntDir`, and check the dependency targets. Read-only project-property evaluation did not invoke those targets or deploy anything.

The next build step is to provide the compatible header dependency and configure explicit external SDK paths/static support targets, then compile the actual plugin and qualify hooks/layouts. No native build definition, dependency replacement or CommonLib implementation is delivered by this prerequisite analysis.
