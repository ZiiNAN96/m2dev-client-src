# B6-X — GPU skinning performance and production decision

Status: **GO — B6-X fully accepted on 2026-09-13. GPU skinning is the production
default in the new build; native CPU skinning remains explicit fallback/reference.**
Baseline: `e9a7bf4` (B5-X). Granny SDK and native CPU deformer remain unchanged.

## Predeclared method and gates

Before any optimization/default change, collect two complementary measurements:

1. Isolated animated actor benchmark: production native pose/deformation and Diligent
   actor draws, 1/10/25/50/100 instances, fixed 1280x720 target, camera, full-detail
   meshes, deterministic 1/60-second animation clocks and phase offsets. Cycle an
   original warrior, goods NPC, wolf and horse; real animation clips. A diagnostic
   diffuse texture controls material/pixel workload. This is **not a map/game FPS
   measurement**. No readback, resize or validation inside timed frames. Warm up
   resources/shaders first. Release without debug layer; test-only Present(0).
   GPU duration queries collected after each block, without per-frame CPU waits.
   Alternating CPU/GPU order across three repetitions controls order/thermal bias.
2. Identical original-map client benchmark, same camera/actors/settings/resolution,
   VSync explicitly documented. World timings must corroborate the isolated result;
   capped FPS is not evidence of equal processing cost. Log/map-load/startup work
   must be separated from steady-state samples. No concurrent benchmark clients.

Report per-frame raw data and average/median/p95/p99/max, measured vertex vs bone
uploads, actual CPU deformation vs GPU preparation, draw parity and live sharing.
Percentiles use sorted empirical samples (p95/p99 rounded up); the median uses the
lower central observation for an even sample count. Means weight frames equally.
Separate GPU command duration from CPU execution and wall/present frame duration.
Do not represent upload payload counts as measured PCIe traffic, nor actor-only
GPU duration as whole-game GPU time.

Predeclared production gates:

- B5-X parity/stability accepted; all existing tests remain required.
- Zero unexpected fallback, binding/remap/palette/validation failures; all monitored
  owners zero after shutdown; ordinary existing categories retain coverage.
- Supported GPU actors execute zero CPU deformation; draw counts match.
- At 50 and 100 actors CPU skin/prep cost decreases reproducibly (>5% in at least
  two of three paired repetitions), or an explicitly justified architectural
  advantage with no material total-frame regression.
- A reproducible >10% total-frame regression is a NO-GO pending explanation/fix.
  GPU regression exceeding **both** 20% and 0.5 ms is a NO-GO pending explanation/fix.
  Investigate p95/p99 regressions separately; do not hide them with averages.
- Verify default-mode world, relog, hair LOD, window lifecycle and CPU override
  after a provisional switch; revoke it if final validation fails.

Only measured low-risk optimization is allowed. No coverage/architecture expansion.
If gates cannot be met or evidenced, CPU stays default and blockers are recorded.

## Results / final decision

**GO: GPU skinning is Production Default.** The predeclared performance gates,
post-switch default/CPU regressions, 25-minute lifetime run, Release/Debug builds
and complete required test suites passed. `--skinning=cpu` remains available.
This accepts the existing B4-X/B5-X coverage, not unsupported new assets or an
unmeasured hardware performance guarantee. No Phase C work or approval.

## 1–10. Final performance evidence

Measured 2026-09-13 on AMD Ryzen 7 9800X3D / NVIDIA GeForce RTX 5070 Ti, Windows,
Release x64, D3D11 debug layer disabled. No benchmark clients/builds run concurrently
in decision samples. Single-machine evidence, not a lower-end GPU guarantee.

Isolated: `build/phase-b6x/production-cpu-baseline.csv` and `...-summary.json`,
three alternating repetitions, each 120 warmup + 600 measured frames per mode/load.
Native capability detection reports SSE2 enabled. All four assets animate through
native clocks, full-detail warrior/goods/wolf/horse, original material groups with
one diagnostic diffuse texture. 18,000 measured frames, no numeric/raster readback
in measured frames. 1280x720, Present(0), fixed camera, no map/LOD. CPU frame below
means CPU work before Present, separately for the two skinning modes.

