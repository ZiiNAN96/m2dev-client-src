# Milestone 4A: Legacy object architecture (before implementation)

> Historischer Milestone-Stand. Aktuell seit [M12](renderer-milestone12.md): Diligent D3D11 ist im ON-Build der Default; Legacy wird explizit mit `--renderer=legacy-d3d9` gewählt. OFF-Builds bleiben Legacy-only. Frühere Testergebnisse und Auswahlbeschreibungen unten gelten für ihren damaligen Stand.

Baseline: `d466965`. Scope: one path, opaque nonanimated rigid PNT map buildings/props; no animation, skinning, blend, specular, shadow, dungeon, SpeedTree or character migration.

## 27 source-backed answers

1. `CMapOutdoor` owns active/deferred-delete `CArea` vectors and the surrounding area pointers. `CArea::TObjectInstance` and its vectors own/collect map instances. `MapOutdoorUpdate.cpp` performs existing area streaming/deletion.
2. `CMapOutdoor::LoadArea` (`MapOutdoorLoad.cpp`) calls `CArea::Load`: `AreaData.txt`, `AreaAmbienceData.txt`, property CRC lookup via `CPropertyManager`, `__Load_BuildObjectInstances`. Building `.prb` resolves `buildingfile`, attribute file and shadow flag.
3. `MapType.h`: NONE, TREE, BUILDING, EFFECT, AMBIENCE, DUNGEON_BLOCK. Characters are not one of these area property types.
4. BUILDING can be static or animated Granny; `IsMotionThing` means a nonempty registered motion map. TREE is SpeedTree, EFFECT uses `CEffectInstance`, AMBIENCE is sound-only. Attribute/collision and height data belong to instances; there is no separate collision-only property enum. A nonvisual attribute is not a drawable mesh.
5. Normal building: `CGraphicThingInstance`, created in `CArea::__SetObjectInstance_SetBuilding`.
6. Simple prop: the same BUILDING property / `CGraphicThingInstance`, not a separate prop renderer.
7. Loading: AreaData CRC -> property -> resource manager -> `CGraphicThing::OnLoad` -> original `GrannyReadEntireFileFromMemory` -> `LoadModels` -> `CGrannyModel::CreateFromGrannyModelPointer` -> meshes/materials/client VB and IB -> Thing/LOD/model instances. Drawing: `CPythonApplication::RenderGame` -> background/map render -> `CMapOutdoor::OnRender` -> `RenderArea` -> `CArea::CollectRenderingObject` -> distance sort -> `CGraphicObjectInstance::Render` -> `CGraphicThingInstance::OnRender/RenderWithOneTexture` -> `CGrannyLODController::RenderWithOneTexture` -> `CGrannyModelInstance::RenderWithOneTexture/RenderMeshNodeListWithOneTexture` -> material Apply -> `CStateManager::DrawIndexedPrimitive` -> D3D9.
8. `CArea::TObjectData`: position, yaw/pitch/roll, height bias, CRC/portals. `CGraphicObjectInstance`: position/rotation/scale and world matrix. `CGrannyModelInstance`: per-mesh final `m_meshMatrices`.
9. Building position is data XYZ plus height bias on Z; **no additional Y inversion**. Three-angle rotation uses `D3DXMatrixRotationYawPitchRoll` in degrees converted to radians; single-angle setter is Z rotation. `Transform` copies rotation and adds translation. `SetScale` stores scale but this building `Transform` does not multiply it: do not invent scaling. Embedded bind/mesh transforms may still contain scale or reflections.
10. Model instances support parent bone attachments generally. Static building creation does not attach them. `UpdateWorldMatrices` composes rigid mesh bone composite with object world matrix; deformable meshes use object world. Consume its final **m_meshMatrices** matrix, never rebuild bones/parents. Important: the existing similarly named `GetMeshMatrixPointer` is a collision/bone-pose accessor in `ModelInstanceCollisionDetection.cpp`, NOT the final draw matrix; 4A exposes a separate read-only final-world accessor.
11. Registered bounding spheres feed `CCullingManager` / SpherePack; visibility callbacks Show/Hide. Area collection requires `isShow` and excludes `HaveBlendThing`. Portal and PC-blocker handling already affect visibility. Preserve all of this.
12. `CCullingManager::Process` updates view/projection, builds frustum, runs `FrustumTest`. Area streaming limits the population. `UpdateLODLevel` uses existing center/camera distances; `LODController.cpp` has actor/building limits 5,000/25,000 and existing thresholds. No replacement distance culling.
13. Area data sorts by CRC when building instances. Actual opaque rendering sorts by **camera distance ascending**, not material. Mesh/group linked lists reflect existing prepended order. No sorting redesign.
14. Rigid one-texture path binds `TPNTVertex`: Position float3, Normal float3, UV float2, 32-byte stride. Model loader also recognizes PNT2 (40-byte, two UV sets) and deformable inputs. PNT2/deform are excluded, not converted into the first path.
15. Client IB is `D3DFMT_INDEX16` / `TIndex` (uint16). Triangle list, group start = mesh index base + TriFirst*3; base vertex = mesh vertex base. Preserve both offsets and counts.
16. Original Granny file/model mesh data exists during `LoadModels`; conversion uses existing `NEW_LoadVertices` / `LoadIndices`. **After all models load, Thing.cpp frees rigid vertex/index sections.** Later reads through Granny mesh pointers or WRITEONLY D3D buffers are unsafe. 4A needs a bounded CPU snapshot while the existing data is alive, only for nonanimated map loads and eligible rigid PNT models.
17. Client `CGraphicVertexBuffer` allocates D3D9 VB; Granny copies/converts vertices into client-provided storage. Granny does not create the VB.
18. Client `CGraphicIndexBuffer` allocates D3D9 IB; Granny copies indices. Granny does not create the IB.
19. `CGrannyMesh::LoadMaterials` registers bindings in `CGrannyMaterialPalette`; triangle groups map to palette indices. Model instance has its own material palette; use that palette for draws, not a guessed filename/material.
20. `CGrannyMaterial::__ApplyDiffuseRenderState` binds diffuse image texture at stage 0. Image resolution uses `CResourceManager` and original local-path rules. Static adapter must use the already resolved image resource filename; pack/image decoder preserves original DDS mip chain. `CPythonGraphic::SetGameRenderState` sets anisotropic min/mag and linear mip; take the actual legacy MAXANISOTROPY (including its device default), not a new quality setting.
21. Normal opaque path inherits solid/Gouraud, CULL_CW, depth enabled LESSEQUAL and `RenderArea` forces depth writes. Alpha blending default false. Two-sided material temporarily sets CULL_NONE. Specular and shadow multi-stage paths are out of scope and must not silently be approximated.
22. `CScreen::SetBlendOperation` initially sets stage0 RGB/alpha TEXTURE*CURRENT and disables stage1. **Then RenderArea calls CArea::RenderDungeon unconditionally**, even for empty dungeon lists: it leaves stage0 RGB SELECTARG1(TEXTURE), alpha still MODULATE, and disables stage1 before returning. Thus the tested shadow-disabled opaque path has unlit texture RGB plus vertex fog. The shadow-receiver setup can restore MODULATE; both existing simple color operations must be respected. Material diffuse Apply changes texture and optional cull only. Real first-frame diagnostics confirmed SELECTARG1 and no alpha test/blend.
23. Default alpha-test disabled; default stored reference 1, function GREATEREQUAL. Opaque RenderArea does not enable it. Terrain temporarily uses GREATER 0 and restores it. No guessed cutout threshold; first path accepts alpha-test-disabled state and documents unsupported enabled variants.
24. `CGrannyMaterial::CreateFromGrannyMaterialPointer` classifies opacity-image materials as BLEND_PNT (including Blend-named multi-map material resolution). `HaveBlendThing` sends whole objects to RenderBlendArea, which enables SRCALPHA/INVSRCALPHA. Excluded entirely in 4A.
25. Material extended `Two-sided` controls diffuse Apply CULL_NONE. Mesh name prefix `2x` sets a separate flag, but the selected one-texture draw does not consult that mesh flag. Reproduce actual material cull, not the name heuristic.
26. `CMapManager::BeginEnvironment` enables lighting, applies environment D3DMATERIAL9, and enables/disables directional light0. Global ambient/material-source/normal normalization states come from StateManager. Fog uses existing vertex EXP or LINEAR/range-fog state. Lighting is enabled but does not multiply RGB in SELECTARG1 (question 22); preserve this, not a visually improved interpretation. Reproduce existing diffuse+ambient/emissive only for MODULATE. Native MAXMIPLEVEL/LOD bias/anisotropy are read from the device: those default fields are not initialized by the legacy sampler cache. No StateManager rewrite.
27. World is the **actual mesh matrix** above; view/projection are current StateManager D3DTS_VIEW/PROJECTION. Row-vector/row-major multiplication world*view*projection. Preserve D3D9 depth range and the established terrain D3D9-to-D3D11 pixel-center projection correction. Negative determinant does not authorize automatic winding correction: original fixed cull remains authoritative.

