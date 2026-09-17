"""A1 crown/LOD regression route using production assets in a private runtime."""
from pathlib import Path
import json
import sys

runtime = Path(sys.argv[1])
duration = int(sys.argv[2])
source = Path('tests/Graphics/g8_runtime_entry.py').read_text(encoding='utf-8')
source = source.replace('import app, background', 'import math, json\nimport app, background')
source = source.replace('VIEWS = []', "VIEWS = [dict(map='a1', x=70248, y=54328, elevation=900, camera=(16500, 15, 0), style=1, preset=2, sun=-1, armour=11299, label='lod-followup')]")
source = source.replace("'G8 final visual integration'", "'BUGFIX-X tree LOD follow-up'")
source = source.replace("'g8-visual.log'", "'bugfix-route.log'")
source = source.replace('        self.next_view()', '        self.next_sample = 0\n        self.next_capture = 0\n        self.capture_index = 0\n        self.next_view()', 1)
source = source.replace('        if elapsed >= 7 and self.shot:', f'        if elapsed >= {duration}:')
source = source.replace('        x, y, z = self.position\n        app.SetCenterPosition', '''        # Three actual A1 town trees: converted beech/cypress and authored override.
        targets = ((70248, 54328, 900, 16500, 4800),
                   (66013, 50727, 820, 15000, 4500),
                   (69437, 53152, 1100, 24000, 3000))
        segment = int(elapsed // 15) % 3
        local = elapsed % 15
        x, y, elevation, far, near = targets[segment]
        fraction = max(0, min(1, (local - 3) / 10))
        if 45 <= elapsed < 120:
            fraction = .5 + .48 * math.sin((elapsed - 45) * math.pi / 9)
        angle = 0 if elapsed < 45 else ((elapsed - 45) * 8) % 360
        if elapsed >= 120:
            x, y, elevation, far, near = targets[0]
            fraction, angle = .45, 0
        z = background.GetHeight(x, y)
        self.position = (x, y, z + elevation)
        self.camera = (far + (near - far) * fraction, 15, angle)
        self.label.SetText('Tree LOD: %d / distance %.0f' % (segment, self.camera[0]))
        chr.SelectInstance(57900)
        chr.SetPixelPosition(int(x), int(y), int(z))
        systemSetting.TestVegetationTime(10.0)
        if elapsed >= self.next_sample:
            world = systemSetting.TestWorldResidency()
            vegetation = systemSetting.TestVegetationStats()
            if elapsed >= 5:
                assert world['terrainResident'] == 20 and world['areasResident'] == 20
                assert world['treeBlockerDraws'] == 0 and vegetation['grassPlacements'] == 0
                assert world['terrainVisible'] == world['terrainDrawn']
                if hasattr(self, 'prepared_world'):
                    for key in ('terrainLoaded', 'terrainUnloaded', 'areasLoaded', 'areasUnloaded', 'terrainAssignments', 'treeCreated'):
                        assert world[key] == self.prepared_world[key], 'World rebuild: ' + key
                self.prepared_world = world
            log.write('sample=' + json.dumps(dict(seconds=elapsed, frame=self.frames,
                position=self.position, camera=self.camera, world=world, vegetation=vegetation)) + '\\n')
            log.flush()
            self.next_sample = int(elapsed) + 1
        x, y, z = self.position
        app.SetCenterPosition''')
source = source.replace('        if time.monotonic() - self.started >= 6 and not self.shot:',
                        '        if time.monotonic() - self.started >= self.next_capture:')
source = source.replace("            label = VIEWS[self.phase]['label']", "            label = 'lod-%03d' % self.capture_index")
source = source.replace('            self.shot = True', '''            self.shot = True
            self.capture_index += 1
            self.next_capture += .5 if self.next_capture < 45 else 10''')
source = source.replace("log.write('completed=", "log.write('shutdown=' + json.dumps(systemSetting.TestWorldResidency()) + '\\n')\nlog.write('completed=")
wrapped = 'import builtins, traceback\ntry:\n' + '\n'.join('    ' + line for line in source.splitlines()) + '''
except Exception:
    with builtins.old_open('bugfix-failure.log', 'w') as stream:
        stream.write(traceback.format_exc())
    import app
    app.Exit()
'''
compile(wrapped, '<lod-followup>', 'exec')
(runtime / 'test-root/root/prototype.py').write_text(wrapped, encoding='utf-8')
(runtime / 'route.json').write_text(json.dumps(dict(map='a1', duration=duration, warmup=5,
    route='three town trees, approach and reverse, camera rotation, final stationary 30s'), indent=2))
print(runtime)
