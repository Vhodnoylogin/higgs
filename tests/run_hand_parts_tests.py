"""Compile production hand-part service, copied C ABI and actual native phase source."""
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
    parser=argparse.ArgumentParser();parser.add_argument('--build-dir',type=Path,required=True);out=parser.parse_args().build_dir.resolve()
    if out==ROOT or ROOT in out.parents:parser.error('External output required')
    out.mkdir(parents=True,exist_ok=True);env=msvc_environment();cl=shutil.which('cl',path=env.get('PATH',env.get('Path')))
    cmake=(ROOT/'cmake/HiggsServices.cmake').read_text();sources=[ROOT/p for p in re.findall(r'"\$\{HIGGS_SERVICE_ROOT\}/(src/[^"\n]+\.cpp)"',cmake)]
    files=sources+[ROOT/p for p in ['include/higgshandparts.h','include/higgshandpartsnative.h','include/hand_parts.h','include/hand_parts_native.h','src/hand_parts_native.cpp','src/physics.cpp','src/hand_observations_native.cpp','cmake/HiggsServices.cmake','tests/hand_parts_tests.cpp','tests/hand_parts_c_consumer.c','tests/hand_parts_native_fakes.h','tests/hand_parts_native_cases.h','tests/hand_parts_dll_probe.c']]+[Path(__file__)]
    pins={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files};commands=[]
    def run(name,args):
        argv=list(map(str,args));r=subprocess.run(argv,cwd=out,env=env,capture_output=True);(out/(name+'.log')).write_bytes(r.stdout+r.stderr);commands.append({'argv':argv,'exitCode':r.returncode})
        if r.returncode:raise RuntimeError(name+' failed; see '+str(out/(name+'.log')))
        print(name+': passed',flush=True)
    obj=out/'parts_c.obj';run('c-compile',[cl,'/nologo','/TC','/std:c11','/W4','/WX',f'/I{ROOT/"include"}','/c',ROOT/'tests/hand_parts_c_consumer.c',f'/Fo{obj}'])
    flags=[cl,'/nologo','/std:c++20','/EHsc','/O2','/W4','/WX',f'/I{ROOT/"include"}']
    target=out/'hand_parts_tests.exe';run('core-compile',[*flags,*sources,ROOT/'tests/hand_parts_tests.cpp',obj,f'/Fe{target}','/link','/STACK:8388608']);run('core-run',[target])
    native=(ROOT/'src/hand_parts_native.cpp').read_text();native='\n'.join(line for line in native.splitlines() if not line.startswith('#include'))
    def function(text,signature):
        begin=text.index(signature);opening=text.index('{',begin);depth=1;end=opening+1
        while depth:
            depth+=(text[end]=='{')-(text[end]=='}');end+=1
        return text[begin:end]
    physics=(ROOT/'src/physics.cpp').read_text();classification='\n'.join(function(physics,s) for s in ['uint32_t OwnedHandPartSide(', 'inline bool IsLeftRigidBody(', 'inline bool IsRightRigidBody(', 'inline bool IsWeaponRigidBody(', 'inline bool IsHandRigidBody('])
    contact=function((ROOT/'src/hand_observations_native.cpp').read_text(),'void ObserveNativeContact(')
    vector='namespace HiggsHands {void Vector(float* p,const hkVector4& v,float scale=1){for(unsigned i=0;i<3;++i)p[i]=v(i)*scale;}\n'
    extracted=out/'parts_native_extracted.cpp';extracted.write_text('#include "hand_parts_native_fakes.h"\n'+native+'\n'+classification+'\n'+vector+contact+'\n}\n#include "hand_parts_native_cases.h"\n')
    ntarget=out/'hand_parts_native_tests.exe';run('native-compile',[*flags,f'/I{ROOT/"tests"}',*sources,extracted,f'/Fe{ntarget}','/link','/STACK:8388608']);run('native-run',[ntarget])
    definition=out/'parts_probe.def';definition.write_text('EXPORTS\n HIGGS_QueryHandPartsInterface\n HIGGS_QueryHandPartsNativeInterface\n HIGGS_QueryHandInterface\n')
    dll=out/'parts_probe.dll';run('dll-compile',[*flags,'/LD',*sources,f'/Fe{dll}','/link',f'/DEF:{definition}'])
    probe=out/'parts_probe.exe';run('probe-compile',[cl,'/nologo','/TC','/std:c11','/W4','/WX',f'/I{ROOT/"include"}',ROOT/'tests/hand_parts_dll_probe.c',f'/Fe{probe}']);run('probe-run',[probe,dll])
    for p in files:
        if hashlib.sha256(p.read_bytes()).hexdigest()!=pins[str(p.relative_to(ROOT))]:raise RuntimeError('Source changed: '+str(p))
    (out/'hand-parts-manifest.json').write_text(json.dumps({'commands':commands,'sourceSha256':pins,'gameStarts':0,'qualification':'Production BOX-set protocol and extracted actual native phase with synthetic engine seams; no game qualification.'},indent=2)+'\n')
if __name__=='__main__':main()
