# P3 resize black-bar regression closure

2026-09-18: **TECHNICAL GO, TESTCLIENT PASS.** User sight acceptance:
"sah alles gut aus"; manual frame-drag acceptance: "Ja, beide Richtungen ohne Balken".
No production deployment, push, UI redesign or follow-on milestone.

## Root cause and reproduction

The runtime `assets/root/uiquest.py`, `QuestCurtain.BottomBar`, retained its
creation width and the old closed position. Both TOP_MOST bars remain shown:
`Close()` parks the bottom bar at the current screen height + 1. With
`CurtainMode == 0`, the old `OnUpdate()` never moved that parked rectangle when
the screen grew. The bars are independent of the ordinary panel reflow.

Reproduced before product edits in a private A1 client using the real
`game.GameWindow`, actual QuestDialog construction and normal renderer/UI.
The baseline executable matched production Release byte for byte. Only the
private root pack contained the fixture. No network session was required.

| Baseline measurement | Value |
|---|---|
| Initial mode | Windowed |
| Initial client / UI / backbuffer | 1280 x 960 |
| Initial outer window | 1296 x 999 |
| Resized client / UI / backbuffer / CPU and submitted UI viewport | 1920 x 1200 |
| Resized outer window | 1936 x 1239 |
| Stale bottom rectangle, client coordinates | x=0, y=961, width=1280, height=120 |
| Visibility / color / clipping | Shown; 0xff000000; no widget scissor enabled |
| First visible occurrence | First enlarged frame after the closed quest curtain existed |

The original calculation `(960 - 1280 * 9 // 16) // 2` gives **120**. The
reported screenshot geometry is therefore explained by this particular stale
UI element; no assumed DPI multiplier is needed. Only the textual screenshot
description was attached to this task. The independent native reproduction
showed the same rectangular shape, with world content beside and below it.

Hiding **only BottomBar**, diagnostically, removed the rectangle while the
remaining UI and world stayed visible. It persisted across later size changes
while the closed bar retained y=961. Below that coordinate it was outside the
client again. This diagnostic hide is not part of the fix.

The live OS DPI probe reported `GetDpiForWindow=96`, DPI awareness 0, physical
client 800 x 600 and outer 816 x 639 at that later baseline observation. The
native client/UI/swapchain probes agreed at the tested sizes. This is evidence
for this desktop, not an assumption about another monitor or DPI configuration.

## Fix and scope

Runtime product change: **24 added lines in `assets/root/uiquest.py`**.
On a screen-size change, QuestCurtain recomputes both bar extents using the
existing aspect-ratio rule and fallback, preserves the top bar's animation
progress, and anchors the bottom bar to the live height. Fully closed bars
remain outside the current client. An unchanged size does not rewrite geometry.
Opening, closing and completion callbacks retain their existing behavior.

Source changes are confined to focused tests and this evidence:

- `tests/Graphics/GraphicsClientProbe.h`: optional client width/height on the
  existing diagnostics-only `TestGraphicsWindow` hook. It uses SetWindowPos
  to exercise normal WM_SIZE independently of graphics-settings application;
  the existing no-argument probe remains supported.
- `tests/Graphics/test_quest_curtain_resize.py`: executes the actual runtime
  QuestCurtain class with widget storage/time stubs. Covers closed repeated
  grow/shrink, open cinema bars, resizing mid-animation/callbacks, zero-height
  bars and unchanged-size updates.

No native renderer/window/layout product logic changed. No global clamping,
overpainting, extra clear, reinitialization or resolution/DPI constant was added.
Production EXEs, root pack and personal configs were not replaced.

## Focused gate

| Gate | Result and evidence |
|---|---|
| Release UserInterface build | PASS; VS x64 developer environment; missing zlib PDB linker warnings only |
| QuestCurtain regression tests | 4/4 PASS; the same four tests fail against baseline runtime code |
| Graphics.Display / Graphics.DisplayConfiguration | 2/2 PASS |
| Grow A -> B | PASS; 1280x960 -> 1920x1200 |
| Shrink B -> C | PASS; 1920x1200 -> 1024x768 |
| C -> A | PASS; no UI drift |
| Horizontal resize | PASS; 1280x960 -> 1664x960 |
| Vertical resize | PASS; 1664x960 -> 1664x1176 |
| Repeated A -> B -> A | PASS; three cycles; seven panel position/size/visibility records equal the initial state at A |
| Rapid multiple resize | PASS; five intermediate OS sizes, then stable A |
| Windowed | PASS |
| Borderless switch | PASS; 1280x960 windowed -> 2560x1440 borderless -> 1280x960 windowed |
| UI smoke | PASS; HUD, inventory, character, expanded belt bag, chat, minimap and graphics dialog |
| Active cinematic curtain | PASS; opens, follows resize to 1600x1200, closes and returns to A |
| Manual OS frame drag | PASS, user-confirmed in both directions without the black bar |
| User visual acceptance | PASS, "sah alles gut aus" |

The native fixture recorded **20 successful geometry/layout checkpoints**.
Its OS resize calls are automated evidence, separate from the user's manual
frame-drag and sight acceptance. No full P3 or performance suite was run.

## Sanity and preservation

- **Stale viewport: no.** All settled acceptance checkpoints have matching
  ClientRect, backbuffer, UI screen, CPU viewport and submitted UI viewport.
  Immediate baseline OnUpdate samples can still describe the previous frame's
  last-submitted viewport; acceptance checks wait for rendered frames.
- **Stale scissor: none observed.** The offending Bar has no scissor; the
  existing renderer chooses ScissorEnable per draw and supplies current clip
  rectangles for clipped draws. The fix does not alter this path.
- **Repeated resource recreation: no evidence of a loop.** Across 4655 modern
  frames, one lighting buffer was created. The existing water resize path
  created 141 resources across the bounded size transitions, with an early
  return for unchanged sizes. Idle residency snapshots two seconds apart are
  identical: terrain/areas loaded 20, unloaded 0, assignments 2, instance buffer
  creates 58, trees created 368. World rendering remained visible.
- Clean shutdown: ExitCode=0, empty syserr, DiligentErrors=0,
  DiligentFatals=0, and all eight renderer shutdown resource groups zero.
- Personal `config/graphics.cfg` and `config/metin2.cfg`: SHA256 unchanged;
  both retain skip-worktree. Production Release/Debug EXEs and root.pck also
  match their original hashes. Both private test configs were restored
  byte for byte from their pre-test copies after the runs.

## Evidence and Git boundary

Starting main commits:

- Source: `b7c5fe74175150f5cda98e63122d8cb8732b7ac9`.
- Runtime: `f0c25d5016603cf750b7c5ac5852e9207db274a7`.

[Verification receipt](evidence/p3-resize-regression/verification.json) contains
tested hashes, protected-file hashes, manual acceptance and gate results.
[Compact diagnostics](evidence/p3-resize-regression/diagnostics.zip) preserve
baseline/fixed native logs, build/test results and private fixture/preparation
scripts. They contain no production configs or executable copies. The local
native captures remain under `build/resize-regression/`; no gallery was built.
`fixed-grow` was captured during the subsequent size transition and is not
used as a settled-size screenshot. `fixed-final-A` is the settled final UI view.

Separate Source and Runtime commits are permitted by the task after this GO.
Source contains test coverage/evidence; Runtime contains only the QuestCurtain
fix. No push, production deployment or I-X work. **STOP after closure.**
