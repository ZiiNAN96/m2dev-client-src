# C-LIB-X comparison tools

These are isolated measurement programs. `M2_BUILD_CORE_LIBRARY_AUDIT` defaults
to `OFF`; none of their dependencies are linked to the production client.
See [the decision report](../../docs/core/phase-clibx-library-reuse-audit.md).

## Reproduce on the measured Windows x64 environment

Run from the source checkout. Requirements: CMake, Visual Studio 2022 C++/Windows
SDK, Python 3.12+ for archive extraction, and Python with Pillow/NumPy for image
preparation. The measured environment used Python 3.14.7, Pillow 12.3.0 and
NumPy 2.3.5. Network is needed only for the pinned source archives.

```powershell
python tools/CoreLibraryAudit/prepare_dependencies.py
cmake -S build/clibx/deps/basis_universal-2_50 -B build/clibx/basis-build -G "Visual Studio 17 2022" -A x64 -DBASISU_EXAMPLES=OFF -DBASISU_OPENCL=OFF -DBASISU_BUILD_PYTHON=OFF -DBASISU_SSE=ON
cmake --build build/clibx/basis-build --config Release --target basisu --parallel 6
cmake -S . -B build/clibx/animation-build -G "Visual Studio 17 2022" -A x64 -DM2_BUILD_WINDOWS_CLIENT=OFF -DM2_BUILD_RENDERER_TESTS=OFF -DM2_BUILD_PLATFORM_TESTS=OFF -DM2_BUILD_CORE_LIBRARY_AUDIT=ON
cmake --build build/clibx/animation-build --config Release --target CoreAnimationAudit CoreMeshAudit CoreTextureAudit --parallel 6
& build/clibx/animation-build/tools/CoreLibraryAudit/Release/CoreAnimationAudit.exe ../m2dev-client/assets tools/CoreLibraryAudit/animation-corpus.tsv 60
& build/clibx/animation-build/tools/CoreLibraryAudit/Release/CoreAnimationAudit.exe ../m2dev-client/assets tools/CoreLibraryAudit/animation-corpus.tsv 240 quality-only
& build/clibx/animation-build/tools/CoreLibraryAudit/Release/CoreMeshAudit.exe ../m2dev-client/assets tools/CoreLibraryAudit/mesh-corpus.tsv
python tools/CoreLibraryAudit/texture_audit.py prepare
python tools/CoreLibraryAudit/texture_audit.py run
python tools/CoreLibraryAudit/texture_audit.py quality
```

Use the Python executable containing Pillow/NumPy for the last three commands.
Capture stdout as JSONL for native tools and stderr separately; require exit 0.
Run measurement processes sequentially without concurrent builds or games.
The texture helper retains its JSON, command lines, encoded files, images and
logs under `build/clibx/textures`. The native texture target imports the upstream
Release encoder archive, so this proof is currently Windows Release only.
No claim of a tested Android port follows from the platform-neutral algorithms.

The sibling `m2dev-client` must contain the corpus paths in the TSV files.
The tree case also requires the existing H-X conversion at
`build/hx/compiled/vegetation/ymir work/zone/b/tree/1/baobab_rt.glb`.
That generated tree is identified by hash in the retained evidence; it is not
silently replaced by a synthetic tree on a fresh checkout. The animated GLB,
GLB basecolor and normal-map probes are explicitly supplemental fixtures.
They do not satisfy production-asset coverage for those categories.

## Interpretation

- Animation: 64 warm iterations, 31 repetitions, each 32 frames (8 for 100
  actors); median/P95/max are statistics of average frame batches. Actor counts
  are groups, not per-actor timings. Setup/import/compression are separate single
  observations. Counters cover global ordinary C++ allocation and the supplied
  ozz/ACL allocators, not whole-process memory. Payload/capacity sizes are not RSS.
- ACL: medium/default compression, precision 0.001 client units, shell distance
  100, uniformly resampled source. Quality compares the native source against
  decoded poses and world matrices; extra fields isolate resampling/projection
  from quantization. This converter is a proof, not a production motion format.
- ozz: source keys enter the default offline builder without a separate
  optimizer; TRS projection and default storage precision are part of the
  measured proposal. Full scale/shear, STEP, events/root motion and production
  attachments remain integration obligations.
- Mesh: cache/fetch baseline is already used by AssetTool. Extra overdraw is an
  analyzer proxy, not GPU render time. Codec roundtrip checks preserve vertices
  and triangle winding; legal cyclic index rotations are accepted. Simplification
  measurements on skinned/alpha meshes are never production outputs.
- Texture: all existing DDS mip levels are decoded into lossless RGBA DDS input;
  they are neither resized nor regenerated. Both original and KTX2 payloads are
  measured with the existing pack's Zstd level 17. Pack metadata/encryption
  overhead is excluded equally. Transcode targets are CPU capabilities, not
  verified device support. GPU block byte counts are not measured VRAM allocation.
  `memory_load_prepare` includes parse/start/transcode with preallocated output;
  original comparison times are pack decompression only, not a full image load.
  PSNR `null` means exact RGB equality. Normal quality uses reconstructed BC5
  vectors and angular error, not RGB PSNR. Color quality is measured in encoded
  byte space and does not prove correct GPU gamma handling.

