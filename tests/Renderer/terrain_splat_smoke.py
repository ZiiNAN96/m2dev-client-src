"""Test-only prototype for an isolated runtime pack. Uses the real client/map/camera.

Not loaded by normal distributions. No network credentials or alternate map loader.
"""
import app
import background
import builtins
import grp
import systemSetting
import time
import ui
import wndMgr


def isolate_terrain():
    background.SetShadowLevel(0)
    for part in (background.PART_OBJECT, background.PART_TREE, background.PART_SKY,
                 background.PART_CLOUD, background.PART_WATER):
        background.SetVisiblePart(part, 0)


def run():
    wndMgr.SetScreenSize(systemSetting.GetWidth(), systemSetting.GetHeight())
    app.Create("Metin2 terrain splatting 3B test", systemSetting.GetWidth(), systemSetting.GetHeight(), 1)
    app.SetCameraMaxDistance(30000.0)
    app.SetSightRange(25600)  # Disable the application's frame-time-dependent sight reduction for parity.
    grp.SetClearColor(0.08, 0.16, 0.28)
    background.Initialize()
    background.LoadMap("metin2_map_a1", 50000.0, 60000.0, 0.0)
    isolate_terrain()
    background.SetViewDistanceSet(background.DISTANCE0, 25600.0)
    background.SelectViewDistanceNum(background.DISTANCE0)
    log = builtins.old_open("terrain-map-test.log", "w")
    log.write("real map metin2_map_a1 loaded through background.LoadMap\n")
    log.flush()

    class MapWindow(ui.Window):
        def __init__(self):
            ui.Window.__init__(self)
            self.started = time.monotonic()
            self.last_pose = -1
            self.logged = -1
            self.SetSize(systemSetting.GetWidth(), systemSetting.GetHeight())
            self.Show()

        def OnUpdate(self):
            # Feed the existing application camera, not a test-camera implementation.
            poses = [(50000, 60000, 7000, 5, 0), (50000, 60000, 7000, 10, 95),
                     (50000, 60000, 3500, 45, 180), (54000, 63000, 10000, 5, 270),
                     (50000, 60000, 7000, 5, 0), (50000, 60000, 7000, 5, 0)]
            pose = min(int((time.monotonic() - self.started) / 25), 5)
            if pose != self.last_pose:
                if pose in (4, 5):
                    map_name = "metin2_map_b1" if pose == 4 else "metin2_map_a1"
                    # Normal game-phase transition: GameWindow.Close destroys the old
                    # background; the next loading phase creates it before LoadMap.
                    background.Destroy()
                    background.Initialize()
                    background.LoadMap(map_name, 50000.0, 60000.0, 0.0)
                    isolate_terrain()
                    background.SetViewDistanceSet(background.DISTANCE0, 25600.0)
                    log.write("map change: %s\n" % map_name)
                    log.flush()
                x, y, distance, pitch, rotation = poses[pose]
                self.position = (float(x), float(y), background.GetHeight(float(x), float(y)))
                # Python SetCenterPosition and the C++ setter each negate Y.
                # Its public argument is render-space Y (unlike LoadMap's map Y).
                app.SetCenterPosition(self.position[0], -self.position[1], self.position[2])
                app.SetCamera(float(distance), float(pitch), float(rotation), 0.0)
                self.camera = (float(distance), float(pitch), float(rotation))
                self.last_pose = pose
            background.Update(self.position[0], -self.position[1], self.position[2])
            if time.monotonic() - self.started > 150:
                app.Exit()

        def OnRender(self):
            # Same client camera and map renderer, isolated from the later PCBlocker
            # pass, which can re-show hidden trees. Fix the pose immediately before
            # rendering so application collision/easing cannot change the A/B view.
            grp.SetPositionCamera(self.position[0], -self.position[1], self.position[2], *self.camera)
            grp.SetPerspective(30.0, float(systemSetting.GetWidth()) / systemSetting.GetHeight(), 100.0, 25600.0)
            grp.SetGameRenderState()
            grp.PushState()
            background.BeginEnvironment()
            background.Render()
            background.EndEnvironment()
            if int((time.monotonic() - self.started) / 3) != self.logged:
                log.write("pose=%d position=%s camera=%s sight=25600 splats=%s\n" %
                          (self.last_pose, self.position, self.camera, background.GetRenderedSplatNum()))
                log.flush()
                self.logged = int((time.monotonic() - self.started) / 3)
            grp.PopState()
            grp.SetInterfaceRenderState()

        def OnPressEscapeKey(self):
            app.Exit()
            return True

    window = MapWindow()
    app.Loop()
    # Match game.GameWindow.Close: unload the map before application collision cleanup.
    background.Destroy()
    window.Hide()
    window.Destroy()
    log.write("loop returned; application performs normal resource shutdown\n")
    log.close()
