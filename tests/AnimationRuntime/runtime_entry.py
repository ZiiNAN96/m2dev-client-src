"""Short original-asset runtime fixture; login and window interaction remain manual checks."""
import app, background, builtins, chr, chrmgr, grp, item, player
import playersettingmodule, systemSetting, time, ui, wndMgr

width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
wndMgr.SetScreenSize(width, height)
app.Create("ZiiNAN Asset Runtime F1-X", width, height, 1)
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
SCENES = (("a1", 44000, 27200, 0),)
log = builtins.old_open("asset-runtime-smoke.log", "w")


class World(ui.Window):
    def __init__(self):
        ui.Window.__init__(self)
        self.SetSize(width, height)
        self.Show()
        self.started = time.monotonic()
        self.phase = self.step = self.shot = -1
        self.motion = None
        self.position = (0, 0, 0)
        self.camera = (1000, 20, 0)
        self.frames = 0
        self.motion_frames = [0, 0, 0, 0]
        self.motion_step = 0
        self.pending_shot = None
        self.label = ui.TextLine()
        self.label.SetParent(self)
        self.label.SetPosition(20, 20)
        self.label.SetOutline()
        self.label.Show()

    def OnUpdate(self):
        elapsed = time.monotonic() - self.started
        phase = int(elapsed / 60)
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
            for index, (race, kind) in enumerate(((0, 6), (9003, 1), (101, 0), (691, 0), (20101, 1), (0, 6))):
                vid = 57900 + index
                actor_mount = 20104 if index == 5 else mount
                chr.CreateInstance(vid, {"horse": actor_mount if kind == 6 else 0})
                chr.SelectInstance(vid)
                chr.SetVirtualID(vid)
                chr.SetInstanceType(kind)
                chr.SetRace(race)
                if kind == 6:
                    chr.ChangeShape(0)
                    chr.SetHair(1001)
                    chr.SetWeapon(19)
                    chr.SetMotionMode(chr.MOTION_MODE_HORSE_ONEHAND_SWORD if actor_mount else chr.MOTION_MODE_ONEHAND_SWORD)
                else:
                    chr.SetArmor(0)
                    chr.SetMotionMode(chr.MOTION_MODE_GENERAL)
                chr.SetLoopMotion(chr.MOTION_WAIT)
                px = x + index * 250
                chr.SetPixelPosition(int(px), int(y), int(background.GetHeight(px, y)))
                chr.Show()
            player.SetMainCharacterIndex(57900)
            self.phase, self.step = phase, -1
            log.write("phase=%d map=%s mount=20104 actors=6\n" % (phase, name))
            log.flush()
        seconds = elapsed - phase * 60
        motion_step = min(3, int(seconds / 15))
        if self.motion != (phase, motion_step):
            for vid in (57900, 57902):
                chr.SelectInstance(vid)
                motions = (chr.MOTION_WAIT, chr.MOTION_WALK, chr.MOTION_RUN,
                           chr.MOTION_COMBO_ATTACK_1 if vid == 57900 else chr.MOTION_NORMAL_ATTACK)
                chr.SetLoopMotion(motions[motion_step])
            self.motion = (phase, motion_step)
            self.motion_step = motion_step
            log.write("animation phase=%d sample=%d player+mob=idle/walk/run/attack\n" % (phase, motion_step))
            log.flush()
        step = min(2, int(seconds / 20))
        if step != self.step:
            self.step = step
            log.write("transition phase=%d step=%d armor=%d camera=%s\n" %
                      (phase, step, 0, "far" if step == 1 else "near"))
            log.flush()
        z = background.GetHeight(x, y)
        self.position = (x, y, z)
        self.camera = (6500, 35, 0) if step == 1 else (1000, 20, 0)
        app.SetCenterPosition(x, -y, z + 100)
        app.SetCamera(*self.camera, 0.0)
        background.Update(x, -y, z)
        chr.Update()
        self.label.SetText("F1-X %s: player / NPC / mob / boss / mount; hair + armor + weapon" % name)
        shot = "far" if step == 1 and seconds >= 27 else "near" if step == 2 and seconds >= 52 else None
        if shot and self.shot != (phase, shot):
            self.pending_shot = (phase, shot)

    def OnRender(self):
        x, y, z = self.position
        grp.SetPositionCamera(x, -y, z + 100, *self.camera)
        app.RenderGame()
        grp.SetInterfaceRenderState()
        self.frames += 1
        self.motion_frames[self.motion_step] += 1
        # Capture requests belong to rendered frames; import-heavy update catch-up
        # must not queue several captures before the next Present.
        if self.pending_shot:
            phase, shot = self.pending_shot
            success, path = grp.SaveScreenShotToPath("f1x-world-%d-%s-" % (phase, shot))
            if success:
                self.shot = self.pending_shot
                self.pending_shot = None
                log.write("screenshot phase=%d camera=%s success=1 file=%s\n" % (phase, shot, path))
                log.flush()

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
for stage, frames in enumerate(window.motion_frames):
    log.write("rendered animation=%d frames=%d\n" % (stage, frames))
log.close()

