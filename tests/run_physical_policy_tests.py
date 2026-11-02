"""Production owned physical policies, copied C ABI and extracted native adapter."""
import argparse,hashlib,json,re,shutil,subprocess,sys
from pathlib import Path
sys.dont_write_bytecode=True
from run_tests import msvc_environment
ROOT=Path(__file__).resolve().parents[1]
def main():
    parser=argparse.ArgumentParser();parser.add_argument('--build-dir',type=Path,required=True);out=parser.parse_args().build_dir.resolve()
    if out==ROOT or ROOT in out.parents:parser.error('External output required')
    out.mkdir(parents=True,exist_ok=True);env=msvc_environment();cl=shutil.which('cl',path=env.get('PATH',env.get('Path')))
    sources=[ROOT/p for p in re.findall(r'"\$\{HIGGS_SERVICE_ROOT\}/(src/[^"\n]+\.cpp)"',(ROOT/'cmake/HiggsServices.cmake').read_text())]
    files=sources+[ROOT/p for p in ['include/higgsphysicalpolicy.h','include/hand_physical_policy.h','src/hand_physical_policy_native.cpp','src/hand_native.cpp','src/hand.cpp','src/finger_animator.cpp','include/higgshandoperations.h','tests/physical_policy_tests.cpp','tests/physical_policy_c_consumer.c','tests/physical_policy_native_fakes.h','tests/physical_policy_native_cases.h','tests/physical_policy_dll_probe.c','cmake/HiggsServices.cmake']]+[Path(__file__)]
    files+= [ROOT/p for p in re.findall(r'"\$\{HIGGS_SERVICE_ROOT\}/(include/[^"\n]+\.h)"',(ROOT/'cmake/HiggsServices.cmake').read_text())]+[ROOT/'include/hand.h']
    files=list(dict.fromkeys(files))
    pins={p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in files};commands=[]
    def run(name,argv):
        argv=list(map(str,argv));result=subprocess.run(argv,cwd=out,env=env,capture_output=True);(out/(name+'.log')).write_bytes(result.stdout+result.stderr);commands.append({'command':argv,'exitCode':result.returncode})
        if result.returncode:raise RuntimeError(name+' failed; '+str(out/(name+'.log')))
        print(name+': PASS',flush=True)
    flags=[cl,'/nologo','/std:c++20','/EHsc','/O2','/W4','/WX',f'/I{ROOT/"include"}'];cflags=[cl,'/nologo','/std:c11','/TC','/W4','/WX',f'/I{ROOT/"include"}']
    obj=out/'policy_c.obj';run('c-compile',[*cflags,'/c',ROOT/'tests/physical_policy_c_consumer.c',f'/Fo{obj}'])
    core=out/'policy_tests.exe';run('core-compile',[*flags,*sources,ROOT/'tests/physical_policy_tests.cpp',obj,f'/Fe{core}']);run('core-run',[core])
    native=(ROOT/'src/hand_physical_policy_native.cpp').read_text();extracted=out/'policy_native.cpp';extracted.write_text('#include "physical_policy_native_fakes.h"\n'+native[native.index('namespace HiggsHands'):]+ '\n#include "physical_policy_native_cases.h"\n')
    target=out/'policy_native.exe';run('native-compile',[*flags,f'/I{ROOT/"tests"}',*sources,extracted,f'/Fe{target}']);run('native-run',[target])
    hand=(ROOT/'src/hand.cpp').read_text();finger=(ROOT/'src/finger_animator.cpp').read_text();update=finger[finger.index('void FingerAnimator::Update()'):]
    if not update.index('SuppressHiggsFingers(isLeft)')<update.index('FillFingerNodes('):raise RuntimeError('Finger suppression must precede node writes')
    at=hand.index('bool Hand::ShouldUsePhysicsBasedGrab(');end=hand.index('NiPointer<bhkRigidBody> Hand::GetRigidBodyToGrab',at);grab=hand[at:end]
    if not grab.index('PhysicsGrabPreference')<grab.index('DoesNodeHaveConstraint'):raise RuntimeError('Grab preference must preserve actor/constraint safety')
    drop=hand.index('HiggsHands::PreparePhysicalPolicyRelease(isLeft);');restore=hand.index('selectedObject.savedInverseInertias.clear();',drop)
    if drop>restore:raise RuntimeError('Policy restore must precede native drop cleanup')
    definition=out/'policy.def';definition.write_text('EXPORTS\n HIGGS_QueryPhysicalPolicyInterface\n');dll=out/'policy.dll';run('dll-compile',[*flags,'/LD',*sources,f'/Fe{dll}','/link',f'/DEF:{definition}'])
    probe=out/'policy_probe.exe';run('probe-compile',[*cflags,ROOT/'tests/physical_policy_dll_probe.c',f'/Fe{probe}']);run('probe-run',[probe,dll])
    for p in files:
        if hashlib.sha256(p.read_bytes()).hexdigest()!=pins[p.relative_to(ROOT).as_posix()]:raise RuntimeError('Source changed during tests: '+str(p))
    (out/'physical-policy-manifest.json').write_text(json.dumps({'commands':commands,'sourceSha256':pins,'nativePluginBuilt':False,'gameStarts':0,'qualification':'Production service/C ABI plus actual native adapter on synthetic seams; actual SDK build separate; no game qualification.'},indent=2)+'\n')
if __name__=='__main__':main()