| Actors | CPU skin ms | GPU prep ms | CPU upload bytes | GPU bone bytes | CPU work CPU/GPU ms | Device duration CPU/GPU ms | Throughput CPU/GPU FPS |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | .00662 | .00238 | 70,624 | 16,384 | .0583 / .0199 | .00556 / .00531 | 3,694 / 3,835 |
| 10 | .03985 | .01348 | 485,920 | 163,840 | .3433 / .0847 | .01899 / .01044 | 1,514 / 3,357 |
| 25 | .09764 | .03543 | 1,209,952 | 409,600 | .8343 / .2040 | .05088 / .02266 | 942 / 1,951 |
| 50 | .18666 | .06738 | 2,384,800 | 819,200 | 1.6191 / .3747 | .10609 / .03172 | 595 / 1,444 |
| 100 | .37137 | .14007 | 4,747,200 | 1,638,400 | 3.2122 / .7320 | .22688 / .06065 | 309 / 1,012 |

These very high isolated throughput numbers are **not gameplay FPS**. Present/OS
scheduling is included in throughput, excluded from CPU-work columns. GPU duration
is a timestamp interval around submitted clear/draw commands, not an isolated
vertex-shader ALU measurement: uploads/command starvation can make CPU-path GPU
intervals longer. Do not infer that skinning arithmetic is free on every GPU.

Upload reductions: 76.8%, 66.3%, 66.1%, 65.6%, 65.5%. GPU CPU-vertex uploads are zero;
bone bytes count the entire written 16 KiB CB, including zero tail, not just useful
matrix bytes and not measured PCIe bandwidth. Skin/prep reduction at 50/100 is
63.9% / 62.3%, saving .1193 / .2313 ms, reproduced in all three repetitions. Native
pose sampling and world-matrix work are in deformation/frame totals, not prep-only.
Actor render CPU means: 1/10/25/50/100 CPU .0411/.2550/.6263/1.2307/2.4534 ms;
GPU .0084/.0334/.0807/.1499/.2927 ms. The existing CPU path additionally validates
and copies full dynamic PNT buffers; no such CPU-path checks were weakened.

World: `world-cpu-fixed-01`, `world-gpu-fixed-02`, `world-gpu-fixed-03`,
`world-cpu-fixed-02` in `build/phase-b6x/`. Sequential CPU/GPU/GPU/CPU. Each 2,400
frames (120 warmup + 360 measured per load); 7,200 measured frames total. Fixed
original Map1 at (44000,27200), 1280x720, camera (6000,55,0), original world/textures,
24,000 sight range, native LOD retained identically, no dynamic shadows. Mix consists
of warrior, goods NPC, wolf, mounted warrior/horse; requested count includes mounts.
Each measured frame has exactly the requested visible body count. Native fixed
16/17 ms steps, fixed spawn positions; VSync remains Present(1). No active effect
draws in this benchmark; native world effect simulation remains present. Warmup,
asset/spawn and shutdown samples are excluded, not silently mixed into means.

| Bodies | CPU skin / GPU prep ms | CPU vertex / GPU bone bytes | Whole CPU work CPU/GPU ms | Whole GPU interval CPU/GPU ms | Wall/FPS |
|---:|---:|---:|---:|---:|---|
| 1 | .00478 / .00387 | 25,152 / 16,384 | .4328 / .4551 | .1046 / .1095 | both ~16.50 ms / 60.61 |
| 10 | .03315 / .01975 | 304,448 / 163,840 | .6360 / .5292 | .1005 / .1098 | both ~16.50 ms / 60.61 |
| 25 | .07186 / .05034 | 761,120 / 409,600 | .9334 / .7771 | .1193 / .1575 | both ~16.50 ms / 60.61 |
| 50 | .15691 / .09471 | 1,658,656 / 819,200 | 1.5700 / .9908 | .2561 / .2049 | both ~16.50 ms / 60.61 |
| 100 | .31139 / .20026 | 3,499,200 / 1,638,400 | 2.8166 / 1.7448 | .6945 / .4096 | both ~16.50 ms / 60.61 |

