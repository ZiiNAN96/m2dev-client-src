# BUGFIX-X - Stable World / Vegetation LOD & Residency

Status: **Follow-up FAST GATE PASS (17 September); user visual acceptance pending.** Testclient only. The diagnosis below was recorded before functional changes. The follow-up section supersedes the initial direct tree-LOD switch.
Source baseline: `a020541`, `codex/g56-hdr-atmosphere`, clean working tree.
Production runtime remains unchanged; no commit or push.

## Root cause (source inspection)

- Tree transparency: H2 uses complementary dither for V2 LODs, not whole-tree material opacity. However, `MapOutdoorRender.cpp` hides camera blockers from the opaque pass, and `WorldTree.cpp::Context(true)` submits their entire tree with alpha blending and the camera mask. Moving across the blocker classification changes opaque trees into transparent trees. V1 alpha-cutout sample interpolation and V2 LOD selection are stateless; distance culling has no hysteresis.
- Buffer churn: `VegetationRenderer.cpp::DrawBatchImpl` resets every empty group buffer. `DiligentStaticObjectRenderer::UpdateInstances` allocates exactly the current required bytes, despite documentation claiming spare capacity. Visibility/LOD changes therefore destroy/recreate buffers and small increases repeatedly allocate.
- World popping/residency: only a player-centered 3x3 neighborhood is loaded. `UpdateAreaList` removes sectors from the lookup vectors before deferred deletion; `isTerrainLoaded`/`isAreaLoaded` cannot recover them. Returning can load duplicate placements and geometry; the next cleanup destroys queued sectors. Static rendering also uses the changing 3x3 list rather than visibility over resident sectors.
- Terrain: `UpdateTerrain` reassigns the patch window at each patch-cell crossing, centered on the player, while LOD/frustum use the camera. The moving window can exclude terrain still within the camera frustum. HTP/STP loops stop all subsequent patches when the splat budget is reached. LOD selection has no persistent patch state. Existing LOD1/2 indices already preserve full-resolution perimeter stitching; no new terrain engine or geomorph infrastructure is needed.
- A1 settings: 4 x 5 sectors, ViewRadius 128 cells, cell scale 200. Whole-map residency is practical for this map. Larger maps need bounded preload/unload margins instead of eager allocation of the format's theoretical 256 x 256 maximum.
- 3D grass preparation/rendering remains disabled by the baseline; do not reactivate it.

## Validation plan

Instrument the unchanged behavior first, capture a short repeatable A1 route, then run the same route on the fix. Use the existing diagnostics switch and native test harness. Build affected Release targets, run focused vegetation/terrain contracts and GPU checks, a 150-second A1 movement/camera smoke, and verify clean shutdown. Final visual approval belongs to the user; leave the isolated testclient open.

## Initial fix

