#include "TerrainPresentation.h"
#ifdef M2_ENABLE_DILIGENT_D3D11
#include "DiligentTerrainRenderer.h"
#include "DiligentStaticObjectRenderer.h"
#include "DiligentActorRenderer.h" // ZiiNAN: Same world surface and depth target.
#include "DiligentTreeRenderer.h" // ZiiNAN: Diligent SpeedTree rendering integration
#include "DiligentEffectRenderer.h" // ZiiNAN: Diligent effect rendering integration
#include "DiligentWorldRenderer.h" // ZiiNAN: Diligent water and special world rendering.
#include "DiligentUIRenderer.h" // ZiiNAN: UI shares the existing world surface, never a new widget system.
#include "DiligentTextRenderer.h"
#include <windows.h>
#include <fstream>

namespace Renderer
{
namespace
{
class TerrainPresentation final : public ITerrainPresentation
{
    HWND m_parent = nullptr, m_surface = nullptr;
    bool m_addedClipChildren = false, m_visible = false, m_inFrame = false;
    DiligentD3D11Backend m_backend;
    std::unique_ptr<DiligentTerrainRenderer> m_terrain;
    std::unique_ptr<DiligentStaticObjectRenderer> m_objects;
    std::unique_ptr<DiligentActorRenderer> m_actors; // ZiiNAN: Separate actor resource counters.
    std::unique_ptr<DiligentTreeRenderer> m_trees;
    std::unique_ptr<DiligentEffectRenderer> m_effects;
    std::unique_ptr<DiligentWorldRenderer> m_world;
    std::unique_ptr<DiligentUIRenderer> m_ui;
    std::unique_ptr<DiligentTextRenderer> m_text;
    std::ofstream m_diagnostics;
    uint32_t m_frame = 0;
    ScreenshotSink m_screenshot;
public:
    bool RequestScreenshot(ScreenshotSink sink) override
    {
        if(!sink || m_screenshot) return false;
        m_screenshot=std::move(sink); return true;
    }
    bool Initialize(HWND parent, uint32_t width, uint32_t height)
    {
        if (terrainRenderer || !IsWindow(parent) || !width || !height) return false;
        m_parent = parent;
        // Separate HWND/swapchain, no D3D9/D3D11 shared textures. M9 composes native UI primitives here.
        // Disabled child receives no input: existing game camera and UI handlers stay on the parent.
        m_surface = CreateWindowExW(0, L"STATIC", L"Metin2 Diligent terrain", WS_CHILD | WS_DISABLED,
                                    0, 0, width, height, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!m_surface || !m_backend.Initialize({m_surface, width, height})) return false;
        m_terrain = std::make_unique<DiligentTerrainRenderer>(m_backend);
        if (!m_terrain->Initialize()) return false;
        m_objects = std::make_unique<DiligentStaticObjectRenderer>(m_backend);
        if (!m_objects->Initialize()) return false;
        // ZiiNAN: No additional backend, window or animation runtime.
        m_actors=std::make_unique<DiligentActorRenderer>(m_backend);
        if(!m_actors->Initialize()) return false;
        m_trees=std::make_unique<DiligentTreeRenderer>(m_backend);
        if(!m_trees->Initialize()) return false;
        m_effects=std::make_unique<DiligentEffectRenderer>(m_backend);
        if(!m_effects->Initialize()) return false;
        m_world=std::make_unique<DiligentWorldRenderer>(m_backend);
        if(!m_world->Initialize()) return false;
        m_ui=std::make_unique<DiligentUIRenderer>(m_backend);
        if(!m_ui->Initialize()) return false;
        // ZiiNAN: Diligent text rendering integration; initialized before native fonts load.
        m_text=std::make_unique<DiligentTextRenderer>(m_backend);
        if(!m_text->Initialize()) return false;
        const LONG_PTR style = GetWindowLongPtrW(parent, GWL_STYLE);
        m_addedClipChildren = !(style & WS_CLIPCHILDREN);
        if (m_addedClipChildren) SetWindowLongPtrW(parent, GWL_STYLE, style | WS_CLIPCHILDREN);
        terrainRenderer = m_terrain.get();
        staticObjectRenderer = m_objects.get();
        actorRenderer=m_actors.get(); // ZiiNAN: Available before original character assets load.
        treeRenderer=m_trees.get();
        effectRenderer=m_effects.get();
        worldRenderer=m_world.get();
        uiRenderer=m_ui.get();
        textRenderer=m_text.get();
        activePresentation=this;
        // Experimental backend only; bounded frame summaries go to a file, never the console.
        m_diagnostics.open("terrain-renderer.log", std::ios::trunc);
        return true;
    }
    ~TerrainPresentation() override
    {
        if(activePresentation==this) activePresentation=nullptr;
        // ZiiNAN: Native image owners release UI handles before the renderer's final bindings/buffers.
        uiFrame=uiMode=false;
        if(textRenderer==m_text.get()) textRenderer=nullptr;
        if(m_text) {
            m_text->ResetFrame(); m_text->Shutdown();
            if(m_diagnostics) m_diagnostics << "shutdown text_textures=" << m_text->LiveTextureCount()
                << " text_buffers=" << m_text->LiveBufferCount() << std::endl;
        }
        m_text.reset();
        if(uiRenderer==m_ui.get()) uiRenderer=nullptr;
        if(m_ui) {
            m_ui->ResetFrame(); m_ui->Shutdown();
            if(m_diagnostics) m_diagnostics << "shutdown ui_textures=" << m_ui->LiveTextureCount()
                << " ui_buffers=" << m_ui->LiveBufferCount() << std::endl;
        }
        m_ui.reset();
        worldSurfaceFrame=false;
        if(worldRenderer==m_world.get()) worldRenderer=nullptr;
        if(m_world) {
            m_world->ResetFrame(); m_world->Shutdown();
            if(m_diagnostics) m_diagnostics << "shutdown water_geometry=" << waterGeometryCount.load()
                << " world_textures=" << m_world->LiveTextureCount() << " world_buffers=" << m_world->LiveBufferCount() << std::endl;
        }
        m_world.reset();
        // ZiiNAN: Effect owners release images before the renderer releases upload/PSO/SRB resources.
        effectWorldFrame=false;
        if(effectRenderer==m_effects.get()) effectRenderer=nullptr;
        if(m_effects) {
            m_effects->ResetFrame(); m_effects->Shutdown();
            if(m_diagnostics) m_diagnostics << "shutdown effect_textures=" << m_effects->LiveTextureCount()
                << " effect_buffers=" << m_effects->LiveBufferCount() << " effect_instances=" << effectRuntime.instances
                << " particle_systems=" << effectRuntime.systems << " particles=" << effectRuntime.particles << std::endl;
        }
        m_effects.reset();
        // ZiiNAN: Native map/instance owners must release their tree resources first.
        treeWorldFrame=false;
        if(m_diagnostics && m_trees)
            m_diagnostics << "shutdown tree_geometry=" << m_trees->LiveGeometryCount()
                          << " tree_textures=" << m_trees->LiveTextureCount() << std::endl;
        if(treeRenderer==m_trees.get()) treeRenderer=nullptr;
        m_trees.reset();
        // ZiiNAN: Actors must already be destroyed by the existing application teardown.
        if(m_diagnostics && m_actors)
        {
            m_diagnostics << "shutdown actor_geometry=" << m_actors->LiveGeometryCount()
                          << " actor_textures=" << m_actors->LiveTextureCount() << std::endl;
            m_diagnostics << "shutdown attachment_geometry=" << m_actors->AttachmentGeometryCount()
                          << " attachment_textures=" << m_actors->AttachmentTextureCount() << std::endl;
            // ZiiNAN: Diligent mount actor rendering
            m_diagnostics << "shutdown mount_geometry=" << m_actors->MountGeometryCount()
                          << " mount_textures=" << m_actors->MountTextureCount() << std::endl;
        }
        actorWorldFrame=false;
        if(actorRenderer==m_actors.get()) actorRenderer=nullptr;
        m_actors.reset();
        if(m_diagnostics && m_objects)
            m_diagnostics << "shutdown object_geometry=" << m_objects->LiveGeometryCount()
                          << " object_textures=" << m_objects->LiveTextureCount() << std::endl;
        if (terrainRenderer == m_terrain.get()) terrainRenderer = nullptr;
        if (staticObjectRenderer == m_objects.get()) staticObjectRenderer = nullptr;
        m_objects.reset();
        m_terrain.reset(); // Application destroys maps (and their handles) first.
        m_backend.Shutdown();
        if (IsWindow(m_surface)) DestroyWindow(m_surface);
        if (m_addedClipChildren && IsWindow(m_parent))
            SetWindowLongPtrW(m_parent, GWL_STYLE, GetWindowLongPtrW(m_parent, GWL_STYLE) & ~WS_CLIPCHILDREN);
    }
    bool BeginFrame() override
    {
        // ZiiNAN: ResetFrame clears HasTerrain; retain the last completed world boundary.
        const bool worldWasVisible=m_terrain->HasTerrain();
        m_terrain->ResetFrame();
        m_objects->ResetFrame();
        // ZiiNAN: Reject stale world snapshots; selection enters its own explicit actor scope.
        m_actors->ResetFrame(); ++actorFrameSerial; actorWorldFrame=false;
        m_trees->ResetFrame(); treeWorldFrame=false;
        m_effects->ResetFrame(); effectWorldFrame=false; ++effectFrameSerial; effectVisibleParticles=0;
        m_world->ResetFrame(); worldSurfaceFrame=false; ++worldSurfaceSerial;
        m_ui->ResetFrame(); uiFrame=uiMode=false;
        m_text->ResetFrame();
        if (!m_backend.BeginFrame()) return false;
        uiFrame=true;
        actorWorldFrame=worldWasVisible;
        treeWorldFrame=worldWasVisible;
        effectWorldFrame=worldWasVisible;
        worldSurfaceFrame=worldWasVisible;
        m_inFrame = true;
        m_backend.Clear({true, ClearColor{0.08f, 0.16f, 0.28f, 1.0f}});
        return !m_terrain->Failed();
    }
    bool Present() override
    {
        if (!m_inFrame) return false;
        if(m_screenshot) {
            std::vector<uint8_t> rgb; uint32_t width=0,height=0;
            auto sink=std::move(m_screenshot); m_screenshot={};
            const bool saved=m_backend.CaptureRGB(rgb,width,height) && sink(rgb,width,height);
            m_diagnostics<<"screenshot saved="<<saved<<" size="<<width<<'x'<<height<<std::endl;
        }
        m_backend.EndFrame();
        uiFrame=uiMode=false;
        actorWorldFrame=false; // ZiiNAN: No actor submissions outside the completed frame.
        treeWorldFrame=false;
        m_inFrame = false;
        effectWorldFrame=false;
        worldSurfaceFrame=false;
        if (m_terrain->Failed() || m_objects->Failed() || m_actors->Failed() || m_trees->Failed() || m_effects->Failed() || m_world->Failed() || m_ui->Failed() || m_text->Failed()) return false;
        const bool visible = m_terrain->HasTerrain() || m_ui->DrawCount()!=0 || m_text->DrawCount()!=0 || m_actors->DrawCount()!=0 ||
            m_world->DrawCount(WorldPart::Dungeon)!=0 || m_world->DrawCount(WorldPart::Sky)!=0;
        if (m_diagnostics && (++m_frame % 120 == 0 || visible != m_visible))
        {
            const auto size=m_terrain->LastTextureSize();
            m_diagnostics << "text_draws=" << m_text->DrawCount() << " text_vertices=" << m_text->Vertices()
                << " text_textures=" << m_text->LiveTextureCount() << std::endl;
            m_diagnostics << "ui_draws=" << m_ui->DrawCount() << " ui_quads=" << m_ui->Quads()
                << " ui_lines=" << m_ui->Lines() << " ui_vertices=" << m_ui->Vertices()
                << " ui_scissor_binds=" << m_ui->ScissorBinds() << " ui_texture_binds=" << m_ui->TextureBinds()
                << " ui_textures=" << m_ui->LiveTextureCount() << std::endl;
            m_diagnostics << "water_patches=" << m_world->DrawCount(WorldPart::Water)
                << " water_draws=" << m_world->DrawCount(WorldPart::Water) << " water_vertices=" << m_world->WaterVertices()
                << " water_indices=0 water_geometry=" << waterGeometryCount.load() << " water_textures=" << waterTexturesResident
                << " sky_draws=" << m_world->DrawCount(WorldPart::Sky) << " cloud_draws=" << m_world->DrawCount(WorldPart::Cloud)
                << " world_textures=" << m_world->LiveTextureCount() << std::endl;
            m_diagnostics << "frame=" << m_frame << " terrain=" << visible << " draws=" << m_terrain->DrawCount()
                          << " textured_draws=" << m_terrain->TexturedDrawCount() << " textures=" << m_terrain->LiveTextureCount()
                          << " uploads=" << m_terrain->TextureUploadCount() << " size=" << size[0] << 'x' << size[1]
                          << " mips=" << size[2] << std::endl;
            m_diagnostics << "splat_draws=" << m_terrain->SplatDrawCount()
                          << " color_textures=" << m_terrain->LiveTextureCount()-m_terrain->LiveAlphaCount()
                          << " alpha_textures=" << m_terrain->LiveAlphaCount()
                          << " layer_materials=" << m_terrain->LiveMaterialCount() << std::endl;
            m_diagnostics << "object_draws=" << m_objects->DrawCount()
                          << " object_geometry=" << m_objects->LiveGeometryCount()
                          << " object_textures=" << m_objects->LiveTextureCount() << std::endl;
            // ZiiNAN: Measured pose uploads and live actor resources, not performance tuning.
            m_diagnostics << "actors_visible=" << m_actors->VisibleActors() << " actor_uploads=" << m_actors->Uploads()
                          << " actor_vertices=" << m_actors->VerticesUploaded() << " actor_bytes=" << m_actors->BytesUploaded()
                          << " actor_draws=" << m_actors->DrawCount() << " actor_geometry=" << m_actors->LiveGeometryCount()
                          << " actor_textures=" << m_actors->LiveTextureCount() << " actor_index_uploads=" << m_actors->IndexUploads() << std::endl;
            // ZiiNAN: Same renderer, separate measured player/NPC/mob coverage.
            m_diagnostics << "players_visible=" << m_actors->Visible(ActorCategory::Player)
                          << " special_actors_visible=" << m_actors->Visible(ActorCategory::Special)
                          << " npcs_visible=" << m_actors->Visible(ActorCategory::Npc)
                          << " mobs_visible=" << m_actors->Visible(ActorCategory::Mob)
                          << " skinned_vertices=" << m_actors->SkinnedVerticesUploaded() << std::endl;
            // ZiiNAN: Diligent actor attachment rendering
            m_diagnostics << "attachments_visible=" << m_actors->VisibleAttachments()
                          << " weapon_draws=" << m_actors->WeaponDraws() << " shield_draws=0"
                          << " hair_draws=" << m_actors->HairDraws()
                          << " attachment_geometry=" << m_actors->AttachmentGeometryCount()
                          << " attachment_textures=" << m_actors->AttachmentTextureCount() << std::endl;
            // ZiiNAN: Diligent mount actor rendering
            m_diagnostics << "mounts_visible=" << m_actors->Visible(ActorCategory::Mount)
                          << " mounted_actors=" << m_actors->Visible(ActorCategory::MountedPlayer)
                          << " mount_draws=" << m_actors->MountDraws() << " mount_uploads=" << m_actors->MountUploads()
                          << " mount_geometry=" << m_actors->MountGeometryCount()
                          << " mount_textures=" << m_actors->MountTextureCount() << std::endl;
            // ZiiNAN: Tree draw counts are observations, not a batching/LOD change.
            m_diagnostics << "trees_visible=" << m_trees->VisibleInstances()
                          << " branch_draws=" << m_trees->DrawCount(TreePart::Branch)
                          << " frond_draws=" << m_trees->DrawCount(TreePart::Frond)
                          << " leaf_draws=" << m_trees->DrawCount(TreePart::Leaf)
                          << " billboard_draws=" << m_trees->DrawCount(TreePart::Billboard)
                          << " tree_vertices=" << m_trees->Vertices() << " tree_indices=" << m_trees->Indices()
                          << " tree_geometry=" << m_trees->LiveGeometryCount()
                          << " tree_textures=" << m_trees->LiveTextureCount() << std::endl;
            m_diagnostics << "effect_instances=" << effectRuntime.instances << " particle_systems=" << effectRuntime.systems
                          << " particles_alive=" << effectRuntime.particles << " particles_visible=" << effectVisibleParticles
                          << " particle_draws=" << m_effects->DrawCount(EffectPart::Particle)
                          << " mesh_effect_draws=" << m_effects->DrawCount(EffectPart::Mesh)
                          << " weapon_trace_draws=" << m_effects->DrawCount(EffectPart::WeaponTrace)
                          << " fly_trace_draws=" << m_effects->DrawCount(EffectPart::FlyTrace)
                          << " snow_draws=" << m_effects->DrawCount(EffectPart::Snow)
                          << " effect_vertices=" << m_effects->Vertices() << " effect_upload_bytes=" << m_effects->UploadBytes()
                          << " effect_buffer_bytes=" << m_effects->BufferBytes() << " effect_textures=" << m_effects->LiveTextureCount() << std::endl;
            m_diagnostics<<"guild_draws="<<m_world->DrawCount(WorldPart::Guild)
                         <<" dungeon_draws="<<m_world->DrawCount(WorldPart::Dungeon)
                         <<" lens_flare_draws="<<m_world->DrawCount(WorldPart::LensFlare)<<std::endl;
        }
        if (visible) m_backend.Present();
        if (visible != m_visible)
        {
            ShowWindow(m_surface, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
            m_visible = visible;
        }
        return true;
    }
    bool Resize(uint32_t width, uint32_t height) override
    {
        if (m_inFrame) return false;
        if (width && height && !SetWindowPos(m_surface, nullptr, 0, 0, width, height,
                                             SWP_NOZORDER | SWP_NOACTIVATE)) return false;
        return m_backend.Resize(width, height);
    }
    void ClearDepth(float depth) override
    {
        if(m_inFrame) { ClearInfo clear; clear.depthValue=depth; m_backend.Clear(clear); }
    }
};
}
bool IsDiligentTerrainAvailable() { return true; }
std::unique_ptr<ITerrainPresentation> CreateTerrainPresentation(void* parent, uint32_t width, uint32_t height)
{
    auto result = std::make_unique<TerrainPresentation>();
    if (!result->Initialize(static_cast<HWND>(parent), width, height)) return {};
    return result;
}
}
#else
namespace Renderer
{
bool IsDiligentTerrainAvailable() { return false; }
std::unique_ptr<ITerrainPresentation> CreateTerrainPresentation(void*, uint32_t, uint32_t) { return {}; }
}
#endif
