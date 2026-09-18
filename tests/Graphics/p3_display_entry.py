"""Native display transaction smoke, using the existing graphics UI callbacks."""
import app, builtins, grp, json, os, systemSetting, time, traceback, ui, wndMgr
import mouseModule, uigraphicssettings

width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
wndMgr.SetScreenSize(width, height)
app.Create("P3 Display Settings", width, height, 1)
app.SetMouseHandler(mouseModule.mouseController)
wndMgr.SetMouseHandler(mouseModule.mouseController)
assert mouseModule.mouseController.Create()
log = builtins.old_open("display-run.jsonl", "w")


def emit(event, **values):
    values.update(event=event, seconds=time.monotonic())
    log.write(json.dumps(values) + "\n")
    log.flush()


def settings():
    return systemSetting.GetGraphicsSettings()


def verify():
    s, r, actual = settings(), systemSetting.GetGraphicsRuntimeConfig(), systemSetting.GetDisplayOptions()
    for key in ("resolutionWidth", "resolutionHeight", "displayMode", "frameRateLimit", "vsync"):
        assert s[key] == r[key], (key, s, r)
    assert (actual["clientWidth"], actual["clientHeight"]) == (s["resolutionWidth"], s["resolutionHeight"])
    assert actual["borderless"] == (s["displayMode"] == 1)
    assert wndMgr.GetScreenWidth() == s["resolutionWidth"] and wndMgr.GetScreenHeight() == s["resolutionHeight"]
    emit("verified", settings=s, native=actual)


class World(ui.Window):
    def __init__(self):
        ui.Window.__init__(self)
        import background, chr, chrmgr, effect, item, player, playersettingmodule
        self.background, self.chr, self.chrmgr, self.effect = background, chr, chrmgr, effect
        assert app.LoadLocaleData(app.GetLocalePath())
        item.LoadItemTable(app.GetLocalePath() + "/item_proto")
        getattr(playersettingmodule, "__LoadGameNPC")()
        getattr(playersettingmodule, "__LoadGameEffect")()
        for name in ("INIT", "WARRIOR", "ASSASSIN", "SURA", "SHAMAN"):
            playersettingmodule.LoadGameData(name)
        background.Initialize()
        background.LoadMap("metin2_map_a1", 63500.0, 59000.0, 0.0)
        self.origin = (63500, 59000, background.GetHeight(63500, 59000))
        chr.CreateInstance(57900)
        chr.SelectInstance(57900)
        chr.SetVirtualID(57900)
        chr.SetInstanceType(6)
        chr.SetRace(0)
        chr.SetArmor(11299)
        chr.SetHair(1001)
        chr.SetWeapon(19)
        chr.SetMotionMode(chr.MOTION_MODE_ONEHAND_SWORD)
        chr.SetLoopMotion(chr.MOTION_WAIT)
        chr.SetPixelPosition(*map(int, self.origin))
        chr.Show()
        player.SetMainCharacterIndex(57900)
        app.SetCenterPosition(self.origin[0], -self.origin[1], self.origin[2] + 100)
        app.SetCamera(5500.0, 22.0, 0.0, 0.0)
        import interfaceModule
        self.interface = interfaceModule.Interface()
        self.interface.MakeInterface()
        self.interface.ShowDefaultWindows()
        self.size = (wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight())
        self.prewarm = True
        self.Show()

    def OnUpdate(self):
        size = (wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight())
        if size != self.size:
            self.SetSize(*size)
            self.interface.OnScreenSizeChange(*size)
            self.size = size
            emit("hud_resize", size=size, taskbar=self.interface.wndTaskBar.GetLocalPosition(), minimap=self.interface.wndMiniMap.GetLocalPosition())
        self.background.Update(self.origin[0], self.origin[1], self.origin[2])
        self.chr.Update()
        self.effect.Update()

    def OnRender(self):
        x, y, z = self.chr.GetPixelPosition(57900)
        distance, pitch, rotation, _ = app.GetCamera()
        grp.SetPositionCamera(x, -y, z + 100, distance, pitch, rotation)
        if self.prewarm:
            assert self.chrmgr.PrewarmVisibleActors(True)
            self.prewarm = False
        app.RenderGame()
        grp.SetInterfaceRenderState()

    def Destroy(self):
        self.interface.Close()
        self.chr.Destroy()
        self.background.Destroy()
        self.Hide()


