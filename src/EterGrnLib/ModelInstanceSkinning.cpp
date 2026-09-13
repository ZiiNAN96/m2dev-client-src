#include "StdAfx.h"
#include "ModelInstance.h"
#include "SkinningDataAdapter.h"
#include "EterBase/Debug.h"

// ZiiNAN BEGIN - GPU skinning bone palette preparation
void CGrannyModelInstance::__PrepareSkinningBindings()
{
    if(!m_pModel || !m_pModel->GetSkinningData() || !m_pModel->GetSkinningData()->HasSkinnedMeshes()) return;
    const auto* owner=m_ppkSkeletonInst?*m_ppkSkeletonInst:this;
    if(!owner || !owner->m_pModel || !owner->m_pModel->GetSkinningData()) {
        m_skinningStatus=Renderer::SkinDataStatus::MissingData;
        m_skinningRemaps.clear();
        return;
    }
    const auto& destination=owner->m_pModel->GetSkinningData()->skeleton;
    if(m_skinningBindingDestination==destination && m_skinningStatus!=Renderer::SkinDataStatus::DestinationChanged) return;
    const auto& source=*m_pModel->GetSkinningData();
    m_skinningStatus=Renderer::SkinDataStatus::Ready;
    // A linked LOD may replace the pose owner without recreating the native binding.
    // Accept identical bone layouts only; never silently invent a different mapping than CPU uses.
    if(m_skinningBindingDestination &&
       (m_skinningBindingDestination->names!=destination->names || m_skinningBindingDestination->parents!=destination->parents)) {
        m_skinningStatus=Renderer::SkinDataStatus::DestinationChanged;
        m_skinningRemaps.clear();
    } else {
        m_skinningRemaps.assign(source.meshes.size(),{});
        for(size_t mesh=0;mesh<source.meshes.size();++mesh) if(source.meshes[mesh]) {
            Renderer::SkinDataStatus status;
            m_skinningRemaps[mesh]=SkinningDataAdapter::ExtractRemap(*source.meshes[mesh],
                mesh<m_vct_pgrnMeshBinding.size()?m_vct_pgrnMeshBinding[mesh]:nullptr,destination,status);
            if(status!=Renderer::SkinDataStatus::Ready) m_skinningStatus=status;
        }
        m_skinningBindingDestination=destination;
    }
    if(m_skinningStatus!=Renderer::SkinDataStatus::Ready && !m_skinningIssueReported) {
        m_skinningIssueReported=true;
        if(Renderer::skinSidecarFailures.fetch_add(1)<16)
            TraceError("Skinning binding preparation: model=%s status=%s; CPU path unchanged",
                m_pModel->GetGrannyModelPointer()->Name,Renderer::SkinDataStatusName(m_skinningStatus));
    }
}

void CGrannyModelInstance::__CaptureSkinningPose()
{
    if(!m_pModel || !m_pModel->GetSkinningData() || !m_pModel->GetSkinningData()->HasSkinnedMeshes()) return;
    if(!m_skinningPalette) m_skinningPalette=std::make_shared<Renderer::BonePalette>();
    const auto status=SkinningDataAdapter::CapturePose(*m_skinningPalette,m_pModel->GetSkinningData()->skeleton,m_pgrnWorldPoseReal);
    if(status!=Renderer::SkinDataStatus::Ready && !m_skinningIssueReported) {
        m_skinningIssueReported=true;
        if(Renderer::skinSidecarFailures.fetch_add(1)<16)
            TraceError("Skinning palette preparation: model=%s status=%s; CPU path unchanged",
                m_pModel->GetGrannyModelPointer()->Name,Renderer::SkinDataStatusName(status));
    }
}

std::shared_ptr<const Renderer::BonePalette> CGrannyModelInstance::GetSkinningPalette() const
{
    if(m_skinningStatus!=Renderer::SkinDataStatus::Ready) return {};
    const auto* owner=m_ppkSkeletonInst?*m_ppkSkeletonInst:this;
    if(!owner || !owner->m_skinningPalette || !owner->m_skinningPalette->ready ||
       owner->m_skinningPalette->skeleton!=m_skinningBindingDestination) return {};
    return owner->m_skinningPalette;
}
// ZiiNAN END
