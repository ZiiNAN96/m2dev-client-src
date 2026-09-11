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


def run():
    wndMgr.SetScreenSize(systemSetting.GetWidth(), systemSetting.GetHeight())
    app.Create("Metin2 terrain renderer test", systemSetting.GetWidth(), systemSetting.GetHeight(), 1)
    app.SetCameraMaxDistance(30000.0)
    background.Initialize()
    background.LoadMap("metin2_map_a1", 50000.0, 60000.0, 0.0)
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
            pose = min(int((time.monotonic() - self.started) / 15), 5)
            if pose != self.last_pose:
                if pose in (4, 5):
                    map_name = "metin2_map_b1" if pose == 4 else "metin2_map_a1"
                    # Normal game-phase transition: GameWindow.Close destroys the old
                    # background; the next loading phase creates it before LoadMap.
                    background.Destroy()
                    background.Initialize()
                    background.LoadMap(map_name, 50000.0, 60000.0, 0.0)
                    background.SetViewDistanceSet(background.DISTANCE0, 25600.0)
                    log.write("map change: %s\n" % map_name)
                    log.flush()
                x, y, distance, pitch, rotation = poses[pose]
                self.position = (float(x), float(y), background.GetHeight(float(x), float(y)))
                # Python SetCenterPosition and the C++ setter each negate Y.
                # Its public argument is render-space Y (unlike LoadMap's map Y).
                app.SetCenterPosition(self.position[0], -self.position[1], self.position[2])
                app.SetCamera(float(distance), float(pitch), float(rotation), 0.0)
                self.last_pose = pose
            background.Update(self.position[0], -self.position[1], self.position[2])
            if time.monotonic() - self.started > 90:
                app.Exit()

        def OnRender(self):
            app.RenderGame()
            if int((time.monotonic() - self.started) / 3) != self.logged:
                log.write("pose=%d position=%s splats=%s\n" %
                          (self.last_pose, self.position, background.GetRenderedSplatNum()))
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
