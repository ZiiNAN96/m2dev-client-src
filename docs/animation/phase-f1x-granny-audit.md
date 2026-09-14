# F1-X prechange animation audit

Captured on 2026-09-14 before F1-X implementation. Scope is owned `src/**/*.h` and `src/**/*.cpp`; vendor declarations, tests, build output and comments are excluded from dependency counts. These are source token counts, not runtime invocation counts.

| Metric | Before | Location |
|---|---:|---|
| Physical `granny.h` includes | 1 | `AssetRuntime/Granny/Native.h` |
| SDK pose calls | 9 | `AssetRuntime/Granny/GrannyAssetProvider.cpp` |
| SDK sampling calls | 1 | Same provider, accelerated model sampling |
| SDK control calls | 42 | Same provider |
| Local/world pose type occurrences | 18 | 16 private provider; 2 legacy skinning adapter |
| Animation-related SDK type occurrences | 54 | 43 private provider; 11 EterGrnLib |
| Actor SDK animation includes/types/pose/sampling/control calls | 0 | Already zero before F1-X; not a new achievement |

Actor scope comprises the 27 files matching `src/GameLib/Actor*` and `src/UserInterface/InstanceBase*`. Legacy `CGranny*` wrappers are project classes, not raw SDK types. The private `GrannyPoseEvaluator` class is likewise excluded from SDK call counts. Categories overlap deliberately (sampling and pose types are different measurements).

Reproduction: enumerate paths using `rg --files src -g '*.h' -g '*.cpp'`, read each file and remove comments with `[regex]::Replace(text, '(?s)/\*.*?\*/|(?m)//[^\r\n]*', '')`. Sum `[regex]::Matches(code, pattern).Count` for each metric using these patterns:

```text
physical SDK includes: #\s*include\s*[<"][^>"\r\n]*granny\.h[>"]
SDK pose calls:        \bGranny(?:New|Free|Get|Set|Build|Sample|Accumulate|Apply)\w*Pose\w*\s*\(
SDK sampling calls:    \bGranny(?:Sample|Evaluate)\w*\s*\(
SDK control calls:     \bGranny\w*Control\w*\s*\(
local/world types:     \bgranny_(?:local|world)_pose\b
animation types:       \bgranny_(?:animation|skeleton|bone|track_group|transform_track|curve2|control|local_pose|world_pose|model_instance)\b
```

This lightweight textual baseline excludes comments; it is not a C++ semantic parser. Keep the historical values unchanged when measuring the final tree.

| File / function | SDK data or function | Existing purpose | F1-X migration | Retained responsibility |
|---|---|---|---|---|
| `GrannyAssetProvider.cpp / DescribeSkeleton` | skeleton, bone, LocalTransform, InverseWorld4x4 | Copy original bone IDs/order/names, parents, local bind and inverse bind | Validate and own neutral runtime skeleton | GR2 decoding |
| `GrannyAssetProvider.cpp / DescribeAnimation` | animation name/Duration/TimeStep/TrackGroups | Clip metadata and first-group text events | Own neutral clip samples | Existing event order |
| Private raw animation data | transform track PositionCurve/OrientationCurve/ScaleShearCurve | SDK compressed spline channels | Decode/bake once, preserving 3x3 scale/shear | Source-format handling and reference |
| `GrannyAnimationInstance` constructor/destructor | InstantiateModel, FreeModelInstance | Native control lifetime | Keep control compatibility; separate neutral pose buffers | Control clocks, root-motion extraction |
| `SetMotion / ChangeMotion / CopyMotionFrom` | PlayControlledAnimation, control ease/speed/loop/raw-clock APIs | Existing playback and transition timing | Feed owned neutral clips using the same timing | Existing root-motion/control contract |
| `SetMotionAtEnd / SetClock` | control local duration, model clock | End stop, update timing | Consume current clocks without track traversal | Actor timing unchanged |
| `Evaluate / GrannyPoseEvaluator::Evaluate` | LocalPose, WorldPose, SampleModelAnimationsAccelerated | Reference pose and hierarchy sampling | Independent neutral sampling/evaluation/palette in opt-in mode | Explicit default/reference path |
| `UpdateTransform` | UpdateModelMatrix | Existing accumulated movement | Preserve root-motion compatibility | No gameplay rewrite |
| `ModelInstanceModel.cpp / __GetPoseOwner` | Neutral AnimationInstance after D1-X | Shared hair/body skeleton ownership | Same interface | Hair LOD ownership |
| `GetBoneIndexByName / GetBoneMatrixPointer` | Neutral lookup; legacy FindBoneByName fallback | Weapon, hair, rider, collision attachments | Same stable IDs/world matrices | Existing attachment system |
| `ModelInstanceSkinning.cpp / __RefreshLinkedLodBinding` | Mesh bindings via neutral interface | Refresh hair mapping on body LOD changes | Preserve destination skeleton/remap policy | No bone sorting |
| `ModelInstanceUpdate.cpp / UpdateWorldPose` | EvaluatePose -> CompositePose | Pose request, optional attachment transform, GPU palette capture | Same seam delivers independent pose | Existing GPU skinning |