- **Tree LOD:** persistent per-instance selection with separate outward/inward thresholds, including the impostor boundary. V2 uses the existing transition fraction as the hysteresis band; converted V1 uses +/-6%. Frustum culling retains history; distance culling re-enters at 94% of its outer boundary. Classic sample selection is unchanged.
- **Tree transition:** Modern trees/bushes switch between complete representations. Converted V1 samples select one leaf representation and either tree geometry or billboard, preserving the minimum authored cutout threshold for each part. No whole-tree blend or simultaneous tree/impostor draw. Trunks use an opaque depth-writing pass. Modern trees are excluded from transparent camera-blocker collection; rendering also enforces the opaque/cutout state defensively. Grass remains disabled.
- **Sector residency:** Modern maps up to 8x8 sectors are prepared once and retained until map destruction. A1 has 20 terrain and 20 static sectors. The original 3x3 neighborhood remains the gameplay/height query lookup; render proxies and static rendering use resident owners independently. Large maps use camera-centered visible/preload/unload radii of 2/3/5 sectors; unload also preserves the player's gameplay neighborhood. This covers the existing maximum 384 m view without increasing the view-distance setting.
- **Terrain LOD:** fixed proxies are assigned once for a small map, with LOD history stored on each actual terrain patch. +/-8% LOD hysteresis, adjacent delta <=1, original stitched perimeter indices retained. Modern HTP and STP render every visible patch; the global splat budget no longer drops all remaining geometry. Texture layers, terrain data and existing LOD meshes are reused.
- **Rebuild prevention:** empty vegetation batches retain their shared buffers; growth reserves capacity instead of reallocating at every small increase. World preparation is not repeated on movement. First-use LOD buffers/shared override resources can still be prepared lazily; this is measured separately from placement or sector reconstruction.
- **Deferred graphics settings:** a map initially loaded before the renderer receives Modern settings is promoted once at that frame-boundary transition. The first instrumented fix run exposed this integration gap; it is excluded from final gate evidence. Live return to Classic uses the existing Classic draw/LOD behavior over the retained map owners.
- **Telemetry:** new sector/buffer/terrain/camera-blocker counters update only with the existing verbose renderer diagnostics switch. The existing diagnostics-only Python probe samples once per second. Existing tree LOD totals include impostor changes; a separate impostor-only switch total is not recorded.

## Initial measurement

Equal A1 route, 1024x768, High/Modern. World deltas are between samples at seconds 5 and 44. CPU/GPU values use 600 warm frames after skipping 180 `sample=1` frames; no outliers removed. Two pre-existing clients remained untouched, so these are short sanity values, not an isolated benchmark. The initial mountain diagnostic and the run before the deferred-settings correction are retained but excluded from this comparison.

| Counter | Before | Final fix |
| --- | ---: | ---: |
| Terrain sectors loaded / unloaded during equal warm window | 0 / 0 | 0 / 0 |
| Static sectors loaded / unloaded during equal warm window | 0 / 0 | 0 / 0 |
| Resident terrain / static sectors | 9 / 9 | 20 / 20 |
| Terrain proxy assignments during movement | 6 | 0 |
| Tree placement creation during movement | 0 | 0 |
| GPU instance buffer allocations | 19,997 | 160 |
| Transparent tree camera-blocker draws | 13,449 | 0 |
| Tree LOD switches, including impostors | 9,553 | 252 |
| CPU frame mean / median / P95 ms | 1.187 / 1.121 / 1.536 | 1.405 / 1.191 / 1.681 |
| GPU frame mean / median / P95 ms | 0.591 / 0.533 / 1.153 | 0.602 / 0.535 / 1.191 |
| Sampled terrain draw calls per frame, mean | 180 | 180 |

The warm baseline route did **not** reproduce sector unload/reload churn; the old deletion/lookup fault is source evidence, not a fabricated measured delta. The final 150-second route keeps 20/20 sectors and all 368 placed trees unchanged, including a move to the A1 town without map reload. Its final 30 seconds have **0 buffer allocations and 0 terrain assignments**. The final shared instance-buffer capacity is 76,800 bytes before shutdown. Runtime instances include lazily created Modern override render states and settle at 388; they are not additional map placements. Visible terrain count equals drawn terrain count in every post-load sample.

CPU mean is 0.219 ms higher and median 0.070 ms higher in this short window. No CPU improvement is claimed. The measured reduction concerns buffer/LOD churn; full small-map residency retains more sectors. Whole-frame draw calls were not available in the existing skinning CSV; its `draws` column is actor-specific and is not presented as whole-frame draws.

## Initial FAST GATE

