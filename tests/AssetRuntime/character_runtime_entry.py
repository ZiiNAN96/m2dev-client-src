"""F5-X bounded native client scene. Real CInstanceBase/CActorInstance/app.RenderGame.

Only the private smoke root registers race 65000. No server or production races change.
"""
import app, background, builtins, chr, chrmgr, grp, player
import systemSetting, time, ui, wndMgr

width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
wndMgr.SetScreenSize(width, height)
app.Create("F5-X Animated GLB Characters", width, height, 1)
app.SetCameraMaxDistance(12000.0)
app.SetSightRange(24000)
if not app.LoadLocaleData(app.GetLocalePath()):
    raise RuntimeError("Original locale required")
chrmgr.CreateRace(65000)
chrmgr.SelectRace(65000)
chrmgr.LoadLocalRaceData("f5x/character.msm")
chrmgr.SetPathName("f5x/")
chrmgr.RegisterMotionMode(chr.MOTION_MODE_GENERAL)
MOTIONS = (chr.MOTION_WAIT, chr.MOTION_WALK, chr.MOTION_RUN,
           chr.MOTION_NORMAL_ATTACK, chr.MOTION_DAMAGE, chr.MOTION_DEAD)
for motion, name in zip(MOTIONS, ("idle", "walk", "run", "attack", "damage", "death")):
    chrmgr.RegisterMotionData(chr.MOTION_MODE_GENERAL, motion, name + ".msa", 100)
background.Initialize()
x, y = 44000, 27200
background.LoadMap("metin2_map_a1", float(x), float(y), 0.0)
background.SetViewDistanceSet(background.DISTANCE0, 24000.0)
background.SelectViewDistanceNum(background.DISTANCE0)
for i in range(20):
    vid = 58000 + i
    chr.CreateInstance(vid)
    chr.SelectInstance(vid)
    chr.SetVirtualID(vid)
    chr.SetInstanceType(0)
    chr.SetRace(65000)
    chr.SetArmor(0)
    chr.SetMotionMode(chr.MOTION_MODE_GENERAL)
    px = x if i == 0 else x + 400 + ((i - 1) % 5) * 210
    py = y if i == 0 else y + ((i - 1) // 5) * 220
    chr.SetPixelPosition(int(px), int(py), int(background.GetHeight(px, py)))
    chr.SetLoopMotion(MOTIONS[i % 3])
    chr.Show()
player.SetMainCharacterIndex(58000)
log = builtins.old_open("f5x-character-smoke.log", "w")
log.write("race=65000 actors=20 asset=f5x/character.glb motionMapping=MSA-clip-index\n")
log.flush()

# Loop/one-shot policy stays in the character definition/test sequence.
SEQUENCE = (("Idle", 0, 2.0), ("Walk", 1, 2.0), ("Run", 2, 2.0), ("Idle", 0, 1.0),
            ("Attack", 3, 1.4), ("Idle", 0, 1.0), ("Damage", 4, 1.3), ("Death", 5, 2.2),
            ("MultiInstance", None, 3.0))


class Scene(ui.Window):
    def __init__(self):
        ui.Window.__init__(self)
        self.SetSize(width, height)
        self.Show()
        self.started = time.monotonic()
        self.stage = -1
        self.frames = 0
        self.shots = set()
        self.camera = (900, 15, 0)
        self.center = (x, y, background.GetHeight(x, y))
        self.label = ui.TextLine()
        self.label.SetParent(self)
        self.label.SetPosition(20, 20)
        self.label.SetOutline()
        self.label.Show()

    def OnUpdate(self):
        elapsed = time.monotonic() - self.started
        offset = 0
        for stage, (name, clip, duration) in enumerate(SEQUENCE):
            if elapsed < offset + duration:
                break
            offset += duration
        else:
            app.Exit()
            return
        self.stage_time = elapsed - offset
        if stage != self.stage:
            self.stage = stage
            chr.SelectInstance(58000)
            if clip is not None:
                if clip < 3:
                    chr.BlendLoopMotion(MOTIONS[clip], .2)
                elif clip == 5:
                    chr.Die()
                else:
                    chr.PushOnceMotion(MOTIONS[clip], .1)
            self.label.SetText("F5-X GLB / shared Animation Runtime / GPU: " + name)
            log.write("stage=%d motion=%s clip=%s\n" % (stage, name, clip))
            log.flush()
        if name == "MultiInstance":
            self.center = (x + 600, y + 300, background.GetHeight(x + 600, y + 300))
            self.camera = (2300, 30, 0)
        px, py, pz = self.center
        app.SetCenterPosition(px, -py, pz + 100)
        app.SetCamera(*self.camera, 0.0)
        background.Update(px, -py, pz)
        chr.Update()

    def OnRender(self):
        px, py, pz = self.center
        grp.SetPositionCamera(px, -py, pz + 100, *self.camera)
        app.RenderGame()
        grp.SetInterfaceRenderState()
        self.frames += 1
        if self.stage >= 0 and self.stage not in self.shots:
            threshold = 1.6 if SEQUENCE[self.stage][0] == "Death" else .25
            if self.stage_time >= threshold:
                success, path = grp.SaveScreenShotToPath("f5x-native-%d-" % self.stage)
                if not success:
                    raise RuntimeError("Native character screenshot failed")
                self.shots.add(self.stage)
                log.write("screenshot stage=%d file=%s\n" % (self.stage, path))
                log.flush()

    def OnPressEscapeKey(self):
        app.Exit()
        return True


scene = Scene()
app.Loop()
chr.Destroy()
background.Destroy()
scene.label.Hide()
scene.label.Destroy()
scene.Hide()
scene.Destroy()
log.write("completed stages=%d screenshots=%d frames=%d\n" % (scene.stage + 1, len(scene.shots), scene.frames))
log.close()