`SetMotion` uses ease-in/out curves and SDK control loop count/speed. `ChangeMotionPointer` deliberately passes local time minus 0.3 seconds. Copying motion preserves raw local clock, speed and loop count. Actor durations are divided by the same speed ratio. Time advancement is the existing actor clock; stopped time therefore pauses sampling without a separate global pause feature. No explicit additive blend or track-mask API was found in the owned production actor path.

Root motion is an active gameplay dependency: `GameLib/ActorInstance.cpp::__AccumulationMovement` calls `UpdateTransform` with the heading matrix, then passes its translation into `AddMovement`; mounted movement delegates to the horse. This reaches `GrannyUpdateModelMatrix`. The SDK track group stores `InitialPlacement`, `LoopTranslation`, optional `PeriodicLoop`, and accumulation flags (`Extracted=1`, `IsVDA=4`). An opt-in pose implementation must not replace movement extraction with a no-op. Retaining control/root-motion extraction while removing SDK pose sampling preserves this contract.

Track-group matching uses the model name (`GrannyFindTrackGroupForModel`). Broadcast motions that do not animate a rigid attachment are reported as `NoMatchingTracks`, an expected no-op. Missing bone tracks use the skeleton's local default; malformed references must be rejected. Skeleton and animation documents own the source data, while neutral runtime data must own every copied transform and key.

The SDK supports identity, constant, keyframed and compressed spline curve storage. `granny_curve_data_header` contains Format and Degree; query dimension/degree/type using the curve APIs. Position, rotation and scale/shear dimensions are 3, 4 and 9 respectively. The F1-X adapter samples bound local transforms with `GrannySampleModelAnimations` only during one-time import; playback consumes owned neutral keys. This preserves track binding and extracted-root semantics without mistaking B-spline controls for ordinary linear keys.

The bounded original-asset inventory measured 13 representative clips: all curve storage is format 1 (`DaK32fC32f`), with no keyframed curves. Counts are 857 identity curves (dimension 0, degree 0), 859 constant position curves, 167 constant rotation curves, 220 constant scale/shear curves, 109 moving scale/shear curves of degree 1, 86 moving position curves of degree 2, and 537 moving rotation curves of degree 2. F1-X explicitly rejects other storage formats, keyframed curves, degrees above 2, invalid dimensions, and morph/vector track groups. This is an evidence-based support boundary, not a claim about every asset in the library.

All representative groups have flags 2 for idle/attack or flags 3 for locomotion; initial-placement translation is zero. Locomotion loop translation is along source Y: warrior walk -176.858/run -300, wolf walk -75.8081/run -255.45, horse walk -233.308/run -493.784. All measured clip timesteps are 1/30 second. Horse transform tracks begin with `F_Lip_01`, while the group/root is `horse_bip01`; track zero must never be assumed to be the root.

## Matrix and attachment ground truth

Phase-B source evidence remains authoritative (`docs/renderer/phase-b1-gpu-skinning-analysis.md` and `phase-b2-skinning-data.md`): row vectors, translation in row four, local times parent, composite equals stored inverse bind times evaluated joint/model matrix. ArtTool coordinates are RH/Z-up; no new axis conversion or transpose is introduced. Bind/default transforms do not guarantee an identity composite for every original asset.

