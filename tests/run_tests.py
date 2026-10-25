"""Compile the production restriction core and copied C ABI without SKSE/Havok.

python tests/run_tests.py --build-dir <external-output-directory>
Requires an x64 MSVC C++ toolchain on Windows, or g++ on other platforms.
No game files, native mod output, repository files or dependencies are changed.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
sys.dont_write_bytecode = True  # Generated support-module cache must not enter this checkout.
from motion_fixture import generate as generate_motion_fixture

ROOT = Path(__file__).resolve().parents[1]


def run(args, cwd, env=None):
    print("+", subprocess.list2cmdline([str(arg) for arg in args]), flush=True)
    subprocess.run([str(arg) for arg in args], cwd=cwd, env=env, check=True)


def msvc_environment():
    if shutil.which("cl"):
        return dict(os.environ)
    vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    install = subprocess.check_output([str(vswhere), "-latest", "-products", "*", "-requires",
        "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"], text=True).strip()
    if not install:
        raise RuntimeError("An x64 MSVC toolchain is required")
    vcvars = Path(install) / "VC/Auxiliary/Build/vcvars64.bat"
    # Pass a cmd command line directly: list2cmdline escapes embedded quotes for
    # C runtimes, whereas cmd requires its own pair of outer quotes.
    command = f'cmd.exe /d /s /c ""{vcvars}" >nul && set"'
    output = subprocess.check_output(command, text=True)
    environment = dict(os.environ)
    for line in output.splitlines():
        if "=" in line and not line.startswith("="):
            key, value = line.split("=", 1)
            environment[key.upper()] = value
    return environment


def audit_integration():
    expected = json.loads((ROOT / "tests/official001-sha256.json").read_text())
    for name, digest in expected.items():
        actual = hashlib.sha256((ROOT / name).read_bytes().replace(b"\r\n", b"\n")).hexdigest()
        if actual != digest:
            raise RuntimeError(f"Official001 byte preservation failed: {name}")
    ns = {"p": "http://schemas.microsoft.com/developer/msbuild/2003"}
    project = ET.parse(ROOT / "higgs_vr.vcxproj").getroot()
    sources = {node.attrib["Include"] for node in project.findall(".//p:ClCompile[@Include]", ns)}
    for source in [r"src\synergy_core.cpp", r"src\synergy_runtime.cpp"]:
        if source not in sources:
            raise RuntimeError(f"Native project lacks production source: {source}")
    if r"src\controller_tracking.cpp" not in sources:
        raise RuntimeError("Native project lacks the motion history owner")
    if "HIGGS_QuerySynergyInterface" not in (ROOT / "exports.def").read_text():
        raise RuntimeError("Native DLL export is missing")
    # Verify the production policy is consumed by native callsites, not only tests.
    hand = (ROOT / "src/hand.cpp").read_text()
    main = (ROOT / "src/main.cpp").read_text()
    pluginapi = (ROOT / "src/pluginapi.cpp").read_text()
    for call in ["HiggsSynergy::ShouldDisableWeaponCollision", "HiggsSynergy::MayContinueHeldInteraction",
                 "HiggsSynergy::MayStartHandInteraction", "!CanTwoHand(true)"]:
        if call not in hand:
            raise RuntimeError(f"Native admission/continuation/filter integration missing: {call}")
    # Native integration is unbuilt without the author's SDK/Havok dependencies.
    # This scoped structural guard catches admission omitted from an async spawn
    # branch; it does not claim execution of Skyrim's hand state machine.
    for state, clock in [("GrabExternal", "grabbedTime"), ("PreGrabItem", "grabbedTime"),
                         ("PrePullItem", "pulledTime")]:
        start = hand.index(f"if (state == State::{state}) {{")
        first_guard = hand[start:hand.index("state = State::Idle;", start)]
        if f"!isAllowedToHold || g_currentFrameTime - {clock}" not in first_guard:
            raise RuntimeError(f"Pending {state} lacks admission revalidation before commitment")
    for lifecycle in ["HiggsSynergy::ObserveWorld", "HiggsSynergy::BeginGameLoad", "HiggsSynergy::EndGameLoad", "HiggsSynergy::NewGame"]:
        if lifecycle not in main:
            raise RuntimeError(f"Native lifecycle integration missing: {lifecycle}")
    for setter in ["SetLegacyHandDisabled", "SetLegacyWeaponCollisionDisabled", "ForceLegacyWeaponCollision"]:
        if f"HiggsSynergy::{setter}" not in pluginapi:
            raise RuntimeError(f"Legacy adapter missing: {setter}")
    print("PASS official001 bytes, native project/export and policy/lifecycle callsite audit", flush=True)


def run_motion_checks(output):
    generate_motion_fixture(ROOT, output)
    include = ROOT / "include"
    core = ROOT / "src/controller_tracking.cpp"
    targets = [("controller_tracking_tests", ROOT / "tests/controller_tracking_tests.cpp"),
               ("controller_motion_fixture", ROOT / "tests/controller_motion_fixture.cpp")]
    if os.name == "nt":
        environment = msvc_environment()
        compiler = shutil.which("cl", path=environment.get("PATH"))
        for name, source in targets:
            exe = output / (name + ".exe")
            run([compiler, "/nologo", "/std:c++20", "/EHsc", "/W4", "/WX", "/O2",
                 f"/I{include}", f"/I{output}", core, source, f"/Fe{exe}"], output, environment)
            run([exe], output, environment)
    else:
        for name, source in targets:
            exe = output / name
            legacy_warning = ["-Wno-sign-compare"] if name == "controller_motion_fixture" else []
            run(["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-O2", "-pthread", *legacy_warning,
                 f"-I{include}", f"-I{output}", core, source, "-o", exe], output)
            run([exe], output)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True, type=Path)
    parser.add_argument("--motion-only", action="store_true", help="run the motion core and extracted native-source fixture only")
    args = parser.parse_args()
    output = args.build_dir.resolve()
    if output == ROOT or ROOT in output.parents:
        parser.error("--build-dir must be outside the repository")
    output.mkdir(parents=True, exist_ok=True)
    audit_integration()
    if args.motion_only:
        run_motion_checks(output)
        return 0
    include = ROOT / "include"
    sources = [ROOT / "src/synergy_core.cpp", ROOT / "src/synergy_runtime.cpp", ROOT / "tests/synergy_tests.cpp"]
    if os.name == "nt":
        env = msvc_environment()
        compiler = shutil.which("cl", path=env.get("PATH"))
        if not compiler:
            raise RuntimeError("MSVC environment did not provide cl.exe")
        run([compiler, "/nologo", "/std:c11", "/W4", "/WX", "/TC", f"/I{include}", "/c",
             ROOT / "tests/synergy_c_consumer.c", f"/Fo{output / 'c_consumer.obj'}"], output, env)
        exe = output / "synergy_tests.exe"
        run([compiler, "/nologo", "/std:c++20", "/EHsc", "/W4", "/WX", "/O2", f"/I{include}",
             *sources, output / "c_consumer.obj", f"/Fe{exe}", "/link", "/MACHINE:X64"], output, env)
        run([exe], output, env)
        # Build the same provider sources as a DLL with the service's actual
        # public symbol. This is an ABI/export probe, not a native game plugin.
        definition = output / "synergy_probe.def"
        definition.write_text("EXPORTS\n    HIGGS_QuerySynergyInterface\n", encoding="ascii")
        dll = output / "synergy_probe.dll"
        run([compiler, "/nologo", "/std:c++20", "/EHsc", "/W4", "/WX", "/O2", "/LD", f"/I{include}",
             *sources[:2], f"/Fe{dll}", "/link", "/MACHINE:X64", f"/DEF:{definition}"], output, env)
        probe = output / "synergy_dll_probe.exe"
        run([compiler, "/nologo", "/std:c11", "/W4", "/WX", "/TC", f"/I{include}",
             ROOT / "tests/synergy_dll_probe.c", f"/Fe{probe}", "/link", "/MACHINE:X64"], output, env)
        run([probe, dll], output, env)
    else:
        obj = output / "c_consumer.o"
        run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", f"-I{include}", "-c",
             ROOT / "tests/synergy_c_consumer.c", "-o", obj], output)
        exe = output / "synergy_tests"
        run(["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-O2", "-pthread", f"-I{include}",
             *sources, obj, "-o", exe], output)
        run([exe], output)
    run_motion_checks(output)
    return 0


if __name__ == "__main__":
    sys.exit(main())