- Affected Windows x64 Release build: **PASS**, final exact source tree. Existing dependency/compiler/PDB warnings remain; no warning-free claim.
- Focused Release checks: **7/7 PASS** in 13.37 s: vegetation contracts, all 118 converted types, Modern GPU coverage, fixture generation, terrain texture CPU and production terrain GPU integration. No full suite.
- Portable core changed, therefore only the affected GCC contracts were rebuilt/run: **3/3 PASS** in 0.34 s. No general GCC suite.
- Final native A1 smoke: **150 seconds PASS**, repeated sector/LOD boundary motion and camera rotation, then town/buildings/trees on the same map. Assertions require stable residency/placement counters, no tree camera blending, no grass, and complete visible terrain coverage. Scripted native movement; not an authenticated network-login or input/collision acceptance test.
- Diligent ERROR/FATAL: **0 / 0**. AllCPUDeformationCalls/Vertices: **0 / 0**. GPUFallbacks: **0**.
- Shutdown: **Exit 0**, all 30 checked source-resource/failure counters zero, all eight renderer shutdown resource groups zero, terrain/static residency zero, syserr empty.
- Production repository remains clean; production Release/Debug executables and root package hashes are unchanged. Testclient uses the verified Release binary and the original production packages with separate writable config/log directories. No content migration, no grass activation, no commit, no push.

Evidence: `build-bugfix-x/evidence/result.json`, `tests-release.log`, `tests-portable.log`, `build-gate.log`, and `final-binary.json`. Native captures/logs: `build-bugfix-x/runtime-final/`. The evaluator is `tests/Vegetation/summarize_bugfix.py`.

Manual testclient: `build-bugfix-x/runtime-manual/Metin2_Release.exe`. **STOP: await user visual acceptance before any production deployment.**

## Follow-up: visible LOD popping (17 September)

The user's 30 fps video shows abrupt crown/bark changes at frames 24 -> 25,
97 -> 98 and 140 -> 141. The first fix deliberately removed transitions;
hysteresis reduces boundary chatter but cannot hide mismatched representations.
`BuildStableLods` reduces the converted sample table to discrete mesh tuples and
`DrawBatchImpl` submits them with zero transition coverage. Legacy single-view
atlas silhouettes/material lighting also differ from the actual 3D tree.
The earlier FAST GATE did not prove perceptual continuity.

Follow-up plan, before changes: retain the existing coarsest 3D representation
for converted V1 trees in Modern (unchanged culling/view distance and Classic),
and use a bounded, reversible complementary cutout transition for changed parts.
Shared parts stay solid and are submitted once. No asset migration, no grass,
no whole-tree alpha blending. Measure the extra far geometry and inspect fixed
A1 views and boundary motion; keep production unchanged.

### Implemented follow-up

- Modern converted V1 trees retain the final complete, existing 3D mesh tuple.
  Their old single-view billboard is no longer selected. Original assets, Classic
  sample selection, cull distance and view-distance settings are unchanged. V2
  authored impostors remain available.
- A per-instance 0.3-second complementary dither handover replaces direct mesh
  swaps. Existing hysteresis still selects the target. Reversing direction keeps
  the exact same pair and pixel mask; a third target waits for the current
  handover. Identical parts draw once at full coverage. Trees never enable alpha
  blending. The transition clock is independent of diagnostic frozen wind and
  advances once for a timestamp shared by color/shadow collection.
- No new asset pipeline, content migration, sector preparation or grass work.
  Both representations share uploaded meshes; retained instance buffers include
  the temporary transition groups.

### Follow-up evidence and limits

Production `root.pck` was decoded and compared with all **342/342** source files.
Both scripted test packs differ from production in **prototype.py only**. The
manual runtime uses the original production packages directly, with no loose
vegetation overlay. All three runtimes have separate writable config/logs.

The same first 45 seconds approach three actual A1 town trees (converted beech,
converted cypress and authored beech). Final execution continues with reverse
motion and camera rotation, then a stationary final 30 seconds. Fixed views show
the converted 3D crowns/bark already present in the distance, eliminating the old
dark billboard-to-geometry substitution. Remaining detail changes use cutout
coverage; final perceptual acceptance remains with the user.

