# P3 UI layout closure 2

2026-09-18. Native automated closure **PASS**; user sight acceptance **PASS**.
Production **NO-GO / not integrated or deployed**. No commit, main integration,
deployment, push, user-config changes, redesign or scaling changes.

## Reproduction and reference

The earlier probe explicitly moved inventory to (790,80), character to (0,40)
and system options to (450,40). It tested retained test positions, not original
opening semantics. Its automated PASS was insufficient for this acceptance.
The previous user sight run is rejected by the user's Closure 2 report.

The new probe uses the actual `game.GameWindow` / A1 Modern scene and original
Open/Show handlers without replacing their default coordinates. Native baseline
evidence: `build-p3-ui-layout-closure/baseline-2/closure-run.jsonl`.

| Window | Original rule / source | 1024x768 | Expected 2560x1440 | Baseline after resize |
|---|---|---|---|---|
| Inventory | InventoryWindow.py: W-176,H-37-565 | 848,166 | 2384,838 | 848,166 |
| Character / Skills / Quest tabs | CharacterWindow.py: 24,(H-37-361)/2 | 24,185 | 24,521 | 24,185 |
| Messenger | MessengerWindow.py: W-200,H-450 | 824,318 | 2360,990 | stale original coordinates |
| Guild | guildwindow.py: 0,0 | 0,0 | 0,0 | unchanged is correct |
| Shop | shopdialog.py: W-400,10 | 624,10 | 2160,10 | stale original coordinates |
| Safebox / storage / mall | SafeboxWindow.py / MallWindow.py: 100,20 | 100,20 | 100,20 | unchanged is correct |
| System options | OptionDialog initialization: SetCenterPosition, 305x255 | 359,256 | 1127,592 | previous center handling |
| Graphics | SetCenterPosition, 700x510 | 162,129 | 930,465 | previous center handling |
| Costume | CostumeWindow.py: W-315,H-602 | 709,166 | 2245,838 | 709,166 |
| Belt bag, expanded | GetBasePosition: inventory global + (-148,241) | 700,407 | 2236,1079 | 700,407 |

These rules come from the repository's existing UI scripts and class methods;
no new default positions are invented. Locale-selected scripts remain the
source of truth. The referenced screenshot is described in the user's text;
the attachment itself contains only text. Native screenshots supplement it.

## Inventory root cause

`Interface.ToggleInventoryWindow -> InventoryWindow.Show -> __LoadWindow`.
`isLoaded` prevents re-execution after initial construction. PythonScriptLoader
evaluates SCREEN_WIDTH/HEIGHT correctly when loading, but stores only the
computed root position. The previous generic clamp classified this untouched
default as an absolute user position. Both OPEN -> RESIZE and CLOSED -> RESIZE
-> OPEN therefore retained the 1024x768 origin at 2560x1440.

Fix: retain the original compiled UI script's root position rule for movable
top-level windows. Re-evaluate only x/y with the current wndMgr dimensions,
including lazy/hidden windows. Do not recreate widgets, sizes, slots or content.
Once the window is actually moved, keep that absolute user position, clamped
only as a visibility safeguard. Restore the preferred position after a temporary
clamp instead of promoting the clamped value to a new default.

## Child / bag root cause

BeltInventoryWindow is a detached native top-level window with an explicit
Inventory.GetGlobalPosition + offset relationship. Its adjustment was called
by the user-drag callback and expand/collapse, but not by programmatic inventory
SetPosition. Fix: route programmatic inventory placement through the same
existing attachment update; size the bag before updating its child rectangles.
Include a visible belt's left extent when validating inventory's screen bounds.

CostumeWindow is independently movable in the original code. It has an initial
screen rule next to the default inventory, not a parent-follow relationship;
preserve that distinction. Real native children retain local offsets and are
excluded from screen-position management.

## Shared screen basis and resize order

`ApplyDisplayConfiguration` normalizes a real monitor mode, changes window
style/client bounds under `m_applyingDisplay`, reads final ClientRect, resizes
the Diligent swapchain, updates CPU draw viewport/scissor, then calls
`OnSizeChange` to publish the UI size. Synchronous WM_SIZE is ignored during
this transaction. GameWindow notices one changed viewport, reflows HUD anchors,
then existing top-level windows and modal dialogs. No extra window-resize call
is introduced by reflow.

Read-only native audit reports actual ClientWidth/Height, swapchain-desc
BackbufferWidth/Height, native UIScreenWidth/Height, CPU viewport and the last
submitted Diligent UI viewport. Outer window dimensions and desktop mode are
logged separately; they are not the UI coordinate basis. All 29 settled layout
snapshots agree across these five independently read sizes. At the instant of
the reflow callback, the diagnostic's *last submitted* UI draw still describes
the preceding frame; the draw path uses the updated CPU viewport and every
settled snapshot records the new dimensions. Each requested size change
produces exactly one GameWindow reflow, checked by the native run.

