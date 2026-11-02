"""Production own-item contact C/core and actual native capture on explicit engine seams."""
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
    parser=argparse.ArgumentParser();parser.add_argument('--build-dir',required=True,type=Path);args=parser.parse_args();out=args.build_dir.resolve()
    if out==ROOT or ROOT in out.parents:parser.error('External output required')
    out.mkdir(parents=True,exist_ok=True);env=msvc_environment();env['PYTHONDONTWRITEBYTECODE']='1';cl=shutil.which('cl',path=env['PATH']);commands=[]
    closure=(ROOT/'cmake/HiggsServices.cmake').read_text();sources=[ROOT/p for p in re.findall(r'"\$\{HIGGS_SERVICE_ROOT\}/(src/[^"\n]+\.cpp)"',closure)]
    headers=[ROOT/p for p in re.findall(r'"\$\{HIGGS_SERVICE_ROOT\}/(include/[^"\n]+\.h)"',closure)]
    files=list(dict.fromkeys(sources+headers+[ROOT/p for p in ['src/own_contact_native.cpp','src/hand_native.cpp','src/hand.cpp','include/hand.h','tests/own_contact_native_fakes.h','tests/own_contact_native_cases.h','tests/own_contact_c_consumer.c','tests/run_own_contact_tests.py','cmake/HiggsServices.cmake','exports.def','higgs_vr.vcxproj']]))
    pins={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    def run(name,args,negative=False):
        args=list(map(str,args));r=subprocess.run(args,cwd=out,env=env,capture_output=True);(out/(name+'.log')).write_bytes(r.stdout+r.stderr);commands.append({'name':name,'argv':args,'exitCode':r.returncode,'negative':negative})
        if (r.returncode==0)==negative:raise RuntimeError(name+' failed; '+str(out/(name+'.log')))
        print(name+': passed',flush=True)
    cobj=out/'consumer.obj';run('c',[cl,'/nologo','/TC','/std:c11','/W4','/WX',f'/I{ROOT/"include"}','/c',ROOT/'tests/own_contact_c_consumer.c',f'/Fo{cobj}'])
    native=(ROOT/'src/own_contact_native.cpp').read_text();native=native[native.index('namespace HiggsHands {'):]
    layout='static_assert(offsetof(EventDispatcherList,unk4D0)==0x478);'
    if native.count(layout)!=1:raise RuntimeError('Native SDK event-source layout seam changed')
    native=native.replace(layout,'// Actual SDK build verifies holder offset; these engine doubles do not model ABI layout.')
    fixture=out/'own_contact_native.cpp';fixture.write_text('#include "own_contact_native_fakes.h"\n'+native+'\n#include "own_contact_native_cases.h"\n')
    flags=[cl,'/nologo','/std:c++20','/EHsc','/O2','/W4','/WX',f'/I{ROOT/"include"}',f'/I{ROOT/"tests"}']
    exe=out/'own_contact.exe';run('compile',[*flags,*sources,fixture,cobj,f'/Fe{exe}']);run('production',[exe])
    # Remove the common commit ticket: the injected equip invalidation must then
    # expose the old copied source frame and fail the actual-source assertion.
    core=(ROOT/'src/own_contact.cpp').read_text();needle='if(frame.invalidationTicket!=invalidationTicket_.load()||frame.invalidationTicket==UINT64_MAX||'
    if core.count(needle)!=1:raise RuntimeError('Commit fence negative seam changed')
    broken=out/'own_contact_unfenced.cpp';broken.write_text(core.replace(needle,'if(false||'))
    bad=out/'own_contact_unfenced.exe';replacement=[broken if p.name=='own_contact.cpp' else p for p in sources];run('negative-compile',[*flags,*replacement,fixture,cobj,f'/Fe{bad}']);run('negative-unfenced-publish',[bad],True)
    changed=[str(p.relative_to(ROOT)) for p in files if hashlib.sha256(p.read_bytes()).hexdigest()!=pins[str(p.relative_to(ROOT))]]
    if changed:raise RuntimeError('Inputs changed during tests: '+','.join(changed))
    (out/'manifest.json').write_text(json.dumps({'passed':True,'commands':commands,'sourceSha256':pins,'nativeLayoutCheck':'TESEquipEvent18/sourceholder478 compiled separately with actual SDK; omitted from explicit engine doubles','gameStarts':0,'qualification':'Actual portable C/core and native snapshot/resolver source on explicit engine seams, including injected publication invalidation. No game, physics response or audio/hardware qualification.'},indent=2)+'\n')
if __name__=='__main__':main()
