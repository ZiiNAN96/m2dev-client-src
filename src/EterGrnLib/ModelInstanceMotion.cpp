#include "StdAfx.h"
#include "ModelInstance.h"
#include "Model.h"

namespace {
const char* ModelPath(const CGrannyModel* model)
{
    return model && model->GetAssetHandle() ? model->GetAssetHandle().GetDocument()->Id().c_str() : "legacy-reference";
}
const char* ClipPath(const CGrannyMotion* motion)
{
    return motion && motion->GetAssetHandle() ? motion->GetAssetHandle().GetDocument()->Id().c_str() : "legacy-reference";
}
bool UnexpectedMotionError(AssetRuntime::AssetError result)
{
    return result != AssetRuntime::AssetError::None && result != AssetRuntime::AssetError::NoMatchingTracks;
}
}

void CGrannyModelInstance::CopyMotion(CGrannyModelInstance* source, bool freeSource)
{
    if (!source || !source->IsMotionPlaying() || !m_animationInstance || !source->m_animationInstance) return;
    const auto result = m_animationInstance->CopyMotionFrom(*source->m_animationInstance, GetLocalTime(), freeSource);
    if (UnexpectedMotionError(result))
        TraceError("Asset Runtime motion copy failed: %s model=%s sourceModel=%s",
            AssetRuntime::ErrorName(result), ModelPath(m_pModel), ModelPath(source->m_pModel));
}

bool CGrannyModelInstance::IsMotionPlaying()
{
    return m_animationInstance && m_animationInstance->IsPlaying();
}

void CGrannyModelInstance::SetMotionPointer(const CGrannyMotion* motion, float blendTime, int loopCount, float speedRatio)
{
    if (!m_ownsWorldPose || !m_animationInstance || !motion) return;
    const auto result = m_animationInstance->SetMotion(motion->GetAssetHandle(), GetLocalTime(), blendTime, loopCount, speedRatio);
    if (UnexpectedMotionError(result))
        TraceError("Asset Runtime motion start failed: %s model=%s clip=%s",
            AssetRuntime::ErrorName(result), ModelPath(m_pModel), ClipPath(motion));
}

void CGrannyModelInstance::ChangeMotionPointer(const CGrannyMotion* motion, int loopCount, float speedRatio)
{
    if (!m_animationInstance || !motion) return;
    // Preserve the existing 0.3-second interpolation skip.
    const auto result = m_animationInstance->ChangeMotion(motion->GetAssetHandle(), GetLocalTime() - 0.3f, loopCount, speedRatio);
    if (UnexpectedMotionError(result))
        TraceError("Asset Runtime motion change failed: %s model=%s clip=%s",
            AssetRuntime::ErrorName(result), ModelPath(m_pModel), ClipPath(motion));
}

void CGrannyModelInstance::SetMotionAtEnd()
{
    if (m_animationInstance) m_animationInstance->SetMotionAtEnd();
}