| Mode | Client | Backbuffer | Native/Python UI | CPU / submitted UI viewport | Outer window |
|---|---|---|---|---|---|
| Windowed | 1024x768 | 1024x768 | 1024x768 | 1024x768 | 1040x807 |
| Windowed | 1280x720 | 1280x720 | 1280x720 | 1280x720 | 1296x759 |
| Windowed | 1920x1080 | 1920x1080 | 1920x1080 | 1920x1080 | 1936x1119 |
| Borderless | 2560x1440 | 2560x1440 | 2560x1440 | 2560x1440 | 2560x1440 |

The monitor desktop is 2560x1440. It is used only to select borderless bounds,
not as a substitute for a windowed client area. Native mouse normalization also
uses GetClientRect. CPU resize updates viewport, full scissor and projection;
DiligentEffectRenderer explicitly sets viewport and UI scissor for UI draws.

## HUD / dialogs / persistence

Fixed HUD retains its existing taskbar, minimap, energy, game/side-button and
chat anchor routines. The repository's chat rule, already present before P3,
is `(W-600)/2,H-62` (600 pixels centered along the bottom); do not substitute a
new left margin. Its detached height handle keeps the chosen bottom extent.

Normal floating panels and centered modal dialogs use the shared current
viewport. Explicit modal centering persists over resize; ordinary centered
panels retain a subsequent user's move. Script children are never treated as
independent screen windows. Dropdowns retain the existing bounded list,
scrollbar, native wheel dispatch and scissor from Closure 1.

C++ still exposes interface.cfg / SaveWindowStatus / GetWindowStatus, but no
active root Python code calls the per-window APIs or implements the save
callback. These normal-window positions are session state, not newly persisted
defaults. This change adds no writes or configuration fields.

## Dropdown / scrolling root cause and fix

The original resolution ComboBox displayed every monitor mode without a bounded
viewport/scrollbar. Native mouse wheel was routed to the scene, and decorative
text/pick ordering could intercept row or scrollbar input. Closure 1 added an
eight-row list using the existing ScrollBar, UI wheel bubbling before camera
handling, native child picking, non-pickable decorative text and list scissoring.
The list recomputes hover/hit rows at click time after a scroll. Partial wheel
deltas accumulate; limit/outside-list wheel input is consumed while the popup
is open. No replacement scrollbar or UI scaling system is introduced.

Closure 2 validates this inside the real GraphicsDialog after each mode change:
12 enabled visible dropdowns in each windowed mode, 11 in borderless (resolution
is intentionally disabled there). All popup backgrounds, including their three
pixel border, fit their original parent and current viewport. Lower controls
open upwards when appropriate. Reflow closes stale popups; reopening recomputes
their placement. Native wheel, down-arrow and thumb drag reach base row 11 of
19 modes, with eight visible rows. Native hit testing selects the initially
hidden 1920x1080 row correctly, rolls it back after 15.216 seconds, then selects,
confirms, saves to the private configuration and reopens it correctly.

## Numeric layout and child verification

Evidence: `build-p3-ui-layout-closure/closure-final/closure-run.jsonl` and
`fast-gate.json`. The real game initializes all required windows using their
existing Open/Show methods; it does not assign artificial default positions.
Shop, Guild, Safebox and Mall use local real UI fixtures without server contents.
This verifies layout/opening, not network transactions or a full online session.

- 29 snapshots, each with 42 window/control rectangles. All four modes cover
  CLOSED -> RESIZE -> OPEN and OPEN -> RESIZE. STATUS, SKILL and QUEST tabs
  retain the original character position in each mode.
- Five complete A -> B -> A -> B -> A cycles (A=1024x768 windowed,
  B=2560x1440 borderless). Every return matches the original position AND size
  exactly, including individual HP/SP/ST/EXP controls, quickslots and all six
  side buttons. **Zero pixel drift**.
- Default inventory: 848,166 -> 2384,838 -> 1744,478 -> 1104,118 -> 848,166.
  Its existing 176x565 size remains unchanged. Board, title, equipment image,
  equipment slots, item grid and Yang bar retain their original relative
  rectangles. Native screenshots show the existing complete textures/grids.
- Expanded belt remains inventory + (-148,241), size 148x139. Native title drag
  to (1800,650) moves it to (1652,891); Costume stays independent as originally
  implemented. Reducing to 1024x768 clamps inventory to (848,203) and belt to
  (700,444); increasing restores (1800,650), including after close/reopen.
  Collapsing gives x=inventory-10 and width=10; expanding restores width=148.
- Native parent rectangles follow root movement; child local coordinates and
  open state stay intact. Top-level belt ownership remains the existing explicit
  relationship. No reparenting, slot recreation or texture scaling is introduced.
- Question, Popup, Input and System dialogs center against the current viewport
  after resize. The real item tooltip at the lower-right edge clamps to
  (1730,921), size 190x134 in 1920x1080.
- Native picking operates at rendered positions: all tested Graphics dropdowns,
  scrolled rows, scrollbar parts and inventory title drag are selected correctly.

## Release / fast gate / evidence boundaries