class Probe(ui.Window):
    def __init__(self):
        ui.Window.__init__(self, "TOP_MOST")
        self.dialog = uigraphicssettings.GraphicsDialog()
        self.dialog.Open()
        self.shot = None
        self.done = False
        self.steps = self.matrix()
        self.deadline = time.monotonic() + 1.0
        self.Show()

    def matrix(self):
        case = "matrix"
        if os.path.exists("display-case.txt"):
            with builtins.old_open("display-case.txt") as stream:
                case = stream.read().strip()
        verify()
        if case == "restart":
            with builtins.old_open("display-expected.json") as stream:
                assert settings() == json.load(stream), "Restart lost confirmed settings"
        elif case == "persist-borderless":
            # Check the bounded dropdown and the last enumerated item through its scrollbar.
            self.dialog.resolution.OnMouseLeftButtonUp()
            self.dialog.resolution.scroll.SetPos(1.0)
            self.shot = "resolution-dropdown"
            yield 0.5
            self.dialog.CloseOtherCombos(None)
            self.dialog.OnDisplayMode(1)
            yield 0.8
            verify()
            self.dialog.ConfirmDisplay()
            assert systemSetting.GetDisplayConfirmationSeconds() == 0
            with builtins.old_open("display-expected.json", "w") as stream:
                json.dump(settings(), stream)
        elif case == "fallback":
            assert settings()["displayMode"] == 0
            assert (settings()["resolutionWidth"], settings()["resolutionHeight"]) in systemSetting.GetDisplayOptions()["resolutions"]
            emit("startup_fallback", settings=settings())
        else:
            original = settings()
            assert not systemSetting.GetDisplayOptions()["exclusiveSupported"]
            assert not systemSetting.ApplyGraphicsSettings({"displayMode": 2}), "Unsupported exclusive was offered"
            assert not systemSetting.ApplyGraphicsSettings({"resolutionWidth": 1234, "resolutionHeight": 567})
            assert settings() == original
            self.dialog.OnDisplayMode(1)
            yield 0.8
            verify()
            assert systemSetting.GetDisplayConfirmationSeconds() > 0
            assert self.dialog.confirmation and self.dialog.confirmation.IsShow()
            self.shot = "borderless-confirm"
            yield 0.5
            assert self.dialog.ConfirmDisplay()
            assert systemSetting.GetDisplayConfirmationSeconds() == 0
            self.dialog.OnDisplayMode(0)
            yield 0.8
            verify()
            self.dialog.ConfirmDisplay()
            options = systemSetting.GetDisplayOptions()
            # Exercise three resolutions from the actual monitor, including 800x600 when reported.
            modes = options["resolutions"]
            for mode in (modes[0], modes[len(modes) // 2], modes[-1]):
                self.dialog.RefreshDisplayOptions()
                if mode == (settings()["resolutionWidth"], settings()["resolutionHeight"]):
                    continue
                self.dialog.OnResolution(self.dialog.resolutions.index(mode))
                yield 0.8
                verify()
                self.shot = "resolution-%dx%d" % mode
                yield 0.2
                self.dialog.ConfirmDisplay()
            previous = settings()
            self.dialog.OnDisplayMode(1)
            yield 0.5
            start = time.monotonic()
            assert systemSetting.SaveGraphicsSettings()
            with builtins.old_open("config/graphics.cfg") as stream:
                assert "DISPLAY_MODE 0" in stream.read(), "Preview leaked into saved settings"
            self.dialog.Hide()  # Timeout must not depend on a Python dialog's OnUpdate.
            yield 15.2
            verify()
            assert settings() == previous, "Timeout failed to restore previous state"
            emit("timeout_rollback", elapsed=time.monotonic() - start)
            self.dialog.Open()
            yield 0.3
            self.dialog.OnDisplayMode(1)
            yield 0.5
            self.dialog.CancelDisplay()
            yield 0.5
            verify()
            assert settings() == previous, "Explicit cancel failed"
            for limit, vsync in ((0, 0), (1, 0), (2, 0), (0, 1), (1, 1), (2, 1)):
                self.dialog.OnFrameRateLimit(limit)
                self.dialog.OnVSync(vsync)
                yield 0.4
                verify()
                assert (settings()["frameRateLimit"], settings()["vsync"]) == (limit, vsync)
            # Close during a preview and keep independent pacing changes.
            self.dialog.OnDisplayMode(1)
            yield 0.5
            self.dialog.OnFrameRateLimit(1)
            self.dialog.OnVSync(0)
            self.dialog.Close()
            yield 0.6
            verify()
            assert settings()["displayMode"] == 0 and settings()["frameRateLimit"] == 1 and settings()["vsync"] == 0
            assert systemSetting.SaveGraphicsSettings()
            with builtins.old_open("display-expected.json", "w") as stream:
                json.dump(settings(), stream)
            # Exit with another unconfirmed preview: neither explicit Save nor shutdown may persist it.
            self.dialog.Open()
            self.dialog.OnDisplayMode(1)
            yield 0.5
            assert systemSetting.SaveGraphicsSettings()
        emit("completed", case=case)
        self.done = True
        app.Exit()

    def OnUpdate(self):
        if self.done or time.monotonic() < self.deadline:
            return
        try:
            delay = next(self.steps)
            self.deadline = time.monotonic() + delay
        except StopIteration:
            pass
        except Exception:
            with builtins.old_open("display-failure.log", "w") as stream:
                stream.write(traceback.format_exc())
            self.done = True
            app.Exit()

    def OnRender(self):
        if self.shot:
            ok, path = grp.SaveScreenShotToPath(self.shot + "-")
            emit("screenshot", ok=ok, path=path)
            self.shot = None


world = World() if os.path.exists("display-world") else None
probe = Probe()
app.Loop()
probe.dialog.Destroy()
probe.Hide()
if world:
    world.Destroy()
log.close()
