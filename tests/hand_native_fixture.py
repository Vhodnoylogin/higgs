"""Extract real native methods into declared seams; never model game ABI layouts."""
from pathlib import Path

def method(source, signature):
    begin=source.index(signature)
    brace=source.index('{',begin)
    depth=0
    for i in range(brace,len(source)):
        if source[i]=='{': depth+=1
        elif source[i]=='}':
            depth-=1
            if not depth:return source[begin:i+1]
    raise RuntimeError('unterminated '+signature)

def generate(root, out):
    native=(root/'src/hand_native.cpp').read_text()
    hand=(root/'src/hand.cpp').read_text()
    release_start=hand.index('const auto nativeRegionActionCurrent =')
    release_end=hand.index('if (!other.HasHeldObject() || other.selectedObject.rigidBody != selectedObject.rigidBody)',release_start)
    release=hand[release_start:release_end]
    (out/'native_regions_release_extracted.cpp').write_text(
        '#include "native_regions_release_fakes.h"\nvoid Hand::ProbeRegionRelease() {\n'
        'auto phase=HiggsHands::Service().NativePhase();\n'
        'auto* selectedObj=&reference; void* player=nullptr; NiAVObject hmd; auto* hmdNode=&hmd; NiPoint3 palmPos,handPos;\n'
        +release+'\n}\nint main(){return RunReleaseCases();}\n')
    # Verify receipt calls remain after the real keyframe/constraint mutations;
    # the full Hand::Update engine graph cannot be qualified by these seams.
    secondary_start=hand.index('state = State::HeldTwoHanded;')
    if '++synergySecondaryEpisode;' not in hand[secondary_start-100:secondary_start]:
        raise RuntimeError('secondary native lifetime lacks monotonic episode')
    for mutation in ['UpdateKeyframedNode(collidableNode, desiredTransform);',
                     'constraintData->m_atoms.m_transforms.m_transformB.m_translation = NiPointToHkVector(newPivotB);',
                     'NiMathDouble::UpdateTransform(fpWeaponNode, vrikCorrectedTransform);']:
        at=hand.index(mutation)
        if 'HiggsHands::ConfirmNativeGrip(isLeft);' not in hand[at:at+len(mutation)+100]:
            raise RuntimeError('grip receipt precedes/lacks actual actuator mutation')
    extracted=native[native.index('namespace HiggsHands'):]
    for signature in ['void Hand::ControllerStateUpdate(', 'hkTransform Hand::ComputeHandCollisionTransform(', 'void Hand::MoveHandAndWeaponCollision(']:
        extracted+='\n'+method(hand,signature)+'\n'
    extracted+='\n'+method(hand,'void Hand::UpdateNativeRegionSelection(')+'\n'
    observations=(root/'src/hand_observations_native.cpp').read_text()
    extracted+='\nnamespace HiggsHands { namespace {\n'+method(observations,'void Vector(')+'\n}\n'
    extracted+=method(observations,'void ObserveNativeSamples(')+'\n'+method(observations,'void ObserveNativeContact(')+'\n}\n'
    (out/'hand_native_extracted.cpp').write_text('#include "hand_native_fakes.h"\n'+extracted+'\n#include "secondary_grip_cases.h"\n#include "hand_native_fixture_cases.h"\n')
    support=(root/'src/hand_support_native.cpp').read_text()
    (out/'support_native_extracted.cpp').write_text('#include "support_native_fakes.h"\n'+support[support.index('namespace HiggsHands'):]+ '\n#include "support_native_cases.h"\n')
