"""Install original H2 demo assets and fixed cameras into a private test runtime."""
import json
from pathlib import Path
import shutil
import sys

runtime, fixtures, mode = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]
modern = mode not in ('baseline', 'legacy')
if modern:
    source = fixtures / 'vegetation/modern'
    shutil.copytree(source, runtime / 'vegetation/modern', dirs_exist_ok=True)
    texture_root = runtime / 'test-root/root/ymir work/vegetation/modern'
    texture_root.mkdir(parents=True, exist_ok=True)
    for image in source.glob('*.dds'):
        shutil.copy2(image, texture_root / image.name)
    registry_path = runtime / 'vegetation/registry.json'
    registry = json.loads(registry_path.read_text())
    registry['modernOverrides'] = {'d:/ymir work/tree/b1_beech_rt4.spt': 'vegetation/modern/beech.zveg'}
    registry_path.write_text(json.dumps(registry, indent=2), encoding='utf-8')

if mode == 'manual':
    # prepare_runtime.ps1 -Manual retained the original normal-client entrypoint.
    print('Installed optional modern assets; normal client entrypoint retained.')
    sys.exit(0)

def view(label, map='b1', x=68900, y=54000, camera=(3500, 20, -70), **kwargs):
    return dict(label=label, map=map, x=x, y=y, camera=camera, style=1, preset=2, sun=-1, armour=11299, **kwargs)

views = []
if mode in ('preload', 'preload-remote'):
    views = [view('grass-loaded-before-first-frame'),
             view('grass-walk', motion=300, live=True),
             view('grass-remote-tile', x=71800, y=16600, live=True),
             view('grass-return', live=True)]
    for preset in range(4):
        item = view('grass-preloaded-quality-' + str(preset), camera=(1300, 24, -70), live=True)
        item['preset'] = preset
        views.append(item)
    item = view('grass-classic', live=True)
    item['style'] = 0
    views += [item, view('grass-modern-again', live=True),
              view('grass-a1-reload', 'a1', 63500, 59000, (4500, 15, 0)),
              view('grass-b1-reload')]
    if mode == 'preload-remote':
        views = [views[0], views[2], views[3]]
elif mode in ('baseline', 'legacy', 'performance'):
    for preset in range(4):
        item = view('performance-' + ('low', 'medium', 'high', 'ultra')[preset])
        item.update(preset=preset, animate_sky=True, wind=-1)
        views.append(item)
else:
    views = [view('a1', 'a1', 63500, 59000, (4500, 15, 0)), view('b1'),
             view('dense-forest', 'trent', 15000, 15000, (4000, 25, 0))]
    # Actual original B1-beech-rt4 placement in the A1 map audit.
    for label, camera in (('modern-tree-near', (3600, 18, 0)), ('modern-tree-medium', (9000, 18, 0)),
                          ('modern-tree-far', (24000, 18, 0))):
        views.append(view(label, 'a1', 12328, 7030, camera, elevation=900))
    for wind in (0.0, 1.3):
        views.append(view('tree-wind-' + str(wind), 'a1', 12328, 7030, (3600, 18, 0), elevation=900, wind=wind))
    for sun in range(3):
        item = view('foliage-sun-' + str(sun), 'a1', 12328, 7030, (3600, 18, 0), elevation=900)
        item['sun'] = sun
        views.append(item)
    for preset in range(4):
        item = view('grass-quality-' + str(preset), camera=(1300, 24, -70))
        item['preset'] = preset
        item['live'] = preset > 0
        views.append(item)
    views += [view('vegetation-water', x=71500, y=55000, camera=(5000, 18, -70), elevation=400),
              view('slow-camera', camera=(1800, 25, 0), motion=35), view('fast-camera', camera=(5000, 25, 0), motion=1200),
              view('resize-high', window=True), view('resize-ultra', window=True)]
    views[-1]['preset'] = 3
    item = view('classic-return', 'a1', 63500, 59000, (4500, 15, 0))
    item['style'] = 0
    views += [item, view('a1-modern-return', 'a1', 63500, 59000, (4500, 15, 0), live=True)]

