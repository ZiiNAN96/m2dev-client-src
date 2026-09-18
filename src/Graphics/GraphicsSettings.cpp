#include "GraphicsSettings.h"
#include <algorithm>
#include <cassert>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace Graphics
{
namespace
{
template<class E> E Enum(E value, E fallback, int max)
{
    return int(value) >= 0 && int(value) <= max ? value : fallback;
}
bool Integer(std::string_view value, int& result)
{
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    return parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size();
}
bool Float(std::string_view value, float& result)
{
    std::istringstream input{std::string(value)};
    input.imbue(std::locale::classic());
    input >> result;
    return input && input.peek() == std::char_traits<char>::eof() && std::isfinite(result);
}
bool Known(std::string_view key)
{
    for (auto name : {"VERSION", "PRESET", "STYLE", "SHADOWS", "AO", "WATER", "VEGETATION",
                      "TEXTURES", "HDR", "BLOOM", "MODERN_SKY", "HIGH_QUALITY_FOG", "VIEW_DISTANCE", "FOG_LEVEL",
                      "FRAME_RATE_LIMIT", "VSYNC", "RESOLUTION_WIDTH", "RESOLUTION_HEIGHT", "DISPLAY_MODE"})
        if (key == name) return true;
    return false;
}
bool MatchesPreset(const GraphicsSettings& settings)
{
    auto quality = settings;
    quality.frameRateLimit = FrameRateLimit::FPS60;
    quality.vsync = VSync::On;
    quality.resolutionWidth = quality.resolutionHeight = 0;
    quality.displayMode = DisplayMode::Windowed;
    return settings.preset == GraphicsPreset::Custom ||
           quality == PresetSettings(settings.preset, settings.style);
}
}

GraphicsSettings PresetSettings(GraphicsPreset preset, GraphicsStyle style)
{
    GraphicsSettings s;
    s.style = Enum(style, GraphicsStyle::Classic, 1);
    s.preset = Enum(preset, GraphicsPreset::Custom, 4);
    switch (s.preset)
    {
    case GraphicsPreset::Low:
        s.shadows = ShadowQuality::Off; s.water = WaterQuality::Low;
        s.vegetation = VegetationQuality::Low; s.textures = TextureQuality::Medium;
        s.viewDistance = 12800.f;
        break;
    case GraphicsPreset::Medium:
        s.ambientOcclusion = AmbientOcclusionQuality::SSAO; s.water = WaterQuality::Medium;
        s.vegetation = VegetationQuality::Medium; s.viewDistance = 19200.f;
        break;
    case GraphicsPreset::High:
        s.shadows = ShadowQuality::High; s.ambientOcclusion = AmbientOcclusionQuality::GTAO;
        s.hdr = s.bloom = s.modernSky = true;
        break;
    case GraphicsPreset::Ultra:
        s.water = WaterQuality::Ultra;
        s.shadows = ShadowQuality::Ultra; s.ambientOcclusion = AmbientOcclusionQuality::GTAO;
        s.vegetation = VegetationQuality::Ultra; s.textures = TextureQuality::Ultra;
        s.hdr = s.bloom = s.modernSky = true;
        s.viewDistance = MaxViewDistance;
        break;
    case GraphicsPreset::Custom: break;
    }
    if(s.style==GraphicsStyle::Modern) {
        s.hdr=true;
        if(s.preset==GraphicsPreset::Low)s.shadows=ShadowQuality::Low;
        if(s.preset==GraphicsPreset::Medium)s.modernSky=true;
    }
    return s;
}

GraphicsSettings Validate(GraphicsSettings s)
{
    const GraphicsSettings d;
    s.preset = Enum(s.preset, d.preset, 4);
    s.style = Enum(s.style, d.style, 1);
    s.shadows = Enum(s.shadows, d.shadows, 5);
    s.ambientOcclusion = Enum(s.ambientOcclusion, d.ambientOcclusion, 2);
    s.water = Enum(s.water, d.water, 3);
    s.vegetation = Enum(s.vegetation, d.vegetation, 3);
    s.textures = Enum(s.textures, d.textures, 2);
    s.frameRateLimit = Enum(s.frameRateLimit, d.frameRateLimit, 2);
    s.vsync = Enum(s.vsync, d.vsync, 1);
    s.displayMode = Enum(s.displayMode, d.displayMode, 2);
    if (s.resolutionWidth > 16384 || s.resolutionHeight > 16384)
        s.resolutionWidth = s.resolutionHeight = 0;
    s.fogLevel = std::clamp(s.fogLevel, 0, 2);
    s.viewDistance = std::isfinite(s.viewDistance) ?
        std::clamp(s.viewDistance, MinViewDistance, MaxViewDistance) : d.viewDistance;
    if (!MatchesPreset(s)) s.preset = GraphicsPreset::Custom;
    return s;
}

GraphicsSettings MigrateLegacy(int shadowLevel, int fogLevel)
{
    GraphicsSettings s;
    s.shadows = static_cast<ShadowQuality>(shadowLevel);
    s.fogLevel = fogLevel;
    return Validate(s); // Old VISIBILITY was unused; preserve the actual 25600 target.
}

GraphicsRuntimeConfig Resolve(const GraphicsSettings& requested, std::uint64_t revision)
{
    const auto s = Validate(requested);
    GraphicsRuntimeConfig r;
    r.revision = revision; r.style = s.style; r.legacyShadowLevel = int(s.shadows);
    r.frameRateLimit = s.frameRateLimit; r.vsync = s.vsync;
    r.resolutionWidth = s.resolutionWidth; r.resolutionHeight = s.resolutionHeight; r.displayMode = s.displayMode;
    r.vegetation = s.vegetation; r.viewDistance = s.viewDistance;
    constexpr float vegetationScale[]{.65f, .85f, 1.f, 1.25f};
    r.vegetationDistanceScale = vegetationScale[int(s.vegetation)] * s.viewDistance / DefaultViewDistance;
    constexpr float fogScale[]{.75f, 1.f, 1.25f};
    constexpr float fogDensity[]{.000006f, .000004f, .000002f};
    r.fogDistanceScale = fogScale[s.fogLevel]; r.fogDensity = fogDensity[s.fogLevel];
    r.shadowTextureSize = s.shadows == ShadowQuality::Ultra ? 2048 : s.shadows == ShadowQuality::High ? 1024 : 512;
    if(s.style==GraphicsStyle::Modern) {
        r.water=s.water;
        r.hdr=true;r.bloom=s.bloom;r.modernSky=s.modernSky;
        r.shadows=s.shadows;r.ambientOcclusion=s.ambientOcclusion;
        r.shadowTextureSize=s.shadows==ShadowQuality::Ultra?2048:s.shadows==ShadowQuality::High?1536:
            (s.shadows==ShadowQuality::Medium||s.shadows==ShadowQuality::LegacySolo)?1024:512;
    }
    // HDR is intrinsic to Modern. The stored legacy HDR bit is retained for
    // config compatibility; water and texture selection remain unchanged.
    return r;
}

LoadResult LoadGraphicsSettings(std::string_view text, const GraphicsSettings& defaults)
{
    LoadResult result{Validate(defaults)};
    if (text.size() > 65536) { result.writable = false; ++result.invalidValues; return result; }
    std::istringstream versionInput{std::string(text)};
    std::string line, key, value;
    int version = 0; // Unversioned sidecars are v0; same keys, missing fields use migration defaults.
    while (std::getline(versionInput, line))
    {
        std::istringstream row(line);
        if (!(row >> key) || key != "VERSION") continue;
        int parsed{};
        if (!(row >> value) || !Integer(value, parsed) || parsed < 0 || parsed > int(ConfigVersion))
        { result.writable = false; ++result.invalidValues; return result; }
        version = parsed;
    }
    (void)version;
    std::istringstream input{std::string(text)};
    auto& s = result.settings;
    bool invalidDisplay = false;
    while (std::getline(input, line))
    {
        std::istringstream row(line);
        if (!(row >> key) || !Known(key) || key == "VERSION") continue;
        const bool displayKey = key == "RESOLUTION_WIDTH" || key == "RESOLUTION_HEIGHT" || key == "DISPLAY_MODE";
        if (!(row >> value)) { ++result.invalidValues; invalidDisplay |= displayKey; continue; }
        std::string tail;
        if ((row >> tail) && tail[0] != '#') { ++result.invalidValues; invalidDisplay |= displayKey; continue; }
        if (key == "VIEW_DISTANCE")
        {
            float parsed{};
            if (Float(value, parsed)) s.viewDistance = parsed;
            else ++result.invalidValues;
            continue;
        }
        int parsed{};
        if (!Integer(value, parsed)) { ++result.invalidValues; invalidDisplay |= displayKey; continue; }
        const int max = key == "RESOLUTION_WIDTH" || key == "RESOLUTION_HEIGHT" ? 16384 : key == "DISPLAY_MODE" ? 2 : key == "PRESET" ? 4 : key == "SHADOWS" ? 5 : key == "VEGETATION" || key == "WATER" ? 3 :
            key == "AO" || key == "TEXTURES" || key == "FOG_LEVEL" || key == "FRAME_RATE_LIMIT" ? 2 : 1;
        if (parsed < 0 || parsed > max) { ++result.invalidValues; invalidDisplay |= displayKey; continue; }
        if (key == "PRESET") s.preset = static_cast<GraphicsPreset>(parsed);
        else if (key == "STYLE") s.style = static_cast<GraphicsStyle>(parsed);
        else if (key == "SHADOWS") s.shadows = static_cast<ShadowQuality>(parsed);
        else if (key == "AO") s.ambientOcclusion = static_cast<AmbientOcclusionQuality>(parsed);
        else if (key == "WATER") s.water = static_cast<WaterQuality>(parsed);
        else if (key == "VEGETATION") s.vegetation = static_cast<VegetationQuality>(parsed);
        else if (key == "TEXTURES") s.textures = static_cast<TextureQuality>(parsed);
        else if (key == "FRAME_RATE_LIMIT") s.frameRateLimit = static_cast<FrameRateLimit>(parsed);
        else if (key == "RESOLUTION_WIDTH") s.resolutionWidth = unsigned(parsed);
        else if (key == "RESOLUTION_HEIGHT") s.resolutionHeight = unsigned(parsed);
        else if (key == "DISPLAY_MODE") s.displayMode = static_cast<DisplayMode>(parsed);
        else if (key == "VSYNC") s.vsync = static_cast<VSync>(parsed);
        else if (key == "HDR") s.hdr = parsed != 0;
        else if (key == "BLOOM") s.bloom = parsed != 0;
        else if (key == "MODERN_SKY") s.modernSky = parsed != 0;
        // HIGH_QUALITY_FOG is a retired key: validate old files, then discard it.
        else if (key == "FOG_LEVEL") s.fogLevel = parsed;
    }
    if (invalidDisplay)
    { s.resolutionWidth = s.resolutionHeight = 0; s.displayMode = DisplayMode::Windowed; }
    result.settings = Validate(s);
    return result;
}

std::string SaveGraphicsSettings(const GraphicsSettings& requested, std::string_view previousText)
{
    // Retain unknown fields/comments for compatible versions; refuse future-version rewrites.
    if (!LoadGraphicsSettings(previousText).writable) return {};
    const auto s = Validate(requested);
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<float>::max_digits10);
    out << "VERSION " << ConfigVersion << "\nPRESET " << int(s.preset) << "\nSTYLE " << int(s.style)
        << "\nSHADOWS " << int(s.shadows) << "\nAO " << int(s.ambientOcclusion) << "\nWATER " << int(s.water)
        << "\nVEGETATION " << int(s.vegetation) << "\nTEXTURES " << int(s.textures)
        << "\nHDR " << s.hdr << "\nBLOOM " << s.bloom << "\nMODERN_SKY " << s.modernSky
        << "\nVIEW_DISTANCE " << s.viewDistance
        << "\nFOG_LEVEL " << s.fogLevel
        << "\nRESOLUTION_WIDTH " << s.resolutionWidth << "\nRESOLUTION_HEIGHT " << s.resolutionHeight
        << "\nDISPLAY_MODE " << int(s.displayMode)
        << "\nFRAME_RATE_LIMIT " << int(s.frameRateLimit) << "\nVSYNC " << int(s.vsync) << '\n';
    std::istringstream previous{std::string(previousText)};
    std::string line, key;
    while (std::getline(previous, line))
    {
        std::istringstream row(line);
        if (!(row >> key) || !Known(key)) out << line << '\n';
    }
    return out.str();
}

