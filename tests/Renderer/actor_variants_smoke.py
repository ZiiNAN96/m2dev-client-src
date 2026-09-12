"""ZiiNAN: Original player armor/NPC/mob models and motions; no server or attachment changes."""
import app
import background
import builtins
import chr
import chrmgr
import grp
import item
import math
import systemSetting
import time
import ui
import wndMgr

PHASE_SECONDS = 12
RACES = {9003: "goods", 9002: "defence", 9004: "bank", 20016: "blacksmith",
         101: "stray_dog", 102: "wolf", 110: "bear", 301: "bksoldier", 691: "orc_lord"}
MIXED_RACES = [0, 9003, 9002, 20016, 101, 102, 110, 301, 691]
# Map, label, race (negative = mixed), desired player shape, motion.
PHASES = [("a1", "body", 0, 0, "idle"),
          ("a1", "armor-a", 0, 3, "idle"), ("a1", "armor-b", 0, 6, "run"),
          ("a1", "armor-c", 0, 9, "attack"), ("a1", "armor-4-1", 0, 12, "idle"),
          ("a1", "skin-override", 0, 4, "run"), ("a1", "body-return", 0, 0, "idle"),
          ("a1", "npc-goods", 9003, 0, "idle"), ("a1", "npc-alpha", 9002, 0, "idle"),
          ("a1", "npc-bank", 9004, 0, "idle"), ("a1", "npc-rigid", 20016, 0, "idle"),
          ("a1", "dog", 101, 0, "idle"), ("a1", "wolf-alpha", 102, 0, "run"),
          ("a1", "bear", 110, 0, "run"), ("a1", "humanoid", 301, 0, "attack"),
          ("a1", "boss", 691, 0, "attack"), ("a1", "hit", 102, 0, "hit"),
          ("a1", "death", 102, 0, "death"), ("a1", "fade", 102, 0, "idle"),
          ("a1", "restore", 102, 0, "idle"), ("a1", "out-of-view", 102, 0, "idle"),
          ("a1", "reenter", 102, 0, "idle"), ("a1", "mixed", -1, 12, "idle"),
          ("b1", "mixed", -1, 6, "run"), ("b1", "mixed-attack", -1, 9, "attack"),
          ("a1", "mixed-return", -1, 12, "run"), ("a1", "stress", -2, 3, "run"),
          ("a1", "hidden", -2, 3, "run"), ("a1", "despawn", -3, 0, "idle"),
          ("a1", "respawn", -1, 12, "idle"), ("a1", "final-delete", -3, 0, "idle")]