| Bodies | World skin/prep CPU saving ms | Saving % | World upload reduction % |
|---:|---:|---:|---:|
| 1 | .00091 | 19.0 | 34.9 |
| 10 | .01340 | 40.4 | 46.2 |
| 25 | .02153 | 30.0 | 46.2 |
| 50 | .06220 | 39.6 | 50.6 |
| 100 | .11114 | 35.7 | 53.2 |

World actor-render CPU means for 1/10/25/50/100 bodies: CPU
.0195/.1414/.3361/.7365/1.5584 ms; GPU .0188/.0600/.1446/.2607/.5933 ms.

At 50/100 world actors: whole CPU work improves 36.9% / 38.1%, .579 / 1.072 ms;
upload savings 50.6% / 53.2%. At one actor ~.022 ms CPU overhead/no useful FPS gain;
at 25 actors GPU interval increases ~.038 ms (32%) — below the predeclared .5 ms
absolute threshold. No high-load GPU bottleneck was observed on this adapter.
Capped FPS is unchanged by design, not evidence of unchanged CPU headroom.

### Frame-time tails

| Actors | Isolated CPU-work median CPU/GPU ms | p95 CPU/GPU ms | p99 CPU/GPU ms | max CPU/GPU ms |
|---:|---:|---:|---:|---:|
| 1 | .0508 / .0164 | .0877 / .0335 | .1518 / .0583 | .2806 / .1978 |
| 10 | .3248 / .0730 | .4546 / .1305 | .5254 / .1726 | .7793 / .3428 |
| 25 | .8070 / .1884 | 1.0175 / .2934 | 1.1756 / .3409 | 1.2706 / .5832 |
| 50 | 1.5708 / .3555 | 1.9274 / .5053 | 2.3119 / .6203 | 2.4766 / .8440 |
| 100 | 3.1341 / .7091 | 3.6465 / .8820 | 4.5660 / 1.1365 | 6.6166 / 1.3246 |

Do **not** claim every total spike improves: isolated Present(0) wall p99 at 25 rises
3.851→4.872 ms and at 50 rises 2.910→4.383 ms, while CPU-work p99 falls strongly.
This difference is in Present/scheduling, not hidden deformation. The normal-world
confirmation has 50-body CPU p99 2.258–2.510 ms vs GPU 1.421–1.470, and 100-body
4.192–4.453 vs 2.714–2.754 ms. One-body world CPU tails fluctuate (.683–.694 vs
.743–.991 ms); no blanket low-load spike improvement. Raw means/medians/p95/p99/max
for all metrics and repetitions remain in each `summary.json`.

## 11–19. Draws, sharing, buffers, switch

Draws per frame match exactly: isolated 3/22/57/112/225, world 3/18/45/90/180.
Original groups differ between fixtures because world uses native LOD/materials.
GPU does not add draws. All benchmark assets have deformable draw groups; GPU draws
use static PWNT input plus current bone buffer. Isolated sharing assertions require
1/4/4/4/4 immutable mesh sets and 1/10/25/50/100 individual pose buffers. Existing
B4/B5 crowd tests additionally require **one** shared mesh set for 50 identical wolves.

Steady CPU calls/updates: one per body (1/10/25/50/100); GPU: zero native CPU calls
and zero dynamic vertex updates, same number of palette writes. No per-frame mesh
allocation. Native matrices still come from Granny. Linked-hair owner/revision reuse
is retained unchanged and revalidated by existing tests. Constant-buffer strategy
is adequate for measured costs; **no performance optimization/rewrite was applied**.

All four final world runs: exit 0, preparation/fallback errors 0, monitored source,
skin, remap, palette, geometry and static-mesh owners 0 after shutdown. Each GPU world
run executed 89,380 GPU deformations and **0 all-native CPU deformation calls**.

Pre-switch performance gates pass; no >10% mean total regression or critical GPU
interval regression. CPU skin/prep improves in all three 50/100 paired repetitions;
the full world confirms CPU benefit. These results first authorized a provisional
default switch; the mandatory post-switch acceptance below has now passed as well.

