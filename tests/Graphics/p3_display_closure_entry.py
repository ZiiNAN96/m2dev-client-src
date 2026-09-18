"""Private native A1 closure probe: uses the real GameWindow and its existing HUD.

No network session, production pack or production config is modified. Baseline
mode records the unfixed layout; acceptance mode checks the same controls.
"""
import app, builtins, grp, json, os, systemSetting, time, traceback, ui, wndMgr
import background, chr, chrmgr, item, player, playersettingmodule, mouseModule

width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
wndMgr.SetScreenSize(width, height)
app.Create("P3 Display Closure - private A1", width, height, 1)
app.SetMouseHandler(mouseModule.mouseController)
wndMgr.SetMouseHandler(mouseModule.mouseController)
assert mouseModule.mouseController.Create()
log = builtins.old_open("closure-run.jsonl", "w")


def emit(event, **values):
    values.update(event=event, seconds=time.monotonic())
    log.write(json.dumps(values) + "\n")
    log.flush()


assert app.LoadLocaleData(app.GetLocalePath())
item.LoadItemTable(app.GetLocalePath() + "/item_proto")
getattr(playersettingmodule, "__LoadGameNPC")()
getattr(playersettingmodule, "__LoadGameEffect")()
for name in ("INIT", "WARRIOR", "ASSASSIN", "SURA", "SHAMAN"):
    playersettingmodule.LoadGameData(name)
background.Initialize()
background.LoadMap("metin2_map_a1", 63500.0, 59000.0, 0.0)
origin = (63500, 59000, background.GetHeight(63500, 59000))
chr.CreateInstance(57900)
chr.SelectInstance(57900)
chr.SetVirtualID(57900)
chr.SetInstanceType(6)
chr.SetRace(0)
chr.SetArmor(11299)
chr.SetHair(1001)
chr.SetWeapon(19)
chr.SetMotionMode(chr.MOTION_MODE_ONEHAND_SWORD)
chr.SetLoopMotion(chr.MOTION_WAIT)
chr.SetPixelPosition(*map(int, origin))
chr.Show()
player.SetMainCharacterIndex(57900)
for key in range(player.MAX_NUM):
    player.SetStatus(key, 0)
for key, value in ((player.LEVEL, 54), (player.MAX_HP, 5000), (player.HP, 4500),
                   (player.MAX_SP, 3000), (player.SP, 2600), (player.STAT, 1),
                   (player.SKILL_ACTIVE, 1), (player.ATT_SPEED, 100), (player.MOVING_SPEED, 100),
                   (player.ST, 80), (player.HT, 60), (player.DX, 35), (player.IQ, 10),
                   (player.EXP, 250000), (player.NEXT_EXP, 500000)):
    player.SetStatus(key, value)
app.SetCenterPosition(origin[0], -origin[1], origin[2] + 100)

import game


class RealGame(game.GameWindow):
    # Only add renderer prewarm instrumentation. Layout/update/render remain the
    # normal game path, including all interface and game-owned controls.
    def OnRender(self):
        if getattr(self, "prewarm", True):
            assert chrmgr.PrewarmVisibleActors(True)
            self.prewarm = False
        game.GameWindow.OnRender(self)


world = RealGame(None)
# The private A1 has no server socket. Keep the real GameWindow startup and
# suppress only its outbound enter-game packet, restoring the binding at once.
import net
sendEnter = net.SendEnterGamePacket
try:
    net.SendEnterGamePacket = lambda: None
    world.Open()
finally:
    net.SendEnterGamePacket = sendEnter
world.RefreshStatus()
app.SetCamera(5500.0, 22.0, 0.0, 0.0)
interface = world.interface
interface.wndChat.OpenChat()
interface.wndInventory.Show()
interface.wndInventory.SetPosition(790, 80)
interface.wndCharacter.Show()
interface.wndCharacter.SetPosition(0, 40)
interface.dlgSystem._SystemDialog__ClickSystemOptionButton()
options = interface.dlgSystem.systemOptionDlg
options.SetPosition(450, 40)
options.OpenGraphics()
dialog = options.graphicsDialog
mode = "baseline"
if os.path.exists("closure-case.txt"):
    with builtins.old_open("closure-case.txt") as stream:
        mode = stream.read().strip()
