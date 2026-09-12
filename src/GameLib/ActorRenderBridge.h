#pragma once
// ZiiNAN: Diligent actor attachment rendering
#include "Renderer/ActorRenderData.h"
class CActorInstance;
bool IsDiligentActorCandidate(CActorInstance&);
Renderer::ActorInstanceSet GetAnimatedActorParts(CActorInstance&);
Renderer::ActorDrawTarget MakeAnimatedActorTarget(CActorInstance&);
Renderer::ActorMountPair MakeAnimatedMountPair(CActorInstance& rider, CActorInstance* mount);
