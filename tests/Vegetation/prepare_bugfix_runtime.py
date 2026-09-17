"""Repeatable A1 movement probe in an isolated runtime; never touches production packs."""
import json
from pathlib import Path
import sys

runtime = Path(sys.argv[1])
duration = int(sys.argv[2])
source = Path('tests/Graphics/g8_runtime_entry.py').read_text(encoding='utf-8')
source = source.replace('import app, background', 'import math, json\nimport app, background')
source = source.replace('VIEWS = []', "VIEWS = [dict(map='a1', x=44000, y=25600, camera=(5500, 22, 0), style=1, preset=2, sun=-1, armour=11299, label='stable-world')]")
source = source.replace("'G8 final visual integration'", "'BUGFIX-X A1 movement test'")
source = source.replace("'g8-visual.log'", "'bugfix-route.log'")
source = source.replace('        self.next_view()', '        self.next_sample = 0\n        self.next_capture = 0\n        self.next_view()', 1)
source = source.replace('        if elapsed >= 7 and self.shot:', f'        if elapsed >= {duration}:')
source = source.replace('        x, y, z = self.position\n        app.SetCenterPosition', '''        # Cross the 25,600 cm sector boundary repeatedly, at walking speed.
        t = max(0.0, elapsed - 5.0)
        x = 44000 + 1600 * math.sin(t * math.pi / 12)
        y = 25600 + 1000 * math.sin(t * math.pi / 18)
        # The first 45 seconds match the baseline. Then visit the town without
        # reloading the map and repeat the camera/LOD sweep among buildings/trees.
        if elapsed >= 45:
            x = 63500 + 2400 * math.sin((elapsed-45) * math.pi / 18)
            y = 59000 + 1600 * math.sin((elapsed-45) * math.pi / 24)
        z = background.GetHeight(x, y)
        self.position = (x, y, z)
        self.camera = (5500 + 1200 * math.sin(t * math.pi / 15), 22, (t * 8) % 360)
        chr.SelectInstance(57900)
        chr.SetPixelPosition(int(x), int(y), int(z))
        chr.SetLoopMotion(chr.MOTION_RUN)
        if elapsed >= self.next_sample:
            world = systemSetting.TestWorldResidency()
            vegetation = systemSetting.TestVegetationStats()
            if BUGFIX_DURATION > 45 and elapsed >= 5:
                assert world['terrainResident'] == 20 and world['areasResident'] == 20, 'A1 must remain fully resident'
                assert world['treeBlockerDraws'] == 0, 'Trees reached transparent camera blocker path'
                assert vegetation['grassPlacements'] == 0, 'Grass was reactivated'
                if hasattr(self, 'prepared_world'):
                    for key in ('terrainLoaded', 'terrainUnloaded', 'areasLoaded', 'areasUnloaded', 'terrainAssignments', 'treeCreated'):
                        assert world[key] == self.prepared_world[key], 'World rebuilt during motion: ' + key
                self.prepared_world = world
            log.write('sample=' + json.dumps(dict(seconds=elapsed, position=self.position, camera=self.camera,
                world=world, vegetation=vegetation)) + '\\n')
            log.flush()
            self.next_sample = int(elapsed) + 1
        app.SetCenterPosition''')
source = source.replace('        if time.monotonic() - self.started >= 6 and not self.shot:',
                        '        if time.monotonic() - self.started >= self.next_capture:')
source = source.replace('            self.shot = True', '            self.shot = True\n            self.next_capture += 30')
source = source.replace("log.write('completed=", "log.write('shutdown=' + json.dumps(systemSetting.TestWorldResidency()) + '\\n')\nlog.write('completed=")
source = source.replace('BUGFIX_DURATION', str(duration))
wrapped = 'import builtins, traceback\ntry:\n' + '\n'.join('    ' + line for line in source.splitlines()) + '''
except Exception:
    with builtins.old_open('bugfix-failure.log', 'w') as stream:
        stream.write(traceback.format_exc())
    import app
    app.Exit()
'''
compile(wrapped, '<bugfix-native>', 'exec')
(runtime / 'test-root/root/prototype.py').write_text(wrapped, encoding='utf-8')
(runtime / 'route.json').write_text(json.dumps(dict(map='a1', duration=duration, warmup=5, sectorBoundary=25600), indent=2))
print(runtime)
