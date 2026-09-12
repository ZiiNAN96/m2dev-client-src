#pragma once
// ZiiNAN: Actor upload accounting and independent instance/resource lifetime.
#include "Renderer/DiligentActorRenderer.h"
#include <limits>

static void ActorLifetimeChecks(LegacyProbe& screen, Renderer::DiligentD3D11Backend& backend)
{
    using namespace Renderer;
    DiligentActorRenderer actors(backend);
    Check(actors.Initialize(),"actor lifetime initialization");
    ActorModelSource source{3,{0,1,2}};
    std::vector<StaticObjectVertex> vertices={{{0,0,0,0,0,1,0,0}},
        {{0,-3200,0,0,0,1,0,1}},{{3200,0,0,0,0,1,1,0}}};
    auto a=actors.CreateGeometry(source), b=actors.CreateGeometry(source);
    auto image=TerrainFixture::DDS();
    auto texture=LoadTerrainTextureMemory(image.data(),image.size(),actors);
    Check(a && b && texture && actors.IndexUploads()==2,"two actor instances own fixed indices");
    StaticObjectDraw draw; draw.matrices=screen.Matrices(); draw.indexCount=3; draw.vertexCount=3;
    for(int frame=0;frame<4;++frame) {
        actors.ResetFrame(); Check(backend.BeginFrame(),"actor multiple group frame");
        backend.Clear({true,ClearColor{0,0,0,1}});
        vertices[0][2]=float(frame)*25;
        Check(actors.UpdateVertices(a,vertices) && actors.UpdateVertices(b,vertices),"two poses uploaded");
        actors.Draw(a.get(),a,texture,draw,ActorCategory::Npc); actors.Draw(a.get(),a,texture,draw,ActorCategory::Npc);
        actors.Draw(b.get(),b,texture,draw,ActorCategory::Mob);
        Check(actors.Visible(ActorCategory::Player)==0 && actors.Visible(ActorCategory::Npc)==1 &&
              actors.Visible(ActorCategory::Mob)==1,"ZiiNAN: categories count once per actor, not per group");
        Check(actors.VisibleActors()==2 && actors.Uploads()==2 && actors.VerticesUploaded()==6 &&
              actors.BytesUploaded()==192 && actors.DrawCount()==3 && actors.IndexUploads()==2,
              "actors deduplicated across material groups; no per-frame index uploads");
        backend.EndFrame(); backend.Present();
    }
    actors.ResetFrame();
    Check(!actors.VisibleActors() && !actors.Uploads() && !actors.DrawCount() &&
          actors.LiveGeometryCount()==2,"hidden actors submit nothing but retain owned buffers");
    actors.ReleaseBindings();
    std::weak_ptr<StaticObjectGeometry> lifetime=a;
    a.reset(); Check(lifetime.expired() && actors.LiveGeometryCount()==1,"delete only one actor");
    a=actors.CreateGeometry(source);
    Check(a && actors.IndexUploads()==3,"recreated actor gets one new index buffer");
    a.reset(); b.reset(); texture.reset();
    Check(!actors.LiveGeometryCount() && !actors.LiveTextureCount() && !actors.Failed(),"actor map/shutdown resources zero");
    // ZiiNAN: Invalid CPU snapshots cannot reach the GPU or silently reuse the old pose.
    for(int fault=0;fault<3;++fault) {
        DiligentActorRenderer invalid(backend); Check(invalid.Initialize(),"invalid actor snapshot initialization");
        auto own=invalid.CreateGeometry(source);
        auto bad=vertices;
        if(fault==0) bad.pop_back();
        if(fault==1) bad[0][0]=std::numeric_limits<float>::quiet_NaN();
        auto foreign=fault==2 ? actors.CreateGeometry(source) : own;
        Check(backend.BeginFrame(),"invalid actor snapshot frame");
        Check(!invalid.UpdateVertices(foreign,bad) && invalid.Failed() && !invalid.Uploads(),"invalid size/NaN/foreign actor buffer rejected");
        backend.EndFrame(); backend.Present();
    }
    Check(!actors.LiveGeometryCount(),"foreign test handles released");
    std::cout<<"Actor per-frame counters / multi-group / hidden / delete / recreate / invalid snapshot / zero lifetime: PASS\n";
}
