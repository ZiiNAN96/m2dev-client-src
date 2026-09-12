#pragma once
// ZiiNAN: Run the native D3D9 reference cases through the real dynamic actor owner.
#include "Renderer/DiligentActorRenderer.h"
#include <type_traits>

class ActorGpuAdapter : public Renderer::ITextureUploader
{
public:
    explicit ActorGpuAdapter(Renderer::DiligentD3D11Backend& backend, Renderer::ActorCategory category = Renderer::ActorCategory::Player) : actors(backend), category(category) {}
    bool Initialize() { return actors.Initialize(); }
    bool Failed() const { return actors.Failed(); }
    Renderer::StaticObjectGeometryPtr UploadGeometry(const Renderer::StaticObjectSource& data)
    {
        source = &data;
        return actors.CreateGeometry({static_cast<uint32_t>(data.vertices.size()), data.indices},Renderer::ActorPart::Body,category);
    }
    Renderer::TerrainTexturePtr UploadTexture(const Renderer::TerrainTextureData& data) override
    { return actors.UploadTexture(data); }
    void ResetFrame() { actors.ResetFrame(); uploaded = false; }
    void Draw(const Renderer::StaticObjectGeometryPtr& geometry,
        const Renderer::TerrainTexturePtr& texture, const Renderer::StaticObjectDraw& draw)
    {
        if (!uploaded) {
            Check(source && actors.UpdateVertices(geometry, source->vertices,0,category), "dynamic actor upload");
            uploaded = true;
        }
        actors.Draw(this, geometry, texture, draw,category);
        if(category==Renderer::ActorCategory::Mount)
            Check(actors.Visible(category)==1 && actors.MountUploads()==1 && actors.MountGeometryCount()==1 &&
                  actors.MountDraws()==actors.DrawCount(),"mount uses the same dynamic/material renderer");
        Check(actors.VisibleActors() == 1 && actors.Uploads() == 1 &&
            actors.VerticesUploaded() == source->vertices.size() &&
            actors.BytesUploaded() == source->vertices.size() * 32 && actors.IndexUploads() == 1,
            "actor counters: one VB upload per frame, immutable index reuse");
    }
    void ReleaseBindings() { actors.ReleaseBindings(); }
    uint32_t DrawCount() const { return actors.DrawCount(); }
    uint32_t LiveGeometryCount() const { return actors.LiveGeometryCount(); }
    uint32_t LiveTextureCount() const { return actors.LiveTextureCount(); }
private:
    Renderer::DiligentActorRenderer actors;
    Renderer::ActorCategory category;
    const Renderer::StaticObjectSource* source = nullptr;
    bool uploaded = false;
};

// ZiiNAN: Diligent mount actor rendering
class MountGpuAdapter : public ActorGpuAdapter
{
public:
    explicit MountGpuAdapter(Renderer::DiligentD3D11Backend& backend) : ActorGpuAdapter(backend,Renderer::ActorCategory::Mount) {}
};

// ZiiNAN: Diligent actor attachment rendering
class RigidAttachmentGpuAdapter : public Renderer::ITextureUploader
{
public:
    explicit RigidAttachmentGpuAdapter(Renderer::DiligentD3D11Backend& backend) : actors(backend) {}
    bool Initialize() { return actors.Initialize(); }
    bool Failed() const { return actors.Failed(); }
    Renderer::StaticObjectGeometryPtr UploadGeometry(const Renderer::StaticObjectSource& data)
    {
        return actors.CreateGeometry({static_cast<uint32_t>(data.vertices.size()),data.indices,0,data.vertices},Renderer::ActorPart::Weapon);
    }
    Renderer::TerrainTexturePtr UploadTexture(const Renderer::TerrainTextureData& data) override
    {
        auto texture=actors.UploadTexture(data); actors.TrackAttachmentTexture(texture); return texture;
    }
    void ResetFrame() { actors.ResetFrame(); }
    void Draw(const Renderer::StaticObjectGeometryPtr& geometry,
        const Renderer::TerrainTexturePtr& texture,const Renderer::StaticObjectDraw& draw)
    {
        actors.Draw(this,geometry,texture,draw,Renderer::ActorCategory::Player,Renderer::ActorPart::Weapon);
        Check(actors.VisibleActors()==1 && actors.VisibleAttachments()==1 && actors.Uploads()==0 &&
              actors.SkinnedVerticesUploaded()==0 && actors.IndexUploads()==1 && actors.AttachmentGeometryCount()==1,
              "rigid attachment: native world matrix, one immutable upload, no CPU skinning or per-frame upload");
    }
    void ReleaseBindings() { actors.ReleaseBindings(); }
    uint32_t DrawCount() const { return actors.DrawCount(); }
    uint32_t LiveGeometryCount() const { return actors.LiveGeometryCount(); }
    uint32_t LiveTextureCount() const { return actors.LiveTextureCount(); }
private:
    Renderer::DiligentActorRenderer actors;
};