Production policy has one compile-time default (`productionSkinningMode=GPU`), used
by global startup state and parsed options. `--skinning=gpu` is the production name;
`--skinning=cpu` is an explicit native override. `gpu-prototype` remains an accepted
compatibility alias, not the logged production name. Repeated equivalent values
are allowed, conflicting/unknown values rejected. No hot switching or silent CPU
startup default. Per-asset checked fallback remains unchanged. Historical internal
Prototype names remain to avoid unrelated churn. New significant integration is
marked `ZiiNAN`.

## 20–37. Final production acceptance

### Normal-client acceptance completed

Both use the final Release binary and **original packages**, each in a fresh private
runtime. Visual checks below are user-confirmed; mode, world transitions, counters,
exit codes and ownership are log-verified. No credentials or server state changed
by the test tools.

| Run | Observed mode | Visual / lifecycle result | Exit / duration |
|---|---|---|---|
| `gpu-default-normal-01`, PID 40644 | `Skinning=gpu`, `SkinningSelection=default`; no skinning argument | Selection, player/armor/hair/weapon, NPC/mobs, animations, world/effects/UI/quest text passed. Genuine selection return and re-entry verified, then hair/weapon/animation, resize and minimize/restore, X shutdown passed | 0 / 248.4 s |
| `cpu-explicit-normal-01`, PID 32224 | `--skinning=cpu`, explicit | Selection, player/armor/hair/weapon, NPC/mobs, mount, resize/minimize/restore passed; ordinary exit passed | 0 / 56.1 s |

The GPU player's logged race is 0 (male warrior). No new character-name verification
was requested. Logged main-body VID transitions are **0 → 22464 → 0 → 23450**, proving
selection → world → selection → a new world instance in one process. The explicit
CPU comparison has 0 → 23451; it is a short fallback regression, not another relog
claim. Other races and repeated hair near/far/near are covered by the automated
native comparisons and the separate default stability fixture.

GPU normal: 5,167,773 GPU preparation/deformation events, **0 native CPU deformation
calls/vertices**, 0 CPU vertex-upload bytes, 0 fallbacks. These are actor events, not
video frames. Logged peak visibility reaches 962 actors (947 mobs, 24 NPCs; maxima
need not coincide), with 997 actor geometries and 2 attachments. This is observed
normal-world population, not a controlled benchmark or a spawned test crowd.
No mount was observed in this GPU-normal log; GPU rider/mount coverage is checked
in the automated fixture instead. Explicit CPU: 394,475 native calls, 283,653,475
vertices, 9,076,911,200 cumulative upload bytes, **0 GPU preparation events**;
peak 591 actors and 1 mount.

Both runs: hair-binding, invalid-remap, stale-binding, palette, preparation and
actor-renderer errors **0**; all monitored source, mesh, remap, palette, geometry,
attachment and renderer owners **0** after shutdown. Each normal run contains one
pre-existing `GuildMarkManager invalid idx 0` message, unrelated to skinning; it is
not hidden or counted as a skinning success. Per-run `audit-summary.json` records
the full counters. Loading/crowd-related process memory peaks (GPU 961.7 MiB,
CPU 817.1 MiB) are not themselves a leak claim; repeated-map lifetime evidence is
tracked separately below.

### Automated acceptance

| Validation | Result | Evidence |
|---|---|---|
| Full Release build | Exit 0 | `build/phase-b6x-release-build.log` |
| Full Debug build | Exit 0 | `build/phase-b6x-debug-build.log` |
| Full Release suite, including four existing vendor tests and all renderer tests | **28/28 passed**, exit 0, 1,904.83 s | `build/phase-b6x-release-tests.log` |
| Debug renderer suite, B2/B3/B4-X/B5-X and new B6-X contract | **24/24 passed**, exit 0, 1,238.70 s | `build/phase-b6x-debug-renderer-tests.log` |
| Final PowerShell launcher/analyzer/auditor syntax; normal GPU/CPU and long-run audits | Passed | scripts and per-run `audit-summary.json` |

