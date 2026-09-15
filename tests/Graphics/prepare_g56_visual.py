"""Install deterministic G56 camera/sun views into an already isolated test root.

Usage: prepare_runtime.ps1 -Manual first, then this script RUNTIME, then repack
RUNTIME/test-root/root with PackMaker. Original local assets only; no downloads.
The saved pre-G56 client supports the same first four cameras. Sun states are
selected only when the development probe exists in the tested binary.
"""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
runtime = Path(sys.argv[1])
source = (root / 'tests/AnimationRuntime/runtime_entry.py').read_text()
source = source.replace('chrmgr.CreateRace(0)', '').replace('chrmgr.SelectRace(0)', '')
source = source.replace('chrmgr.LoadLocalRaceData("msm/warrior_m.msm")', '')
source = source.replace('playersettingmodule.__LoadGameWarriorEx(0, "d:/ymir work/pc/warrior/")',
    'for phase in ("INIT", "WARRIOR", "ASSASSIN", "SURA", "SHAMAN"): playersettingmodule.LoadGameData(phase)')
source = source.replace('SCENES = (("a1", 44000, 27200, 0),)', '''SCENES = (("b1", 64000, 55300, 0), ("b1", 64000, 55300, 0),
          ("a1", 44000, 27200, 0), ("b1", 68900, 53200, 0),
          ("a1", 44000, 27200, 0), ("a1", 44000, 27200, 0), ("a1", 44000, 27200, 0))
VIEWS = ((1800, 22, 0), (4000, 18, 0), (6500, 35, 0), (3500, 20, 0),
         (4000, -15, 97), (4000, -75, 129), (4000, -15, -97))
NAMES = ("player-wolf", "building", "terrain", "vegetation", "morning", "noon", "evening")''')
source = source.replace('int(elapsed / 60)', 'int(elapsed / 8)').replace('elapsed - phase * 60', 'elapsed - phase * 8')
source = source.replace('motion_step = min(3, int(seconds / 15))', 'motion_step = 0')
source = source.replace('step = min(2, int(seconds / 20))', 'step = 0')
source = source.replace('self.camera = (6500, 35, 0) if step == 1 else (1000, 20, 0)',
    '''self.camera = VIEWS[phase]
        if hasattr(systemSetting, "TestGraphicsSun"):
            systemSetting.TestGraphicsSun(phase - 4 if phase >= 4 else -1)
        if phase == 0:
            x += 250
        if phase == 3:
            z += 500
        if phase >= 4:
            z += 5000 # Keep an upward-looking camera above terrain.
        self.position = (x, y, z)''')
source = source.replace('shot = "far" if step == 1 and seconds >= 27 else "near" if step == 2 and seconds >= 52 else None',
    'shot = NAMES[phase] if seconds >= 6 else None')
source = source.replace('"f1x-world-%d-%s-"', '"g56-view-%d-%s-"')
source = source.replace('ZiiNAN Asset Runtime F1-X', 'G56 fixed visual proof')
if len(sys.argv) > 2:
    sequence = tuple(int(value) for value in sys.argv[2].split(','))
    assert sequence and all(0 <= value < 7 for value in sequence)
    source = source.replace('phase = int(elapsed / 8)', 'clock_phase = int(elapsed / 8)\n        phase = clock_phase')
    source = source.replace('if phase >= len(SCENES):', 'if phase >= %d:' % len(sequence))
    source = source.replace('name, x, y, mount = SCENES[phase]', 'phase = %r[clock_phase]\n        name, x, y, mount = SCENES[phase]' % (sequence,))
    source = source.replace('elapsed - phase * 8', 'elapsed - clock_phase * 8')
indented = '\n'.join('    ' + line for line in source.splitlines())
wrapped = 'import builtins, traceback\ntry:\n' + indented + '''
except Exception:
    with builtins.old_open('g56-visual-failure.log', 'w') as stream:
        stream.write(traceback.format_exc())
    import app
    app.Exit()
'''
(runtime / 'test-root/root/prototype.py').write_text(wrapped, encoding='utf-8')
print(runtime)
