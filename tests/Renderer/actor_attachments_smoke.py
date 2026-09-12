"""ZiiNAN: Diligent actor attachment rendering; isolated native assets, no server writes."""
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

PHASE_SECONDS = 10
# Map, label, equipment set, hair, animation. Preserve instances across equipment changes.
PHASES = [("a1", "none", 0, 0, "idle"),
          ("a1", "weapon-a", 1, 0, "idle"), ("a1", "walk-a", 1, 0, "walk"),
          ("a1", "run-a", 1, 0, "run"), ("a1", "attack-a", 1, 0, "attack"),
          ("a1", "weapon-b", 2, 1001, "idle"), ("a1", "run-b", 2, 1001, "run"),
          ("a1", "attack-b", 2, 1001, "attack"), ("a1", "hit-b", 2, 1001, "hit"),
          ("a1", "death-b", 2, 1001, "death"),
          ("a1", "weapon-c-single-hand", 3, 1001, "idle"), ("a1", "none-return", 0, 0, "idle"),
          ("a1", "hair-b", 1, 1001, "idle"), ("a1", "hair-a-return", 1, 0, "run"),
          ("a1", "skill-animation", 1, 0, "skill"), ("a1", "fade", 1, 1001, "idle"),
          ("a1", "out-of-view", 1, 1001, "run"), ("a1", "reenter", 1, 1001, "run"),
          ("b1", "map-b", 2, 1001, "attack"), ("b1", "hidden", 2, 1001, "idle"),
          ("a1", "map-a-return", 1, 0, "run"),
          ("a1", "gender-swap", 1, 1001, "walk"), ("a1", "gender-return", 1, 0, "run"),
          ("a1", "despawn", 0, 0, "idle"),
          ("a1", "respawn", 2, 1001, "attack"), ("a1", "final-delete", 0, 0, "idle")]

RACES = [("warrior", "m"), ("assassin", "w"), ("sura", "m"), ("shaman", "w"),
         ("warrior", "w"), ("assassin", "m"), ("sura", "w"), ("shaman", "m")]
WEAPONS = {0: (0, 0, 0, 0), 1: (19, 1009, 19, 7009), 2: (3009, 2009, 29, 5009),
           3: (19, 19, 29, 5009)}
MODES = {0: chr.MOTION_MODE_GENERAL, 19: chr.MOTION_MODE_ONEHAND_SWORD,
         29: chr.MOTION_MODE_ONEHAND_SWORD, 3009: chr.MOTION_MODE_TWOHAND_SWORD,
         1009: chr.MOTION_MODE_DUALHAND_SWORD, 2009: chr.MOTION_MODE_BOW,
         7009: chr.MOTION_MODE_FAN, 5009: chr.MOTION_MODE_BELL}