The final expected-draw assertion was additionally rebuilt for both configurations
before their suites (`phase-b6x-benchmark-final-release-build.log` and
`phase-b6x-benchmark-final-debug-build.log`). No existing test is removed or tolerance
widened. Existing Python/zlib missing-PDB linker warnings remain (LNK4099; Debug also
has existing LNK4075/LNK4098); successful builds are not described as warning-free.

Debug validation highlights (native D3D11 debug layer enabled, warnings 0):

- B4-X: 583 coverage cases / 762,998 samples; 163-bone boss included.
- Hair LOD: 224 cases / 75,005 samples; native pose delta 0, position maximum
  0.0000457764, normal maximum 0.000000178814.
- B5-X parity: 844 cases / 1,427,815 vertices, 1,070 palette readbacks,
  264 lifecycles; maximum position delta 0.000488281, unchanged tolerances.
- B5-X stress: 672 cases / 225,015 vertices; repeated hair, crowd sharing,
  fallback recovery and invalid-data rejection passed.
- Fallback parity: 8 deliberately forced fallbacks / 8 recovery cycles passed.
  These negative tests are not unexpected production fallbacks.
- B6-X real-asset benchmark contract passed, 1/10/25/50/100 CPU/GPU draw counts,
  zero GPU-path CPU deformation/uploads and static sharing asserted.

Every listed test reports resources/errors 0 at completion. Image comparison uses
the existing B5 tolerance rather than promising bit-identical edge pixels.
Release reproduces the same 583 coverage / 224 hair / 844 parity / 672 stress cases,
eight intentional fallback/recovery cases and B6-X benchmark assertions, all passed.
Release disables the graphics debug layer by design; the Debug suite supplies
the validation-layer evidence rather than treating an absent layer as validation.

The completed default stability fixture reuses B5-X coverage: all eight player races,
armor/hair/weapon swaps, NPC/mob/boss assets, rider/mount recreation, near/far LOD,
16/25/50/100-actor populations and two six-map cycles. It ran 12 × 125 seconds;
user confirmed repeated resize and three minimize/restore checks. The isolated
fixture does not log in or relog: that requirement is evidenced separately above.
Concurrent correctness suites during this stability run are **not** benchmark data.

`gpu-default-stability-01`, PID 52624: **exit 0, 1,503 seconds**, `Skinning=gpu`,
`SkinningSelection=default`, no skinning argument. All 12 phases completed:
**A1 → B1 → A1 → monkey dungeon → guild_01 → A1**, repeated twice. Observed peaks:
108 visible bodies (100 actors plus up to 8 mounts), 48 NPCs, 44 mobs,
130 actor geometries and 20 attachment geometries; maxima need not coincide.

- 5,369,651 GPU preparation/deformation events; 4,698,468 palette updates.
- **All native CPU calls/vertices, CPU upload bytes and unexpected fallbacks: 0.**
- Hair binding / invalid remap / stale binding / bone palette / preparation /
  actor-renderer errors: **0**. `syserr.txt` is empty.
- Source textures/buffers, skin meshes, remaps, bone palettes/buffers, static meshes,
  actor/attachment/mount geometry/textures, trees, effects, water/world, text/UI
  renderer owners after shutdown: **all 0**, checked by the audit script.
- 300 process samples, peak private memory **682.4 MiB**, last running sample
  **667.1 MiB**; peak handles 934, last 925. After startup, windows 125–750 s and
  875–1500 s span respectively 535.6–682.4 and 572.5–674.1 MiB, handles 921–933
  and 921–927. No monotonically growing/high-water increase observed. Different
  actor populations and asset caching mean equal memory at each map is not expected;
  this is a bounded 25-minute observation, not proof against every possible leak.

The long fixture exits itself; **X shutdown was separately exercised in the normal
GPU client**. Mounted recreation/shape stress does not substitute for a real relog;
the logged normal-client relog above supplies that evidence. All required scenarios
were covered across the automated and manual runs, not claimed as one continuous
25-minute interactive session.

