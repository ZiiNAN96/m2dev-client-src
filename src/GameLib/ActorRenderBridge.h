#pragma once
// ZiiNAN: Scoped native main-model submission shared by player, NPC and mob.
#include "Renderer/ActorRenderData.h"
class CActorInstance;
bool IsDiligentActorCandidate(CActorInstance&);
Renderer::ActorDrawTarget MakeAnimatedActorTarget(CActorInstance&);
