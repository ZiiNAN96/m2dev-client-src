"""Bounded A1/Trent comparison with identical cameras and original packed assets."""
from pathlib import Path
import json
import sys

runtime = Path(sys.argv[1])
duration = int(sys.argv[2])  # Per map: 45s reference, 75s fixed-high validation.
source = Path('tests/Graphics/g8_runtime_entry.py').read_text(encoding='utf-8')
source = source.replace('import app, background', 'import math, json\nimport app, background')
views = [dict(map=name, x=x, y=y, elevation=900, camera=(16500, 15, 0),
              style=1, preset=2, sun=-1, armour=11299, label=name)
         for name, x, y in [('a1', 70248, 54328), ('trent', 22000, 22000)]]
source = source.replace('VIEWS = []', 'VIEWS = ' + repr(views))
source = source.replace("'G8 final visual integration'", "'Tree fixed-detail comparison'")
source = source.replace("'g8-visual.log'", "'bugfix-route.log'")
source = source.replace('        self.shot = self.window_checked = False', '''        self.shot = self.window_checked = False
        self.next_sample = 0
        self.capture_index = 0
        self.prepared_world = None''')
source = source.replace('        if elapsed >= 7 and self.shot:', f'        if elapsed >= {duration}:')
source = source.replace('        x, y, z = self.position\n        app.SetCenterPosition', '''        town = self.phase == 0
        targets = ((70248, 54328, 900, 16500, 4800),
                   (66013, 50727, 820, 15000, 4500),
                   (69437, 53152, 1100, 24000, 3000)) if town else (
                   (18000, 18000, 1000, 18000, 3000),
                   (26000, 22000, 1000, 18000, 3000),
                   (15000, 31000, 1000, 18000, 3000))
        segment = int(elapsed // 15) % 3
        local = elapsed % 15
        x, y, elevation, far, near = targets[segment]
        fraction = max(0, min(1, (local - 3) / 10))
        angle = 0
        if 45 <= elapsed < 60:
            fraction = .5 + .48 * math.sin((elapsed - 45) * math.pi / 4)
            angle = (elapsed - 45) * 24
        if elapsed >= 60:
            x, y, elevation, far, near = targets[0]
            fraction, angle = .45, 0
        z = background.GetHeight(x, y)
        self.position = (x, y, z + elevation)
        self.camera = (far + (near - far) * fraction, 15 if town else 25, angle)
        self.label.SetText('%s tree detail / distance %.0f' % (VIEWS[self.phase]['map'], self.camera[0]))
        chr.SelectInstance(57900)
        chr.SetPixelPosition(int(x), int(y), int(z))
        systemSetting.TestVegetationTime(10.0)
        if elapsed >= self.next_sample:
            world = systemSetting.TestWorldResidency()
            vegetation = systemSetting.TestVegetationStats()
            if elapsed >= 5:
                assert world['terrainResident'] == world['areasResident'] == (20 if town else 4)
                assert world['treeBlockerDraws'] == 0 and vegetation['grassPlacements'] == 0
                assert world['terrainVisible'] == world['terrainDrawn']
                if self.prepared_world:
                    for key in ('terrainLoaded', 'terrainUnloaded', 'areasLoaded', 'areasUnloaded', 'terrainAssignments', 'treeCreated'):
                        assert world[key] == self.prepared_world[key], 'World rebuild: ' + key
                self.prepared_world = world
            log.write('sample=' + json.dumps(dict(map=VIEWS[self.phase]['map'], seconds=elapsed,
                frame=self.frames, position=self.position, camera=self.camera,
                world=world, vegetation=vegetation)) + '\\n')
            log.flush()
            self.next_sample = int(elapsed) + 1
        x, y, z = self.position
        app.SetCenterPosition''')
source = source.replace('        if time.monotonic() - self.started >= 6 and not self.shot:',
                        '        if self.capture_index < 3 and time.monotonic() - self.started >= (8, 22, 37)[self.capture_index]:')
source = source.replace("            label = VIEWS[self.phase]['label']", "            label = VIEWS[self.phase]['label'] + '-%d' % self.capture_index")
source = source.replace('            self.shot = True', '            self.shot = True\n            self.capture_index += 1')
source = source.replace("log.write('completed=", "log.write('shutdown=' + json.dumps(systemSetting.TestWorldResidency()) + '\\n')\nlog.write('completed=")
wrapped = 'import builtins, traceback\ntry:\n' + '\n'.join('    ' + line for line in source.splitlines()) + '''
except Exception:
    with builtins.old_open('bugfix-failure.log', 'w') as stream:
        stream.write(traceback.format_exc())
    import app
    app.Exit()
'''
compile(wrapped, '<fixed-tree-detail>', 'exec')
(runtime / 'test-root/root/prototype.py').write_text(wrapped, encoding='utf-8')
(runtime / 'route.json').write_text(json.dumps(dict(maps=['a1', 'trent'], seconds_per_map=duration,
    warmup=5, comparison_end=44, route='approach three locations, then reverse/rotate and stationary tail'), indent=2))
print(runtime)
