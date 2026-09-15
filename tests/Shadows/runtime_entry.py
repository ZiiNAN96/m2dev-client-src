"""Bounded G34 production-client proof: Classic, G2, G34 for each subject. Fixed camera, actor pose and wind in A/B pairs."""
import app, background, builtins, chr, chrmgr, grp, item, json, mouseModule, player
import playersettingmodule, systemSetting, time, ui, uisystemoption, wndMgr

width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
wndMgr.SetScreenSize(width, height)
app.Create("G34-X Modern Lighting / Classic + Modern", width, height, 1)
app.SetMouseHandler(mouseModule.mouseController)
wndMgr.SetMouseHandler(mouseModule.mouseController)
assert mouseModule.mouseController.Create()
app.SetCameraMaxDistance(20000.0)
app.SetHairColorEnable(True)
assert app.LoadLocaleData(app.GetLocalePath())
playersettingmodule.__LoadGameNPC()
playersettingmodule.__LoadGameEffect()
item.LoadItemTable(app.GetLocalePath() + "/item_proto")
for phase in ("INIT", "WARRIOR", "ASSASSIN", "SURA", "SHAMAN"):
    playersettingmodule.LoadGameData(phase)
chrmgr.CreateRace(65000)
chrmgr.SelectRace(65000)
chrmgr.LoadLocalRaceData("f5x/character.msm")
chrmgr.SetPathName("f5x/")
chrmgr.RegisterMotionMode(chr.MOTION_MODE_GENERAL)
for motion, name in ((chr.MOTION_WAIT, "idle"), (chr.MOTION_WALK, "walk"), (chr.MOTION_NORMAL_ATTACK, "attack")):
    chrmgr.RegisterMotionData(chr.MOTION_MODE_GENERAL, motion, name + ".msa", 100)
try:
    with builtins.old_open("g34x-expected.json") as stream:
        saved = json.load(stream)
except FileNotFoundError:
    saved = None
restart = saved["restart"] if saved else 0
if saved:
    assert systemSetting.GetGraphicsSettings() == saved["settings"], "Persisted style/settings changed"
log = builtins.old_open("g34x-run-%d.log" % restart, "w")
steps = 2 if restart else 36