Comparison uses **all frames between the 5s and 44s samples**, including identical
half-second screenshot cadence. This is a short sanity check on a shared desktop,
not an isolated benchmark. Vegetation submission counts include rendering passes,
and are not whole-frame draw counts.

| Counter | Previous fix | Follow-up |
| --- | ---: | ---: |
| CPU frame mean / median ms | 3.168 / 1.646 | 3.256 / 1.666 |
| GPU frame mean / median ms | 1.847 / 0.520 | 1.888 / 0.525 |
| Vegetation draw submissions per frame | 54.72 | 67.58 |
| Vegetation triangles per frame | 9,866 | 15,986 |
| Tree target-LOD switches | 326 | 296 |
| Instance buffer allocations in warm window | 147 | 311 |
| Terrain/static loads, unloads, placement/proxy rebuilds | 0 | 0 |

Additional low-poly far geometry and transition groups increase drawing/allocation
cost; no performance reduction is claimed. Final stationary 30s: **0 new instance
buffer allocations**. All post-warmup samples retain **20 terrain / 20 static
sectors**, **368 placed trees**, complete visible terrain coverage, zero transparent
tree-blocker draws and zero grass placements.

- Affected Release build: **PASS**; existing dependency/PDB warnings remain.
- Focused Release gate: **6/6 PASS**, 1.96s (contracts, all 118 compiled types,
  legacy numeric goldens, Modern GPU and its asset-generation fixture).
- GPU pixel regressions: threshold crossing is pixel-identical to the previous
  representation; reversal preserves the exact midpoint image; return restores
  the initial image; a shared trunk submits once with two leaf representations.
- Changed portable core: **3/3 GCC PASS**, 0.40s; no general suite.
- Final native A1 route: **150s / 8,981 frames**, exit 0, empty syserr, zero
  Diligent ERROR/FATAL, CPU deformation and GPU fallback. All 30 checked source
  lifetime/failure counters and all eight renderer shutdown groups are zero.
- Production executable/package hashes preserved; no commit or push.

Evidence: `build-bugfix-x/lod-followup/evidence/result.json`, `tests-release.log`,
`tests-portable.log`, `gpu-transitions.jpg`, `a1-beech-comparison.jpg` and
`a1-cypress-comparison.jpg`. Route/evaluator: `prepare_lod_followup.py` and
`summarize_lod_followup.py` under `tests/Vegetation`.

Current manual testclient: **`build-bugfix-x/lod-followup/runtime-manual/Metin2_Release.exe`**.
Release SHA256: `4aee7ac2d1cfabf55c76c0bc1641ca4d6c701fa8dad663785aafbd8b5db79756`.
**STOP: await user visual acceptance; no production deployment.**

## 2026-09-17: fixed highest tree detail experiment

User requested a comparison without tree detail changes. The current experimental
Release uses the highest complete authored 3D representation for Modern trees at
every visible distance. No tree LOD handover or impostor is selected. Existing
distance/frustum culling, wind, opaque trunks, cutout foliage, terrain residency
and disabled grass are retained. Classic and explicitly classified bushes/grass
keep their existing paths. The internal `RenderContext::fixedTreeDetail` switch
retains the adaptive path for GPU regression coverage; no new user setting or
asset conversion was introduced. This source default is an experiment awaiting
visual acceptance, not a production deployment.

Focused validation against the final source:

- Affected Release build PASS; Release 6/6 in 2.36s, GCC/LP64 3/3 in 0.31s.
- All 118 compiled legacy types retain the highest 3D state across outward and
  inward distance sweeps, including recovery after culling.
- V2 GPU captures are pixel-identical across former LOD ranges with fixed camera
  and wind. One trunk and one crown draw, no transition, and distance culling
  still applies. Existing adaptive transition/reversal regressions also pass.
- Native original assets: 75s A1 plus 75s Trent, 8,988 frames, exit 0 and empty
  syserr. A1 retains all 20 sectors and 368 tree placements; Trent retains all
  four sectors and its 115 placements. No within-map reload/unload, tree placement
  rebuild, incomplete visible terrain, transparent tree draw or grass placement.