Do not distribute production assets with these tools. Dependency archives,
generated images, binaries and logs remain in the ignored build directory.

## C-LIB-X-C targeted closure

Run only the affected targets and helpers; do not repeat the original full audit
or production gates. `closure_animation.py` selects six existing GLB clips at
60/240 Hz, four GR2 controls, three allocator-pattern repeats and one GLB warm
case at 1/20/100 actors. It checks process success, finite recorded metrics,
stable payload/error fields, hierarchy permutations and zero warm allocations.

```powershell
cmake --build build/clibx/animation-build --config Release --target CoreAnimationAudit CoreTextureAudit --parallel 6
python tools/CoreLibraryAudit/closure_animation.py
```

ACL input tracks now follow `RuntimeSkeleton::EvaluationOrder()`, while
`output_index` preserves original native bone IDs. Parent indices use the inverse
map. `CLIB_CHECK_METRIC_ORDER=1` diagnoses full-pose metric calls that read a
parent before it was written; it fails explicitly rather than suppressing a
native exception. `CLIB_POISON_ACL_ALLOCATIONS=1` fills supplied ACL allocations
with byte 0x7f, a diagnostic pattern, without changing compression parameters.
Both diagnostics are off by default. A/B/C and ozz quality now use the same dense
grid plus all translation/rotation/scale key times and +/- 1 microsecond.
Original GLB ACL measurements are invalidated; see closure evidence and report.

### Authored PBR input

The closure uses Microsoft's CC0 FlightHelmet from the official Khronos sample
repository, pinned at `d7a3cc8e51d7c573771ae77a57f16b0662a905c6`. This is an
external production-near static PBR model, not a Metin2 character or an animated
production GLB. Fetch the existing KTX2 variant (already containing mip chains):

```powershell
$closureDir = 'build/clibx/closure'
$sampleUrl = 'https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Models/d7a3cc8e51d7c573771ae77a57f16b0662a905c6/2.0/FlightHelmet'
New-Item -ItemType Directory -Force -Path $closureDir | Out-Null
Invoke-WebRequest "$sampleUrl/glTF-KTX-BasisU/FlightHelmet.gltf" -OutFile "$closureDir/FlightHelmet.gltf"
Invoke-WebRequest "$sampleUrl/README.md" -OutFile "$closureDir/FlightHelmet-README.md"
$sampleDoc = Get-Content "$closureDir/FlightHelmet.gltf" -Raw | ConvertFrom-Json
foreach ($sampleFile in @($sampleDoc.buffers.uri) + @($sampleDoc.images.uri)) {
    Invoke-WebRequest "$sampleUrl/glTF-KTX-BasisU/$sampleFile" -OutFile "$closureDir/$sampleFile"
}
python tools/CoreLibraryAudit/closure_textures.py all
```

Use the Python executable with Pillow/NumPy. The helper has separate `prepare`,
`run`, and `quality` phases. It packages the unchanged geometry and all 15
original KTX2 images into a GLB, then reads it back and byte-verifies the payloads
and unchanged material/scene data. Source and GLB hashes are retained in
`build/clibx/closure/textures/model.json` and the closure evidence.

Only four material textures are compared: LeatherParts basecolor, normal and
occlusion/roughness/metallic, plus Lenses basecolor/alpha. The original KTX2
mips become a **derived BC7 DDS baseline**, not a claimed production-pack DDS.
Both candidate codecs receive the identical lossless RGBA decode of that DDS's
stored mips. No new mip generation, resize, orientation or material edits occur.
BC7 UNORM retains all three authored normal components, including rare negative
Z values; a positive-Z BC5 reconstruction would change those values. Normal
quality measures normalized RGB vectors interpreted linearly. Normal/data use
linear KTX2 metadata; basecolor uses sRGB. Alpha remains linear. The normal-map
encoder preset does not regenerate or normalize the provided mip images. ORM
does not receive a normal-map preset or channel swizzle.

Native helper interfaces:

```text
CoreTextureAudit --extract-baseline original.ktx2 prefix normal|color|data
CoreTextureAudit candidate.ktx2 baseline.dds prefix BC7
```

The optional final target limits transcoding to the directly affected format.
The closure compares ETC1S q255/level1 and UASTC level2, single-threaded, retaining
all 12 leather / 11 lens mip levels. Both container alternatives receive the
existing pack's Zstd level 17. BC7 block payload bytes are equal. KTX2 transfer
metadata and derived baseline DDS formats are validated; diagnostic transcoded
DDS outputs are UNORM byte-space views, not production sRGB GPU views. Timings
cover CPU preparation only, excluding candidate outer-pack decode and GPU upload.
All-mip image/alpha/channel/angular metrics and a visual contact sheet stay in
the ignored output directory. This is not in-game/device visual acceptance.
