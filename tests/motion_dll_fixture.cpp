// Isolated-module test control ONLY. Never compiled into the native plugin.
#include "controller_tracking.h"
#include "higgsmotion.h"

extern "C" uint64_t HIGGS_MOTION_CALL MotionFixtureControl(uint32_t action,uint64_t observedAtNs) {
    using namespace HiggsMotion;
    auto& owner=Tracking();
    Event event;
    if(action==1||action==7){event.kind=EventKind::World;event.world=action==1?1:0;}
    else if(action==4)event.kind=EventKind::BeginLoad;
    else if(action==5)event.kind=EventKind::EndLoad;
    else if(action==6)event.kind=EventKind::NewGame;
    else {
        const auto ticket=owner.BeginObservation();event.expectedGeneration=ticket.generation;
        event.observationSequence=ticket.sequence;event.observedAtNs=observedAtNs;
        const Rotation rotation{{{1,0,0},{0,1,0},{0,0,1}}};
        event.hands={Convert(1,{1,2,3},{4,5,6},rotation),Convert(2,{},{},rotation)};
        if(action==3)event.hands[0].status=Status::InvalidPose;
    }
    owner.Apply(event);
    return owner.CopyObservation().generation;
}
