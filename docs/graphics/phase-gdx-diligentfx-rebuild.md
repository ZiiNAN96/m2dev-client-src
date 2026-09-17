# G-DX — Diligent/DiligentFX-first renderer rebuild

Status after [G-DX-C Closure Pass](phase-gdxc-closure.md): **GO for the four
bounded acceptance items**, 2026-09-15. Off-camera casters, Debug effect bindings,
world-entry prewarm and authenticated login/relog/shutdown are verified.
Explicit Classic/Modern reconstruction costs remain documented. The original
NO-GO evidence below is retained as the historical state before closure.

## 1. Git Start State

Verified before source edits: `feature/gdx-diligentfx`,
`ca63fd1` (`feat(graphics): add centralized graphics settings foundation`),
clean working tree. No reset, checkout, commit, staging or push.
Source repository: `m2dev-client-src`.
The separate `m2dev-client` runtime repository is on `main` and already dirty;
its existing assets, configuration and UI changes are outside this source change.

## 2. G0 Baseline

The G0 report records Release 57/57, Debug 57/57 and GCC/LP64 32/32.
Those are historical results, not G-DX validation. Fresh results follow below.
The initial restricted build could not discover the Windows SDK/ATL headers.
Both final configurations were built successfully with the installed native
Visual Studio environment; this environment limitation was not a product failure.

## 3. Main Reference Boundary

Source `main` started at `ea1142c5224311c9ed7cfe604bd3471d775dea54`.
No G1/G2/G3/4 renderer source has been imported. Old binaries, captures and
measurements are external references only. Cached `build/gdx/start-head.txt`
contains that old main hash and is **not** the start-state evidence for this run.

## 4. Diligent Engine Version

