"""Private native A1 closure probe: uses the real GameWindow and its existing HUD.

No network session, production pack or production config is modified. Baseline
mode records the unfixed layout; acceptance mode checks the same controls.
"""
import app, builtins, grp, json, os, systemSetting, time, traceback, ui, wndMgr
import background, chr, chrmgr, item, player, playersettingmodule, mouseModule

width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
wndMgr.SetScreenSize(width, height)
app.Create("P3 UI Layout Closure - private A1", width, height, 1)
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


# Appended to the existing native RealGame fixture. No test window coordinates
# replace the product's default Open/Show rules.
import net, uiCommon
world = RealGame(None)
sendEnter = net.SendEnterGamePacket
try:
    net.SendEnterGamePacket = lambda: None
    world.Open()
finally:
    net.SendEnterGamePacket = sendEnter
world.RefreshStatus()
app.SetCamera(5500.0, 22.0, 0.0, 0.0)
interface = world.interface
reflow_count = 0
original_reflow = interface.OnScreenSizeChange
def audited_reflow(w, h):
    global reflow_count
    reflow_count += 1
    emit('reflow', count=reflow_count, viewport=(w, h),
         geometry=systemSetting.TestDisplayGeometry() if hasattr(systemSetting, 'TestDisplayGeometry') else None)
    original_reflow(w, h)
interface.OnScreenSizeChange = audited_reflow
interface.dlgSystem._SystemDialog__ClickSystemOptionButton()
options = interface.dlgSystem.systemOptionDlg
options.OpenGraphics()
dialog = options.graphicsDialog
mode = 'baseline'
if os.path.exists('closure-case.txt'):
    with builtins.old_open('closure-case.txt') as stream:
        mode = stream.read().strip()

def open_windows():
    interface.OpenCharacterWindowWithState('STATUS')
    if not interface.wndInventory.IsShow():
        interface.ToggleInventoryWindow()
    if interface.wndInventory.wndBelt:
        interface.wndInventory.wndBelt.OpenInventory()
    if not interface.wndInventory.wndCostume:
        interface.wndInventory.ClickCostumeButton()
    else:
        interface.wndInventory.wndCostume.Show()
    interface.wndMessenger.Show()
    interface.wndGuild.Open()
    interface.dlgShop.Open(57900)
    interface.OpenSafeboxWindow(3)
    interface.OpenMallWindow(3)
    options.Show()
    dialog.Open()

def panels():
    values = {name: getattr(interface, name) for name in ('wndInventory', 'wndCharacter',
        'wndMessenger', 'wndGuild', 'dlgShop', 'wndSafebox', 'wndMall', 'wndTaskBar',
        'wndMiniMap', 'wndChat', 'wndGameButton', 'wndEnergyBar')}
    values.update(options=options, graphics=dialog, belt=interface.wndInventory.wndBelt,
                  costume=interface.wndInventory.wndCostume, chatSizing=interface.wndChat.btnChatSizing)
    values.update(('side.'+name,window) for name,window in interface.wndGameButton.gameButtonDict.items())
    for name in ('Gauge_Board','EXP_Gauge_Board','quickslot_board','quick_slot_1','quick_slot_2',
                 'HPGauge','SPGauge','STGauge','EXPGauge_01','EXPGauge_02','EXPGauge_03','EXPGauge_04'):
        values['taskbar.'+name] = interface.wndTaskBar.GetChild(name)
    return {name: value for name, value in values.items() if value}

