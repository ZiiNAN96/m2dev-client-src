"""Short GR2 + freshly converted OBJ runtime fixture; login remains a manual check."""
import app, background, builtins, chr, chrmgr, grp, item, player
import playersettingmodule, systemSetting, time, ui, wndMgr

width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
wndMgr.SetScreenSize(width, height)
app.Create("ZiiNAN Offline Asset Pipeline E2-X", width, height, 1)
app.SetCameraMaxDistance(40000.0)
app.SetSightRange(24000)
app.SetHairColorEnable(True)
if not app.LoadLocaleData(app.GetLocalePath()):
    raise RuntimeError("Original locale required")
playersettingmodule.__LoadGameNPC()
playersettingmodule.__LoadGameEffect()
item.LoadItemTable(app.GetLocalePath() + "/item_proto")
chrmgr.CreateRace(0)
chrmgr.SelectRace(0)
chrmgr.LoadLocalRaceData("msm/warrior_m.msm")
playersettingmodule.__LoadGameWarriorEx(0, "d:/ymir work/pc/warrior/")
chrmgr.CreateRace(57999)
chrmgr.SelectRace(57999)
chrmgr.LoadLocalRaceData("assettool/market_stall.msm")
SCENES = (("a1", 44000, 27200, 0), ("b1", 70400, 53600, 20104),
          ("a1", 44000, 27200, 0))
log = builtins.old_open("asset-runtime-smoke.log", "w")


class World(ui.Window):
    def __init__(self):
        ui.Window.__init__(self)
        self.SetSize(width, height)
        self.Show()
        self.started = time.monotonic()
        self.phase = self.step = self.shot = -1
        self.position = (0, 0, 0)
        self.camera = (1000, 20, 0)
        self.frames = 0
        self.label = ui.TextLine()
        self.label.SetParent(self)
        self.label.SetPosition(20, 20)
        self.label.SetOutline()
        self.label.Show()

    def OnUpdate(self):
        elapsed = time.monotonic() - self.started
        phase = int(elapsed / 20)
        if phase >= len(SCENES):
            app.Exit()
            return
        name, x, y, mount = SCENES[phase]
        if phase != self.phase:
            chr.Destroy()
            if self.phase >= 0:
                background.Destroy()
            background.Initialize()
            background.LoadMap("metin2_map_" + name, float(x), float(y), 0.0)
            background.SetViewDistanceSet(background.DISTANCE0, 24000.0)
            background.SelectViewDistanceNum(background.DISTANCE0)
            for index, (race, kind) in enumerate(((0, 6), (9003, 1), (101, 0), (691, 0), (20101, 1))):
                vid = 57900 + index
                chr.CreateInstance(vid, {"horse": mount if kind == 6 else 0})
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
                px = x + index * 250
                chr.SetPixelPosition(int(px), int(y), int(background.GetHeight(px, y)))
                chr.Show()
            # ZiiNAN: Offline asset pipeline proof uses the existing rigid NPC consumer.
            chr.CreateInstance(57999)
            chr.SelectInstance(57999)
            chr.SetVirtualID(57999)
            chr.SetInstanceType(1)
            chr.SetRace(57999)
            chr.SetArmor(0)
            chr.SetPixelPosition(int(x - 350), int(y + 100), int(background.GetHeight(x - 350, y + 100)))
            chr.Show()
            log.write("converted phase=%d race=57999 vid=57999 source=market_stall.obj glb=assettool/market_stall.glb\n" % phase)
            player.SetMainCharacterIndex(57900)
            self.phase, self.step = phase, -1
            log.write("phase=%d map=%s mount=%d actors=5\n" % (phase, name, mount))
            log.flush()
        seconds = elapsed - phase * 20
        step = min(2, int(seconds / 6))
        if step != self.step:
            chr.SelectInstance(57900)
            chr.ChangeShape(11219 if step == 1 else 0)
            chr.SetHair(1001)
            chr.SetWeapon(19)
            chr.SetLoopMotion(chr.MOTION_RUN if step == 1 else chr.MOTION_WAIT)
            self.step = step
            log.write("transition phase=%d step=%d armor=%d camera=%s\n" %
                      (phase, step, 11219 if step == 1 else 0, "far" if step == 1 else "near"))
            log.flush()
        z = background.GetHeight(x, y)
        self.position = (x, y, z)
        self.camera = (6500, 35, 0) if step == 1 else (1000, 20, 0)
        app.SetCenterPosition(x, -y, z + 100)
        app.SetCamera(*self.camera, 0.0)
        background.Update(x, -y, z)
        chr.Update()
        self.label.SetText("E2-X %s: converted OBJ stall + original GR2 player / NPC / mob / boss / mount" % name)
        shot = "far" if step == 1 and seconds >= 9 else "near" if step == 2 and seconds >= 16 else None
        if shot and self.shot != (phase, shot):
            self.shot = (phase, shot)
            success, path = grp.SaveScreenShotToPath("e2x-world-%d-%s-" % (phase, shot))
            log.write("screenshot phase=%d camera=%s success=%d file=%s\n" % (phase, shot, bool(success), path))
            log.flush()
            if not success:
                raise RuntimeError("Required E2-X %s screenshot failed" % shot)

    def OnRender(self):
        x, y, z = self.position
        grp.SetPositionCamera(x, -y, z + 100, *self.camera)
        app.RenderGame()
        grp.SetInterfaceRenderState()
        self.frames += 1

    def OnPressEscapeKey(self):
        app.Exit()
        return True


window = World()
app.Loop()
chr.Destroy()
background.Destroy()
window.label.Hide()
window.label.Destroy()
window.Hide()
window.Destroy()
log.write("completed phases=%d frames=%d\n" % (window.phase + 1, window.frames))
log.close()
