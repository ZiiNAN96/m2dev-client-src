#include "StdAfx.h"
#include "StaticObjectBridge.h"
#include "EterGrnLib/ThingInstance.h"
#include "EterLib/StateManager.h"
#include "EterLib/StaticObjectTextureLoader.h"
#include <unordered_map>
#include <unordered_set>
#include <fstream>

namespace
{
using namespace Renderer;
struct ModelResources
{
    StaticObjectGeometryPtr geometry;
    std::unordered_map<std::string,TerrainTexturePtr> textures;
};
struct ObjectResources
{
    std::unordered_map<CGrannyModel*,ModelResources> models;
    std::unordered_set<std::string> reports;
};
// Entries are owned by live CArea instances and erased before their pooled Thing is deleted.
std::unordered_map<CGraphicThingInstance*,ObjectResources> objects;
std::ofstream diagnostics;
StaticObjectDraw baseDraw;
bool baseDrawValid=false;
void Report(CGraphicThingInstance& thing,const char* status)
{
    auto& object=objects[&thing];
    if(!object.reports.insert(status).second) return;
    if(!diagnostics.is_open()) diagnostics.open("static-object-adapter.log",std::ios::trunc);
    static bool statesReported=false;
    if(!statesReported || strncmp(status,"excluded:",9)==0) {
        statesReported=true;
        for(auto type:{D3DRS_ALPHABLENDENABLE,D3DRS_ALPHATESTENABLE,D3DRS_ALPHAFUNC,D3DRS_ALPHAREF,
                      D3DRS_SRCBLEND,D3DRS_DESTBLEND,D3DRS_BLENDOP,D3DRS_SEPARATEALPHABLENDENABLE,D3DRS_ZENABLE,D3DRS_ZWRITEENABLE,D3DRS_ZFUNC,
                      D3DRS_SPECULARENABLE,D3DRS_COLORVERTEX,D3DRS_CULLMODE,D3DRS_LIGHTING,D3DRS_FOGENABLE,D3DRS_FOGTABLEMODE,D3DRS_FOGVERTEXMODE})
            diagnostics << "state " << type << '=' << STATEMANAGER.GetRenderState(type) << '\n';
        for(DWORD stage=0;stage<2;++stage) for(auto type:{D3DTSS_COLOROP,D3DTSS_COLORARG1,D3DTSS_COLORARG2,D3DTSS_ALPHAOP,D3DTSS_ALPHAARG1,D3DTSS_ALPHAARG2,D3DTSS_TEXCOORDINDEX,D3DTSS_TEXTURETRANSFORMFLAGS}) {
            DWORD value=0; STATEMANAGER.GetTextureStageState(stage,type,&value); diagnostics << "stage " << stage << ':' << type << '=' << value << '\n';
        }
        for(DWORD stage=0;stage<2;++stage) for(auto type:{D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV,D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,D3DSAMP_MIPFILTER}) {
            DWORD value=0; STATEMANAGER.GetSamplerState(stage,type,&value); diagnostics << "sampler " << stage << ':' << type << '=' << value << '\n';
        }
    }
    auto* base=thing.GetBaseThingPtr(); const auto& p=thing.GetPosition();
    diagnostics << status << " file=" << (base ? base->GetFileName() : "unknown")
                << " position=" << p.x << ',' << p.y << ',' << p.z << std::endl;
}
DWORD Stage(DWORD stage,D3DTEXTURESTAGESTATETYPE type)
{ DWORD value=0; STATEMANAGER.GetTextureStageState(stage,type,&value); return value; }
float Float(D3DRENDERSTATETYPE type)
{ DWORD value=STATEMANAGER.GetRenderState(type); float result; memcpy(&result,&value,4); return result; }
// Read-only access to existing legacy device light enable state, not a state-manager rewrite.
class StateReader : public CGraphicBase
{
public:
    static bool Capture(StaticObjectDraw& d, bool cameraMask, bool shadowBase, bool actorLighting)
    {
        // ZiiNAN: Ensure deterministic actor material state
        if(actorLighting) d=StaticObjectDraw{};
        // Some sampler fields were never initialized in the legacy cache. Read the
        // real device defaults/settings without changing the legacy state manager.
        const auto Sample=[](D3DSAMPLERSTATETYPE type) { DWORD value=0; return SUCCEEDED(ms_lpd3dDevice->GetSamplerState(0,type,&value)) ? value : ~DWORD(0); };
        // RenderArea's imminent shadow setup changes base RGB to TEXTURE*DIFFUSE
        // and disables texture alpha. Preserve that lighting, not the shadow texture.
        const bool textureOnly=!shadowBase && Stage(0,D3DTSS_COLOROP)==D3DTOP_SELECTARG1;
        // ZiiNAN: Inspect the already applied native actor stages; never set legacy state.
        if(actorLighting) {
            const auto operation=Stage(1,D3DTSS_COLOROP);
            if(operation!=D3DTOP_DISABLE) {
                if(Stage(1,D3DTSS_COLORARG1)!=D3DTA_CURRENT) return false;
                if((operation==D3DTOP_ADD || operation==D3DTOP_MODULATE) &&
                   Stage(1,D3DTSS_COLORARG2)==D3DTA_TFACTOR && Stage(1,D3DTSS_ALPHAOP)==D3DTOP_DISABLE)
                    d.actorStage=operation==D3DTOP_ADD ? ActorMaterialStage::Add : ActorMaterialStage::Modulate;
                else if(operation==D3DTOP_MODULATEALPHA_ADDCOLOR && Stage(1,D3DTSS_COLORARG2)==D3DTA_TEXTURE &&
                        Stage(1,D3DTSS_ALPHAOP)==D3DTOP_SELECTARG1 && Stage(1,D3DTSS_ALPHAARG1)==D3DTA_CURRENT &&
                        Stage(1,D3DTSS_TEXCOORDINDEX)==D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR &&
                        Stage(1,D3DTSS_TEXTURETRANSFORMFLAGS)==D3DTTFF_COUNT2)
                    d.actorStage=ActorMaterialStage::Specular;
                else return false;
            }
            const D3DXCOLOR factor(STATEMANAGER.GetRenderState(D3DRS_TEXTUREFACTOR));
            d.textureFactor={factor.r,factor.g,factor.b,factor.a};
            d.factorAlpha=Stage(0,D3DTSS_ALPHAARG2)==D3DTA_TFACTOR && Stage(0,D3DTSS_ALPHAOP)==D3DTOP_MODULATE;
            d.factorAlphaOnly=Stage(0,D3DTSS_ALPHAARG2)==D3DTA_TFACTOR && Stage(0,D3DTSS_ALPHAOP)==D3DTOP_SELECTARG2;
            if(d.actorStage==ActorMaterialStage::Specular && !d.factorAlpha) return false;
        }
        if(!STATEMANAGER.GetRenderState(D3DRS_ZENABLE) ||
           STATEMANAGER.GetRenderState(D3DRS_ZFUNC)!=D3DCMP_LESSEQUAL ||
           STATEMANAGER.GetRenderState(D3DRS_SPECULARENABLE) || STATEMANAGER.GetRenderState(D3DRS_COLORVERTEX) ||
           (!cameraMask && !actorLighting && Stage(1,D3DTSS_COLOROP)!=D3DTOP_DISABLE) || (!shadowBase && !textureOnly && Stage(0,D3DTSS_COLOROP)!=D3DTOP_MODULATE) ||
           Stage(0,D3DTSS_COLORARG1)!=D3DTA_TEXTURE ||
           (Stage(0,D3DTSS_COLORARG2)!=D3DTA_CURRENT && Stage(0,D3DTSS_COLORARG2)!=D3DTA_DIFFUSE) ||
           (!shadowBase && !d.factorAlphaOnly && Stage(0,D3DTSS_ALPHAOP)!=D3DTOP_MODULATE && Stage(0,D3DTSS_ALPHAOP)!=D3DTOP_SELECTARG1) ||
           (!d.factorAlphaOnly && Stage(0,D3DTSS_ALPHAARG1)!=D3DTA_TEXTURE) ||
           (!shadowBase && !d.factorAlpha && Stage(0,D3DTSS_ALPHAOP)==D3DTOP_MODULATE && Stage(0,D3DTSS_ALPHAARG2)!=D3DTA_CURRENT && Stage(0,D3DTSS_ALPHAARG2)!=D3DTA_DIFFUSE) ||
           Stage(0,D3DTSS_TEXCOORDINDEX)!=0 || Stage(0,D3DTSS_TEXTURETRANSFORMFLAGS)!=D3DTTFF_DISABLE) return false;
        d.blend=STATEMANAGER.GetRenderState(D3DRS_ALPHABLENDENABLE)!=FALSE;
        d.depthWrite=STATEMANAGER.GetRenderState(D3DRS_ZWRITEENABLE)!=FALSE;
        d.textureAlpha=Stage(0,D3DTSS_ALPHAOP)==D3DTOP_SELECTARG1;
        d.diffuseAlphaOnly=shadowBase;
        if(d.blend && (STATEMANAGER.GetRenderState(D3DRS_SRCBLEND)!=D3DBLEND_SRCALPHA ||
           STATEMANAGER.GetRenderState(D3DRS_DESTBLEND)!=D3DBLEND_INVSRCALPHA ||
           STATEMANAGER.GetRenderState(D3DRS_BLENDOP)!=D3DBLENDOP_ADD ||
           STATEMANAGER.GetRenderState(D3DRS_SEPARATEALPHABLENDENABLE))) return false;
        if(STATEMANAGER.GetRenderState(D3DRS_ALPHATESTENABLE)) {
            const auto function=STATEMANAGER.GetRenderState(D3DRS_ALPHAFUNC);
            if(function!=D3DCMP_GREATEREQUAL && function!=D3DCMP_GREATER) return false;
            d.alphaTest=function==D3DCMP_GREATEREQUAL ? StaticObjectAlphaTest::GreaterEqual : StaticObjectAlphaTest::Greater;
            d.alphaReference=STATEMANAGER.GetRenderState(D3DRS_ALPHAREF);
            if(d.alphaReference>255) return false;
        }
        if(cameraMask) {
            // A previous native blocker (including a tree) may leave stage 0
            // alpha MODULATE. Stage 1 replaces that alpha entirely with its mask.
            if(!d.blend || Stage(1,D3DTSS_COLOROP)!=D3DTOP_SELECTARG1 ||
               Stage(1,D3DTSS_COLORARG1)!=D3DTA_CURRENT || Stage(1,D3DTSS_ALPHAOP)!=D3DTOP_SELECTARG1 ||
               Stage(1,D3DTSS_ALPHAARG1)!=D3DTA_TEXTURE ||
               Stage(1,D3DTSS_TEXCOORDINDEX)!=D3DTSS_TCI_CAMERASPACEPOSITION ||
               Stage(1,D3DTSS_TEXTURETRANSFORMFLAGS)!=D3DTTFF_COUNT2) return false;
            D3DXMATRIX matrix; STATEMANAGER.GetTransform(D3DTS_TEXTURE1,&matrix);
            memcpy(d.cameraAlphaTransform.data(),&matrix,64);
            const auto CameraSample=[](D3DSAMPLERSTATETYPE type) { DWORD value=0; return SUCCEEDED(ms_lpd3dDevice->GetSamplerState(1,type,&value)) ? value : ~DWORD(0); };
            const auto min=CameraSample(D3DSAMP_MINFILTER),mag=CameraSample(D3DSAMP_MAGFILTER),mip=CameraSample(D3DSAMP_MIPFILTER);
            if(CameraSample(D3DSAMP_ADDRESSU)!=D3DTADDRESS_CLAMP || CameraSample(D3DSAMP_ADDRESSV)!=D3DTADDRESS_CLAMP ||
               (min!=D3DTEXF_POINT && min!=D3DTEXF_LINEAR && min!=D3DTEXF_ANISOTROPIC) ||
               (mag!=D3DTEXF_POINT && mag!=D3DTEXF_LINEAR && mag!=D3DTEXF_ANISOTROPIC) || mip>D3DTEXF_LINEAR ||
               CameraSample(D3DSAMP_MAXMIPLEVEL)!=0 || CameraSample(D3DSAMP_MIPMAPLODBIAS)!=0) return false;
            d.cameraAlphaSampling={false,false,min==D3DTEXF_LINEAR,mag==D3DTEXF_LINEAR,mip==D3DTEXF_LINEAR,mip!=D3DTEXF_NONE};
            d.cameraAlphaAnisotropic=min==D3DTEXF_ANISOTROPIC || mag==D3DTEXF_ANISOTROPIC;
            if(d.cameraAlphaAnisotropic) {
                d.cameraAlphaMaxAnisotropy=CameraSample(D3DSAMP_MAXANISOTROPY);
                if(min!=D3DTEXF_ANISOTROPIC || mag!=D3DTEXF_ANISOTROPIC || mip!=D3DTEXF_LINEAR ||
                   d.cameraAlphaMaxAnisotropy<1 || d.cameraAlphaMaxAnisotropy>16) return false;
            }
        }
        // ZiiNAN: Original sphere-map matrix and sampler, separate from camera-blocker alpha.
        if(d.actorStage==ActorMaterialStage::Specular) {
            D3DXMATRIX matrix; STATEMANAGER.GetTransform(D3DTS_TEXTURE1,&matrix);
            memcpy(d.cameraAlphaTransform.data(),&matrix,64);
            const auto SphereSample=[](D3DSAMPLERSTATETYPE type) { DWORD value=0; return SUCCEEDED(ms_lpd3dDevice->GetSamplerState(1,type,&value)) ? value : ~DWORD(0); };
            const auto min=SphereSample(D3DSAMP_MINFILTER),mag=SphereSample(D3DSAMP_MAGFILTER),mip=SphereSample(D3DSAMP_MIPFILTER);
            if(SphereSample(D3DSAMP_ADDRESSU)!=D3DTADDRESS_WRAP || SphereSample(D3DSAMP_ADDRESSV)!=D3DTADDRESS_WRAP ||
               (min!=D3DTEXF_POINT && min!=D3DTEXF_LINEAR && min!=D3DTEXF_ANISOTROPIC) ||
               (mag!=D3DTEXF_POINT && mag!=D3DTEXF_LINEAR && mag!=D3DTEXF_ANISOTROPIC) || mip>D3DTEXF_LINEAR ||
               SphereSample(D3DSAMP_MAXMIPLEVEL)!=0 || SphereSample(D3DSAMP_MIPMAPLODBIAS)!=0) return false;
            d.cameraAlphaSampling={true,true,min==D3DTEXF_LINEAR,mag==D3DTEXF_LINEAR,mip==D3DTEXF_LINEAR,mip!=D3DTEXF_NONE};
            d.cameraAlphaAnisotropic=min==D3DTEXF_ANISOTROPIC || mag==D3DTEXF_ANISOTROPIC;
            if(d.cameraAlphaAnisotropic) {
                d.cameraAlphaMaxAnisotropy=SphereSample(D3DSAMP_MAXANISOTROPY);
                if(min!=D3DTEXF_ANISOTROPIC || mag!=D3DTEXF_ANISOTROPIC || mip!=D3DTEXF_LINEAR ||
                   d.cameraAlphaMaxAnisotropy<1 || d.cameraAlphaMaxAnisotropy>16) return false;
            }
        }
        const auto cull=STATEMANAGER.GetRenderState(D3DRS_CULLMODE);
        if(cull<D3DCULL_NONE || cull>D3DCULL_CCW) return false;
        d.cull=StaticObjectCull(cull-1);
        const auto u=Sample(D3DSAMP_ADDRESSU),v=Sample(D3DSAMP_ADDRESSV);
        const auto min=Sample(D3DSAMP_MINFILTER),mag=Sample(D3DSAMP_MAGFILTER),mip=Sample(D3DSAMP_MIPFILTER);
        if((u!=D3DTADDRESS_WRAP && u!=D3DTADDRESS_CLAMP) || (v!=D3DTADDRESS_WRAP && v!=D3DTADDRESS_CLAMP) ||
           (min!=D3DTEXF_POINT && min!=D3DTEXF_LINEAR && min!=D3DTEXF_ANISOTROPIC) ||
           (mag!=D3DTEXF_POINT && mag!=D3DTEXF_LINEAR && mag!=D3DTEXF_ANISOTROPIC) ||
           mip>D3DTEXF_LINEAR || Sample(D3DSAMP_MAXMIPLEVEL)!=0 || Sample(D3DSAMP_MIPMAPLODBIAS)!=0) return false;
        d.sampling={u==D3DTADDRESS_WRAP,v==D3DTADDRESS_WRAP,min==D3DTEXF_LINEAR,mag==D3DTEXF_LINEAR,mip==D3DTEXF_LINEAR,mip!=D3DTEXF_NONE};
        d.anisotropic=min==D3DTEXF_ANISOTROPIC || mag==D3DTEXF_ANISOTROPIC;
        if(d.anisotropic) {
            if(min!=D3DTEXF_ANISOTROPIC || mag!=D3DTEXF_ANISOTROPIC || mip!=D3DTEXF_LINEAR) return false;
            // Legacy's default MAXANISOTROPY may never enter the state cache.
            DWORD maximum=1;
            if(FAILED(ms_lpd3dDevice->GetSamplerState(0,D3DSAMP_MAXANISOTROPY,&maximum)) || maximum<1 || maximum>16) return false;
            d.maxAnisotropy=maximum;
        }
        D3DXMATRIX view,projection;
        STATEMANAGER.GetTransform(D3DTS_VIEW,&view); STATEMANAGER.GetTransform(D3DTS_PROJECTION,&projection);
        memcpy(d.matrices.view.data(),&view,64); memcpy(d.matrices.projection.data(),&projection,64);
        if(textureOnly) {
            // SELECTARG1 ignores lit RGB, including light 1 left on by character
            // selection. Lighting still supplies material alpha to ALPHAOP.
            if(STATEMANAGER.GetRenderState(D3DRS_LIGHTING)) {
                D3DMATERIAL9 material; STATEMANAGER.GetMaterial(&material);
                d.ambient[3]=material.Diffuse.a;
            }
        } else if(STATEMANAGER.GetRenderState(D3DRS_LIGHTING)) {
            D3DMATERIAL9 material; STATEMANAGER.GetMaterial(&material);
            D3DLIGHT9 light{}; BOOL enabled=FALSE;
            if(FAILED(ms_lpd3dDevice->GetLightEnable(0,&enabled))) return false;
            for(DWORD i=1;i<8;++i) {
                BOOL other=FALSE;
                if(FAILED(ms_lpd3dDevice->GetLightEnable(i,&other)) || !other) continue;
                // ZiiNAN: Same existing point light for the normal actor material, no new lights.
                if((!cameraMask && !shadowBase && !actorLighting) || i!=1) return false;
                D3DLIGHT9 point{};
                if(FAILED(ms_lpd3dDevice->GetLight(1,&point)) || point.Type!=D3DLIGHT_POINT) return false;
                D3DXVECTOR3 position(point.Position.x,point.Position.y,point.Position.z);
                D3DXVec3TransformCoord(&position,&position,&view);
                d.pointPositionRange={position.x,position.y,position.z,point.Range};
                d.pointAttenuation={point.Attenuation0,point.Attenuation1,point.Attenuation2,0};
                d.pointAmbient={material.Ambient.r*point.Ambient.r,material.Ambient.g*point.Ambient.g,material.Ambient.b*point.Ambient.b,0};
                d.pointDiffuse={material.Diffuse.r*point.Diffuse.r,material.Diffuse.g*point.Diffuse.g,material.Diffuse.b*point.Diffuse.b,0};
            }
            if(enabled) { STATEMANAGER.GetLight(0,&light); if(light.Type!=D3DLIGHT_DIRECTIONAL) return false; }
            const D3DXCOLOR ambient(STATEMANAGER.GetRenderState(D3DRS_AMBIENT));
            d.ambient={material.Emissive.r+material.Ambient.r*(ambient.r+light.Ambient.r),
                       material.Emissive.g+material.Ambient.g*(ambient.g+light.Ambient.g),
                       material.Emissive.b+material.Ambient.b*(ambient.b+light.Ambient.b),material.Diffuse.a};
            d.diffuse={material.Diffuse.r*light.Diffuse.r,material.Diffuse.g*light.Diffuse.g,material.Diffuse.b*light.Diffuse.b,0};
            if(enabled) {
                D3DXVECTOR3 direction(-light.Direction.x,-light.Direction.y,-light.Direction.z);
                D3DXVec3TransformNormal(&direction,&direction,&view); D3DXVec3Normalize(&direction,&direction);
                d.lightDirection={direction.x,direction.y,direction.z,0};
            }
        }
        // CArea::RenderDungeon sets SELECTARG1 even in outdoor areas without a
        // dungeon block. Keep that original unlit RGB; alpha is still modulated.
        if(textureOnly) { d.ambient[0]=d.ambient[1]=d.ambient[2]=1; d.diffuse={}; }
        d.normalizeNormals=STATEMANAGER.GetRenderState(D3DRS_NORMALIZENORMALS)!=FALSE;
        if(STATEMANAGER.GetRenderState(D3DRS_FOGENABLE)) {
            const auto fog=STATEMANAGER.GetRenderState(D3DRS_FOGVERTEXMODE);
            if(fog>D3DFOG_LINEAR || STATEMANAGER.GetRenderState(D3DRS_FOGTABLEMODE)!=D3DFOG_NONE) return false;
            d.fog=TerrainFog(fog); d.rangeFog=STATEMANAGER.GetRenderState(D3DRS_RANGEFOGENABLE)!=FALSE;
            d.fogParameters={Float(D3DRS_FOGSTART),Float(D3DRS_FOGEND),Float(D3DRS_FOGDENSITY),0};
            if(d.fog==TerrainFog::Linear && d.fogParameters[0]==d.fogParameters[1]) return false;
            const D3DXCOLOR color(STATEMANAGER.GetRenderState(D3DRS_FOGCOLOR)); d.fogColor={color.r,color.g,color.b,color.a};
        }
        return true;
    }
};
}