def run():
    width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
    wndMgr.SetScreenSize(width, height)
    app.Create("Metin2 actor attachments 5C test", width, height, 1)
    app.SetCameraMaxDistance(30000.0)
    app.SetSightRange(25600)
    app.SetArmorSpecularEnable(True)
    app.SetHairColorEnable(True)
    grp.SetClearColor(0.08, 0.16, 0.28)
    log = builtins.old_open("actor-attachments-test.log", "w")
    if not app.LoadLocaleData(app.GetLocalePath()):
        raise RuntimeError("Original locale/item list/proto failed")
    for weapon, subtype in ((19, item.WEAPON_SWORD), (29, item.WEAPON_SWORD),
                            (1009, item.WEAPON_DAGGER), (2009, item.WEAPON_BOW),
                            (3009, item.WEAPON_TWO_HANDED), (5009, item.WEAPON_BELL),
                            (7009, item.WEAPON_FAN)):
        item.SelectItem(weapon)
        if item.GetItemSubType() != subtype:
            raise RuntimeError("Unexpected native weapon subtype: %d" % weapon)
        log.write("weapon item=%d subtype=%d name=%s\n" % (weapon, subtype, item.GetItemName()))
    for race, (name, gender) in enumerate(RACES):
        chrmgr.CreateRace(race)
        chrmgr.SelectRace(race)
        chrmgr.LoadLocalRaceData("msm/%s_%s.msm" % (name, gender))
        path = "d:/ymir work/%s/%s/" % ("pc" if race < 4 else "pc2", name)
        # Original registrations, including hand bones, weapon modes and skill animations.
        getattr(playersettingmodule, "__LoadGame%sEx" % name.capitalize())(race, path)
        log.write("registered race=%d %s_%s\n" % (race, name, gender))
    log.flush()

    class AttachmentsWindow(ui.Window):
        def __init__(self):
            ui.Window.__init__(self)
            self.started = time.monotonic()
            self.phase = -1
            self.map = None
            self.actors = []
            self.equipped = {}
            self.last_motion = -1
            self.last_sample = -1
            self.frames = 0
            self.SetSize(width, height)
            self.Show()

        def populate(self, label, equipment, hair, motion):
            if label in ("despawn", "final-delete"):
                chr.Destroy()
                self.actors = []
                self.equipped = {}
                return
            if not self.actors:
                for race in range(8):
                    vid = 52000 + race
                    chr.CreateInstance(vid)
                    chr.SelectInstance(vid)
                    chr.SetVirtualID(vid)
                    chr.SetInstanceType(chr.INSTANCE_TYPE_PLAYER)
                    chr.SetRace(race)
                    chr.SetArmor(0)
                    self.actors.append((vid, race))
            if label in ("gender-swap", "gender-return"):
                self.actors = [(vid, (race + 4) % 8) for vid, race in self.actors]
                for vid, race in self.actors:
                    chr.SelectInstance(vid)
                    chr.SetRace(race)
                    chr.SetArmor(0)
                self.equipped = {}
            for vid, race in self.actors:
                chr.SelectInstance(vid)
                weapon = WEAPONS[equipment][race % 4]
                actual_hair = hair + (race % 4) * 1000 if hair >= 1000 else hair
                previous = self.equipped.get(vid)
                if previous is None or previous[0] != weapon:
                    chr.SetWeapon(weapon)
                if previous is None or previous[1] != actual_hair:
                    chr.SetHair(actual_hair)
                self.equipped[vid] = (weapon, actual_hair)
                chr.testRestoreRenderMode(vid)
                chr.SetBlendRenderMode(vid, 1.0)
                chr.SetMotionMode(chr.MOTION_MODE_GENERAL if motion == "skill" else MODES[weapon])
                loop = {"walk": chr.MOTION_WALK, "run": chr.MOTION_RUN}.get(motion, chr.MOTION_WAIT)
                chr.SetLoopMotion(loop)
                chr.Show()
                if label == "hidden":
                    chr.Hide()
                log.write("equipment phase=%d vid=%d race=%d weapon=%d hair=%d\n" %
                          (self.phase, vid, race, weapon, actual_hair))

        def OnUpdate(self):
            elapsed = time.monotonic() - self.started
            phase = min(int(elapsed / PHASE_SECONDS), len(PHASES) - 1)
            suffix, label, equipment, hair, motion = PHASES[phase]
            if phase != self.phase:
                if suffix != self.map:
                    chr.Destroy()
                    self.actors = []
                    self.equipped = {}
                    if self.map is not None:
                        background.Destroy()
                    background.Initialize()
                    x, y = (58300.0, 63000.0) if suffix == "a1" else (69100.0, 56000.0)
                    background.LoadMap("metin2_map_" + suffix, x, y, 0.0)
                    self.position = (x, y, background.GetHeight(x, y))
                    self.map = suffix
                    log.write("map loaded: %s\n" % suffix)
                background.SetShadowLevel(0)
                for part in (background.PART_TREE, background.PART_SKY, background.PART_CLOUD, background.PART_WATER):
                    background.SetVisiblePart(part, 0)
                background.SetVisiblePart(background.PART_OBJECT, 1)
                background.SetViewDistanceSet(background.DISTANCE0, 25600.0)
                background.SelectViewDistanceNum(background.DISTANCE0)
                self.phase = phase
                self.populate(label, equipment, hair, motion)
                self.last_motion = -1
                log.write("phase=%d label=%s actors=%d equipment=%d hair=%d motion=%s\n" %
                          (phase, label, len(self.actors), equipment, hair, motion))
                log.flush()
            x, y, z = self.position
            self.camera = (2000.0, 22.0, float((phase % 4) * 90) + math.sin(elapsed * 0.25) * 12.0)
            app.SetCenterPosition(x, -y, z + 85.0)
            app.SetCamera(*self.camera, 0.0)
            background.Update(x, -y, z)
            motion_tick = int(elapsed / 3.0)
            for index, (vid, race) in enumerate(self.actors):
                chr.SelectInstance(vid)
                dx = ((index % 4) - 1.5) * 230.0
                dy = ((index // 4) - 0.5) * 300.0
                if motion in ("run", "walk"):
                    dx += math.sin(elapsed) * 60.0
                if label == "out-of-view":
                    dx += 25000.0
                chr.SetPixelPosition(int(x + dx), int(y + dy), int(background.GetHeight(x + dx, y + dy)))
                chr.SetRotation(float((phase % 4) * 90))
                if label == "fade":
                    chr.SetBlendRenderMode(vid, max(0.05, 1.0 - (elapsed % PHASE_SECONDS) / PHASE_SECONDS))
                if motion_tick != self.last_motion:
                    action = {"attack": chr.MOTION_COMBO_ATTACK_1, "hit": chr.MOTION_DAMAGE,
                              "death": chr.MOTION_DEAD, "skill": chr.MOTION_SKILL + 1}.get(motion)
                    if action is not None:
                        chr.PushOnceMotion(action, 0.1)
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
            grp.SetPositionCamera(x, -y, z + 85.0, *self.camera)
            grp.SetPerspective(30.0, float(width) / height, 100.0, 25600.0)
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

    window = AttachmentsWindow()
    app.Loop()
    chr.Destroy()
    background.Destroy()
    window.Hide()
    window.Destroy()
    log.write("normal actor/map/window shutdown\n")
    log.close()