### Delivery boundary / known limits

GPU default is compiled into `build/bin/Release/Metin2_Release.exe`. All final manual
and stability copies share SHA-256
`66E14D49A26F1970ACF6071FA675551F48F7672B3C698B78E25D8984F4B20806`.
The original sibling runtime executable/packages/configuration were not overwritten;
this milestone delivers a new build, not an unrequested deployment over that copy.
Build/log/raw CSV evidence resides in ignored `build/phase-b6x/`; the report and
reproduction tools are source changes. No commit or push was made.

Single-adapter performance, low-load overhead, isolated Present-related tail spikes,
retained checked CPU fallback and retained CPU fallback storage are explicit limits,
not concealed blockers. No recommendation to delete Granny/CPU reference tests is
made. Phase C can only be scoped separately after this B6-X acceptance; **no Phase C
implementation, asset-runtime rewrite, platform work or visual remaster is started**.

### Final gate decision

- Existing actor coverage/parity, hair/rigid attachments/mounts, native remaps and
  resource lifetime: passed; no coverage extension or new rendering feature.
- Supported default GPU actors: native CPU deformation **0**, unexpected fallback,
  binding/remap/palette errors **0**; all monitored shutdown owners **0**.
- Performance: measured CPU benefit at 50/100 actors, exact draw parity, lower
  dynamic upload payload and no critical GPU regression under the declared gates.
  Low-load overhead and Present-related tails remain explicitly documented above.
- Production selection/relog, window lifecycle, original world/UI/effects, explicit
  CPU reference and both complete required test suites: passed.

**B6-X complete — GO. Stop here. CPU is retained only as the checked per-asset
fallback and explicit diagnostic/reference mode. No Phase C is begun.**

## Measurement corrections (before decision)

- The initial `baseline.csv` isolated run used the fixture's uninitialized
  `CPU_HAS_SSE2=false`, unlike a normal client. **Excluded from production scoring**.
  The benchmark now calls the unchanged `CGraphicDevice::Create`, exactly like normal
  startup, and records CPU capability detection. No deformer implementation changed.
- First world trials allowed run-motion root displacement; mount visibility fell
  below the requested load. `world-cpu-01` and `world-gpu-01` are **excluded**. The
  fixed fixture restores all spawn positions each frame, retaining native animation.
- `world-gpu-fixed-01` is a setup check, not decision evidence. Final world comparisons
  run sequential CPU/GPU/GPU/CPU, without concurrent test clients or builds.

## CPU fallback / retained-code audit (source inspection)

| Condition | Current behavior | Expected production frequency / action |
|---|---|---|
| Explicit CPU startup | Native pose + original SIMD/Granny deformer, dynamic PNT upload | Deliberate diagnostic/reference mode; retain |
| Missing/unsupported static skin sidecar or vertex layout | GPU preparation rejects; current native CPU pose replaces GPU geometry | Not expected for covered assets; negative tests required |
| Missing/invalid native remap or destination mismatch | GPU preparation rejects; no stale GPU geometry reused | Not expected; B5 forced-remap fallback tests validate recovery |
| Missing/unready/nonfinite or >163-bone palette | GPU rejects, native path attempted where valid | Corrupt/unsupported assets, not new coverage; no success promised for invalid native data |
| GPU buffer/map/resource preparation failure | Preparation returns false; reset old GPU handle before CPU upload | Device/allocation fault, not a normal asset mode; keep diagnostics |
| Rigid weapon/hair submesh | Existing rigid vertex path with current Bone*World | **Not a skinning fallback**, no CPU vertex deformation |
| Native Hair-LOD rebinding failure | Current pose rejected before either deformation path | **Not a valid fallback**; must remain zero |
| Outside eligible actor scope | Existing native path; no forced coverage expansion | Account separately with all-call telemetry |

The new `AllCPUDeformationCalls/Vertices` counters sit at the model dispatch wrapper,
not in the native deformer. `GPUFallbacks` counts attempts (bounded error logs alone
previously counted only distinct reported instances). These counters do not create
or mutate animation data.

Retained CPU code:

