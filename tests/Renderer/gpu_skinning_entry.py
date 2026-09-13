"""ZiiNAN: Diligent GPU skinning prototype; isolated original-asset regression, no login."""
import app, background, builtins, chr, chrmgr, grp, player, item
import playersettingmodule, systemSetting, time, ui, wndMgr

width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
wndMgr.SetScreenSize(width, height)
app.Create("Metin2 GPU skinning B3 test", width, height, 1)
app.SetCameraMaxDistance(40000.0)
app.SetSightRange(24000)
if not app.LoadLocaleData(app.GetLocalePath()):
    raise RuntimeError("Original locale missing")
playersettingmodule.__LoadGameNPC()
playersettingmodule.__LoadGameEffect()
chrmgr.CreateRace(0)
chrmgr.SelectRace(0)
chrmgr.LoadLocalRaceData("msm/warrior_m.msm")
playersettingmodule.__LoadGameWarriorEx(0, "d:/ymir work/pc/warrior/")
item.LoadItemTable(app.GetLocalePath() + "/item_proto")
item.SelectItem(11219)
if item.GetValue(3) == 0:
    raise RuntimeError("Armor fallback fixture needs a non-base shape")
SCENES = (("a1", 44000, 27200), ("b1", 70400, 53600), ("a1", 75200, 56000),
          ("monkeydungeon", 7260, 11390), ("guild_01", 27000, 26000), ("a1", 44000, 27200))
MOTIONS = (chr.MOTION_WAIT, chr.MOTION_WALK, chr.MOTION_RUN, chr.MOTION_COMBO_ATTACK_1)
log = builtins.old_open("gpu-skinning-test.log", "w")


class World(ui.Window):
    def __init__(self):
        ui.Window.__init__(self)
        self.SetSize(width, height)
        self.Show()
        self.started = time.monotonic()
        self.phase = self.motion = self.shot = -1
        self.transition = -1
        self.position = (0, 0, 0)
        self.camera = (700, 15, 0)
        self.label = ui.TextLine()
        self.label.SetParent(self)
        self.label.SetPosition(20, 20)
        self.label.SetOutline()
        self.label.Show()

    def OnUpdate(self):
        elapsed = time.monotonic() - self.started
        phase = int(elapsed / 25)
        if phase >= len(SCENES):
            app.Exit()
            return
        name, x, y = SCENES[phase]
        if phase != self.phase:
            chr.Destroy()
            if self.phase >= 0:
                background.Destroy()
            background.Initialize()
            background.LoadMap("metin2_map_" + name, float(x), float(y), 0.0)
            background.SetViewDistanceSet(background.DISTANCE0, 24000.0)
            background.SelectViewDistanceNum(background.DISTANCE0)
            for i, (race, kind) in enumerate(((0, 6), (9003, 1), (101, 0), (20101, 1))):
                vid = 57300 + i
                chr.CreateInstance(vid)
                chr.SelectInstance(vid)
                chr.SetVirtualID(vid)
                chr.SetInstanceType(kind)
                chr.SetRace(race)
                chr.SetArmor(0)
                chr.SetMotionMode(chr.MOTION_MODE_GENERAL)
                chr.SetLoopMotion(chr.MOTION_WAIT)
                px = x + i * 250
                chr.SetPixelPosition(int(px), int(y), int(background.GetHeight(px, y)))
                chr.Show()
            player.SetMainCharacterIndex(57300)
            self.phase, self.motion = phase, -1
            log.write("phase=%d map=%s\n" % (phase, name))
            log.flush()
        seconds = elapsed - phase * 25
        # ZiiNAN: Diligent GPU skinning prototype
        # The final map adds existing shape, CPU hair and native mount fallback paths.
        if phase == 5:
            step = min(4, int(seconds / 5))
            if step != self.transition:
                chr.SelectInstance(57300)
                if step == 1:
                    chr.ChangeShape(11219)
                elif step == 2:
                    chr.ChangeShape(0)
                    chr.SetHair(1001)
                    chr.SetWeapon(19)
                elif step in (3, 4):
                    # Match NetworkActorManager's mount-status replacement at the same VID.
                    # The old standalone chr.DismountHorse helper leaves a native rider link dangling.
                    chr.DeleteInstance(57300)
                    chr.CreateInstance(57300, {"horse": 20030 if step == 3 else 0})
                    chr.SelectInstance(57300)
                    chr.SetVirtualID(57300)
                    chr.SetInstanceType(6)
                    chr.SetRace(0)
                    chr.ChangeShape(0)
                    chr.SetWeapon(19)
                    chr.SetHair(1001)
                    chr.SetMotionMode(chr.MOTION_MODE_HORSE_ONEHAND_SWORD if step == 3 else chr.MOTION_MODE_ONEHAND_SWORD)
                    chr.SetPixelPosition(int(x), int(y), int(background.GetHeight(x, y)))
                    player.SetMainCharacterIndex(57300)
                    chr.Show()
                self.motion = -1
                self.transition = step
                log.write("transition=%d (base,armor,base+CPU-hair,mount,dismount)\n" % step)
                log.flush()
        motion = min(3, int(seconds / 6))
        if motion != self.motion:
            chr.SelectInstance(57300)
            chr.SetLoopMotion(chr.MOTION_WAIT if phase == 5 and self.transition >= 3 else MOTIONS[motion])
            self.motion = motion
        z = background.GetHeight(x, y)
        background.Update(x, -y, z)
        self.position = (x, y, z)
        distance = 4500 if 12 < seconds < 15 else 700
        self.camera = (distance, 15, seconds * 12)
        self.label.SetText("B3 reference warrior / %s / animation %d / CPU fallback at distance" % (name, motion))
        app.SetCenterPosition(x, -y, z + 90)
        app.SetCamera(*self.camera, 0.0)
        chr.Update()
        shot = phase * 10 + (self.transition if phase == 5 else 0)
        if seconds % 5 > 3 and self.shot != shot:
            self.shot = shot
            log.write("screenshot=%s\n" % (grp.SaveScreenShotToPath("b3-world-%02d-%d-" % (phase, self.transition)),))
            log.flush()

    def OnRender(self):
        x, y, z = self.position
        grp.SetPositionCamera(x, -y, z + 90, *self.camera)
        app.RenderGame()
        grp.SetInterfaceRenderState()

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
log.write("completed phases=%d\n" % (window.phase + 1))
log.close()
