#pragma once
// ZiiNAN: Existing visible actor main-body submission; no animation or attachment traversal.
class CActorInstance;
void SubmitAnimatedActorBody(CActorInstance&);
void ReportAnimatedActorExclusion(CActorInstance&, const char*);