The GPU palette uses the active destination skeleton's full original order. Hair maps mesh-local indices into the body-owner palette; it must not combine a hair-source inverse bind with a destination joint transform. Attachment model matrices may already contain a hand or saddle matrix; ActorWorld is applied once by the existing draw path. Bone world matrices and inverse-bind composite matrices remain separate interfaces. Normals preserve Phase-B weighted matrix multiplication without per-bone inverse transpose or extra normalization.

Recommended integration seam: `GrannyDocument::CreateAnimationInstance` constructs the existing provider instance, whose experimental `Evaluate` branches into neutral Sample -> Evaluate -> BuildPalette. `CompositePose`, `BoneWorldMatrix`, mesh binding and parent pose ownership retain their current contracts. The experimental branch must not call the reference sampler on failure and must reject a stale pose visibly.

## Implemented import and playback boundary

Content-retention validation before stall instrumentation: Release
37/37 in 93.74 seconds, Debug 37/37 in 137.60 seconds and GCC 10/10 in 0.35
seconds. Current Granny04 and ZiiNAN05 native smokes each pass in 61.4 seconds,
with exit 0 and all tracked owner/retention counters zero. ZiiNAN05 records
23,747 independent poses and zero reference poses. Earlier Release/Debug
gates (97.13/129.47 seconds), Granny03 and ZiiNAN04 precede this revision. Manual03 failed
because the user still observed the whole image briefly freezing during
walking and combat, despite its clean exit. The historical inventory and
measurements below retain their original scope.

`GrannyAnimationAdapter` imports the exact bound local poses into immutable neutral translation/quaternion/full-scale-shear tracks. Four imported variants preserve the source curve's previous/next loop neighborhoods. Native controls supply their existing local clock, loop-neighborhood flags, effective blend weights and gameplay root-motion extraction; the experimental frame evaluator reads no native pose or animation tracks. It samples neutral keys, accumulates weighted local poses, normalizes the quaternion sum once, evaluates the hierarchy and builds the inverse-bind palette.

Import sampling starts at a maximum 1/60-second interval and refines using quarter, midpoint and three-quarter checks. These adaptive acceptance checks use the larger of 0.00002 source units and two local float ULPs for translation, 0.000002 for normalized quaternion components, and 0.000002 for scale/shear components. A separate channel reduction pass uses tighter thresholds of 0.000005 units/two ULPs for translation and 0.0000005 for quaternion/scale. These are sampled acceptance thresholds, not a proof of an error bound at every time. Final independent parity tests assess pose, model, palette and vertex errors separately; the import thresholds are not claims of bit equality.

Two measured numerical issues were corrected without disguising them as artistic interpolation: adding a whole duration before sampling loses local-clock precision, and storing double timestamps for float-clock samples mislabels the native sample time. The importer now sets raw local clock and loop index separately and records the exact float-representable sample timestamp. The two-ULP translation floor handles measured source floating-point quantization (for example a 0.000030517578125 root-Z difference around 131 units).

Conversion occurs on first clip binding. Active provider instances retain their
imported clips and animation handles; the model document owns the neutral
skeleton. New process retention owns immutable neutral imports plus copied
content keys, with no global source-document, skeleton or SDK ownership.
Each key contains SHA256 of both original files, model and animation indices,
and skeleton binding ID. Reloading identical bytes under another asset ID can
reuse the same import after all original documents disappear. Changed bytes
under the same ID must miss. Fingerprints are computed before SDK decoding by
a private provider function using Windows CNG; `bcrypt` is a PRIVATE provider
link dependency. The neutral core has no Windows dependency and Granny-default
frame sampling is unchanged; hashing adds document-load work. Primary source:
[Microsoft BCryptHashData](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcrypthashdata).

Missing source tracks on a reduced LOD are collected by name and produce an
explicit once-per-import binding warning while preserving the SDK's existing
mapping. Invalid neutral target IDs remain errors. Failed imports free the new
control before returning; the evaluator rejects failed/stale poses without
falling back to the reference sampler.

