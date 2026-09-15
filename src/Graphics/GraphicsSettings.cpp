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
                      "TEXTURES", "HDR", "BLOOM", "MODERN_SKY", "HIGH_QUALITY_FOG", "VIEW_DISTANCE", "FOG_LEVEL"})
        if (key == name) return true;
    return false;
}
bool MatchesPreset(const GraphicsSettings& settings)
{
    return settings.preset == GraphicsPreset::Custom ||
           settings == PresetSettings(settings.preset, settings.style);
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
        s.hdr = s.bloom = s.modernSky = s.highQualityFog = true;
        break;
    case GraphicsPreset::Ultra:
        s.shadows = ShadowQuality::Ultra; s.ambientOcclusion = AmbientOcclusionQuality::GTAO;
        s.vegetation = VegetationQuality::Ultra; s.textures = TextureQuality::Ultra;
        s.hdr = s.bloom = s.modernSky = s.highQualityFog = true;
        s.viewDistance = MaxViewDistance;
        break;
    case GraphicsPreset::Custom: break;
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
    s.water = Enum(s.water, d.water, 2);
    s.vegetation = Enum(s.vegetation, d.vegetation, 3);
    s.textures = Enum(s.textures, d.textures, 2);
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
    r.vegetation = s.vegetation; r.viewDistance = s.viewDistance;
    constexpr float vegetationScale[]{.65f, .85f, 1.f, 1.25f};
    r.vegetationDistanceScale = vegetationScale[int(s.vegetation)] * s.viewDistance / DefaultViewDistance;
    constexpr float fogScale[]{.75f, 1.f, 1.25f};
    constexpr float fogDensity[]{.000006f, .000004f, .000002f};
    r.fogDistanceScale = fogScale[s.fogLevel]; r.fogDensity = fogDensity[s.fogLevel];
    r.shadowTextureSize = s.shadows == ShadowQuality::Ultra ? 2048 : s.shadows == ShadowQuality::High ? 1024 : 512;
    // G0 capabilities: AO/HDR/bloom/sky/HQ fog unavailable even in Modern.
    // Water/texture quality use the accepted current path at every requested level.
    // Add capability + style gates here when an actual G1-G8 renderer exists.
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
    while (std::getline(input, line))
    {
        std::istringstream row(line);
        if (!(row >> key) || !Known(key) || key == "VERSION") continue;
        if (!(row >> value)) { ++result.invalidValues; continue; }
        std::string tail;
        if ((row >> tail) && tail[0] != '#') { ++result.invalidValues; continue; }
        if (key == "VIEW_DISTANCE")
        {
            float parsed{};
            if (Float(value, parsed)) s.viewDistance = parsed;
            else ++result.invalidValues;
            continue;
        }
        int parsed{};
        if (!Integer(value, parsed)) { ++result.invalidValues; continue; }
        const int max = key == "PRESET" ? 4 : key == "SHADOWS" ? 5 : key == "VEGETATION" ? 3 :
            key == "AO" || key == "WATER" || key == "TEXTURES" || key == "FOG_LEVEL" ? 2 : 1;
        if (parsed < 0 || parsed > max) { ++result.invalidValues; continue; }
        if (key == "PRESET") s.preset = static_cast<GraphicsPreset>(parsed);
        else if (key == "STYLE") s.style = static_cast<GraphicsStyle>(parsed);
        else if (key == "SHADOWS") s.shadows = static_cast<ShadowQuality>(parsed);
        else if (key == "AO") s.ambientOcclusion = static_cast<AmbientOcclusionQuality>(parsed);
        else if (key == "WATER") s.water = static_cast<WaterQuality>(parsed);
        else if (key == "VEGETATION") s.vegetation = static_cast<VegetationQuality>(parsed);
        else if (key == "TEXTURES") s.textures = static_cast<TextureQuality>(parsed);
        else if (key == "HDR") s.hdr = parsed != 0;
        else if (key == "BLOOM") s.bloom = parsed != 0;
        else if (key == "MODERN_SKY") s.modernSky = parsed != 0;
        else if (key == "HIGH_QUALITY_FOG") s.highQualityFog = parsed != 0;
        else if (key == "FOG_LEVEL") s.fogLevel = parsed;
    }
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
        << "\nHIGH_QUALITY_FOG " << s.highQualityFog << "\nVIEW_DISTANCE " << s.viewDistance
        << "\nFOG_LEVEL " << s.fogLevel << '\n';
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
    s.preset = GraphicsPreset::Custom;
    return Commit(s);
}
bool Store::ApplyGraphicsPreset(GraphicsPreset preset)
{
    if (!IsOwnerThread() || int(preset) < 0 || int(preset) > 4) return false;
    if (preset == GraphicsPreset::Custom) { auto s = settings_; s.preset = preset; return Commit(s); }
    return Commit(PresetSettings(preset, settings_.style));
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
    if (s.shadows != settings_.shadows) pending_ |= ShadowsChanged;
    if (s.vegetation != settings_.vegetation) pending_ |= VegetationChanged;
    if (s.viewDistance != settings_.viewDistance) pending_ |= ViewDistanceChanged | VegetationChanged;
    if (s.fogLevel != settings_.fogLevel) pending_ |= FogChanged;
    if (s.water != settings_.water) pending_ |= WaterChanged;
    if (s.style != settings_.style || s.ambientOcclusion != settings_.ambientOcclusion ||
        s.textures != settings_.textures || s.hdr != settings_.hdr || s.bloom != settings_.bloom ||
        s.modernSky != settings_.modernSky || s.highQualityFog != settings_.highQualityFog) pending_ |= ReservedChanged;
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
