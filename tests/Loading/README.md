# P0-L native load probe

Opt-in diagnostics only. The normal client does not enable these timers or use
this private Python entry point. `app.MapLoadTrace('enable')` opts in;
`begin(label)`, `push(phase)`, `pop`, `active` and `end(reason)` delimit capture.
`M2_MAP_LOAD_TRACE=1` is an alternative enable switch, but an explicit `begin`
is still required. Elevated Windows launches may not inherit the environment.

The fixture uses original packed assets, current original-client configuration,
the real map/actor/renderer paths and a private replacement `prototype.py`.
The production root pack is never overwritten. Cold means first A1 load in a
fresh process; the OS file cache is uncontrolled. B1 is only an optional first
B1 load in the same session, then a warm reload.

From a Visual Studio x64 developer shell, build only the relevant Release:

```powershell
cmake --build build-h2x/msvc --config Release --target UserInterface --parallel 6
& tests/Loading/prepare_probe.ps1 -Name chosen-fresh-name
& tests/Loading/run_probe.ps1 -Name chosen-fresh-name
python.exe tests/Loading/summarize_trace.py build-p0l/chosen-fresh-name
python.exe tests/Loading/verify_probe.py build-p0l/chosen-fresh-name
```

`prepare_probe.ps1` expects the existing `PackMaker.exe` in that build. The
native client retains its existing administrator requirement. Start it through
the authorized desktop environment; do not change its manifest.

`map-load-trace.tsv` records inclusive and exclusive timings and per-file work
counters. Exclusive sums include an explicit unattributed remainder and must
equal total elapsed time. Only the main thread is captured. GPU entries time
CPU calls; pack reads are logical mapped payload reads, not physical disk reads.
Native asset names can contain legacy non-UTF-8 bytes, so the parser preserves
them losslessly using Latin-1 when reading the TSV.

The endpoint is three consecutive presented world frames no longer than 50 ms
with no new recorded file/parse/upload/compile work. Screenshots and the short
smoke follow that endpoint; screenshot readback waits are not loading costs.
Network login, complete game UI and manual acceptance are outside this probe.

Do not repeat the route as a statistical benchmark. The completed investigation,
the one decoder fix and its evidence are documented in
[the P0-L report](../../docs/performance/p0l-map-loading.md).

## P0-L2 shader lifecycle

Use `-ShaderLifecycle` for the short Client -> A1 cold -> B1 -> A1 warm route:

```powershell
& tests/Loading/prepare_probe.ps1 -Name shader-fresh-name -ShaderLifecycle
& tests/Loading/run_probe.ps1 -Name shader-fresh-name
python.exe tests/Loading/summarize_trace.py build-p0l/shader-fresh-name
python.exe tests/Loading/summarize_shader_trace.py build-p0l/shader-fresh-name
python.exe tests/Loading/verify_probe.py build-p0l/shader-fresh-name --shader-lifecycle
```

The same opt-in capture now includes central Diligent compiler/PSO calls,
embedded-source reads, SRBs and existing native/Modern caches. Configure-time
copies of three pinned Core source files preserve the vendor checkout.
`COST` has an optional eighth column, maximum inclusive milliseconds for a
single call. Older captures remain readable. Shader bytecode equality uses
the full bytes; descriptor/input hashes are diagnostics, never production
cache keys. The capture is synchronous main-thread work, not a GPU timer.

The focused pass-equivalence check uses the Windows HLSL compiler, no GPU or
game assets. From the same Visual Studio x64 developer shell:

```powershell
cl /nologo /std:c++17 /O2 /EHsc /I src tests/Loading/mesh_vertex_equivalence.cpp /Fe:build-p0l2/mesh_vertex_equivalence.exe /Fo:build-p0l2/mesh_vertex_equivalence.obj /link d3dcompiler.lib
& build-p0l2/mesh_vertex_equivalence.exe build-deps/DiligentCore build-h2x/msvc/src/Renderer/fx-subset
```

It checks all 36 existing geometry/tangent/instance/pass combinations and
compares complete bytecode, including reflection. See the
[P0-L2 report](../../docs/performance/p0l2-shader-loading.md) and
[runtime shader inventory](../../docs/performance/p0l2-runtime-shaders.md).
