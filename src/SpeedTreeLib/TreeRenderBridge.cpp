// ZiiNAN: Diligent SpeedTree rendering integration; synchronous CPU data and read-only legacy state.
#include "StdAfx.h"
#include "EterLib/DrawStateView.h"
#include "TreeRenderBridge.h"
#include "SpeedTreeWrapper.h"
#include "EterLib/DrawState.h"
#include "EterLib/StaticObjectTextureLoader.h"
#include <fstream>
#include <set>

namespace
{
using namespace Renderer;
CGraphicImage* cameraMask=nullptr;
std::ofstream diagnostics;
std::set<std::string> reports;
void Report(const TreeModelData& data,const std::string& status,bool error=false)
{
    if(error && treeRenderer) treeRenderer->ReportFailure();
    if(!reports.insert(data.asset+status).second) return;
    if(!diagnostics.is_open()) diagnostics.open("tree-renderer.log",std::ios::trunc);
    diagnostics << (error ? "ERROR: " : "") << status << " asset=" << data.asset << std::endl;
}
DWORD Stage(DWORD index,Renderer::TextureStageKey type)
{ DWORD result=0; DRAWSTATE.GetTextureStageState(index,type,&result); return result; }
float StateFloat(Renderer::RenderStateKey type)
{ DWORD bits=DRAWSTATE.GetRenderState(type); float value; memcpy(&value,&bits,4); return value; }
bool Sampling(DWORD stage,TreeSampler& sampler)
{
    const auto get=[&](Renderer::SamplerStateKey type) { DWORD value=0; return SUCCEEDED(DrawStateView().GetSamplerState(stage,type,&value)) ? value : ~DWORD(0); };
    const auto u=get(Renderer::SamplerAddressU),v=get(Renderer::SamplerAddressV),min=get(Renderer::SamplerMinFilter),mag=get(Renderer::SamplerMagFilter),mip=get(Renderer::SamplerMipFilter);
    if((u!=Renderer::AddressWrap && u!=Renderer::AddressClamp) || (v!=Renderer::AddressWrap && v!=Renderer::AddressClamp) ||
       min<Renderer::FilterPoint || min>Renderer::FilterAnisotropic || mag<Renderer::FilterPoint || mag>Renderer::FilterAnisotropic ||
       mip>Renderer::FilterLinear || get(Renderer::SamplerMaxMipLevel)!=0 || get(Renderer::SamplerMipMapLodBias)!=0) return false;
    sampler.sampling={u==Renderer::AddressWrap,v==Renderer::AddressWrap,min==Renderer::FilterLinear,mag==Renderer::FilterLinear,mip==Renderer::FilterLinear,mip!=Renderer::FilterNone};
    sampler.anisotropic=min==Renderer::FilterAnisotropic || mag==Renderer::FilterAnisotropic;
    if(sampler.anisotropic) {
        sampler.maxAnisotropy=get(Renderer::SamplerMaxAnisotropy);
        if(min!=Renderer::FilterAnisotropic || mag!=Renderer::FilterAnisotropic || mip!=Renderer::FilterLinear ||
           sampler.maxAnisotropy<1 || sampler.maxAnisotropy>16) return false;
    }
    return true;
}
bool CaptureState(TreeDraw& draw,bool hasSecond)
{
    draw.blend=DRAWSTATE.GetRenderState(Renderer::StateAlphaBlendEnable)!=FALSE;
    draw.depthTest=DRAWSTATE.GetRenderState(Renderer::StateZEnable)!=FALSE;
    draw.depthWrite=DRAWSTATE.GetRenderState(Renderer::StateZWriteEnable)!=FALSE;
    draw.depthFunction=DRAWSTATE.GetRenderState(Renderer::StateZFunc);
    draw.cull=DRAWSTATE.GetRenderState(Renderer::StateCullMode)-1;
    draw.alphaTest=DRAWSTATE.GetRenderState(Renderer::StateAlphaTestEnable)!=FALSE;
    draw.alphaReference=DRAWSTATE.GetRenderState(Renderer::StateAlphaRef);
    if(draw.cull>2 || draw.depthFunction<1 || draw.depthFunction>8 || draw.alphaReference>255 ||
       (draw.alphaTest && DRAWSTATE.GetRenderState(Renderer::StateAlphaFunc)!=Renderer::CompareGreater) ||
       (draw.blend && (DRAWSTATE.GetRenderState(Renderer::StateSrcBlend)!=Renderer::BlendSrcAlpha ||
        DRAWSTATE.GetRenderState(Renderer::StateDestBlend)!=Renderer::BlendInvSrcAlpha ||
        DRAWSTATE.GetRenderState(Renderer::StateBlendOp)!=Renderer::BlendOpAdd || DRAWSTATE.GetRenderState(Renderer::StateSeparateAlphaBlendEnable)))) return false;
    if(DRAWSTATE.GetRenderState(Renderer::StateLighting) || Stage(0,Renderer::StageColorOp)!=Renderer::TextureOpModulate ||
       Stage(0,Renderer::StageColorArg1)!=Renderer::ArgTexture || Stage(0,Renderer::StageColorArg2)!=Renderer::ArgDiffuse ||
       Stage(0,Renderer::StageAlphaOp)!=Renderer::TextureOpModulate || Stage(0,Renderer::StageAlphaArg1)!=Renderer::ArgTexture ||
       Stage(0,Renderer::StageAlphaArg2)!=Renderer::ArgDiffuse || Stage(0,Renderer::StageTexCoordIndex)!=0 ||
       Stage(0,Renderer::StageTextureTransformFlags)!=Renderer::TexTransformDisable) return false;
    if(hasSecond) {
        if(Stage(1,Renderer::StageColorOp)==Renderer::TextureOpModulate && Stage(1,Renderer::StageColorArg1)==Renderer::ArgTexture &&
           Stage(1,Renderer::StageColorArg2)==Renderer::ArgCurrent && Stage(1,Renderer::StageAlphaOp)==Renderer::TextureOpDisable) draw.stage1=1;
        else if(Stage(1,Renderer::StageColorOp)==Renderer::TextureOpSelectArg1 && Stage(1,Renderer::StageColorArg1)==Renderer::ArgCurrent &&
                Stage(1,Renderer::StageAlphaOp)==Renderer::TextureOpModulate && Stage(1,Renderer::StageAlphaArg1)==Renderer::ArgTexture &&
                Stage(1,Renderer::StageAlphaArg2)==Renderer::ArgCurrent) draw.stage1=2;
        else return false;
        const auto coordinates=Stage(1,Renderer::StageTexCoordIndex),transform=Stage(1,Renderer::StageTextureTransformFlags);
        if(coordinates==Renderer::StageTciCameraSpacePosition && transform==Renderer::TexTransformCount2) draw.cameraCoordinates=true;
        else if(coordinates!=1 || transform!=Renderer::TexTransformDisable) return false;
    }
    for(unsigned stage=0;stage<2;++stage) if((stage==0 || hasSecond) && !Sampling(stage,draw.samplers[stage])) return false;
    Math::Matrix matrix;
    for(auto entry:{std::pair<Renderer::MatrixSlot,std::array<float,16>*>(Renderer::MatrixWorld,&draw.matrices.world),
                    {Renderer::MatrixView,&draw.matrices.view},{Renderer::MatrixProjection,&draw.matrices.projection},{Renderer::MatrixTexture1,&draw.textureTransform}}) {
        DRAWSTATE.GetTransform(entry.first,&matrix); memcpy(entry.second->data(),&matrix,64);
    }
    if(draw.part==TreePart::Leaf && FAILED(DrawStateView().GetVertexConstants(0,draw.legacyConstants[0].data(),96))) return false;
    if(DRAWSTATE.GetRenderState(Renderer::StateFogEnable)) {
        if(DRAWSTATE.GetRenderState(Renderer::StateFogTableMode)!=Renderer::FogNone) return false;
        draw.fog=draw.part==TreePart::Leaf ? 4u : DRAWSTATE.GetRenderState(Renderer::StateFogVertexMode);
        if(draw.fog>4) return false;
        draw.rangeFog=DRAWSTATE.GetRenderState(Renderer::StateRangeFogEnable)!=FALSE;
        draw.fogParameters={StateFloat(Renderer::StateFogStart),StateFloat(Renderer::StateFogEnd),StateFloat(Renderer::StateFogDensity),0};
        if(draw.fog==3 && draw.fogParameters[0]==draw.fogParameters[1]) return false;
        const Math::Color color(DRAWSTATE.GetRenderState(Renderer::StateFogColor)); draw.fogColor={color.r,color.g,color.b,color.a};
    }
    return true;
}
void IndexedSource(const CSpeedTreeRT::SGeometry::SIndexed& native,TreeSource& source)
{
    source.vertices.resize(native.m_usVertexCount);
    for(size_t i=0;i<source.vertices.size();++i) {
        auto& v=source.vertices[i]; memcpy(v.position.data(),native.m_pCoords+i*3,12);
        v.color=native.m_pColors[i]; memcpy(v.uv.data(),native.m_pTexCoords0+i*2,8);
        memcpy(v.shadowUv.data(),native.m_pTexCoords1+i*2,8);
    }
    for(unsigned s=0;s<native.m_usNumStrips;++s)
        source.indices.insert(source.indices.end(),native.m_pStrips[s],native.m_pStrips[s]+native.m_pStripLengths[s]);
}
}
TreeCameraMaskScope::TreeCameraMaskScope(CGraphicImage* mask):previous(cameraMask) { cameraMask=mask; }
TreeCameraMaskScope::~TreeCameraMaskScope() { cameraMask=previous; }

