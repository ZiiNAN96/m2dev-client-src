# P3 display closure

Historical Closure 1 result only. The user's subsequent sight test rejected
normal window placement. Superseded by [P3 UI layout closure 2](p3-ui-layout-closure.md),
which retests the actual original opening rules without injected window positions.
The earlier automated core PASS was insufficient for overall UI acceptance.
P3 production remains **NO-GO**: these fixes have not been integrated/deployed.
Separate `codex/p3-display-closure` branches; no main integration, deployment or push.

## Reproduction before changes — 2026-09-18

Private native Release client, real `game.GameWindow` / existing HUD on Modern A1,
normal production asset packs, no server connection. Evidence:
`build-p3-display-closure/baseline/closure-run.jsonl` and native screenshots.
The referenced user screenshot was not in the attachment (text only); the
reproduction independently identifies the faulty rectangle without claiming an
image match.

| Viewport | Chat background height | Help button position |
|---|---:|---|
| 1024 x 768, window | 163 | 50,598 |
| 1920 x 1080, window | 475 | 50,598 (stale) |
| 2560 x 1440, borderless | 835 | 50,598 (stale) |

2560 x 1440 is the actual desktop mode. It is exercised borderless; unsupported
full-desktop-sized framed windows are not injected into the monitor mode list.

## BUG 1 ROOT CAUSE

* Chat's detached sizing handle retains its old screen Y. The input moves to
  the new bottom edge, so `RefreshBoardEditState` derives an oversized black
  background from the distance between them. Its stale rectangle is rendered
  by `ChatWindow.OnRender`; hiding the chat would only hide the symptom.
  The native drag button also retains its creation-time movement restriction:
  resizing only its position still clamps it to the old 1024 x 768 bounds.
  The chat resize hook must update that restriction before moving the handle.
* Game-button and energy-bar script coordinates use the initial SCREEN_HEIGHT.
  These controls have no resize hook in the old interface path.
* The interface clamps every visible top-level window, including anchored HUD
  roots, as though its local coordinates were absolute floating positions.
  Nested system/graphics option dialogs are omitted. Graphics independently
  recenters on preview/rollback, discarding manual placement.
* C++ UI layer sizes change without recursively updating child alignment
  rectangles. This leaves right/center/bottom anchored geometry stale.
* Native client, graphics settings and wndMgr viewport sizes agree in the
  recorded transitions. This is not desktop-size substitution or a swapchain
  mismatch. The Python game path notices each changed viewport once.

## BUG 2 ROOT CAUSE

* `CPythonApplication::OnMouseWheel` sends all wheel events to the camera;
  there is no UI wheel dispatch.
* `CWindowManager::__PickWindow` returns a PickAlways list root directly,
  bypassing its scrollbar buttons/thumb children.
* The graphics dropdown caps visible rows but has no wheel handler, explicit
  scissor, viewport-aware direction, or bounded row hit test.
  The decorative combo text also intercepts a click exactly at its center.

## BUG 1 FIX

* Recursively update native UI layer rectangles after changing their viewport
  size. The game-owned root also refreshes child rectangles when resized.
* Reflow taskbar, minimap, game buttons, energy bar, chat input, detached chat
  handle and quest/whisper side buttons from their existing layout anchors.
* Store chat's user-selected height; refresh the handle's movement restriction
  to the current client bounds before positioning it. Recompute background and
  chat text geometry together, including the currently animated height.
* Replace the generic HUD clamp with explicit floating-panel handling. Keep a
  preferred position across a temporary clamp; honor an explicit center until
  the user moves the panel. Cover nested system/graphics options as well.
* Reflow the existing target board when visible; clamp the movable atlas.
  Resize the existing phase curtain and console, without replacing their state.

## BUG 2 FIX

* Dispatch wheel events to an open PickAlways dropdown first; otherwise bubble
  only through the picked hierarchy, respecting the existing UI lock. The
  camera receives only unhandled events. Consume wheel input at list boundaries.
* Reuse `ui.ListBox` / `ui.ScrollBar`. Native picking descends into the popup's
  children so the existing arrows and draggable thumb receive input/capture.
  Decorative list/combo text is not pickable.
* Bound rows and hit testing to the same base index, accumulate partial wheel
  deltas, keep the scrollbar entirely inside the list, and enable the existing
  native scissor. Open upwards when the dialog has insufficient space below.

## Focused evidence

Release build: `build-p3-display-closure/build-release.log`.
Existing targeted CTest selection: **10/10 PASS**, recorded in
`build-p3-display-closure/ctest-release.log` (DisplayConfiguration, Display,
FramePacing, Presets, Custom, Invalid, LiveApply, Persistence, Write/ReadRestart).
No long suite was run.

The native replay uses the real `game.GameWindow`, existing interface and
production asset packs on Modern A1. Only its outbound enter-game packet is
suppressed because this private probe has no server socket. It replays native
UI picking/capture/wheel dispatch inside the owned process; physical mouse
acceptance remains a separate user check.

