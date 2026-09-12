"""Isolated 4B real packed maps: native loaders/lists, no actors or account login."""
import app
import background
import builtins
import grp
import systemSetting
import time
import ui
import wndMgr

POSE_SECONDS = 25
# Identical camera poses for both backends. Coordinates from the original AreaData.
POSES = [
    ("metin2_map_a1", 58082.84375, 60984.550781, 2500, 15, 0, 0, "a1-hotel"),
    ("metin2_map_a1", 58082.84375, 60984.550781, 2500, 15, 0, 3, "a1-hotel-shadow"),
    ("metin2_map_a1", 62744.542969, 45875.074219, 5000, 25, 0, 0, "a1-bank"),
    ("metin2_map_a1", 62744.542969, 45875.074219, 5000, 25, 0, 3, "a1-bank-shadow"),
    ("metin2_map_b1", 69641.585938, 54848.457031, 2300, 20, 0, 0, "b1-two-sided-prop"),
    ("metin2_map_b1", 69641.585938, 54848.457031, 2300, 20, 0, 3, "b1-two-sided-prop-shadow"),
    ("metin2_map_b1", 69641.585938, 54848.457031, 2300, 20, 180, 3, "b1-two-sided-prop-reverse"),
    ("metin2_map_b1", 52096.035156, 69551.843750, 5000, 25, 40, 0, "b1-multimesh"),
    ("metin2_map_b1", 52096.035156, 69551.843750, 5000, 25, 40, 3, "b1-multimesh-shadow"),
    ("metin2_map_a1", 58082.84375, 60984.550781, 2500, 15, 0, 0, "a1-return"),
    ("metin2_map_a1", 58082.84375, 60984.550781, 2500, 15, 0, 3, "a1-return-shadow"),
    ("metin2_map_a1", 63058.957031, 64182.101563, 2300, 20, 0, 0, "a1-16bit-drum"),
    ("metin2_map_a1", 63058.957031, 64182.101563, 2300, 20, 0, 3, "a1-16bit-drum-shadow"),
]


def run():
    width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
    wndMgr.SetScreenSize(width, height)
    app.Create("Metin2 static materials 4B test", width, height, 1)
    app.SetCameraMaxDistance(30000.0)
    app.SetSightRange(25600)
    grp.SetClearColor(0.08, 0.16, 0.28)
    log = builtins.old_open("static-material-test.log", "w")

    class MapWindow(ui.Window):
        def __init__(self):
            ui.Window.__init__(self)
            self.started = time.monotonic()
            self.last_pose = -1
            self.map = None
            self.logged = -1
            self.SetSize(width, height)
            self.Show()

        def OnUpdate(self):
            elapsed = time.monotonic() - self.started
            pose = min(int(elapsed / POSE_SECONDS), len(POSES) - 1)
            if pose != self.last_pose:
                name, x, y, distance, pitch, rotation, shadow, label = POSES[pose]
                if name != self.map:
                    if self.map is not None:
                        background.Destroy()
                    background.Initialize()
                    background.LoadMap(name, float(x), float(y), 0.0)
                    self.map = name
                    log.write("map loaded: %s\n" % name)
                background.SetShadowLevel(shadow)
                for part in (background.PART_TREE, background.PART_SKY, background.PART_CLOUD, background.PART_WATER):
                    background.SetVisiblePart(part, 0)
                background.SetVisiblePart(background.PART_OBJECT, 1)
                background.SetViewDistanceSet(background.DISTANCE0, 25600.0)
                background.SelectViewDistanceNum(background.DISTANCE0)
                self.position = (float(x), float(y), background.GetHeight(float(x), float(y)) + 350.0)
                self.camera = (float(distance), float(pitch), float(rotation))
                app.SetCenterPosition(self.position[0], -self.position[1], self.position[2])
                app.SetCamera(*self.camera, 0.0)
                self.last_pose = pose
                log.write("pose=%d label=%s shadow=%d position=%s camera=%s\n" %
                          (pose, label, shadow, self.position, self.camera))
                log.flush()
            background.Update(self.position[0], -self.position[1], self.position[2])
            if elapsed > len(POSES) * POSE_SECONDS:
                app.Exit()

        def OnRender(self):
            grp.SetPositionCamera(self.position[0], -self.position[1], self.position[2], *self.camera)
            grp.SetPerspective(30.0, float(width) / height, 100.0, 25600.0)
            grp.Culling()
            grp.SetGameRenderState()
            grp.PushState()
            # The existing native shadow target is cleared by its normal path;
            # no new shadow resource or Diligent shadow implementation is added.
            background.RenderCharacterShadowToTexture()
            background.BeginEnvironment()
            background.Render()
            background.EndEnvironment()
            background.BeginEnvironment()
            background.RenderPCBlocker()
            background.EndEnvironment()
            tick = int((time.monotonic() - self.started) / 3)
            if tick != self.logged:
                log.write("frame pose=%d splats=%s\n" % (self.last_pose, background.GetRenderedSplatNum()))
                log.flush()
                self.logged = tick
            grp.PopState()
            grp.SetInterfaceRenderState()

        def OnPressEscapeKey(self):
            app.Exit()
            return True

    window = MapWindow()
    app.Loop()
    background.Destroy()
    window.Hide()
    window.Destroy()
    log.write("normal map/window shutdown\n")
    log.close()
