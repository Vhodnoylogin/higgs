"""Actual extended input/frame services and sole Broker arbitration; no game launch."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
sys.dont_write_bytecode=True
from run_tests import msvc_environment
ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--build-dir',type=Path,required=True)
    out=parser.parse_args().build_dir.resolve()
    if out==ROOT or ROOT in out.parents:parser.error('External output required')
    out.mkdir(parents=True,exist_ok=True);env=msvc_environment();cl=shutil.which('cl',path=env.get('PATH',env.get('Path')))
    cmake=(ROOT/'cmake/HiggsServices.cmake').read_text()
    sources=[ROOT/p for p in re.findall(r'"\$\{HIGGS_SERVICE_ROOT\}/(src/[^"\n]+\.cpp)"',cmake)]
    files=sources+[ROOT/'include/higgsinput.h',ROOT/'include/hand_input_extended.h',ROOT/'include/hand_broker.h',ROOT/'src/main.cpp',ROOT/'cmake/HiggsServices.cmake',ROOT/'tests/input_tests.cpp',ROOT/'tests/input_c_consumer.c',ROOT/'tests/input_dll_probe.c',ROOT/'tests/input_native_fakes.h',ROOT/'tests/input_native_cases.h',Path(__file__)]
    files += [ROOT/p for p in re.findall(r'"\$\{HIGGS_SERVICE_ROOT\}/(include/[^"\n]+\.h)"',cmake)]
    files += [ROOT/'src/menu_checker.cpp',ROOT/'include/menu_checker.h',ROOT/'tests/input_context_cases.h']
    pins={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files};commands=[]
    def run(name,args):
        argv=list(map(str,args));r=subprocess.run(argv,cwd=out,env=env,capture_output=True);(out/(name+'.log')).write_bytes(r.stdout+r.stderr);commands.append({'argv':argv,'exitCode':r.returncode})
        if r.returncode:raise RuntimeError(name+' failed; see '+str(out/(name+'.log')))
        print(name+': passed',flush=True)
    obj=out/'input_c.obj';run('c-compile',[cl,'/nologo','/TC','/std:c11','/W4','/WX',f'/I{ROOT/"include"}','/c',ROOT/'tests/input_c_consumer.c',f'/Fo{obj}'])
    flags=[cl,'/nologo','/std:c++20','/EHsc','/O2','/W4','/WX',f'/I{ROOT/"include"}']
    target=out/'input_tests.exe';run('core-compile',[*flags,*sources,ROOT/'tests/input_tests.cpp',obj,f'/Fe{target}','/link','/STACK:8388608']);run('core-run',[target])
    native=(ROOT/'src/main.cpp').read_text();begin=native.index('void ControllerStateCB(');opening=native.index('{',begin);depth=1;end=opening+1
    while depth:
        depth+=(native[end]=='{')-(native[end]=='}');end+=1
    menu=(ROOT/'src/menu_checker.cpp').read_text();menu=menu[menu.index('namespace MenuChecker'):]
    extracted=out/'input_native_extracted.cpp';extracted.write_text('#include "input_native_fakes.h"\n'+menu+'\n'+native[begin:end]+'\n#include "input_native_cases.h"\n')
    native_target=out/'input_native_tests.exe';run('native-compile',[*flags,'/wd4100',f'/I{ROOT/"tests"}',*sources,extracted,f'/Fe{native_target}','/link','/STACK:8388608']);run('native-run',[native_target])
    definition=out/'input_probe.def';definition.write_text('EXPORTS\n HIGGS_QueryInputInterface\n HIGGS_QueryInputContextInterface\n HIGGS_QueryHandInterface\n')
    dll=out/'input_probe.dll';run('dll-compile',[*flags,'/LD',*sources,f'/Fe{dll}','/link',f'/DEF:{definition}'])
    probe=out/'input_dll_probe.exe';run('probe-compile',[cl,'/nologo','/TC','/std:c11','/W4','/WX',f'/I{ROOT/"include"}',ROOT/'tests/input_dll_probe.c',f'/Fe{probe}']);run('probe-run',[probe,dll])
    for p in files:
        if hashlib.sha256(p.read_bytes()).hexdigest()!=pins[str(p.relative_to(ROOT))]:raise RuntimeError('Source changed: '+str(p))
    (out/'input-manifest.json').write_text(json.dumps({'commands':commands,'sourceSha256':pins,'gameStarts':0,'qualification':'Production fixed input catalog, frame observations, Broker selection and deferred/session protocol; no native/hardware qualification.'},indent=2)+'\n')
if __name__=='__main__':main()