`build-p3-display-closure/closure-final/closure-run.jsonl` records every control's
local/global coordinates and dimensions before and after ten transitions:
1024x768 -> 1920x1080 -> borderless 2560x1440 -> window 1920x1080 ->
1024x768 -> 1920x1080 -> 1024x768 -> 1920x1080 -> 1024x768 -> 800x600 -> 1024x768.
All captured controls return to their first A position. Explicit centered
graphics placement is separately verified at (610,285) and back at (162,129).

| Control | Canonical anchor / source | Recorded corrected behavior |
|---|---|---|
| Taskbar / hotbar | wndMgr client width; bottom 37 | y=731 / 1043 / 1403 |
| Taskbar mouse controls / quickslots | bottom center, existing fixed offsets | X recomputed from width/2 |
| Character/inventory/messenger/system taskbar buttons | bottom right | width-144/-110/-76/-42 |
| Expanded taskbar | bottom center | width/2-5, height-74 |
| Minimap | top right | width-136,0 |
| Chat input | bottom center | (width-600)/2,height-62 |
| Chat sizing handle | bottom center, retained extent | height-200; background stays 163 high |
| Energy / player status gauges | existing bottom-left taskbar / energy anchor | energy y=height-55 |
| Help/status buttons | bottom left | 50,height-170 / 68,height-100 |
| Quest/skill/build/observer buttons | bottom right | width-82, existing bottom offset |
| Quest/whisper side buttons | existing viewport layout routines | recomputed, no added deltas |
| Inventory / character / system options | preferred floating position | clamp only as needed, restore preferred position |
| Graphics settings | explicit center or user position | (300,210) -> (100,90) at 800x600 -> (300,210) |
| Target / affects / player gauge | top center / top left / projected actor | existing control types retained |

The monitor provides **19** available framed-window resolutions. The list shows
8 rows; wheel (-60,-60), wheel over another control, scrollbar arrow and native
thumb drag reach base index 11. Clicking the final visible row selects the
initially hidden **1920x1200** mode. Confirmation appears; timeout rolls back
after at least 15 seconds. A repeated selection is kept and saved; reopening
shows the selected value and remains scrollable. The fresh `restart-final`
process reads 1920x1200 from the saved private config and shows it in the UI.

Both `closure-final/fast-gate.json` and `restart-final/fast-gate.json` report
**PASS**: exit 0, empty fresh syserr, Diligent ERROR/FATAL 0, GPUFallbacks 0,
AllCPUDeformationCalls/Vertices 0, shutdown resource counters 0. These are
private-client results, not a new production deployment or network-play proof.

Native screenshots:

* Before: `build-p3-display-closure/baseline/preview-1-0918_091754.jpg`.
* Corrected HUD: `build-p3-display-closure/closure-final/preview-1-0918_093019.jpg`.
* Scrolled list: `build-p3-display-closure/closure-final/scrolled-native-0918_093032.jpg`.
* Reopened list: `build-p3-display-closure/closure-final/reopened-native-0918_093051.jpg`.

| Check | Result |
|---|---|
| Resolution A -> B -> A drift | PASS, native recorded coordinates |
| Windowed / borderless | PASS, real client sizes agree with settings / UI viewport |
| Resolution scrolling | PASS, native wheel / arrows / thumb drag |
| Offscreen selection | PASS, 1920x1200 |
| 15-second rollback / keep / persistence | PASS, including a fresh process |
| Modern HUD | PASS automated core layout; target/atlas supplemental check pending |
| User visual acceptance | PENDING, separate private `manual-review` client |
| Release / fast native gate | PASS |

Protected production baseline is saved under
`build-p3-display-closure/protected-production/`. Current user graphics SHA256:
`D23624C0F7B09C3ACF216F8D5D32CF481827D304F754A6636BF14FB4EED9A96F`;
metin2 SHA256:
`D90C3CA9EF372BE99E219147AA3D80621E9AE46C83CC316663D78EDB463C9209`.
The two existing local skip-worktree flags remain in place. No config is staged.

## Working state / boundary

Both repositories remain on `codex/p3-display-closure` with unstaged changes.
Source HEAD/main remain `1bd011cf42c349af47bd4cdb8aaed19a87ab3a60`;
runtime HEAD/main remain `e0498a4f8fd9d3979f8ac51b8e5d17756b585995`.
No commit, merge, rebase, squash, deployment or push was performed.

Test Release SHA256:
`50526BFF1179F136154C09C4F4C1228DD06657613FC39C6169982EADED83A52F`.
The protected production executable, root pack and both user configs were
rechecked against the initial SHA256 snapshot with exact matches; receipt:
`build-p3-display-closure/final-integrity.json`.

User sight acceptance was requested in the visible private `manual-review`
client (PID 43140). This is an offline A1 with the actual existing HUD, not a
synthetic replacement UI and not the deployed production client. Await the
user's result; close this owned run through `manual.done`, then finish the
supplemental target/atlas run before declaring the complete closure accepted.
