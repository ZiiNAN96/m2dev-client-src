# C2-X – final validation disposition

Recorded 2026-09-14, 09:38 Europe/Vienna, following the user's explicit instruction to stop long-running tests.

| Gate | Final status | Evidence |
|---|---|---|
| Fresh x64 Configure / solution generation | PASSED | `build-c2x-clean/configure.log`, generated solution and project mappings |
| Full Release / Debug builds | PASSED | Both build commands exited 0 |
| Platform tests | PASSED | Release 5/5; Debug 5/5 |
| Broad GPU coverage | PASSED | 583 representative cases; Release `Renderer.SkinningCoverage`, 292.57 s |
| Hair LOD | PASSED | Release `Renderer.HairLodParity`, 92.87 s |
| Runtime / world / window / shutdown regression | PASSED | User-confirmed complete requested matrix; fresh Release run PID 64780, exit 0; monitored resources 0 |
| Normal Release logging | PASSED | Separate ingame run PID 67936, exit 0; verbose diagnostics off; no large renderer logs; startup log 130 bytes |
| PE / imports | PASSED | 62/62 audited artifacts PE32+/AMD64; no direct D3D8/D3D9/D3DX imports |
| Long CPU/GPU image stability suite, as a whole | INTENTIONALLY ABORTED / NOT FULLY RERUN FOR C2-X | Explicit user decision; not PASSED |
| Full Debug renderer suite | NOT RUN | Not started; explicitly waived with the long-suite rerun |
| C1 long fuzzer | SKIPPED / ABORTED | Historical status retained; not PASSED; no C2 rerun |
| Zstandard playTests | NOT RUN FOR C2-X | Previously documented MSYS/cygdrive environment issue remains separate |

## Exact termination state

The Release renderer run recorded 21 completed passing tests out of 24 scheduled tests. This is **not** a 24/24 PASS and **not** a completed suite:

- `Renderer.SkinningStability.parity` had already completed successfully in 371.04 s before the termination instruction was acted on. This individual result does not make the full long stability suite pass.
- `Renderer.SkinningStability.stress` was active and was intentionally terminated without a completed result.
- `Renderer.SkinningFallbackParity` and `Renderer.SkinningBenchmark` had not started in this run.
- No test failure was observed or recorded before termination. This is not a claim about uncompleted work.

The verified C2-X CTest runner PID 14956 and its stress child PID 57920 were the termination targets. After the runner was stopped, its child was no longer present; subsequent process inspection found no remaining CTest, skinning-stability, coverage, fallback-parity or benchmark processes. No completed stress result was recorded. The execution session ended with termination code -1, not a successful suite exit. Existing unrelated game processes were not stopped.

## Why the long-suite rerun was stopped

C2-X changes platform boundaries and diagnostic logging, not GPU-skinning or rendering algorithms. The user accepted the already successful relevant GPU smoke, representative coverage, Hair LOD and live runtime gates as the C2-X acceptance scope and explicitly prohibited any further long, stress or fuzz runs. No additional tests were started after that instruction.

## Binary identity and runtime evidence

The final Release binary and both normal-client test copies have SHA256:

`0F7F33CEF84F4EB8A856A7693E5338DE8063FAC595F0CC59380E04ED2340783C`

The diagnostic run lasted 317.3 s, with `SkinPreparationFailures=0`, `AllCPUDeformationCalls=0`, `GPUFallbacks=0` and 670007 GPU actor-deformation events (not video frames). Source, skin, remap, palette, geometry, static-skin and all reported renderer shutdown owners were 0. Full A1 → B1 → A1 → Dungeon → guild map → A1, input/UI/rendering, multiple resize, 3× minimize/restore, relog and X shutdown were user-confirmed.

Raw evidence is retained in `build-c2x-clean/renderer-release-tests.log`, the platform test logs, `build-c2x-clean/evidence/`, and `build-c2x-clean/runtime/`. No old-build binary was substituted and no runtime-package executable was overwritten.
