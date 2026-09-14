# D1-X runtime API and ownership notes

These implementation notes supplement the milestone report; build, parity and live acceptance results belong in the report.

## Public boundary

`src/AssetRuntime/AssetRuntime.h` is the format, backend and platform independent API. It contains no SDK, window, GPU backend or renderer types. `AssetRuntime` builds independently of the Windows client; `AssetRuntimeGranny` is the production provider target.

The existing resource manager supplies decoded asset bytes and the unchanged resource path to `LoadModel`. Provider selection is an argument at this boundary. The current resource entry point selects the production provider; adding another provider does not require changing the resource/pack path convention or creating another asset cache.

The public API covers immutable model, mesh, material, skeleton, animation and binding metadata, caller-owned geometry upload buffers, persistent animation instances, pose views, attachment lookup and CPU deformation through mesh bindings. Renderer buffers, matrix conventions, shaders and GPU skinning mathematics remain separate.

## Ownership

`AssetHandle`, `ModelHandle` and `AnimationHandle` retain one document. Mesh metadata is stored inside its model and does not carry a separate shared owner. A provider document owns the complete loaded file through RAII. Failed metadata construction releases this file automatically. Model/animation handles remain valid after their root handle is reset.

`ReleaseUploadData` is explicit and idempotent. The resource caller releases upload sections only after model buffers, actor indices, static-object snapshots and skinning data have been captured. Later neutral geometry reads report `UploadDataReleased`; metadata and animation data remain available. No old load path is attempted after a provider failure.

An animation instance retains its model document and the clip documents of active controls. The provider prunes clip owners by inspecting the SDK's active model-control bindings after completed controls are freed; this avoids dereferencing freed controls. Model destruction occurs before its retained clip handles are released. Source and destination documents remain alive while a mesh binding uses their skeletons.

`liveDocuments`, `liveAnimationInstances` and `liveMeshBindings` expose RAII lifetime counts for acceptance and shutdown checks. The adapter retains the former shared local-pose scratch policy on the existing model-update thread; it does not allocate scratch or reconstruct skeleton metadata per frame.

## Metadata and geometry

Original mesh/material slots, bone order, parent indices, mesh-to-skeleton remaps, local bind transforms and inverse-bind matrix bytes are retained. Duplicate bone names resolve to the first original slot. Bounds describe original local vertex positions, with individual binding OBBs copied separately. Bounds extraction reads source positions directly without an additional large vertex array.

Exact packed vertex layout and semantic vertex channels are distinct. The latter preserve the legacy renderer's channel detection even if a source contains additional attributes; the former preserve the existing GPU-skinning eligibility decision. Geometry reads use the established provider conversion into PNT, PNT2 or weighted PNT output. Source index width is explicit; a request to narrow 32-bit indices to 16 bits fails before copying if an index cannot be represented.

Material descriptions preserve render texture references, original root-material matching references, legacy diffuse/opacity stage, two-sided culling and draw-state defaults. Palette copies and overrides maintain their own cached material description. No texture resource loading or shader-state redesign is introduced by the provider.

Animation metadata includes duration, time step, track-group count and the existing first-track-group text-event sequence.

## Animation, pose and binding

`AnimationInstance` owns the animation model and lazily prepares its world pose. Its motion start, blend, change, copy, completion, clock, loop, speed, root-motion and end-position operations delegate to the same SDK calls and preserve their original order. The legacy 0.3-second change-motion skip remains in the consumer. No animation mixing or skeleton-solving algorithm has been added.

Actor motion broadcasts also reach rigid weapon instances. If the SDK returns no control and its track-group lookup confirms that the clip does not target the model, the provider returns the explicit `NoMatchingTracks` no-op status. A failed control creation for a matching model remains `EvaluationFailed`. Direct original-asset tests cover start, change and copy using a rigid weapon with Warrior wait/run clips; the source of an unmatched copy continues playing, preserving the original early-return behavior.

Pose output is a borrowed sequence of unchanged row-vector composite matrix values. Bone-world matrix access is distinct from composite/inverse-bind pose output. Attachment matrices pass through unchanged. The GPU palette consumer copies/validates the same matrix values as before.

`MeshBinding` supplies stable bone remaps and encapsulates CPU deformation. Its provider implementation calls the existing SSE function or the existing SDK deformer according to the caller's CPU feature decision. `EterGrnLib/Deform.cpp` remains unchanged; only its compilation ownership moves into the provider target to avoid a circular module dependency. Bindings are persistent instance resources, not per-draw objects.

## Explicit legacy interop

The sole SDK include is `AssetRuntime/Granny/Native.h`. Native conversions and reference-test factories are under `AssetRuntime/Granny/GrannyInterop.h`. `CreateLegacyAnimationHandle` deliberately borrows the reference fixture's animation; that helper is not an asset-loading fallback and its caller must keep the original fixture alive.

The previous native constructors and raw-data extraction helpers remain useful for independent old/reference tests. They must not be selected when a valid neutral provider handle exists. CPU deformation internals and the SDK-specific provider are intentional implementation dependencies; game and renderer consumers receive neutral handles, metadata, animation instances, poses and mesh bindings.

The executable provider-substitution test is the evidence for extending this boundary: a provider without native SDK objects must construct an actual production model and model instance and exercise geometry, pose and rendering-consumer setup. A metadata-only test would not establish that claim. The main report records the actual result.