Build: `build-p3-ui-layout-closure/build-release.log`, Release UserInterface and
the three focused test targets, successful exit. Targeted CTest: **10/10**
(FramePacing, Presets, Custom, Invalid, LiveApply, Display, Persistence,
WriteRestart, ReadRestart, DisplayConfiguration). No long overall suite.

Private native A1 Modern closure process: PID 36412, exit 0, empty syserr,
35 recorded reflows including rollback and moved-window/modal cases. Final
checker: `tests/Graphics/check_ui_layout_closure.py` -> **PASS**.

| Fast gate counter | Result |
|---|---|
| Diligent ERROR / FATAL | 0 / 0 |
| GPU fallback | 0 |
| All CPU deformation calls / vertices | 0 / 0 |
| CPU reference frames / CPU vertex bytes | 0 / 0 |
| GPU frames | 2981 |
| Shutdown source / skin / animation / collision / vegetation resources | 0 |
| Shutdown Modern / water / terrain / static adapter resources | 0 |
| Renderer failure log | enabled header only |

Release SHA256: `93EE9B5D46D6E0C760B424A1180F2BF24EE9BEA3E3A117D8CEF5773B80AAF9FD`.
Private root pack SHA256:
`A0E7104CE3FCCFF1CEB0EB1EA0D3B99461E3EC6FC235CFF63E925393CD3AF750`.
The private root overlays the changed runtime files and the native fixture on
the decoded production root; other production packs are read-only shared inputs.
The fixture replays input inside the actual native WindowManager; it does not
replace the controls with mocks. Physical mouse/desktop sight acceptance remains
the user's separate final check. Earlier pilot failures were test setup errors;
only `closure-final` supplies the completed acceptance result.

## Protected production and manual sight run

Byte-exact private backup: `build-p3-ui-layout-closure/protected-production/`.
All five original config files match that backup; production executable and
root pack also match their initial hashes. Receipt:
`build-p3-ui-layout-closure/protected-verification.json`.

| Protected production file | Unchanged SHA256 |
|---|---|
| config/graphics.cfg | D23624C0F7B09C3ACF216F8D5D32CF481827D304F754A6636BF14FB4EED9A96F |
| config/metin2.cfg | D90C3CA9EF372BE99E219147AA3D80621E9AE46C83CC316663D78EDB463C9209 |
| Metin2_Release.exe | C1145D82B178BE2F627DB82621D93A99C54CAD5F33ED6511BFA4F837CBBAA5D1 |
| pack/root.pck | AEA566D4B56A74B1A2D6A33E4C6B24B57546B1D7F5F97BF9A8939D46B96400E3 |

Both worktrees remain on `codex/p3-display-closure`, with unstaged closure edits.
No new commits. Source HEAD/main remains
`1bd011cf42c349af47bd4cdb8aaed19a87ab3a60`; runtime HEAD/main remains
`e0498a4f8fd9d3979f8ac51b8e5d17756b585995`. Diff whitespace checks pass.

Visible private sight client: `build-p3-ui-layout-closure/manual-2560/`, PID 4248,
2560x1440 borderless, `manual_ready` recorded. Character, Inventory with Belt and
Costume, System Options and Graphics are open at their original rules. Both
options dialogs are originally centered: System Options is raised so both are
visible, without inventing separate positions. Resolution dropdown remains
correctly disabled in borderless; switch to Windowed to manually inspect its
list. The automated windowed scrolling evidence is already recorded above.
The sight client's Release executable has the same SHA256 as the completed
gate; its separately packed root SHA256 is
`A7082D40C6C4CCAD7CEEA283DE18ADA726B9EA4CE320210CEFE8333D310E1A8B`.

Native ready capture:
`manual-2560/manual-defaults-2560-0918_140140.jpg` (under the same evidence base).
The user explicitly answered **"Sichtabnahme: PASS"** for this exact visible
2560x1440 client. This is the manual acceptance for Closure 2, separate from the
automated evidence. The private client had already exited normally at the final
check (exit 0; no fixture completion event, consistent with interactive closure).
Its syserr is empty and final Diligent errors/fatals, CPU deformation, GPU fallback
and audited live resources are zero. Production hashes were verified again after
its exit. No production acceptance, commit, merge, deployment or push is implied.
The separately prepared root input files were hash-compared
with the completed native gate and are identical.

## Acceptance

| Check | Result |
|---|---|
| Inventory | PASS (native automated) |
| Character / Skills / Quest | PASS (native automated) |
| HUD | PASS (native automated) |
| Bags / slot grids / child geometry | PASS (native automated + captured render) |
| System / Graphics | PASS (native automated) |
| Messenger / Guild / Shop / Safebox / Mall | PASS (local layout fixtures) |
| CLOSED -> RESIZE -> OPEN | PASS, all four modes |
| OPEN -> RESIZE | PASS, all four modes |
| Resolution scrolling / offscreen selection | PASS, actual GraphicsDialog/native picking |
| A -> B -> A repeated drift | PASS, five A-B-A-B-A cycles |
| Windowed / borderless | PASS, repeated round trips |
| Fast gate | PASS |
| User sight acceptance at 2560x1440 | PASS, explicit user confirmation |
