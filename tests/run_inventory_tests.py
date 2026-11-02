"""Actual native inventory source + production C/coordinator service on explicit SDK doubles."""
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
    out.mkdir(parents=True,exist_ok=True);env=msvc_environment();cl=shutil.which('cl',path=env['PATH']);commands=[]
    sources=[ROOT/p for p in re.findall(r'"\$\{HIGGS_SERVICE_ROOT\}/(src/[^"\n]+\.cpp)"',(ROOT/'cmake/HiggsServices.cmake').read_text())]
    headers=[ROOT/p for p in re.findall(r'"\$\{HIGGS_SERVICE_ROOT\}/(include/[^"\n]+\.h)"',(ROOT/'cmake/HiggsServices.cmake').read_text())]
    paths=list(dict.fromkeys(sources+headers+[ROOT/p for p in ['include/higgsinventory.h','include/synergy_inventory_native.h','src/synergy_inventory_native.cpp','src/hand_native.cpp','src/hand_transactions.cpp','include/hand_transactions.h','tests/inventory_c_consumer.c','tests/inventory_native_fakes.h','tests/inventory_native_cases.h','tests/run_inventory_tests.py','cmake/HiggsServices.cmake','higgs_vr.vcxproj','exports.def','docs/native-inventory.md']]))
    pins={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}
    def run(name,argv,negative=False):
        argv=list(map(str,argv));r=subprocess.run(argv,cwd=out,env=env,capture_output=True);(out/(name+'.log')).write_bytes(r.stdout+r.stderr);commands.append({'name':name,'argv':argv,'exitCode':r.returncode,'negative':negative})
        if (r.returncode==0)==negative:raise RuntimeError(name+' unexpected result; '+str(out/(name+'.log')))
        print(name+': passed',flush=True)
    flags=[cl,'/nologo','/std:c++20','/EHsc','/O2','/W4','/WX',f'/I{ROOT/"include"}',f'/I{ROOT/"tests"}']
    obj=out/'inventory_c.obj';run('c',[cl,'/nologo','/TC','/std:c11','/W4','/WX',f'/I{ROOT/"include"}','/c',ROOT/'tests/inventory_c_consumer.c',f'/Fo{obj}'])
    native=(ROOT/'src/synergy_inventory_native.cpp').read_text();native=native[native.index('namespace HiggsHands {'):]
    fixture=out/'inventory_native.cpp';fixture.write_text('#include "inventory_native_fakes.h"\n'+native+'\n#include "inventory_native_cases.h"\n')
    exe=out/'inventory_tests.exe';run('compile',[*flags,*sources,fixture,obj,f'/Fe{exe}']);run('production',[exe])
    # A count delta without a matching engine container event must not commit.
    mutation='outgoing!=op.quantity||incoming'
    if native.count(mutation)!=1:raise RuntimeError('Postfact negative seam changed')
    broken=out/'inventory_missing_event_guard.cpp';broken.write_text('#include "inventory_native_fakes.h"\n'+native.replace(mutation,'false')+'\n#include "inventory_native_cases.h"\n')
    bad=out/'inventory_missing_event_guard.exe';run('negative-compile',[*flags,*sources,broken,obj,f'/Fe{bad}']);run('negative-missing-event-guard',[bad],True)
    # Each boundary matters: pre-permit refusal protects authority, while the
    # second fresh read protects against a change during permit admission.
    for name,bank in [('crosshand-pre','before'),('crosshand-final','fresh')]:
        guard='if(op.kind==HIGGS_TRANSFER_EQUIP_GAME&&CrossHandEquipment(op,'+bank+',list))return {HIGGS_HAND_UNSUPPORTED};'
        if native.count(guard)!=1:raise RuntimeError('Cross-hand negative seam changed: '+name)
        broken=out/(name+'.cpp');broken.write_text('#include "inventory_native_fakes.h"\n'+native.replace(guard,'')+'\n#include "inventory_native_cases.h"\n')
        bad=out/(name+'.exe');run(name+'-compile',[*flags,*sources,broken,obj,f'/Fe{bad}']);run(name+'-removed',[bad],True)
    for p in paths:
        if hashlib.sha256(p.read_bytes()).hexdigest()!=pins[str(p.relative_to(ROOT))]:raise RuntimeError('Source changed during tests '+str(p))
    (out/'inventory-manifest.json').write_text(json.dumps({'commands':commands,'sourceSha256':pins,'gameStarts':0,'qualification':'Actual copied C ABI, production ticket/coordinator service and actual native adapter source on explicit SDK doubles. Not Skyrim event delivery, vtable/runtime behavior or consumption effect qualification.'},indent=2)+'\n')
if __name__=='__main__':main()