- A, fallback: model/mesh deformation dispatch, `Deform.cpp`, CPU PNT storage and
  `DiligentActorRenderer::UpdateVertices`. Pose sampling, native mesh bindings and
  world matrices are shared prerequisites, **not** removable CPU skinning overhead.
- B, reference: B2 direct numeric deformation; B3/B4/B5 CPU/GPU pairing, fallback
  recovery and image comparisons; explicit CPU startup. Keep these tests.
- C, cleanup candidates only: B3 reference-asset fingerprint helpers and prototype
  names are compatibility/test debt, not proof of dead production behavior. Native
  CPU vertex storage is still allocated at instance creation for safe fallback;
  GPU frames skip filling/copying/uploading it. Do not remove it in this milestone.

No buffer pool/structured-buffer/SRB rewrite or new coverage is planned. The current
16 KiB dynamic constant buffer is reused per palette owner and revision; linked hair
shares its owner's buffer. Immutable mesh/remap combinations share weak-cached GPU
VB/IBs. Existing static-sharing and 163-bone boundary tests remain authoritative.

## Reproduction / diff scope

From the source checkout, after Release build, run measurements sequentially with
fresh output names and no competing builds/game clients:

```powershell
& ./build/tests/Renderer/Release/RendererSkinningBenchmarkTest.exe ../m2dev-client/assets ./build/phase-b6x/repeat-isolated.csv
& ./tests/Renderer/analyze_skinning_benchmark.ps1 -Csv ./build/phase-b6x/repeat-isolated.csv
& ./tests/Renderer/run_skinning_prototype.ps1 -Name repeat-world-cpu -Skinning cpu -Benchmark
& ./tests/Renderer/run_skinning_prototype.ps1 -Name repeat-world-gpu -Skinning gpu -Benchmark
& ./tests/Renderer/analyze_skinning_benchmark.ps1 -Csv ./build/phase-b6x/repeat-world-gpu/skinning-benchmark.csv -Kind world -WorldMode gpu
```

Repeat the world pair in reverse order. `-Skinning default -Production -Stability`
selects the 25-minute default fixture; add `-Visible` for manual window checks.
`-Production -Normal` uses original login packages. `-Skinning cpu` always passes
the explicit override now; only `default` omits the skinning argument. The audit
script rejects incomplete map sequences, missing shutdown evidence, nonzero owners,
unexpected native deformation/fallback and logged binding/remap/palette errors.

Full validation commands:

```powershell
cmake --build build --config Release --parallel 8
cmake --build build --config Debug --parallel 8
cmake -E env "PATH=C:\Program Files\Git\usr\bin;$env:PATH" ctest --test-dir build -C Release --verbose
ctest --test-dir build/tests/Renderer -C Debug --verbose
```

The child-only Git shell path is necessary for the unchanged zstd shell test on this
machine; the global PATH and vendor sources are not modified. Debug timing values
are correctness/validation evidence only, not performance scores.

Change groups:

- Production policy and CLI: `SkinningData.h`, `GpuSkinningPrototype.h`,
  `StartupOptions.h`, startup/summary logging and default-option assertions.
- Measurements only: bounded opt-in `SkinningBenchmark.h`, world/process/present
  timings, nonblocking GPU query ring and private Python fixture activation. Normal
  sessions do not allocate these queries or capture per-frame CSVs. Two model-wrapper
  counters expose all CPU dispatches and fallback attempts without changing math.
- Tests: one additional real-asset benchmark contract, test-only timing access,
  isolated/world fixtures, analyzer, launcher/auditor updates and longer reuse of
  B5-X stability scenarios. Existing test tolerances/coverage remain unchanged.
- Documentation: renderer startup README and this report. No native deformer,
  Granny SDK, static skinning shader, material/attachment/LOD architecture or
  gameplay change; no CPU-path deletion or speculative buffer rewrite.

Final source diff: **22 modified tracked files + 6 new files** (28 total), limited
to the groups above. `git diff --check` passed. No commit, push, restore, reset,
runtime-package replacement or cleanup of unrelated files was performed.
