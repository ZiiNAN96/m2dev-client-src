#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>

// Platform-neutral settings. Only the owner (game/main) thread may access a Store.
// Asset loading never accesses it; render consumers receive resolved value copies.
namespace Graphics
{
inline constexpr unsigned ConfigVersion = 1;
inline constexpr float MinViewDistance = 6400.f;
inline constexpr float DefaultViewDistance = 25600.f;
inline constexpr float MaxViewDistance = 38400.f;
enum class GraphicsPreset { Low, Medium, High, Ultra, Custom };
enum class GraphicsStyle { Classic, Modern };
// Keep all six existing shadow modes, including old configs with solo shadows.
enum class ShadowQuality { Off, Low, LegacySolo, Medium, High, Ultra };
enum class AmbientOcclusionQuality { Off, SSAO, GTAO };
enum class WaterQuality { Low, Medium, High, Ultra };
enum class VegetationQuality { Low, Medium, High, Ultra };
enum class TextureQuality { Medium, High, Ultra };
enum class FrameRateLimit { FPS60, FPS120, Unlimited };
enum class VSync { Off, On };
constexpr unsigned FrameRate(FrameRateLimit limit)
{
    return limit == FrameRateLimit::FPS60 ? 60 : limit == FrameRateLimit::FPS120 ? 120 : 0;
}
constexpr unsigned PresentInterval(VSync vsync) { return vsync == VSync::On ? 1 : 0; }

struct GraphicsSettings
{
    GraphicsPreset preset{GraphicsPreset::Custom};
    GraphicsStyle style{GraphicsStyle::Classic};
    ShadowQuality shadows{ShadowQuality::Medium};
    AmbientOcclusionQuality ambientOcclusion{AmbientOcclusionQuality::Off};
    WaterQuality water{WaterQuality::High};
    VegetationQuality vegetation{VegetationQuality::High};
    TextureQuality textures{TextureQuality::High};
    bool hdr{}, bloom{}, modernSky{};
    float viewDistance{DefaultViewDistance};
    int fogLevel{}; // Classic-only dense / middle / light choice.
    FrameRateLimit frameRateLimit{FrameRateLimit::FPS60};
    VSync vsync{VSync::On}; // Preserve the previous Present(1) default.
    bool operator==(const GraphicsSettings&) const = default;
};

struct GraphicsRuntimeConfig
{
    std::uint64_t revision{};
    GraphicsStyle style{GraphicsStyle::Classic};
    ShadowQuality shadows{ShadowQuality::Off};
    int legacyShadowLevel{3};
    AmbientOcclusionQuality ambientOcclusion{AmbientOcclusionQuality::Off};
    WaterQuality water{WaterQuality::High};
    VegetationQuality vegetation{VegetationQuality::High};
    TextureQuality textures{TextureQuality::High};
    bool hdr{}, bloom{}, modernSky{};
    float viewDistance{DefaultViewDistance};
    float vegetationDistanceScale{1.f};
    float fogDistanceScale{.75f}, fogDensity{.000006f};
    unsigned shadowTextureSize{512}, waterFrameMilliseconds{70};
    FrameRateLimit frameRateLimit{FrameRateLimit::FPS60};
    VSync vsync{VSync::On};
};

struct GraphicsFeatures
{
    const GraphicsRuntimeConfig& config;
    bool ShadowsEnabled() const { return config.shadows != ShadowQuality::Off; }
    bool UseHDR() const { return config.hdr; }
    bool UseBloom() const { return config.bloom; }
    bool UseModernSky() const { return config.style==GraphicsStyle::Modern; }
    AmbientOcclusionQuality AOQuality() const { return config.ambientOcclusion; }
};

enum Change : std::uint32_t
{
    NoChange = 0, RendererChanged = 1, ShadowsChanged = 2,
    VegetationChanged = 4, ViewDistanceChanged = 8, FogChanged = 16,
    WaterChanged = 32, ReservedChanged = 64, FramePacingChanged = 128, AllChanged = 255
};
enum class ApplyCategory { Live, PipelineRebuild, RestartRequired };
struct GraphicsSettingsChanged
{
    std::uint32_t fields{};
    GraphicsRuntimeConfig runtime;
    // G0 needs no new PSOs. Shadow target size changes use the existing resize path.
    ApplyCategory category{ApplyCategory::Live};
    bool Has(Change field) const { return (fields & field) != 0; }
};

GraphicsSettings PresetSettings(GraphicsPreset, GraphicsStyle = GraphicsStyle::Classic);
GraphicsSettings Validate(GraphicsSettings);
GraphicsSettings MigrateLegacy(int shadowLevel, int fogLevel);
GraphicsRuntimeConfig Resolve(const GraphicsSettings&, std::uint64_t revision = 0);
struct LoadResult
{
    GraphicsSettings settings;
    bool writable{true}; // Future/unsupported versions must never be overwritten.
    unsigned invalidValues{};
};
LoadResult LoadGraphicsSettings(std::string_view text, const GraphicsSettings& defaults = {});
std::string SaveGraphicsSettings(const GraphicsSettings&, std::string_view previousText = {});

inline std::atomic_size_t liveSettingsStores{};
class Store final
{
public:
    Store();
    ~Store();
    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;
    bool IsOwnerThread() const { return owner_ == std::this_thread::get_id(); }
    const GraphicsSettings& GetGraphicsSettings() const;
    const GraphicsRuntimeConfig& GetRuntimeConfig() const;
    bool ApplyGraphicsSettings(GraphicsSettings);
    bool ApplyGraphicsPreset(GraphicsPreset);
    bool LoadGraphicsSettings(const GraphicsSettings&);
    GraphicsSettingsChanged ConsumeChanges();
private:
    bool Commit(GraphicsSettings);
    const std::thread::id owner_;
    GraphicsSettings settings_;
    GraphicsRuntimeConfig runtime_;
    std::uint32_t pending_{AllChanged};
};
}
