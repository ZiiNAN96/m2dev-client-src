"""ZiiNAN: GPU skinning production path; fixed original-map CPU/GPU benchmark."""
import app, background, builtins, chr, chrmgr, grp, player
import playersettingmodule, ui, wndMgr

WIDTH, HEIGHT = 1280, 720
wndMgr.SetScreenSize(WIDTH, HEIGHT)
app.Create("Metin2 GPU skinning B6-X benchmark", WIDTH, HEIGHT, 1)
app.SetCameraMaxDistance(40000.0)
app.SetSightRange(24000)
app.SetFPS(120)
app.LoadLocaleData(app.GetLocalePath())
playersettingmodule.__LoadGameNPC()
chrmgr.CreateRace(0)
chrmgr.SelectRace(0)
chrmgr.LoadLocalRaceData("msm/warrior_m.msm")
playersettingmodule.__LoadGameWarriorEx(0, "d:/ymir work/pc/warrior/")
COUNTS = (1, 10, 25, 50, 100)
WARMUP, SAMPLES = 120, 360
X, Y = 44000, 27200
log = builtins.old_open("gpu-skinning-benchmark-test.log", "w")
background.Initialize()
background.LoadMap("metin2_map_a1", float(X), float(Y), 0.0)
background.SetViewDistanceSet(background.DISTANCE0, 24000.0)
background.SelectViewDistanceNum(background.DISTANCE0)


class World(ui.Window):
    def __init__(self):
        ui.Window.__init__(self)
        self.SetSize(WIDTH, HEIGHT)
        self.Show()
        self.phase = -1
        self.frame = 0
        self.actors = []
        self.z = background.GetHeight(X, Y)

    def populate(self, count):
        chr.Destroy()
        self.actors = []
        bodies = 0
        while bodies < count:
            index = len(self.actors)
            race, kind = ((0, 6), (9003, 1), (101, 0), (0, 6))[index % 4]
            mount = 20101 if index % 4 == 3 and bodies + 2 <= count else 0
            vid = 58600 + index
            chr.CreateInstance(vid, {"horse": mount})
            chr.SelectInstance(vid)
            chr.SetVirtualID(vid)
            chr.SetInstanceType(kind)
            chr.SetRace(race)
            if kind == 6:
                chr.ChangeShape(0)
            else:
                chr.SetArmor(0)
            chr.SetMotionMode(chr.MOTION_MODE_HORSE if mount else chr.MOTION_MODE_GENERAL)
            chr.SetLoopMotion(chr.MOTION_WAIT if kind == 1 else chr.MOTION_RUN)
            px, py = X + (index % 10 - 4.5) * 220, Y + (index // 10 - 4.5) * 190
            chr.SetPixelPosition(int(px), int(py), int(background.GetHeight(px, py)))
            chr.Show()
            self.actors.append((vid, int(px), int(py), int(background.GetHeight(px, py))))
            bodies += 2 if mount else 1
        player.SetMainCharacterIndex(58600)
        log.write("phase=%d requestedBodies=%d instances=%d\n" % (self.phase, bodies, len(self.actors)))
        log.flush()

    def OnUpdate(self):
        phase = self.frame // (WARMUP + SAMPLES)
        if phase >= len(COUNTS):
            app.Exit()
            return
        if phase != self.phase:
            self.phase = phase
            self.populate(COUNTS[phase])
        sample = self.frame % (WARMUP + SAMPLES) - WARMUP
        app.SkinningBenchmarkStage(COUNTS[phase], sample)
        app.SetCenterPosition(X, -Y, self.z + 100)
        app.SetCamera(6000.0, 55.0, 0.0, 0.0)
        background.Update(X, -Y, self.z)
        for vid, px, py, pz in self.actors:
            chr.SelectInstance(vid)
            chr.SetPixelPosition(px, py, pz)
        chr.Update()
        self.frame += 1

    def OnRender(self):
        grp.SetPositionCamera(X, -Y, self.z + 100, 6000.0, 55.0, 0.0)
        app.RenderGame()
        grp.SetInterfaceRenderState()

    def OnPressEscapeKey(self):
        app.Exit()
        return True


window = World()
app.StartSkinningBenchmark()
app.Loop()
chr.Destroy()
background.Destroy()
window.Hide()
window.Destroy()
log.write("completed phases=%d frames=%d\n" % (window.phase + 1, window.frame))
log.close()
