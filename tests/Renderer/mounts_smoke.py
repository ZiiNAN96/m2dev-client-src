"""ZiiNAN: Diligent mount actor rendering; real Create/Delete/Race/Motion paths, no server writes."""
import app
import background
import builtins
import chr
import chrmgr
import grp
import item
import math
import playersettingmodule
import systemSetting
import time
import ui
import wndMgr

PHASE_SECONDS = 8
# map, label, mount race (0 = on foot), motion, equipment set, alternate hair/armor
PHASES = [("a1", "foot-a", 0, "idle", 1, False),
          ("b1", "foot-b", 0, "run", 1, False), ("a1", "foot-a-return", 0, "run", 1, False),
          ("a1", "mount-horse", 20104, "idle", 1, False),
          ("a1", "horse-walk", 20104, "walk", 1, False), ("a1", "horse-run", 20104, "run", 1, False),
          ("a1", "horse-attack", 20104, "attack", 1, False),
          ("a1", "weapon-b", 20104, "idle", 2, False), ("a1", "weapon-b-run", 20104, "run", 2, False),
          ("a1", "weapon-b-attack", 20104, "attack", 2, False),
          ("a1", "armor-hair-change", 20104, "run", 2, True),
          ("a1", "mount-boar", 20110, "idle", 1, True), ("a1", "boar-run", 20110, "run", 1, True),
          ("a1", "mount-lion", 20114, "idle", 1, True), ("a1", "lion-attack", 20114, "attack", 1, True),
          ("a1", "mount-dinosaur", 20225, "idle", 2, True), ("a1", "dinosaur-run", 20225, "run", 2, True),
          ("a1", "mount-halloween", 20219, "idle", 1, True), ("a1", "halloween-run", 20219, "run", 1, True),
          ("a1", "horse-return", 20104, "run", 1, False),
          ("a1", "hidden", 20104, "idle", 1, False), ("a1", "show", 20104, "run", 1, False),
          ("a1", "out-of-view", 20104, "walk", 1, False), ("a1", "reenter", 20104, "run", 1, False),
          ("b1", "mounted-b", 20114, "run", 2, True), ("a1", "mounted-a-return", 20114, "run", 2, True),
          ("a1", "dismount-1", 0, "run", 1, False), ("a1", "remount-2", 20110, "run", 1, False),
          ("a1", "dismount-2", 0, "walk", 2, True), ("a1", "remount-3", 20104, "run", 1, False),
          ("a1", "despawn", 20104, "idle", 0, False), ("a1", "respawn", 20114, "run", 2, True),
          ("a1", "final-delete", 0, "idle", 0, False)]
RACES = [("warrior", "m"), ("assassin", "w"), ("sura", "m"), ("shaman", "w"),
         ("warrior", "w"), ("assassin", "m"), ("sura", "w"), ("shaman", "m")]
WEAPONS = {0: (0, 0, 0, 0), 1: (19, 1009, 19, 7009), 2: (3009, 2009, 29, 5009)}
MODE_SUFFIX = {0: "", 19: "ONEHAND_SWORD", 29: "ONEHAND_SWORD", 1009: "DUALHAND_SWORD",
               2009: "BOW", 3009: "TWOHAND_SWORD", 5009: "BELL", 7009: "FAN"}