bool CaptureStaticMapObjectDraw(Renderer::StaticObjectDraw& draw, bool cameraMask, bool shadowBase, bool actorLighting)
{
    return StateReader::Capture(draw,cameraMask,shadowBase,actorLighting);
}

void BeginStaticMapObjects(bool legacyShadowActive)
{
    baseDrawValid=false;
    if(!Renderer::staticObjectRenderer) return;
    baseDraw={};
    baseDrawValid=CaptureStaticMapObjectDraw(baseDraw,false,legacyShadowActive);
}

void SubmitStaticMapObject(CGraphicThingInstance& thing, StaticMapObjectPass pass, CGraphicImage* cameraAlpha)
{
    using namespace Renderer;
    auto* renderer=staticObjectRenderer;
    if(!renderer || !thing.isShow()) return;
    if(thing.IsMotionThing() || thing.HaveBlendThing()) { Report(thing,"excluded: animation/blend"); return; }
    StaticObjectDraw common=baseDraw;
    if(pass==StaticMapObjectPass::CameraBlocker) {
        common={};
        if(!cameraAlpha || !CaptureStaticMapObjectDraw(common,true)) { Report(thing,"excluded: camera blocker state"); return; }
    } else if(!baseDrawValid) { Report(thing,"excluded: base render state"); return; }
    // RenderArea enables writes for its opaque list after the shadow receiver pass.
    if(pass==StaticMapObjectPass::Opaque) common.depthWrite=true;
    // Validate only referenced groups: unused Granny palette entries are not draws.
    for(DWORD i=0;i<thing.GetLODControllerCount();++i) {
        auto* instance=thing.GetLODControllerPointer(i)->GetModelInstance();
        auto* model=instance ? instance->GetModel() : nullptr;
        if(!model || !model->GetStaticObjectSource()) { Report(thing,"excluded: no static PNT source"); return; }
        auto& palette=instance->GetStaticObjectMaterialPalette();
        for(auto* node=model->GetMeshNodeList(CGrannyMesh::TYPE_RIGID,CGrannyMaterial::TYPE_DIFFUSE_PNT);node;node=node->pNextMeshNode)
        for(auto* group=node->pMesh->GetTriGroupNodeList(CGrannyMaterial::TYPE_DIFFUSE_PNT);group;group=group->pNextTriGroupNode) {
            if(group->mtrlIndex>=palette.GetMaterialCount()) { Report(thing,"excluded: invalid material index"); return; }
            auto& material=palette.GetMaterialRef(group->mtrlIndex);
            if(material.GetType()!=CGrannyMaterial::TYPE_DIFFUSE_PNT || material.IsSpecularEnabled() ||
               !material.GetImagePointer(0) || material.GetImagePointer(1)) { Report(thing,"excluded: material outside diffuse contract"); return; }
            D3DSURFACE_DESC description{};
            auto* texture=material.GetD3DTexture(0);
            if(!texture || FAILED(texture->GetLevelDesc(0,&description))) { Report(thing,"excluded: missing diffuse texture"); return; }
            switch(description.Format) {
            case D3DFMT_DXT1: case D3DFMT_DXT3: case D3DFMT_DXT5:
            case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: case D3DFMT_A8B8G8R8: case D3DFMT_A1R5G5B5: break;
            default: Report(thing,"excluded: legacy texture format outside static map subset"); return;
            }
        }
    }
    auto& object=objects[&thing];
    for(DWORD i=0;i<thing.GetLODControllerCount();++i) {
        auto* instance=thing.GetLODControllerPointer(i)->GetModelInstance();
        auto* model=instance->GetModel(); auto& resource=object.models[model];
        if(!resource.geometry) resource.geometry=renderer->UploadGeometry(*model->GetStaticObjectSource());
        if(!resource.geometry) { Report(thing,"ERROR: geometry upload"); return; }
        if(pass==StaticMapObjectPass::CameraBlocker) {
            const std::string name=cameraAlpha->GetFileName();
            auto& mask=resource.textures[name];
            if(!mask) mask=LoadStaticObjectTextureFile(name.c_str(),*renderer);
            if(!mask) { Report(thing,"ERROR: camera alpha upload"); return; }
            common.cameraAlpha=mask;
        }
        auto& palette=instance->GetStaticObjectMaterialPalette();
        for(auto* node=model->GetMeshNodeList(CGrannyMesh::TYPE_RIGID,CGrannyMaterial::TYPE_DIFFUSE_PNT);node;node=node->pNextMeshNode) {
            const D3DXMATRIX* world=instance->GetStaticObjectWorldMatrix(node->iMesh);
            if(!world) { renderer->UploadGeometry({}); Report(thing,"ERROR: missing mesh matrix"); return; }
            auto draw=common;
            memcpy(draw.matrices.world.data(),world,64);
            D3DXMATRIX view,normal; memcpy(&view,draw.matrices.view.data(),64);
            normal=(*world)*view;
            if(!D3DXMatrixInverse(&normal,nullptr,&normal)) { Report(thing,"excluded: singular transform"); return; }
            D3DXMatrixTranspose(&normal,&normal); memcpy(draw.normalTransform.data(),&normal,64);
            draw.baseVertex=node->pMesh->GetVertexBasePosition(); draw.vertexCount=node->pMesh->GetVertexCount();
            for(auto* group=node->pMesh->GetTriGroupNodeList(CGrannyMaterial::TYPE_DIFFUSE_PNT);group;group=group->pNextTriGroupNode) {
                auto& material=palette.GetMaterialRef(group->mtrlIndex);
                const std::string name=material.GetImagePointer(0)->GetFileName();
                auto& texture=resource.textures[name];
                if(!texture) texture=LoadStaticObjectTextureFile(name.c_str(),*renderer);
                if(!texture) { Report(thing,"ERROR: texture upload"); return; }
                draw.cull=material.IsTwoSided() ? StaticObjectCull::None : common.cull;
                draw.firstIndex=group->idxPos; draw.indexCount=group->triCount*3;
                renderer->Draw(resource.geometry,texture,draw);
            }
        }
    }
    Report(thing,pass==StaticMapObjectPass::CameraBlocker ? "submitted: static camera blocker" :
        (pass==StaticMapObjectPass::ShadowReceiver ? "submitted: static shadow receiver base" : "submitted: static rigid diffuse"));
}
void ReleaseStaticMapObject(CGraphicThingInstance* thing)
{
    const auto found=objects.find(thing);
    if(found==objects.end()) return;
    if(Renderer::staticObjectRenderer && !found->second.models.empty()) Renderer::staticObjectRenderer->ReleaseBindings();
    objects.erase(found);
    if(diagnostics.is_open()) diagnostics << "release live_objects=" << objects.size() << std::endl;
}
