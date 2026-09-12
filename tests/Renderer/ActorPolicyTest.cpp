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
    int body=0,attachment=0,calls=0;
    ActorNativeDraw group{};
    const auto record=[](void* context,const ActorNativeDraw&) { ++*static_cast<int*>(context); };
    {
        ActorDrawScope scope({&body,&calls,record});
        SubmitActorNativeDraw(&body,group); SubmitActorNativeDraw(&attachment,group);
        { ActorDrawScope nested({}); SubmitActorNativeDraw(&body,group); }
        SubmitActorNativeDraw(&body,group);
    }
    SubmitActorNativeDraw(&body,group);
    if(calls!=2 || actorDrawTarget.instance || actorDrawTarget.submit) return 3;
    std::cout<<"Player/NPC/mob / companion exclusion / exact body / nested scope restoration: PASS\n";
}