Store::Store() : owner_(std::this_thread::get_id()), runtime_(Resolve(settings_, 1)) { ++liveSettingsStores; }
Store::~Store() { --liveSettingsStores; }
const GraphicsSettings& Store::GetGraphicsSettings() const { assert(IsOwnerThread()); return settings_; }
const GraphicsRuntimeConfig& Store::GetRuntimeConfig() const { assert(IsOwnerThread()); return runtime_; }
bool Store::ApplyGraphicsSettings(GraphicsSettings s)
{
    if (!IsOwnerThread()) return false;
    s = Validate(s);
    if (s == settings_) return true;
    auto quality = s;
    quality.frameRateLimit = settings_.frameRateLimit;
    quality.vsync = settings_.vsync;
    quality.resolutionWidth = settings_.resolutionWidth; quality.resolutionHeight = settings_.resolutionHeight;
    quality.displayMode = settings_.displayMode;
    if (quality != settings_) s.preset = GraphicsPreset::Custom;
    return Commit(s);
}
bool Store::ApplyGraphicsPreset(GraphicsPreset preset)
{
    if (!IsOwnerThread() || int(preset) < 0 || int(preset) > 4) return false;
    if (preset == GraphicsPreset::Custom) { auto s = settings_; s.preset = preset; return Commit(s); }
    auto s = PresetSettings(preset, settings_.style);
    s.frameRateLimit = settings_.frameRateLimit;
    s.vsync = settings_.vsync;
    s.resolutionWidth = settings_.resolutionWidth; s.resolutionHeight = settings_.resolutionHeight;
    s.displayMode = settings_.displayMode;
    return Commit(s);
}
bool Store::LoadGraphicsSettings(const GraphicsSettings& settings)
{
    if (!IsOwnerThread()) return false;
    return Commit(Validate(settings));
}
bool Store::Commit(GraphicsSettings s)
{
    if (s == settings_) return true;
    pending_ |= RendererChanged;
    if (s.resolutionWidth != settings_.resolutionWidth || s.resolutionHeight != settings_.resolutionHeight ||
        s.displayMode != settings_.displayMode) pending_ |= DisplayChanged;
    if (s.frameRateLimit != settings_.frameRateLimit || s.vsync != settings_.vsync) pending_ |= FramePacingChanged;
    if (s.shadows != settings_.shadows) pending_ |= ShadowsChanged;
    if (s.vegetation != settings_.vegetation) pending_ |= VegetationChanged;
    if (s.viewDistance != settings_.viewDistance) pending_ |= ViewDistanceChanged | VegetationChanged;
    if (s.fogLevel != settings_.fogLevel) pending_ |= FogChanged;
    if (s.water != settings_.water) pending_ |= WaterChanged;
    if (s.style != settings_.style || s.ambientOcclusion != settings_.ambientOcclusion ||
        s.textures != settings_.textures || s.hdr != settings_.hdr || s.bloom != settings_.bloom ||
        s.modernSky != settings_.modernSky) pending_ |= ReservedChanged;
    settings_ = s; runtime_ = Resolve(s, runtime_.revision + 1);
    return true;
}
GraphicsSettingsChanged Store::ConsumeChanges()
{
    if (!IsOwnerThread()) return {};
    GraphicsSettingsChanged event{pending_, runtime_};
    pending_ = NoChange;
    return event;
}
}
