#pragma once
#include <algorithm>

// ZiiNAN: Ensure deterministic actor material state
static void ActorStateIsolationChecks(Renderer::DiligentD3D11Backend& backend)
{
    using namespace Renderer;
    DiligentActorRenderer actors(backend);
    Check(actors.Initialize(),"actor order initialization");
    const auto bytes=TerrainFixture::GradientDDS(), alpha=TerrainFixture::AlphaDDS(), overrideBytes=TerrainFixture::DDS();
    auto color=LoadTerrainTextureMemory(bytes.data(),bytes.size(),actors);
    auto mask=LoadTerrainTextureMemory(alpha.data(),alpha.size(),actors);
    auto overrideTexture=LoadTerrainTextureMemory(overrideBytes.data(),overrideBytes.size(),actors);
    Check(color && mask && overrideTexture,"actor order textures");
    ActorModelSource source{20,{}};
    std::vector<StaticObjectVertex> vertices;
    for(unsigned i=0;i<5;++i) {
        const float x=-0.95f+float(i)*0.39f;
        vertices.insert(vertices.end(),{{{x,0.8f,0.4f,0,0,1,-0.3f,0.1f}},
            {{x,-0.8f,0.4f,0,0,1,-0.3f,2.1f}},{{x+0.3f,0.8f,0.4f,0,0,1,1.7f,0.1f}},
            {{x+0.3f,-0.8f,0.4f,0,0,1,1.7f,2.1f}}});
        for(uint16_t index:{0,1,2,2,1,3,0,2,1,2,3,1}) source.indices.push_back(index);
    }
    auto geometry=actors.CreateGeometry(source);
    Check(geometry!=nullptr,"actor order geometry");
    std::array<StaticObjectDraw,5> draws;
    std::array<TerrainTexturePtr,5> textures;
    const ActorCategory categories[]{ActorCategory::Player,ActorCategory::Npc,ActorCategory::Mob,ActorCategory::Npc,ActorCategory::Npc};
    unsigned pipelines=0,total=0;
    std::vector<uint32_t> lastColor,lastDepth;
    for(unsigned cycle=0;cycle<6;++cycle) {
        const unsigned width=cycle%2 ? 240 : 200,height=160;
        actors.ReleaseBindings();
        Check(backend.Resize(0,0) && !backend.BeginFrame() && backend.Resize(width,height),"actor order suspend/resize/restore");
        for(unsigned i=0;i<5;++i) {
            auto& d=draws[i]; d={};
            for(unsigned j=0;j<16;j+=5) d.matrices.world[j]=d.matrices.view[j]=d.matrices.projection[j]=d.normalTransform[j]=d.cameraAlphaTransform[j]=1;
            d.cull=StaticObjectCull((i+cycle)%3);
            d.firstIndex=i*12+(d.cull==StaticObjectCull::CounterClockwise ? 6 : 0);
            d.baseVertex=i*4; d.vertexCount=4; d.indexCount=6;
            d.depthWrite=(i+cycle)%2==0; d.blend=i==3;
            d.sampling={i%2==0,i%2!=0,i!=2,i!=2,i!=2,i%2==0};
            d.anisotropic=i==4; d.maxAnisotropy=i==4 ? 4 : 1;
            d.sampling.useMips|=d.anisotropic;
            d.ambient={0.2f+0.1f*i,0.7f-0.1f*i,0.4f,0.6f};
            d.diffuse={0.1f,0.2f,0.15f,0}; d.lightDirection={0,0,1,0};
            d.normalizeNormals=i%2!=0;
            if(i==1) { d.alphaTest=StaticObjectAlphaTest::Greater; d.alphaReference=127; d.textureAlpha=true; }
            if(i==3) { d.factorAlphaOnly=true; d.textureFactor={1,1,1,0.4f}; }
            if(i==0) {
                d.actorStage=ActorMaterialStage::Specular; d.factorAlpha=true;
                d.textureFactor={1,1,1,0.3f}; d.sphereMap=cycle%2 ? mask : overrideTexture;
                d.cameraAlphaTransform[12]=0.2f; d.cameraAlphaTransform[13]=0.4f;
            }
            if(i==2) { d.actorStage=cycle%2 ? ActorMaterialStage::Add : ActorMaterialStage::Modulate; d.textureFactor={0.2f,0.4f,0.7f,1}; }
            if(i==4) { d.fog=TerrainFog::Linear; d.fogParameters={0,2,0,0}; d.fogColor={0.1f,0.2f,0.3f,1}; }
            if(i==1) { d.pointPositionRange={0,0,2,10}; d.pointAttenuation={1,0,0,0}; d.pointDiffuse={0.2f,0.1f,0.2f,0}; }
            // Shared SRV with conflicting samplers; override SRV with the same PSO.
            textures[i]=i==1 ? mask : (i==4 && cycle%2 ? overrideTexture : color);
            pipelines|=1u<<(unsigned(d.cull)+(d.blend?3:0)+(d.depthWrite?0:6));
        }
        std::array<unsigned,5> order{0,1,2,3,4};
        std::vector<uint32_t> expectedColor,expectedDepth;
        do {
            actors.ResetFrame();
            Check(backend.BeginFrame(),"actor order frame");
            backend.Clear({true,ClearColor{0,0,0,1}});
            Check(actors.UpdateVertices(geometry,vertices),"actor order CPU pose");
            for(auto i:order) actors.Draw(&draws[i],geometry,textures[i],draws[i],categories[i]);
            Check(!actors.Failed() && actors.DrawCount()==5 && actors.VisibleActors()==5 && actors.Uploads()==1,"all five ordered material draws");
            backend.EndFrame();
            const auto actualColor=BackendTestAccess::Read(backend,false),actualDepth=BackendTestAccess::Read(backend,true);
            if(expectedColor.empty()) {
                expectedColor=actualColor; expectedDepth=actualDepth;
                for(unsigned i=0;i<5;++i) {
                    size_t visible=0,written=0;
                    const auto left=unsigned((0.05f+i*0.39f)*width/2),right=unsigned((0.35f+i*0.39f)*width/2);
                    for(unsigned y=20;y<height-20;++y) for(unsigned x=left;x<right;++x) {
                        const auto pixel=size_t(y)*width+x;
                        visible+=(actualColor[pixel]&0xffffff)!=0;
                        written+=(actualDepth[pixel]&0xffffff)<0xffffff;
                    }
                    Check(visible>200,"each actor visible; not an empty-image equality");
                    Check(draws[i].depthWrite ? written>200 : written==0,"each material owns its depth-write state");
                }
            } else {
                Check(actualColor==expectedColor,"actor color independent of prior material draw");
                Check(actualDepth==expectedDepth,"actor depth independent of prior material draw");
            }
            backend.Present(); ++total;
        } while(std::next_permutation(order.begin(),order.end()));
        lastColor=expectedColor; lastDepth=expectedDepth;
    }
    Check(pipelines==0xfff && total==720,"all 12 PSOs and 6 x 120 orders covered");
    // Same actor, different submeshes/materials must not use an actor-level bind cache.
    actors.ResetFrame(); Check(backend.BeginFrame(),"same actor submeshes frame");
    backend.Clear({true,ClearColor{0,0,0,1}});
    Check(actors.UpdateVertices(geometry,vertices),"same actor pose");
    for(unsigned i=0;i<5;++i) actors.Draw(geometry.get(),geometry,textures[i],draws[i],ActorCategory::Npc);
    Check(actors.DrawCount()==5 && actors.VisibleActors()==1,"five materials on one actor");
    backend.EndFrame();
    Check(BackendTestAccess::Read(backend,false)==lastColor && BackendTestAccess::Read(backend,true)==lastDepth,
          "submesh colors/depth equal independent actor draws");
    backend.Present();
    actors.ReleaseBindings();
    draws={}; textures={}; geometry.reset(); color.reset(); mask.reset(); overrideTexture.reset();
    Check(!actors.Failed() && !actors.LiveGeometryCount() && !actors.LiveTextureCount(),"actor order shutdown resources zero");
    DiligentActorRenderer missing(backend);
    Check(missing.Initialize(),"missing texture test initialization");
    auto missingGeometry=missing.CreateGeometry(source);
    Check(backend.BeginFrame() && missing.UpdateVertices(missingGeometry,vertices),"missing texture frame");
    StaticObjectDraw invalid; invalid.indexCount=6; invalid.vertexCount=4;
    missing.Draw(missingGeometry.get(),missingGeometry,{},invalid,ActorCategory::Npc);
    Check(missing.Failed() && missing.DrawCount()==0,"missing texture fails closed, never reuses preceding SRV");
    backend.EndFrame(); backend.Present(); missing.ReleaseBindings(); missingGeometry.reset();
    Check(!missing.LiveGeometryCount() && !missing.LiveTextureCount(),"missing texture shutdown resources zero");
    std::cout<<"Actor state isolation: 720 permutations, color/depth exact, 12 PSOs, shared/override SRV, samplers, submeshes, resize/restore, resources zero: PASS\n";
}
