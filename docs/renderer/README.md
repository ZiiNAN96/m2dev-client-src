# Production Renderer: Diligent D3D11

Current architecture after M13C:

```text
Client / existing asset, animation and UI producers
  -> Math values + CPU texture/buffer sources + CDrawState snapshots
  -> existing Renderer interfaces
  -> Diligent D3D11
```

There is one production renderer. No native D3D9 backend, device, draw dispatcher,
GPU resource wrapper, matrix stack, optional OFF build or fallback exists.
Granny runtime, CPU skinning, gameplay, asset formats and Python interfaces remain.

## Build / dependencies

Windows x64, Visual Studio 2022 with C++ and ATL, current Windows SDK, CMake and Git.
DirectXMath is header-only in the Windows SDK; it does not require the old DirectX SDK
or a runtime DLL. Packed project Math values remain unaligned floats, with row vectors,
row-4 translation, RH camera projections and the previous quaternion composition order.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel 8
cmake --build build --config Debug --parallel 8
ctest --test-dir build -C Release --output-on-failure
ctest --test-dir build/tests/Renderer -C Debug --output-on-failure
```

DiligentCore is pinned to b036337d68be2353c9950a85929acf796b9a6d50 (v2.5.6).
The initial configure downloads it unless a source is already available. For an offline
checkout use CMake's `FETCHCONTENT_SOURCE_DIR_DILIGENTCORE` override with that revision.
Only its D3D11 engine is built. The project keeps its original CPU requirement; no
optional AVX2 requirement is added.

`M2_BUILD_RENDERER_TESTS` controls tests/isolated diagnostic fixture entry points only.
There is no `M2_ENABLE_DILIGENT_D3D11` switch anymore. An obsolete cache entry has no
routing effect and can be removed from the cache; do not use it as a build matrix.

Granny, SpeedTree, Python, audio and the other existing dependencies remain. The
`WindowsInput` interface target links dinput8/dxguid, not d3d9/d3dx9. Its historical
`extern/library/DirectX` folder name does not imply a graphics SDK requirement.
DirectShow and CMovieMan's DirectDraw video path are independent existing video
components. Their Windows SDK d3dtypes/d3dcaps declarations are not D3D9 SDK headers.

## Start / configuration

Default: run `Metin2_Release.exe` in the existing runtime directory.
Explicit equivalent: `--renderer=d3d11`.
Skinning defaults to GPU after B6-X. `--skinning=gpu` selects it explicitly;
`--skinning=cpu` retains the native CPU reference/fallback path. The old
`--skinning=gpu-prototype` spelling is a compatibility alias only. Conflicting or
unknown skinning options fail startup; there is no runtime skinning hot switch.
An unsupported individual asset may use the checked native CPU path, recorded by
`GPUFallbacks` and a bounded diagnostic. See the [B6-X report](phase-b6x-gpu-skinning-production.md)
for measured performance and final acceptance status.
The old `--renderer=diligent-d3d11` spelling is accepted solely as an alias for the
same API. No Legacy value is accepted; unsupported values produce an error and exit 2.
There is no hot switch and no automatic fallback.

`WINDOWED 1` remains required. Dynamic shadows remain deliberately unsupported;
M13C does not add new rendering features. Existing shadow settings do not allocate
a native shadow surface.

## Ownership / developer notes

- `Math/Math.h`: packed vectors, matrix, quaternion, plane, color and value-owned stack.
- `Renderer/DrawStateTypes.h`: only used CPU material/state vocabulary and layouts;
  asset-facing numeric material values are retained.
- `EterLib/DrawState.h`: CPU state scopes, transforms, lights, viewport and texture owners.
  No shader/device/stream/index/draw facade. Renderers consume material snapshots.
- `EterLib/TextureBinding.h` and `Renderer/ResourceData.h`: shared CPU texture sources
  and checked CPU buffers, with actual source-owner counters.
- `EterImageLib/DDSImageData.h`: bounded, borrowed CPU DDS mip views; no GPU allocation.
- Existing Diligent renderers own GPU resources. Always release map/material/actor
  handles before their renderer/device owner.

Asset decode is not a second backend. Missing/unsupported sources keep the existing
explicit failure behavior; they do not create a native fallback texture.

## Tests / diagnosis

The renderer tests cover startup policy, 653 analytic Math checks, DDS/data bounds,
CPU state isolation, retained texture ownership, actor draw-order permutations,
GPU pixels/depth, UI/text and lifecycle. The full suite also includes dependency tests.

`AuditGraphicsDependencies.ps1` inspects actual compiler include tracking, the client
link command and application-owned binary imports in each configuration. OS/API-set/
driver internals are reported separately and are not treated as client libraries.

```powershell
./tests/Renderer/AuditGraphicsDependencies.ps1 -Configuration Release -OutputDirectory build/milestone13c/dependency-audit
./tests/Renderer/AuditGraphicsDependencies.ps1 -Configuration Debug -OutputDirectory build/milestone13c/dependency-audit
```

`renderer-startup.log`: selected API and final exit status.
`source-resource-audit.log`: actual remaining CPU texture/buffer owners after shutdown.
Renderer diagnostic logs include the remaining GPU owners; diagnostic enablement is
private to the test fixture, not a permanently verbose production setting.

For full-suite Zstandard shell tests on this Windows machine, put Git's `usr/bin`
before Cygwin in the **test process's** PATH; do not change the user's global PATH.
Prebuilt Python/zlib PDB warnings are unrelated to graphics dependencies.

Private fixture setup defaults to M13C and creates fresh directories, leaving original
packages, runtime and other clients unchanged. Login, account input and Windows
security confirmations are performed by the user.

## Historical material

M1–M13B reports, their CSV audits, old fixture scripts and packaged external SDK artifacts
are historical evidence, not current build/setup instructions. The unbuilt native
comparison program and its exclusive check headers were removed; Git retains them.
The active production GPU tests remain.

See [initial M13C inventory](milestone13c-audit.md) and [M13C final report](milestone13c-report.md)
for the actual validation status and remaining manual boundaries.
