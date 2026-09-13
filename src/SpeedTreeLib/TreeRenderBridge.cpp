// ZiiNAN: Diligent SpeedTree rendering integration; synchronous CPU data and read-only legacy state.
#include "StdAfx.h"
#include "EterLib/NativeStateView.h"
#include "TreeRenderBridge.h"
#include "SpeedTreeWrapper.h"
#include "EterLib/StateManager.h"
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
DWORD Stage(DWORD index,D3DTEXTURESTAGESTATETYPE type)
{ DWORD result=0; STATEMANAGER.GetTextureStageState(index,type,&result); return result; }
float StateFloat(D3DRENDERSTATETYPE type)
{ DWORD bits=STATEMANAGER.GetRenderState(type); float value; memcpy(&value,&bits,4); return value; }
bool Sampling(DWORD stage,TreeSampler& sampler)
{
    auto* device=STATEMANAGER.GetDevice();
    const auto get=[&](D3DSAMPLERSTATETYPE type) { DWORD value=0; return SUCCEEDED(NativeStateView().GetSamplerState(stage,type,&value)) ? value : ~DWORD(0); };
    const auto u=get(D3DSAMP_ADDRESSU),v=get(D3DSAMP_ADDRESSV),min=get(D3DSAMP_MINFILTER),mag=get(D3DSAMP_MAGFILTER),mip=get(D3DSAMP_MIPFILTER);
    if((u!=D3DTADDRESS_WRAP && u!=D3DTADDRESS_CLAMP) || (v!=D3DTADDRESS_WRAP && v!=D3DTADDRESS_CLAMP) ||
       min<D3DTEXF_POINT || min>D3DTEXF_ANISOTROPIC || mag<D3DTEXF_POINT || mag>D3DTEXF_ANISOTROPIC ||
       mip>D3DTEXF_LINEAR || get(D3DSAMP_MAXMIPLEVEL)!=0 || get(D3DSAMP_MIPMAPLODBIAS)!=0) return false;
    sampler.sampling={u==D3DTADDRESS_WRAP,v==D3DTADDRESS_WRAP,min==D3DTEXF_LINEAR,mag==D3DTEXF_LINEAR,mip==D3DTEXF_LINEAR,mip!=D3DTEXF_NONE};
    sampler.anisotropic=min==D3DTEXF_ANISOTROPIC || mag==D3DTEXF_ANISOTROPIC;
    if(sampler.anisotropic) {
        sampler.maxAnisotropy=get(D3DSAMP_MAXANISOTROPY);
        if(min!=D3DTEXF_ANISOTROPIC || mag!=D3DTEXF_ANISOTROPIC || mip!=D3DTEXF_LINEAR ||
           sampler.maxAnisotropy<1 || sampler.maxAnisotropy>16) return false;
    }
    return true;
}
bool CaptureState(TreeDraw& draw,bool hasSecond)
{
    draw.blend=STATEMANAGER.GetRenderState(D3DRS_ALPHABLENDENABLE)!=FALSE;
    draw.depthTest=STATEMANAGER.GetRenderState(D3DRS_ZENABLE)!=FALSE;
    draw.depthWrite=STATEMANAGER.GetRenderState(D3DRS_ZWRITEENABLE)!=FALSE;
    draw.depthFunction=STATEMANAGER.GetRenderState(D3DRS_ZFUNC);
    draw.cull=STATEMANAGER.GetRenderState(D3DRS_CULLMODE)-1;
    draw.alphaTest=STATEMANAGER.GetRenderState(D3DRS_ALPHATESTENABLE)!=FALSE;
    draw.alphaReference=STATEMANAGER.GetRenderState(D3DRS_ALPHAREF);
    if(draw.cull>2 || draw.depthFunction<1 || draw.depthFunction>8 || draw.alphaReference>255 ||
       (draw.alphaTest && STATEMANAGER.GetRenderState(D3DRS_ALPHAFUNC)!=D3DCMP_GREATER) ||
       (draw.blend && (STATEMANAGER.GetRenderState(D3DRS_SRCBLEND)!=D3DBLEND_SRCALPHA ||
        STATEMANAGER.GetRenderState(D3DRS_DESTBLEND)!=D3DBLEND_INVSRCALPHA ||
        STATEMANAGER.GetRenderState(D3DRS_BLENDOP)!=D3DBLENDOP_ADD || STATEMANAGER.GetRenderState(D3DRS_SEPARATEALPHABLENDENABLE)))) return false;
    if(STATEMANAGER.GetRenderState(D3DRS_LIGHTING) || Stage(0,D3DTSS_COLOROP)!=D3DTOP_MODULATE ||
       Stage(0,D3DTSS_COLORARG1)!=D3DTA_TEXTURE || Stage(0,D3DTSS_COLORARG2)!=D3DTA_DIFFUSE ||
       Stage(0,D3DTSS_ALPHAOP)!=D3DTOP_MODULATE || Stage(0,D3DTSS_ALPHAARG1)!=D3DTA_TEXTURE ||
       Stage(0,D3DTSS_ALPHAARG2)!=D3DTA_DIFFUSE || Stage(0,D3DTSS_TEXCOORDINDEX)!=0 ||
       Stage(0,D3DTSS_TEXTURETRANSFORMFLAGS)!=D3DTTFF_DISABLE) return false;
    if(hasSecond) {
        if(Stage(1,D3DTSS_COLOROP)==D3DTOP_MODULATE && Stage(1,D3DTSS_COLORARG1)==D3DTA_TEXTURE &&
           Stage(1,D3DTSS_COLORARG2)==D3DTA_CURRENT && Stage(1,D3DTSS_ALPHAOP)==D3DTOP_DISABLE) draw.stage1=1;
        else if(Stage(1,D3DTSS_COLOROP)==D3DTOP_SELECTARG1 && Stage(1,D3DTSS_COLORARG1)==D3DTA_CURRENT &&
                Stage(1,D3DTSS_ALPHAOP)==D3DTOP_MODULATE && Stage(1,D3DTSS_ALPHAARG1)==D3DTA_TEXTURE &&
                Stage(1,D3DTSS_ALPHAARG2)==D3DTA_CURRENT) draw.stage1=2;
        else return false;
        const auto coordinates=Stage(1,D3DTSS_TEXCOORDINDEX),transform=Stage(1,D3DTSS_TEXTURETRANSFORMFLAGS);
        if(coordinates==D3DTSS_TCI_CAMERASPACEPOSITION && transform==D3DTTFF_COUNT2) draw.cameraCoordinates=true;
        else if(coordinates!=1 || transform!=D3DTTFF_DISABLE) return false;
    }
    for(unsigned stage=0;stage<2;++stage) if((stage==0 || hasSecond) && !Sampling(stage,draw.samplers[stage])) return false;
    D3DXMATRIX matrix;
    for(auto entry:{std::pair<D3DTRANSFORMSTATETYPE,std::array<float,16>*>(D3DTS_WORLD,&draw.matrices.world),
                    {D3DTS_VIEW,&draw.matrices.view},{D3DTS_PROJECTION,&draw.matrices.projection},{D3DTS_TEXTURE1,&draw.textureTransform}}) {
        STATEMANAGER.GetTransform(entry.first,&matrix); memcpy(entry.second->data(),&matrix,64);
    }
    if(draw.part==TreePart::Leaf && FAILED(NativeStateView().GetVertexShaderConstantF(0,draw.legacyConstants[0].data(),96))) return false;
    if(STATEMANAGER.GetRenderState(D3DRS_FOGENABLE)) {
        if(STATEMANAGER.GetRenderState(D3DRS_FOGTABLEMODE)!=D3DFOG_NONE) return false;
        draw.fog=draw.part==TreePart::Leaf ? 4u : STATEMANAGER.GetRenderState(D3DRS_FOGVERTEXMODE);
        if(draw.fog>4) return false;
        draw.rangeFog=STATEMANAGER.GetRenderState(D3DRS_RANGEFOGENABLE)!=FALSE;
        draw.fogParameters={StateFloat(D3DRS_FOGSTART),StateFloat(D3DRS_FOGEND),StateFloat(D3DRS_FOGDENSITY),0};
        if(draw.fog==3 && draw.fogParameters[0]==draw.fogParameters[1]) return false;
        const D3DXCOLOR color(STATEMANAGER.GetRenderState(D3DRS_FOGCOLOR)); draw.fogColor={color.r,color.g,color.b,color.a};
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
    const auto resolve=[&](LPDIRECT3DBASETEXTURE9 bound) -> CGraphicImage* {
        if(!bound) return nullptr;
        for(auto* candidate:{&tree.m_BranchImageInstance,&tree.m_CompositeImageInstance,&tree.m_ShadowImageInstance})
            if(!candidate->IsEmpty() && candidate->GetTextureReference().GetD3DTexture()==bound)
                return const_cast<CGraphicImageInstance*>(candidate)->GetGraphicImagePointer(); // Legacy read-only getter is not const-qualified.
        if(cameraMask && cameraMask->GetTexturePointer()->GetD3DTexture()==bound) return cameraMask;
        return nullptr;
    };
    LPDIRECT3DBASETEXTURE9 bound[2]{}; STATEMANAGER.GetTexture(0,&bound[0]); STATEMANAGER.GetTexture(1,&bound[1]);
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
        for(auto type:{D3DRS_LIGHTING,D3DRS_ALPHABLENDENABLE,D3DRS_SRCBLEND,D3DRS_DESTBLEND,D3DRS_ZFUNC,D3DRS_ALPHAFUNC,D3DRS_FOGTABLEMODE})
            state+=" rs"+std::to_string(type)+"="+std::to_string(STATEMANAGER.GetRenderState(type));
        for(unsigned stage=0;stage<2;++stage) for(auto type:{D3DTSS_COLOROP,D3DTSS_COLORARG1,D3DTSS_COLORARG2,D3DTSS_ALPHAOP,D3DTSS_ALPHAARG1,D3DTSS_ALPHAARG2,D3DTSS_TEXCOORDINDEX,D3DTSS_TEXTURETRANSFORMFLAGS})
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
