# Renderer milestone 1

> Historischer Milestone-Stand. Aktuell seit [M12](renderer-milestone12.md): Diligent D3D11 ist im ON-Build der Default; Legacy wird explizit mit `--renderer=legacy-d3d9` gewählt. OFF-Builds bleiben Legacy-only. Frühere Testergebnisse und Auswahlbeschreibungen unten gelten für ihren damaligen Stand.

Historical milestone-1 description. With milestone 2 applied, normal
`--renderer=diligent-d3d11` starts the game and its experimental terrain presentation.
The isolated clear/present bootstrap now requires `--renderer-smoke-test`.
See [milestone 2](renderer-milestone2.md) for current behavior and restrictions.

The normal client still uses D3D9Ex. This milestone introduces only the device/frame
lifecycle boundary. Existing terrain, characters, effects, Granny, UI and draw calls
are not ported.

## Build and start

From the client-source directory, using Visual Studio 2022 C++, the Windows SDK and ATL:

```powershell
# Legacy-only build: no Diligent download or dependency.
cmake -S . -B build -DM2_ENABLE_DILIGENT_D3D11=OFF
cmake --build build --config Release --parallel 4

# Include the optional Diligent D3D11 backend.
cmake -S . -B build -DM2_ENABLE_DILIGENT_D3D11=ON
cmake --build build --config Release --parallel 4
```

DiligentCore is pinned to v2.5.6, commit
`b036337d68be2353c9950a85929acf796b9a6d50`, including its pinned xxHash submodule.
The initial opt-in configure requires network access. An already downloaded matching
checkout can be supplied with `-DFETCHCONTENT_SOURCE_DIR_DILIGENTCORE=<absolute-path>`.
Only D3D11 is enabled; the existing static MSVC runtime and baseline CPU requirement
are retained. Diligent's other graphics backends and AVX2 default are disabled.

Run the built executable from the normal runtime-data directory for the full game:

```text
Metin2_Release.exe
Metin2_Release.exe --renderer=legacy-d3d9
```

Either command starts the normal legacy game. The selection is parsed exactly once,
in `WinMain`; there is no runtime setter or hot-switching.

```text
Metin2_Release.exe --renderer=diligent-d3d11
```

This starts a resizable clear/present window, not the game. It runs before Python,
game resources and the legacy device are initialized. The close button shuts down
GPU resources before destroying the window. This path needs no runtime data.
An unavailable Diligent backend or conflicting/invalid backend options produce an
explicit startup error instead of silently falling back to another backend.

## Automated checks

Renderer tests are built by default; opt out with `-DM2_BUILD_RENDERER_TESTS=OFF`.
GPU tests need a Windows desktop session and an available D3D9Ex/D3D11 adapter.

```powershell
ctest --test-dir build -C Release -R 'Renderer\.' --output-on-failure
```

`RendererStartupOptionsTest` checks default/explicit selection and rejects invalid
or conflicting selections. `RendererSmokeTest` uses real back buffers and reads
pixels at the corners and center. It checks two clear colors, depth-only color
preservation, multiple resize extents, zero-size suspension, restore, invalid
lifecycle calls, idempotent shutdown and released resource ownership.

The actual client executable can also run a five-frame integration check:

```text
Metin2_Release.exe --renderer-smoke-test
Metin2_Release.exe --renderer=diligent-d3d11 --renderer-smoke-test
```

These hidden-window checks exercise the startup selection, an actual `WM_SIZE`
resize and shutdown. Exit code 0 means success. Diagnostics are written to
`renderer-bootstrap.log` in the working directory (overwritten per run).
Pixel readback is covered separately by `RendererSmokeTest`.

Existing Zstandard tests are also registered with CTest. Their full runs can exceed
two minutes; use an appropriate timeout, e.g. `--timeout 900`. On this Windows host,
`sh` resolves to Cygwin, whose `/cygdrive/...` paths are not accepted by the native
Zstandard executable in `playTests`. Run CTest with Git's `usr/bin` first in PATH
or run the unchanged `playTests.sh` with Git Bash. Do not change the vendor sources
to work around this local shell selection.

## Implementation boundary

- `IRenderBackend`: Initialize, BeginFrame, Clear, EndFrame, Present, Resize, Shutdown.
  Its public data types contain no DirectX or Diligent types.
- `LegacyD3D9Backend`: wraps the application's existing `CGraphicDevice` and `CScreen`.
  Device creation codes, debug/release clear behavior, scene/present calls and the
  existing resource-before-device destruction order are retained.
- `DiligentD3D11Backend`: owns only a device, immediate context and swap chain.
  Implementation types are hidden from the common interface.
- `PythonApplication`: lifecycle delegation only. The existing Python-driven world
  and UI rendering order remains in place.
- `RendererBootstrap`: isolated Win32 clear/present loop for the new backend.

No terrain/static-object/character migration, GPU skinning, materials, shaders,
PBR, Vulkan, DX12 or Granny changes are part of this milestone. GPU device-loss
recovery for Diligent and visual game parity belong to later work.

Upstream reference: [DiligentCore v2.5.6](https://github.com/DiligentGraphics/DiligentCore/tree/v2.5.6).
