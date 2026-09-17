# C-LIB-X - Core Library Reuse Audit & Selective Adoption

**C-LIB-X = GO.** The audit is closed with the existing core retained and no new
production adoption. The targeted evidence and its limits are in **C-LIB-X-C
Closure** below. Library decisions remain unchanged. Original ACL measurements
for the procedural GLB are superseded because the closure found an encoder-input
hierarchy error in the isolated harness.

## 1. Baseline

2026-09-15: Source `feature/gdx-diligentfx` / `d92dc48`, client assets
`main` / `72d92593`; both working trees clean before this audit. The committed
[G-DX-C closure](../graphics/phase-gdxc-closure.md) is the baseline, including
62/62 Release, 62/62 Debug, 33/33 GCC/LP64, 12/12 Classic images and the verified
two-entry lifecycle. These are historical baseline results, not fresh C-LIB-X tests.

## 2. Philosophy

Library-first, not library-everywhere. Keep working domain-specific code. A library
must remove measurable cost or complexity without weakening correctness.

## 3. Selection Criteria

Audit -> real-data proof -> integration cost -> decision. Warm and setup costs
are separate. No production adoption before correctness and quality evidence.
Dependencies used by the proof live only in an opt-in offline audit target.
No ECS/physics/engine rewrite, G5/6, G7, H2, or UI work. No commit or push.

## 4. Reuse Matrix

The matrix was created before measurement and is now updated with the measured
decisions. API portability is not a tested device claim.

| Library | Current area | Potential benefit | Integration cost | Runtime dependency? | Offline-only possible? | D3D11 | Vulkan | Android | License | Benchmark needed? | Decision |
|---|---|---|---|---|---|---|---|---|---|---|---|
| ozz-animation 0.17.0 | AnimationRuntime Sample/Blend/Evaluate | SIMD pose throughput, compact tracks | High: TRS/shear, bone order, semantics, attachments | Yes for replacement | Conversion only | CPU neutral | CPU neutral | ARM path; untested here | MIT | Real skeletons, poses and quality | NO-GO |
| ACL 2.1.0 + pinned RTM | GR2/GLB imported clips | RAM/storage, decode | Medium/high: resampling, full scale/shear, cache format | Decoder | Encoder only | CPU neutral | CPU neutral | ARM path; untested here | MIT | Real clips, errors, size and decode | LATER |
| EnTT resource_cache | CResourceManager, shared document handles | Less cache boilerplate | High: existing identity, queue and lifetime rules remain | Header library | No for runtime cache | Neutral | Neutral | C++ portability | MIT | Only if architectural benefit survives audit | NO-GO |
| EnTT ECS | Actors/maps | Future component queries | Very high gameplay migration | Yes | Editor tooling | Neutral | Neutral | C++ portability | MIT | No rewrite in scope | LATER |
| KTX2 / Khronos KTX | DDS/decoded image uploads | Mips, metadata, portable GPU payloads | Medium: validation, formats, upload | Optional loader | Yes, platform payloads | BC formats | API-neutral container | ASTC/ETC2 payloads | Apache-2.0 + component terms, see section 41 | Real textures and load cost | LATER |
| Basis Universal 2.50 | Texture distribution | Smaller packs, portable transcode | Medium/high: quality, colorspace, preparation | Only for universal payloads | Yes | BC1/3/5/7 as appropriate | Format-dependent | ASTC/ETC2 as supported | Apache-2.0 + component notices | Size, timing, mips, visual error | LATER |
| meshoptimizer 0.25 | Existing opt-in AssetTool | Cache/fetch, LOD, geometry compression | Low for offline options; high for new runtime codecs | Currently no | Yes | Neutral | Neutral | Neutral | MIT | Real meshes, ACMR and bytes | KEEP; expansion LATER |
| Recast/Detour | Current collision/movement | Future navmesh/editor tools | High without a current consumer | If navigation adopted | Navmesh generation | Neutral | Neutral | Portable C++ | zlib | Need audit only | LATER |
| Jolt | Current game collision | Rigid bodies/ragdolls | Very high; no demonstrated consumer | Yes | Limited tooling | Neutral | Neutral | ARM supported upstream | MIT | Need audit only | NO-GO |
| Editor/gizmo helpers | Future J-X | Editor manipulation/UI | Deferred | Editor only | Yes | Backend-dependent | Backend-dependent | Not a current target | Candidate-specific | Outside this milestone | LATER, J-X note only |

### Final decisions

| Candidate | Decision for this milestone | Concrete reason |
|---|---|---|
| ozz | **NO-GO** for runtime replacement | Large blend/pose gains, but TRS projection loses real boss shear; default precision/interpolation changes quality; GR2 sampling often slower |
| ACL | **LATER** | Very large GR2 payload/decode gains; current resampling/shear converter fails quality; no production clip/cache format or visual acceptance |
| EnTT cache | **NO-GO** | Existing identity, queues, deduplication and thread/lifetime rules would remain underneath another wrapper |
| EnTT ECS | **LATER** | Possible editor/query use; no justified gameplay migration |
| Khronos KTX / KTX2 | **LATER** | Useful portable container; no present desktop loading/VRAM win established, full Khronos loader not adopted |
| Basis Universal | **LATER**; current desktop conversion **NO-GO** | ETC1S saves pack bytes with image/alpha changes; UASTC mostly increases DDS pack size; preparation costs remain |
| meshoptimizer | **KEEP**, extensions **LATER** | Existing cache/fetch/static opaque LOD retained; extra analyzer/codec gains lack a validated renderer/storage consumer |
| Recast/Detour | **LATER** | No current client navmesh consumer; editor/server navigation is a separate need |
| Jolt | **NO-GO** | No required rigid-body/ragdoll/vehicle system; replacing Metin2 collision is disproportionate |
| Editor helpers | **LATER / J-X** | Note only, no integration |

Evidence: [retained raw measurements](clibx-benchmark-evidence.json),
[reproduction instructions](../../tools/CoreLibraryAudit/README.md),
[animation corpus](../../tools/CoreLibraryAudit/animation-corpus.tsv),
[mesh corpus](../../tools/CoreLibraryAudit/mesh-corpus.tsv).
The JSON contains all median/P95/max rows, quality metrics, byte counts, texture
encoder arguments and input hashes. Generated assets/logs/binaries stay in
`build/clibx`; production assets are not duplicated into documentation.

## 5. ozz Audit

