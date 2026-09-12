#pragma once
// ZiiNAN: Run the native D3D9 reference cases through the real dynamic actor owner.
#include "Renderer/DiligentActorRenderer.h"
#include <type_traits>

class ActorGpuAdapter : public Renderer::ITextureUploader
{
public:
    explicit ActorGpuAdapter(Renderer::DiligentD3D11Backend& backend) : actors(backend) {}
    bool Initialize() { return actors.Initialize(); }
    bool Failed() const { return actors.Failed(); }
    Renderer::StaticObjectGeometryPtr UploadGeometry(const Renderer::StaticObjectSource& data)
    {
        source = &data;
        return actors.CreateGeometry({static_cast<uint32_t>(data.vertices.size()), data.indices});
    }
    Renderer::TerrainTexturePtr UploadTexture(const Renderer::TerrainTextureData& data) override
    { return actors.UploadTexture(data); }
    void ResetFrame() { actors.ResetFrame(); uploaded = false; }
    void Draw(const Renderer::StaticObjectGeometryPtr& geometry,
        const Renderer::TerrainTexturePtr& texture, const Renderer::StaticObjectDraw& draw)
    {
        if (!uploaded) {
            Check(source && actors.UpdateVertices(geometry, source->vertices), "dynamic actor upload");
            uploaded = true;
        }
        actors.Draw(this, geometry, texture, draw);
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
    const Renderer::StaticObjectSource* source = nullptr;
    bool uploaded = false;
};
