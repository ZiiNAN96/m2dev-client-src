#include "StdAfx.h"
#include "EterLib/DrawStateView.h"
#include "StaticObjectBridge.h"
#include "EterGrnLib/ThingInstance.h"
#include "EterLib/DrawState.h"
#include "EterLib/StaticObjectTextureLoader.h"
#include "Renderer/WorldRenderData.h"
#include "Renderer/Diagnostics.h"
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
    if(!verboseDiagnostics) return;
    auto& object=objects[&thing];
    if(!object.reports.insert(status).second) return;
    if(!diagnostics.is_open()) diagnostics.open("static-object-adapter.log",std::ios::trunc);
    static bool statesReported=false;
    if(!statesReported || strncmp(status,"excluded:",9)==0) {
        statesReported=true;
        for(auto type:{Renderer::StateAlphaBlendEnable,Renderer::StateAlphaTestEnable,Renderer::StateAlphaFunc,Renderer::StateAlphaRef,
                      Renderer::StateSrcBlend,Renderer::StateDestBlend,Renderer::StateBlendOp,Renderer::StateSeparateAlphaBlendEnable,Renderer::StateZEnable,Renderer::StateZWriteEnable,Renderer::StateZFunc,
                      Renderer::StateSpecularEnable,Renderer::StateColorVertex,Renderer::StateCullMode,Renderer::StateLighting,Renderer::StateFogEnable,Renderer::StateFogTableMode,Renderer::StateFogVertexMode})
            diagnostics << "state " << type << '=' << DRAWSTATE.GetRenderState(type) << '\n';
        for(DWORD stage=0;stage<2;++stage) for(auto type:{Renderer::StageColorOp,Renderer::StageColorArg1,Renderer::StageColorArg2,Renderer::StageAlphaOp,Renderer::StageAlphaArg1,Renderer::StageAlphaArg2,Renderer::StageTexCoordIndex,Renderer::StageTextureTransformFlags}) {
            DWORD value=0; DRAWSTATE.GetTextureStageState(stage,type,&value); diagnostics << "stage " << stage << ':' << type << '=' << value << '\n';
        }
        for(DWORD stage=0;stage<2;++stage) for(auto type:{Renderer::SamplerAddressU,Renderer::SamplerAddressV,Renderer::SamplerMinFilter,Renderer::SamplerMagFilter,Renderer::SamplerMipFilter}) {
            DWORD value=0; DRAWSTATE.GetSamplerState(stage,type,&value); diagnostics << "sampler " << stage << ':' << type << '=' << value << '\n';
        }
    }
    auto* base=thing.GetBaseThingPtr(); const auto& p=thing.GetPosition();
    diagnostics << status << " file=" << (base ? base->GetFileName() : "unknown")
                << " position=" << p.x << ',' << p.y << ',' << p.z << std::endl;
}
DWORD Stage(DWORD stage,Renderer::TextureStageKey type)
{ DWORD value=0; DRAWSTATE.GetTextureStageState(stage,type,&value); return value; }
float Float(Renderer::RenderStateKey type)
{ DWORD value=DRAWSTATE.GetRenderState(type); float result; memcpy(&result,&value,4); return result; }
// Shared material snapshot: CPU compatibility state in Diligent, native device in Legacy.
class StateReader : public CGraphicBase
{
public:
    static bool Capture(StaticObjectDraw& d, bool cameraMask, bool shadowBase, bool actorLighting, bool groundItem)
    {
        // ZiiNAN: Ensure deterministic actor material state
        if(actorLighting) d=StaticObjectDraw{};
        // Startup-seeded defaults avoid historically uninitialized legacy sampler cache fields.
        const auto Sample=[](Renderer::SamplerStateKey type) { DWORD value=0; return SUCCEEDED(DrawStateView().GetSamplerState(0,type,&value)) ? value : ~DWORD(0); };
        // RenderArea's imminent shadow setup changes base RGB to TEXTURE*DIFFUSE
        // and disables texture alpha. Preserve that lighting, not the shadow texture.
        const bool textureOnly=!shadowBase && Stage(0,Renderer::StageColorOp)==Renderer::TextureOpSelectArg1;
        // ZiiNAN: Inspect the already applied native actor stages; never set legacy state.
        if(actorLighting) {
            const auto operation=Stage(1,Renderer::StageColorOp);
            const auto stage1=DrawStateView().GetTextureBinding(1);
            const bool disabledByNullTexture=!stage1 && operation==Renderer::TextureOpSelectArg1 && Stage(1,Renderer::StageColorArg1)==Renderer::ArgTexture;
            if(operation!=Renderer::TextureOpDisable && !disabledByNullTexture) {
                if(Stage(1,Renderer::StageColorArg1)!=Renderer::ArgCurrent) return false;
                if((operation==Renderer::TextureOpAdd || operation==Renderer::TextureOpModulate) &&
                   Stage(1,Renderer::StageColorArg2)==Renderer::ArgTFactor && Stage(1,Renderer::StageAlphaOp)==Renderer::TextureOpDisable)
                    d.actorStage=operation==Renderer::TextureOpAdd ? ActorMaterialStage::Add : ActorMaterialStage::Modulate;
                else if(operation==Renderer::TextureOpModulateAlphaAddColor && Stage(1,Renderer::StageColorArg2)==Renderer::ArgTexture &&
                        Stage(1,Renderer::StageAlphaOp)==Renderer::TextureOpSelectArg1 && Stage(1,Renderer::StageAlphaArg1)==Renderer::ArgCurrent &&
                        Stage(1,Renderer::StageTexCoordIndex)==Renderer::StageTciCameraSpaceReflectionVector &&
                        Stage(1,Renderer::StageTextureTransformFlags)==Renderer::TexTransformCount2)
                    d.actorStage=ActorMaterialStage::Specular;
                else return false;
            }
            const Math::Color factor(DRAWSTATE.GetRenderState(Renderer::StateTextureFactor));
            d.textureFactor={factor.r,factor.g,factor.b,factor.a};
            d.factorAlpha=Stage(0,Renderer::StageAlphaArg2)==Renderer::ArgTFactor && Stage(0,Renderer::StageAlphaOp)==Renderer::TextureOpModulate;
            d.factorAlphaOnly=Stage(0,Renderer::StageAlphaArg2)==Renderer::ArgTFactor && Stage(0,Renderer::StageAlphaOp)==Renderer::TextureOpSelectArg2;
            if(d.actorStage==ActorMaterialStage::Specular && !d.factorAlpha) return false;
        }
        // ZiiNAN: Ground items retain the native post-effect TFACTOR * TEXTURE alpha order.
        const bool swappedFactorAlpha=groundItem && Stage(0,Renderer::StageAlphaOp)==Renderer::TextureOpModulate &&
            Stage(0,Renderer::StageAlphaArg1)==Renderer::ArgTFactor && Stage(0,Renderer::StageAlphaArg2)==Renderer::ArgTexture;
        if(swappedFactorAlpha) {
            d.factorAlpha=true;
            const Math::Color factor(DRAWSTATE.GetRenderState(Renderer::StateTextureFactor));
            d.textureFactor={factor.r,factor.g,factor.b,factor.a};
        }
        if(!DRAWSTATE.GetRenderState(Renderer::StateZEnable) ||
           DRAWSTATE.GetRenderState(Renderer::StateZFunc)!=Renderer::CompareLessEqual ||
           DRAWSTATE.GetRenderState(Renderer::StateSpecularEnable) || DRAWSTATE.GetRenderState(Renderer::StateColorVertex) ||
           (!cameraMask && !actorLighting && Stage(1,Renderer::StageColorOp)!=Renderer::TextureOpDisable) || (!shadowBase && !textureOnly && Stage(0,Renderer::StageColorOp)!=Renderer::TextureOpModulate) ||
           Stage(0,Renderer::StageColorArg1)!=Renderer::ArgTexture ||
           (Stage(0,Renderer::StageColorArg2)!=Renderer::ArgCurrent && Stage(0,Renderer::StageColorArg2)!=Renderer::ArgDiffuse) ||
           (!shadowBase && !d.factorAlphaOnly && Stage(0,Renderer::StageAlphaOp)!=Renderer::TextureOpModulate && Stage(0,Renderer::StageAlphaOp)!=Renderer::TextureOpSelectArg1) ||
           (!d.factorAlphaOnly && !swappedFactorAlpha && Stage(0,Renderer::StageAlphaArg1)!=Renderer::ArgTexture) ||
           (!shadowBase && !d.factorAlpha && Stage(0,Renderer::StageAlphaOp)==Renderer::TextureOpModulate && Stage(0,Renderer::StageAlphaArg2)!=Renderer::ArgCurrent && Stage(0,Renderer::StageAlphaArg2)!=Renderer::ArgDiffuse) ||
           Stage(0,Renderer::StageTexCoordIndex)!=0 || Stage(0,Renderer::StageTextureTransformFlags)!=Renderer::TexTransformDisable) return false;
        d.blend=DRAWSTATE.GetRenderState(Renderer::StateAlphaBlendEnable)!=FALSE;
        d.depthWrite=DRAWSTATE.GetRenderState(Renderer::StateZWriteEnable)!=FALSE;
        d.textureAlpha=Stage(0,Renderer::StageAlphaOp)==Renderer::TextureOpSelectArg1;
        d.diffuseAlphaOnly=shadowBase;
        if(d.blend && (DRAWSTATE.GetRenderState(Renderer::StateSrcBlend)!=Renderer::BlendSrcAlpha ||
           DRAWSTATE.GetRenderState(Renderer::StateDestBlend)!=Renderer::BlendInvSrcAlpha ||
           DRAWSTATE.GetRenderState(Renderer::StateBlendOp)!=Renderer::BlendOpAdd ||
           DRAWSTATE.GetRenderState(Renderer::StateSeparateAlphaBlendEnable))) return false;
        if(DRAWSTATE.GetRenderState(Renderer::StateAlphaTestEnable)) {
            const auto function=DRAWSTATE.GetRenderState(Renderer::StateAlphaFunc);
            if(function!=Renderer::CompareGreaterEqual && function!=Renderer::CompareGreater) return false;
            d.alphaTest=function==Renderer::CompareGreaterEqual ? StaticObjectAlphaTest::GreaterEqual : StaticObjectAlphaTest::Greater;
            d.alphaReference=DRAWSTATE.GetRenderState(Renderer::StateAlphaRef);
            if(d.alphaReference>255) return false;
        }
        if(cameraMask) {
            // A previous native blocker (including a tree) may leave stage 0
            // alpha MODULATE. Stage 1 replaces that alpha entirely with its mask.
            if(!d.blend || Stage(1,Renderer::StageColorOp)!=Renderer::TextureOpSelectArg1 ||
               Stage(1,Renderer::StageColorArg1)!=Renderer::ArgCurrent || Stage(1,Renderer::StageAlphaOp)!=Renderer::TextureOpSelectArg1 ||
               Stage(1,Renderer::StageAlphaArg1)!=Renderer::ArgTexture ||
               Stage(1,Renderer::StageTexCoordIndex)!=Renderer::StageTciCameraSpacePosition ||
               Stage(1,Renderer::StageTextureTransformFlags)!=Renderer::TexTransformCount2) return false;
            Math::Matrix matrix; DRAWSTATE.GetTransform(Renderer::MatrixTexture1,&matrix);
            memcpy(d.cameraAlphaTransform.data(),&matrix,64);
            const auto CameraSample=[](Renderer::SamplerStateKey type) { DWORD value=0; return SUCCEEDED(DrawStateView().GetSamplerState(1,type,&value)) ? value : ~DWORD(0); };
            const auto min=CameraSample(Renderer::SamplerMinFilter),mag=CameraSample(Renderer::SamplerMagFilter),mip=CameraSample(Renderer::SamplerMipFilter);
            if(CameraSample(Renderer::SamplerAddressU)!=Renderer::AddressClamp || CameraSample(Renderer::SamplerAddressV)!=Renderer::AddressClamp ||
               (min!=Renderer::FilterPoint && min!=Renderer::FilterLinear && min!=Renderer::FilterAnisotropic) ||
               (mag!=Renderer::FilterPoint && mag!=Renderer::FilterLinear && mag!=Renderer::FilterAnisotropic) || mip>Renderer::FilterLinear ||
               CameraSample(Renderer::SamplerMaxMipLevel)!=0 || CameraSample(Renderer::SamplerMipMapLodBias)!=0) return false;
            d.cameraAlphaSampling={false,false,min==Renderer::FilterLinear,mag==Renderer::FilterLinear,mip==Renderer::FilterLinear,mip!=Renderer::FilterNone};
            d.cameraAlphaAnisotropic=min==Renderer::FilterAnisotropic || mag==Renderer::FilterAnisotropic;
            if(d.cameraAlphaAnisotropic) {
                d.cameraAlphaMaxAnisotropy=CameraSample(Renderer::SamplerMaxAnisotropy);
                if(min!=Renderer::FilterAnisotropic || mag!=Renderer::FilterAnisotropic || mip!=Renderer::FilterLinear ||
                   d.cameraAlphaMaxAnisotropy<1 || d.cameraAlphaMaxAnisotropy>16) return false;
            }
        }
        // ZiiNAN: Original sphere-map matrix and sampler, separate from camera-blocker alpha.
        if(d.actorStage==ActorMaterialStage::Specular) {
            Math::Matrix matrix; DRAWSTATE.GetTransform(Renderer::MatrixTexture1,&matrix);
            memcpy(d.cameraAlphaTransform.data(),&matrix,64);
            const auto SphereSample=[](Renderer::SamplerStateKey type) { DWORD value=0; return SUCCEEDED(DrawStateView().GetSamplerState(1,type,&value)) ? value : ~DWORD(0); };
            const auto min=SphereSample(Renderer::SamplerMinFilter),mag=SphereSample(Renderer::SamplerMagFilter),mip=SphereSample(Renderer::SamplerMipFilter);
            if(SphereSample(Renderer::SamplerAddressU)!=Renderer::AddressWrap || SphereSample(Renderer::SamplerAddressV)!=Renderer::AddressWrap ||
               (min!=Renderer::FilterPoint && min!=Renderer::FilterLinear && min!=Renderer::FilterAnisotropic) ||
               (mag!=Renderer::FilterPoint && mag!=Renderer::FilterLinear && mag!=Renderer::FilterAnisotropic) || mip>Renderer::FilterLinear ||
               SphereSample(Renderer::SamplerMaxMipLevel)!=0 || SphereSample(Renderer::SamplerMipMapLodBias)!=0) return false;
            d.cameraAlphaSampling={true,true,min==Renderer::FilterLinear,mag==Renderer::FilterLinear,mip==Renderer::FilterLinear,mip!=Renderer::FilterNone};
            d.cameraAlphaAnisotropic=min==Renderer::FilterAnisotropic || mag==Renderer::FilterAnisotropic;
            if(d.cameraAlphaAnisotropic) {
                d.cameraAlphaMaxAnisotropy=SphereSample(Renderer::SamplerMaxAnisotropy);
                if(min!=Renderer::FilterAnisotropic || mag!=Renderer::FilterAnisotropic || mip!=Renderer::FilterLinear ||
                   d.cameraAlphaMaxAnisotropy<1 || d.cameraAlphaMaxAnisotropy>16) return false;
            }
        }
        const auto cull=DRAWSTATE.GetRenderState(Renderer::StateCullMode);
        if(cull<Renderer::CullNone || cull>Renderer::CullCcw) return false;
        d.cull=StaticObjectCull(cull-1);
        const auto u=Sample(Renderer::SamplerAddressU),v=Sample(Renderer::SamplerAddressV);
        const auto min=Sample(Renderer::SamplerMinFilter),mag=Sample(Renderer::SamplerMagFilter),mip=Sample(Renderer::SamplerMipFilter);
        if((u!=Renderer::AddressWrap && u!=Renderer::AddressClamp) || (v!=Renderer::AddressWrap && v!=Renderer::AddressClamp) ||
           (min!=Renderer::FilterPoint && min!=Renderer::FilterLinear && min!=Renderer::FilterAnisotropic) ||
           (mag!=Renderer::FilterPoint && mag!=Renderer::FilterLinear && mag!=Renderer::FilterAnisotropic) ||
           mip>Renderer::FilterLinear || Sample(Renderer::SamplerMaxMipLevel)!=0 || Sample(Renderer::SamplerMipMapLodBias)!=0) return false;
        d.sampling={u==Renderer::AddressWrap,v==Renderer::AddressWrap,min==Renderer::FilterLinear,mag==Renderer::FilterLinear,mip==Renderer::FilterLinear,mip!=Renderer::FilterNone};
        d.anisotropic=min==Renderer::FilterAnisotropic || mag==Renderer::FilterAnisotropic;
        if(d.anisotropic) {
            if(min!=Renderer::FilterAnisotropic || mag!=Renderer::FilterAnisotropic || mip!=Renderer::FilterLinear) return false;
            // Legacy's default MAXANISOTROPY may never enter the state cache.
            DWORD maximum=1;
            if(FAILED(DrawStateView().GetSamplerState(0,Renderer::SamplerMaxAnisotropy,&maximum)) || maximum<1 || maximum>16) return false;
            d.maxAnisotropy=maximum;
        }
        Math::Matrix view,projection;
        DRAWSTATE.GetTransform(Renderer::MatrixView,&view); DRAWSTATE.GetTransform(Renderer::MatrixProjection,&projection);
        memcpy(d.matrices.view.data(),&view,64); memcpy(d.matrices.projection.data(),&projection,64);
        if(actorLighting) {
            Math::Viewport viewport{}; if(FAILED(DrawStateView().GetViewport(&viewport))) return false;
            d.viewport={viewport.X,viewport.Y,viewport.Width,viewport.Height};
        }
        if(textureOnly) {
            // SELECTARG1 ignores lit RGB, including light 1 left on by character
            // selection. Lighting still supplies material alpha to ALPHAOP.
            if(DRAWSTATE.GetRenderState(Renderer::StateLighting)) {
                Renderer::MaterialValues material; DRAWSTATE.GetMaterial(&material);
                d.ambient[3]=material.Diffuse.a;
            }
        } else if(DRAWSTATE.GetRenderState(Renderer::StateLighting)) {
            Renderer::MaterialValues material; DRAWSTATE.GetMaterial(&material);
            Renderer::LightValues light{}; BOOL enabled=FALSE;
            if(FAILED(DrawStateView().GetLightEnable(0,&enabled))) return false;
            for(DWORD i=1;i<8;++i) {
                BOOL other=FALSE;
                if(FAILED(DrawStateView().GetLightEnable(i,&other)) || !other) continue;
                // ZiiNAN: Same existing point light for the normal actor material, no new lights.
                if((!cameraMask && !shadowBase && !actorLighting && !groundItem) || i!=1) return false;
                Renderer::LightValues point{};
                if(FAILED(DrawStateView().GetLight(1,&point)) || point.Type!=Renderer::LightPoint) return false;
                Math::Vector3 position(point.Position.x,point.Position.y,point.Position.z);
                Math::Vec3TransformCoord(&position,&position,&view);
                d.pointPositionRange={position.x,position.y,position.z,point.Range};
                d.pointAttenuation={point.Attenuation0,point.Attenuation1,point.Attenuation2,0};
                d.pointAmbient={material.Ambient.r*point.Ambient.r,material.Ambient.g*point.Ambient.g,material.Ambient.b*point.Ambient.b,0};
                d.pointDiffuse={material.Diffuse.r*point.Diffuse.r,material.Diffuse.g*point.Diffuse.g,material.Diffuse.b*point.Diffuse.b,0};
            }
            if(enabled) {
                if(FAILED(DrawStateView().GetLight(0,&light))) return false;
                if(actorLighting && light.Type==Renderer::LightSpot) {
                    // ZiiNAN: Diligent character-select actor rendering.
                    Math::Vector3 position(light.Position),direction(light.Direction);
                    Math::Vec3TransformCoord(&position,&position,&view);
                    Math::Vec3TransformNormal(&direction,&direction,&view); Math::Vec3Normalize(&direction,&direction);
                    d.spotPositionRange={position.x,position.y,position.z,light.Range};
                    d.spotAttenuation={light.Attenuation0,light.Attenuation1,light.Attenuation2,0};
                    d.spotAmbient={material.Ambient.r*light.Ambient.r,material.Ambient.g*light.Ambient.g,material.Ambient.b*light.Ambient.b,0};
                    d.spotDiffuse={material.Diffuse.r*light.Diffuse.r,material.Diffuse.g*light.Diffuse.g,material.Diffuse.b*light.Diffuse.b,0};
                    d.spotDirection={direction.x,direction.y,direction.z,0};
                    d.spotCone={std::cos(light.Theta*0.5f),std::cos(light.Phi*0.5f),light.Falloff,0};
                    light={}; enabled=FALSE;
                } else if(light.Type!=Renderer::LightDirectional) return false;
            }
            const Math::Color ambient(DRAWSTATE.GetRenderState(Renderer::StateAmbient));
            d.ambient={material.Emissive.r+material.Ambient.r*(ambient.r+light.Ambient.r),
                       material.Emissive.g+material.Ambient.g*(ambient.g+light.Ambient.g),
                       material.Emissive.b+material.Ambient.b*(ambient.b+light.Ambient.b),material.Diffuse.a};
            d.diffuse={material.Diffuse.r*light.Diffuse.r,material.Diffuse.g*light.Diffuse.g,material.Diffuse.b*light.Diffuse.b,0};
            if(enabled) {
                Math::Vector3 direction(-light.Direction.x,-light.Direction.y,-light.Direction.z);
                Math::Vec3TransformNormal(&direction,&direction,&view); Math::Vec3Normalize(&direction,&direction);
                d.lightDirection={direction.x,direction.y,direction.z,0};
            }
        }
        // CArea::RenderDungeon sets SELECTARG1 even in outdoor areas without a
        // dungeon block. Keep that original unlit RGB; alpha is still modulated.
        if(textureOnly) { d.ambient[0]=d.ambient[1]=d.ambient[2]=1; d.diffuse={}; }
        d.normalizeNormals=DRAWSTATE.GetRenderState(Renderer::StateNormalizeNormals)!=FALSE;
        if(DRAWSTATE.GetRenderState(Renderer::StateFogEnable)) {
            const auto fog=DRAWSTATE.GetRenderState(Renderer::StateFogVertexMode);
            if(fog>Renderer::FogLinear || DRAWSTATE.GetRenderState(Renderer::StateFogTableMode)!=Renderer::FogNone) return false;
            d.fog=TerrainFog(fog); d.rangeFog=DRAWSTATE.GetRenderState(Renderer::StateRangeFogEnable)!=FALSE;
            d.fogParameters={Float(Renderer::StateFogStart),Float(Renderer::StateFogEnd),Float(Renderer::StateFogDensity),0};
            if(d.fog==TerrainFog::Linear && d.fogParameters[0]==d.fogParameters[1]) return false;
            const Math::Color color(DRAWSTATE.GetRenderState(Renderer::StateFogColor)); d.fogColor={color.r,color.g,color.b,color.a};
        }
        return true;
    }
};
}