RULES = {
    'wndInventory': lambda w,h: (w-176,h-602),
    'wndCharacter': lambda w,h: (24,(h-398)//2),
    'wndMessenger': lambda w,h: (w-200,h-450),
    'wndGuild': lambda w,h: (0,0), 'dlgShop': lambda w,h: (w-400,10),
    'wndSafebox': lambda w,h: (100,20), 'wndMall': lambda w,h: (100,20),
    'costume': lambda w,h: (w-315,h-602),
    'options': lambda w,h: ((w-305)//2,(h-255)//2),
    'graphics': lambda w,h: ((w-700)//2,(h-510)//2),
    'wndTaskBar': lambda w,h: (0,h-37), 'wndMiniMap': lambda w,h: (w-136,0),
    'wndChat': lambda w,h: ((w-600)//2,h-62),
    'chatSizing': lambda w,h: ((w-600)//2,h-200),
    'wndGameButton': lambda w,h: (0,0), 'wndEnergyBar': lambda w,h: (0,h-55),
}
child_reference = None

def check_layout(label, default_positions=True):
    global child_reference
    controls = snapshot(label)
    w,h = wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight()
    geometry = systemSetting.TestDisplayGeometry()
    for prefix in ('Client','Backbuffer','UIScreen','CPUViewport','RenderViewport'):
        assert (geometry[prefix+'Width'],geometry[prefix+'Height']) == (w,h), (prefix,geometry)
    for name, window in panels().items():
        if default_positions and name in RULES:
            x,y = RULES[name](w,h)
            expected = (max(0,min(x,w-window.GetWidth())),max(0,min(y,h-window.GetHeight())))
            assert window.GetGlobalPosition() == expected, (label,name,window.GetGlobalPosition(),expected)
    hud = {'taskbar.Gauge_Board':(0,h-47), 'taskbar.EXP_Gauge_Board':(158,h-37),
           'taskbar.quickslot_board':(w//2-86,h-37), 'taskbar.quick_slot_1':(w//2-86,h-34),
           'taskbar.quick_slot_2':(w//2+56,h-34), 'side.HELP':(50,h-170),
           'side.STATUS':(68,h-100), 'side.SKILL':(w-82,h-100), 'side.QUEST':(w-82,h-170),
           'side.BUILD':(w-82,h-170), 'side.EXIT_OBSERVER':(w-82,h-170)}
    for name, expected in hud.items():
        assert controls[name]['globalPos'] == expected,(label,name,controls[name],expected)
    inv = interface.wndInventory
    ix,iy = inv.GetGlobalPosition()
    if inv.wndBelt:
        bx,by = inv.wndBelt.GetGlobalPosition()
        assert (bx,by) == (ix-148,iy+241), (label,'belt',(bx,by),(ix,iy))
        assert inv.wndBelt.IsOpeningInventory()
    children = {name:(c['globalPos'][0]-ix,c['globalPos'][1]-iy,c['size'])
                for name,c in controls.items() if name.startswith('inventory.')}
    if child_reference is None:
        child_reference = children
    assert children == child_reference, (label,'child/slot geometry drift')
    emit('layout_pass',label=label,geometry=geometry,reflows=reflow_count)
    return controls

def close_windows():
    # These are the existing toggle/hide paths; inventory Hide also remembers
    # and closes its attached subpanels. No fake coordinates are supplied.
    for name in ('wndInventory','wndCharacter','wndMessenger','wndGuild','dlgShop','wndSafebox','wndMall'):
        getattr(interface,name).Hide()
    dialog.Close()
    options.Hide()

def snapshot(label):
    controls = {}
    for name, window in panels().items():
        controls[name] = dict(local=window.GetLocalPosition(), globalPos=window.GetGlobalPosition(),
                              size=(window.GetWidth(), window.GetHeight()), visible=bool(window.IsShow()))
    inv = interface.wndInventory
    for name in ('board', 'TitleBar', 'Equipment_Base', 'EquipmentSlot', 'ItemSlot', 'Money', 'Money_Slot'):
        if name in inv.ElementDictionary:
            window = inv.GetChild(name)
            controls['inventory.' + name] = dict(local=window.GetLocalPosition(), globalPos=window.GetGlobalPosition(),
                                                size=(window.GetWidth(), window.GetHeight()), visible=bool(window.IsShow()))
    emit('layout', label=label, viewport=(wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight()),
         native=systemSetting.GetDisplayOptions(), controls=controls)
    return controls

def geometry(window):
    return dict(local=window.GetLocalPosition(), globalPos=window.GetGlobalPosition(),
                size=(window.GetWidth(), window.GetHeight()), visible=bool(window.IsShow()))

class Probe(ui.Window):
    def __init__(self):
        ui.Window.__init__(self, 'TOP_MOST')
        self.done = False
        self.shot = None
        self.steps = self.matrix()
        self.deadline = time.monotonic() + 1
        self.Show()

    def matrix(self):
        open_windows()
        interface.wndChat.OpenChat()
        yield 0.5
        if mode == 'manual':
            # Native initial mode is 2560x1440 borderless. Show the requested
            # four windows at their actual rules, without test positioning.
            for name in ('wndMessenger','wndGuild','dlgShop','wndSafebox','wndMall'):
                getattr(interface,name).Hide()
            options.SetTop()
            self.shot = 'manual-defaults-2560'
            snapshot('manual-ready-2560')
            emit('manual_ready', mode=mode)
            while not os.path.exists('manual.done'):
                yield 0.5
            emit('completed',mode=mode)
            self.done=True
            app.Exit()
            return
        if hasattr(systemSetting, 'TestDisplayGeometry'):
            yield from self.verify_matrix()
            emit('completed', mode=mode)
            self.done = True
            app.Exit()
            return
        snapshot('initial-open-1024')
        self.shot = 'initial-defaults'
        yield 0.2
        for window in tuple(panels().values()):
            if window not in (interface.wndTaskBar, interface.wndMiniMap, interface.wndChat,
                              interface.wndGameButton, interface.wndEnergyBar, interface.wndChat.btnChatSizing):
                window.Hide()
        dialog.ApplyDisplay(dict(displayMode=1, resolutionWidth=2560, resolutionHeight=1440))
        yield 0.7
        systemSetting.ConfirmDisplaySettings()
        open_windows()
        yield 0.5
        snapshot('closed-resize-open-2560')
        self.shot = 'closed-resize-open'
        yield 0.2
        dialog.ApplyDisplay(dict(displayMode=0, resolutionWidth=1920, resolutionHeight=1080))
        yield 0.7
        dialog.ConfirmDisplay()
        snapshot('open-resize-1920')
        self.shot = 'open-resize'
        yield 0.2
        emit('completed', mode=mode)
        self.done = True
        app.Exit()

    def transition(self, w, h, borderless=0):
        before = reflow_count
        old = (wndMgr.GetScreenWidth(),wndMgr.GetScreenHeight())
        dialog.ApplyDisplay(dict(displayMode=borderless,resolutionWidth=w,resolutionHeight=h))
        yield 0.5
        assert (wndMgr.GetScreenWidth(),wndMgr.GetScreenHeight()) == (w,h)
        assert reflow_count-before == int(old != (w,h)), ('duplicate/missing reflow',before,reflow_count)
        systemSetting.ConfirmDisplaySettings()
        yield 0.1

    def verify_matrix(self):
        first = check_layout('initial-open-1024')
        self.shot='defaults-1024'
        yield 0.2
        # Both paths for every required window at each of the four real modes.
        for w,h,borderless in ((2560,1440,1),(1920,1080,0),(1280,720,0),(1024,768,0)):
            close_windows()
            yield from self.transition(w,h,borderless)
            open_windows()
            yield 0.2
            check_layout('closed-resize-open-%dx%d'%(w,h))
            yield from self.popup_bounds()
            for state in ('SKILL','QUEST','STATUS'):
                interface.OpenCharacterWindowWithState(state)
                assert interface.wndCharacter.GetGlobalPosition() == RULES['wndCharacter'](w,h)
                emit('character_tab_pass',state=state,viewport=(w,h))
            self.shot='defaults-%dx%d'%(w,h)
            yield 0.2
        for w,h,borderless in ((1280,720,0),(1920,1080,0),(2560,1440,1),(1024,768,0)):
            yield from self.transition(w,h,borderless)
            check_layout('open-resize-%dx%d'%(w,h))
        for cycle in range(5):
            for w,h,borderless in ((2560,1440,1),(1024,768,0),(2560,1440,1),(1024,768,0)):
                yield from self.transition(w,h,borderless)
                current=check_layout('open-resize-cycle%d-%dx%d'%(cycle,w,h))
                if (w,h)==(1024,768):
                    for name in first:
                        assert current[name]['globalPos']==first[name]['globalPos'],(name,'A drift')
                        assert current[name]['size']==first[name]['size'],(name,'size drift')
        emit('drift_pass',cycles=5)
        yield from self.scrolling()
        # Existing movable behavior after a real native title-bar drag.
        yield from self.transition(2560,1440,1)
        inv=interface.wndInventory
        inv.SetTop()
        title=inv.GetChild('TitleBar')
        tx,ty=title.GetGlobalPosition()
        ix,iy=inv.GetGlobalPosition()
        old_costume=inv.wndCostume.GetGlobalPosition()
        systemSetting.TestUIInput('down',tx+20,ty+8)
        systemSetting.TestUIInput('move',tx+20+1800-ix,ty+8+650-iy)
        systemSetting.TestUIInput('up',tx+20+1800-ix,ty+8+650-iy)
        assert inv.GetGlobalPosition()==(1800,650),inv.GetGlobalPosition()
        assert inv.wndCostume.GetGlobalPosition()==old_costume,'Costume is independently movable in the original UI'
        assert inv.wndBelt.GetGlobalPosition()==(1652,891)
        yield from self.transition(1024,768)
        assert inv.GetGlobalPosition()==(848,203),inv.GetGlobalPosition()
        assert inv.wndBelt.GetGlobalPosition()==(700,444)
        yield from self.transition(2560,1440,1)
        assert inv.GetGlobalPosition()==(1800,650)
        inv.Close()
        inv.Show()
        assert inv.GetGlobalPosition()==(1800,650)
        inv.wndBelt.CloseInventory()
        assert inv.wndBelt.GetGlobalPosition()==(1790,891)
        assert inv.wndBelt.GetWidth()==10
        inv.wndBelt.OpenInventory()
        assert inv.wndBelt.GetGlobalPosition()==(1652,891)
        assert inv.wndBelt.GetWidth()==148
        emit('user_position_bag_pass')
        # Standard modal rules and tooltip use the same client basis.
        self.modals=[uiCommon.QuestionDialog(),uiCommon.PopupDialog(),uiCommon.InputDialog()]
        for modal in self.modals: modal.Open()
        interface.dlgSystem.OpenDialog()
        yield from self.transition(1920,1080)
        for modal in self.modals+[interface.dlgSystem]:
            assert modal.GetGlobalPosition()==((1920-modal.GetWidth())//2,(1080-modal.GetHeight())//2)
            modal.Hide()
        emit('modal_center_pass')
        tooltip=interface.tooltipItem
        systemSetting.TestUIInput('move',1915,1075)
        tooltip.SetItemToolTip(19)
        tooltip.OnUpdate()
        x,y=tooltip.GetGlobalPosition()
        assert 0<=x and 0<=y and x+tooltip.GetWidth()<=1920 and y+tooltip.GetHeight()<=1080
        emit('tooltip_bounds_pass',position=(x,y),size=(tooltip.GetWidth(),tooltip.GetHeight()))
        tooltip.HideToolTip()

    def popup_bounds(self):
        dialog.SetTop()
        checked = 0
        w,h = wndMgr.GetScreenWidth(),wndMgr.GetScreenHeight()
        for combo in dialog.combos:
            if not combo.enable or not combo.IsShow():
                continue
            yield from self.click(combo)
            assert combo.isListOpened
            x,y = combo.listBox.GetGlobalPosition()
            dx,dy = dialog.GetGlobalPosition()
            assert dx<=x and dy<=y-3 and x+combo.listBox.GetWidth()<=dx+dialog.GetWidth()
            assert y+combo.listBox.GetHeight()+3<=min(h,dy+dialog.GetHeight())
            assert 0<=x and x+combo.listBox.GetWidth()<=w
            checked += 1
            combo.CloseListBox()
        emit('popup_bounds_pass',viewport=(w,h),controls=checked)

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
        chosen = dialog.resolutions.index((1920, 1080))
        expected = dialog.resolutions[chosen]
        yield from self.click(combo.listBox, 20, (chosen - combo.listBox.basePos) * combo.listBox.stepSize + 8)
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
        yield from self.click(combo.listBox, 20, (chosen - combo.listBox.basePos) * combo.listBox.stepSize + 8)
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
            with builtins.old_open('closure-failure.log', 'w') as stream:
                stream.write(traceback.format_exc())
            self.done = True
            app.Exit()

    def OnRender(self):
        if self.shot:
            ok, path = grp.SaveScreenShotToPath(self.shot + '-')
            emit('screenshot', ok=ok, path=path)
            self.shot = None

probe = Probe()
app.Loop()
probe.Hide()
world.Close()
log.close()
