#include "Graphics/GraphicsSettingsFile.h"
#include "Vegetation/VegetationRuntime.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

using namespace Graphics;
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void Presets()
{
    const ShadowQuality shadow[]{ShadowQuality::Off, ShadowQuality::Medium, ShadowQuality::High, ShadowQuality::Ultra};
    const AmbientOcclusionQuality ao[]{AmbientOcclusionQuality::Off, AmbientOcclusionQuality::SSAO, AmbientOcclusionQuality::GTAO, AmbientOcclusionQuality::GTAO};
    const float distance[]{12800,19200,25600,38400};
    for (int i = 0; i < 4; ++i)
    {
        const auto p = static_cast<GraphicsPreset>(i);
        const auto s = PresetSettings(p);
        Check(s == PresetSettings(p) && s == Validate(s), "deterministic complete preset");
        Check(s.preset == p && s.shadows == shadow[i] && s.ambientOcclusion == ao[i], "preset shadow/AO");
        Check(s.viewDistance == distance[i] && int(s.vegetation) == i, "preset view/vegetation");
        Check(s.water == (i == 0 ? WaterQuality::Low : i == 1 ? WaterQuality::Medium : WaterQuality::High), "preset water");
        Check(s.textures == (i == 0 ? TextureQuality::Medium : i == 3 ? TextureQuality::Ultra : TextureQuality::High), "preset texture");
        Check(s.hdr == (i >= 2) && s.bloom == (i >= 2) && s.modernSky == (i >= 2) && s.highQualityFog == (i >= 2), "preset reserved flags");
        Check(s.style == GraphicsStyle::Classic && s.fogLevel == 0, "presets retain classic by default");
        for (auto style : {GraphicsStyle::Classic, GraphicsStyle::Modern})
        {
            const auto r = Resolve(PresetSettings(p, style));
            const GraphicsFeatures features{r};
            Check(features.ShadowsEnabled()==(style==GraphicsStyle::Modern&&i!=0)&&
                features.AOQuality()==(style==GraphicsStyle::Modern?ao[i]:AmbientOcclusionQuality::Off), "style-gated FX shadows/AO");
            Check(!features.UseHDR() && !features.UseBloom() && !features.UseModernSky(), "future feature gates");
            Check(r.water == WaterQuality::High && r.waterFrameMilliseconds == 70 && r.textures == TextureQuality::High, "existing water/texture path");
        }
    }
    for (int shadowLevel = 0; shadowLevel <= 5; ++shadowLevel)
        for (int fog = 0; fog <= 2; ++fog)
        {
            const auto s = MigrateLegacy(shadowLevel, fog);
            Check(int(s.shadows) == shadowLevel && s.fogLevel == fog && s.preset == GraphicsPreset::Custom, "legacy six-mode migration");
            const auto r = Resolve(s);
            Check(r.vegetationDistanceScale == 1.f && r.viewDistance == 25600 && r.legacyShadowLevel == shadowLevel, "classic compatible defaults");
        }
}
void Custom()
{
    Store store;
    Check(store.ApplyGraphicsPreset(GraphicsPreset::High), "apply high");
    const auto revision = store.GetRuntimeConfig().revision;
    Check(store.ApplyGraphicsPreset(GraphicsPreset::High) && store.GetRuntimeConfig().revision == revision, "same preset no-op");
    auto s = store.GetGraphicsSettings(); s.shadows = ShadowQuality::Ultra;
    Check(store.ApplyGraphicsSettings(s) && store.GetGraphicsSettings().preset == GraphicsPreset::Custom, "single option becomes custom");
    s.shadows = ShadowQuality::High;
    store.ApplyGraphicsSettings(s);
    Check(store.GetGraphicsSettings().preset == GraphicsPreset::Custom, "custom stays explicit");
    store.ApplyGraphicsPreset(GraphicsPreset::Low);
    s = store.GetGraphicsSettings(); s.style = GraphicsStyle::Modern;
    store.ApplyGraphicsSettings(s);
    Check(store.GetGraphicsSettings().preset == GraphicsPreset::Custom, "style change becomes custom");
    store.ApplyGraphicsPreset(GraphicsPreset::Medium);
    Check(store.GetGraphicsSettings().style == GraphicsStyle::Modern, "preset retains explicit style");
    auto bad = PresetSettings(GraphicsPreset::Ultra); bad.hdr = false;
    Check(Validate(bad).preset == GraphicsPreset::Custom, "inconsistent loaded preset becomes custom");
    Check(!store.ApplyGraphicsPreset(static_cast<GraphicsPreset>(-1)), "invalid preset rejected without changes");
}
void Invalid()
{
    const auto legacy = MigrateLegacy(2, 1);
    auto loaded = LoadGraphicsSettings("VERSION 1\nPRESET 99\nSTYLE -1\nSHADOWS 99\nAO 3\nWATER 3\nVEGETATION -8\nTEXTURES 999\nHDR 8\nBLOOM nope\nMODERN_SKY -1\nHIGH_QUALITY_FOG 42\nFOG_LEVEL 8\nVIEW_DISTANCE nan\n", legacy);
    Check(loaded.settings == legacy && loaded.invalidValues == 13, "all invalid values use migration defaults");
    for (auto text : {"VIEW_DISTANCE inf", "VIEW_DISTANCE 1e1000", "VIEW_DISTANCE 100oops", "SHADOWS 99999999999999999999999", "HDR", "SHADOWS 2 garbage"})
        Check(LoadGraphicsSettings(text, legacy).settings == legacy, "malformed token fallback");
    Check(LoadGraphicsSettings("VIEW_DISTANCE -10").settings.viewDistance == MinViewDistance, "min clamp");
    Check(LoadGraphicsSettings("VIEW_DISTANCE 1e9").settings.viewDistance == MaxViewDistance, "max clamp");
    auto s = legacy; s.viewDistance = std::numeric_limits<float>::quiet_NaN();
    Check(Validate(s).viewDistance == DefaultViewDistance, "API NaN safe");
    s.shadows = static_cast<ShadowQuality>(-10); s.fogLevel = -99;
    Check(Validate(s).shadows == ShadowQuality::Medium && Validate(s).fogLevel == 0, "API enum/fog safe");
    for (auto version : {"VERSION 2\nSHADOWS 5", "VERSION bad", "VERSION -1", "VERSION 999\nVERSION 1"})
    {
        loaded = LoadGraphicsSettings(version, legacy);
        Check(!loaded.writable && loaded.settings == legacy && SaveGraphicsSettings({}, version).empty(), "unsupported version protected");
    }
    Check(!LoadGraphicsSettings(std::string(65537, 'x')).writable, "bounded config parser");
    Check(LoadGraphicsSettings("# comment\nUNKNOWN 9\nSHADOWS 4\n", legacy).settings.shadows == ShadowQuality::High, "v0 migration/unknown values");
    const auto output = SaveGraphicsSettings(legacy, "VERSION 1\n# keep me\nFUTURE_VALUE custom text\nSHADOWS 5\n");
    Check(output.find("# keep me") != output.npos && output.find("FUTURE_VALUE custom text") != output.npos &&
        LoadGraphicsSettings(output).settings == legacy, "unknown fields preserved and known fields canonicalized");
}
void LiveApply()
{
    Store store;
    Check(store.ConsumeChanges().fields == AllChanged && store.ConsumeChanges().fields == 0, "initial event then clean");
    auto s = store.GetGraphicsSettings(); s.fogLevel = 2;
    store.ApplyGraphicsSettings(s);
    auto event = store.ConsumeChanges();
    Check(event.fields == (RendererChanged | FogChanged) && event.runtime.fogDistanceScale == 1.25f, "targeted fog live apply");
    s.viewDistance = 12800; store.ApplyGraphicsSettings(s);
    event = store.ConsumeChanges();
    Check(event.Has(ViewDistanceChanged) && event.Has(VegetationChanged) && !event.Has(ShadowsChanged), "targeted view/vegetation notification");
    const auto revision = event.runtime.revision;
    bool applied = true, preset = true, loaded = true;
    std::thread worker([&] { applied = store.ApplyGraphicsSettings({}); preset = store.ApplyGraphicsPreset(GraphicsPreset::Ultra);
        loaded = store.LoadGraphicsSettings({}); Check(store.ConsumeChanges().fields == 0, "worker cannot consume events"); });
    worker.join();
    Check(!applied && !preset && !loaded && store.GetRuntimeConfig().revision == revision, "worker mutations rejected before access");
    store.ApplyGraphicsSettings(store.GetGraphicsSettings());
    Check(store.ConsumeChanges().fields == 0, "no-op apply has no rebuild/event");
    for (int i = 0; i < 4; ++i)
    {
        store.ApplyGraphicsPreset(static_cast<GraphicsPreset>(i));
        event = store.ConsumeChanges();
        Check(event.category == ApplyCategory::Live && event.runtime.revision > revision, "presets apply without PSO/restart");
    }
}
void VegetationTest()
{
    auto asset = std::make_shared<Vegetation::Asset>();
    auto& m = asset->metadata;
    m.nearDistance = 10; m.farDistance = 50; m.cullDistance = 100;
    m.renderBounds = {{-1,-1,0},{1,1,5},true}; m.lods.resize(2);
    m.lods[0].meshes[4] = 1; m.lods[1].meshes[0] = 0;
    Vegetation::Instance tree(asset, Vegetation::Identity);
    const auto compatible = Resolve(MigrateLegacy(3,0));
    Check(tree.Update({20,0,0}, {}, compatible.vegetationDistanceScale), "compatible tree visible");
    Check(tree.lod.meshes == Vegetation::SelectLOD(m,20).meshes, "classic LOD unchanged");
    const auto high = Resolve(PresetSettings(GraphicsPreset::High));
    const auto low = Resolve(PresetSettings(GraphicsPreset::Low));
    const auto ultra = Resolve(PresetSettings(GraphicsPreset::Ultra));
    Check(tree.Update({80,0,0}, {}, high.vegetationDistanceScale), "high distance visible");
    Check(!tree.Update({80,0,0}, {}, low.vegetationDistanceScale), "low culls at resolved distance");
    Check(tree.Update({120,0,0}, {}, ultra.vegetationDistanceScale), "ultra extends existing asset range");
    Check(tree.Update({80,0,0}, {}, high.vegetationDistanceScale), "live restore without asset recreation");
    Check(!tree.Update({0,0,0}, {}, 0), "invalid scale fails safely");
}
void Persistence()
{
    for (int preset = 0; preset <= 4; ++preset)
    {
        auto s = PresetSettings(static_cast<GraphicsPreset>(preset), GraphicsStyle::Modern);
        Check(LoadGraphicsSettings(SaveGraphicsSettings(s)).settings == s, "all preset values roundtrip");
    }
    auto s = PresetSettings(GraphicsPreset::Ultra, GraphicsStyle::Modern);
    s.preset = GraphicsPreset::Custom; s.fogLevel = 2; s.viewDistance = 12345.6787f;
    std::string error;
    const auto file = std::filesystem::path("graphics-persistence.cfg");
    Check(SaveGraphicsSettingsFile(file, s, error), error.c_str());
    Check(LoadGraphicsSettingsFile(file, {}, error).settings == s && error.empty(), "real file roundtrip");
    s.shadows = ShadowQuality::LegacySolo;
    Check(SaveGraphicsSettingsFile(file, s, error), "atomic replacement existing file");
    Check(LoadGraphicsSettingsFile(file, {}, error).settings == s, "atomic update readback");
    { std::ofstream out(file); out << "VERSION 9\nFUTURE_SETTING do-not-change\n"; }
    Check(!SaveGraphicsSettingsFile(file, s, error), "future file save blocked");
    std::ifstream in(file); const std::string text{std::istreambuf_iterator<char>(in), {}};
    Check(text == "VERSION 9\nFUTURE_SETTING do-not-change\n", "future file byte preservation");
    in.close(); std::filesystem::remove(file);
    Check(!SaveGraphicsSettingsFile("missing-g0x-directory/graphics.cfg", s, error), "write failure propagated");
    Check(LoadGraphicsSettingsFile("absent-g0x.cfg", s, error).settings == s && error.empty(), "absent file migrates");
}
int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "mode required"); const std::string mode = argv[1];
        if (mode == "Presets") Presets();
        else if (mode == "Custom") Custom();
        else if (mode == "Invalid") Invalid();
        else if (mode == "LiveApply") LiveApply();
        else if (mode == "Vegetation") VegetationTest();
        else if (mode == "Persistence") Persistence();
        else if (mode == "WriteRestart" || mode == "ReadRestart")
        {
            auto s = PresetSettings(GraphicsPreset::Ultra, GraphicsStyle::Modern);
            s.preset = GraphicsPreset::Custom; s.shadows = ShadowQuality::LegacySolo; s.fogLevel = 1; s.viewDistance = 17555.125f;
            std::string error;
            if (mode == "WriteRestart") Check(SaveGraphicsSettingsFile("graphics-restart.cfg", s, error), error.c_str());
            else { Store store; const auto result = LoadGraphicsSettingsFile("graphics-restart.cfg", {}, error);
                Check(error.empty() && result.settings == s, "new process persisted every setting");
                store.LoadGraphicsSettings(result.settings); Check(store.GetGraphicsSettings() == s, "new process store rehydrated"); }
        }
        else Check(false, "unknown mode");
        Check(liveSettingsStores == 0 && Vegetation::liveInstances == 0 && Vegetation::liveAssets == 0, "zero settings/vegetation objects");
        std::cout << "PASS " << mode << " GraphicsSettingsObjects=0 VegetationInstances=0 VegetationAssets=0\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
