"""Private native fixed-camera fixture, never imported by the regular client."""
import app, background, builtins, chr, chrmgr, effect, grp, item, player
import playersettingmodule, systemSetting, time, ui, wndMgr

VIEWS = []
width, height = systemSetting.GetWidth(), systemSetting.GetHeight()
wndMgr.SetScreenSize(width, height)
app.Create('G8 final visual integration', width, height, 1)
app.SetCameraMaxDistance(40000.0)
app.SetSightRange(24000)
app.SetHairColorEnable(True)
app.SetArmorSpecularEnable(True)
if not app.LoadLocaleData(app.GetLocalePath()):
    raise RuntimeError('Original locale required')
playersettingmodule.__LoadGameNPC()
playersettingmodule.__LoadGameEffect()
item.LoadItemTable(app.GetLocalePath() + '/item_proto')
for stage in ('INIT', 'WARRIOR', 'ASSASSIN', 'SURA', 'SHAMAN'):
    playersettingmodule.LoadGameData(stage)
log = builtins.old_open('g8-visual.log', 'w')


class World(ui.Window):
    def __init__(self):
        ui.Window.__init__(self)
        self.SetSize(width, height)
        self.Show()
        self.phase = -1
        self.started = 0
        self.shot = False
        self.window_checked = False
        self.frames = 0
        self.position = (0, 0, 0)
        self.camera = (4000, 25, 0)
        self.label = ui.TextLine()
        self.label.SetParent(self)
        self.label.SetPosition(20, 20)
        self.label.SetOutline()
        self.label.Show()
        self.next_view()

    def next_view(self):
        self.phase += 1
        if self.phase == len(VIEWS):
            app.Exit()
            return
        v = VIEWS[self.phase]
        chr.Destroy()
        if self.phase:
            background.Destroy()
        systemSetting.ApplyGraphicsSettings({'style': v['style']})
        systemSetting.ApplyGraphicsPreset(v['preset'])
        if v.get('custom'):
            systemSetting.ApplyGraphicsSettings({'bloom': 0})
        if hasattr(systemSetting, 'TestGraphicsSun'):
            systemSetting.TestGraphicsSun(v['sun'])
        if hasattr(systemSetting, 'TestWaterTime'):
            systemSetting.TestWaterTime(10.0)
        if hasattr(systemSetting, 'TestSkyTime'):
            systemSetting.TestSkyTime(-1.0 if v.get('animate_sky') else 10.0)
        background.Initialize()
        name, x, y = v['map'], v['x'], v['y']
        map_name = name if name.startswith('map_') else 'metin2_map_' + name
        background.LoadMap(map_name, float(x), float(y), 0.0)
        if v.get('snow'):
            background.EnableSnow(1)
        z = background.GetHeight(x, y)
        for index, (race, kind) in enumerate(((0, 6), (9003, 1), (101, 0), (691, 0), (20101, 1), (0, 6))):
            vid = 57900 + index
            mounted = index == 5
            chr.CreateInstance(vid, {'horse': 20104 if mounted else 0})
            chr.SelectInstance(vid)
            chr.SetVirtualID(vid)
            chr.SetInstanceType(kind)
            chr.SetRace(race)
            if kind == 6:
                chr.SetArmor(v['armour'])
                chr.SetHair(1001)
                chr.SetWeapon(19)
                chr.SetMotionMode(chr.MOTION_MODE_HORSE_ONEHAND_SWORD if mounted else chr.MOTION_MODE_ONEHAND_SWORD)
            else:
                chr.SetArmor(0)
                chr.SetMotionMode(chr.MOTION_MODE_GENERAL)
            chr.SetLoopMotion(chr.MOTION_WAIT)
            px = x + index * 250
            chr.SetPixelPosition(int(px), int(y), int(background.GetHeight(px, y)))
            chr.Show()
        if v.get('effects'):
            chrmgr.SetAffect(57900, chr.AFFECT_JUMAGAP, 1)
            chrmgr.SetAffect(57901, chr.AFFECT_FIRE, 1)
            self.next_attack = 0
        player.SetMainCharacterIndex(57900)
        self.position = (x, y, z + v.get('elevation', 0))
        self.camera = v['camera']
        self.label.SetText('G8 %02d %s' % (self.phase, v['label']))
        self.started = time.monotonic()
        self.shot = self.window_checked = False
        log.write('view=%d map=%s position=%r camera=%r settings=%r\n' %
                  (self.phase, name, self.position, self.camera, systemSetting.GetGraphicsSettings()))
        log.flush()

    def OnUpdate(self):
        if self.phase >= len(VIEWS):
            return
        elapsed = time.monotonic() - self.started
        if elapsed >= 7 and self.shot:
            self.next_view()
            return
        app.SkinningBenchmarkStage(self.phase, int(elapsed >= 2))
        x, y, z = self.position
        app.SetCenterPosition(x, -y, z + 100)
        app.SetCamera(*self.camera, 0.0)
        background.Update(x, -y, z)
        if VIEWS[self.phase].get('effects') and elapsed >= self.next_attack:
            chr.SelectInstance(57900)
            chr.PushOnceMotion(chr.MOTION_COMBO_ATTACK_1, 0.0)
            self.next_attack = elapsed + .7
        chr.Update()
        effect.Update()
        if VIEWS[self.phase].get('window') and elapsed >= 3 and not self.window_checked:
            log.write('window=%r\n' % systemSetting.TestGraphicsWindow())
            log.flush()
            self.window_checked = True

    def OnRender(self):
        if self.phase >= len(VIEWS):
            return
        x, y, z = self.position
        grp.SetPositionCamera(x, -y, z + 100, *self.camera)
        app.RenderGame()
        grp.SetInterfaceRenderState()
        self.frames += 1
        if time.monotonic() - self.started >= 6 and not self.shot:
            label = VIEWS[self.phase]['label']
            success, path = grp.SaveScreenShotToPath('g8-%02d-%s-' % (self.phase, label))
            if not success:
                raise RuntimeError('Screenshot failed: ' + label)
            self.shot = True
            log.write('capture=%d label=%s path=%s\n' % (self.phase, label, path))
            log.flush()

    def OnPressEscapeKey(self):
        app.Exit()
        return True


app.StartSkinningBenchmark()
window = World()
app.Loop()
chr.Destroy()
background.Destroy()
window.label.Hide()
window.label.Destroy()
window.Hide()
window.Destroy()
log.write('completed=%d frames=%d\n' % (window.phase, window.frames))
log.close()
