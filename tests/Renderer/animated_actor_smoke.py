"""ZiiNAN: Real original Granny body/motions through existing Python game APIs only."""
import app
import background
import builtins
import chr
import chrmgr
import grp
import math
import systemSetting
import time
import ui
import wndMgr

POSE_SECONDS = 18
# ZiiNAN: Repeated real-map lifecycle with one selected body, no network/account access.
POSES = [
    ("metin2_map_a1", "idle-front", 0, 650),
    ("metin2_map_a1", "mounted-excluded", 0, 650),
    ("metin2_map_a1", "unmounted-return", 0, 650),
    ("metin2_map_a1", "run", 0, 650),
    ("metin2_map_a1", "attack", 0, 650),
    ("metin2_map_a1", "idle-back", 180, 650),
    ("metin2_map_a1", "hidden", 0, 650),
    ("metin2_map_a1", "restore", 90, 650),
    ("metin2_map_a1", "out-of-view", 0, 650),
    ("metin2_map_a1", "reenter", 0, 650),
    ("metin2_map_a1", "deleted", 0, 650),
    ("metin2_map_b1", "recreated-idle", 0, 650),
    ("metin2_map_b1", "run", 90, 650),
    ("metin2_map_b1", "attack", 180, 650),
    ("metin2_map_a1", "return-idle", 0, 650),
    ("metin2_map_a1", "lod-far", 0, 7000),
    ("metin2_map_a1", "lod-near", 0, 650),
    ("metin2_map_a1", "final-delete", 0, 650),
]


def run():
    width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
    wndMgr.SetScreenSize(width, height)
    app.Create("Metin2 animated actor 5A test", width, height, 1)
    app.SetCameraMaxDistance(30000.0)
    app.SetSightRange(25600)
    grp.SetClearColor(0.08, 0.16, 0.28)
    log = builtins.old_open("animated-actor-test.log", "w")

    # ZiiNAN: Native RaceManager loads the original MSM/MSA/GR2; no animation replacement.
    chrmgr.CreateRace(0)
    chrmgr.SelectRace(0)
    chrmgr.LoadLocalRaceData("msm/warrior_m.msm")
    chrmgr.SetPathName("d:/ymir work/pc/warrior/general/")
    chrmgr.RegisterMotionMode(chr.MOTION_MODE_GENERAL)
    for motion, name in ((chr.MOTION_WAIT, "wait.msa"),
                         (chr.MOTION_RUN, "run.msa"),
                         (chr.MOTION_NORMAL_ATTACK, "attack.msa")):
        chrmgr.RegisterMotionData(chr.MOTION_MODE_GENERAL, motion, name)

    # ZiiNAN: Regression fixture uses the real legacy mount creation; no mount migration.
    chrmgr.SetPathName("d:/ymir work/pc/warrior/horse/")
    chrmgr.RegisterMotionMode(chr.MOTION_MODE_HORSE)
    chrmgr.RegisterMotionData(chr.MOTION_MODE_HORSE, chr.MOTION_WAIT, "wait.msa")
    chrmgr.CreateRace(20114)
    chrmgr.SelectRace(20114)
    chrmgr.SetPathName("d:/ymir work/npc/lion_white/")
    chrmgr.LoadRaceData("lion_white.msm")
    chrmgr.RegisterMotionMode(chr.MOTION_MODE_GENERAL)
    chrmgr.RegisterMotionData(chr.MOTION_MODE_GENERAL, chr.MOTION_WAIT, "wait.msa")

    class ActorWindow(ui.Window):
        def __init__(self):
            ui.Window.__init__(self)
            self.started = time.monotonic()
            self.last_pose = -1
            self.map = None
            self.last_attack = -1
            self.SetSize(width, height)
            self.Show()

        def spawn(self, mounted=False):
            if mounted:
                chr.CreateInstance(50001, {"horse": 20114})
            else:
                chr.CreateInstance(50001)
            chr.SelectInstance(50001)
            chr.SetVirtualID(50001)
            chr.SetInstanceType(chr.INSTANCE_TYPE_PLAYER)
            chr.SetRace(0)
            chr.SetArmor(0)
            # No separate hair or weapon is attached in this selected body comparison.
            chr.Refresh()
            chr.SetMotionMode(chr.MOTION_MODE_HORSE if mounted else chr.MOTION_MODE_GENERAL)
            chr.SetLoopMotion(chr.MOTION_WAIT)
            chr.Show()
            log.write("spawn warrior_m shape=0 mount=%d\n" % (20114 if mounted else 0))

        def OnUpdate(self):
            elapsed = time.monotonic() - self.started
            pose = min(int(elapsed / POSE_SECONDS), len(POSES) - 1)
            name, label, rotation, distance = POSES[pose]
            if pose != self.last_pose:
                if name != self.map:
                    chr.Destroy()
                    if self.map is not None:
                        background.Destroy()
                    background.Initialize()
                    x, y = (58300.0, 63000.0) if name.endswith("a1") else (69100.0, 56000.0)
                    background.LoadMap(name, x, y, 0.0)
                    self.map = name
                    self.position = (x, y, background.GetHeight(x, y))
                    log.write("map loaded: %s position=%s\n" % (name, self.position))
                background.SetShadowLevel(0)
                for part in (background.PART_TREE, background.PART_SKY, background.PART_CLOUD, background.PART_WATER):
                    background.SetVisiblePart(part, 0)
                background.SetVisiblePart(background.PART_OBJECT, 1)
                background.SetViewDistanceSet(background.DISTANCE0, 25600.0)
                background.SelectViewDistanceNum(background.DISTANCE0)
                self.camera = (float(distance), 15.0, 0.0)
                if label in ("deleted", "final-delete"):
                    chr.DeleteInstance(50001)
                else:
                    # ZiiNAN: Destroy/recreate through existing APIs; Diligent must not own the mount.
                    if label in ("mounted-excluded", "unmounted-return"):
                        chr.DeleteInstance(50001)
                    if not chr.HasInstance(50001):
                        self.spawn(label == "mounted-excluded")
                    chr.SelectInstance(50001)
                    chr.SetRotation(float(rotation))
                    chr.SetLoopMotion(chr.MOTION_RUN if label == "run" else chr.MOTION_WAIT)
                    if label == "hidden":
                        chr.Hide()
                    else:
                        chr.Show()
                self.last_pose = pose
                self.last_attack = -1
                log.write("pose=%d label=%s rotation=%d distance=%d\n" % (pose, label, rotation, distance))
                log.flush()
            x, y, z = self.position
            app.SetCenterPosition(x, -y, z + 85.0)
            app.SetCamera(*self.camera, 0.0)
            background.Update(x, -y, z)
            if chr.HasInstance(50001):
                chr.SelectInstance(50001)
                actor_x = x + (20000.0 if label == "out-of-view" else 0.0)
                if label == "run":
                    actor_x += math.sin(elapsed * 1.2) * 140.0
                chr.SetPixelPosition(int(actor_x), int(y), int(z))
                if label == "attack":
                    attack = int(elapsed / 2.5)
                    if attack != self.last_attack:
                        chr.PushOnceMotion(chr.MOTION_NORMAL_ATTACK, 0.1)
                        self.last_attack = attack
            chr.Update()
            if elapsed >= len(POSES) * POSE_SECONDS:
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

    window = ActorWindow()
    app.Loop()
    chr.Destroy()
    background.Destroy()
    window.Hide()
    window.Destroy()
    log.write("normal actor/map/window shutdown\n")
    log.close()