Import safety limits are 600 seconds per clip, 1,024 source bones, 131,072 times the original bone count in sampled bone transforms per loop variant, and a separate maximum of 2,097,152 sliced SDK calls per variant. This preserves the original whole-pose work ceiling while bounding per-call overhead. Further limits are 32,768 saved samples per bone/variant, 256 MiB of saved-pose working storage, and 2,000,000 final channel keys across all four variants. Source/neutral bone count, order and parent indices must agree. Key reduction uses an explicit worklist; finite extremes and invalid quaternion arithmetic fail closed. A narrow native probe verified finite interpolation and preservation of opposite `FLT_MAX` translation keys, finite ULP tolerance and zero-quaternion rejection, including after the sliced-sampling change.

## Final private source dependency comparison

The same baseline definition measured after the implementation gives SDK includes 1 -> 1, SDK pose calls 9 -> 12, SDK sampling calls 1 -> 2, control calls 42 -> 57, local/world pose type occurrences 18 -> 19 and animation SDK type occurrences 54 -> 60. Actor direct SDK metrics remain 0 -> 0. The extra sampling site is import-only. Source totals grow because F1-X preserves the reference path and adds a private importer; independence is established by the experimental frame path and its counters, not by claiming a total source-count reduction that did not occur.

## Initial first-import cost before the manual lag correction

The initial standalone Release import probe succeeded for the following clips. Measurements ran alongside builds and are approximate orientation only. Key bytes below count the allocated final key arrays for all four variants, excluding source data, clip metadata and temporary import storage. These historical numbers precede the manual lag correction below.

| Model/clip | Source samples | Final keys | Key bytes | First import ms |
|---|---:|---:|---:|---:|
| Warrior run | 118,278 | 394,323 | 9,470,952 | 2,223 |
| Wolf attack | 245,928 | 722,672 | 17,347,968 | 3,684 |
| Boss idle | 109,396 | 419,481 | 10,418,088 | 5,707 |
| Boss attack | 108,988 | 428,012 | 10,626,576 | 4,472 |
| Horse idle | 18,196 | 50,213 | 1,210,296 | 239 |
| Horse walk | 65,984 | 263,455 | 6,328,104 | 813 |
| Horse run | 94,470 | 392,288 | 9,420,096 | 2,254 |

## Manual lag finding and bounded import correction

The real ingame manual check reported severe lag, including repeated actions. Its first run recorded 69 imports and 4.64 million whole-pose import calls. The original weak import cache could lose conversions after the last animation instance disappeared, even while the model asset remained loaded. This manual result blocks acceptance of that revision despite its earlier automated correctness passes.

A bounded profile of `warrior_4-1` / `onehand_sword/combo_01` attributed approximately 74% of 1.52 seconds to whole-pose sampling/refinement and 26% to channel construction/reduction. Sampling all bones whenever one channel needed refinement repeated unnecessary work. A separate exact SDK proof compared full-pose sampling with `GrannySampleModelAnimations(model, firstBone, 1, localPose)` for 14,580 bone samples across armored warrior, reduced warrior LOD, wolf, boss and horse, all four loop neighborhoods and nine times. The observed position, quaternion and scale/shear values matched bit for bit.

The importer now refines each bone separately using that same bound SDK sampler and reusable one-transform scratch. Core interpolation, binding, root-motion semantics, loop neighborhoods and final parity tolerances are unchanged. A direct bound-track shortcut was investigated but not adopted. The bounded seven-clip standalone Release scan succeeded after the change:

| Model/clip | Sliced SDK calls / sampled bones | Final keys | Key capacity bytes | First import ms |
|---|---:|---:|---:|---:|
| Warrior run | 1,231,584 | 225,649 | 5,422,776 | 1,025 |
| Wolf attack | 2,135,832 | 416,377 | 9,996,888 | 1,039 |
| Boss idle | 1,864,672 | 241,531 | 6,160,824 | 768 |
| Boss attack | 1,898,902 | 247,295 | 6,303,000 | 786 |
| Horse idle | 180,966 | 29,682 | 717,552 | 78 |
| Horse walk | 842,220 | 159,105 | 3,823,704 | 312 |
| Horse run | 1,180,356 | 231,168 | 5,553,216 | 423 |

