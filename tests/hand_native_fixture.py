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
    # Verify receipt calls remain after the real keyframe/constraint mutations;
    # the full Hand::Update engine graph cannot be qualified by these seams.
    for mutation in ['UpdateKeyframedNode(collidableNode, desiredTransform);',
                     'constraintData->m_atoms.m_transforms.m_transformB.m_translation = NiPointToHkVector(newPivotB);']:
        at=hand.index(mutation)
        if 'HiggsHands::ConfirmNativeGrip(isLeft);' not in hand[at:at+len(mutation)+100]:
            raise RuntimeError('grip receipt precedes/lacks actual actuator mutation')
    extracted=native[native.index('namespace HiggsHands'):]
    for signature in ['void Hand::ControllerStateUpdate(', 'hkTransform Hand::ComputeHandCollisionTransform(', 'void Hand::MoveHandAndWeaponCollision(']:
        extracted+='\n'+method(hand,signature)+'\n'
    (out/'hand_native_extracted.cpp').write_text('#include "hand_native_fakes.h"\n'+extracted+'\n#include "hand_native_fixture_cases.h"\n')