source = (Path(__file__).parents[1] / 'Graphics/g8_runtime_entry.py').read_text(encoding='utf-8')
source = source.replace('VIEWS = []', 'VIEWS = ' + repr(views))
source = source.replace('        if self.phase:\n            background.Destroy()', "        if self.phase and not v.get('live'):\n            background.Destroy()")
source = source.replace('        background.Initialize()', "        if not v.get('live'):\n            background.Initialize()")
source = source.replace('        background.LoadMap(map_name, float(x), float(y), 0.0)', "        if not v.get('live'):\n            background.LoadMap(map_name, float(x), float(y), 0.0)")
source = source.replace("self.label.SetText('G8", "self.label.SetText('H2")
source = source.replace("        self.camera = v['camera']", "        self.render_position = self.position\n        if hasattr(systemSetting, 'TestVegetationTime'):\n            systemSetting.TestVegetationTime(v.get('wind', 10.0))\n        self.camera = v['camera']")
source = source.replace('        x, y, z = self.position\n        app.SetCenterPosition', "        x, y, z = self.position\n        x += VIEWS[self.phase].get('motion', 0) * min(elapsed, 5)\n        self.render_position = (x, y, z)\n        app.SetCenterPosition")
source = source.replace('        x, y, z = self.position\n        grp.SetPositionCamera', "        x, y, z = getattr(self, 'render_position', self.position)\n        grp.SetPositionCamera")
source = source.replace('            self.shot = True', "            if hasattr(systemSetting, 'TestVegetationStats'):\n                log.write('vegetation=%d stats=%r\\n' % (self.phase, systemSetting.TestVegetationStats()))\n            self.shot = True")
if mode in ('preload', 'preload-remote'):
    source = source.replace('        z = background.GetHeight(x, y)', '''        background.Update(float(x), -float(y), 0.0)
        prepared = systemSetting.TestVegetationStats()
        keys = ('grassPlacements', 'grassCells', 'grassTiles', 'grassExpectedTiles', 'grassBytes', 'grassSerial')
        snapshot = tuple(prepared[key] for key in keys)
        assert prepared['grassPlacements'] > 0, 'Grass missing before first world frame'
        assert prepared['grassTiles'] == prepared['grassExpectedTiles'], 'Incomplete whole-map preparation'
        if v.get('live'):
            assert snapshot == self.grass_snapshot, 'Camera/preset/style regenerated grass'
        self.grass_snapshot = snapshot
        log.write('preloaded=%d stats=%r\\n' % (self.phase, prepared))
        log.flush()
        z = background.GetHeight(x, y)
        assert z > 0, 'B1/A1 camera must stand on actual terrain' ''')
    source = source.replace('        self.render_position = (x, y, z)', '''        if VIEWS[self.phase].get('motion'):
            z = background.GetHeight(x, y) + VIEWS[self.phase].get('elevation', 0)
        self.render_position = (x, y, z)''')
    source = source.replace('            self.shot = True', '''            current = systemSetting.TestVegetationStats()
            keys = ('grassPlacements', 'grassCells', 'grassTiles', 'grassExpectedTiles', 'grassBytes', 'grassSerial')
            assert tuple(current[key] for key in keys) == self.grass_snapshot, 'Rendering regenerated grass'
            self.shot = True''')
    source = source.replace('background.Destroy()\nwindow.label.Hide()', '''background.Destroy()
assert systemSetting.TestVegetationStats()['grassPlacements'] == 0, 'Map grass not released'
log.write('grass-unloaded=0\\n')
window.label.Hide()''')
wrapped = 'import builtins, traceback\ntry:\n' + '\n'.join('    ' + line for line in source.splitlines()) + '''
except Exception:
    with builtins.old_open('g8-visual-failure.log', 'w') as stream:
        stream.write(traceback.format_exc())
    import app
    app.Exit()
'''
compile(wrapped, '<h2-native>', 'exec')
(runtime / 'test-root/root/prototype.py').write_text(wrapped, encoding='utf-8')
(runtime / 'views.json').write_text(json.dumps(views, indent=2), encoding='utf-8')
print(mode, len(views), 'views; optional registry override:', modern)
