"""Bounded A1 pacing/settings smoke. Real native motion; no network login."""
import app, background, builtins, chr, chrmgr, effect, grp, item, player
import playersettingmodule, systemSetting, time, ui, wndMgr, json, os, traceback
import mouseModule, uisystemoption

width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
wndMgr.SetScreenSize(width, height)
app.Create("P3 Frame Pacing", width, height, 1)
app.SetMouseHandler(mouseModule.mouseController)
wndMgr.SetMouseHandler(mouseModule.mouseController)
assert mouseModule.mouseController.Create()
app.SetCameraMaxDistance(40000.0)
app.SetSightRange(25600)
app.SetHairColorEnable(True)
app.SetArmorSpecularEnable(True)
assert app.LoadLocaleData(app.GetLocalePath())
item.LoadItemTable(app.GetLocalePath() + "/item_proto")
getattr(playersettingmodule, "__LoadGameNPC")()
getattr(playersettingmodule, "__LoadGameEffect")()
for name in ("INIT", "WARRIOR", "ASSASSIN", "SURA", "SHAMAN"):
    playersettingmodule.LoadGameData(name)

restart = os.path.exists("p3-expected.json")
initial = systemSetting.GetGraphicsSettings()
if restart:
    with builtins.old_open("p3-expected.json") as stream:
        assert initial == json.load(stream), "Restart lost settings"
log = builtins.old_open("p3-restart.jsonl" if restart else "p3-run.jsonl", "w")
def emit(**fields):
    fields["ns"] = time.perf_counter_ns()
    log.write(json.dumps(fields) + "\n")
    log.flush()
def failure():
    with builtins.old_open("p3-failure.log", "w") as stream:
        stream.write(traceback.format_exc())
    app.Exit()

class Animation(ui.AniImageBox):
    def __init__(self):
        ui.AniImageBox.__init__(self)
        self.loops = 0
        self.SetDelay(2)
        for index in range(4):
            self.AppendImage("d:/ymir work/ui/public/slotfinishcooltimeeffect/%02d.sub" % index)
        self.SetPosition(20, 20)
        self.Show()
    def OnEndFrame(self):
        self.loops += 1

MODES = [(0, 0), (1, 0), (2, 0), (0, 1), (1, 1), (2, 1)]
PHASES = ("IDLE", "CAMERA", "RUN", "COMBAT")
if restart:
    # The second process verifies the saved On value, then saves Off.
    # A third process verifies the non-default Off value as well.
    MODES = [(2, 0)]
    PHASES = ("IDLE",)
if os.path.exists("p3-pilot") and not restart:
    MODES = [(2, 0)]
ui_only = os.path.exists("p3-ui-only")
if ui_only:
    MODES, PHASES = [(0, 0), (1, 0), (2, 0)], ("IDLE",)

