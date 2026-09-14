#include "Stdafx.h"
#include "AssetRuntime/Granny/Native.h"
#include "ModelInstance.h"
#include "Model.h"

void CGrannyModelInstance::MakeBoundBox(TBoundBox* pBoundBox, 
										 const float* mat, 
										 const float* OBBMin, 
										 const float* OBBMax, 
										 Math::Vector3* vtMin,
										 Math::Vector3* vtMax)
{
	pBoundBox->sx = OBBMin[0] * mat[0] + OBBMin[1] * mat[4] + OBBMin[2] * mat[8] + mat[12];
	pBoundBox->sy = OBBMin[0] * mat[1] + OBBMin[1] * mat[5] + OBBMin[2] * mat[9] + mat[13];
	pBoundBox->sz = OBBMin[0] * mat[2] + OBBMin[1] * mat[6] + OBBMin[2] * mat[10] + mat[14];

	pBoundBox->ex = OBBMax[0] * mat[0] + OBBMax[1] * mat[4] + OBBMax[2] * mat[8] + mat[12];
	pBoundBox->ey = OBBMax[0] * mat[1] + OBBMax[1] * mat[5] + OBBMax[2] * mat[9] + mat[13];
	pBoundBox->ez = OBBMax[0] * mat[2] + OBBMax[1] * mat[6] + OBBMax[2] * mat[10] + mat[14];

	vtMin->x = std::min(vtMin->x, pBoundBox->sx);
	vtMin->x = std::min(vtMin->x, pBoundBox->ex);
	vtMin->y = std::min(vtMin->y, pBoundBox->sy);
	vtMin->y = std::min(vtMin->y, pBoundBox->ey);
	vtMin->z = std::min(vtMin->z, pBoundBox->sz);
	vtMin->z = std::min(vtMin->z, pBoundBox->ez);

	vtMax->x = std::max(vtMax->x, pBoundBox->sx);
	vtMax->x = std::max(vtMax->x, pBoundBox->ex);
	vtMax->y = std::max(vtMax->y, pBoundBox->sy);
	vtMax->y = std::max(vtMax->y, pBoundBox->ey);
	vtMax->z = std::max(vtMax->z, pBoundBox->sz);
	vtMax->z = std::max(vtMax->z, pBoundBox->ez);
}

namespace {
AssetRuntime::Bounds BoneBounds(const CGrannyModel& model, int mesh, size_t bone)
{
    if (const auto* asset=model.GetAsset()) {
        if (mesh<0 || size_t(mesh)>=asset->meshes.size() || bone>=asset->meshes[mesh].skin.boneBounds.size()) return {};
        return asset->meshes[mesh].skin.boneBounds[bone];
    }
    // Explicit raw-reference construction is retained for the original SDK parity fixtures.
    const auto* native=model.GetMeshPointer(mesh)->GetGrannyMeshPointer();
    if (!native || bone>=size_t(native->BoneBindingCount)) return {};
    const auto& source=native->BoneBindings[bone];
    AssetRuntime::Bounds result;
    std::memcpy(result.min.data(),source.OBBMin,sizeof(result.min));
    std::memcpy(result.max.data(),source.OBBMax,sizeof(result.max));
    result.valid=true;
    return result;
}
}

bool CGrannyModelInstance::Intersect(const Math::Matrix* matrix, float*, float*, float* distance)
{
    if (!m_animationInstance || !distance) return false;
    float u,v,t;
    *distance=100000000.0f;
    Math::Vector3 minimum(10000000.0f,10000000.0f,10000000.0f);
    Math::Vector3 maximum(-10000000.0f,-10000000.0f,-10000000.0f);
    TBoundBox box;
    for (size_t mesh=0;mesh<m_meshBindings.size();++mesh) {
        const auto indices=m_meshBindings[mesh]->BoneIndices();
        for(size_t bone=0;bone<indices.size();++bone) {
            const auto bounds=BoneBounds(*m_pModel,static_cast<int>(mesh),bone);
            const auto* transform=GetBoneMatrixPointer(indices[bone]);
            if (!bounds.valid || !transform) return false;
            MakeBoundBox(&box,transform,bounds.min.data(),bounds.max.data(),&minimum,&maximum);
        }
    }
    return IntersectCube(matrix,minimum.x,minimum.y,minimum.z,maximum.x,maximum.y,maximum.z,
        ms_vtPickRayOrig,ms_vtPickRayDir,&u,&v,&t);
}

void CGrannyModelInstance::GetBoundBox(Math::Vector3* minimum, Math::Vector3* maximum)
{
    if (!m_animationInstance || !minimum || !maximum) return;
    TBoundBox box;
    minimum->x=minimum->y=minimum->z=100000.0f;
    maximum->x=maximum->y=maximum->z=-100000.0f;
    for (size_t mesh=0;mesh<m_meshBindings.size();++mesh) {
        const auto indices=m_meshBindings[mesh]->BoneIndices();
        for(size_t bone=0;bone<indices.size();++bone) {
            const auto bounds=BoneBounds(*m_pModel,static_cast<int>(mesh),bone);
            const auto* transform=GetBoneMatrixPointer(indices[bone]);
            if (!bounds.valid || !transform) continue;
            MakeBoundBox(&box,transform,bounds.min.data(),bounds.max.data(),minimum,maximum);
        }
    }
}

bool CGrannyModelInstance::GetMeshMatrixPointer(int mesh,const Math::Matrix** matrix) const
{
    if (!m_animationInstance || !matrix || mesh<0 || size_t(mesh)>=m_meshBindings.size()) return false;
    const auto indices=m_meshBindings[mesh]->BoneIndices();
    if (indices.empty()) return false;
    const auto* values=GetBoneMatrixPointer(indices[0]);
    if (!values) return false;
    *matrix=reinterpret_cast<const Math::Matrix*>(values);
    return true;
}