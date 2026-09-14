# D1-X prechange dependency audit

The prechange inventory was captured before D1-X source implementation. The CSV contains 921 matching source lines in 67 owned files, with file, line, nearest textual class/function context, referenced SDK/wrapper symbols, purpose, A-J categories, migration recommendation and exact source text. Context is a navigation aid from textual declarations, not a C++ semantic-parser claim; file and line are authoritative. The JSON records SHA-256 hashes of the captured files and all physical include directives. Historical before artifacts must not be regenerated from the modified tree.

Scope: all owned files below `src/`, `tests/`, `android/`, `buildtool/`, `.github/`, root CMake, vendor CMake and the imported Granny target declaration. Build outputs and third-party source contents were excluded. SDK files were separately enumerated: `extern/include/granny.h`, `extern/include/granny2_spu_samplemodel.h`, `extern/library/Granny/granny2_static.lib`, and its `CMakeLists.txt` target declaration. The SDK and library are retained.

Search includes `granny.h`, `granny2`, every `granny_*` spelling, `Granny*`, `CGranny*`, `EterGrnLib`, and Granny build definitions. Comments, wrapper references and tooling are marked separately from raw SDK references. Multiple A-J categories on one line describe mixed responsibility.

## Include baseline and measurement caveat

| Physical include metric | Before |
|---|---:|
| SDK includes, all owned files | 4 |
| SDK includes in owned headers, including legacy/PCH internals | 4 |
| SDK includes in `src/Renderer` | 0 |
| SDK includes in Game/Actor | 0 |
| SDK includes in World/Static | 0 |
| Direct cross-module `EterGrnLib/...` includes | 24 |
| Those cross-module includes in headers | 11 |
| Those cross-module includes in Game/Actor | 7 |
| Those cross-module includes in World/Static | 5 |

The four physical SDK includes were `EterGrnLib/StdAfx.h`, `Material.h`, `Deform.h`, and `SkinningDataAdapter.h`. Renderer/Game/World already had zero physical SDK includes before this work. Their dependency was transitive through legacy headers, and must not be reported as a newly achieved zero. The historical “public headers” column conservatively counts every owned header, even an internal PCH/adapter header; the new public AssetRuntime API is assessed independently. A single private SDK include after isolation would reduce physical SDK includes 4 to 1, while opaque legacy wrapper signatures can still remain.

## Dependency responsibilities and migration boundaries