The new sample-call counts cannot be directly compared with old whole-pose calls: each new call evaluates one bone. Times are local observations, not guaranteed frame budgets. Cold imports remain synchronous and can still take about a second in this scan.

The earlier document cache retained 32 MiB of key vector capacity per model
and 128 MiB overall. The manual scene's logged unique model/clip combinations
account for 313.49 MiB, including 73.95 MiB for 12 stray-dog clips and 71.89 MiB
for 17 warrior clips. These are sums of distinct key capacities, not measured
simultaneous RSS or cache peaks; they explain why the old limits were too small.

The new private process LRU retains up to **128 MiB per model digest/model index
and 512 MiB globally**, with 1,024 strong entries per model, 4,096 globally and
8,192 weak live entries. Shared import allocations count once globally and
once per relevant model budget. Active pins can outlive eviction and are
outside the retention budget; source data, skeletons, metadata and scratch
are also separate. A weak content index preserves lookup of still-active
evicted imports and prunes expired runtime entries. `ClearImportCache()`
releases strong retention and the weak index after Main and before the owner
audit. It does not invalidate active owners. There is no disk cache or reuse
across process restarts, and no persistent animation format.

Import-only diagnostics record each conversion taking at least 50 ms and
aggregate total/peak microseconds; hit, eviction, current and peak key-byte
counters describe the observed cache behavior. The second manual run exposed
loss of lookup entries while actors still owned the same import. The then
added document-owned weak index passed regression, but did not prevent genuine
misses after strong LRU eviction and source teardown.

Manual03 included that live-index fix but still froze briefly while walking
and fighting, according to the user. The agent closed PID 67176 through the
window X: exit 0, 140 imports, 36.612591 seconds of import time, 85 evictions,
1,780,982 independent poses, zero reference poses, CPU deformation, fallbacks
or runtime failures, and all owner/retained-byte counters zero after shutdown.
This is a failed manual performance check, not acceptance of the new retention.
Cold and genuinely evicted combinations still require synchronous imports.

The new native cache regression passes in Release and Debug; current native
smokes also pass. It verifies source documents expire while neutral clips
survive; identical bytes reload under other IDs without SDK import; changed
valid animation or model bytes under the same ID miss; model indices and
binding IDs separate entries; active evicted imports remain reusable; and
source counts are zero before explicit Clear, all neutral/retention counts
zero afterwards. It checks real original-import key capacity against actual
vector allocations. Separate small valid neutral fixtures use explicitly
**declared byte sizes** to exercise the 128/512-MiB limits and global LRU order
without allocating 512 MiB. Earlier 32-MiB tests used physically reserved
arrays; the new budget fixtures are not a physical 512-MiB stress test.
ActorPath now also clears neutral process retention after source teardown.
No F2 conversion, asynchronous import graph or animated GLB migration was
introduced. The final manual stall audit is complete and the user accepts
the remaining first-use/warmup limit for F1-X; the hitch problem is not fixed.
The captured run contains 62 imports for 62 full content keys, 1,942 cache
hits, zero repeated imports, zero evictions and zero dropped audit records.
All active gameplay display intervals above 40 ms contain imports. Twelve
import-free intervals above 20 ms remain (maximum 39.8406 ms), with at most
1.4389 ms spent in independent pose evaluation in those intervals.
The overall active-game pose-frame maximum is 6.6438 ms in the world-entry
burst (1,039 evaluations across three updates), versus a largest single
import of 773.1129 ms. The warm pose path is not the demonstrated stall
bottleneck. Functional steady-state use after warmup is supported by this
short run, not a guarantee of every frame staying below 20 ms.
The user confirmed the requested repeated Wildhund actions and X close;
exit 0, 446,455 independent poses, zero reference poses/fallbacks/runtime
failures and all tracked owners zero. Full results and scope are in
[the stall audit](phase-f1x-stall-audit.md).
Final instrumentation checks also pass: Release 6/6 focused plus 4/4 final
format/header checks, Debug build plus 5/5 focused checks, GCC 11/11.
F1-X GO with the accepted performance limitation. Technical GO for the next
F2-X reader boundary, but F2-X is not started. No further cache/preload or
persistent-cache optimization. Granny remains the production default.
