# M11 – D3D9 rest-path audit and bounded integration plan

> Historischer Milestone-Stand. Aktuell seit [M12](renderer-milestone12.md): Diligent D3D11 ist im ON-Build der Default; Legacy wird explizit mit `--renderer=legacy-d3d9` gewählt. OFF-Builds bleiben Legacy-only. Frühere Testergebnisse und Auswahlbeschreibungen unten gelten für ihren damaligen Stand.

Baseline: `56a79e1`. Initial analysis/plan; implementation and acceptance evidence are recorded in `renderer-milestone11-validation.md`.
Legacy remains the default. No device removal or backend hot-switching.

## Reviewed call chains

| Path / category | Reachable chain and native consumer | Existing Diligent alternative | M11 action |
|---|---|---|---|
| B selection/creation | `introSelect.py` / `introCreate.py::CharacterRenderer.OnRender` → `grp.ClearDepthBuffer/SetOmniLight/SetViewport` → `chr.Deform/Render` → `CPythonCharacterManager` → `CActorInstance` → `CGrannyModelInstance::RenderMeshNodeListWithOneTexture` | M5 actor bridge, blocked by `actorWorldFrame`; separate viewport and native spot + point light; no offscreen RT | Explicit scoped Python actor entry, reuse skinning/material/attachments; snapshot viewport and existing lighting; depth clear on D3D11 surface |
| A guild land projection | guild areas → `CMapOutdoor::VisibleMarkedArea` → `CTerrain::AllocateMarkedSplats` → `RenderMarkedArea/RecurseRenderAttr/DrawPatchAttr` | None | Existing terrain patch geometry, generated mask, camera-space projection; pulsating white texture-factor alpha; SRCALPHA/INVSRCALPHA; preserve order and depth. Not guild logo or new decal engine |
| A dungeon geometry | property `DungeonBlockFile` → `CArea::__SetObjectInstance_SetDungeonBlock` → `CDungeonBlock::Load/Update/Render` → `CDungeonModelInstance::RenderDungeonBlock` → Granny two-texture rigid groups | None (M4 explicitly rejects PNT2/blend models) | Native PNT2 geometry and two-texture combiner, model-owned lifetime; collision and portal handling unchanged |
| A actor remainder | packet type/race → `CInstanceBase` → same `CActorInstance` and Granny parts | M5 excludes poly, high NPC/mob races, stone/door/building/horse/object categories | Remove artificial scope exclusions only where native geometry/material contract is valid; capture rigid body models too; no new pet/mount system |
| A minimap | `CPythonMiniMap::Render` → 9 native indexed textured quads plus camera-space alpha-cover texture → ordinary marker images | Whole function excluded by `UIExcludeScope` | Port native mask pass; allow ordinary markers through M9; preserve rotation/coordinates/clipping |
| A atlas | `CPythonMiniMap::RenderAtlas` → atlas image / normal mark images / guild flags | Whole function excluded by `UIExcludeScope` | Remove exclusion with native transforms retained. Neither map path generates a render target |
| C dynamic shadows | `CPythonApplication::RenderGame` → `PythonBackground::RenderCharacterShadowToTexture` → `MapOutdoorCharacterShadow` → actors `RenderToShadowMap`; area/terrain shadow receivers | No Diligent shadow generation | Option B: deliberately disabled in Diligent only; later separate shadow milestone. No shadow-engine redesign |
| C shadow helper | `CGraphicShadowTexture::Begin/End` (RT, depth, viewport) | None | Trace callers; retain as helper only if not a normal visible path |
| C snow blur | `CSnowEnvironment` optional RT blur versus native snow particle pass | M7 snow particles, not blur RT | Explicitly classify separately from visible snow particles |
| A screenshot | `CPythonGraphic::SaveScreenShot` → native backbuffer lock → existing JPEG writer | None | Read composed D3D11 surface, reuse existing output format; no screenshot of hidden legacy surface |
| G normal world | `RenderGame` → sky/lens/cloud/map/actors/water/snow/effects/items/flying/blockers | M2–M8 bridges | Audit unsupported states and parallel native calls before suppressing actual D3D9 draw wrappers |
| G UI/text | window traversal → `GrpImage/ExpandedImage/Mark/Screen`, text atlas and tails | M9/M10 bridges | Preserve window logic; replace live native state queries with explicit snapshot view |
| D developer geometry | `GrpScreen` boxes/lines/mesh helpers; terrain wireframe; `RenderCollision` | Partial M9 2D helpers only | Inventory caller reachability, do not invent new debug renderer |
| A notice banners | `uitip.py::TextBar/BigTextBar` → `grp.RenderTextBar` → `CDibBar/CBlockTexture` | None | Upload original CPU DIB pixels and clip blocks through M9, preserve GDI rasterization |
| F/library candidate | `CDecal/CTerrainDecal` | None | No external instantiation found; retain with caller evidence, do not confuse these with guild projection |

## Minimal integration boundaries

1. Explicit preview scope at `chr.Deform/Render`; no second actor engine or changes to original Python packages.
2. Narrow native-material additions for existing two-texture special passes; no generic material framework.
3. D3D9 resource objects remain for native asset decoding / CPU skinning uploads in M11. Zero native draws must not be misrepresented as zero D3D9 device dependence.
4. `CStateManager` retains its legacy implementation. Diligent-only compatibility state must be initialized from valid defaults and never rely on uninitialized sampler entries. Raw light/viewport/scissor setters and snapshot readers must use the same view.
5. Only after path coverage: suppress the five actual native Draw wrappers when Diligent is selected, with separate attempted/actual counters. Do not count attempts as hardware calls or treat suppression itself as proof of visual parity.
6. Runtime resources, state validation, ON/OFF and GPU oracle tests remain mandatory. Unknown reachability or inaccessible live scenarios remain explicitly unverified.

The complete call-site inventory is `renderer-milestone11-d3d9.csv`; the 32-point report is `renderer-milestone11-validation.md`. No M12 implementation is authorized here.
