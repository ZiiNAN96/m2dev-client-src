"""Prepare a minimal, reproducible build of the pinned upstream FX sources.

Sibling-repository includes and the SSAO Imgui editor are removed; a readiness
getter exposes placeholder use. Cascade caster-depth padding preserves the
receiver bias in world units. Original notices and FX algorithms are retained.
"""
from pathlib import Path
import re
import sys

source, destination = map(Path, sys.argv[1:3])
files = [
    "Components/interface/ShadowMapManager.hpp",
    "Components/src/ShadowMapManager.cpp",
    "PostProcess/Common/interface/PostFXContext.hpp",
    "PostProcess/Common/interface/PostFXRenderTechnique.hpp",
    "PostProcess/Common/src/PostFXContext.cpp",
    "PostProcess/Common/src/PostFXRenderTechnique.cpp",
    "PostProcess/Common/src/SamplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp.cpp",
    "PostProcess/ScreenSpaceAmbientOcclusion/interface/ScreenSpaceAmbientOcclusion.hpp",
    "PostProcess/ScreenSpaceAmbientOcclusion/src/ScreenSpaceAmbientOcclusion.cpp",
    "PostProcess/Bloom/interface/Bloom.hpp",
    "PostProcess/Bloom/src/Bloom.cpp",
    "PostProcess/ScreenSpaceReflection/interface/ScreenSpaceReflection.hpp",
    "PostProcess/ScreenSpaceReflection/src/ScreenSpaceReflection.cpp",
    "Shaders/PostProcess/TemporalAntiAliasing/public/TemporalAntiAliasingStructures.fxh",
    "Utilities/interface/DiligentFXShaderSourceStreamFactory.hpp",
    "Utilities/src/DiligentFXShaderSourceStreamFactory.cpp",
    "License.txt",
]
for group in ("Common", "PBR", "Shadows", "PostProcess/ScreenSpaceAmbientOcclusion", "PostProcess/ScreenSpaceReflection", "PostProcess/Bloom", "PostProcess/ToneMapping"):
    files.extend(str(p.relative_to(source)).replace("\\", "/")
                 for p in (source / "Shaders" / group).rglob("*") if p.is_file())

# Only the atmosphere's physical tables and lookup helpers: no epipolar
# screen-space sampling, shadowed shafts, luminance/history or demo frontend.
atmosphere = "Shaders/PostProcess/EpipolarLightScattering/"
files.extend(atmosphere + name for name in (
    "public/EpipolarLightScatteringStructures.fxh",
    "public/EpipolarLightScatteringFunctions.fxh",
    "private/AtmosphereShadersCommon.fxh", "private/LookUpTables.fxh",
    "private/ScatteringIntegrals.fxh", "private/Extinction.fxh",
    "private/precompute/PrecomputeCommon.fxh",
    "private/precompute/PrecomputeNetDensityToAtmTop.fx",
    "private/precompute/PrecomputeSingleScattering.fx"))

