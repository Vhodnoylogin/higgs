"""Production observation store/C ABI and native controller capture on explicit SDK doubles."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
sys.dont_write_bytecode = True
from run_tests import msvc_environment

ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--build-dir',type=Path,required=True)
    args=parser.parse_args()
    out=args.build_dir.resolve()
    if out==ROOT or ROOT in out.parents:parser.error('External build directory required')
    out.mkdir(parents=True,exist_ok=True)
    env=msvc_environment()
    cl=shutil.which('cl',path=env.get('PATH',env.get('Path')))
    commands=[]
    def run(name,command):
        argv=list(map(str,command));r=subprocess.run(argv,cwd=out,env=env,capture_output=True);(out/(name+'.log')).write_bytes(r.stdout+r.stderr);commands.append({'argv':argv,'exitCode':r.returncode})
        if r.returncode:raise RuntimeError(name+' failed; see '+str(out/(name+'.log')))
        print(name+': passed',flush=True)
    flags=[cl,'/nologo','/std:c++20','/EHsc','/O2','/W4','/WX',f'/I{ROOT/"include"}']
    closure=ROOT/'cmake/HiggsServices.cmake'
    source_list=closure.read_text()
    sources=[ROOT/name for name in re.findall(r'"\$\{HIGGS_SERVICE_ROOT\}/(src/[^"\n]+\.cpp)"',source_list)]
    headers=[ROOT/name for name in re.findall(r'"\$\{HIGGS_SERVICE_ROOT\}/(include/[^"\n]+\.h)"',source_list)]
    paths=list(dict.fromkeys(headers+sources+[closure,ROOT/'src/hand_observations_native.cpp',ROOT/'src/main.cpp',ROOT/'src/physics.cpp',ROOT/'src/controller_tracking.cpp',ROOT/'include/controller_tracking.h',ROOT/'tests/observation_tests.cpp',ROOT/'tests/observation_c_consumer.c',ROOT/'tests/observation_controller_fakes.h',ROOT/'tests/observation_controller_cases.h',ROOT/'docs/hand-observations.md',Path(__file__)]))
    pins={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    main_source=(ROOT/'src/main.cpp').read_text()
    begin=main_source.index('void FillControllerVelocities(')
    apply=main_source.index('HiggsMotion::Tracking().Apply(event);',begin)
    observe=main_source.index('HiggsHands::ObserveNativeControllers(',apply)
    if '#include "hand_observations.h"' not in main_source or observe>main_source.index('void UpdateShadowDelay()',begin):
        raise RuntimeError('Native controller hook is not after tracking publication')
    obj=out/'observation_c.obj'
    run('c-consumer',[cl,'/nologo','/std:c11','/TC','/W4','/WX',f'/I{ROOT/"include"}','/c',ROOT/'tests/observation_c_consumer.c',f'/Fo{obj}'])
    executable=out/'observation_tests.exe'
    run('core-compile',[*flags,*sources,ROOT/'tests/observation_tests.cpp',obj,f'/Fe{executable}','/link','/STACK:8388608'])
    run('core-run',[executable])
    native=(ROOT/'src/hand_observations_native.cpp').read_text()
    start=native.index('namespace {\nbool PoseFinite(')
    extracted=out/'observation_controller_extracted.cpp'
    extracted.write_text('#include "observation_controller_fakes.h"\nnamespace HiggsHands {\n'+native[start:]+'\n#include "observation_controller_cases.h"\n')
    controller=out/'observation_controller_tests.exe'
    run('controller-compile',[*flags,f'/I{ROOT/"tests"}',*sources,ROOT/'src/controller_tracking.cpp',extracted,f'/Fe{controller}','/link','/STACK:8388608'])
    run('controller-run',[controller])
    for p in paths:
        if hashlib.sha256(p.read_bytes()).hexdigest()!=pins[str(p.relative_to(ROOT))]:raise RuntimeError('Source changed during build: '+str(p))
    (out/'observation-manifest.json').write_text(json.dumps({'commands':commands,'sourceSha256':pins,'gameStarts':0,'qualification':'Actual production store, C layout, C ABI broker and extracted native controller capture on explicit SDK doubles. No native engine/hardware qualification.'},indent=2)+'\n')

if __name__=='__main__':main()
