#pragma once
// ZiiNAN: Actor upload accounting and independent instance/resource lifetime.
#include "Renderer/DiligentActorRenderer.h"
#include <limits>

template<class CameraProbe>
static void ActorLifetimeChecks(CameraProbe& screen, Renderer::DiligentD3D11Backend& backend)
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
    // ZiiNAN: Diligent actor attachment rendering
    source.rigidVertices=vertices;
    for(int cycle=0;cycle<3;++cycle) {
        auto weapon=actors.CreateGeometry(source,ActorPart::Weapon);
        auto left=actors.CreateGeometry(source,ActorPart::WeaponLeft);
        ActorModelSource skinned{3,{0,1,2},3,{}};
        auto hair=actors.CreateGeometry(skinned,ActorPart::Hair);
        auto sharedTexture=LoadTerrainTextureMemory(image.data(),image.size(),actors);
        actors.TrackAttachmentTexture(sharedTexture); actors.TrackAttachmentTexture(sharedTexture);
        Check(weapon && left && hair && sharedTexture && actors.AttachmentGeometryCount()==3 &&
              actors.AttachmentTextureCount()==1,"attachment slots own meshes, tracked shared image deduplicated");
        for(int frame=0;frame<3;++frame) {
            actors.ResetFrame(); Check(backend.BeginFrame(),"attachment sync frame");
            Check(actors.UpdateVertices(hair,vertices,3),"only skinned hair is uploaded");
            actors.Draw(&actors,weapon,sharedTexture,draw,ActorCategory::Player,ActorPart::Weapon);
            actors.Draw(&actors,weapon,sharedTexture,draw,ActorCategory::Player,ActorPart::Weapon);
            actors.Draw(&actors,left,sharedTexture,draw,ActorCategory::Player,ActorPart::WeaponLeft);
            actors.Draw(&actors,hair,sharedTexture,draw,ActorCategory::Player,ActorPart::Hair);
            Check(actors.VisibleAttachments()==3 && actors.WeaponDraws()==3 && actors.HairDraws()==1 &&
                  actors.Uploads()==1 && actors.SkinnedVerticesUploaded()==3,"attachment groups count parts once, rigid never reskinned");
            backend.EndFrame(); backend.Present();
        }
        actors.ResetFrame(); Check(!actors.VisibleAttachments() && !actors.WeaponDraws() && !actors.HairDraws(),"hidden parts disappear together");
        actors.ReleaseBindings(); weapon.reset(); left.reset(); hair.reset(); sharedTexture.reset();
        Check(!actors.AttachmentGeometryCount() && !actors.AttachmentTextureCount() &&
              !actors.LiveGeometryCount() && !actors.LiveTextureCount(),"equipment swap/despawn/map change releases all attachment resources");
    }
    // ZiiNAN: Diligent mount actor rendering
    for(int cycle=0;cycle<3;++cycle) {
        ActorModelSource skinned{3,{0,1,2},3,{}};
        auto mount=actors.CreateGeometry(skinned,ActorPart::Body,ActorCategory::Mount);
        auto rider=actors.CreateGeometry(skinned,ActorPart::Body,ActorCategory::MountedPlayer);
        auto hair=actors.CreateGeometry(skinned,ActorPart::Hair,ActorCategory::MountedPlayer);
        auto weapon=actors.CreateGeometry(source,ActorPart::Weapon,ActorCategory::MountedPlayer);
        auto mountTexture=LoadTerrainTextureMemory(image.data(),image.size(),actors);
        auto riderTexture=LoadTerrainTextureMemory(image.data(),image.size(),actors);
        actors.TrackMountTexture(mountTexture); actors.TrackMountTexture(mountTexture);
        actors.TrackAttachmentTexture(riderTexture);
        Check(mount && rider && hair && weapon && mountTexture && riderTexture,"mount/rider resources created");
        for(int frame=0;frame<3;++frame) {
            actors.ResetFrame(); Check(backend.BeginFrame(),"mount/rider frame");
            Check(actors.UpdateVertices(mount,vertices,3,ActorCategory::Mount) &&
                  actors.UpdateVertices(rider,vertices,3,ActorCategory::MountedPlayer) &&
                  actors.UpdateVertices(hair,vertices,3,ActorCategory::MountedPlayer),"native parent then rider/hair uploads");
            actors.Draw(mount.get(),mount,mountTexture,draw,ActorCategory::Mount);
            actors.Draw(mount.get(),mount,mountTexture,draw,ActorCategory::Mount);
            actors.Draw(rider.get(),rider,riderTexture,draw,ActorCategory::MountedPlayer);
            actors.Draw(rider.get(),weapon,riderTexture,draw,ActorCategory::MountedPlayer,ActorPart::Weapon);
            actors.Draw(rider.get(),hair,riderTexture,draw,ActorCategory::MountedPlayer,ActorPart::Hair);
            Check(actors.VisibleActors()==2 && actors.Visible(ActorCategory::Mount)==1 &&
                  actors.Visible(ActorCategory::MountedPlayer)==1 && actors.Visible(ActorCategory::Player)==1 &&
                  actors.VisibleAttachments()==2 && actors.MountDraws()==2 && actors.MountUploads()==1 &&
                  actors.Uploads()==3 && actors.MountGeometryCount()==1 && actors.MountTextureCount()==1,
                  "mount separate from rider/attachments; one upload despite multiple material groups");
            backend.EndFrame(); backend.Present();
        }
        actors.ResetFrame();
        Check(!actors.MountDraws() && !actors.MountUploads() && !actors.VisibleActors(),"hidden mount and rider submit nothing");
        actors.ReleaseBindings(); mount.reset(); mountTexture.reset();
        Check(!actors.MountGeometryCount() && !actors.MountTextureCount() && actors.LiveGeometryCount()==3,
              "dismount frees only the mount, not rider attachments");
        rider.reset(); hair.reset(); weapon.reset(); riderTexture.reset();
        Check(!actors.LiveGeometryCount() && !actors.LiveTextureCount() && !actors.AttachmentGeometryCount() &&
              !actors.AttachmentTextureCount() && !actors.Failed(),"mount change/map/shutdown all resources zero");
    }
    std::cout<<"Mount/rider/attachments / material groups / hide / dismount / repeated lifetime: PASS\n";
    std::cout<<"Actor per-frame counters / multi-group / hidden / delete / recreate / invalid snapshot / zero lifetime: PASS\n";
}
