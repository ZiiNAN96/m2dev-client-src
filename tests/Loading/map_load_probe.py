"""Opt-in native load probe: original packs, normal map/actor/render paths.

Cold = first map load in a fresh process (OS file cache is uncontrolled).
Warm = destroy and reload in the same process. No network/login claim.
"""
import app, background, builtins, chr, effect, grp, item, player
import playersettingmodule, systemSetting, time, ui, wndMgr

class Phase:
    def __init__(self, name): self.name = name
    def __enter__(self): app.MapLoadTrace('push', self.name)
    def __exit__(self, *args): app.MapLoadTrace('pop')

app.MapLoadTrace('enable')
app.MapLoadTrace('begin', 'client-setup')
with Phase('Python UI startup'):
    width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
    wndMgr.SetScreenSize(width, height)
    app.Create('P0-L map loading probe', width, height, 1)
    app.SetCameraMaxDistance(40000.0)
    app.SetSightRange(24000)
    app.SetHairColorEnable(True)
    app.SetArmorSpecularEnable(True)
    if not app.LoadLocaleData(app.GetLocalePath()):
        raise RuntimeError('Original locale required')
    item.LoadItemTable(app.GetLocalePath() + '/item_proto')
app.MapLoadTrace('end', 'client-setup-complete')

VIEWS = [('A1-cold', 'metin2_map_a1', 13000, 9500),
         ('A1-warm', 'metin2_map_a1', 13000, 9500),
         ('B1-first-in-session', 'metin2_map_b1', 94300, 27100),
         ('B1-warm', 'metin2_map_b1', 94300, 27100)]
log = builtins.old_open('p0l-smoke.log', 'w')

class World(ui.Window):
    def __init__(self):
        ui.Window.__init__(self)
        self.SetSize(width, height)
        self.Show()
        self.index = -1
        self.frames = 0
        self.next_map()

    def next_map(self):
        self.index += 1
        if self.index == len(VIEWS):
            app.Exit()
            return
        label, name, x, y = VIEWS[self.index]
        app.MapLoadTrace('begin', label)
        with Phase('Actors'):
            chr.Destroy()
        if self.index:
            background.Destroy()
        with Phase('Actors'):
            if self.index == 0:
                getattr(playersettingmodule, '__LoadGameNPC')()
                getattr(playersettingmodule, '__LoadGameEffect')()
            for stage in ('INIT', 'WARRIOR', 'ASSASSIN', 'SURA', 'SHAMAN'):
                playersettingmodule.LoadGameData(stage)
        background.Initialize()
        background.LoadMap(name, float(x), float(y), 0.0)
        with Phase('Actors'):
            for i, (race, kind) in enumerate(((0, 6), (9003, 1), (101, 0))):
                vid = 57900 + i
                chr.CreateInstance(vid)
                chr.SelectInstance(vid)
                chr.SetVirtualID(vid)
                chr.SetInstanceType(kind)
                chr.SetRace(race)
                if kind == 6:
                    chr.SetArmor(11299)
                    chr.SetHair(1001)
                    chr.SetWeapon(19)
                    chr.SetMotionMode(chr.MOTION_MODE_ONEHAND_SWORD)
                else:
                    chr.SetArmor(0)
                    chr.SetMotionMode(chr.MOTION_MODE_GENERAL)
                chr.SetLoopMotion(chr.MOTION_WAIT)
                px = x + 250 * i
                chr.SetPixelPosition(int(px), int(y), int(background.GetHeight(px, y)))
                chr.Show()
            player.SetMainCharacterIndex(57900)
        self.position = (x, y, background.GetHeight(x, y))
        self.started = time.monotonic()
        self.shot = False
        self.stable_at = None
        log.write('loaded=%s settings=%r\n' % (label, systemSetting.GetGraphicsSettings()))
        log.flush()

    def OnUpdate(self):
        if self.index >= len(VIEWS): return
        elapsed = time.monotonic() - self.started
        if not app.MapLoadTrace('active'):
            if self.stable_at is None: self.stable_at = elapsed
            if self.shot and elapsed - self.stable_at >= 1.0:
                self.next_map()
                return
        elif elapsed > 45:
            app.MapLoadTrace('end', 'TIMEOUT-no-stable-frame')
            raise RuntimeError('No stable presented frame')
        x, y, z = self.position
        app.SetCenterPosition(x, -y, z + 100)
        app.SetCamera(4500.0, 25.0, 0.0, 0.0)
        background.Update(x, -y, z)
        chr.Update()
        effect.Update()

    def OnRender(self):
        if self.index >= len(VIEWS): return
        x, y, z = self.position
        grp.SetPositionCamera(x, -y, z + 100, 4500.0, 25.0, 0.0)
        app.RenderGame()
        grp.SetInterfaceRenderState()
        self.frames += 1
        # Screenshot waits are deliberately after the measurement endpoint.
        if not app.MapLoadTrace('active') and not self.shot:
            ok, path = grp.SaveScreenShotToPath('p0l-' + VIEWS[self.index][0] + '-')
            if not ok: raise RuntimeError('Screenshot failed')
            self.shot = True
            log.write('capture=%s path=%s\n' % (VIEWS[self.index][0], path))
            log.flush()

    def OnPressEscapeKey(self):
        app.Exit()
        return True

window = World()
app.Loop()
chr.Destroy()
background.Destroy()
window.Hide()
window.Destroy()
log.write('completed=%d frames=%d\n' % (window.index, window.frames))
log.close()