def run():
    width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
    wndMgr.SetScreenSize(width, height)
    app.Create("Metin2 mounts 5D test", width, height, 1)
    app.SetCameraMaxDistance(40000.0)
    app.SetSightRange(32000)
    app.SetArmorSpecularEnable(True)
    app.SetHairColorEnable(True)
    grp.SetClearColor(0.08, 0.16, 0.28)
    log = builtins.old_open("mounts-test.log", "w")
    if not app.LoadLocaleData(app.GetLocalePath()):
        raise RuntimeError("Original locale/item data failed")
    playersettingmodule.__LoadGameNPC()  # Native npclist aliases, MSM and motlist resolution.
    armors = {}
    for job in range(4):
        armors[job] = {0: 0}
        for vnum in range(11209 + job * 200, 11310 + job * 200, 10):
            item.SelectItem(vnum)
            armors[job].setdefault(item.GetValue(3), vnum)
            if 3 in armors[job]:
                break
        if 3 not in armors[job]:
            raise RuntimeError("Original alternate armor missing for job %d" % job)
    for race, (name, gender) in enumerate(RACES):
        chrmgr.CreateRace(race)
        chrmgr.SelectRace(race)
        chrmgr.LoadLocalRaceData("msm/%s_%s.msm" % (name, gender))
        path = "d:/ymir work/%s/%s/" % ("pc" if race < 4 else "pc2", name)
        getattr(playersettingmodule, "__LoadGame%sEx" % name.capitalize())(race, path)
        log.write("registered race=%d %s_%s native horse modes\n" % (race, name, gender))
    log.flush()

    class MountWindow(ui.Window):
        def __init__(self):
            ui.Window.__init__(self)
            self.started = time.monotonic()
            self.phase = -1
            self.map = None
            self.mount = None
            self.actors = []
            self.last_motion = -1
            self.last_sample = -1
            self.frames = 0
            self.SetSize(width, height)
            self.Show()

        def populate(self, label, mount, equipment, alternate):
            if label in ("despawn", "final-delete"):
                chr.Destroy()
                self.actors = []
                self.mount = None
                return
            if mount != self.mount or not self.actors:
                # NetworkActorManager also replaces the CInstanceBase at the same VID when mounting.
                chr.Destroy()
                self.actors = []
                for race in range(8):
                    vid = 53000 + race
                    chr.CreateInstance(vid, {"horse": mount})
                    chr.SelectInstance(vid)
                    chr.SetVirtualID(vid)
                    chr.SetRace(race)
                    # The existing ChangeArmor rebinds the native saddle after the test race selection.
                    chr.ChangeShape(0)
                    self.actors.append((vid, race))
                self.mount = mount
            for vid, race in self.actors:
                chr.SelectInstance(vid)
                chr.ChangeShape(armors[race % 4][3 if alternate else 0])
                weapon = WEAPONS[equipment][race % 4]
                hair = 1001 + (race % 4) * 1000 if alternate else 0
                chr.SetWeapon(weapon)
                chr.SetHair(hair)
                suffix = MODE_SUFFIX[weapon]
                mode = ("HORSE" + ("_" + suffix if suffix else "")) if mount else (suffix or "GENERAL")
                chr.SetMotionMode(getattr(chr, "MOTION_MODE_" + mode))
                chr.SetLoopMotion(chr.MOTION_WAIT)
                chr.Show()
                if label == "hidden":
                    chr.Hide()
                log.write("actor phase=%d vid=%d race=%d mount=%d weapon=%d hair=%d armor=%d mode=%s\n" %
                          (self.phase, vid, race, mount, weapon, hair, armors[race % 4][3 if alternate else 0], mode))

        def OnUpdate(self):
            elapsed = time.monotonic() - self.started
            phase = min(int(elapsed / PHASE_SECONDS), len(PHASES) - 1)
            suffix, label, mount, motion, equipment, alternate = PHASES[phase]
            if phase != self.phase:
                if suffix != self.map:
                    chr.Destroy()
                    self.actors = []
                    self.mount = None
                    if self.map is not None:
                        background.Destroy()
                    background.Initialize()
                    x, y = (58300.0, 63000.0) if suffix == "a1" else (69100.0, 56000.0)
                    background.LoadMap("metin2_map_" + suffix, x, y, 0.0)
                    self.position = (x, y, background.GetHeight(x, y))
                    self.map = suffix
                    log.write("map loaded: %s mounted=%d\n" % (suffix, bool(mount)))
                background.SetShadowLevel(0)
                for part in (background.PART_TREE, background.PART_SKY, background.PART_CLOUD, background.PART_WATER):
                    background.SetVisiblePart(part, 0)
                background.SetVisiblePart(background.PART_OBJECT, 1)
                background.SetViewDistanceSet(background.DISTANCE0, 32000.0)
                background.SelectViewDistanceNum(background.DISTANCE0)
                self.phase = phase
                self.populate(label, mount, equipment, alternate)
                self.last_motion = -1
                for vid, _ in self.actors:
                    chr.SelectInstance(vid)
                    chr.SetLoopMotion({"walk": chr.MOTION_WALK, "run": chr.MOTION_RUN}.get(motion, chr.MOTION_WAIT))
                log.write("phase=%d label=%s actors=%d mount=%d motion=%s\n" % (phase, label, len(self.actors), mount, motion))
                log.flush()
            x, y, z = self.position
            self.camera = (3800.0, 24.0, float((phase % 4) * 90) + math.sin(elapsed * 0.25) * 12.0)
            app.SetCenterPosition(x, -y, z + 130.0)
            app.SetCamera(*self.camera, 0.0)
            background.Update(x, -y, z)
            motion_tick = int(elapsed / 3.0)
            for index, (vid, _) in enumerate(self.actors):
                chr.SelectInstance(vid)
                dx, dy = ((index % 4) - 1.5) * 480.0, ((index // 4) - 0.5) * 620.0
                if motion in ("run", "walk"):
                    dx += math.sin(elapsed * 0.7) * 150.0
                if label == "out-of-view":
                    dx += 30000.0
                chr.SetPixelPosition(int(x + dx), int(y + dy), int(background.GetHeight(x + dx, y + dy)))
                chr.SetRotation(float((phase % 4) * 90))
                if motion == "attack" and motion_tick != self.last_motion:
                    chr.PushOnceMotion(chr.MOTION_COMBO_ATTACK_1, 0.1)
            self.last_motion = motion_tick
            chr.Update()
            self.frames += 1
            sample = int(elapsed / 5.0)
            if sample != self.last_sample:
                log.write("sample seconds=%.1f frames=%d fps=%s\n" % (elapsed, self.frames, app.GetRenderFPS()))
                log.flush()
                self.last_sample = sample
            if elapsed >= len(PHASES) * PHASE_SECONDS:
                app.Exit()

        def OnRender(self):
            x, y, z = self.position
            grp.SetPositionCamera(x, -y, z + 130.0, *self.camera)
            grp.SetPerspective(30.0, float(width) / height, 100.0, 32000.0)
            grp.Culling()
            chr.Deform()
            grp.SetGameRenderState()
            grp.PushState()
            background.BeginEnvironment()
            background.Render()
            background.SetCharacterDirLight()
            chr.Render()
            background.EndEnvironment()
            grp.PopState()
            grp.SetInterfaceRenderState()

        def OnPressEscapeKey(self):
            app.Exit()
            return True

    window = MountWindow()
    app.Loop()
    chr.Destroy()
    background.Destroy()
    window.Hide()
    window.Destroy()
    log.write("normal mount/actor/map/window shutdown\n")
    log.close()