| Owned area | Classes/functions and SDK responsibility | Categories | D1-X boundary |
|---|---|---|---|
| `EterGrnLib/Thing.*` | `CGraphicThing::OnLoad`, `OnClear`, `LoadModels`, `LoadMotions`; file/file-info, model and animation construction; section frees | A/B/E | Provider owns memory-loaded file and metadata; existing resource manager/pack loading and cache remain |
| `EterGrnLib/Model.*` | `CGrannyModel`; mesh aggregation, raw model access, rigid/skinned source capture and existing upload/deform dispatch | B/D/G/I | Own a neutral ModelHandle; metadata/upload reads through provider; retain named native upload/deformer interop |
| `EterGrnLib/Mesh.*` | `CGrannyMesh`; mesh layout/counts/indices/groups, mesh bindings and native deformer setup | B/C/D/G | MeshAsset geometry/index-width/material views; legacy vertex declaration becomes adapter-private; deformation mathematics unchanged |
| `EterGrnLib/Material.*` | `CGrannyMaterial`, palette; texture/name lookup, two-sided extended-data conversion and legacy draw state | C | Stable neutral material view plus resolved resource texture paths; retain texture cache and draw-state application |
| `EterGrnLib/Motion.*` | `CGrannyMotion`; clip reference/name/duration/text tracks | E | AnimationHandle metadata; legacy text-track/native animation access is explicit interop |
| `EterGrnLib/ModelInstance*` | Instance lifetime; native animation controls/mixer; local/world poses; bone lookup; transform/collision/bounds; remaps/palettes | D/E/F/G/H/I | Neutral pose, bone/attachment and asset views; native mixer, pose evaluation, collision and existing CPU/GPU bridge implementation retained in legacy adapter |
| `EterGrnLib/SkinningDataAdapter.*` | Model skeleton/weighted vertices; mesh binding remaps; world-pose palette capture | D/F/G/H | Existing Phase-B extraction consumes neutral metadata/pose where possible; no palette/math/reorder changes |
| `EterGrnLib/Deform.*` | SSE CPU reference/fallback function with SDK scalar/vertex/matrix layout | G | Retained private native adapter; no new CPU path, no math rewrite |
| `EterGrnLib/LODController.*`, `ThingInstance.*` | Legacy actor/world instance wrappers, hair linked skeleton lifetime, LOD swapping, attachment dispatch | D/E/F/G/H/I | Existing state/lifetime machinery retained; neutral IDs and handles at resource boundary; hair remaps require explicit regression coverage |
| `EterGrnLib/Util.*`, `StdAfx.h` | Rigid/deformable query and broad transitive SDK include root | B/G | Opaque type declarations only in legacy headers; implementation explicitly includes adapter-native SDK header |
| `GameLib/ActorRenderBridge.cpp` | `CGrannyModelInstance` wrapper; mesh/model source, palette and material paths | B/C/G/H | Consume neutral model/material/pose metadata; preserve existing GPU resource submissions |
| `GameLib/StaticObjectBridge.cpp` | `CGrannyModel`, model-instance, mesh nodes and material palette; rigid world/camera blocker rendering | B/C/I | Neutral model/mesh/material identity and metadata, retaining existing buffer/render bridge |
| `GameLib/DungeonBlock.*` | `CDungeonModelInstance : CGrannyModelInstance`; rigid/blended dungeon groups | B/C/I | Documented legacy renderer subclass interop; neutral source metadata inherited from model boundary |
| `GameLib/ActorInstanceCollisionDetection.cpp` | `CGrannyLODController`, `CGrannyModelInstance`; collision attachment matrices | F/H | Neutral bone/attachment lookup, retained existing instance matrix/collision implementation |
| `GameLib/ActorInstanceMotion.cpp` | `CGrannyMotion::GetDuration` | E | Neutral AnimationAsset duration |
| `GameLib/ActorInstanceData.cpp` | Legacy `Util.h` include | B/H | Remove SDK propagation, retain material override data contract |
| `GameLib/ActorInstance*.h`, `Area.h`, `RaceData.h`, `ItemData.h`, `WeaponTrace.h` | Model/thing resource and attachment declarations, no raw SDK calls | A/H/I | Legacy class includes remain explicit; new AssetRuntime public headers are native-type-free |
| `UserInterface/InstanceBase.cpp` | LOD-controller wrapper access | H | Retained LOD orchestration |
| `UserInterface/PythonApplication.cpp` | Sphere-map/material functions, LOD mode and shared-deformer/model-pool lifecycle | C/G/H | Existing application/legacy-renderer lifecycle; SDK-free wrapper declarations |
| `UserInterface/UserInterface.cpp` | `granny_log_*`, unused `GrannyError`, `Setup` calls `GrannySetLogCallback` with null function | A | SDK-free diagnostic initialization in adapter; preserve disabled callback behavior |
| `EterPythonLib/PythonGraphic.h`, `UserInterface/PythonItem.h`, `ScriptLib/Resource.cpp`, `ScriptLib/StdAfx.h`, `PRTerrainLib/StdAfx.h` | Transitive legacy model/thing resource includes | A/H/I | Remove transitive SDK header exposure; retain established resource/script contracts |
| Renderer source | `ActorRenderData.h` has only a comment mentioning Granny; no raw SDK type/function/include | G | Renderer already has native-type-free Phase-B data; connect neutral runtime without rewriting it |
| Tests/build/platform audits | Real-asset skinning references, coverage/prototype/instance/benchmark tests, resource audits and Granny target declarations | J | Tests needing raw reference data explicitly include private adapter SDK header; build attaches Granny to adapter target |

## Every raw SDK site outside the legacy adapter before D1-X

Outside `EterGrnLib` and tests, the only raw SDK declarations/calls were in `src/UserInterface/UserInterface.cpp`: lines 366 (`GrannyError` signature), 376 (`granny_log_callback`), and 379 (`GrannySetLogCallback`). The unused error helper did not install logging: `Setup` explicitly stored `Callback.Function = nullptr` and `Callback.UserData = 0`. Moving this setup must preserve that behavior.

`src/Renderer`, Game/Actor implementation files, `PRTerrainLib`, and the static-object bridges had no raw `granny_*` type or direct `Granny*` SDK call beyond the cited UI startup site. They did depend on legacy `CGranny*` wrappers and transitive SDK headers. `GrannyCreateSharedDeformBuffer` / `GrannyDestroySharedDeformBuffer` are project-owned wrapper functions, not SDK functions.

## Critical preservation points

- `CGraphicThing::LoadModels` frees rigid vertex/index, deformable index and texture sections after source capture; neutral metadata must be captured before this release, and later provider reads must report released upload data rather than dereference it.
- Legacy local texture paths resolve against `GetModelLocalPath`; cached material texture IDs for rendering must reflect the resolved `CGraphicImage` resource path, including per-instance palette overrides.
- `CGrannyMaterial::Copy` historically copies native material pointer, images and stage only. Neutral metadata must reflect the actual destination culling/specular fields without silently changing render behavior.
- Granny bone order, binding-to-bone mappings, hair LOD refresh and existing world/composite matrix conventions are preserved. Owning handles must outlive borrowed native pointers/metadata spans.
- CPU deformation and the Phase-B GPU submission path remain production code. The new metadata layer must not recreate large vertex/skeleton/material objects per draw or per frame.
