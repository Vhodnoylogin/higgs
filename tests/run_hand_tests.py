"""Actual production hand broker + copied C ABI; no SKSE/Havok/game simulation."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
sys.dont_write_bytecode = True
from run_tests import msvc_environment
from hand_native_fixture import generate

ROOT = Path(__file__).resolve().parents[1]

def audit_native():
    ns={'p':'http://schemas.microsoft.com/developer/msbuild/2003'}
    project=ET.parse(ROOT/'higgs_vr.vcxproj').getroot()
    sources=[node.attrib['Include'] for node in project.findall('.//p:ClCompile[@Include]',ns)]
    for name in ['hand_broker','hand_runtime','hand_native']:
        if sources.count('src\\'+name+'.cpp')!=1:raise RuntimeError('native source omitted/duplicated '+name)
    exports=(ROOT/'exports.def').read_text()
    for symbol in ['HIGGS_QueryHandInterface','HIGGS_QueryHandRoutingInterface','HIGGS_QueryHandOperationsInterface']:
        if symbol not in exports:raise RuntimeError('missing '+symbol)
    hooks=(ROOT/'src/hooks.cpp').read_text()
    publish=hooks.index('HiggsHands::PublishNative();')
    if not hooks.index('g_leftHand->PostVrikUpdate();')<publish<hooks.index('HiggsPluginAPI::TriggerPostVrikPostHiggsCallbacks();'):
        raise RuntimeError('hand publication is not before post-VRIK consumer callbacks')
    print('PASS native source/project/export and pose-publication placement audit',flush=True)

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--build-dir',required=True,type=Path)
    args=parser.parse_args()
    out=args.build_dir.resolve()
    if out==ROOT or ROOT in out.parents:
        parser.error('build directory must be outside Git')
    out.mkdir(parents=True,exist_ok=True)
    audit_native()
    env=msvc_environment() if os.name=='nt' else None
    commands=[]
    def run(name,args):
        command=[str(arg) for arg in args]
        commands.append(command)
        print('+ '+subprocess.list2cmdline(command),flush=True)
        result=subprocess.run(command,cwd=out,env=env,capture_output=True)
        log=result.stdout+result.stderr
        (out/(name+'.log')).write_bytes(log)
        print(log.decode(errors='replace'),end='',flush=True)
        if result.returncode:
            raise RuntimeError(name+' failed')
    sources=[ROOT/'src/hand_broker.cpp',ROOT/'src/hand_runtime.cpp']
    if os.name=='nt':
        compiler=shutil.which('cl',path=env['PATH'])
        flags=[compiler,'/nologo','/std:c++20','/EHsc','/W4','/WX','/O2',f'/I{ROOT / "include"}']
        cflags=[compiler,'/nologo','/std:c11','/W4','/WX','/TC',f'/I{ROOT / "include"}']
        obj=out/'hand_c_consumer.obj'
        run('c-compile',[*cflags,'/c',ROOT/'tests/hand_c_consumer.c',f'/Fo{obj}'])
        exe=out/'hand_broker_tests.exe'
        run('core-compile',[*flags,*sources,ROOT/'tests/hand_broker_tests.cpp',obj,f'/Fe{exe}'])
        run('core-run',[exe])
        generate(ROOT,out)
        fixture=out/'hand_native_fixture.exe'
        run('native-source-compile', [*flags,'/wd4100',f'/I{ROOT / "tests"}',*sources,out/'hand_native_extracted.cpp',f'/Fe{fixture}'])
        run('native-source-run',[fixture])
        definition=out/'hand_probe.def'
        definition.write_text('EXPORTS\n HIGGS_QueryHandInterface\n HIGGS_QueryHandRoutingInterface\n HIGGS_QueryHandOperationsInterface\n')
        dll=out/'hand_probe.dll'
        run('dll-compile',[*flags,'/LD',*sources,f'/Fe{dll}','/link',f'/DEF:{definition}'])
        probe=out/'hand_dll_probe.exe'
        run('probe-compile',[*cflags,ROOT/'tests/hand_dll_probe.c',f'/Fe{probe}'])
        run('probe-run',[probe,dll])
    else:
        obj=out/'hand_c_consumer.o'
        run('c-compile',['cc','-std=c11','-Wall','-Wextra','-Werror',f'-I{ROOT / "include"}','-c',ROOT/'tests/hand_c_consumer.c','-o',obj])
        exe=out/'hand_broker_tests'
        run('core-compile',['g++','-std=c++20','-Wall','-Wextra','-Werror','-pthread',f'-I{ROOT / "include"}',*sources,ROOT/'tests/hand_broker_tests.cpp',obj,'-o',exe])
        run('core-run',[exe])
    pins=[p for p in ['include/higgshandapi.h','include/higgshandrouting.h','include/higgshandoperations.h','include/hand_broker.h','src/hand_broker.cpp','src/hand_runtime.cpp','src/hand_native.cpp','src/hand.cpp','src/main.cpp','src/hooks.cpp','src/synergy_runtime.cpp','tests/hand_broker_tests.cpp','tests/hand_c_consumer.c','tests/run_hand_tests.py','tests/hand_dll_probe.c','tests/hand_native_fixture.py','tests/hand_native_fakes.h','tests/hand_native_fixture_cases.h','src/config.cpp','include/config.h','config/hand-broker.ini.example','docs/hand-broker.md','higgs_vr.vcxproj','higgs_vr.vcxproj.filters','exports.def']]
    (out/'hand-manifest.json').write_text(json.dumps({'commands':commands,'sourceSha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in pins},'nativePluginBuilt':False,'gameStarts':0,'qualification':'Production broker/C ABI/DLL discovery and extracted actual native source on synthetic engine seams; no engine layouts, game equipment behavior, collision response, hardware or complete native-plugin qualification.'},indent=2)+'\n')

if __name__=='__main__':
    main()
