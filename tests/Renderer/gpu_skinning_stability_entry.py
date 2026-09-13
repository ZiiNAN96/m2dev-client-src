"""ZiiNAN: GPU skinning stability validation; private repeated original-asset world fixture."""
import app, background, builtins, chr, chrmgr, grp, item, player
import math, playersettingmodule, systemSetting, time, ui, wndMgr

width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
wndMgr.SetScreenSize(width, height)
app.Create("Metin2 GPU skinning B5-X stability test", width, height, 1)
app.SetCameraMaxDistance(40000.0)
app.SetSightRange(24000)
app.SetArmorSpecularEnable(True)
app.SetHairColorEnable(True)
if not app.LoadLocaleData(app.GetLocalePath()):
    raise RuntimeError("Original locale required")
playersettingmodule.__LoadGameNPC()
playersettingmodule.__LoadGameEffect()
item.LoadItemTable(app.GetLocalePath() + "/item_proto")
RACES = [("warrior", "m"), ("assassin", "w"), ("sura", "m"), ("shaman", "w"),
         ("warrior", "w"), ("assassin", "m"), ("sura", "w"), ("shaman", "m")]
armors = {}
for job in range(4):
    armors[job] = {0: 0}
    for vnum in range(11209 + job * 200, 11310 + job * 200, 10):
        item.SelectItem(vnum)
        armors[job].setdefault(item.GetValue(3), vnum)
        if all(shape in armors[job] for shape in (3, 6, 9)):
            break
    if any(shape not in armors[job] for shape in (3, 6, 9)):
        raise RuntimeError("Required original armor shapes absent")
for race, (name, gender) in enumerate(RACES):
    chrmgr.CreateRace(race)
    chrmgr.SelectRace(race)
    chrmgr.LoadLocalRaceData("msm/%s_%s.msm" % (name, gender))
    getattr(playersettingmodule, "__LoadGame%sEx" % name.capitalize())(race, "d:/ymir work/%s/%s/" % ("pc" if race < 4 else "pc2", name))
SCENES = [("a1", 44000, 27200, 0), ("b1", 70400, 53600, 20104),
          ("a1", 75200, 56000, 20110), ("monkeydungeon", 7260, 11390, 20114),
          ("guild_01", 27000, 26000, 20219), ("a1", 44000, 27200, 0)]
OTHERS = [(9003, 1), (9002, 1), (20016, 1), (20001, 1), (101, 0), (102, 0), (301, 0), (691, 0)]
SCENES = SCENES * 2
PHASE_SECONDS = 80  # Twelve phases: sixteen-minute stability session, no performance scoring.
log = builtins.old_open("gpu-skinning-stability-test.log", "w")


