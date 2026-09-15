"""Short real-client settings test, separate from server login acceptance."""
import app, background, builtins, grp, json, mouseModule, systemSetting, time, ui, wndMgr
import uisystemoption

width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
wndMgr.SetScreenSize(width, height)
app.Create("G0-X Graphics Settings", width, height, 1)
app.SetMouseHandler(mouseModule.mouseController)
wndMgr.SetMouseHandler(mouseModule.mouseController)
if not mouseModule.mouseController.Create():
    raise RuntimeError("Mouse UI initialization failed")
app.SetCameraMaxDistance(20000.0)
if not app.LoadLocaleData(app.GetLocalePath()):
    raise RuntimeError("Locale required")
try:
    with builtins.old_open("g0x-expected.json") as stream:
        expected_restart = json.load(stream)
except FileNotFoundError:
    expected_restart = None
log = builtins.old_open("g0x-restart.log" if expected_restart else "g0x-settings.log", "w")
initial = systemSetting.GetGraphicsSettings()
if expected_restart and initial != expected_restart:
    raise RuntimeError("Client restart did not retain every graphics setting")


class World(ui.Window):
    def __init__(self):
        ui.Window.__init__(self)
        self.SetSize(width, height)
        self.Show()
        self.frames = 0
        self.step = -1
        self.started = time.monotonic()
        self.options = None
        self.shots = set()
        self.last_revision = 0
        self.loadMap("a1", 44000, 27200)

    def loadMap(self, name, x, y):
        background.Initialize()
        background.LoadMap("metin2_map_" + name, float(x), float(y), 0.0)
        background.SetViewDistanceSet(background.DISTANCE0, 25600.0)
        background.SelectViewDistanceNum(background.DISTANCE0)
        self.position = (x, y, background.GetHeight(x, y))

    def OnUpdate(self):
        elapsed = time.monotonic() - self.started
        step = int(elapsed / 2)
        changed = step != self.step
        if step >= (2 if expected_restart else 14):
            if not expected_restart:
                # Step 13 proves Classic teardown. Persist Modern again so
                # the second process proves its startup and saved settings.
                systemSetting.ApplyGraphicsSettings({"style": 1})
                assert systemSetting.SaveGraphicsSettings()
                with builtins.old_open("g0x-expected.json", "w") as stream:
                    json.dump(systemSetting.GetGraphicsSettings(), stream)
            app.Exit()
            return
        if step != self.step:
            self.step = step
            if self.options is None:
                self.options = uisystemoption.OptionDialog()
                self.options.Show()
                self.options.SetPosition(20, 90)
                self.options.OpenGraphics()
                self.options.graphicsDialog.SetPosition(355, 90)
            dialog = self.options.graphicsDialog
            if not expected_restart:
                if 1 <= step <= 4:
                    dialog.preset.SelectItem(step - 1)
                    assert systemSetting.GetGraphicsSettings()["preset"] == step - 1
                elif step == 5:
                    dialog.vegetation.SelectItem(1)
                    assert systemSetting.GetGraphicsSettings()["preset"] == 4
                elif step == 6:
                    dialog.distance.cursor.Down()
                    dialog.distance.SetSliderPos(.35)
                    dialog.distance.cursor.SetUp()
                    assert 17000 < systemSetting.GetGraphicsSettings()["viewDistance"] < 18000
                    log.write("pressedSlider=PASS\n")
                elif step == 7:
                    dialog.fog.SelectItem(2)
                elif step == 8:
                    # G56 enables the existing Modern HDR/atmosphere controls.
                    systemSetting.ApplyGraphicsSettings({"style": 1, "ambientOcclusion": 2, "hdr": 1,
                        "bloom": 1, "modernSky": 1, "highQualityFog": 1, "shadows": 2, "water": 0, "textures": 2})
                    assert systemSetting.SaveGraphicsSettings()
                    self.expected = systemSetting.GetGraphicsSettings()
                    systemSetting.ApplyGraphicsPreset(0)
                    assert systemSetting.LoadGraphicsSettings()
                    assert systemSetting.GetGraphicsSettings() == self.expected
                    with builtins.old_open("g0x-expected.json", "w") as stream:
                        json.dump(self.expected, stream)
                    dialog.Refresh()
                elif step == 9:
                    dialog.Close()
                    dialog.Open()
                    dialog.vegetation.OnMouseLeftButtonUp()
                    dialog.preset.OnMouseLeftButtonUp()
                    assert not dialog.vegetation.isListOpened and dialog.preset.isListOpened
                    window_result = systemSetting.TestGraphicsWindow()
                    assert all(window_result.values()), window_result
                    log.write("window=%s\n" % json.dumps(window_result, sort_keys=True))
                elif step == 10:
                    dialog.preset.CloseListBox()
                    systemSetting.ApplyGraphicsSettings({"bloom": 0, "modernSky": 0, "highQualityFog": 0})
                    dialog.Refresh()
                elif step == 11:
                    systemSetting.ApplyGraphicsSettings({"bloom": 1, "modernSky": 1, "highQualityFog": 1})
                    dialog.Refresh()
                elif step == 12:
                    background.Destroy()
                    self.loadMap("b1", 69642, 54848)
                elif step == 13:
                    systemSetting.ApplyGraphicsSettings({"style": 0})
                    self.options.Close()
                    self.options.Show()
                    self.options.OpenGraphics()
            current = systemSetting.GetGraphicsSettings()
            log.write("step=%d settings=%s\n" % (step, json.dumps(current, sort_keys=True)))
            log.flush()
        runtime = systemSetting.GetGraphicsRuntimeConfig()
        current = systemSetting.GetGraphicsSettings()
        # Query for assertions only in this test root; production UI has no polling.
        # Renderer snapshots are consumed at the next frame boundary, after
        # this callback has applied the requested setting.
        if self.frames > 1 and not changed:
            expected_shadows = current["shadows"] if current["style"] == 1 else 0
            expected_ao = current["ambientOcclusion"] if current["style"] == 1 else 0
            assert runtime["shadows"] == expected_shadows and runtime["ambientOcclusion"] == expected_ao
            modern = current["style"] == 1
            assert runtime["hdr"] == int(modern)
            assert runtime["bloom"] == (current["bloom"] if modern else 0)
            assert runtime["modernSky"] == (current["modernSky"] if modern else 0)
            assert runtime["waterFrameMilliseconds"] == 70
            if runtime["revision"] != self.last_revision:
                log.write("runtime=%s\n" % json.dumps(runtime, sort_keys=True))
                log.flush()
                self.last_revision = runtime["revision"]
        x, y, z = self.position
        app.SetCenterPosition(x, -y, z + 100)
        app.SetCamera(5000, 35, 0, 0)
        background.Update(x, -y, z)

    def OnRender(self):
        x, y, z = self.position
        grp.SetPositionCamera(x, -y, z + 100, 5000, 35, 0)
        app.RenderGame()
        grp.SetInterfaceRenderState()
        self.frames += 1


class Capture(ui.Window):
    def __init__(self, world):
        ui.Window.__init__(self, "TOP_MOST")
        self.world = world
        self.Show()

    def OnRender(self):
        world = self.world
        if world.step >= 0 and world.step not in world.shots and time.monotonic() - world.started - world.step * 2 > .7:
            success, path = grp.SaveScreenShotToPath("g0x-%02d-" % world.step)
            if not success:
                raise RuntimeError("Settings screenshot failed")
            world.shots.add(world.step)
            log.write("image=%s\n" % path)
            log.flush()


world = World()
capture = Capture(world)
app.Loop()
if world.options:
    world.options.Close()
    world.options.Destroy()
    world.options.Hide()
world.options = None
capture.Hide()
capture.Destroy()
world.Hide()
world.Destroy()
background.Destroy()
log.write("PASS steps=%d frames=%d screenshots=%d restart=%d\n" % (world.step + 1, world.frames, len(world.shots), bool(expected_restart)))
log.close()