void TreeRenderBridge::Capture(CSpeedTreeWrapper& tree,const char* asset)
{
    if(!treeRenderer) return;
#if !defined(WRAPPER_USE_STATIC_LIGHTING) || !defined(WRAPPER_USE_NO_WIND) || !defined(WRAPPER_USE_GPU_LEAF_PLACEMENT) || !defined(WRAPPER_RENDER_SELF_SHADOWS)
    treeRenderer->ReportFailure(); // Do not silently support an unanalysed alternate SDK build configuration.
#else
    auto data=std::make_shared<TreeModelData>(); data->asset=asset;
    const auto& native=*tree.m_pGeometryCache;
    IndexedSource(native.m_sBranches,data->branches.source); IndexedSource(native.m_sFronds,data->fronds.source);
    data->leaves.resize(tree.m_usNumLeafLods);
    const auto& leaf=native.m_sLeaves0;
    constexpr unsigned corners[]={0,1,2,0,2,3};
    const auto* sizes=tree.m_pSpeedTree->GetLeafLodSizeAdjustments();
    for(size_t lod=0;lod<data->leaves.size();++lod) {
        auto& vertices=data->leaves[lod].source.vertices; vertices.resize(size_t(leaf.m_usLeafCount)*6);
        for(unsigned i=0;i<leaf.m_usLeafCount;++i) for(unsigned k=0;k<6;++k) {
            auto& v=vertices[i*6+k]; memcpy(v.position.data(),leaf.m_pCenterCoords+i*3,12);
            v.color=leaf.m_pColors[i]; memcpy(v.uv.data(),leaf.m_pLeafMapTexCoords[i]+corners[k]*2,8);
            v.leaf={float(c_nVertexShader_LeafTables+leaf.m_pLeafClusterIndices[i]*4+corners[k]),sizes[lod]};
        }
    }
    tree.m_treeRenderData=data;
    Report(*data,"captured branches="+std::to_string(data->branches.source.vertices.size())+
        " fronds="+std::to_string(data->fronds.source.vertices.size())+" leaf_vertices="+std::to_string(leaf.m_usLeafCount*6)+
        " leaf_lods="+std::to_string(data->leaves.size()));
#endif
}
void TreeRenderBridge::Draw(CSpeedTreeWrapper const& tree,TreePart part,uint32_t lod,uint32_t first,uint32_t count)
{
    if(!treeRenderer || !treeDrawScope || !treeWorldFrame) return; // Native callers already applied visibility.
    auto data=tree.m_treeRenderData;
    if(!data) { treeRenderer->ReportFailure(); return; }
    TreeMesh* mesh=nullptr;
    switch(part) {
    case TreePart::Branch: mesh=&data->branches; break;
    case TreePart::Frond: mesh=&data->fronds; break;
    case TreePart::Leaf: if(lod<data->leaves.size()) mesh=&data->leaves[lod]; break;
    case TreePart::Billboard: mesh=&tree.m_treeBillboard; break;
    }
    if(!mesh) { Report(*data,"invalid native component/LOD",true); return; }
    if(!mesh->geometry) mesh->geometry=treeRenderer->UploadGeometry(mesh->source,part==TreePart::Billboard);
    if(!mesh->geometry) { Report(*data,"geometry upload",true); return; }
    if(part==TreePart::Billboard && !treeRenderer->UpdateVertices(mesh->geometry,mesh->source.vertices)) { Report(*data,"billboard update",true); return; }
    const auto resolve=[&](TextureBinding bound) -> CGraphicImage* {
        if(!bound) return nullptr;
        for(auto* candidate:{&tree.m_BranchImageInstance,&tree.m_CompositeImageInstance,&tree.m_ShadowImageInstance})
            if(!candidate->IsEmpty() && candidate->GetTextureReference().GetTextureBinding()==bound)
                return const_cast<CGraphicImageInstance*>(candidate)->GetGraphicImagePointer(); // Legacy read-only getter is not const-qualified.
        if(cameraMask && cameraMask->GetTexturePointer()->GetTextureBinding()==bound) return cameraMask;
        return nullptr;
    };
    TextureBinding bound[2]{DRAWSTATE.GetTextureBinding(0),DRAWSTATE.GetTextureBinding(1)};
    TerrainTexturePtr images[2];
    for(unsigned stage=0;stage<2;++stage) {
        if(stage==1 && !bound[stage]) continue;
        auto* image=resolve(bound[stage]);
        if(!image) { Report(*data,"unresolved texture stage="+std::to_string(stage),true); return; }
        auto& texture=data->textures[image->GetFileName()];
        if(!texture) texture=LoadStaticObjectTextureFile(image->GetFileName(),*treeRenderer);
        if(!texture) { Report(*data,"texture upload",true); return; }
        images[stage]=texture;
    }
    TreeDraw draw; draw.part=part; draw.strip=part==TreePart::Branch || part==TreePart::Frond; draw.first=first; draw.count=count;
    if(!CaptureState(draw,images[1]!=nullptr)) {
        std::string state="unsupported state part="+std::to_string(uint32_t(part));
        for(auto type:{Renderer::StateLighting,Renderer::StateAlphaBlendEnable,Renderer::StateSrcBlend,Renderer::StateDestBlend,Renderer::StateZFunc,Renderer::StateAlphaFunc,Renderer::StateFogTableMode})
            state+=" rs"+std::to_string(type)+"="+std::to_string(DRAWSTATE.GetRenderState(type));
        for(unsigned stage=0;stage<2;++stage) for(auto type:{Renderer::StageColorOp,Renderer::StageColorArg1,Renderer::StageColorArg2,Renderer::StageAlphaOp,Renderer::StageAlphaArg1,Renderer::StageAlphaArg2,Renderer::StageTexCoordIndex,Renderer::StageTextureTransformFlags})
            state+=" t"+std::to_string(stage)+":"+std::to_string(type)+"="+std::to_string(Stage(stage,type));
        Report(*data,state,true); return;
    }
    treeRenderer->Draw(&tree,mesh->geometry,images[0],images[1],draw);
    Report(*data,"submitted part="+std::to_string(uint32_t(part))+" lod="+std::to_string(lod)+
        " alpha="+std::to_string(draw.alphaReference)+" blend="+std::to_string(draw.blend)+
        " second="+std::to_string(draw.stage1)+" fog="+std::to_string(draw.fog));
}
void TreeRenderBridge::Billboard(CSpeedTreeWrapper const& tree,const float* coords,const float* uv)
{
    if(!treeRenderer || !treeDrawScope || !treeWorldFrame) return;
    constexpr unsigned corners[]={0,1,2,0,2,3};
    auto& vertices=tree.m_treeBillboard.source.vertices; vertices.resize(6);
    for(unsigned i=0;i<6;++i) {
        vertices[i]=TreeVertex{};
        memcpy(vertices[i].position.data(),coords+corners[i]*3,12); memcpy(vertices[i].uv.data(),uv+corners[i]*2,8);
    }
    Draw(tree,TreePart::Billboard,0,0,6);
}