class World(ui.Window):
    def __init__(self):
        ui.Window.__init__(self)
        self.SetSize(width, height)
        self.Show()
        self.started = time.monotonic()
        self.phase = self.step = -1
        self.actors = []
        self.label = ui.TextLine()
        self.label.SetParent(self)
        self.label.SetPosition(20, 20)
        self.label.SetOutline()
        self.label.Show()

    def populate(self, mount, count=16):
        chr.Destroy()
        self.actors = []
        for index, (race, kind) in enumerate(([(race, 6) for race in range(8)] + OTHERS + [OTHERS[i % len(OTHERS)] for i in range(max(0, count - 16))])[:count]):
            log.write("creating race=%d kind=%d mount=%d\n" % (race, kind, mount))
            log.flush()
            vid = 57400 + index
            chr.CreateInstance(vid, {"horse": mount if kind == 6 else 0})
            chr.SelectInstance(vid)
            chr.SetVirtualID(vid)
            chr.SetInstanceType(kind)
            chr.SetRace(race)
            if kind == 6:
                chr.ChangeShape(0)
                chr.SetWeapon((19, 1009, 19, 7009)[race % 4])
                chr.SetHair(1001 + (race % 4) * 1000)
                suffix = ("ONEHAND_SWORD", "DUALHAND_SWORD", "ONEHAND_SWORD", "FAN")[race % 4]
                chr.SetMotionMode(getattr(chr, "MOTION_MODE_" + ("HORSE_" if mount else "") + suffix))
            else:
                chr.SetArmor(0)
                chr.SetMotionMode(chr.MOTION_MODE_GENERAL)
            chr.SetLoopMotion(chr.MOTION_WAIT)
            chr.Show()
            self.actors.append((vid, race, kind))
            log.write("created race=%d\n" % race)
            log.flush()
        player.SetMainCharacterIndex(57400)

    def OnUpdate(self):
        elapsed = time.monotonic() - self.started
        phase = int(elapsed / PHASE_SECONDS)
        if phase >= len(SCENES):
            app.Exit()
            return
        name, x, y, mount = SCENES[phase]
        if phase != self.phase:
            log.write("loading map=%s\n" % name)
            log.flush()
            chr.Destroy()
            if self.phase >= 0:
                background.Destroy()
            background.Initialize()
            background.LoadMap("metin2_map_" + name, float(x), float(y), 0.0)
            background.SetViewDistanceSet(background.DISTANCE0, 24000.0)
            background.SelectViewDistanceNum(background.DISTANCE0)
            self.populate(mount, (16, 25, 50, 100)[phase % 4])
            self.phase, self.step = phase, -1
            log.write("phase=%d map=%s mount=%d actors=%d\n" % (phase, name, mount, len(self.actors)))
            log.flush()
        seconds = elapsed - phase * PHASE_SECONDS
        step = int(seconds / 4)
        if step != self.step:
            if step and step % 5 == 0:
                current_mount = (0, mount or 20104, 0, 20110)[(step // 5) % 4]
                self.populate(current_mount, (16, 25, 50, 100)[phase % 4])  # Real same-VID spawn/despawn; no unsafe direct DismountHorse helper.
            for vid, race, kind in self.actors:
                chr.SelectInstance(vid)
                if kind == 6:
                    shape = (0, 3, 6, 9, 0)[step % 5]
                    chr.ChangeShape(armors[race % 4][shape])
                    chr.SetHair(1001 + (race % 4) * 1000 + (1 if step % 4 in (1, 2) else 0))
                    chr.SetWeapon((0, (19, 1009, 19, 7009)[race % 4], (29, 1019, 29, 7019)[race % 4], 0)[step % 4])
                chr.SetLoopMotion(chr.MOTION_RUN if step % 5 in (1, 2) else chr.MOTION_WAIT)
                if step % 5 == 3:
                    chr.PushOnceMotion(chr.MOTION_COMBO_ATTACK_1 if kind == 6 else chr.MOTION_NORMAL_ATTACK, 0.1)
                if step % 5 == 4 and kind == 0:
                    chr.PushOnceMotion(chr.MOTION_DAMAGE, 0.1)
                if step % 5 == 0 and step and kind == 0:
                    chr.PushOnceMotion(chr.MOTION_DEAD, 0.1)
            self.step = step
            log.write("transition phase=%d step=%d\n" % (phase, step))
            log.flush()
        z = background.GetHeight(x, y)
        self.position = (x, y, z)
        self.camera = (3200.0 if step % 4 in (0, 3) else 8500.0, 25.0, math.sin(elapsed * .12) * 70.0)
        app.SetCenterPosition(x + (2200 if step % 4 in (1, 2) else 0), -y, z + 100)
        app.SetCamera(*self.camera, 0.0)
        background.Update(x, -y, z)
        for index, (vid, race, kind) in enumerate(self.actors):
            chr.SelectInstance(vid)
            px, py = x + ((index % 10) - 4.5) * 220, y + ((index // 10) - 4.5) * 220
            chr.SetPixelPosition(int(px), int(py), int(background.GetHeight(px, py)))
        chr.Update()
        self.label.SetText("B5-X | %s | mount %d | shape/hair %d | CPU default / GPU opt-in" % (name, mount, step))

    def OnRender(self):
        x, y, z = self.position
        grp.SetPositionCamera(x, -y, z + 100, *self.camera)
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
