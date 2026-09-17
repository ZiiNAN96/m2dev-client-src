# P0-L2 - Runtime-Shader-Inventar

Erfasst direkt um D3DCompile in der frischen BEFORE-Baseline. Alle Werte in ms;
Count zaehlt Aufrufe, Max den teuersten Einzelaufruf. Gleiche Zeilen innerhalb
einer Phase sind aggregiert. Quellen/Includes sind eingebettet; der Zeitpunkt
ist vorhandene Renderer-/FX-Erstinitialisierung ohne Compile-Bytecodecache.
Der Vergleichsbericht steht in [P0-L2](p0l2-shader-loading.md).

## client-setup

| Bereich | Entry / Source | Count | Summe ms | Max ms | Stage / Makros |
| --- | --- | --- | --- | --- | --- |
| Terrain | Metin2 single terrain texture PS / main | 1 | 1.050 | 1.050 | `stage=2` |
| Terrain | Metin2 terrain VS / main | 1 | 4.006 | 4.006 | `stage=1` |
| Terrain | Metin2 terrain constant PS / main | 1 | 1.017 | 1.017 | `stage=2` |
| UI / legacy / effects | B3 original PWNT skinning VS / SkinningVS | 1 | 321.465 | 321.465 | `stage=1` |
| UI / legacy / effects | Mesh auxiliary colors and card pivots / AuxiliaryVS | 2 | 29.603 | 15.335 | `stage=1` |
| UI / legacy / effects | Metin2 original splat PS / ps_main | 1 | 2.861 | 2.861 | `stage=2` |
| UI / legacy / effects | Metin2 original splat VS / vs_main | 1 | 7.896 | 7.896 | `stage=1` |
| UI / legacy / effects | Native CPU effect vertices / VS | 4 | 37.213 | 9.874 | `stage=1` |
| UI / legacy / effects | Native effect texture factor alpha fog / PS | 4 | 103.409 | 27.815 | `stage=2 EFFECT_TEXTURE=0 SECONDARY_TEXTURE=0 EFFECT_HDR=0` |
| UI / legacy / effects | Native effect texture factor alpha fog / PS | 4 | 101.292 | 26.249 | `stage=2 EFFECT_TEXTURE=0 SECONDARY_TEXTURE=0 EFFECT_HDR=1` |
| UI / legacy / effects | Native effect texture factor alpha fog / PS | 4 | 195.197 | 51.782 | `stage=2 EFFECT_TEXTURE=0 SECONDARY_TEXTURE=1 EFFECT_HDR=0` |
| UI / legacy / effects | Native effect texture factor alpha fog / PS | 4 | 189.099 | 48.477 | `stage=2 EFFECT_TEXTURE=0 SECONDARY_TEXTURE=1 EFFECT_HDR=1` |
| UI / legacy / effects | Native effect texture factor alpha fog / PS | 4 | 104.778 | 29.510 | `stage=2 EFFECT_TEXTURE=1 SECONDARY_TEXTURE=0 EFFECT_HDR=0` |
| UI / legacy / effects | Native effect texture factor alpha fog / PS | 4 | 99.031 | 25.466 | `stage=2 EFFECT_TEXTURE=1 SECONDARY_TEXTURE=0 EFFECT_HDR=1` |
| UI / legacy / effects | Native effect texture factor alpha fog / PS | 4 | 186.799 | 47.920 | `stage=2 EFFECT_TEXTURE=1 SECONDARY_TEXTURE=1 EFFECT_HDR=0` |
| UI / legacy / effects | Native effect texture factor alpha fog / PS | 4 | 200.797 | 55.878 | `stage=2 EFFECT_TEXTURE=1 SECONDARY_TEXTURE=1 EFFECT_HDR=1` |
| UI / legacy / effects | Static diffuse PS / PS | 2 | 9.939 | 5.137 | `stage=2` |
| UI / legacy / effects | Static rigid PNT VS / VS | 2 | 26.121 | 13.190 | `stage=1` |

