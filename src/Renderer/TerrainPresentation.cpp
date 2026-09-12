#include "TerrainPresentation.h"
#ifdef M2_ENABLE_DILIGENT_D3D11
#include "DiligentTerrainRenderer.h"
#include "DiligentStaticObjectRenderer.h"
#include "DiligentActorRenderer.h" // ZiiNAN: Same world surface and depth target.
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
    std::ofstream m_diagnostics;
    uint32_t m_frame = 0;
public:
    bool Initialize(HWND parent, uint32_t width, uint32_t height)
    {
        if (terrainRenderer || !IsWindow(parent) || !width || !height) return false;
        m_parent = parent;
        // Separate HWND/swapchain, no D3D9/D3D11 shared textures or UI compositing.
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
        const LONG_PTR style = GetWindowLongPtrW(parent, GWL_STYLE);
        m_addedClipChildren = !(style & WS_CLIPCHILDREN);
        if (m_addedClipChildren) SetWindowLongPtrW(parent, GWL_STYLE, style | WS_CLIPCHILDREN);
        terrainRenderer = m_terrain.get();
        staticObjectRenderer = m_objects.get();
        actorRenderer=m_actors.get(); // ZiiNAN: Available before original character assets load.
        // Experimental backend only; bounded frame summaries go to a file, never the console.
        m_diagnostics.open("terrain-renderer.log", std::ios::trunc);
        return true;
    }
    ~TerrainPresentation() override
    {
        // ZiiNAN: Actors must already be destroyed by the existing application teardown.
        if(m_diagnostics && m_actors)
            m_diagnostics << "shutdown actor_geometry=" << m_actors->LiveGeometryCount()
                          << " actor_textures=" << m_actors->LiveTextureCount() << std::endl;
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
        // ZiiNAN: Never compose actors into login/selection; reject stale frame snapshots.
        m_actors->ResetFrame(); ++actorFrameSerial; actorWorldFrame=false;
        if (!m_backend.BeginFrame()) return false;
        actorWorldFrame=worldWasVisible;
        m_inFrame = true;
        m_backend.Clear({true, ClearColor{0.08f, 0.16f, 0.28f, 1.0f}});
        return !m_terrain->Failed();
    }
    bool Present() override
    {
        if (!m_inFrame) return false;
        m_backend.EndFrame();
        actorWorldFrame=false; // ZiiNAN: Actor submissions only inside a world frame.
        m_inFrame = false;
        if (m_terrain->Failed() || m_objects->Failed() || m_actors->Failed()) return false;
        const bool visible = m_terrain->HasTerrain();
        if (m_diagnostics && (++m_frame % 120 == 0 || visible != m_visible))
        {
            const auto size=m_terrain->LastTextureSize();
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