`buildtool/Diligent.cmake` pins Core
`b036337d68be2353c9950a85929acf796b9a6d50`; the local dependency HEAD matches.
The official [Engine v2.5.6 tree](https://github.com/DiligentGraphics/DiligentEngine/tree/v2.5.6)
references that exact Core commit, FX below, and Tools
`809313db98843b6fbff2f67299282672979a8532`.
This verifies a compatible release set; it is not a claim that v2.5.6 is latest.

## 5. DiligentFX Version

Audit source: [DiligentFX cb380ac52100672b5762f595acfb6609e0ecc248](https://github.com/DiligentGraphics/DiligentFX/tree/cb380ac52100672b5762f595acfb6609e0ecc248).
Local archive: `build/gdx/diligentfx.tar.gz`, SHA256
`7117a1d0067ef0c36315900647904116234cf55cadc11682f6f6e70064658236`.
It was already cached before this run. Shader/API observations below refer to
the contents at this pin, not unversioned web documentation.
The fresh official archive has the same SHA256; all 305 extracted original
files match byte-for-byte (`build/gdx/dependency-verification.json`).

## 6. License

Core and FX contain Apache-2.0 `License.txt`; retain upstream notices when
redistributing/adapting sources. FX's shader sources also contain attribution
to algorithms they implement. No new asset loader or UI library is required
by the selected rendering algorithms themselves.

## 7. Dependency Integration

Upstream `DiligentFX` is one static target: it adds all Components, PostProcess,
PBR, GLTF/USD frontends and publicly links AssetLoader, TextureLoader and Imgui.
Therefore linking that entire target is not the minimal client integration.
The linked subset is the shader source factory, ShadowMapManager,
PostFXRenderTechnique, PostFXContext and ScreenSpaceAmbientOcclusion, with
generic PBR shaders. UI helpers are separable from SSAO execution. Core supplies
device/context, resource registry, shader cache and shader utility interfaces.
No Samples, Hydrogent, USD frontend or GLTF_PBR_Renderer is needed.
`buildtool/DiligentFX.cmake` pins URL and SHA256 and links five implementation
units statically. `PrepareDiligentFX.py` retains the license and source notices,
rewrites sibling Core includes, removes SSAO Imgui/UpdateUI and an unused SSR
include, and exposes upstream SSAO PSO readiness through a small getter/flag.
It does not alter rendering algorithms. The 44-entry explicit shader manifest
is checked for include closure and excludes unused stock frontends. Shader
embedding uses upstream BuildUtils. No runtime shader download or FX DLL.

## 8. DiligentFX Feature Audit

Paths in this section are relative to the verified FX source root.
`D3D11/Vulkan: source-supported` means the generic Core API and portable shader
paths exist; it does not mean this client's Vulkan integration was executed.
Android requires the Vulkan shader toolchain and device validation later.
All effects below are format-neutral unless explicitly stated otherwise.

| Component | Exact API/files | Modules/dependencies | Inputs → outputs and targets | Lifetime/platform/decision |
|---|---|---|---|---|
| A PBR | `PBR/interface/PBR_Renderer.hpp`, `PBR/src/PBR_Renderer.cpp`; `Shaders/PBR/public/PBR_Shading.fxh` | PBR + Common shaders; full class also TextureUtilities | Material/mesh, camera and lights → shaded color; custom PS output hook exists | Cached PSOs/signatures, owned IBL textures; D3D11/Vulkan source-supported. **ADAPT** generic shaders to existing neutral mesh/palette bindings. |
| B Metallic/Roughness | `Shaders/Common/public/PBR_Common.fxh`, `Shaders/PBR/public/PBR_Structures.fxh` | Shader-only | Base color, metallic, perceptual roughness → reflectance and GGX BRDF | No loader or persistent resources. **USE** upstream formulas. |
| C Normal Mapping | `PBR_Shading.fxh`: `GetPerturbNormalInfo`, `PerturbNormal`; `ShaderUtilities.fxh` | Shader-only | World position derivatives, normal, face, UV derivatives, sampled tangent-space normal → world normal | Supports derivative tangent frame; no per-frame CPU tangent generation. **USE/ADAPT** to verified legacy frontface convention. |
| D IBL | `PBR_Renderer::PrecomputeCubemaps`, `PrecomputeBRDF`; `PBR_Shading.fxh` IBL helpers; `Shaders/PBR/private/PrecomputeBRDF.psh` | PBR/Common shaders; full renderer loads optional sheen tables | Environment cube/lat-long → diffuse irradiance, roughness mip cube, BRDF LUT → indirect lighting | Renderer owns caches/LUTs; rebake on environment change, not per actor. D3D11/Vulkan source-supported. **ADAPT** neutral environment inputs, no probe engine. |
| E Lighting | `PBR_Shading.fxh`: `ApplyDirectionalLightGGX`; `PBR_Structures.fxh` light data | PBR/Common shaders | Sun direction/color/intensity, surface, view → direct light | Stateless GPU math; **USE**, ZiiNAN owns map semantics. |
| F ShadowMapManager | `Components/interface/ShadowMapManager.hpp`, `Components/src/ShadowMapManager.cpp`: `Initialize`, `GetCascadeDSV`, `GetSRV` | Components, Core GraphicsTools, FX shader factory | Format/resolution/cascade count/mode → depth-array DSVs/SRV | Owns maps, views, conversion resources and PSOs. D3D11/Vulkan source-supported. **USE**. |
| G Cascades | `ShadowMapManager::DistributeCascades`, `GetCascadeTranform` | Same as F | View/projection, single sun, distance callback → cascade splits/matrices | Texel snapping, extent stabilization, handedness/row-major controls and range callbacks exist. **USE**, no new CSM implementation. |
| H Filtering/Bias | `Shaders/Common/public/Shadows.fxh`, `PCF.fxh`; `BasicStructures.fxh::ShadowMapAttribs`; `ConvertToFilterable` | Shadow shader helpers; VSM/EVSM add conversion targets | Shadow depth or moments, receiver position/derivatives → visibility | Fixed/receiver-plane bias, PCF, VSM/EVSM and cascade transitions exist. **USE** PCF initially; defer optional moment targets. |
| I SSAO | `PostProcess/ScreenSpaceAmbientOcclusion/interface/ScreenSpaceAmbientOcclusion.hpp`: `PrepareResources`, `Execute`, `GetAmbientOcclusionSRV` | SSAO, PostFXContext, PostFXRenderTechnique, Core; `UpdateUI` alone needs Imgui | Device depth + unpacked normals + context → R8 AO; R32 depth mip chains (R16 optional), R16 history length | Half-resolution and spatial/bilateral reconstruction exist. Reversed depth and packed-normal flags say **not implemented**. Temporal accumulation is part of Execute. **USE/ADAPT**, never silently supply invalid history. |
| J TAA | `PostProcess/TemporalAntiAliasing/interface/TemporalAntiAliasing.hpp`: `GetJitterOffset`, `Execute`, `GetAccumulatedFrameSRV` | TAA + PostFXContext | Scene color, context depth/motion/current+previous camera → ping-pong accumulated color | Buffer sets indexed per view; `ResetAccumulation` contract. D3D11/Vulkan source-supported. **USE later** after true motion exists. |
| K SSR | `PostProcess/ScreenSpaceReflection/interface/ScreenSpaceReflection.hpp`: `Execute`, `GetSSRRadianceSRV` | SSR + PostFXContext | Scene color, depth, unpacked normals, material roughness, motion/history → reflected radiance | Hierarchical depth, stencil/roughness, intersection and temporal/spatial reconstruction; half-res and previous-frame flags. **USE later**, no G7 water integration. |
| L Bloom | `PostProcess/Bloom/interface/Bloom.hpp`: `PrepareResources`, `Execute`, `GetBloomTextureSRV` | Bloom + PostFXContext | Linear scene color → prefilter/downsample/upsample color pyramid | Owns size-dependent pyramid/PSOs; no normals/motion input in RenderAttributes. HDR input is needed for useful bright-range separation. **USE later**, G5/6. |
| M Tone Mapping | `Shaders/Common/public/ToneMapping.fxh`, `ToneMappingStructures.fxh`; `Components/interface/ToneMapping.hpp` | Shader-only operators; CPU inverse helpers | Linear HDR, exposure/luminance parameters → display-range color | No obligatory depth, normal or history allocation. **USE later** with G5/6 color pipeline; no new ACES formula. |
| N Atmosphere | `PostProcess/EpipolarLightScattering/interface/EpipolarLightScattering.hpp`: `PrepareForNewFrame`, `PerformPostProcessing` | EpipolarLightScattering + Common shaders | Source HDR color/depth, camera, light and shadow map → scattering/composite RTV; LUTs and epipolar intermediates | RGBA16F scattering, RG32F density/depth, 3D scattering textures; light uses same sun. D3D11/Vulkan source paths, significant LUT/memory work. **ADAPT later** map scale/fog, G5/6. |
| O PostFXContext | `PostProcess/Common/interface/PostFXContext.hpp`: `FrameDesc`, `PrepareResources`, `Execute` | Common, Core ResourceRegistry, embedded blue-noise data | Current/previous depth, motion, two CameraAttribs → camera CB, blue noise, reprojected/previous depth, closest motion | One context per view; owns size-dependent shared targets and PSOs. D3D11 subresource capability paths exist. **USE**; no duplicated per-effect scene depth. |
| P Depth reconstruction | `Shaders/Common/public/ShaderUtilities.fxh`; `Shaders/Common/private/ComputeReprojectedDepth.fx` | Common shaders + CameraAttribs | Device depth + inverse projection/view-projection → reconstructed position/reprojected depth | Projection, depth range and row-major must agree. D3D11/Vulkan source-supported. **USE**. |
| Q Motion/Reprojection | `PostFXContext` closest motion; `ShaderUtilities.fxh` reprojection helpers | Common; producer is client draw/animation data | Current/previous clip positions → screen motion; depth → rejection | FX cannot invent previous object/bone/wind state. **ADAPT** producer; do not enable TAA using camera-only actor motion. |
| R History buffers | TAA `AccumulationBufferInfo`, SSAO and SSR resource registries | Respective effects + context | Previous results/depth/motion + current frame → temporal estimate | Resize/reallocation and frame discontinuity reset supported; client must also reset on map/teleport/style changes. **USE** effect-owned histories. |
| S Common GPU constants | `Shaders/Common/public/BasicStructures.fxh`, `ShaderDefinitions.fxh`, PBR and effect Structures | Shared C++/HLSL definitions | Camera/light/material numeric data → correctly aligned constant buffers | Internal adapter types only. **USE**; no FX type leaks into public asset/game API. |
| T Shared PostFX resources | `PostFXContext`, `Components/interface/GBuffer.hpp`, `PostFXRenderTechnique.hpp` | Common/Components/Core | Frame dimensions and attachments → shared scene inputs and effect resources | `GBuffer` offers generic ownership, context shares derived inputs. **USE/ADAPT** to existing frame boundary; no per-actor light/context. |

### Audit constraints that determine integration

* SSAO's `UpdateUI` can be excluded without changing the algorithm. Including
  Imgui and the full glTF asset loader merely to satisfy the aggregate target
  would introduce unnecessary dependencies.
* The generic PBR class is separate from `GLTF_PBR_Renderer`, but its default
  vertex shader/constant layout (`GLTFNodeShaderTransforms`) differs from
  ZiiNAN's byte-weighted, remapped GPU palette and procedural vegetation cards.
  Preserve these game-owned inputs and call upstream shading functions.
* SSAO has `ResetAccumulation` and `TemporalStabilityFactor`. A spatial-only
  integration must explicitly disable history use; zero motion cannot be
  represented as accurate moving-actor motion. TAA/SSR remain gated on real
  previous world, bone and wind inputs.
* ShadowMapManager supplies infrastructure, not the game's caster list.
  Caster submission, material cutoff and mesh ownership remain ZiiNAN.

## 9. Reuse Matrix

The implemented subset follows these decisions. Future effects remain audit-only.

| Feature | Diligent/DiligentFX | Decision | Reason |
|---|---|---|---|
| PBR | PBR_Shading / PBR_Structures | ADAPT | Shared GR2/GLB inputs and existing GPU palette/card vertex frontend |
| BRDF | PBR_Common | USE | GGX/Smith/Schlick already provided |
| Metallic/Roughness | PBR surface helpers | USE | Standard workflow and channels |
| Normal Mapping | PerturbNormal / ShaderUtilities | USE/ADAPT | Reuse math; preserve correct face/UV convention |
| IBL | PBR precomputation and sampling | ADAPT | Neutral environment boundary; no probe engine |
| Lighting | ApplyDirectionalLightGGX | USE | Map sun is adapted once |
| Shadows | ShadowMapManager / Shadows.fxh | USE | Neutral depth-array ownership and sampling |
| Cascades | DistributeCascades | USE | Splits, snapping and stabilization already present |
| SSAO | ScreenSpaceAmbientOcclusion | USE/ADAPT | Reuse effect; detach UI dependency and define valid history policy |
| TAA | TemporalAntiAliasing | USE later | Needs complete motion producers |
| SSR | ScreenSpaceReflection | USE later | Needs roughness/color/motion; water is G7 |
| Bloom | Bloom | USE later | G5/6 HDR scene prerequisite |
| Tone Mapping | ToneMapping.fxh | USE later | G5/6 production boundary |
| Atmosphere | EpipolarLightScattering | ADAPT later | Sun/map-unit/fog semantics; G5/6 |
| PostFXContext | PostFXContext | USE | One shared camera/depth/motion infrastructure |
| Depth Reconstruction | ShaderUtilities | USE | Portable depth conventions |
| Motion Vectors | Shader helpers + game state | ADAPT | Previous animation/card state belongs to ZiiNAN |

## 10. Final Architecture

GR2 / GLB / legacy content → neutral AssetRuntime → cached MaterialRuntimeData
and existing mesh/palette/card submissions → ModernFrame adapter → FX PBR,
ShadowMapManager, PostFXContext and SSAO → Core D3D11. Classic keeps its G0 path.
The world pass writes separate direct, indirect, emission and world-normal targets.
FX shadows affect direct light; FX screen AO affects indirect light; composition
then applies the existing map fog and display transfer. Blended meshes and late
ground-item/PCBlocker draws use the same PBR inputs in the forward phase.

## 11. ZiiNAN/Diligent Boundary

ZiiNAN owns identity, loading, materials, skeletons, animation, skinning palette,
vegetation wind/cards, map light semantics, draw order and settings. Diligent owns
GPU resources, pipelines and synchronization. FX supplies standard shading and
effects. Public AssetRuntime/Graphics/Renderer data contains no loader, Win32,
D3D11 or FX implementation types; FX constants are private to the adapter.

## 12. GR2/GLB Shared Render Path

Both providers produce MaterialAsset and the existing MeshAsset. Both enter
ActorRenderBridge or StaticObjectBridge and DiligentStaticObjectRenderer. Modern
draw submission has no GR2-versus-GLB shader dispatch. GLTF_PBR_Renderer,
USD/Hydrogent and a second asset loader are not linked.

## 13. MaterialAsset

`AssetRuntime.h` adds Legacy/PBRMetallicRoughness, explicit alpha mode, factors,
normal scale, occlusion strength, emission, sidedness and six neutral texture
slots. `MaterialRuntimeData.h` contains numeric values, selected scalar channels
and shared uploaded textures. The existing diffuse identity remains compatible.

## 14. glTF Material Mapping

The provider reads baseColorFactor/Texture, metallicFactor, roughnessFactor,
normalTexture/scale, occlusionTexture/strength, emissiveFactor/Texture,
alphaMode/cutoff and doubleSided. Packed MR uses G for roughness and B for metal;
occlusion uses R. Missing PBR blocks still default to metallic=1, roughness=1,
baseColor=white, normalScale=1, occlusionStrength=1, emissive=0, alpha=Opaque.
Nonfinite/out-of-range values fail explicitly. Optional maps currently require
TEXCOORD_0 and untransformed UVs; unsupported independent UV/transform requests
are rejected. Existing base-only texture-transform baking remains supported.

## 15. Legacy GR2 Materials

Existing diffuse texture, metallic=0, roughness=.85, normalScale=1,
occlusionStrength=1 with neutral absent AO, geometric normal, emissive=0.
No authored normal/metal map is invented. Existing alpha/cull/animation state
is retained. Legacy sphere-map gloss is replaced by the FX material response.
Actor Add tint is an unshadowed linear emission contribution; Modulate tint
multiplies all light contributions. These game cues do not create another sun.

## 16. GR2 Material Overrides

`MaterialOverrides.h` supplies a version-1 neutral import-time API. Overrides
address exact asset identity plus a unique exact material name. All records are
validated before an atomic update; duplicate, ambiguous, unknown and invalid
records are rejected. Optional maps/scalars can enrich existing geometry without
editing GR2. No asset-specific shader rule or per-frame parsing. This is the
requested foundation: no sidecar authoring UI or automatic sidecar loader yet.

## 17. Diligent PBR

Modern shaders call upstream `GetSurfaceReflectanceMR`,
`ApplyDirectionalLightGGX`, `GetIBLSamplingInfo`, `GetLambertianIBL` and
`GetSpecularIBL_GGX`. The game vertex frontend is retained for packed palette
indices/weights, rigid attachments, STP splats and procedural vegetation.
There is no local replacement BRDF implementation.

## 18. BRDF

GGX distribution, Smith masking and Schlick Fresnel come from FX PBR helpers.
Perceptual roughness is clamped to .045 at evaluation to avoid a singular smooth
surface; imported material values remain unchanged. Terrain uses a conservative
dielectric roughness=.9 through the same direct-light helper.

## 19. IBL Foundation

FX `PrecomputeBRDF.psh` generates a 256×256 RG16F LUT with 512 samples once per
Modern owner. A unit constant cube supplies exact constant-environment diffuse
sampling; neutral ambient scales it. `environmentColor` feeds FX specular IBL.
This is a constant-environment foundation, not a directional cubemap/probe
system. G5/6 can later supply filtered sky environments without changing assets.

## 20. Color Space

Existing shared texture uploads are UNORM. FX FastSRGBToLinear decodes base and
emissive texture RGB once; factors are already linear glTF numeric values.
Normal/MR/AO are sampled as data. Lighting and MRT values are linear. FX
FastLinearToSRGB runs once at final composition/forward output. Existing
display-space fog remains after this conversion. The three color MRTs are
RGBA8_UNORM: values above one clip. HDR/tonemapping is deliberately G5/6.

## 21. Normals

Static and skinned vertices use FX `InverseTranspose3x3` on the actual combined
skin/world transform. Singular transforms fall back to geometric derivatives.
Face orientation and determinant sign are handled separately. World normals
are written unpacked into RGBA16F for FX SSAO.

## 22. Tangents

Authored float4 tangents are copied once during mesh capture and uploaded as
an immutable shared stream. Rigid and deforming actor ranges preserve offsets.
Mirroring adjusts handedness; tangent directions use the same skin/world basis.
Missing tangents use FX derivative reconstruction when a normal map is active.
There is no CPU per-frame tangent generation; offline tangents remain preferable.

## 23. SceneLighting

`Graphics/SceneLighting.h` defines one sun direction/color/intensity, ambient,
optional environment color and existing fog inputs. Direction is normalized;
invalid/zero direction disables direct sun. Invalid intensities are sanitized.
It is world data, not an alternative lighting engine.

## 24. Sun Source

Map BACKGROUND light → ResolveLegacyEnvironmentLight → SceneLighting → one shared
light buffer → FX direct lighting and ShadowMapManager cascade distribution.
There is no actor/terrain/shadow-specific Modern sun. One buffer creation and one
upload per rendered world scene are asserted, including a 20-draw fixture.

## 25. Map Environment

Sun color is Light.Diffuse × Material.Diffuse. Legacy Lambert strength is
converted to irradiance with intensity π. Indirect fill is Material.Emissive +
Light.Ambient × Material.Ambient. The map Material.Emissive field historically
means world fill, not glowing assets. A1/B1 background ambient is zero, so losing
this field visibly darkened an early trial; the corrected neutral adapter
preserves it. No map-name conditions or new map files.

## 26. Diligent Lighting

Both mesh and terrain shaders evaluate FX directional PBR with the shared sun.
Scene lighting is uploaded when the camera is established, once per world pass.
Character-select spot/point lighting remains in the existing non-world path.
The adapter does not evaluate a second custom directional-light formula.

## 27. Terrain

Existing terrain vertices, patch indices, texture transforms, camera-space splat
UVs, blend order and alpha are retained. Vertex-lit RGB is excluded from Modern
lighting to prevent double sun; vertex alpha still blends splats. Terrain uses
the shared depth/normal/light/shadow/AO infrastructure. Immutable patch geometry
is retained for cascade replay; mutable STP color attributes are not retained.

## 28. Static World

Buildings, props, rocks, walls, bridges and static GLB use the same material
adapter. Optional map loading is lazy and cached per material/uploader lifetime.
Invalid optional texture resources are diagnosed and use neutral missing-map
behavior; a wrong resource type is checked before image casting.

## 29. Actors

Player, NPC, mob, boss, mount, weapon, hair and rigid attachments use the common
mesh frontend. Palette inputs and existing game alpha/tint states are adapted.
Transparent actors are deferred until after opaque composition and use the same
FX lighting, shadow map, screen AO and fog. Late world items and blockers use
an explicit forward-world boundary before the existing effects/UI flow.

## 30. GPU Skinning

GPU skinning remains default. The cascade pass replays the same vertex/palette
data, with no CPU shadow mesh or deformation. Shared meshes/tangents are reused
across actors; each instance retains its existing pose palette. Native resource
audits and existing animation/production GPU tests check zero CPU deformation
and zero fallback; evidence is separated from manual appearance below.

## 31. Vegetation

The ZiiNAN runtime, species/LOD choice, cards, pitch tables and wind parameters
remain. Modern shaders adapt those inputs in both color and shadow passes.
Lighting and AO use the same world data. No new grass/trees/assets/wind engine,
leaf transmission or H2 work.

## 32. Diligent Shadows

Directional depth-array shadows use FX ShadowMapManager and upstream
Shadows.fxh/PCF.fxh. Opaque and alpha-masked mesh submissions, GPU-skinned
actors, attachments, terrain and vegetation are replayed into each cascade.
Blended surfaces do not cast opaque silhouettes. The caster set currently comes
from geometry submitted by the game camera, not an independent off-frustum
caster query. This coverage limitation must remain visible in acceptance.

## 33. ShadowMapManager

FX owns the depth array, DSVs/SRV and light transforms. The adapter calls
Initialize, DistributeCascades, GetCascadeDSV and GetSRV. It supplies the game's
single sun, camera, quality and caster draws; no second CSM allocator exists.

## 34. Cascades

Off disables evaluation. Low=512²×1; LegacySolo/Medium=1024²×2;
High=1536²×3; Ultra=2048²×4. Shadow distance is min(viewDistance, 2500×cascades).
FX performs split distribution, stable extents, texel snapping and transitions.
The RH game camera is converted to positive-forward FX camera space by flipping
view Z and compensating projection Z; the world view-projection stays identical.

## 35. Bias

Raster depth bias=100, slope scale=1, clamp=.002; FX fixed receiver bias=.0005.
The first world trial exposed terrain acne. An AO-off comparison isolated it to
shadows; corrected bias removed those broad stripes in subsequent inspected
captures. These values are not proof against every extreme slope/distance case.

## 36. Filtering

Upstream 3×3 PCF and cross-cascade filtering are enabled. VSM/EVSM were audited
but are not allocated. Quality changes cascade count, resolution and range;
they do not claim a different unimplemented filter algorithm.

## 37. Alpha Shadows

Color and shadow use the same SampleBase function and round alpha to the native
8-bit comparison before Greater/GreaterEqual cutoff. Diffuse/factor alpha,
camera-space mask and modulate-camera-alpha are preserved. Material and camera
samplers reuse existing wrap/filter/mip/anisotropy settings through a small
shared sampler cache. Blended meshes are excluded from the caster list.

## 38. Double-sided / Frontface

Cull-none retains the game's CW front-face definition; mirrored transforms
invert it. Double-sided shader normals orient by SV_IsFrontFace. The narrow
front-face convention was checked against the old main implementation as an
allowed game integration reference; its old shading stack was not copied.

## 39. Diligent SSAO

Actual upstream ScreenSpaceAmbientOcclusion is used: half-resolution for the
existing SSAO setting and full-resolution for the existing GTAO setting. These
are two quality levels of the same FX algorithm, not two different algorithms.
Radius=120 game units. Upstream depth hierarchy, AO estimation, spatial/bilateral
reconstruction and one-second initial fade are retained.

## 40. AO Semantics

Final opaque result is direct×shadow + indirect×screenAO + emission.
Material occlusion multiplies only indirect before screen AO. Forward materials
follow the same separation. Pixel tests compare AO off/on with direct-only and
emission-only inputs and require byte equality; indirect contacts must darken.

## 41. Shared Depth/Normal Infrastructure

One R24G8 typeless world depth with D24S8 DSV and SRV is shared by meshes,
terrain, composition, AO and later forward draws. Normal input is RGBA16F world
normal. The world background is copied before MRT rendering. Zeroed RG16F motion
is explicitly spatial scratch, not valid animated motion evidence.

## 42. PostFXContext

One context per Modern view owns FX shared derived depth/noise resources.
PrepareResources/Execute receive current camera/depth, duplicated previous
camera/depth and zero motion. SSAO always sets ResetAccumulation=true and
TemporalStabilityFactor=0, so temporal history is not trusted. Both context and
SSAO PSO readiness are required; placeholder AO is not accepted as success.

## 43. TAA Audit

FX TemporalAntiAliasing provides jitter, temporal rejection, accumulation and
history ownership. Production TAA is disabled until complete object/animation
motion exists. Its Structures header is embedded because SSAO includes it;
this does not enable or compile the TAA effect class.

## 44. Motion Vector Requirements

Future true motion needs previous world transform, previous bones and remaps,
previous vegetation/card wind state, current/previous jitter and camera,
teleport/map invalidation and depth-disocclusion handling. Current camera-only
or zero actor motion would be incorrect for TAA/SSR. No claim of complete
motion-vector production is made.

## 45. SSR Audit

FX ScreenSpaceReflection supplies hierarchical tracing, roughness-dependent
sampling, temporal/spatial reconstruction and effect-owned histories. Required
linear scene color, normal/roughness, depth and reliable previous motion are
documented in section 8. No SSR effect or Modern water is enabled in G-DX.

## 46. Bloom Audit

FX Bloom provides prefilter/downsample/upsample resources. Production needs the
G5/6 HDR scene and exposure contract. No bloom target or pass is created now.

## 47. Tone Mapping Audit

FX ToneMapping.fxh provides the operators and structures. G-DX uses only the
display transfer for its LDR pipeline; it does not duplicate ACES or claim HDR.
Production tone mapping remains part of G5/6.

## 48. Atmosphere Audit

FX EpipolarLightScattering can reuse the shared sun and depth/shadows. Map scale,
fog/sky integration and LUT/memory policy need the later milestone. Existing
sky/clouds remain; no new visible sun or atmosphere pass is installed.

## 49. GraphicsSettings

The G0 store and revision snapshot remain the source of truth. Resolve enables
requested shadows/AO only for Modern; Classic keeps them off. Existing persisted
enumeration values stay compatible. HDR/bloom/new-sky/water gates remain unchanged.

## 50. Presets

Existing presets and custom serialization are preserved. Modern shadow quality
selects the real configurations in section 34. AO Off allocates no active AO
evaluation; SSAO/GTAO select half/full resolution. Portable preset/custom/invalid/
persistence tests verify resolved values and future-version write refusal.

## 51. Live Apply

Revision changes are consumed at frame boundaries. Shadow size/count changes
reinitialize FX maps; AO quality is passed to PrepareResources. Switching to
Classic destroys the Modern owner. Resize resets window-sized effects. Native
test evidence and any remaining interactive acceptance are listed below.

## 52. Adapter Layer

`ModernFrame.h` exposes only scene Begin/End, late-forward boundaries and counters.
`DiligentModernRenderer` owns implementation types and pipelines. The neutral
material cache sits with existing material/resource lifetime. Asset providers
never create GPU resources or depend on FX classes.

## 53. Production Rendering Paths

Classic: unchanged G0 shader path. Modern world: common FX PBR + cascades + SSAO.
Non-world login/selection and existing water/effects/UI retain their established
paths. This is a deliberate scope boundary, not a second GR2/GLB Modern renderer.

## 54. Removed Custom Standard Rendering Code

No G1/G2/G3/4 custom stack existed at this clean G0 starting point, so none was
reimported or deleted from main. No old custom BRDF/CSM/AO file is compiled into
the new Modern path. Existing Classic shader code stays for byte regression.

## 55. Remaining Custom Standard Rendering Code

Local code is limited to scene/material/input adaptation, pipeline binding,
MRT separation/composition, game alpha/tint/fog rules, vertex/palette/card
transforms, sampler caching, frame ownership and diagnostics. Standard BRDF,
IBL evaluation/preintegration, normal helpers, shadow distribution/filtering,
AO estimation/reconstruction and shared post-effect processing are upstream.

## 56. Reason for Every Remaining Custom Rendering Component

| Component | Technical reason |
|---|---|
| Mesh/card/palette vertex frontend | FX stock frontend expects different node/vertex/palette contracts; changing it would replace the neutral game asset runtime. |
| Authored tangent basis assembly | FX exposed perturb-normal helper constructs a derivative basis; the game also carries authored tangents that must survive skin/mirror transforms. |
| Terrain splat adapter | Camera-space UVs, STP alpha and patch order are Metin2 semantics absent from FX. |
| Material/lighting bindings and PSO cache | Existing neutral submissions and state variants differ from the FX glTF node frontend. Generic shader math is reused. |
| Direct/indirect/emission composition | Screen AO must affect only indirect while preserving the existing LDR/fog/order contract. FX does not supply this exact game compositor. |
| Caster submission and lifetime | ShadowMapManager does not discover game objects or own their palettes/cards. |
| Map irradiance/fill conversion, alpha and hit tint | These describe authored Metin2 behavior, not general lighting algorithms. |
| Resize/style/history policy | Effect classes cannot know game map/teleport/style boundaries. |
| Minimal FX build preparation | Aggregate upstream target brings unrelated Imgui/loader/USD modules; a deterministic subset avoids a second frontend. |

No technical reason was found to replace the standard FX BRDF, cascade
distribution or SSAO algorithm; consequently none is reimplemented locally.

## 57. GR2 Proof

The final native fixture uses real client assets and production-default ZiiNAN
GR2/animation/vegetation and GPU skinning. It exercises A1 → B1 → A1, near/far
cameras, walk/run/attack, player, NPC 9003, mob 101, boss 691, mount 20101,
horse 20104/rider, weapon 19 and hair 1001. Final run results are recorded in
the evidence summary below; earlier preliminary runs are not SSAO proof.
`gdx-modern-verified-05`: **3,499 frames, six captures, 62.9 seconds, exit 0**.
NativeGR2Reads=933; all checked source owners=0, ModernRenderers=0,
AllCPUDeformationCalls=0, GPUFallbacks=0. The final runtime syserr is empty.

## 58. GLB Proof

AssetRuntime.GlTFModern drives the real market-stall GLB through the same
production material cache and static-object renderer, with three cameras and
opaque/mask/blend surfaces. Optional invalid normal-map fallback is checked.
Captures are `gdx-modern-e1x-market-stall-blend-camera-0..2.bmp` in
`build-hx-clean/tests/AssetRuntime/`. Classic versions retain their old names.

## 59. Animated GLB Proof

AssetRuntime.GlTFCharacterModern runs six clips, 20 shared instances, GPU pose
parity, rigid hand attachment, resize and restore. Its `gdx-modern-f5x-*.bmp`
captures share the same test source with the unchanged Classic path. The
separate normal-client smoke exercises nine stages and is reported below.
`build/f5x/runtime/gdx-modern-verified`: **892 frames, nine stages/nine captures,
17.51 seconds, exit 0**; all 20 expected actor IDs submitted. Source owners=0,
ModernRenderers=0, CPU deformation=0, fallback=0; syserr is empty.

## 60. Vegetation Proof

The bounded gate retains five Windows vegetation checks and four portable
checks. Native GR2 smoke requires actual branch, frond, leaf and billboard
submissions plus LOD changes and zero vegetation failures/owners on shutdown.
Shadow/color both use the original wind/card inputs. Detailed leaf-edge
appearance at every wind phase remains an interactive acceptance item.

## 61. Material Tests

Graphics.MaterialMapping covers glTF defaults, factors/channels, invalid
requests, neutral legacy defaults, override validation/atomicity and map fill.
Graphics.DiligentFXMaterials produces twelve native sphere fixtures:

| Row | Left to right |
|---|---|
| 1 | rough dielectric; smooth dielectric; rough metal; smooth metal |
| 2 | normal mapped; zero material AO; emissive; alpha mask |
| 3 | double-sided state; mirrored transform; nonuniform/sheared transform; normalScale=0 |

The double-sided sphere exercises cull-none state, not a dedicated reverse-face
oracle. The real GLB canopy provides an additional double-sided material case.
Images are visual evidence, not numerical proof of every BRDF parameter.

## 62. Lighting Tests

Graphics.DiligentFXModern asserts a lit center, unchanged background, one shared
light buffer, one light upload per world scene, and 20 submissions reusing it.
Left/right/high/low sun inputs each produce measurable receiver shadows; the
shadow centroid moves from left to right with the sun. Add and Modulate game
tints are separately verified. SceneLighting tests reject invalid sun data.

## 63. Shadow Tests

The native GPU test asserts cascade replay counts for mesh and terrain and
measurable shadowing in all four sun directions. Real GR2 and animated-GLB
fixtures supply production geometry/palettes. Alpha-mask and cull-none material
captures are included. Camera-edge/off-frustum caster completeness and exhaustive
extreme-angle bias behavior are not proven by these fixtures.

## 64. SSAO Tests

Graphics.DiligentFXModern waits for the upstream one-second fade, then verifies
real contact darkening for half/full-resolution AO and exact direct-only and
emission-only off/on equality. It requires both FX PSO readiness states.

An early subset missed TemporalAntiAliasingStructures.fxh, included by SSAO.
Upstream Execute returned a neutral placeholder while a simple draw smoke
appeared successful. That evidence was rejected. Include closure and explicit
readiness checks now prevent this false positive. Preliminary native runs
01/02/03 do **not** prove working SSAO; only the final verified run does.

## 65. Normal Matrix

The shader uses the actual combined skin/world inverse transpose; tangents use
the matching direction transform and determinant sign. Fixtures include
nonuniform/sheared and mirrored objects. Existing GPU/CPU pose parity tests
and six animated GLB clips remain in the gate. No new independent full-image
normal oracle for every animation/attachment is claimed.

## 66. Frontface

Cull-none and mirrored transforms are covered in fixtures; real GLB canopy and
GR2 actors exercise the native winding convention. Classic's twelve hashes are
an independent regression check. Subjective side-by-side approval is separate
from automated image generation.

## 67. Multi-Actor

The native animated-GLB test uses 20 actors sharing assets; the renderer fixture
also checks that 20 draws do not create 20 light buffers. PSOs are cached by
geometry/state variant, not actor identity. GPU textures are shared via existing
resource ownership. Numeric material caches remain palette-local; this is not
a claim of globally deduplicating every CPU material struct.

## 68. Performance

See `build/gdx/performance.md` and `performance.json` for final native Classic
versus Modern medians, p95 and sample counts. Both use the same A1/B1/A1 harness
at 1024×768 with VSync; first 60 frames per map/camera group are omitted.
CPU process excludes Present and the frame limiter. GPU frame uses the existing
query instrumentation. CSV `draws` counts actor submissions, not all FX passes.
FX counters separately report mesh/terrain/cascade draws and cached mesh/terrain
PSOs. Shadow/AO timings are CPU submission cost, **not GPU effect duration**.

| Metric | Classic | Modern High shadows + full AO |
|---|---:|---:|
| Captured frames | 3,578 | 3,499 |
| Warm scene CPU median range | .464–.614 ms | .637–.805 ms |
| Warm scene GPU median range | .054–.404 ms | .273–.731 ms |
| Largest scene CPU p95 | .890 ms | 1.125 ms |
| Largest scene GPU p95 | 1.590 ms | 2.103 ms |
| Actor draw medians (camera-dependent) | 14–18 | 14–18 |
| Bone upload medians | 65,536–114,688 bytes | 65,536–114,688 bytes |
| CPU deformation / GPU fallback | 0 / 0 | 0 / 0 |
| Peak private process memory | 495.1 MiB | 615.9 MiB |

Modern totals: 220,206 mesh draws, 185,386 terrain draws, 736,965 cascade draws;
19 cached mesh/terrain PSOs, one light-buffer creation and 3,499 light uploads.
These 19 exclude FX internal PSOs, the LUT and compositor. Whole-run average CPU
submission cost: shadows .1905 ms/frame, AO .0766 ms/frame, including first use.
The first captured frame includes initialization/loading: Classic 1,015.17 ms,
Modern 2,320.38 ms. This is a visible first-use cost, not hidden by the warm
table and not resolved by this milestone. No shader-prewarm/performance-finish
work was added. Short warm samples show no massive sustained frame regression;
they do not establish hardware-wide performance or absence of first-use hitches.

The old external G3/4 `build/g34x/performance.md` records different short scenes
at 1024×768: full shadows+AO CPU medians .765–1.047 ms and GPU .401–.653 ms.
Those historical samples have different cameras/contents and are context only,
not a controlled direct comparison or reused source implementation.

## 69. Build Impact

Five FX implementation units and 44 embedded shader files are added. Release
FX static library is 21,001,532 bytes, including build/link representation; that
is not the final executable increment. G0 native executable: 30,051,840 bytes;
G-DX: 30,703,616 bytes, +651,776 bytes (+2.17%). Baseline comes from the preserved
G0 private runtime, not a newly rebuilt clean G0. No full-build wall-time benchmark
was instrumented. Dependency download/build stays at configure/build time.
PE imports include D3D11/DXGI/compiler and existing Windows services; no FX,
Granny, SpeedTree or D3D9 DLL was introduced. Legacy DDRAW movie support remains
an existing import and is not mislabeled as a D3D9 rendering backend.

## 70. Memory

The Modern owned-target counter at High shadows and 1024×768 is 54,525,952 bytes
(52 MiB): frame attachments, shadow maps and BRDF LUT. It excludes internal FX
AO/PostFX targets, driver allocation, shader objects, assets and tiny cube data;
it is **not total GPU memory**. Native process-private peak is reported in the
performance evidence. No global VRAM peak or leak-free driver claim is made.
Observed private-process increase is 120.8 MiB (24.4%) in the matched harness.

## 71. Resource Lifetime

Dynamic material/palette SRB references and caster lists are cleared at scene
end. Late-forward bindings are cleared separately. Resize releases window
targets, PostFX context and AO; Classic destroys the Modern owner. Shutdown waits
for GPU idle before backend destruction. Native source counters and FX owner
zero are checked separately; they do not enumerate driver-internal allocations.

## 72. Resize

The GPU fixture resizes Modern resources to 192×144 and renders transparent
material afterward. Animated-GLB and existing native-window tests also exercise
resize. The normal-client settings smoke covers the real window event path;
`build/g0x/runtime/gdx-live-complete` records resize, minimize, restore and
originalSize all equal to 1. It completes 12 steps/12 captures/1,338 frames,
then a second process completes two steps/two captures/183 frames. Both exit 0,
both have empty syserr and zero checked source/render owners. ModernRenderers=0
is also logged for both Modern owner lifetimes.

## 73. Minimize/Restore

Tests require zero-size suspension without rendering, restore/reallocation and
a subsequent visible frame. The AO/history policy remains reset after restore.
The final client settings fixture records each native window result explicitly.

## 74. Map Change

Final real GR2 smoke repeats A1 → B1 → A1. Map-owned geometry/material references
are released at frame/map boundaries; the shared renderer owns view resources.
Every SSAO frame resets history, so old-map temporal AO is not reused. Native
shutdown counters validate released source assets and animation objects.

## 75. Login/Character Select

Source inspection confirms new world Begin/End only in RenderGame. Non-world
selection retains the existing point/spot/camera path. Classic regression
fixtures and live style/resource tests cover common infrastructure. An actual
authenticated login → character-select → world transition has **NOT RUN** in
this task, and no manual approval is inferred from automated captures.

## 76. Release

Full Release build: PASS. Bounded final gate: **62/62 PASS**, 88.20 seconds.
Log: `build/gdx/release-gate-final.log`. The regex includes Graphics, AssetRuntime,
AnimationRuntime, Vegetation, AssetTool, Platform and the five named Renderer
checks; it excludes vendor benchmark/fuzzer tests.

An initial overbroad `ctest -E '^playTests$'` also selected vendor fullbench,
fuzzer and zstreamtest. It was **INTENTIONALLY ABORTED** immediately after this
was noticed. It is not counted as PASS. No replacement stress/fuzz campaign
was run; all final results use the bounded selection above.

## 77. Debug

Full Debug build: PASS. Final bounded gate: **62/62 PASS**, 161.26 seconds
(`build/gdx/debug-gate-final.log`). Build logs retain LNK4099 PDB warnings, LNK4098
library conflict and LNK4075/LTCG messages; this is not a warning-free build.
Upstream FX also emits enum/shader warnings. Missing-shader errors from the
preliminary subset were fixed and are not accepted final evidence.

The full Debug output also contains **10 Diligent ERROR diagnostics** in the
unchanged G0 `Renderer.ProductionGpu` effect-test path: unbound EffectTexture
or SecondaryTexture. That executable/test passes its pixel and ownership checks;
nevertheless the diagnostics remain. New FX material/modern/GLB tests emit none
of these errors. A fresh historical G0 Debug execution was not made to prove
earlier frequency; this report does not silently classify them as harmless.
Full output: `build/gdx/debug-test-details.log`.

## 78. GCC/LP64

Portable build and gate: **33/33 PASS**, 4.60 seconds, Cygwin GCC/LP64 in
`build-hx-common`. This includes the new material/lighting contract. It does not
compile or run the D3D11 FX backend and is not an Android/Vulkan execution test.

## 79. D3D11

All new GPU fixtures execute native D3D11. Real-client smoke uses the final
Release executable, not a synthetic asset-provider switch. Final process exits,
frame counts, runtime diagnostics and source shutdown counters are summarized
below. Generated captures are inspected separately from test exit codes.

## 80. Vulkan Readiness

Neutral public data, Diligent resource interfaces and upstream portable FX
shaders preserve the architectural path. No raw D3D11 calls were added to game
or asset APIs. The backend remains D3D11; actual Vulkan compilation/execution,
coordinate conventions and device format validation are **NOT RUN**.

## 81. Android Implications

FX modular code and neutral interfaces can be carried to a future Vulkan
backend. Mobile MRT bandwidth, RGBA16F normals, depth sampling, shader resources
and AO memory need real device validation. Current High settings are not
automatically appropriate for Android. No APK/device/performance claim.

## 82. Classic Goldens

Twelve images compare against the existing external G3/4 stored baseline
SHA256 values: three market-stall cameras, six clips, 20 actors, hand attachment
and window-restored image. Both the final Release comparison and the comparison
after the Debug gate are **12/12 byte identical**. Final hashes are retained in
`build/gdx/classic-comparison.json`. No baseline was rewritten.

## 83. Zero Legacy Audit

The bounded gate includes NoGrannyDependency, ProviderDependencies,
NoLegacyArchitecture and Vegetation.NoLegacyDependency. Native startup requires
production GPU/native reader/animation/vegetation defaults. Shutdown checks
GrannyFileReads=0, reference/import pose use=0, CPU deformation=0 and fallback=0.
No D3D9, Granny or SpeedTree rendering path is restored. PE imports are retained
as separate binary evidence; unavailable OS module enumeration is not invented.

## 84. Visual Proof

`build/gdx/gallery.html` collects native material fixtures, Classic/Modern GLB
views, animated GLB, and matched real-world near/far captures. Images remain
unaltered except lossless PPM/BMP-to-PNG conversion for viewing. They prove what
was rendered in these scenes; final artistic/gameplay acceptance remains
separate. Early acne/ambient/placeholder runs are diagnostic evidence only.
The gallery has 34 source captures with a SHA256/dimension manifest. Inspected
final images include material fixtures, market stall, A1 player/NPC/weapon,
B1 fountain and the native 20-actor group. B1's pre-existing water overlay is
also present in the Classic reference and was not modified as G7 work.

The first settings fixture run checked the backend snapshot inside the callback
that changed it; the contract applies it at the next frame boundary. A second
run exposed its stale restart expectation after the added Classic toggle.
Both test assumptions were corrected. The final fresh `gdx-live-complete` run
checks next-frame application and explicitly persists Modern before restart;
the earlier failed fixture runs are not counted as PASS.

## 85. Git Diff

Final branch/HEAD/main/index and complete changed-file inventory are recorded
in `build/gdx/git-final.txt`. Main and the runtime repository are untouched by
this work. Source changes are uncommitted and unstaged. All generated builds,
captures, dependency archives, logs and temporary tools stay in ignored build
directories. No reset, commit, push, history rewrite or milestone expansion.
Final source branch `feature/gdx-diligentfx`, HEAD
`ca63fd11959971699844421bae78083f82d8c018`; main remains
`ea1142c5224311c9ed7cfe604bd3471d775dea54`. Index diff is empty.
Runtime HEAD remains `c63a542141741071bec798c17a205d12e362d0e1` with its
pre-existing dirty file set. Complete reviewable patch, including new files:
`build/gdx/source.diff`; SHA256 inventory: `build/gdx/changed-files.json`.
There are 42 changed/new source, build-integration, test and report files.

## 86. Known Limitations

* Independent off-frustum shadow caster discovery is absent; camera-edge shadow
  coverage requires follow-up within G-DX acceptance.
* Real authenticated login/character-selection and exhaustive manual visual
  approval have not been completed.
* Independent optional-map UV sets/transforms are explicitly rejected.
* Overrides are an import-time API foundation; no authored sidecar loader yet.
* IBL is constant-environment foundation; no directional sky/probe filtering.
* LDR intermediate clipping remains until G5/6; no HDR/tonemap claim.
* Spatial AO resets every frame; TAA/SSR require future true motion producers.
* Shadow/AO GPU costs and total VRAM are not separately instrumented; CPU submit
  costs and full GPU frame timings must not be confused.
* Fixture-based normal/front-face/alpha coverage is bounded, not exhaustive.
* First captured Modern frame takes 2.32 seconds including initialization,
  versus 1.02 seconds in Classic. First-use compilation/allocation needs explicit
  acceptance; warm performance numbers do not remove that cost.
* Ten unresolved Debug effect-binding ERROR diagnostics remain in the unchanged
  G0 effect-test path; the tests pass, but a clean diagnostic gate is not claimed.
* Vulkan/Android execution is unverified; no blanket warning-free claim.

## 87. Historical GO/NO-GO before G-DX-C

**Overall: NO-GO for an unconditional G-DX acceptance / move to G5/6.**
The common DiligentFX production path is implemented and the bounded builds,
GPU tests and native fixtures pass. Remaining acceptance blockers are the
incomplete off-frustum caster coverage, missing actual login/character-select
and manual visual approval, plus unaccepted first-use cost and unresolved
Debug effect-binding diagnostics. Passing tests do not override these gaps.

| Evidence | Final result |
|---|---|
| Correct G0 start / unchanged main / empty index | PASS |
| FX A–T audit, reuse matrix, verified archive/subset | PASS |
| Full Release build / bounded gate | PASS / 62 of 62 |
| Full Debug build / bounded gate | PASS / 62 of 62; 10 effect-test diagnostics retained |
| GCC/LP64 build / bounded gate | PASS / 33 of 33 |
| Classic external goldens | PASS / 12 of 12 byte-identical |
| Native GR2 A1 → B1 → A1 | PASS / 3,499 frames, six captures, exit 0 |
| Native animated GLB / 20 actors | PASS / nine stages, 892 frames, exit 0 |
| Native live apply / resize / minimize / restore / restart | PASS / two processes, 14 captures, both exit 0 |
| Tracked source shutdown owners / Modern owner | PASS / zero |
| CPU deformation / GPU fallback / Granny reads | PASS / zero |
| Sustained warm performance sanity | PASS for these local samples; first use remains open |
| True login/character select / manual visual approval | NOT RUN |
| Vulkan / Android execution | NOT RUN |
| Initial accidental vendor benchmark/fuzzer selection | INTENTIONALLY ABORTED |

Evidence index: `release-build-final.log`, `debug-build-final.log`,
`release-gate-final.log`, `debug-gate-final.log`, `gcc-gate-final.log`,
`debug-test-details.log`, `native-gr2-verified-05.log`, `native-glb-final.log`,
`native-live-complete.log`, `classic-comparison.json`, `performance.json`,
`capture-manifest.json`, `dependency-verification.json`, `binary-dependencies.txt`
and `git-final.txt`, all under `build/gdx/`. Full local images/runtimes remain
in ignored build directories for review. This report records a concrete tested
implementation with remaining gaps; it does not certify those gaps as solved.

## 88. Recommendation G5/6-X

Do not start G5/6 automatically. Close the G-DX acceptance gaps first. The audit
identifies FX Bloom, ToneMapping and EpipolarLightScattering as later candidates
with the shared scene sun and depth boundary. No G5/6, G7, H2, UI implementation
or performance-finish work is included. Stop after this report and evidence.

## 89. G-DX-C Closure Result

The [closure report](phase-gdxc-closure.md) supersedes the four acceptance gaps
in sections 86–87: light/cascade-based off-camera caster collection, zero Debug
effect-binding errors, measured world-entry loading/prewarm and a real
authenticated lifecycle with two world entries and exit 0. Release/Debug
bounded gates remain 62/62, GCC/LP64 33/33 and Classic 12/12 byte-identical;
subsequent changes received focused GPU rechecks and final native/manual proof.
No subsequent milestone, commit or push was started. Other explicitly scoped
limitations in this original report remain unchanged.