class World(ui.Window):
    def __init__(self):
        ui.Window.__init__(self)
        self.SetSize(width, height)
        self.Show()
        background.Initialize()
        background.LoadMap("metin2_map_a1", 63500.0, 59000.0, 0.0)
        self.origin = (63500, 59000, background.GetHeight(63500, 59000))
        self.effects = []
        self.rows = []
        self.completed = False
        self.stage = -1
        self.options = uisystemoption.OptionDialog()
        self.options.OpenGraphics()
        self.dialog = self.options.graphicsDialog
        self.dialog.SetPosition(width - 370, 15)
        self.animation = Animation()
        self.chat_window = None
        if ui_only:
            import uichat
            self.chat_window = uichat.ChatWindow()
            self.chat_window.SetSize(500, 150)
            self.chat_window.SetPosition(20, height - 70)
            self.chat_window.Show()
        self.slot = ui.SlotWindow()
        self.slot.SetPosition(20, 60)
        self.slot.AppendSlot(0, 0, 0, 32, 32)
        self.slot.SetItemSlot(0, 19)
        self.slot.Show()
        self.next_stage()

    def clear_effects(self):
        for index in self.effects:
            effect.DeleteEffect(index)
        self.effects = []

    def next_stage(self):
        app.RotateCamera(0)
        if self.stage >= 0:
            emit(event="end", stage=self.stage, mode=self.mode, phase=self.phase,
                 state=systemSetting.TestActorTiming(57900), game=app.GetTime(),
                 camera=app.GetCamera(), ui_loops=self.animation.loops, updates=self.updates,
                 wall=time.monotonic() - self.measure_start, rows=self.rows, chat_tick_check=ui_only)
        self.stage += 1
        if self.stage == len(MODES) * len(PHASES):
            self.completed = True
            self.dialog.Close()
            assert not self.dialog.IsShow(), "UI save failed"
            saved = systemSetting.GetGraphicsSettings()
            assert systemSetting.LoadGraphicsSettings()
            assert saved == systemSetting.GetGraphicsSettings(), "File reload lost settings"
            with builtins.old_open("p3-expected.json", "w") as stream:
                json.dump(saved, stream)
            emit(event="completed", stages=self.stage, restart=restart)
            app.Exit()
            return
        self.mode = MODES[self.stage // len(PHASES)]
        self.phase = PHASES[self.stage % len(PHASES)]
        self.dialog.frameRateLimit.SelectItem(self.mode[0])
        self.dialog.vsync.SelectItem(self.mode[1])
        actual = systemSetting.GetGraphicsSettings()
        assert (actual["frameRateLimit"], actual["vsync"]) == self.mode
        for key in initial:
            if key not in ("frameRateLimit", "vsync"):
                assert actual[key] == initial[key], "Quality changed: " + key
        self.dialog.Show() if self.phase == "IDLE" else self.dialog.Hide()
        self.clear_effects()
        chr.Destroy()
        chr.CreateInstance(57900)
        chr.SelectInstance(57900)
        chr.SetVirtualID(57900)
        chr.SetInstanceType(6)
        chr.SetRace(0)
        chr.SetArmor(11299)
        chr.SetHair(1001)
        chr.SetWeapon(19)
        chr.SetMoveSpeed(100)
        chr.SetAttackSpeed(100)
        chr.SetMotionMode(chr.MOTION_MODE_ONEHAND_SWORD)
        chr.SetLoopMotion(chr.MOTION_WAIT)
        chr.SetPixelPosition(*map(int, self.origin))
        chr.Show()
        player.SetMainCharacterIndex(57900)
        app.SetCenterPosition(self.origin[0], -self.origin[1], self.origin[2] + 100)
        app.SetCamera(5500.0, 22.0, 0.0, 0.0)
        self.prewarm = True
        self.ready = False
        self.measure_start = None
        self.last_attack = -1
        self.updates = 0
        self.rows = []
        self.shot = False
        self.started = time.monotonic()

    def OnUpdate(self):
        try:
            self.update()
        except Exception:
            failure()

    def update(self):
        if self.completed:
            return
        now = time.monotonic()
        if self.ready and self.measure_start is None and now - self.started >= 1.0:
            self.measure_start = now
            runtime = systemSetting.GetGraphicsRuntimeConfig()
            assert (runtime["frameRateLimit"], runtime["vsync"]) == self.mode, "Runtime apply failed"
            self.slot.SetSlotCoolTime(0, 2.0)
            if self.chat_window:
                self.chat_window.heightBar = 200
                self.chat_window.curHeightBar = 0
                self.chat_height = 0
            if self.phase == "RUN":
                chr.MoveToDestPosition(57900, self.origin[0] + 2500, self.origin[1])
            if self.phase == "CAMERA":
                app.RotateCamera(1)
            emit(event="start", stage=self.stage, mode=self.mode, phase=self.phase,
                 state=systemSetting.TestActorTiming(57900), game=app.GetTime(),
                 camera=app.GetCamera(), ui_loops=self.animation.loops, settings=runtime)
        t = now - self.measure_start if self.measure_start is not None else 0
        if self.measure_start is not None and t >= 4.0:
            self.next_stage()
            return
        if self.phase == "COMBAT" and self.measure_start is not None and int(t / 1.1) != self.last_attack:
            self.last_attack = int(t / 1.1)
            chr.SelectInstance(57900)
            chr.PushOnceMotion(chr.MOTION_COMBO_ATTACK_1, 0.0)
            self.clear_effects()
            x, y, z = chr.GetPixelPosition(57900)
            chr.SetPixelPosition(int(x), int(-y), int(z + 100))
            for path in ("d:/ymir work/pc/warrior/effect/samyeon_d.mse", "d:/ymir work/pc/warrior/effect/palbang_spin.mse"):
                self.effects.append(effect.CreateEffect(path))
            chr.SetPixelPosition(int(x), int(y), int(z))
        x, y, z = chr.GetPixelPosition(57900)
        background.Update(x, -y, z)
        chr.Update()
        effect.Update()
        x, y, z = chr.GetPixelPosition(57900)
        app.SetCenterPosition(x, -y, z + 100)
        if self.measure_start is not None:
            if self.chat_window:
                assert self.chat_window.curHeightBar == self.chat_height, "Chat animation depends on render frames"
                self.chat_height += (200 - self.chat_height) // 10
            self.updates += 1
            state = systemSetting.TestActorTiming(57900)
            state.update(t=t, game=app.GetTime(), camera=app.GetCamera()[2], ui_loops=self.animation.loops)
            self.rows.append(state)

    def OnRender(self):
        if self.completed:
            return
        try:
            x, y, z = chr.GetPixelPosition(57900)
            distance, pitch, rotation, _ = app.GetCamera()
            grp.SetPositionCamera(x, -y, z + 100, distance, pitch, rotation)
            if self.prewarm:
                assert chrmgr.PrewarmVisibleActors(True), "Actor prewarm failed"
                self.prewarm = False
                self.started = time.monotonic()
                self.ready = True
            app.RenderGame()
            grp.SetInterfaceRenderState()
        except Exception:
            failure()

class Capture(ui.Window):
    def __init__(self, world):
        ui.Window.__init__(self, "TOP_MOST")
        self.world = world
        self.Show()
    def OnRender(self):
        w = self.world
        if w.ready and not w.shot and w.measure_start is None and time.monotonic() - w.started > .5:
            ok, path = grp.SaveScreenShotToPath("p3-%02d-" % w.stage)
            if not ok:
                raise RuntimeError("Screenshot failed")
            emit(event="image", stage=w.stage, path=path)
            w.shot = True

world = World()
capture = Capture(world)
app.Loop()
world.clear_effects()
world.options.Destroy()
if world.chat_window:
    world.chat_window.Hide()
    world.chat_window.Destroy()
world.animation.Hide()
world.slot.Hide()
capture.Hide()
world.Hide()
chr.Destroy()
background.Destroy()
log.close()