# User placement must survive preview, rollback and temporary small viewports.
if mode != "baseline":
    dialog.SetPosition(300, 210)
if mode == "closure":
    world.targetBoard.Open(0, "P3 HUD target")
    interface.wndMiniMap.AtlasWindow.Show()

ANCHORS = {
    "wndTaskBar": "BOTTOM/stretch", "wndExpandedTaskBar": "BOTTOM_CENTER",
    "wndMiniMap": "TOP_RIGHT", "wndChat": "BOTTOM_CENTER/retained-height",
    "wndGameButton": "viewport root", "chatSizing": "BOTTOM_CENTER/retained-height",
    "wndEnergyBar": "BOTTOM_LEFT", "wndInventory": "floating/preferred",
    "wndCharacter": "floating/preferred", "systemOptions": "floating/preferred",
    "graphics": "center unless user moved, then floating/preferred",
    "target": "CENTER_X/top", "affects": "TOP_LEFT", "playerGauge": "projected actor",
}


def geometry(window):
    return dict(local=window.GetLocalPosition(), globalPos=window.GetGlobalPosition(),
                size=(window.GetWidth(), window.GetHeight()), visible=bool(window.IsShow()))


def snapshot(label):
    controls = {}
    for name in ("wndTaskBar", "wndExpandedTaskBar", "wndMiniMap", "wndChat", "wndCharacter",
                 "wndInventory", "wndGameButton", "wndEnergyBar", "wndParty", "wndMessenger"):
        window = getattr(interface, name, None)
        if window:
            controls[name] = geometry(window)
    for name, window in interface.wndGameButton.gameButtonDict.items():
        controls["gameButton." + name] = geometry(window)
    for name in ("LeftMouseButton", "RightMouseButton", "quickslot_board", "CharacterButton",
                 "InventoryButton", "MessengerButton", "SystemButton"):
        controls["taskbar." + name] = geometry(interface.wndTaskBar.GetChild(name))
    for name, window in (("systemOptions", options), ("graphics", dialog),
                         ("chatSizing", interface.wndChat.btnChatSizing),
                         ("atlas", interface.wndMiniMap.AtlasWindow),
                         ("target", world.targetBoard), ("affects", world.affectShower),
                         ("playerGauge", world.playerGauge)):
        controls[name] = geometry(window)
    chatWindow = interface.wndChat
    emit("layout", label=label, viewport=(wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight()),
         native=systemSetting.GetDisplayOptions(), controls=controls,
         anchors=ANCHORS, sizeSource="wndMgr client viewport after native resize",
         chatRect=(chatWindow.xBar, chatWindow.yBar, chatWindow.widthBar, chatWindow.heightBar, chatWindow.curHeightBar))
    return controls