## A1-cold

| Bereich | Entry / Source | Count | Summe ms | Max ms | Stage / Makros |
| --- | --- | --- | --- | --- | --- |
| HDR / Bloom / Tone | ComputeDownsampledTexturePS / ComputeDownsampledTexturePS / Bloom_ComputeDownsampledTexture.fx | 1 | 6.060 | 6.060 | `stage=2` |
| HDR / Bloom / Tone | ComputePrefilteredTexturePS / ComputePrefilteredTexturePS / Bloom_ComputePrefilteredTexture.fx | 1 | 14.043 | 14.043 | `stage=2` |
| HDR / Bloom / Tone | ComputeUpsampledTexturePS / ComputeUpsampledTexturePS / Bloom_ComputeUpsampledTexture.fx | 1 | 5.306 | 5.306 | `stage=2` |
| HDR / Bloom / Tone | FullScreenTriangleVS / FullScreenTriangleVS / FullScreenTriangleVS.fx | 3 | 4.113 | 1.397 | `stage=1` |
| HDR / Bloom / Tone | TonePS / TonePS | 1 | 2.298 | 2.298 | `stage=2` |
| HDR / Bloom / Tone | ToneVS / ToneVS | 1 | 1.506 | 1.506 | `stage=1` |
| PBR / IBL | FullScreenTriangleVS / FullScreenTriangleVS | 1 | 1.595 | 1.595 | `stage=1` |
| PBR / IBL | PrecomputeBRDF_PS / PrecomputeBRDF_PS | 1 | 94.541 | 94.541 | `stage=2 NUM_SAMPLES=512u` |
| PBR / IBL | ModernPS / ModernPS | 1 | 39.756 | 39.756 | `stage=2 GDX_SKIN=0 GDX_AUX=0 GDX_SHADOW=0 GDX_FORWARD=0 GDX_TANGENT=0 H2_INSTANCED=0` |
| PBR / IBL | ModernPS / ModernPS | 1 | 41.052 | 41.052 | `stage=2 GDX_SKIN=1 GDX_AUX=0 GDX_SHADOW=0 GDX_FORWARD=0 GDX_TANGENT=0 H2_INSTANCED=0` |
| PBR / IBL | ModernVS / ModernVS | 1 | 19.187 | 19.187 | `stage=1 GDX_SKIN=0 GDX_AUX=0 GDX_SHADOW=0 GDX_FORWARD=0 GDX_TANGENT=0 H2_INSTANCED=0` |
| PBR / IBL | ModernVS / ModernVS | 1 | 305.498 | 305.498 | `stage=1 GDX_SKIN=1 GDX_AUX=0 GDX_SHADOW=0 GDX_FORWARD=0 GDX_TANGENT=0 H2_INSTANCED=0` |
| SSAO / PostFX | ComputeBlueNoiseTexturePS / ComputeBlueNoiseTexturePS / ComputeBlueNoiseTexture.fx | 1 | 6.349 | 6.349 | `stage=2` |
| SSAO / PostFX | ComputeClosestMotionPS / ComputeClosestMotionPS / ComputeClosestMotion.fx | 1 | 3.457 | 3.457 | `stage=2 POSTFX_OPTION_INVERTED_DEPTH=0` |
| SSAO / PostFX | ComputeReprojectedDepthPS / ComputeReprojectedDepthPS / ComputeReprojectedDepth.fx | 1 | 4.252 | 4.252 | `stage=2` |
| SSAO / PostFX | CopyTexturePS / main | 1 | 1.044 | 1.044 | `stage=2` |
| SSAO / PostFX | CopyTextureVS / main | 1 | 1.557 | 1.557 | `stage=1` |
| SSAO / PostFX | FullScreenTriangleVS / FullScreenTriangleVS / FullScreenTriangleVS.fx | 3 | 4.014 | 1.374 | `stage=1` |
| SSAO / PostFX | ComputeAmbientOcclusionPS / ComputeAmbientOcclusionPS / SSAO_ComputeAmbientOcclusion.fx | 1 | 24.201 | 24.201 | `stage=2 SSAO_OPTION_INVERTED_DEPTH=0 SSAO_OPTION_UNIFORM_WEIGHTING=0 SSAO_OPTION_HALF_RESOLUTION=0 SSAO_OPTION_HALF_PRECISION_DEPTH=0` |
| SSAO / PostFX | ComputeBilateralUpsamplingPS / ComputeBilateralUpsamplingPS / SSAO_ComputeBilateralUpsampling.fx | 1 | 8.548 | 8.548 | `stage=2 SSAO_OPTION_INVERTED_DEPTH=0` |
| SSAO / PostFX | ComputeConvolutedDepthHistoryPS / ComputeConvolutedDepthHistoryPS / SSAO_ComputeConvolutedDepthHistory.fx | 1 | 17.185 | 17.185 | `stage=2 SUPPORTED_SHADER_SRV=1 SSAO_OPTION_INVERTED_DEPTH=0` |
| SSAO / PostFX | ComputeDownsampledDepthPS / ComputeDownsampledDepthPS / SSAO_ComputeDownsampledDepth.fx | 1 | 2.482 | 2.482 | `stage=2 SSAO_OPTION_INVERTED_DEPTH=0` |
| SSAO / PostFX | ComputePrefilteredDepthBufferPS / ComputePrefilteredDepthBufferPS / SSAO_ComputePrefilteredDepthBuffer.fx | 1 | 15.710 | 15.710 | `stage=2 SUPPORTED_SHADER_SRV=1 SSAO_OPTION_INVERTED_DEPTH=0` |
| SSAO / PostFX | ComputeResampledHistoryPS / ComputeResampledHistoryPS / SSAO_ComputeResampledHistory.fx | 1 | 20.347 | 20.347 | `stage=2 SSAO_OPTION_INVERTED_DEPTH=0` |
| SSAO / PostFX | ComputeSpatialReconstructionPS / ComputeSpatialReconstructionPS / SSAO_ComputeSpatialReconstruction.fx | 1 | 36.488 | 36.488 | `stage=2 SSAO_OPTION_INVERTED_DEPTH=0 SSAO_OPTION_HALF_RESOLUTION=0` |
| SSAO / PostFX | ComputeTemporalAccumulationPS / ComputeTemporalAccumulationPS / SSAO_ComputeTemporalAccumulation.fx | 1 | 19.932 | 19.932 | `stage=2 SSAO_OPTION_INVERTED_DEPTH=0` |
| SSAO / PostFX | FullScreenTriangleVS / FullScreenTriangleVS / FullScreenTriangleVS.fx | 8 | 10.681 | 1.419 | `stage=1` |
| Shadow | ModernPS / ModernPS | 1 | 6.495 | 6.495 | `stage=2 GDX_SKIN=0 GDX_AUX=0 GDX_SHADOW=1 GDX_FORWARD=0 GDX_TANGENT=0 H2_INSTANCED=0` |
| Shadow | ModernPS / ModernPS | 1 | 8.859 | 8.859 | `stage=2 GDX_SKIN=0 GDX_AUX=1 GDX_SHADOW=1 GDX_FORWARD=0 GDX_TANGENT=0 H2_INSTANCED=1` |
| Shadow | ModernPS / ModernPS | 1 | 7.442 | 7.442 | `stage=2 GDX_SKIN=1 GDX_AUX=0 GDX_SHADOW=1 GDX_FORWARD=0 GDX_TANGENT=0 H2_INSTANCED=0` |
| Shadow | ModernVS / ModernVS | 1 | 18.026 | 18.026 | `stage=1 GDX_SKIN=0 GDX_AUX=0 GDX_SHADOW=1 GDX_FORWARD=0 GDX_TANGENT=0 H2_INSTANCED=0` |
| Shadow | ModernVS / ModernVS | 1 | 27.244 | 27.244 | `stage=1 GDX_SKIN=0 GDX_AUX=1 GDX_SHADOW=1 GDX_FORWARD=0 GDX_TANGENT=0 H2_INSTANCED=1` |
| Shadow | ModernVS / ModernVS | 1 | 306.879 | 306.879 | `stage=1 GDX_SKIN=1 GDX_AUX=0 GDX_SHADOW=1 GDX_FORWARD=0 GDX_TANGENT=0 H2_INSTANCED=0` |
| Shadow | TerrainPS / TerrainPS | 1 | 1.050 | 1.050 | `stage=2 GDX_SHADOW=1 GDX_SOLID=0` |
| Shadow | TerrainVS / TerrainVS | 1 | 3.936 | 3.936 | `stage=1 GDX_SHADOW=1 GDX_SOLID=0` |
| Shadow / AO composition | CompositePS / CompositePS | 1 | 44.535 | 44.535 | `stage=2` |
| Shadow / AO composition | CompositeVS / CompositeVS | 1 | 2.687 | 2.687 | `stage=1` |
| Sky / atmosphere | FullScreenTriangleVS / FullScreenTriangleVS | 2 | 2.830 | 1.420 | `stage=1 PRECOMPUTED_SCTR_LUT_DIM=float4(16.0,128.0,32.0,16.0) ENABLE_LIGHT_SHAFTS=0 SINGLE_SCATTERING_MODE=2 MULTIPLE_SCATTERING_MODE=0 AUTO_EXPOSURE=0` |
| Sky / atmosphere | PrecomputeNetDensityToAtmTopPS / PrecomputeNetDensityToAtmTopPS | 1 | 19.576 | 19.576 | `stage=2 PRECOMPUTED_SCTR_LUT_DIM=float4(16.0,128.0,32.0,16.0) ENABLE_LIGHT_SHAFTS=0 SINGLE_SCATTERING_MODE=2 MULTIPLE_SCATTERING_MODE=0 AUTO_EXPOSURE=0` |
| Sky / atmosphere | PrecomputeSingleScatteringCS / PrecomputeSingleScatteringCS | 1 | 55.508 | 55.508 | `stage=32 PRECOMPUTED_SCTR_LUT_DIM=float4(16.0,128.0,32.0,16.0) ENABLE_LIGHT_SHAFTS=0 SINGLE_SCATTERING_MODE=2 MULTIPLE_SCATTERING_MODE=0 AUTO_EXPOSURE=0` |
| Sky / atmosphere | SkyPS / SkyPS | 1 | 55.397 | 55.397 | `stage=2 PRECOMPUTED_SCTR_LUT_DIM=float4(16.0,128.0,32.0,16.0) ENABLE_LIGHT_SHAFTS=0 SINGLE_SCATTERING_MODE=2 MULTIPLE_SCATTERING_MODE=0 AUTO_EXPOSURE=0` |
| Sky / atmosphere | main / main | 1 | 1.419 | 1.419 | `stage=1 PRECOMPUTED_SCTR_LUT_DIM=float4(16.0,128.0,32.0,16.0) ENABLE_LIGHT_SHAFTS=0 SINGLE_SCATTERING_MODE=2 MULTIPLE_SCATTERING_MODE=0 AUTO_EXPOSURE=0` |
| Sky / atmosphere | main / main | 2 | 20.116 | 10.960 | `stage=2 PRECOMPUTED_SCTR_LUT_DIM=float4(16.0,128.0,32.0,16.0) ENABLE_LIGHT_SHAFTS=0 SINGLE_SCATTERING_MODE=2 MULTIPLE_SCATTERING_MODE=0 AUTO_EXPOSURE=0` |
| Terrain | TerrainPS / TerrainPS | 1 | 3.504 | 3.504 | `stage=2 GDX_SHADOW=0 GDX_SOLID=0` |
| Terrain | TerrainVS / TerrainVS | 1 | 5.327 | 5.327 | `stage=1 GDX_SHADOW=0 GDX_SOLID=0` |
| Vegetation | ModernPS / ModernPS | 1 | 43.260 | 43.260 | `stage=2 GDX_SKIN=0 GDX_AUX=1 GDX_SHADOW=0 GDX_FORWARD=0 GDX_TANGENT=0 H2_INSTANCED=1` |
| Vegetation | ModernVS / ModernVS | 1 | 26.322 | 26.322 | `stage=1 GDX_SKIN=0 GDX_AUX=1 GDX_SHADOW=0 GDX_FORWARD=0 GDX_TANGENT=0 H2_INSTANCED=1` |
| Water / SSR | ComputeBilateralCleanupPS / ComputeBilateralCleanupPS / SSR_ComputeBilateralCleanup.fx | 1 | 13.893 | 13.893 | `stage=2 SSR_OPTION_INVERTED_DEPTH=0` |
| Water / SSR | ComputeDownsampledStencilMaskPS / ComputeDownsampledStencilMaskPS / SSR_ComputeDownsampledStencilMask.fx | 1 | 9.428 | 9.428 | `stage=2 SSR_OPTION_INVERTED_DEPTH=0` |
| Water / SSR | ComputeHierarchicalDepthBufferPS / ComputeHierarchicalDepthBufferPS / SSR_ComputeHierarchicalDepthBuffer.fx | 1 | 4.058 | 4.058 | `stage=2 SUPPORTED_SHADER_SRV=1 SSR_OPTION_INVERTED_DEPTH=0` |
| Water / SSR | ComputeIntersectionPS / ComputeIntersectionPS / SSR_ComputeIntersection.fx | 1 | 32.903 | 32.903 | `stage=2 SSR_OPTION_PREVIOUS_FRAME=0 SSR_OPTION_INVERTED_DEPTH=0 SSR_OPTION_HALF_RESOLUTION=0` |
| Water / SSR | ComputeSpatialReconstructionPS / ComputeSpatialReconstructionPS / SSR_ComputeSpatialReconstruction.fx | 1 | 68.546 | 68.546 | `stage=2 SSR_OPTION_INVERTED_DEPTH=0 SSR_OPTION_HALF_RESOLUTION=0` |
| Water / SSR | ComputeStencilMaskAndExtractRoughnessPS / ComputeStencilMaskAndExtractRoughnessPS / SSR_ComputeStencilMaskAndExtractRoughness.fx | 1 | 2.540 | 2.540 | `stage=2 SSR_OPTION_INVERTED_DEPTH=0` |
| Water / SSR | ComputeTemporalAccumulationPS / ComputeTemporalAccumulationPS / SSR_ComputeTemporalAccumulation.fx | 1 | 38.539 | 38.539 | `stage=2 SSR_OPTION_INVERTED_DEPTH=0` |
| Water / SSR | FullScreenTriangleVS / FullScreenTriangleVS / FullScreenTriangleVS.fx | 7 | 9.949 | 1.640 | `stage=1` |
| Water / SSR | WaterCompositePS / WaterCompositePS | 1 | 68.219 | 68.219 | `stage=2` |
| Water / SSR | WaterNormalPS / WaterNormalPS | 1 | 7.025 | 7.025 | `stage=2` |
| Water / SSR | WaterRoughnessPS / WaterRoughnessPS | 1 | 5.681 | 5.681 | `stage=2` |
| Water / SSR | WaterScreenVS / WaterScreenVS | 2 | 9.752 | 5.101 | `stage=1` |
| Water / SSR | WaterVS / WaterVS | 1 | 9.045 | 9.045 | `stage=1` |