bool CaptureStaticMapObjectDraw(Renderer::StaticObjectDraw& draw, bool cameraMask, bool shadowBase, bool actorLighting, bool groundItem)
{
    return StateReader::Capture(draw,cameraMask,shadowBase,actorLighting,groundItem);
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
    } else if(pass==StaticMapObjectPass::GroundItem) {
        // ZiiNAN: Capture this native item draw, never inherit the earlier map-object snapshot.
        common={};
        if(!CaptureStaticMapObjectDraw(common,false,false,false,true)) { Report(thing,"excluded: ground item state"); return; }
    } else if(!baseDrawValid) { Report(thing,"excluded: base render state"); return; }
    // RenderArea enables writes for its opaque list after the shadow receiver pass.
    if(pass==StaticMapObjectPass::Opaque) common.depthWrite=true;
    // Validate only referenced groups: unused Granny palette entries are not draws.
    for(DWORD i=0;i<thing.GetLODControllerCount();++i) {
        auto* instance=thing.GetLODControllerPointer(i)->GetModelInstance();
        auto* model=instance ? instance->GetModel() : nullptr;
        if(!model || (!model->GetStaticObjectSource() && !(pass==StaticMapObjectPass::GroundItem &&
            model->GetActorSource() && model->GetActorSource()->IsRigid()))) { Report(thing,"excluded: no static PNT source"); return; }
        auto& palette=instance->GetStaticObjectMaterialPalette();
        if(!model->GetAsset()) { Report(thing,"ERROR: missing Asset Runtime model"); return; }
        for(auto* node=model->GetMeshNodeList(CGrannyMesh::TYPE_RIGID,CGrannyMaterial::TYPE_DIFFUSE_PNT);node;node=node->pNextMeshNode)
        for(auto* group=node->pMesh->GetTriGroupNodeList(CGrannyMaterial::TYPE_DIFFUSE_PNT);group;group=group->pNextTriGroupNode) {
            if(group->mtrlIndex>=palette.GetMaterialCount()) { Report(thing,"excluded: invalid material index"); return; }
            auto& material=palette.GetMaterialRef(group->mtrlIndex);
            const auto& description=material.GetAsset();
            if(description.stage!=AssetRuntime::MaterialStage::Diffuse || material.IsSpecularEnabled() ||
               description.textures[0].empty() || !description.textures[1].empty()) { Report(thing,"excluded: material outside diffuse contract"); return; }
            const auto binding=material.GetTextureBinding(0);
            if (binding.source) {
                const auto format=binding.source->desc.format;
                if (format==TerrainTextureFormat::Unknown || format==TerrainTextureFormat::Alpha8) {
                    Report(thing,"excluded: source texture format outside static map subset"); return;
                }
            } else {
                Report(thing,"excluded: missing diffuse texture"); return;
            }
        }
    }
    auto& object=objects[&thing];
    for(DWORD i=0;i<thing.GetLODControllerCount();++i) {
        auto* instance=thing.GetLODControllerPointer(i)->GetModelInstance();
        auto* model=instance->GetModel(); auto& resource=object.models[model];
        if(!resource.geometry) {
            if(model->GetStaticObjectSource()) resource.geometry=renderer->UploadGeometry(*model->GetStaticObjectSource());
            else {
                // ZiiNAN: An equipped weapon may already be cached; reuse its existing rigid CPU snapshot.
                const auto& source=*model->GetActorSource();
                resource.geometry=renderer->UploadGeometry({source.rigidVertices,source.indices});
            }
        }
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
            const auto& meshes=model->GetAsset()->meshes;
            if(node->iMesh<0 || size_t(node->iMesh)>=meshes.size()) { Report(thing,"ERROR: invalid Asset Runtime mesh"); return; }
            const auto& mesh=meshes[node->iMesh];
            const Math::Matrix* world=instance->GetStaticObjectWorldMatrix(node->iMesh);
            if(!world) { renderer->UploadGeometry({}); Report(thing,"ERROR: missing mesh matrix"); return; }
            auto draw=common;
            memcpy(draw.matrices.world.data(),world,64);
            Math::Matrix view,normal; memcpy(&view,draw.matrices.view.data(),64);
            normal=(*world)*view;
            if(!Math::MatrixInverse(&normal,nullptr,&normal)) { Report(thing,"excluded: singular transform"); return; }
            Math::MatrixTranspose(&normal,&normal); memcpy(draw.normalTransform.data(),&normal,64);
            draw.baseVertex=node->pMesh->GetVertexBasePosition(); draw.vertexCount=mesh.vertexCount;
            for(auto* group=node->pMesh->GetTriGroupNodeList(CGrannyMaterial::TYPE_DIFFUSE_PNT);group;group=group->pNextTriGroupNode) {
                auto& material=palette.GetMaterialRef(group->mtrlIndex);
                const auto& name=material.GetAsset().textures[0];
                auto& texture=resource.textures[name];
                if(!texture) texture=LoadStaticObjectTextureFile(name.c_str(),*renderer);
                if(!texture) { Report(thing,"ERROR: texture upload"); return; }
                draw.cull=material.GetAsset().culling==AssetRuntime::Culling::None ? StaticObjectCull::None : common.cull;
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

// ZiiNAN: Animated/blended map Things reuse completed CPU poses and the M5 material renderer.
namespace {
struct SpecialThingContext { CGraphicThingInstance& thing; CGraphicImage* cameraAlpha; };
void SubmitSpecialThing(void* context,const void* native,const Renderer::ActorNativeDraw& group)
{
    using namespace Renderer;
    auto& c=*static_cast<SpecialThingContext*>(context); CGrannyModelInstance* instance=nullptr;
    for(DWORD i=0;i<c.thing.GetLODControllerCount();++i) {
        auto* candidate=c.thing.GetLODControllerPointer(i)->GetModelInstance();
        if(candidate==native) { instance=candidate; break; }
    }
    const auto fail=[&](const char* message) { Report(c.thing,message); if(worldRenderer) worldRenderer->ReportFailure(); };
    if(!instance || !instance->GetModel()) { fail("ERROR: special thing instance"); return; }
    auto& data=instance->GetActorRenderData(); const auto& source=instance->GetModel()->GetActorSource();
    if(!source) { fail("ERROR: special thing PNT source"); return; }
    if(!source->IsRigid() && (!data.ready || data.capturedFrame!=actorFrameSerial)) return;
    StaticObjectDraw draw;
    if(!CaptureStaticMapObjectDraw(draw,c.cameraAlpha!=nullptr,false,c.cameraAlpha==nullptr)) { fail("ERROR: special thing material state"); return; }
    auto& palette=instance->GetStaticObjectMaterialPalette();
    if(group.material>=palette.GetMaterialCount()) { fail("ERROR: special thing material index"); return; }
    auto& material=palette.GetMaterialRef(group.material);
    const auto load=[&](CGraphicImage* image) -> TerrainTexturePtr {
        if(!image) return {}; auto& texture=data.textures[image->GetFileName()];
        if(!texture) texture=LoadStaticObjectTextureFile(image->GetFileName(),*actorRenderer); return texture;
    };
    auto texture=load(material.GetImagePointer(0));
    if(!texture) { fail("ERROR: special thing diffuse image"); return; }
    if(c.cameraAlpha) { draw.cameraAlpha=load(c.cameraAlpha); if(!draw.cameraAlpha) { fail("ERROR: special thing camera mask"); return; } }
    if(draw.actorStage==ActorMaterialStage::Specular) {
        draw.sphereMap=load(material.GetSphereMapImage()); if(!draw.sphereMap) { fail("ERROR: special thing sphere image"); return; }
    }
    const auto* world=instance->GetStaticObjectWorldMatrix(group.mesh);
    if(!world) { fail("ERROR: special thing matrix"); return; }
    memcpy(draw.matrices.world.data(),world,64);
    Math::Matrix view,normal; memcpy(&view,draw.matrices.view.data(),64); normal=(*world)*view;
    if(!Math::MatrixInverse(&normal,nullptr,&normal)) { fail("ERROR: singular special thing matrix"); return; }
    Math::MatrixTranspose(&normal,&normal); memcpy(draw.normalTransform.data(),&normal,64);
    draw.baseVertex=group.baseVertex+(group.rigid ? source->deformVertexCount : 0);
    draw.vertexCount=group.vertexCount; draw.firstIndex=group.firstIndex; draw.indexCount=group.indexCount;
    if(!data.geometry) data.geometry=actorRenderer->CreateGeometry(*source,ActorPart::Body,ActorCategory::Special);
    if(!data.geometry) { fail("ERROR: special thing geometry"); return; }
    if(!source->IsRigid() && data.uploadedRevision!=data.revision) {
        if(!actorRenderer->UpdateVertices(data.geometry,data.vertices,source->deformVertexCount,ActorCategory::Special)) { fail("ERROR: special thing pose upload"); return; }
        data.uploadedRevision=data.revision;
    }
    actorRenderer->Draw(&c.thing,data.geometry,texture,draw,ActorCategory::Special);
    Report(c.thing,"submitted: native animated/blended map thing");
}
}
bool DrawSpecialMapObject(CGraphicThingInstance& thing,bool blend,CGraphicImage* cameraAlpha)
{
    if(!Renderer::actorRenderer || !Renderer::worldSurfaceFrame || (!thing.IsMotionThing() && !thing.HaveBlendThing())) return false;
    SpecialThingContext context{thing,cameraAlpha}; Renderer::ThingDrawScope scope({&context,SubmitSpecialThing});
    if(blend) thing.BlendRender(); else if(cameraAlpha) thing.RenderPCBlocker(); else thing.Render();
    return true;
}