class World(ui.Window):
    def __init__(self):
        ui.Window.__init__(self)
        self.SetSize(width, height)
        self.Show()
        self.step = -1
        self.frames = 0
        self.shots = set()
        self.map = None
        self.started = 0
        self.options = uisystemoption.OptionDialog()
        self.options.SetPosition(15, 80)
        self.options.Show()
        self.options.OpenGraphics()
        self.options.graphicsDialog.SetPosition(350, 80)

    def Load(self, name):
        chr.Destroy()
        if self.map:
            background.Destroy()
        background.Initialize()
        x, y = (65000, 59000) if name == "a1" else (67000, 52000)
        background.LoadMap("metin2_map_" + name, float(x), float(y), 0.0)
        background.SetViewDistanceSet(background.DISTANCE0, 24000.0)
        background.SelectViewDistanceNum(background.DISTANCE0)
        self.position = (x, y, background.GetHeight(x, y))
        for index, (race, kind) in enumerate(((0, 6), (9003, 1), (110, 0), (691, 0), (20101, 1), (0, 6), (65000, 0))):
            vid = 58200 + index
            mount = 20104 if index == 5 else 0
            chr.CreateInstance(vid, {"horse": mount})
            chr.SelectInstance(vid)
            chr.SetVirtualID(vid)
            chr.SetInstanceType(kind)
            chr.SetRace(race)
            if kind == 6:
                chr.ChangeShape(0)
                chr.SetHair(1001)
                chr.SetWeapon(19)
                chr.SetMotionMode(chr.MOTION_MODE_HORSE_ONEHAND_SWORD if mount else chr.MOTION_MODE_ONEHAND_SWORD)
            else:
                chr.SetArmor(0)
                chr.SetMotionMode(chr.MOTION_MODE_GENERAL)
            chr.SetLoopMotion(chr.MOTION_WAIT)
            px = x + index * 220
            chr.SetPixelPosition(int(px), int(y), int(background.GetHeight(px, y)))
            chr.Show()
        player.SetMainCharacterIndex(58200)
        self.map = name
        chr.Update()
        log.write("map=%s actors=7 player/bear/npc/boss/weapon/hair/mount/GLB\n" % name)

    def OnUpdate(self):
        if time.monotonic() - self.started >= 1.0 and (self.step < 0 or self.step in self.shots):
            self.step += 1
            if self.step >= steps:
                app.Exit()
                return
            stage = self.step
            subject = stage // 4
            mode = stage % 4
            name = "b1" if subject in (3, 4) else "a1"
            if name != self.map:
                self.Load(name)
            modern = mode != 0
            if restart:
                modern = restart == 1
                mode = 3 if modern else 0
            self.options.graphicsDialog.style.SelectItem(int(modern))
            self.options.graphicsDialog.shadows.SelectItem(3 if mode >= 2 else 0)
            self.options.graphicsDialog.ao.SelectItem(2 if mode == 3 else 0)
            assert systemSetting.GetGraphicsSettings()["style"] == int(modern)
            x, y, z = self.position
            focus = 2 if subject == 1 else 6 if subject in (5, 6, 7) else 0
            self.focus = (x + focus * 220, y, z + 70)
            self.camera = (1000, 20, 0) if subject in (0, 1, 5, 6, 7) else (6000, 35, 0)
            if subject == 4:
                self.focus = (68903, 53126, 19936)
                self.camera = (4000, 15, 0)
            if subject in (6, 7):
                chr.SelectInstance(58206)
                chr.SetLoopMotion(chr.MOTION_WALK if subject == 6 else chr.MOTION_NORMAL_ATTACK)
            if subject == 8:
                self.camera = (1900, 30, stage * 4)
                result = systemSetting.TestGraphicsWindow()
                assert all(result.values())
                log.write("window style=%s result=%s\n" % ("Modern" if modern else "Classic", json.dumps(result, sort_keys=True)))
                self.options.Show()
                self.options.OpenGraphics()
            else:
                self.options.Close()
            self.started = time.monotonic()
            log.write("step=%d map=%s style=%s camera=%s focus=%s\n" % (stage, name, "Modern" if modern else "Classic", self.camera, self.focus))
            log.flush()
        x, y, z = self.position
        fx, fy, fz = self.focus
        app.SetCenterPosition(fx, -fy, fz)
        app.SetCamera(*self.camera, 0.0)
        background.Update(x, -y, z)
        frozen = bool(restart) or self.step // 4 not in (6, 7)
        systemSetting.SetLightingProof(self.step, int(frozen))
        if not frozen:
            chr.Update()

    def OnRender(self):
        fx, fy, fz = self.focus
        grp.SetPositionCamera(fx, -fy, fz, *self.camera)
        app.RenderGame()
        grp.SetInterfaceRenderState()
        self.frames += 1


class Capture(ui.Window):
    def __init__(self, world):
        ui.Window.__init__(self, "TOP_MOST")
        self.world = world
        self.Show()

    def OnRender(self):
        world = self.world
        if 0 <= world.step < steps and world.step not in world.shots and time.monotonic() - world.started > .45:
            success, path = grp.SaveScreenShotToPath("g34x-%d-%02d-" % (restart, world.step))
            assert success, "Native screenshot failed"
            world.shots.add(world.step)
            proof = systemSetting.GetLightingProof()
            depth = systemSetting.GetDepthProof()
            if systemSetting.GetGraphicsSettings()["style"] == 1 and world.step % 4 == 3:
                assert depth["shadowDraws"] > 0 and depth["aoTargets"] == 4, str(depth)
            assert proof["buffers"] == 1, "Scene light buffer not shared"
            log.write("capture=" + json.dumps(dict(step=world.step, map=world.map, image=path, lighting=proof, depth=depth), sort_keys=True) + "\n")
            log.flush()


world = World()
capture = Capture(world)
app.Loop()
# Three real processes prove persistence first for Modern, then for Classic.
if restart == 1:
    assert systemSetting.ApplyGraphicsSettings({"style": 0})
assert systemSetting.SaveGraphicsSettings()
with builtins.old_open("g34x-expected.json", "w") as stream:
    json.dump(dict(restart=restart + 1, settings=systemSetting.GetGraphicsSettings()), stream)
chr.Destroy()
background.Destroy()
world.options.Close()
world.options.Destroy()
world.options = None
capture.Hide()
capture.Destroy()
world.Hide()
world.Destroy()
log.write("PASS steps=%d frames=%d screenshots=%d restart=%d\n" % (world.step, world.frames, len(world.shots), restart))
log.close()