- All 30 checked source shutdown/failure counters and eight renderer shutdown
  groups are zero. Both final stationary intervals allocate zero instance buffers.

The reference is the preceding adaptive follow-up binary
`4aee7ac2d1cfabf55c76c0bc1641ca4d6c701fa8dad663785aafbd8b5db79756`.
Both runs use identical cameras and packed content, 1024x768 Modern High.
Measurements include every frame between the 5s and 44s samples on each map,
including the same three screenshots per map. These are short comparisons on a
shared desktop with other background processes, not an isolated benchmark or a
worst-case forest bound. Vegetation draw/triangle counts include all passes.

| Metric | A1 adaptive | A1 fixed high | Trent adaptive | Trent fixed high |
| --- | ---: | ---: | ---: | ---: |
| CPU frame mean ms | 2.268 | 2.137 | 1.399 | 1.160 |
| CPU frame median ms | 1.973 | 1.837 | 1.291 | 1.082 |
| GPU frame mean ms | 0.649 | 0.636 | 0.599 | 0.595 |
| GPU frame median ms | 0.516 | 0.521 | 0.445 | 0.445 |
| Vegetation submissions/frame | 67.59 | 51.17 | 75.07 | 52.94 |
| Vegetation triangles/frame | 15,991 | 33,832 | 20,847 | 37,318 |
| Warm instance-buffer allocations | 311 | 12 | 344 | 16 |
| Raw `treeLodSwitches` delta | 296 | 21 | 353 | 29 |

The raw switch counter also increments when a newly visible instance first
receives geometry. Its remaining fixed-high counts are initial assignments;
the fixed tree path never substitutes a lower representation. The CPU and GPU
regressions verify geometry stability directly. More triangles are offset here
by fewer submission groups and no transition work. GPU time is effectively
unchanged within this short comparison; no general speedup is claimed.

Evidence: `build-bugfix-x/fixed-detail/evidence/result.json`, Release/portable logs,
`source-files.json`, `gpu/`, and `comparison-a1.jpg` / `comparison-trent.jpg`.
Route and strict evaluator: `tests/Vegetation/prepare_fixed_tree_test.py` and
`tests/Vegetation/summarize_fixed_tree_test.py`. Native test root packs differ
from production only in `prototype.py` (342 entries, one difference); the manual
testclient uses the original packs directly. Production Git status and the two
production binaries/root pack hashes remain unchanged. No commit or push.

Current manual testclient:
**`build-bugfix-x/fixed-detail/runtime-manual/Metin2_Release.exe`**.
Exact Release SHA256:
`6ed6c0047cbd7dd6f4a8deca35b3a35c72b7040e4a4580bbabfc798f4afda1ce`.
Manual launch confirmed: responding `METIN2` window, PID 37124; see
`evidence/manual-start.json`. The ordinary login path logs the pre-existing
`invalid idx 0` warning, also present in the preceding manual testclient. This is
separate from the automated route's empty syserr; no new login fix is claimed.
**STOP: fixed-high experiment ready for user visual acceptance; no production deployment.**

## 2026-09-17: user acceptance and original-client deployment

The user accepted fixed-high tree visuals and explicitly requested deployment and
original-client cleanup. The exact accepted Release above and a fresh same-source
Debug were installed in the sibling `m2dev-client`; Debug's focused gate passed
6/6 in 5.57s. No implementation change followed the accepted Release gate.
Production assets, packages and graphics preferences remain unchanged. Thirteen
old diagnostic logs were archived after hash-verified backup; generated logs are
removed from version control. See the runtime repository's
`docs/stable-trees-deployment.md` and
`build-bugfix-x/fixed-detail/deployment-20260917/result.json` in this repository.
This supersedes the experiment's deployment stop above. No commit or push.
