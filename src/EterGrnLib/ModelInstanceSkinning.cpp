#include "StdAfx.h"
#include "EterBase/MapLoadTrace.h"
#include "ModelInstance.h"
#include "SkinningDataAdapter.h"
#include "EterBase/Debug.h"

// ZiiNAN: GPU skinning actor coverage - update native and GPU hair remaps together.
bool CGrannyModelInstance::__RefreshLinkedLodBinding()
{
    if(!m_linkedLodBindingDestination) return true;
    const auto fail=[&]() {
        m_skinningStatus=Renderer::SkinDataStatus::DestinationChanged;
        if(!m_skinningIssueReported) {
            m_skinningIssueReported=true;
            if(Renderer::skinSidecarFailures.fetch_add(1)<16)
                TraceError("Hair LOD binding refresh failed; current pose rejected before native/GPU deformation");
        }
        return false;
    };
    auto* owner=m_ppkSkeletonInst ? *m_ppkSkeletonInst : nullptr;
    if(!owner || !owner->m_pModel || !owner->m_pModel->GetSkinningData() ||
       !m_pModel || !m_pModel->GetSkinningData()) return fail();
    const auto& destination=owner->m_pModel->GetSkinningData()->skeleton;
    if(!destination) return fail();
    if(m_linkedLodBindingDestination==destination) return true;
    if(m_linkedLodBindingDestination->names==destination->names &&
       m_linkedLodBindingDestination->parents==destination->parents) {
        m_linkedLodBindingDestination=destination;
        return true;
    }
    std::vector<std::unique_ptr<AssetRuntime::MeshBinding>> next;
    try {
        const auto& data=*m_pModel->GetSkinningData();
        if(!owner->m_animationInstance || m_pModel->GetMeshCount()!=data.meshes.size()) return fail();
        next.resize(data.meshes.size());
        std::vector<std::shared_ptr<const Renderer::BoneRemap>> remaps;
        if(data.HasSkinnedMeshes()) remaps.resize(data.meshes.size());
        for(size_t m=0;m<next.size();++m) {
            auto binding=owner->m_animationInstance->CreateMeshBinding(m_pModel->GetAssetHandle(),m);
            if(!binding) return fail();
            const auto indices=binding->BoneIndices();
            for(const auto bone:indices)
                if(bone<0 || size_t(bone)>=destination->names.size()) return fail();
            if(data.meshes[m]) {
                Renderer::SkinDataStatus status;
                remaps[m]=SkinningDataAdapter::ExtractRemap(*data.meshes[m],indices,destination,status);
                if(status!=Renderer::SkinDataStatus::Ready) return fail();
            }
            next[m]=std::move(binding);
        }
        m_meshBindings.swap(next);
        m_skinningRemaps=std::move(remaps);
        m_skinningBindingDestination=data.HasSkinnedMeshes() ? destination : nullptr;
        m_linkedLodBindingDestination=destination;
        m_skinningStatus=data.HasSkinnedMeshes() ? Renderer::SkinDataStatus::Ready : Renderer::SkinDataStatus::Empty;
        return true;
    } catch(...) { return fail(); }
}

// ZiiNAN BEGIN - GPU skinning bone palette preparation
void CGrannyModelInstance::__PrepareSkinningBindings()
{
    MapLoadTrace::FirstUseScope trace("GPU skinning binding preparation");
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
                mesh<m_meshBindings.size() && m_meshBindings[mesh] ? m_meshBindings[mesh]->BoneIndices() : std::span<const int32_t>{},destination,status);
            if(status!=Renderer::SkinDataStatus::Ready) m_skinningStatus=status;
        }
        m_skinningBindingDestination=destination;
    }
    if(m_skinningStatus!=Renderer::SkinDataStatus::Ready && !m_skinningIssueReported) {
        m_skinningIssueReported=true;
        if(Renderer::skinSidecarFailures.fetch_add(1)<16)
            TraceError("Skinning binding preparation: model=%s status=%s; CPU path unchanged",
                m_pModel->GetAsset() ? m_pModel->GetAsset()->name.c_str() : "legacy-reference",Renderer::SkinDataStatusName(m_skinningStatus));
    }
}

void CGrannyModelInstance::__CaptureSkinningPose()
{
    MapLoadTrace::FirstUseScope trace("GPU skinning palette preparation");
    if(!m_pModel || !m_pModel->GetSkinningData() || !m_pModel->GetSkinningData()->HasSkinnedMeshes()) return;
    if(!m_skinningPalette) m_skinningPalette=std::make_shared<Renderer::BonePalette>();
    const auto status=SkinningDataAdapter::CapturePose(*m_skinningPalette,m_pModel->GetSkinningData()->skeleton,
        m_ownsWorldPose && m_animationInstance ? m_animationInstance->CompositePose() : AssetRuntime::PoseView{});
    if(status!=Renderer::SkinDataStatus::Ready && !m_skinningIssueReported) {
        m_skinningIssueReported=true;
        if(Renderer::skinSidecarFailures.fetch_add(1)<16)
            TraceError("Skinning palette preparation: model=%s status=%s; CPU path unchanged",
                m_pModel->GetAsset() ? m_pModel->GetAsset()->name.c_str() : "legacy-reference",Renderer::SkinDataStatusName(status));
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
