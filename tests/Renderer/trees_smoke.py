"""ZiiNAN: Diligent SpeedTree rendering integration; original maps, trees and actor lifecycle."""
import app
import background
import builtins
import chr
import chrmgr
import grp
import math
import os
import playersettingmodule
import systemSetting
import time
import ui
import wndMgr

PHASE_SECONDS = max(2.0, float(os.environ.get("M2_TREE_TEST_PHASE_SECONDS", "10")))
# Map, area, camera distance, yaw, tree visibility. Native placement only.
PHASES = [("a1", "city", 4200, 0, True), ("a1", "city", 8500, 90, True),
          ("a1", "city", 16000, 180, True), ("a1", "city", 4200, 270, True),
          ("a1", "dense", 5000, 0, True), ("a1", "dense", 10000, 90, True),
          ("a1", "dense", 19000, 180, True), ("a1", "dense", 5000, 270, True),
          ("b1", "city", 4200, 0, True), ("b1", "city", 8500, 90, True),
          ("b1", "city", 16000, 180, True), ("b1", "dense", 10000, 270, True),
          ("b1", "dense", 19000, 0, True), ("b1", "dense", 5000, 90, True),
          ("a1", "city", 4200, 0, True), ("a1", "city", 8500, 90, True),
          ("a1", "city", 16000, 180, False), ("a1", "city", 4200, 270, True)]
POSITIONS = {("a1", "city"): (58300.0, 63000.0), ("a1", "dense"): (38000.0, 16000.0),
             ("b1", "city"): (69100.0, 56000.0), ("b1", "dense"): (38000.0, 113000.0)}


def run():
    width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
    wndMgr.SetScreenSize(width, height)
    app.Create("Metin2 SpeedTree milestone 6 test", width, height, 1)
    app.SetCameraMaxDistance(40000.0)
    app.SetSightRange(32000)
    app.SetArmorSpecularEnable(True)
    app.SetHairColorEnable(True)
    grp.SetClearColor(0.08, 0.16, 0.28)
    log = builtins.old_open("trees-test.log", "w")
    if not app.LoadLocaleData(app.GetLocalePath()):
        raise RuntimeError("Original locale/item data unavailable")
    playersettingmodule.__LoadGameNPC()
    chrmgr.CreateRace(0)
    chrmgr.SelectRace(0)
    chrmgr.LoadLocalRaceData("msm/warrior_m.msm")
    playersettingmodule.__LoadGameWarriorEx(0, "d:/ymir work/pc/warrior/")

    class TreeWindow(ui.Window):
        def __init__(self):
            ui.Window.__init__(self)
            self.started = time.monotonic()
            self.phase = -1
            self.map = None
            self.actors = []
            self.last_sample = -1
            self.frames = 0
            self.SetSize(width, height)
            self.Show()

        def populate(self):
            chr.Destroy()
            self.actors = []
            # Real body, mounted rider with attachments, two NPCs and two mobs.
            for index, (race, horse) in enumerate(((0, 0), (0, 20114), (9003, 0), (9002, 0), (101, 0), (102, 0))):
                vid = 54000 + index
                log.write("creating vid=%d race=%d mount=%d\n" % (vid, race, horse))
                log.flush()
                if race == 0:
                    chr.CreateInstance(vid, {"horse": horse})
                else:
                    chr.CreateInstance(vid)
                chr.SelectInstance(vid)
                chr.SetVirtualID(vid)
                chr.SetInstanceType(chr.INSTANCE_TYPE_PLAYER if race == 0 else
                                    chr.INSTANCE_TYPE_NPC if race >= 9000 else chr.INSTANCE_TYPE_ENEMY)
                chr.SetRace(race)
                if race == 0:
                    chr.ChangeShape(0)
                    chr.SetWeapon(19)
                    chr.SetHair(1001)
                else:
                    chr.SetArmor(0)
                chr.SetMotionMode(chr.MOTION_MODE_HORSE_ONEHAND_SWORD if horse else
                                  chr.MOTION_MODE_ONEHAND_SWORD if race == 0 else chr.MOTION_MODE_GENERAL)
                chr.SetLoopMotion(chr.MOTION_WAIT)
                chr.Show()
                self.actors.append(vid)

        def OnUpdate(self):
            elapsed = time.monotonic() - self.started
            phase = min(int(elapsed / PHASE_SECONDS), len(PHASES) - 1)
            suffix, area, distance, rotation, show_trees = PHASES[phase]
            x, y = POSITIONS[suffix, area]
            if phase != self.phase:
                if suffix != self.map:
                    chr.Destroy()
                    if self.map is not None:
                        background.Destroy()
                    background.Initialize()
                    background.LoadMap("metin2_map_" + suffix, x, y, 0.0)
                    self.map = suffix
                    self.populate()
                    log.write("map loaded: %s\n" % suffix)
                background.SetShadowLevel(0)
                for part in (background.PART_SKY, background.PART_CLOUD, background.PART_WATER):
                    background.SetVisiblePart(part, 0)
                background.SetVisiblePart(background.PART_TREE, int(show_trees))
                background.SetVisiblePart(background.PART_OBJECT, 1)
                background.SetTransparentTree(True)
                background.SetViewDistanceSet(background.DISTANCE0, 32000.0)
                background.SelectViewDistanceNum(background.DISTANCE0)
                self.phase = phase
                log.write("phase=%d map=%s area=%s distance=%d rotation=%d trees=%d actors=%d\n" %
                          (phase, suffix, area, distance, rotation, show_trees, len(self.actors)))
                log.flush()
            z = background.GetHeight(x, y)
            self.position = (x, y, z)
            self.camera = (float(distance), 28.0, rotation + math.sin(elapsed * 0.25) * 12.0)
            app.SetCenterPosition(x, -y, z + 200.0)
            app.SetCamera(*self.camera, 0.0)
            background.Update(x, -y, z)
            for index, vid in enumerate(self.actors):
                dx, dy = (index % 3 - 1) * 380.0, (index // 3 - 0.5) * 650.0
                chr.SelectInstance(vid)
                chr.SetPixelPosition(int(x + dx), int(y + dy), int(background.GetHeight(x + dx, y + dy)))
                chr.SetRotation(float(rotation))
            chr.Update()
            self.frames += 1
            sample = int(elapsed / 2.0)
            if sample != self.last_sample:
                log.write("sample seconds=%.1f frames=%d fps=%s\n" % (elapsed, self.frames, app.GetRenderFPS()))
                log.flush()
                self.last_sample = sample
            if elapsed >= len(PHASES) * PHASE_SECONDS:
                app.Exit()

        def OnRender(self):
            x, y, z = self.position
            grp.SetPositionCamera(x, -y, z + 200.0, *self.camera)
            grp.SetPerspective(30.0, float(width) / height, 100.0, 32000.0)
            grp.Culling()
            chr.Deform()
            grp.SetGameRenderState()
            grp.PushState()
            background.BeginEnvironment()
            background.Render()
            background.SetCharacterDirLight()
            chr.Render()
            background.RenderPCBlocker()
            background.EndEnvironment()
            grp.PopState()
            grp.SetInterfaceRenderState()

        def OnPressEscapeKey(self):
            app.Exit()
            return True

    window = TreeWindow()
    app.Loop()
    chr.Destroy()
    background.Destroy()
    window.Hide()
    window.Destroy()
    log.write("normal tree/actor/map/window shutdown\n")
    log.close()