class Probe(ui.Window):
    def __init__(self):
        ui.Window.__init__(self, "TOP_MOST")
        self.done, self.shot = False, None
        self.steps = self.matrix()
        self.deadline = time.monotonic() + 2
        self.Show()

    def matrix(self):
        if mode == "restart":
            with builtins.old_open("closure-expected.json") as stream:
                expected = tuple(json.load(stream))
            assert (wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight()) == expected
            assert dialog.resolution.textLine.GetText() == "%d \xd7 %d" % expected
            snapshot("restart")
            emit("completed", mode=mode)
            self.done = True
            app.Exit()
            return
        first = snapshot("initial-A")
        firstChat = (interface.wndChat.heightBar, interface.wndChat.curHeightBar)
        self.shot = "initial-A"
        yield 0.3
        if mode == "manual":
            emit("manual_ready")
            while not os.path.exists("manual.done"):
                yield 0.5
            emit("completed", mode=mode)
            self.done = True
            app.Exit()
            return
        for index, (displayMode, w, h) in enumerate(((0, 1920, 1080), (1, 2560, 1440),
                                             (0, 1920, 1080), (0, 1024, 768), (0, 1920, 1080),
                                             (0, 1024, 768), (0, 1920, 1080), (0, 1024, 768),
                                             (0, 800, 600), (0, 1024, 768))):
            dialog.ApplyDisplay(dict(displayMode=displayMode, resolutionWidth=w, resolutionHeight=h))
            yield 0.8
            actual = snapshot("preview-%d" % index)
            if globals()["mode"] != "baseline":
                assert (wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight()) == (w, h)
                assert actual["gameButton.HELP"]["globalPos"] == (50, h - 170)
                assert actual["gameButton.STATUS"]["globalPos"] == (68, h - 100)
                assert actual["wndMiniMap"]["globalPos"] == (w - 136, 0)
                assert actual["wndTaskBar"]["globalPos"] == (0, h - 37)
                assert actual["wndGameButton"]["size"] == (w, h)
                assert actual["target"]["globalPos"] == ((w - world.targetBoard.GetWidth()) // 2, 10)
                assert actual["chatSizing"]["globalPos"] == ((w - 600) // 2, h - 200)
                assert (interface.wndChat.heightBar, interface.wndChat.curHeightBar) == firstChat
                for name in ("graphics", "systemOptions", "wndInventory", "wndCharacter", "atlas"):
                    px, py = actual[name]["globalPos"]
                    pw, ph = actual[name]["size"]
                    assert px >= 0 and py >= 0 and px + pw <= w and py + ph <= h, (name, actual[name])
                if (w, h) == (1024, 768):
                    for name in first:
                        assert actual[name]["globalPos"] == first[name]["globalPos"], (name, actual[name], first[name])
            self.shot = "preview-%d" % index
            yield 0.2
            dialog.ConfirmDisplay()
            yield 0.3
        snapshot("final-A")
        if globals()["mode"] != "baseline":
            dialog.SetCenterPosition()
            dialog.ApplyDisplay(dict(resolutionWidth=1920, resolutionHeight=1080))
            yield 0.8
            assert dialog.GetGlobalPosition() == (610, 285)
            dialog.CancelDisplay()
            yield 0.5
            assert dialog.GetGlobalPosition() == (162, 129)
            emit("explicit_center_pass")
            yield from self.scrolling()
            emit("completed", mode=globals()["mode"])
            self.done = True
            app.Exit()
            return
        dialog.resolution.OnMouseLeftButtonUp()
        emit("dropdown", count=dialog.resolution.listBox.GetItemCount(),
             size=geometry(dialog.resolution.listBox), scrollbar=geometry(dialog.resolution.scroll))
        self.shot = "dropdown"
        yield 0.3
        emit("completed")
        self.done = True
        app.Exit()

    def click(self, window, dx=None, dy=None):
        x, y = window.GetGlobalPosition()
        x += window.GetWidth() // 2 if dx is None else dx
        y += window.GetHeight() // 2 if dy is None else dy
        systemSetting.TestUIInput("down", x, y)
        emit("input_down", control=window.__class__.__name__, picked=bool(wndMgr.IsPickedWindow(window.hWnd)), point=(x, y))
        # Replay one complete gesture before the next OS cursor poll. We do
        # not move the user's desktop cursor from this in-process test hook.
        systemSetting.TestUIInput("up", x, y)
        yield 0.2

    def scrolling(self):
        combo = dialog.resolution
        yield from self.click(combo)
        assert combo.isListOpened
        count, rows = combo.listBox.GetItemCount(), combo.visibleRows
        assert count > rows
        x, y = combo.listBox.GetGlobalPosition()
        sx, sy = combo.scroll.GetGlobalPosition()
        assert sx >= x and sx + combo.scroll.GetWidth() <= x + combo.listBox.GetWidth()
        camera = app.GetCamera()
        assert systemSetting.TestUIInput("wheel", x + 10, y + 10, -60)
        assert combo.listBox.basePos == 0
        assert systemSetting.TestUIInput("wheel", x + 10, y + 10, -60)
        assert combo.listBox.basePos == 1
        yield 0.2
        assert app.GetCamera() == camera
        # Open popup consumes wheel even above covered controls/outside the list.
        assert systemSetting.TestUIInput("wheel", 20, 20, -120)
        assert combo.listBox.basePos == 2
        yield from self.click(combo.scroll.downButton)
        assert combo.listBox.basePos == 3, ("Scrollbar button", combo.listBox.basePos)
        thumb = combo.scroll.middleBar
        tx, ty = thumb.GetGlobalPosition()
        systemSetting.TestUIInput("down", tx + 4, ty + 3)
        systemSetting.TestUIInput("move", tx + 4, y + combo.listBox.GetHeight() - 4)
        systemSetting.TestUIInput("up", tx + 4, y + combo.listBox.GetHeight() - 4)
        yield 0.2
        assert combo.listBox.basePos == count - rows, (combo.listBox.basePos, count, rows)
        self.shot = "scrolled-native"
        emit("scroll_pass", count=count, visibleRows=rows, base=combo.listBox.basePos,
             listRect=geometry(combo.listBox), graphicsRect=geometry(dialog))
        yield 0.3
        chosen = count - 1
        expected = dialog.resolutions[chosen]
        yield from self.click(combo.listBox, 20, (rows - 1) * combo.listBox.stepSize + 8)
        yield 0.8
        state = systemSetting.GetGraphicsSettings()
        assert (state["resolutionWidth"], state["resolutionHeight"]) == expected
        assert systemSetting.GetDisplayConfirmationSeconds() > 0
        assert dialog.confirmation and dialog.confirmation.IsShow()
        start = time.monotonic()
        yield 15.2
        assert systemSetting.GetDisplayConfirmationSeconds() == 0
        assert (wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight()) == (1024, 768)
        emit("rollback_pass", elapsed=time.monotonic() - start)
        yield from self.click(combo)
        assert combo.isListOpened
        x, y = combo.listBox.GetGlobalPosition()
        for _ in range(count):
            assert systemSetting.TestUIInput("wheel", x + 10, y + 10, -120)
        assert combo.listBox.basePos == count - rows
        yield from self.click(combo.listBox, 20, (rows - 1) * combo.listBox.stepSize + 8)
        yield 0.8
        assert systemSetting.GetDisplayConfirmationSeconds() > 0
        dialog.ConfirmDisplay()
        yield 0.3
        assert (wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight()) == expected
        assert systemSetting.GetDisplayConfirmationSeconds() == 0
        with builtins.old_open("config/graphics.cfg") as stream:
            saved = stream.read()
        assert "RESOLUTION_WIDTH %d" % expected[0] in saved
        assert "RESOLUTION_HEIGHT %d" % expected[1] in saved
        with builtins.old_open("closure-expected.json", "w") as stream:
            json.dump(expected, stream)
        dialog.Close()
        dialog.Open()
        yield from self.click(combo)
        assert combo.isListOpened
        assert combo.textLine.GetText() == "%d \xd7 %d" % expected
        x, y = combo.listBox.GetGlobalPosition()
        assert systemSetting.TestUIInput("wheel", x + 10, y + 10, -120)
        assert combo.listBox.basePos == 1
        emit("keep_reopen_pass", resolution=expected)
        self.shot = "reopened-native"
        yield 0.3

    def OnUpdate(self):
        if self.done or time.monotonic() < self.deadline:
            return
        try:
            self.deadline = time.monotonic() + next(self.steps)
        except StopIteration:
            pass
        except Exception:
            with builtins.old_open("closure-failure.log", "w") as stream:
                stream.write(traceback.format_exc())
            self.done = True
            app.Exit()

    def OnRender(self):
        if self.shot:
            ok, path = grp.SaveScreenShotToPath(self.shot + "-")
            emit("screenshot", ok=ok, path=path)
            self.shot = None


probe = Probe()
app.Loop()
probe.Hide()
world.Close()
log.close()