Audited/built [ozz 0.17.0](https://github.com/guillaumeblanc/ozz-animation/tree/0.17.0).
Its SoA SIMD sampling, weighted blending and local-to-model jobs are reusable CPU
algorithms. Per-instance sampling contexts and output poses must remain separate;
immutable clips/skeletons can be shared. The library does not replace the game's
motion events, root movement, playback state, attachment identity or GPU palette.

The proof maps the hierarchy back to original bone indices, retains native
row-vector world-matrix conventions, and measures actual ozz local-to-model output.
It uses original irregular keys, the default builder and no extra key optimizer.
Current `LocalTransform` has a full 3x3 scale/shear matrix; ozz uses TRS. This is
an actual corpus mismatch: boss maximum off-diagonal shear is **0.0530004**.
GR2 native interpolation and GLB spherical interpolation also need a defined
conversion contract. STEP boundaries are not represented by a plain nlerp track;
none of these 15 clips contains STEP tracks, so STEP parity is not established.

## 6. ozz Benchmark

9 real GR2 motions: player idle/walk/run/attack/damage/death, wolf, boss, mount.
6 clips from the repository's procedural GLB character supplement them.
**No production animated GLB was identified; the user has no additional path.**
That category remains a coverage gap, rather than being relabeled production data.
Each clip was measured with 1, 20 and 100 actors: 45 warm cases.

Windows x64, MSVC 14.44, Release/O2; 64 warm iterations, 31 repeated batches;
32 frames per batch, 8 for 100 actors. Statistics are average-frame batch
statistics, not individual-frame tail latency. Timings below are microseconds
for the entire group of 20 actors, **median / P95 / max**.

| Operation / case | Current | ozz |
|---|---:|---:|
| Sample player run | 82.475 / 84.313 / 84.963 | 141.716 / 154.591 / 182.831 |
| Blend player run | 130.972 / 137.378 / 142.666 | 3.047 / 3.203 / 3.216 |
| Local-to-model player run | 112.006 / 115.963 / 118.562 | 6.525 / 6.684 / 6.731 |
| Sample wolf | 64.334 / 67.375 / 71.797 | 123.509 / 131.556 / 144.031 |
| Sample boss | 127.106 / 132.134 / 133.188 | 106.028 / 114.009 / 179.213 |
| Blend boss | 282.403 / 299.631 / 302.175 | 6.259 / 6.334 / 6.412 |
| Local-to-model boss | 241.216 / 252.250 / 266.197 | 13.356 / 13.941 / 13.963 |
| Sample GLB fixture clip 5 | 1.622 / 1.816 / 1.969 | 1.500 / 1.650 / 1.653 |

These are substantial blend/pose gains and must not be dismissed as negligible.
They are not an end-to-end FPS claim: format adaptation, palette uploads,
attachments and a production renderer were not switched to ozz.
Instrumented warm allocation count is zero for all 45 cases.
Default ozz storage reduces GR2 clip payload to roughly one third, but retains
many adaptive source keys. Observed setup builder time is 0.027-33.805 ms per clip.
Player/mob world-position deviations are about 0.054-0.065 client units; mount
0.102; boss **5.240**. GLB fixture clip 3 reaches **1.482** world-position units
and **1.27 degrees** local rotation deviation. Actual ozz world matrices were
checked separately, not inferred only from decoded TRS.

## 7. ozz Decision

**NO-GO for replacing AnimationRuntime now.** Correctness and motion semantics
are not preserved by this proof, despite the strong SIMD benefit. Adopting only
blend/local-to-model would still require an efficient full-shear-compatible
boundary and end-to-end measurements. No runtime dependency or hybrid wrapper
was added. SIMD pose work is a possible future measured task, not this adoption.

## 8. ACL Audit

Built [ACL 2.1.0](https://github.com/nfrechette/acl/tree/v2.1.0) with its pinned RTM
revision. Offline encoder and a small runtime decoder can share immutable
compressed data with one decoding context per actor. The proof writes into the
current pose representation; it does not replace skinning or asset ownership.
Settings: default medium compression, transform precision 0.001 client units,
shell distance 100. Source clips are resampled to approximately 60 Hz, then
checked at a dense grid and, originally, translation key boundaries +/- 1 microsecond.
Closure checks translation, rotation and scale keys +/- 1 microsecond on one
shared time set for A/current, B/uncompressed bake and C/ACL decode.
An additional 240 Hz run isolates resampling effects. These are candidate
conversion settings, not ACL's fundamental maximum quality.

Full scale/shear, adaptive GR2 knots, GLB interpolation, boundary/loop rules,
root motion/events, attachment indices, source hashes and cache versioning need
a production offline clip format. The native source's irregular keys cannot be
silently replaced by a fixed frequency without an error-bounded conversion.

## 9. ACL Benchmark

Sizes below are **current resident clip capacity / compressed ACL payload**,
not original GR2 file size or an existing on-disk clip pack. Skeletons and
per-actor state are separate. Maximum world-position errors are client units.

| Real motion | Current bytes | ACL 60 Hz bytes | World error 60 Hz | World error 240 Hz |
|---|---:|---:|---:|---:|
| Player idle | 1,185,808 | 45,179 | 0.0112 | 0.00087 |
| Walk | 2,112,400 | 18,307 | 1.0728 | 0.0701 |
| Run | 2,721,064 | 17,642 | 1.1315 | 0.0761 |
| Attack | 4,254,592 | 26,777 | 6.7579 | 0.4275 |
| Damage | 2,379,448 | 13,977 | 1.6919 | 0.1147 |
| Death | 7,731,184 | 65,945 | 2.4054 | 0.1567 |
| Wolf run | 2,796,872 | 13,687 | 2.2024 | 0.1456 |
| Boss attack | 3,458,152 | 145,805 | 5.2623 | 5.2631 |
| Mount run | 2,854,752 | 16,108 | 1.8581 | 0.1256 |

60 Hz reduces this measured resident payload by **95.8-99.5%**. That impressive
number is primarily against native expanded/adaptive runtime tracks; it is
**not** a measured production-pack reduction. Increasing rate increases payload:
idle becomes 149,719 bytes at 240 Hz, versus 45,179 at 60 Hz.

| Decode, 20 actors | Current sample median/P95/max us | ACL median/P95/max us |
|---|---:|---:|
| Player run | 82.475 / 84.313 / 84.963 | 23.116 / 26.653 / 27.047 |
| Player attack | 88.153 / 91.303 / 91.519 | 24.562 / 25.872 / 26.425 |
| Wolf | 64.334 / 67.375 / 71.797 | 15.216 / 15.628 / 15.850 |
| Boss | 127.106 / 132.134 / 133.188 | 50.103 / 51.522 / 51.803 |
| Mount | 70.584 / 76.138 / 78.300 | 18.428 / 18.984 / 19.962 |

Local translation/rotation/scale and world-matrix maxima are all retained in
the JSON. For example player attack at 60 Hz has 0.7701 translation and
4.3000-degree rotation error; boss has 0.05302 scale/shear error.
Boss resampling/projection alone originally gave 5.26233 world error; ACL
quantization against that resampled pose gave 0.05350. With the closure's shared
all-channel boundary sample set these become 5.26384 and 0.05479. Fourfold
resampling cannot restore discarded shear.

**Superseded GLB ACL evidence:** the original 78.763 world error / 40.85-degree
rotation error for clip 2, its 240 Hz result, GLB compressed sizes and GLB ACL
timings were produced with invalid encoder hierarchy order. They cannot support
a codec-quality or performance claim. The original raw JSON is preserved as
history; use the corrected closure evidence instead. Clip 2 at 60 Hz now has
4.09170 world / 2.04762-degree rotation error, mainly from the uncompressed bake.
Clip 5 is 1,086 ACL bytes at 60 Hz, versus 880 resident source bytes.
The real GR2 size/decode tables remain valid: targeted player/mob/boss/mount
controls retain the same payloads and require no encoder-index reordering.

## 10. ACL Decision

**LATER.** Highest animation-storage potential, but no currently proven
quality-preserving converter. Retain GR2/GLB originals and current runtime.
A future bounded follow-up needs error-bounded resampling, full-shear policy,
actual production GLB coverage, serialized-cache/pack measurements and visual
motion/attachment acceptance. This audit rejects the tested conversion path,
not ACL's ability to achieve higher quality with another representation.

## 11. EnTT Resource Cache Audit

Compared [EnTT resource management](https://github.com/skypjack/entt/wiki/Resource-management)
and [cache implementation](https://github.com/skypjack/entt/blob/main/src/entt/resource/cache.hpp)
against `AssetRuntime.h`, `ResourceManager.cpp`, `FileLoaderThread.cpp` and the
GR2 bound-animation cache. Asset/model/animation handles already own shared
documents and indices. The existing manager also supplies normalized identity,
resource reuse, encoded-image content checks, asynchronous completion queues,
delayed destruction and reload behavior. GR2 binding cache identity includes
skeleton/model/boundary semantics. EnTT cannot remove those requirements.

Generic loader/shared ownership is useful in isolation, but no concrete lifetime,
sharing, deduplication or synchronization defect here is solved by wrapping it.
EnTT cache is not an automatic thread-safety guarantee; application synchronization
and GPU-thread ownership would still be necessary. Existing mutexes are described
only where implemented; this audit does not certify every manager API thread-safe.

## 12. EnTT Cache Decision

**NO-GO.** No demonstrable code removal or performance benefit survives the
identity/queue/lifetime requirements. No fabricated microbenchmark of an empty
map was used to justify replacing the real resource manager.

## 13. EnTT ECS Decision

**LATER**, potentially for J-X queries/editor entities. Current actor movement,
network identity, map ownership, race data and attachments remain. No ECS rewrite,
runtime benchmark or dependency addition was performed.

## 14. KTX2 Audit

[KTX2 specification](https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html)
provides levels, dimensions, format/data-format description, metadata and
supercompression. Its level index is useful for level-oriented loading; this
alone does not implement streaming, residency or scheduling. D3D11/Vulkan still
need validated format selection, capabilities, upload layout and sRGB/linear
views. Current `ImageDecoder.cpp` preserves DDS payloads and decodes other images
with stb; embedded-image acceptance currently covers PNG/JPEG, not KTX2.

The proof uses Basis' KTX2 reader, not a production integration of
[Khronos KTX-Software](https://github.com/KhronosGroup/KTX-Software).
Full KTX-Software compile time, loader memory and runtime binary cost were **not
measured**. Its broader feature surface is not needed to justify this proof.

## 15. Basis Universal Audit

Built [Basis Universal 2.50](https://github.com/BinomialLLC/basis_universal/tree/v2_50).
ETC1S uses q255/comp_level1; UASTC uses level2; encoder is single-threaded and
OpenCL is disabled. All original DDS mip levels are preserved via decoded RGBA
DDS input. There is no resizing or newly generated mip chain in this comparison.
Color inputs use sRGB metadata, the supplemental normal uses linear metadata
and explicit R/G packing for BC5. Native CPU transcoding was exercised for
BC1/BC3/BC5/BC7/ETC2/ASTC; semantic quality is checked for the relevant BC target
and BC7, not asserted for every target format or Android GPU.

## 16. Texture Benchmark

Six real assets: grass, dirt/field, rock, GR2 character, leaf atlas and UI.
The 8x8 GLB basecolor fixture and generated normal probe are **supplemental**.
No local production PBR basecolor/normalmap pair was found; user supplied no path.
The closure adds an external authored FlightHelmet PBR model with existing mips;
see the separate four-texture comparison below. It is not a Metin2 production asset.
These missing categories cannot receive a production quality PASS.

Pack size uses the actual current **Zstd level 17** policy from PackMaker on
both original and encoded bytes; equal pack metadata/encryption overhead is
excluded. Source DDS bytes and mip counts, all KTX2 sizes and CPU block payloads
are retained. Table uses same BC1/BC3 GPU format as the DDS reference where
applicable. UI is an uncompressed TGA baseline.

| Asset | Original pack B | ETC1S pack B | UASTC pack B | RGB PSNR dB ETC1S / UASTC | GPU block bytes |
|---|---:|---:|---:|---:|---:|
| Grass, 256x256, 4 mips | 39,091 | 31,111 | 69,860 | 31.98 / exact | 43,520 BC1 |
| Dirt, 256x256, 4 mips | 36,662 | 29,928 | 66,923 | 33.30 / exact | 43,520 BC1 |
| Rock, 512x512, 3 mips | 121,802 | 96,866 | 229,048 | 36.38 / 62.60 | 172,032 BC1 |
| Character, 512x512, 10 mips | 244,051 | 143,780 | 310,076 | 31.65 / 47.18 | 349,552 BC3 |
| Leaf, 1024x1024, 6 mips | 390,470 | 216,231 | 472,332 | 34.86 / 41.89 | 1,397,760 BC3 |
| UI, 41x40, 1 mip | 2,747 | 739 | 1,662 | 25.68 / 28.05 | 1,760 BC3 |

ETC1S saves about 18-45% of DDS pack bytes; UASTC increases these DDS payloads
by 21-88%. UI can save bytes but its small gradient/border changes; this does
not justify applying a lossy policy to UI. Leaf alpha RMSE is **8.946 / 5.330**
on a 0-255 scale; mip-0 alpha coverage at threshold128 shifts +0.244 / +0.071
percentage points. Character alpha also changes. These are measured deviations,
not an accepted visual quality threshold.

CPU parse/start/transcode preparation, median/P95/max in microseconds:

| Asset | ETC1S | UASTC | Original pack-decode median only |
|---|---:|---:|---:|
| Grass | 505.10 / 568.05 / 578.90 | 267.45 / 331.40 / 497.75 | 38.8 |
| Dirt | 489.80 / 515.40 / 516.05 | 271.25 / 313.30 / 541.90 | 41.8 |
| Rock | 1841.10 / 1896.60 / 1930.35 | 1187.75 / 1201.60 / 1224.50 | 174-175 |
| Character | 3354.80 / 3382.50 / 3384.10 | 3665.05 / 3754.70 / 3849.50 | 258-260 |
| Leaf | 5331.00 / 6098.90 / 6300.75 | 6265.35 / 6325.25 / 6330.75 | 465-474 |
| UI | 20.15 / 23.05 / 24.75 | 22.25 / 22.60 / 23.05 | 8.8 |

Preparation excludes disk I/O, GPU upload, candidate outer-pack decompression
and output allocation (buffers preallocated). Original column excludes DDS/TGA
decode/upload. They are component costs, **not equivalent full loading paths**.
Warm transcoder-only and first-use values are separately retained in JSON.
Texture timing uses 31 batches (warm transcode batch4, prepare batch2).
Basis global initialization is about 24 ms in the observed grass process.

Mip dimensions/count and KTX2 transfer-function metadata were checked for every
candidate; per-mip RGB/coverage results are retained. A contact sheet was visually
inspected: terrain detail, character alpha, leaf silhouette and the small UI
gradient are represented. This is offline image QA, **not in-game gamma/lighting
or mip-transition acceptance**. The normal probe reaches mean/max angular error
1.041/3.330 degrees ETC1S and 0.337/1.394 UASTC in reconstructed BC5 vectors;
it remains synthetic. RGB PSNR is not used as normal-map quality evidence.

## 17. KTX2/Basis Decision

**LATER**, with **NO-GO for a blanket current desktop conversion**. Mobile
distribution remains a valid possible use, but no current-device VRAM/load win
was shown. Prefer platform-specific offline BC/ASTC/ETC2 output when a target
exists; otherwise prepare transcodes during Loading and cache results. Do not
introduce first-use transcoding during gameplay. DDS remains production.

## 18. meshoptimizer Current Usage

Keep the existing pinned [meshoptimizer 0.25](https://github.com/zeux/meshoptimizer/tree/v0.25)
inside opt-in offline AssetTool. `Processing.cpp` already optimizes vertex cache
and fetch/remapping. Static opaque LOD simplification uses 1% relative error and
locked borders; skinned and non-opaque meshes are deliberately excluded.
It is not a production client dependency.

## 19. meshoptimizer Additional Opportunities

21 mesh records from a real building, wolf, weapon, rock and compiled baobab
(15 primitives), plus two procedural GLB character primitives. Codec roundtrips
passed all 21; index encoding may cyclically rotate triangle vertices but
preserves triangle order/winding. Full vertex strides are preserved.

| Mesh | Vertices / indices | Current cache ACMR | With extra overdraw | Raw / encoded stream bytes |
|---|---:|---:|---:|---:|
| Building | 2251 / 2664 | 1.8739 | 1.8739 | 118704 / 38196 |
| Wolf | 762 / 2898 | 0.8530 | 0.8675 | 48168 / 23811 |

Building overdraw analyzer improves 1.3047 -> 1.0897 (~16.5%); rock changes only
1.01469 -> 1.01455. Wolf cache metric becomes slightly worse with extra overdraw.
Building offline overdraw cost is 10.600/10.675/14.400 us median/P95/max;
wolf 6.775/6.825/7.475. Full corpus counts, ACMR, codec sizes and attempted LOD
errors are in JSON. Byte counts are streams, **not GLB or Zstd-packed asset files**.
Analyzer results are geometric proxies, not actual camera/material GPU render
cost; GR2 material-range constraints and alpha ordering need separate validation.
No measured FPS claim is made. LOD trials on skinned/leaf data are measurements
only, not approved output. Meshlets lack a production consumer here.

## 20. meshoptimizer Decision

**KEEP**, additional overdraw/geometry codec/LOD options **LATER**. Existing useful
offline processing remains. A few proxy improvements do not justify a new
runtime codec or a blanket overdraw setting without representative draw costs,
material-safe processing and real packed-file comparison.

## 21. Recast/Detour Decision

[Recast/Detour](https://github.com/recastnavigation/recastnavigation): **LATER**.
`InstanceBaseMovement.cpp`, world attributes and collision checks currently
implement movement/avoidance semantics, not a navmesh consumer. Editor navigation
visualization or server AI would be a concrete future reason. Neither exists as
an adoption requirement here; no dependency or navigation benchmark was added.

## 22. Jolt Decision

[Jolt](https://github.com/jrouwe/JoltPhysics): **NO-GO for this client task**.
`PhysicsObject.cpp` implements game movement/knockback and world collision;
`ActorInstanceCollisionDetection.cpp` implements actor/body/attack collision.
No rigid bodies, vehicles, ragdolls or new character controller are requested.
A physics engine would require reconciling gameplay/network semantics rather
than removing a demonstrated generic bottleneck.

## 23. Editor Libraries Note

J-X only: Dear ImGui/ImGuizmo could supply editor panels/manipulators; node and
file-dialog helpers need a specific editor flow before selection. No library
download, integration or UI change was made for this note.

## 24. GR2 Reader Decision

**KEEP native ZiiNAN.** It is format-specific and supplies production binding
semantics the generic candidates do not replace. No Granny/proprietary fallback.
The repaired Redthief `back_damage.gr2` remains intact. The known raw-corpus
rejects documented by F3/4-X remain distinct from supported production references;
this report makes no new 100%-raw-corpus claim.

## 25. Asset Runtime Decision

**KEEP** shared handles/documents, native import, caching and async preparation.
Future ACL/KTX work would attach a validated representation under those contracts,
not add a parallel owner/cache hierarchy. Current source tests and runtime
lifetime checks are the relevant regression evidence.

## 26. Animation Runtime Final Decision

**KEEP** current Sample/Blend/Evaluate, full scale/shear, attachments, palettes,
loop/playback and GPU skinning. The proof shows potential for compact clips and
SIMD pose operations, but neither tested converter preserves required quality.
No current CPU vertex deformation path or automatic GPU fallback was added.

## 27. Vegetation Runtime Decision

**KEEP** legacy SPT name registry, compiled GLB/ZVEG mappings, map instances,
LOD/wind/collision semantics. Existing meshoptimizer and Diligent instancing are
the relevant generic pieces. Texture alpha measurements forbid claiming a safe
leaf conversion. No new vegetation library or H2 work.

## 28. SceneLighting Decision

**KEEP** ZiiNAN world/map lighting semantics and Diligent/DiligentFX rendering.
A generic scene graph does not solve map sun placement; none was added.

## 29. GraphicsSettings Decision

**KEEP** persistence, presets and existing UI/runtime integration. No external
library offers a demonstrated simplification. No graphics settings/UI edits.

## 30. Actor/Map/Game Integration Decision

**KEEP** Metin2-specific actors, movement, races, maps and network integration.
Generic jobs/data structures are already used where needed; no profile supports
an ECS or general game framework migration in this milestone.

## 31. Threading Audit if needed

`FileLoaderThread.cpp` uses `CGameThreadPool`, synchronized completion and an
existing synchronous fallback if no pool exists. G-DX-C moved first-use
preparation into Loading. No new measured bottleneck requires a job-system
library; **KEEP**. This is a focused source/needs audit, not a scheduler benchmark.

## 32. Offline-vs-Runtime Strategy

Encode clips/textures and optimize meshes offline where possible. Preserve
original GR2/GLB/DDS. Runtime should consume validated, versioned outputs and
perform only necessary decode/upload work. Existing Zstd pack framing remains;
no new pack system or runtime Assimp/meshoptimizer dependency.

## 33. First-Use Costs

Separate single observed import/setup/encode/init from repeated warm work.
Animation import/bind spans 0.538-89.487 ms, ozz building up to 33.805 ms,
ACL compression up to 59.048 ms in the measured corpus. These are offline/setup
costs, not gameplay work to introduce. Basis global init (~24 ms) and multi-ms
large-texture preparation need Loading/prewarm or offline target formats.
No cold-disk or total world-entry speedup was measured for a candidate.

## 34. RAM

Clip bytes are explicit payload/capacity accounting; allocator counters are
requested bytes, not resident process RAM. ACL decoder context is 128 bytes per
actor in this build; poses/world buffers remain necessary. Ozz uses additional
contexts/SoA buffers, so clip compression is not total memory reduction.
Full before/after process RSS for an adopted candidate is **not applicable**:
none was adopted. Compression scratch figures and setup state are retained,
rather than claiming the payload ratio is a process-memory result.

## 35. VRAM

Same BC format, dimensions and stored mip chain yields the same GPU block byte
count for DDS and KTX2. BC7 would double BC1 block storage; it is not selected
blindly. UI could reduce RGBA8 payload but fails quality acceptance. Actual VRAM
allocation/pitch/driver residency was **not measured** for candidate textures.
No production formats or buffers changed.

## 36. Asset Size

Texture table measures both original source and real Zstd17 pack payloads.
Animation table measures resident tracks vs ACL payload; no serialized production
clip cache/pack savings are claimed. Mesh table measures raw/codec streams;
no GLB-extension or packed-file savings are claimed. These distinct byte counts
must not be combined into a fictitious overall download/RAM saving.

## 37. Binary Size

Baseline executable sizes: Release **30,716,928 B**, Debug **42,526,208 B**.
Hashes were captured before work in the evidence JSON. Fresh production builds
are compared in the validation appendix below; relinked files need not have
identical PE hashes. No audit library is in the production link inputs and
`M2_BUILD_CORE_LIBRARY_AUDIT=OFF` in both normal build trees. Khronos KTX itself
was not built, so no invented full-library binary-cost number is reported.

## 38. Loading Time

No production loading path changed. Native clip import/setup and texture
preparation component costs were measured. End-to-end candidate disk/pack/GPU
load time and world-entry time are **NOT RUN**; no candidate was integrated.
G-DX-C loading preparation remains the production behavior.

## 39. Runtime Performance

Current, ACL and ozz warm timing groups use the same skeletons/motion times and
actor counts. Pose/codec differences are disclosed above. Mesh metrics are
proxies; texture timing is CPU work. No fabricated FPS/render-cost improvement.
Production performance is protected by retaining its code/dependencies and the
bounded regression gate, not by asserting an unmeasured global speedup.

## 40. Build Complexity

Only a default-OFF root CMake option and an opt-in tools subdirectory were added.
No new FetchContent call or network access is added to normal configure/build.
Five archive pins (ozz/ACL/RTM/Basis/meshoptimizer) and SHA256 checks are in
`prepare_dependencies.py`; meshoptimizer's version matches existing AssetTool.
The proof may reuse the already prepared E2-X meshoptimizer directory via
`CLIB_MESHOPT`. Basis' upstream encoder archive is separately built; the Windows
texture proof matches its DLL CRT. The client retains its own build settings.

Observed production Release incremental rebuild/regeneration: **74.806 s**.
The diagnostic animation target rebuild took approximately **5.15 s**; this is
not a clean dependency build-time measurement. First clean upstream build time
was not instrumented and is not estimated. Upstream source archives include
unused examples/tools; they are not production binary dependencies.

## 41. License

| Candidate | License / redistribution obligations | Third-party boundary |
|---|---|---|
| ozz | MIT: retain copyright/license | Optional FBX/glTF/tools disabled |
| ACL + RTM | MIT: retain both notices | RTM commit pinned; headers used only by proof |
| EnTT | MIT: retain notice if adopted | Header cache/ECS only audited |
| KTX-Software | Generally Apache-2.0 plus per-component terms; preserve license/notices and modification notices where required | Upstream license lists a special Ericsson `etcdec.cxx` license; **not blanket Apache**, not vendored/adopted |
| Basis | Apache-2.0 plus component notices | Zstd BSD-3-Clause, tinyexr BSD-3-Clause, QOI/tinydds MIT; OpenCL disabled |
| meshoptimizer | MIT; existing notice retained | Existing offline dependency unchanged |
| Recast/Detour | zlib; retain notice, mark altered source | Need audit only |
| Jolt | MIT; retain notice | Need audit only |
| Dear ImGui / ImGuizmo | MIT; retain respective notices on future use | J-X note, no vendoring |

Primary license locations: the pinned upstream trees linked above,
[KTX license breakdown](https://github.com/KhronosGroup/KTX-Software/blob/main/LICENSE.md),
Basis `LICENSE` and `.reuse/dep5`. Tool binaries are local proof artifacts, not a
redistribution package. Any future binary distribution must package notices for
its actual linked closure; no license conclusion overrides component terms.

## 42. D3D11

Production Windows x64/Diligent D3D11 remains. BC1/3/5/7 CPU outputs were produced;
candidate GPU upload/view/sRGB behavior was not integrated or accepted. Existing
renderer/asset gates and native smokes are reported below.

## 43. Vulkan

CPU animation/cache/mesh libraries do not require D3D11. KTX2 carries formats
usable with Vulkan subject to device capabilities. No Vulkan client execution
or candidate GPU upload was performed; API compatibility is not a runtime PASS.
DiligentFX-first architecture and existing Vulkan preparation remain intact.

## 44. Android

No configured Android SDK/NDK/JDK/Gradle/adb/device was available for this audit.
ARM-capable upstream algorithms and CPU ASTC/ETC2 transcoding are readiness
evidence only. Android build, APK, device memory and first Present are **NOT RUN**.
No desktop-only dependency was added to the production runtime.

## 45. Accepted Libraries

**No new production library.** Existing meshoptimizer, Diligent/DiligentFX and
current core dependencies remain. ozz/ACL/Basis sources are isolated proof inputs.
No candidate met both benefit and correctness/integration criteria for adoption.

## 46. Rejected Libraries

Current replacement proposals rejected: ozz runtime replacement, EnTT cache,
Jolt and blanket desktop Basis conversion. Reasons are concrete (real shear and
interpolation/precision differences, redundant ownership/cache rules, no physics
need, quality/size/preparation tradeoffs), not opposition to external libraries.

## 47. Deferred Libraries

ACL conversion/cache work, KTX2/Basis mobile packaging, extra meshoptimizer
options, EnTT ECS/editor queries, Recast navigation tooling and J-X editor helpers.
Each requires a consumer or missing validation before another integration step.

## 48. Exact Reasons

1. **Correctness:** boss full-shear loss remains at 240 Hz; other resampling and
   interpolation errors preclude claiming animation parity.
2. **Texture tradeoff:** ETC1S pack wins are measured but alpha/image changes are
   unaccepted; UASTC usually grows existing DDS packs and adds preparation.
3. **Architecture:** caches/jobs already implement essential domain behavior.
4. **Adoption conditions:** an actual Metin2 production animated GLB, candidate
   GPU visual acceptance and Android execution remain outside the available
   evidence. The closure supplies the requested production-near PBR/normal data
   for an offline format comparison.
5. **Proof robustness, closed by C-LIB-X-C:** two original native animation
   runs failed with access violation (-1073741819). The closure reproduced a
   native ACL encoder AV and identified invalid parent/child ordering in the
   harness adapter for both historical next-clip inputs. The corrected adapter
   is isolated from production. Original partial runs and all original GLB ACL
   values are superseded, not silently reused. Exact reproduction limits and
   the source-level failure chain are documented in the closure section.

## 49. Production Changes

**None.** `src/`, shaders, client assets, packs, UI and configuration are unchanged.
Root CMake adds only the default-OFF audit entry. Tools, corpus manifests,
documentation and measurement evidence are the reviewable source changes.

## 50. Tests

Validation appendix below records final fresh results. Gates are bounded to
Graphics/AssetRuntime/AnimationRuntime/Vegetation/AssetTool/Platform plus selected
renderer production contracts. No vendor stress/fuzz suite or milestone expansion.
Benchmarks use real data where available and explicitly label supplemental data.
PASS for a harness process means it completed and emitted evidence; it does not
mean candidate quality passed or a library was approved.

## 51. Release

Fresh build and selected 62-test gate: see validation appendix. Classic comparison
uses the committed G-DX-C baseline's 12 capture hashes and fresh test outputs.

## 52. Debug

Fresh build, the same selected 62 tests, Classic comparison and available D3D
diagnostic output: see validation appendix. Build warnings are not conflated
with runtime ERROR/FATAL diagnostics.

## 53. GCC/LP64

Existing Cygwin GCC 12/Ninja common build, selected 33 portable tests: see appendix.
This validates unchanged portable core/default-off build behavior, **not** a
compiled ARM/Android port or Linux build of the entire new texture proof.

## 54. Runtime Smoke

Native GR2 A1 -> B1 -> A1 and animated GLB fixture smokes use private generated
runtime directories and fresh Release executables, with Modern AO/shadows.
They validate the retained renderer/runtime, not ozz/ACL/KTX rendering. The
GLB fixture remains a fixture. New manual login/relog/window acceptance is
**NOT RUN**; accepted G-DX-C manual evidence remains historical baseline.

## 55. Resource Lifetime

Proof native animation completion requires zero live skeletons/clips/documents.
Production smoke shutdown checks include asset documents, animation instances,
mesh bindings, skeletons, clips, vegetation assets/instances, CPU deformation,
GPU fallbacks and preparation failures. Final values are in the appendix.

## 56. Git Diff

No commit, push, reset, branch rewrite or asset edit. Source HEAD remains
`d92dc48`; client HEAD remains `72d92593`. Final exact status and hashes are in
the appendix. Generated dependencies, encodes, images and logs are build-ignored.

## 57. GO/NO-GO

The initial audit withheld GO while the two proof crashes and production-near
PBR/normal corpus were open. That historical status is superseded by the explicit
**C-LIB-X-C Closure** decision below. Candidate adoption remains unapproved;
the milestone decision concerns completing the audit while retaining the core.

## 58. Recommendation G5/6-X

Do not start G5/6 automatically. Retain the secured G-DX-C architecture and this
audit's no-adoption outcome. A future ACL/KTX task should first provide the missing
production corpus and explicit quality budgets/target devices. G5/6, G7, H2 and UI
remain separate user decisions. **STOP after this report; no commit or push.**

## Original C-LIB-X validation appendix (not rerun for closure)

Retained [validation evidence](clibx-validation-evidence.json) includes executable
hashes, input/changed-tool hashes, counters, log hashes, git status and the original
failed environment attempts. Log paths below are relative to `build/clibx/`.

| Check | Final result | Evidence / limit |
|---|---|---|
| Release build | PASS, 74.806 s | `release-build.log`; incremental rebuild |
| Release gate | **62/62 PASS**, 102.47 s test time | `release-tests.log`, exit 0 |
| Release Classic | **12/12 byte-identical** | `classic-Release.json` against G-DX-C hashes |
| Debug build | PASS, 30.787 s | `debug-build.log`; incremental rebuild |
| Debug gate | **62/62 PASS**, 181.79 s test time | `debug-tests.log`, exit 0 |
| Debug renderer diagnostics | **0 ERROR/FATAL** | Explicit Diligent diagnostic result in verbose gate output |
| Debug Classic | **12/12 byte-identical** | `classic-Debug.json` |
| GCC/LP64 build | PASS, 27.101 s retry | Initial sandbox object-file write denied; `portable-build-retry.log` passes |
| GCC/LP64 tests | **33/33 covered and passed across runs** | Initial run: 31 pass, WriteRestart fails, dependent ReadRestart not run; targeted writable-environment retry **2/2 PASS** |
| Final animation measurement | Historical PASS process, 15 clips / 45 warm cases, 25.159 s | `animation-measured.jsonl`; GLB ACL values superseded by closure |
| Dense animation quality at 240 Hz | Historical PASS process, 15 clips | `animation-quality240.jsonl`; GLB ACL values superseded by closure |
| Animation diagnostic runs | Historical ASan 15-clip quality PASS; native diagnostic quality/full PASS | Earlier passes did not establish cause; closure now identifies the harness adapter error |
| Mesh proof | 21/21 codec roundtrips, exit 0 | `mesh.jsonl`; geometric/stream measurements only |
| Texture proof | 16 KTX2 encodes, 30 relevant quality records | `textures/`; 6 real plus 2 supplemental inputs, 2 codecs each |
| Native GR2 Modern smoke | **PASS**, exit 0, 63.3 s | `build/f2x/runtime/clibx-gr2`; A1 -> B1 -> A1 |
| Native animated GLB Modern smoke | **PASS**, exit 0, 17.867 s | `build/f5x/runtime/clibx-glb`; 20 actors, 9 stages/screenshots, 894 fixture frames |
| Fresh manual login/relog/window acceptance | **NOT RUN** | Secured G-DX-C manual acceptance remains historical |
| Candidate in-game visual/Android/Vulkan acceptance | **NOT RUN** | No candidate adopted; closure adds only offline production-near PBR/normal evidence |
| Diff whitespace check | PASS | `git diff --check`; Git emits only the existing CRLF conversion notice |

The portable `WriteRestart` case writes `graphics-restart.cfg` in its test
directory. It passes with the same source/binary when run with write access;
the dependent read then passes. This was not silently relabeled a first-run
33/33 result. No additional test suite was run after the targeted retry.

Builds are **not warning-free**: MSVC logs contain LNK4099, LNK4098 and LNK4075;
GCC reports `-Wstringop-overread` in the unchanged GLTF provider. Debug GR2 render
tests also print missing loose-texture diagnostics from their fixture environment.
Those are distinct from the zero Diligent ERROR/FATAL counter and from native
client smokes, whose `log/syserr.txt` files are both **0 bytes**.

### Fresh smoke shutdown

Both smokes finish with **SourceTextures=0, SourceBuffers=0, SkinMeshes=0,
BoneRemaps=0, BonePalettes=0, CollisionResources=0, AssetDocuments=0,
AnimationInstances=0, MeshBindings=0, RuntimeSkeletons=0, RuntimeAnimationClips=0,
VegetationAssets=0, VegetationInstances=0**. Additional vegetation/render counters
and all raw values are retained in validation JSON.

Both also report **AllCPUDeformationCalls=0, AllCPUDeformationVertices=0,
GPUFallbacks=0, SkinPreparationFailures=0, AnimationRuntimeFailures=0**.
GR2 smoke performed 933 native reads, 29,809 GPU frames and 329,278 vegetation
draws with **GrannyFileReads=0**. The GLB smoke recorded 17,880 GPU frames.
The frame counters have different scopes from the GLB fixture's 894 loop frames;
they are not equated. Both ran copies matching the fresh Release hash below.

### Binary and dependency comparison

| Executable | Before B | After B | Delta |
|---|---:|---:|---:|
| Metin2 Release | 30,716,928 | 30,716,928 | **0** |
| Metin2 Debug | 42,526,208 | 42,526,208 | **0** |

Fresh SHA256 Release:
`f7fcb1fcd8522465660b7d48c88c347c83e6193bc92f1b7363fcbb2cdf153563`.
Debug:
`2e137a2781cabd5019ad5f3b3519d696b16fa868055aff9f5c61e5827dbfa7f1`.
Hashes differ after relinking; this report claims equal **size**, not identical
executables. Before hashes remain in JSON. No new DLL or audit library was
introduced in the client link project; default-off production caches were checked.

### Final repository state

Client `main` / `72d92593` is clean and remains aligned with its locally recorded
`origin/main`. Redthief SHA256 remains
`2e0f34887edff2f4cfddbb97ae7d9f1a15236752f1330ddaf4f998f4479c51b4`.
No client assets, configuration or packs were modified by this audit.

Source `feature/gdx-diligentfx` / `d92dc48`:

```text
## feature/gdx-diligentfx...origin/feature/gdx-diligentfx
 M CMakeLists.txt
?? docs/core/
?? tools/CoreLibraryAudit/
```

Original audit: four CMake lines plus 10 tool/manifest/instruction files and three
documentation/evidence files; closure adds targeted tools/evidence below. Nothing staged. No changes under production
`src/`, `extern/` or existing `tests/`; no commit or push. Generated dependencies,
test images, private smoke directories and logs remain ignored build artifacts.

## C-LIB-X-C Closure

2026-09-15. **C-LIB-X = GO:** the audit is robustly closed; the existing core
remains. This is an audit-completion decision. No candidate is approved for a
production switch, and no production code, assets, settings or dependencies
were changed. Closure evidence is retained in
[clibx-closure-evidence.json](clibx-closure-evidence.json); commands are in the
[tool instructions](../../tools/CoreLibraryAudit/README.md#c-lib-x-c-targeted-closure).
Raw closure paths below are relative to `build/clibx/closure/`.

### Crash 1 and crash 2

| Historical failure | Reproduction and cause | Classification |
|---|---|---|
| #1: `animation-quality60.jsonl`, last completed clip `mount_run`, next `glb_fixture_0`; exit -1073741819 | Original whole quality invocation passed on closure retry. The isolated historical mount/GLB0 profile passed 5/5. The old adapter nevertheless fails a deterministic full-pose metric-order check on GLB0: `bone=0 parent=20`. | Incorrect ACL conversion in the isolated harness; parent object transform read before initialization. |
| #2: `animation-final.jsonl`, last completed GLB1 warm/100 actors, next `glb_fixture_2`; same exit | Original whole full-mode invocation passed on closure retry. The isolated GLB1/GLB2 profile passed 5/5. The same deterministic check fails on GLB2: `bone=0 parent=20`. | Same adapter defect; not a second production or ozz failure. |

The narrowed sequence mount -> GLB0 -> GLB1 -> GLB2 reproduced a **native access
violation on attempt 2** (attempt 1 passed). Its last completed row was GLB0;
the captured stack identifies the next ACL compression:

```text
0xc0000005
acl::acl_impl::find_contributing_error  quantize.transform.h:1797
acl::acl_impl::compress_transform_track_list  compress.transform.impl.h:259
Audit  AnimationAudit.cpp:184   [pre-fix line number]
```

Evidence: `transition-runs.json`, `transition-2-errors.log`,
`order-before-0-errors.log`, `order-before-2-errors.log`, and
`historical-0-runs.json` / `historical-2-runs.json`.
**Reproduction limit:** the two original processes retained no crash dump or
stack; their exact individual AV schedules were not deterministically replayed.
The closure establishes a naturally reproduced encoder AV of the same class
and the identical invalid hierarchy invariant in both historical next inputs.
It does not claim to have recovered their missing call stacks.

#### Failure chain and production boundary

The native GLTF skeleton legitimately preserves skin-joint IDs and appends
non-joint ancestors. `GlTFAssetProvider.cpp:739` initializes it as
`IndexedForest`; bone 0 therefore has parent 20. Production
`src/AnimationRuntime/AnimationRuntime.cpp:344` evaluates
`skeleton.EvaluationOrder()`, so storage order is not evaluation order.

The old audit copied bone IDs directly to ACL input-track order. ACL 2.1.0's
default QVV metric walks the supplied dirty indices and reads the parent's
object transform before storing the child's result. Quantization initializes
the full dirty list to ascending input indices. In this adapter's order it
therefore reads unwritten parent data. Invalid error values can leave
`best_error.keyframe_index` at its invalid sentinel; the Release path writes
`contributing_error[best_error.keyframe_index]` at the captured crash line.
The source evidence is in the pinned
[ACL metric](https://github.com/nfrechette/acl/blob/v2.1.0/includes/acl/compression/transform_error_metrics.h)
and [quantization implementation](https://github.com/nfrechette/acl/blob/v2.1.0/includes/acl/compression/impl/quantize.transform.h).

This is a harness ACL-adapter ordering defect. The GLB input is valid; no
production-runtime defect was found. No ownership/lifetime repair, ozz change,
bind/rest transform alteration, root-motion change, mirrored-transform change,
scale projection change or exception suppression was used to resolve it.
Real full-shear loss remains a separate conversion-quality issue. A prior ASan
PASS did not exclude this use of unwritten metric data.

The correction changes **only ACL encoder input order** to the existing native
evaluation order, remaps input parent IDs through its inverse, and keeps original
native output IDs through ACL `output_index`. Bijection and parent-before-child
checks enforce the adapter contract. A diagnostic metric checks actual full-pose
calls, and optional allocation filling tests independence from prior memory
contents. Native exceptions still terminate with a captured stack; contract
violations fail explicitly. Compression settings remain medium/default, precision
0.001 and shell distance 100. No upstream library or production source was edited.

### ACL error decomposition and ozz consequence

A/current, B/uncompressed uniform bake and C/ACL now use exactly the same sample
times: dense grid and all translation, rotation and scale keys, including both
sides of each boundary. The original boundary loop omitted B/C and only covered
translation keys. All earlier GLB ACL bytes, errors and timing rows are invalidated
in favor of this evidence. GR2 controls have identity encoder order and unchanged
compressed payload sizes; their earlier size/decode conclusions remain applicable.

Maximum world-position deviations below are in client units. Each column is a
separate maximum over the shared times; maxima need not add algebraically.

| GLB clip | ACL bytes, 60 Hz | A -> B, 60 Hz | B -> C, 60 Hz | A -> C, 60 Hz | A -> C, 240 Hz |
|---|---:|---:|---:|---:|---:|
| 0 | 1,090 | 0.000031 | 0.000847 | 0.000847 | 0.000894 |
| 1 | 2,029 | 1.026553 | 0.000636 | 1.026677 | 0.259377 |
| 2 | 1,756 | 4.091578 | 0.001661 | 4.091700 | 0.000733 |
| 3 | 1,183 | 4.010962 | 0.006304 | 4.010986 | 1.018046 |
| 4 | 652 | 0.872794 | 0.000320 | 0.872719 | 0.167392 |
| 5 | 1,086 | 0.636291 | 0.000546 | 0.636175 | 0.000731 |

The remaining significant GLB deviations arise predominantly in B, the
fixed-rate bake. Raising the rate helps but does not establish general parity.
On the real boss control, A -> B = **5.26384**, B -> C = **0.05479**,
A -> C = **5.26386**; source off-diagonal shear is **0.0530004**. Player attack
has A -> B = **6.75794**, B -> C = **0.000862**. Those are conversion limits,
not evidence that ACL inherently requires those large errors. **ACL stays LATER.**

ozz uses its own hierarchy traversal and was not the crashing path. It was
checked alongside the affected poses without a new broad ozz benchmark.
GLB clip 3 still reaches approximately **1.482** world-position error; the
real boss reaches **5.24135** with lost full shear. Its TRS representation and
the current proof's precision/interpolation still do not preserve required
semantics. **ozz stays NO-GO.** Neither candidate changes the native runtime.

### Production-near GLB and real normalmap

The added data case is **Microsoft FlightHelmet**, an authored static PBR model
published by Khronos under CC0. The official KTX2 variant already includes
ETC1S color / UASTC non-color textures with stored mip chains.
Sources: [model and provenance](https://github.com/KhronosGroup/glTF-Sample-Models/blob/d7a3cc8e51d7c573771ae77a57f16b0662a905c6/2.0/FlightHelmet/README.md),
[pinned KTX2 input](https://github.com/KhronosGroup/glTF-Sample-Models/tree/d7a3cc8e51d7c573771ae77a57f16b0662a905c6/2.0/FlightHelmet/glTF-KTX-BasisU).

The original geometry and all 15 original KTX2 images were packed into
`textures/FlightHelmet.closure.glb`, then read back and verified byte for byte.
All materials, texture references, samplers, scene nodes, geometry buffer views
and accessors remain identical. The model has **55,392 vertices, 94,722 triangles,
six meshes/primitives, six materials** and no animation. The GLB is 34,033,276
bytes, SHA256 `0314db84ace85cce82d5191eb79e19b732af0dcc0bcfa403bd951f13afdb4ff0`.

This satisfies the requested **production-near PBR texture case**. It is neither
a Metin2 production asset nor an animated production character. No native KTX2
GLB loading/rendering support is claimed or added. Animated production GLB
acceptance remains a condition of any future animation adoption, not a claim
made by this static texture comparison.

Only four additional texture cases were measured:

| Material texture | Original dimensions | Existing mip levels | Interpretation |
|---|---:|---:|---|
| LeatherParts BaseColor | 2048 x 2048 | 12 | sRGB RGB, opaque material |
| LeatherParts Normal | 2048 x 2048 | 12 | Linear signed RGB normal after [0,1] -> [-1,1] mapping |
| LeatherParts OcclusionRoughMetal | 2048 x 2048 | 12 | Linear R occlusion / G roughness / B metallic |
| Lenses BaseColor | 1024 x 1024 | 11 | sRGB RGB, linear alpha, original `alphaMode=BLEND` |

**Controlled baseline:** these external textures were supplied as KTX2, not DDS.
Each existing mip was decoded/transcoded to a derived BC7 DDS baseline, with
sRGB DXGI 99 for color and UNORM DXGI 98 for normal/data. Both ETC1S and UASTC
were then encoded from the **same decoded DDS mip bytes**, recorded by hash.
No mip generation, resizing, material changes, orientation changes or additional
sharpening were requested. The relative sizes are therefore an external-format
comparison, not a measured saving in an existing Metin2 pack.

The normalmap's top mip contains **99 of 4,194,304 texels with negative Z**.
An initial positive-Z BC5 baseline check rejected this assumption explicitly.
The completed comparison retains RGB in linear BC7 and applies no R/alpha
swizzle or forced positive-Z reconstruction. Normal vectors are interpreted
linearly and normalized only for angular measurement. The original KTX2 -> DDS
reference transcode has top-mip mean/P95/max angular error **0.143/0.469/2.364
degrees**, recorded separately; it is not silently counted as codec error.

KTX2 metadata is checked on every output: transfer 1 for normal/data, transfer 2
for basecolor. All stored dimensions/mip counts match. Color/alpha byte-space QA
does not apply an sRGB curve to normals, ORM or alpha. The semantics follow
[KHR_texture_basisu](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_texture_basisu/README.md).
No new GPU sampling, Vulkan or Android device acceptance was performed.

### Texture results and KTX2/Basis consequence

Both alternatives use Zstd level 17, as the existing pack does. Sizes are bytes.
Preparation is median CPU parse/start/transcode with preallocated output; it
excludes candidate outer-pack decode and GPU upload. DDS comparison times below
measure only its pack decode, not an equivalent full renderer image load.

| Texture | DDS + Zstd | ETC1S + Zstd | UASTC + Zstd | ETC1S preparation ms | UASTC preparation ms |
|---|---:|---:|---:|---:|---:|
| Leather basecolor | 814,210 | 511,564 | 1,246,372 | 21.506 | 32.996 |
| Leather normal | 3,605,980 | 740,541 | 4,072,908 | 79.008 | 35.679 |
| Leather ORM | 3,779,831 | 836,290 | 4,381,856 | 79.368 | 38.422 |
| Lenses alpha | 266,316 | 207,426 | 404,152 | 7.239 | 7.622 |

DDS pack decode medians range **0.633-4.863 ms**. Target GPU block bytes are
equal for each baseline/candidate pair: **5,592,432** per leather texture and
**1,398,128** for lenses. No VRAM saving is demonstrated.

All 47 stored source levels were compared for both codecs (**94 mip comparisons**).
Representative top-mip quality and the most relevant lower-mip limits:

| Check | ETC1S | UASTC |
|---|---:|---:|
| Leather normal angle mean / P95 / max, degrees | 4.971 / 14.769 / 118.868 | 0.096 / 0.611 / 15.600 |
| Highest normal mean / P95 over any stored mip, degrees | 4.971 / 14.769 | 0.182 / 0.920 |
| Leather basecolor RGB PSNR, dB | 52.881 | 60.455 |
| ORM roughness / metallic RMSE, 8-bit units | 7.570 / 3.065 | 0.692 / 0.173 |
| Highest roughness RMSE over any stored mip | 32.402 | 1.068 |
| Lenses alpha RMSE / maximum absolute difference, 8-bit units | 1.061 / 34 | 0.720 / 22 |
| Highest lenses alpha RMSE over any stored mip | 6.139 | 1.750 |

The contact sheet was inspected: material layout/orientation is retained;
ETC1S loses fine normal/ORM detail. Numerical checks retain local outliers that
the reduced previews do not expose. Top-mip alpha coverage at threshold 128
changes by -0.0643 percentage points for ETC1S and -0.0126 for UASTC; this is a
diagnostic only because the actual lens material uses blending, not alpha test.
No in-game lighting or mip-transition quality PASS is claimed.

**KTX2/Basis stays LATER.** ETC1S reduces this derived baseline's compressed
bytes by about 22-79%, with significant normal/ORM/alpha changes. UASTC preserves
more quality but grows all four Zstd-packed references by about 13-53%, adds CPU
preparation and does not reduce BC7 block bytes. These data strengthen the
existing desktop deferral. Mobile/platform packaging remains a later consumer.

### Targeted verification and final repository state

| Closure check | Result |
|---|---|
| Native reproduction | AV captured on narrowed transition attempt 2; both historical inputs fail the old-order metric precondition; exact historical AV schedules not reliably repeatable |
| Corrected animation audit | **7/7 runs PASS**, 35 clip checks, including 6 GLB at 60 Hz, 6 at 240 Hz, 4 real GR2 controls, 3 x 6 GLB with allocation filling, and 1 GLB warm case |
| ACL adapter determinism | All six GLB payload/error records identical across three allocation-pattern repeats; 21 reordered tracks each; GR2 controls need 0 reorderings |
| Warm actor checks | **1/20/100 actors PASS**, instrumented warm allocations 0; native skeleton/clip/document lifetime checks pass at exit |
| GLB container | All geometry and 15 embedded original KTX2 images byte-identical; material/scene definitions unchanged |
| Texture process/semantic checks | **8/8 comparisons PASS**, 94 stored-mip comparisons, same input hashes, dimensions, transfer metadata and GPU block bytes |
| Image inspection | Contact sheet inspected; numerical quality deviations retained; no candidate visual-acceptance PASS |
| Production verification | No production changes; original Release/Debug executable hashes unchanged; client and repaired Redthief asset unchanged |
| Full Release/Debug/GCC/Classic gates and native smokes | **NOT RERUN in closure**; original audit results retained above; production was untouched |
| Git whitespace/scope | PASS; only CMake default-off audit entry, audit tools and docs remain changed; nothing staged |

Only isolated `CoreAnimationAudit` and `CoreTextureAudit` targets were rebuilt.
EnTT, meshoptimizer and Jolt were not rebenchmarked. Generated downloads,
encodes, GLB, images, binaries and logs remain ignored under `build/clibx`.
The helper's temporary Python bytecode was removed and further local helper
bytecode generation disabled.

Client remains clean on `main` / `72d92593`. Source remains
`feature/gdx-diligentfx` / `d92dc48`:

```text
## feature/gdx-diligentfx...origin/feature/gdx-diligentfx
 M CMakeLists.txt
?? docs/core/
?? tools/CoreLibraryAudit/
```

Final decisions: **ACL LATER; KTX2/Basis LATER; ozz NO-GO; EnTT cache NO-GO;
Jolt NO-GO; meshoptimizer KEEP.** No remaining technical blocker to completing
this no-adoption audit was found. The remaining candidate quality/integration
conditions are the reason to retain the core.

**C-LIB-X = GO. STOP. No commit, push, G5/6, G7 or H2.**