unused_frontends = {"RenderPBR.psh", "RenderPBR.vsh", "RenderUnshaded.psh", "BoundBox.psh", "EnvMap.psh"}
files = sorted(set(relative for relative in files if Path(relative).name not in unused_frontends))
shader_names = {Path(relative).name for relative in files if relative.startswith("Shaders/")}
shader_manifest = []
for relative in files:
    text = (source / relative).read_text(encoding="utf-8-sig")
    if relative.startswith("Shaders/"):
        for include in re.findall(r'^\s*#\s*include\s+"([^"]+)"', text, re.MULTILINE):
            assert include in shader_names, f"Missing FX shader dependency: {relative} -> {include}"
        shader_manifest.append((destination / relative).absolute().as_posix())
    text = re.sub(r'"(?:\.\./)+DiligentCore/', '"', text)
    if relative.endswith("SSR_ComputeTemporalAccumulation.fx"):
        marker = "    float4 Position = VSOut.f4PixelPos;"
        assert text.count(marker) == 1
        # A true spatial-only mode: avoid even reading unavailable history.
        # Merely lerping history by zero can still propagate NaNs or history-
        # dependent variance through the upstream disocclusion branch.
        text = text.replace(marker, marker + '''
    if (g_SSRAttribs.TemporalRadianceStabilityFactor == 0.0 &&
        g_SSRAttribs.TemporalVarianceStabilityFactor == 0.0)
    {
        PSOutput Spatial;
        Spatial.Radiance = SampleCurrRadiance(int2(Position.xy));
        Spatial.Variance = SampleCurrVariance(int2(Position.xy));
        return Spatial;
    }
''')
    if relative.endswith("ScreenSpaceReflection.cpp"):
        marker = "bool AllPSOsReady = PrepareShadersAndPSO(RenderAttribs, m_FeatureFlags) && RenderAttribs.pPostFXContext->IsPSOsReady();"
        assert text.count(marker) == 1
        text = text.replace(marker, marker + "\n    m_PSOsReady = AllPSOsReady;")
        for include in ('#include "imgui.h"', '#include "ImGuiUtils.hpp"'):
            assert text.count(include) == 1
            text = text.replace(include, "")
        start = text.index("bool ScreenSpaceReflection::UpdateUI(")
        brace = text.index("{", start)
        depth, end = 1, brace + 1
        while depth:
            depth += (text[end] == "{") - (text[end] == "}")
            end += 1
        text = text[:start] + text[end:]
    if relative.endswith("ScreenSpaceReflection.hpp"):
        text, count = re.subn(r"    static bool UpdateUI\([^;]+;\n", "", text)
        assert count == 1
        text = text.replace("    void Execute(const RenderAttributes& RenderAttribs);",
            "    void Execute(const RenderAttributes& RenderAttribs);\n    bool IsPSOsReady() const { return m_PSOsReady; }")
        text = text.replace("    CreateInfo    m_Settings;", "    CreateInfo    m_Settings;\n    bool m_PSOsReady = false;")
        text = text.replace("    ITextureView* GetSSRRadianceSRV() const;", "    ITextureView* GetSSRRadianceSRV() const;\n    ITextureView* GetIntersectionRadianceSRV() const { return m_Resources[RESOURCE_IDENTIFIER_RADIANCE].GetTextureSRV(); }\n    ITextureView* GetRoughnessSRV() const { return m_Resources[RESOURCE_IDENTIFIER_ROUGHNESS].GetTextureSRV(); }")
    if relative.endswith("ShadowMapManager.hpp"):
        marker = "        float              fPartitioningFactor = 0.95f;"
        assert text.count(marker) == 1
        text = text.replace(marker, marker + "\n        float LightSpaceDepthPadding = 0.f; // Game-loaded caster depth beyond receiver bounds.")
    if relative.endswith("ShadowMapManager.cpp"):
        marker = "        float3 f3CascadeExtent = f3MaxXYZ - f3MinXYZ;"
        assert text.count(marker) == 1
        text = text.replace(marker,
            "        const float ReceiverDepth = f3MaxXYZ.z - f3MinXYZ.z;\n"
            "        CurrCascade.f4CasterDepthBiasScale = float4(ReceiverDepth / (ReceiverDepth + Info.LightSpaceDepthPadding), 1, 1, 1);\n"
            "        f3MinXYZ.z -= Info.LightSpaceDepthPadding;\n" + marker)
    if relative.endswith("BasicStructures.fxh"):
        marker = "    float4 f4MarginProjSpace;"
        assert text.count(marker) == 1
        text = text.replace(marker, marker + "\n    float4 f4CasterDepthBiasScale; // Keep receiver bias in world units after caster extrusion.")
    if relative.endswith("Shadows.fxh"):
        marker = "abs(f2DepthSlopeScaledBias.xy) ) + ShadowAttribs.fFixedDepthBias;"
        assert text.count(marker) == 1
        text = text.replace(marker, "abs(f2DepthSlopeScaledBias.xy) ) + ShadowAttribs.fFixedDepthBias * ShadowAttribs.Cascades[SamplingInfo.iCascadeIdx].f4CasterDepthBiasScale.x;")
    if relative.endswith("Bloom.cpp"):
        marker = "bool AllPSOsReady = PrepareShadersAndPSO(RenderAttribs, m_FeatureFlags) && RenderAttribs.pPostFXContext->IsPSOsReady();"
        assert text.count(marker) == 1
        text = text.replace(marker, marker + "\n    m_PSOsReady = AllPSOsReady;")
        for include in ('#include "imgui.h"', '#include "ImGuiUtils.hpp"', '#include "ScreenSpaceReflection.hpp"'):
            assert text.count(include) == 1
            text = text.replace(include, "")
        start = text.index("bool Bloom::UpdateUI(")
        brace = text.index("{", start)
        depth, end = 1, brace + 1
        while depth:
            depth += (text[end] == "{") - (text[end] == "}")
            end += 1
        text = text[:start] + text[end:]
    if relative.endswith("Bloom.hpp"):
        text, count = re.subn(r"    static bool UpdateUI\([^;]+;\n", "", text)
        assert count == 1
        marker = "    void Execute(const RenderAttributes& RenderAttribs);"
        assert text.count(marker) == 1
        text = text.replace(marker, marker + "\n    bool IsPSOsReady() const { return m_PSOsReady; }")
        marker = "    CreateInfo    m_Settings;"
        assert text.count(marker) == 1
        text = text.replace(marker, marker + "\n    bool m_PSOsReady = false;")
    if relative.endswith("ScreenSpaceAmbientOcclusion.cpp"):
        marker = "bool AllPSOsReady = PrepareShadersAndPSO(RenderAttribs, m_FeatureFlags) && RenderAttribs.pPostFXContext->IsPSOsReady();"
        assert text.count(marker) == 1
        text = text.replace(marker, marker + "\n    m_PSOsReady = AllPSOsReady;")
        for include in ('#include "imgui.h"', '#include "ImGuiUtils.hpp"',
                        '#include "ScreenSpaceReflection.hpp"'):
            assert text.count(include) == 1, include
            text = text.replace(include, "")
        start = text.index("bool ScreenSpaceAmbientOcclusion::UpdateUI(")
        brace = text.index("{", start)
        depth = 1
        end = brace + 1
        while depth:
            depth += (text[end] == "{") - (text[end] == "}")
            end += 1
        text = text[:start] + text[end:]
    if relative.endswith("ScreenSpaceAmbientOcclusion.hpp"):
        text, count = re.subn(r"    static bool UpdateUI\([^;]+;\n", "", text)
        assert count == 1
        marker = "    void Execute(const RenderAttributes& RenderAttribs);"
        assert text.count(marker) == 1
        text = text.replace(marker, marker + "\n    bool IsPSOsReady() const { return m_PSOsReady; }")
        marker = "    CreateInfo    m_Settings;"
        assert text.count(marker) == 1
        text = text.replace(marker, marker + "\n    bool m_PSOsReady = false;")
    output = destination / relative
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists() or output.read_text(encoding="utf-8") != text:
        output.write_text(text, encoding="utf-8", newline="\n")

