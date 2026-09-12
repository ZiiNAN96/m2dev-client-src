// ZiiNAN: Category and main-instance gates also run in builds without Diligent.
#include "Renderer/ActorRenderData.h"
#include <iostream>
int main()
{
    using namespace Renderer;
    if(ClassifyActor(6,0)!=ActorCategory::Player || ClassifyActor(6,7)!=ActorCategory::Player ||
       ClassifyActor(1,9003)!=ActorCategory::Npc || ClassifyActor(1,20016)!=ActorCategory::Npc ||
       ClassifyActor(0,101)!=ActorCategory::Mob || ClassifyActor(0,691)!=ActorCategory::Mob) return 1;
    for(auto pair:{std::pair{6u,20114u},std::pair{1u,20114u},std::pair{1u,34001u},
                   std::pair{0u,34001u},std::pair{7u,101u},std::pair{8u,20101u},std::pair{2u,8001u}})
        if(ClassifyActor(pair.first,pair.second)!=ActorCategory::Unsupported) return 2;
    int body=0,attachment=0,left=0,hair=0,foreign=0,reservedHead=0,calls=0;
    ActorNativeDraw group{};
    const auto record=[](void* context,const void*,ActorPart,const ActorNativeDraw&) { ++*static_cast<int*>(context); };
    ActorInstanceSet bodyOnly{{&body}}, parts{{&body,&attachment,&reservedHead,&left,&hair}};
    {
        ActorDrawScope scope({bodyOnly,&calls,record});
        SubmitActorNativeDraw(&body,group); SubmitActorNativeDraw(&attachment,group);
        { ActorDrawScope nested({}); SubmitActorNativeDraw(&body,group); }
        SubmitActorNativeDraw(&body,group);
    }
    SubmitActorNativeDraw(&body,group);
    if(calls!=2 || actorDrawTarget.targets.Find(&body)!=ActorPart::Unsupported || actorDrawTarget.submit) return 3;
    // ZiiNAN: Diligent actor attachment rendering
    if(parts.Find(&attachment)!=ActorPart::Weapon || parts.Find(&left)!=ActorPart::WeaponLeft ||
       parts.Find(&hair)!=ActorPart::Hair || parts.Find(nullptr)!=ActorPart::Unsupported ||
       parts.Find(&reservedHead)!=ActorPart::Unsupported) return 4;
    {
        ActorDrawScope scope({parts,&calls,record});
        for(auto* instance:{&body,&attachment,&left,&hair,&foreign,&reservedHead}) SubmitActorNativeDraw(instance,group);
        { ActorDrawScope hidden({}); SubmitActorNativeDraw(&attachment,group); }
        SubmitActorNativeDraw(&hair,group);
    }
    if(calls!=7 || actorDrawTarget.submit) return 5;
    actorDeformTargets=bodyOnly;
    { ActorDeformScope disabled(parts); if(actorDeformTargets.Find(&body)!=ActorPart::Unsupported) return 6; }
    if(actorDeformTargets.Find(&body)!=ActorPart::Body) return 7;
    actorDeformTargets={};
    ActorModelSource rigid{3,{0,1,2},0,std::vector<StaticObjectVertex>(3)};
    if(!rigid.IsRigid()) return 8;
    rigid.deformVertexCount=1; if(rigid.IsRigid()) return 9;
    rigid.deformVertexCount=0; rigid.rigidVertices.pop_back(); if(rigid.IsRigid()) return 10;
    std::cout<<"Player/NPC/mob / companion exclusion / exact body and attachment parts / nested scopes / rigid contract: PASS\n";
}