def run():
    width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
    wndMgr.SetScreenSize(width, height)
    app.Create("Metin2 actor variants 5B test", width, height, 1)
    app.SetCameraMaxDistance(30000.0)
    app.SetSightRange(25600)
    app.SetArmorSpecularEnable(True)
    grp.SetClearColor(0.08, 0.16, 0.28)
    log = builtins.old_open("actor-variants-test.log", "w")
    # ZiiNAN: Resolve real item Shape/Specular through the original item table, not invented powers.
    item.LoadItemTable(app.GetLocalePath() + "/item_proto")
    armors = {0: 0}
    for vnum in range(11209, 11310, 10):
        item.SelectItem(vnum)
        shape = item.GetValue(3)
        armors.setdefault(shape, vnum)
        log.write("armor item=%d shape=%d name=%s\n" % (vnum, shape, item.GetItemName()))
    for shape in (3, 4, 6, 9, 12):
        if shape not in armors:
            raise RuntimeError("Original armor shape %d missing from fixture items" % shape)
    chrmgr.CreateRace(0)
    chrmgr.SelectRace(0)
    chrmgr.LoadLocalRaceData("msm/warrior_m.msm")
    chrmgr.SetPathName("d:/ymir work/pc/warrior/general/")
    chrmgr.RegisterMotionMode(chr.MOTION_MODE_GENERAL)
    for motion, name in ((chr.MOTION_WAIT, "wait.msa"), (chr.MOTION_RUN, "run.msa"),
                         (chr.MOTION_NORMAL_ATTACK, "attack.msa")):
        chrmgr.RegisterMotionData(chr.MOTION_MODE_GENERAL, motion, name)
    for race, name in RACES.items():
        chrmgr.RegisterRaceName(race, name)  # Existing RaceManager loads original MSM and motlist.
    log.flush()

    class VariantsWindow(ui.Window):
        def __init__(self):
            ui.Window.__init__(self)
            self.started = time.monotonic()
            self.phase = -1
            self.map = None
            self.actors = []
            self.last_motion = -1
            self.last_sample = -1
            self.frames = 0
            self.SetSize(width, height)
            self.Show()

        def populate(self, races, shape):
            # Preserve a real player instance across the armor sequence; model handles must recycle.
            existing = [entry[1] for entry in self.actors]
            if races != existing:
                chr.Destroy()
                self.actors = []
                for index, race in enumerate(races):
                    vid = 51000 + index
                    chr.CreateInstance(vid)
                    chr.SelectInstance(vid)
                    chr.SetVirtualID(vid)
                    chr.SetInstanceType(chr.INSTANCE_TYPE_PLAYER if race == 0 else
                                        (chr.INSTANCE_TYPE_NPC if race >= 9000 else chr.INSTANCE_TYPE_ENEMY))
                    chr.SetRace(race)
                    chr.SetArmor(armors[shape] if race == 0 else 0)
                    chr.Refresh()
                    self.actors.append((vid, race))
            for vid, race in self.actors:
                chr.SelectInstance(vid)
                if race == 0:
                    chr.SetArmor(armors[shape])
                chr.testRestoreRenderMode(vid)
                chr.SetBlendRenderMode(vid, 1.0)
                chr.SetMotionMode(chr.MOTION_MODE_GENERAL)
                chr.SetLoopMotion(chr.MOTION_WAIT)
                chr.Show()

        def OnUpdate(self):
            elapsed = time.monotonic() - self.started
            phase = min(int(elapsed / PHASE_SECONDS), len(PHASES) - 1)
            suffix, label, race, shape, motion = PHASES[phase]
            if phase != self.phase:
                if suffix != self.map:
                    chr.Destroy()
                    self.actors = []
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
                races = [race] if race >= 0 else ([] if race == -3 else MIXED_RACES[:])
                if race == -2:
                    races *= 4
                self.populate(races, shape)
                for vid, actor_race in self.actors:
                    chr.SelectInstance(vid)
                    if motion == "run" and actor_race < 9000:
                        chr.SetLoopMotion(chr.MOTION_RUN)
                    if label == "hidden":
                        chr.Hide()
                    if label == "npc-goods":
                        chr.SetAddRenderMode(vid, 0.0, 0.3, 0.0)
                self.phase = phase
                self.last_motion = -1
                log.write("phase=%d label=%s actors=%d shape=%d armor=%d motion=%s\n" %
                          (phase, label, len(self.actors), shape, armors.get(shape, 0), motion))
                log.flush()
            x, y, z = self.position
            multiple = len(self.actors) > 1
            distance = 4200.0 if multiple else (2100.0 if race == 691 else 850.0)
            self.camera = (distance, 22.0 if multiple else 15.0, float((phase % 4) * 90))
            app.SetCenterPosition(x, -y, z + 85.0)
            app.SetCamera(*self.camera, 0.0)
            background.Update(x, -y, z)
            motion_tick = int(elapsed / 3.0)
            for index, (vid, actor_race) in enumerate(self.actors):
                chr.SelectInstance(vid)
                dx = ((index % 6) - 2.5) * 180.0 if multiple else 0.0
                dy = ((index // 6) - (len(self.actors) // 6) / 2.0) * 200.0 if multiple else 0.0
                if motion == "run" and actor_race < 9000:
                    dx += math.sin(elapsed) * 90.0
                if label == "out-of-view":
                    dx += 25000.0
                chr.SetPixelPosition(int(x + dx), int(y + dy), int(background.GetHeight(x + dx, y + dy)))
                chr.SetRotation(float((phase % 4) * 90))
                if label == "fade":
                    chr.SetBlendRenderMode(vid, max(0.05, 1.0 - (elapsed % PHASE_SECONDS) / PHASE_SECONDS))
                if motion_tick != self.last_motion and actor_race < 9000:
                    action = {"attack": chr.MOTION_NORMAL_ATTACK, "hit": chr.MOTION_DAMAGE, "death": chr.MOTION_DEAD}.get(motion)
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

    window = VariantsWindow()
    app.Loop()
    chr.Destroy()
    background.Destroy()
    window.Hide()
    window.Destroy()
    log.write("normal actor/map/window shutdown\n")
    log.close()
