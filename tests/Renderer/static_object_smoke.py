"""Isolated real-map 4A smoke: original object, camera, culling and map loaders."""
import app
import background
import builtins
import grp
import systemSetting
import time
import ui
import wndMgr


def setup_map(name, x, y):
    background.Initialize()
    background.LoadMap(name, float(x), float(y), 0.0)
    background.SetShadowLevel(0)  # Shadow-receiver variant is outside the first opaque path.
    for part in (background.PART_TREE, background.PART_SKY, background.PART_CLOUD, background.PART_WATER):
        background.SetVisiblePart(part, 0)
    background.SetVisiblePart(background.PART_OBJECT, 1)
    background.SetViewDistanceSet(background.DISTANCE0, 25600.0)
    background.SelectViewDistanceNum(background.DISTANCE0)


def run():
    width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
    wndMgr.SetScreenSize(width, height)
    app.Create("Metin2 static objects 4A test", width, height, 1)
    app.SetCameraMaxDistance(30000.0)
    app.SetSightRange(25600)
    grp.SetClearColor(0.08, 0.16, 0.28)
    setup_map("metin2_map_a1", 58082.84375, 60984.550781)
    log = builtins.old_open("static-map-test.log", "w")

    class MapWindow(ui.Window):
        def __init__(self):
            ui.Window.__init__(self)
            self.started = time.monotonic()
            self.last_pose = -1
            self.logged = -1
            self.SetSize(width, height)
            self.Show()

        def OnUpdate(self):
            poses = [(58082.84375, 60984.550781, 2500, 15, 0),
                     (58082.84375, 60984.550781, 2500, 20, 95),
                     (58082.84375, 60984.550781, 5000, 35, 180),
                     (65000, 62000, 10000, 12, 270),
                     (50000, 60000, 7000, 15, 0),
                     (58082.84375, 60984.550781, 2500, 15, 0)]
            pose = min(int((time.monotonic() - self.started) / 40), 5)
            if pose != self.last_pose:
                x, y, distance, pitch, rotation = poses[pose]
                if pose in (4, 5):
                    background.Destroy()
                    name = "metin2_map_b1" if pose == 4 else "metin2_map_a1"
                    setup_map(name, x, y)
                    log.write("map change: %s\n" % name)
                self.position = (float(x), float(y), background.GetHeight(float(x), float(y)) + 350.0)
                app.SetCenterPosition(self.position[0], -self.position[1], self.position[2])
                app.SetCamera(float(distance), float(pitch), float(rotation), 0.0)
                self.camera = (float(distance), float(pitch), float(rotation))
                self.last_pose = pose
            background.Update(self.position[0], -self.position[1], self.position[2])
            if time.monotonic() - self.started > 240:
                app.Exit()

        def OnRender(self):
            grp.SetPositionCamera(self.position[0], -self.position[1], self.position[2], *self.camera)
            grp.SetPerspective(30.0, float(width) / height, 100.0, 25600.0)
            grp.Culling()  # Same CCullingManager::Process as CPythonApplication::RenderGame.
            grp.SetGameRenderState()
            grp.PushState()
            background.BeginEnvironment()
            background.Render()
            background.EndEnvironment()
            tick = int((time.monotonic() - self.started) / 3)
            if tick != self.logged:
                log.write("pose=%d position=%s camera=%s splats=%s\n" %
                          (self.last_pose, self.position, self.camera, background.GetRenderedSplatNum()))
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
