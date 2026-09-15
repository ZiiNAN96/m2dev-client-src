#pragma once
#include "Renderer/DiligentStaticObjectRenderer.h"
#include "Renderer/SceneLightingRuntime.h"
#include "Renderer/ShadowAmbientRuntime.h"

// Uses the normal model loader, native animation instance and parent-bone binding.
// Deform is performed once per frame; every cascade consumes that same palette.
static void NativeShadowProof(DiligentD3D11Backend& backend,DiligentActorRenderer& renderer,
    CGraphicThing& thing,CGrannyModelInstance& actor,CGrannyModelInstance& prop,
    const Math::Matrix& view,const Math::Matrix& projection)
{
    DiligentStaticObjectRenderer floorRenderer(backend);
    Check(floorRenderer.Initialize(false),"native proof floor renderer");
    StaticObjectSource source;
    source.vertices={{-1000,-1000,0,0,0,1,0,0},{1000,-1000,0,0,0,1,1,0},{1000,1000,0,0,0,1,1,1},{-1000,1000,0,0,0,1,0,1}};
    source.indices={0,1,2,0,2,3};auto floor=floorRenderer.UploadGeometry(source);
    std::array<std::uint8_t,4> pixel{180,180,180,255};TerrainTextureData image;image.width=image.height=1;image.format=TerrainTextureFormat::RGBA8;image.mips.push_back({pixel.data(),4,4});
    auto texture=floorRenderer.UploadTexture(image);auto material=std::make_shared<MaterialRuntime>();material->maps[0]=material->classicDiffuse=texture;
    StaticObjectDraw draw;draw.material=material;draw.vertexCount=4;draw.indexCount=6;draw.cull=StaticObjectCull::None;
    std::memcpy(draw.matrices.view.data(),&view,64);std::memcpy(draw.matrices.projection.data(),&projection,64);
    auto light=sceneLighting.Get();light.sun.direction={-1,0,1};sceneLighting.Set(light);
    unsigned revision=100;
    const auto capture=[&](unsigned clip,bool shadows,bool propCaster,const std::string& name){
        Graphics::GraphicsSettings settings;settings.style=Graphics::GraphicsStyle::Modern;
        settings.shadows=shadows?Graphics::ShadowQuality::High:Graphics::ShadowQuality::Off;
        settings.ambientOcclusion=Graphics::AmbientOcclusionQuality::Off;
        ApplyGraphicsRuntimeConfig(Graphics::Resolve(settings,++revision));
        Check(backend.BeginFrame(),"native shadow frame");renderer.ResetFrame();++actorFrameSerial;
        backend.Clear({true,ClearColor{.04f,.05f,.07f,1}});
        actor.SetLocalTime(0);actor.SetMotionPointer(thing.GetMotionPointer(clip),0,1,1);actor.SetLocalTime(.25f);
        Deform(actor);Deform(prop);
        const auto* expected=actor.GetBoneMatrixPointer(12);const auto* actual=prop.GetBoneMatrixPointer(0);
        for(unsigned i=0;i<16;++i)Check(std::abs(expected[i]-actual[i])<.001,"shadow attachment shares animated hand transform");
        const auto count=backend.BeginModernScene(draw.matrices.view,draw.matrices.projection);
        for(unsigned c=0;c<count;++c){Check(backend.BeginSunCascade(c),"native caster cascade");
            floorRenderer.Draw(floor,texture,draw);Draw(actor,renderer,view,projection);if(propCaster)Draw(prop,renderer,view,projection);}
        backend.EndSunCascades();floorRenderer.Draw(floor,texture,draw);Draw(actor,renderer,view,projection);Draw(prop,renderer,view,projection);backend.EndModernScene();
        if(shadows)Check(shadowCasters>=2,"native actor and floor actually cast");
        std::vector<std::uint8_t> rgb;unsigned w{},h{};Check(backend.CaptureRGB(rgb,w,h),"native shadow readback");Save(rgb,w,h,name+".bmp");
        backend.EndFrame();backend.Present();renderer.ReleaseBindings();return rgb;
    };
    const auto darkened=[](const auto& a,const auto& b){unsigned count=0;for(std::size_t i=0;i<a.size();i+=3)if(int(a[i])-int(b[i])>3)++count;return count;};
    for(unsigned clip:{0u,1u,3u}){
        const auto prefix="g34-native-clip-"+std::to_string(clip);
        auto off=capture(clip,false,true,prefix+"-off"),on=capture(clip,true,true,prefix+"-shadow");
        Check(darkened(off,on)>50,"idle walk attack produce native GLB shadow pixels");
        if(clip==3){auto withoutProp=capture(clip,true,false,prefix+"-without-prop-shadow");
            const auto pixels=darkened(withoutProp,on);std::cout<<"Hand attachment shadow pixels="<<pixels<<'\n';Check(pixels>2,"hand attachment contributes its own shadow");}
    }
    draw.material.reset();material.reset();floor.reset();texture.reset();floorRenderer.ReleaseBindings();floorRenderer.ResetFrame();
    std::cout<<"PASS native GLB idle/walk/attack and animated hand attachment shadow\n";
}