# CMake consumes this explicit list, so stale files from an older subset are
# never embedded. The list also makes the dependency closure reviewable.
(destination / "shader-manifest.txt").write_text("\n".join(shader_manifest) + "\n", encoding="utf-8")

# Preserve the pinned physical coefficient calculation verbatim apart from
# owner/constant names. It needs neither the epipolar renderer nor its buffers.
upstream = (source / "PostProcess/EpipolarLightScattering/src/EpipolarLightScattering.cpp").read_text(encoding="utf-8-sig")
start = upstream.index("void EpipolarLightScattering::ComputeScatteringCoefficients(")
body = upstream[upstream.index("{", start) + 1:upstream.index("    if (pDeviceCtx && m_pcbMediaAttribs)", start)]
body = body.replace("m_MediaParams", "media").replace("m_PostProcessingAttribs", "settings")
body = re.sub(r"\bPI_F\b", "3.14159265358979323846f", body)
body = re.sub(r"\bPI\b", "3.14159265358979323846", body)
generated = upstream[:upstream.index("#include")] + "\n#pragma once\ninline void InitializeAtmosphereCoefficients(AirScatteringAttribs& media, const EpipolarLightScatteringAttribs& settings)\n{" + body + "}\n"
output = destination / "AtmosphereCoefficients.hpp"
if not output.exists() or output.read_text(encoding="utf-8") != generated:
    output.write_text(generated, encoding="utf-8", newline="\n")
