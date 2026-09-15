#include "StdAfx.h"
#include "WorldTree.h"
#include "EterLib/Camera.h"
#include "EterLib/DrawState.h"
#include "EterLib/GrpImage.h"
#include "EterLib/ResourceManager.h"
#include "EterBase/Timer.h"
#include "PackLib/PackManager.h"
#include "Renderer/Diagnostics.h"
#include "Renderer/GraphicsConfig.h"
#include "Renderer/StaticObjectRenderData.h"
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
class NativeTree;
struct NativeResources {std::shared_ptr<const Vegetation::RenderAsset> render;std::vector<CGraphicImage::TRef> images;};
struct State {
    Vegetation::Runtime runtime;bool registryAttempted{};float windStrength{1};CGraphicImage*cameraMask{};
    std::map<const Vegetation::Asset*,std::shared_ptr<NativeResources>> resources;
    std::vector<std::weak_ptr<NativeTree>> instances;std::ofstream log;std::set<std::string> errors;
};
State& World(){static State state;return state;}
void Log(const std::string&line){auto&s=World();if(!s.log.is_open())s.log.open("vegetation-runtime.log",std::ios::trunc);s.log<<line<<std::endl;}
void Failure(const std::string&error){++Vegetation::statistics.failures;if(World().errors.insert(error).second){TraceError("Vegetation Runtime: %s",error.c_str());Log("ERROR "+error);}}
bool Read(std::string_view name,std::vector<std::byte>&bytes){TPackFile file;if(!CPackManager::Instance().GetFile(name,file)||file.size()>128*1024*1024)return false;bytes.resize(file.size());std::memcpy(bytes.data(),file.data(),file.size());return true;}
std::shared_ptr<NativeResources> Resources(Vegetation::AssetPtr asset){
    auto&s=World();if(auto it=s.resources.find(asset.get());it!=s.resources.end())return it->second;
    if(!Renderer::staticObjectRenderer){Failure("static mesh renderer unavailable");return {};}
    auto resources=std::make_shared<NativeResources>();std::string error;
    resources->render=Vegetation::Prepare(asset,*Renderer::staticObjectRenderer,[&](std::string_view key){
        auto*resource=CResourceManager::Instance().GetResourcePointer(std::string(key).c_str());
        if(!resource||!resource->IsType(CGraphicImage::Type()))return Renderer::TerrainTexturePtr{};
        CGraphicImage::TRef image=static_cast<CGraphicImage*>(resource);if(image->IsEmpty())return Renderer::TerrainTexturePtr{};
        auto texture=image->GetAssetTexture(*Renderer::staticObjectRenderer);resources->images.push_back(image);return texture;
    },error);
    if(!resources->render){Failure(error);return {};}
    s.resources[asset.get()]=resources;return resources;
}
float RenderFloat(Renderer::RenderStateKey key){const auto bits=DRAWSTATE.GetRenderState(key);float f;std::memcpy(&f,&bits,4);return f;}
Vegetation::RenderContext Context(bool blocker){
    Vegetation::RenderContext c;Math::Matrix view,projection;DRAWSTATE.GetTransform(Renderer::MatrixView,&view);DRAWSTATE.GetTransform(Renderer::MatrixProjection,&projection);std::memcpy(c.view.data(),&view,64);std::memcpy(c.projection.data(),&projection,64);
    if(auto*camera=CCameraManager::Instance().GetCurrentCamera()){const auto&e=camera->GetEye();c.camera={e.x,e.y,e.z};}
    c.time=CTimer::Instance().GetCurrentSecond();c.windStrength=World().windStrength;
    c.distanceScale=Renderer::GetGraphicsRuntimeConfig().vegetationDistanceScale;
    auto&d=c.state;d.depthWrite=true;d.blend=blocker;d.sampling={true,true,true,true,true,true};d.cameraAlphaSampling=d.sampling;
    if(!blocker&&DRAWSTATE.GetRenderState(Renderer::StateFogEnable)){
        d.fog=static_cast<Renderer::TerrainFog>(DRAWSTATE.GetRenderState(Renderer::StateFogVertexMode));d.rangeFog=DRAWSTATE.GetRenderState(Renderer::StateRangeFogEnable)!=0;
        d.fogParameters={RenderFloat(Renderer::StateFogStart),RenderFloat(Renderer::StateFogEnd),RenderFloat(Renderer::StateFogDensity),0};
        const Math::Color color(DRAWSTATE.GetRenderState(Renderer::StateFogColor));d.fogColor={color.r,color.g,color.b,color.a};
    }
    if(blocker&&World().cameraMask){d.cameraAlpha=World().cameraMask->GetAssetTexture(*Renderer::staticObjectRenderer);Math::Matrix transform;DRAWSTATE.GetTransform(Renderer::MatrixTexture1,&transform);std::memcpy(d.cameraAlphaTransform.data(),&transform,64);}
    return c;
}
class NativeTree final:public CWorldTreeInstance {
public:
    NativeTree(Vegetation::AssetPtr asset,std::shared_ptr<NativeResources>resources,const Vegetation::Matrix&m):instance_(std::move(asset),m),resources_(std::move(resources)){++Vegetation::statistics.created;CGraphicObjectInstance::SetPosition(m[12],m[13],m[14]);}
    ~NativeTree()override{Clear();}
    int GetType()const override{return TREE_OBJECT;}
    void SetPosition(float x,float y,float z)override{instance_.transform[12]=x;instance_.transform[13]=y;instance_.transform[14]=z;instance_.phase=Vegetation::WindPhase(instance_.transform);CGraphicObjectInstance::SetPosition(x,y,z);}
    const float*GetPosition()override{return instance_.transform.data()+12;}
    bool GetBoundingSphere(Math::Vector3&center,float&radius)override{
        const auto b=Vegetation::TransformBounds(instance_.asset->metadata.renderBounds,instance_.transform);if(!b.valid)return false;float squared=0;
        for(unsigned k=0;k<3;++k){center[k]=(b.min[k]+b.max[k])*.5f;const float d=b.max[k]-b.min[k];squared+=d*d;}radius=std::sqrt(squared)*.5f;return true;
    }
    void RenderTree(const Vegetation::RenderContext&c){if(isShow()&&resources_&&Renderer::staticObjectRenderer)Vegetation::Draw(instance_,*resources_->render,*Renderer::staticObjectRenderer,c);else ++Vegetation::statistics.culled;}
    void OnRender()override{if(Renderer::vegetationWorldFrame)RenderTree(Context(false));}
    void OnRenderPCBlocker()override{if(Renderer::vegetationWorldFrame)RenderTree(Context(true));}
    void OnBlendRender()override{}void OnRenderToShadowMap()override{}void OnRenderShadow()override{}
protected:
    void OnUpdateCollisionData(const CStaticCollisionDataVector*)override{
        Math::Matrix transform;std::memcpy(&transform,instance_.transform.data(),64);
        for(const auto&entry:instance_.asset->metadata.collisions){if(entry.kind==2)continue;CStaticCollisionData c{};c.dwType=entry.kind==0?COLLISION_TYPE_SPHERE:COLLISION_TYPE_CYLINDER;for(unsigned k=0;k<3;++k){c.v3Position[k]=entry.position[k];c.fDimensions[k]=entry.dimensions[k];}AddCollision(&c,&transform);}
    }
    void OnUpdateHeighInstance(CAttributeInstance*)override{}bool OnGetObjectHeight(float,float,float*)override{return false;}
private:Vegetation::Instance instance_;std::shared_ptr<NativeResources>resources_;
};
}
WorldTreePtr CreateWorldTree(float x,float y,float z,std::uint32_t,const char*key){
    auto&s=World();if(!s.registryAttempted){s.registryAttempted=true;std::vector<std::byte>bytes;if(!Read("vegetation/registry.json",bytes)){Failure("compiled registry missing");return {};}
        const auto r=s.runtime.registry.Parse({reinterpret_cast<const char*>(bytes.data()),bytes.size()});if(!r){Failure(r.error);return {};}Log("registry entries="+std::to_string(s.runtime.registry.Size()));}
    const auto loaded=s.runtime.Load(key,Read);if(!loaded){Failure(loaded.error);return {};}
    auto resources=Resources(loaded.asset);if(!resources)return {};
    auto matrix=Vegetation::Identity;matrix[12]=x;matrix[13]=y;matrix[14]=z;
    auto instance=std::make_shared<NativeTree>(loaded.asset,resources,matrix);instance->RegisterBoundingSphere();s.instances.push_back(instance);
    if(Renderer::verboseDiagnostics)Log("instance key="+std::string(key)+" position="+std::to_string(x)+","+std::to_string(y)+","+std::to_string(z));
    return instance;
}
void DeleteWorldTree(WorldTreePtr&tree){tree.reset();}
void RenderNativeVegetation(){
    if(!Renderer::vegetationWorldFrame)return;auto&s=World();const auto c=Context(false);
    std::erase_if(s.instances,[](const auto&w){return w.expired();});for(const auto&w:s.instances)if(auto tree=w.lock())tree->RenderTree(c);
}
void ClearNativeVegetation(){
    auto&s=World();std::erase_if(s.instances,[](const auto&w){return w.expired();});s.resources.clear();s.runtime.Clear();
    if(s.log.is_open())Log("clear assets="+std::to_string(Vegetation::liveAssets.load())+" instances="+std::to_string(Vegetation::liveInstances.load())+" geometry="+std::to_string(Vegetation::liveGeometry.load())+" draws="+std::to_string(Vegetation::statistics.submitted));
}
void SetNativeVegetationWind(float strength){World().windStrength=std::clamp(strength,0.f,1.f);}
VegetationCameraMaskScope::VegetationCameraMaskScope(CGraphicImage*image):previous_(World().cameraMask){World().cameraMask=image;}
VegetationCameraMaskScope::~VegetationCameraMaskScope(){World().cameraMask=previous_;}
