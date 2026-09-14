"""Twenty-second pack-backed Redthief fixture; real login is checked separately."""
import app, background, builtins, chr, grp, playersettingmodule, systemSetting, time, ui, wndMgr

width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
wndMgr.SetScreenSize(width, height)
app.Create("ZiiNAN F3-B Redthief", width, height, 1)
app.SetCameraMaxDistance(12000.0)
app.SetSightRange(16000)
if not app.LoadLocaleData(app.GetLocalePath()):
    raise RuntimeError("Original locale required")
playersettingmodule.__LoadGameNPC()
playersettingmodule.__LoadGameEffect()
background.Initialize()
background.LoadMap("metin2_map_a1", 44000.0, 27200.0, 0.0)
background.SetViewDistanceSet(background.DISTANCE0, 16000.0)
background.SelectViewDistanceNum(background.DISTANCE0)
log = builtins.old_open("redthief-smoke.log", "w")
for index, race in enumerate((3505, 3555, 3909)):
    vid = 58300 + index
    chr.CreateInstance(vid)
    chr.SelectInstance(vid)
    chr.SetVirtualID(vid)
    chr.SetInstanceType(0)
    chr.SetRace(race)
    chr.SetArmor(0)
    chr.SetMotionMode(chr.MOTION_MODE_GENERAL)
    chr.SetLoopMotion(chr.MOTION_WAIT)
    x, y = 44000 + index * 280, 27200
    chr.SetPixelPosition(x, y, int(background.GetHeight(x, y)))
    chr.Show()
    log.write("created race=%d vid=%d\n" % (race, vid))
log.flush()


class World(ui.Window):
    def __init__(self):
        ui.Window.__init__(self)
        self.SetSize(width, height)
        self.Show()
        self.started = time.monotonic()
        self.stage = -1
        self.frames = [0] * 5
        self.shot = False

    def OnUpdate(self):
        elapsed = time.monotonic() - self.started
        if elapsed >= 20:
            app.Exit()
            return
        stage = min(4, int(elapsed / 4))
        if stage != self.stage:
            motion = (chr.MOTION_DAMAGE, chr.MOTION_DAMAGE_BACK, 25, chr.MOTION_DAMAGE, chr.MOTION_DAMAGE_BACK)[stage]
            for vid in (58300, 58301, 58302):
                chr.SelectInstance(vid)
                chr.PushOnceMotion(motion, 0.1)
                chr.PushLoopMotion(chr.MOTION_WAIT, 0.1)
            self.stage = stage
            log.write("queued once motion=%d stage=%d races=3505,3555,3909\n" % (motion, stage))
            log.flush()
        z = background.GetHeight(44280, 27200)
        app.SetCenterPosition(44280, -27200, z + 100)
        app.SetCamera(2200.0, 25.0, 0.0, 0.0)
        background.Update(44280, -27200, z)
        chr.Update()

    def OnRender(self):
        z = background.GetHeight(44280, 27200)
        grp.SetPositionCamera(44280, -27200, z + 100, 2200.0, 25.0, 0.0)
        app.RenderGame()
        grp.SetInterfaceRenderState()
        if self.stage >= 0:
            self.frames[self.stage] += 1
        if not self.shot and self.stage == 3:
            success, path = grp.SaveScreenShotToPath("f3b-redthief-")
            if success:
                self.shot = True
                log.write("screenshot=%s\n" % path)
                log.flush()


window = World()
app.Loop()
chr.Destroy()
background.Destroy()
window.Hide()
window.Destroy()
log.write("completed frames=%s\n" % ",".join(str(x) for x in window.frames))
log.close()