## Categories and selection

| Object | Current rendering | Animation | Granny | Scope |
|---|---|---|---|---|
| Terrain | Legacy or Diligent splatting | no | no | completed |
| Rigid opaque PNT building/prop | Legacy | none registered | yes | **4A first path** |
| Animated building | Legacy | yes | yes | later |
| Blend/specular/PNT2 building | Legacy | varies | yes | later |
| Dungeon block | separate CDungeonBlock/CDungeonModelInstance | separate path | yes | later |
| SpeedTree | separate forest | wind/LOD | no | later |
| Characters/NPCs/mobs | actor path | yes | yes | later |
| Effects/flying | effect path | varies | separate | later |
| Attribute/collision, ambience | nonvisual/debug only | n/a | no drawable mesh | not migrated |

Real representative verified with the **existing Granny library**, not a new parser:

- Property `property/a/01/a1_018-stonelight.prb`, CRC **935928503**, Building.
- `d:/ymir work/zone/a/building/a1_018-stonelight.gr2`: 1 model, **0 animations**, rigid `Cone20`, **1375 vertices / 2169 indices / 723 triangles**, 1 material/group, Position/Normal/TextureCoordinates0.
- Diffuse `D:/YMIR WORK/zone/A/building/a1-018-stonelight-01.dds`; no opacity texture.
- A1 tile `002002/areadata.txt`, Object055: `(58082.843750,-60984.550781,19845.179688)`, rotation `(0,0,15)`, height bias `-5`. Additional real instances with rotation 15 and 0 exist in the same tile.
- A separate checked rigid rock `general_obj_bigstone01.gr2` also has zero animations, 109 vertices/387 indices/1 group. This is architecture evidence, not permission to add another renderer variant.

## Minimal implementation seams

1. Renderer: neutral static-object source/handles/draw parameters and separate Diligent object renderer. Reuse existing neutral image upload schema through a tiny upload interface, not the terrain-specific 289-vertex API.
2. Area: bracket synchronous building resource load with a map-only capture scope; unregister object GPU resources before pooled Thing deletion.
3. Thing/Model **client wrappers only**: snapshot original converted rigid PNT data before existing section frees, gated by map-load scope and zero animations. Read-only instance material accessor. No Granny API replacement, parser, pose, animation or skeleton edits.
4. MapOutdoor: call adapter only in the existing sorted opaque object loop, using existing visibility/current LOD. Existing Legacy Render call remains intact. Adapter rejects unsupported paths; explicit counters separate exclusions from errors.
5. Presentation: same existing Diligent device/swapchain/depth/frame for terrain and objects; object renderer dies before backend. No UI/compositing change.
6. Tests: targeted GPU parity/validation, real A1/B1/A1 object smoke, ON/OFF release, full suite, manual min/restore and normal-start shutdown. Record gaps honestly; no 4B until 4A succeeds.
