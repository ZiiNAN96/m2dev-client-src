"""Fixed original-map water views in an isolated prepare_runtime.py root.

Only locally installed assets. Sequence and cameras are part of the evidence.
Usage: prepare_g7_visual.py RUNTIME [comma-separated view indices]
"""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
runtime = Path(sys.argv[1])
sequence = tuple(map(int, sys.argv[2].split(','))) if len(sys.argv) > 2 else tuple(range(18))
assert sequence and all(0 <= value < 23 for value in sequence)
source = (root / 'tests/AnimationRuntime/runtime_entry.py').read_text()
source = source.replace('chrmgr.CreateRace(0)', '').replace('chrmgr.SelectRace(0)', '')
source = source.replace('chrmgr.LoadLocalRaceData("msm/warrior_m.msm")', '')
source = source.replace('playersettingmodule.__LoadGameWarriorEx(0, "d:/ymir work/pc/warrior/")',
    'for phase in ("INIT", "WARRIOR", "ASSASSIN", "SURA", "SHAMAN"): playersettingmodule.LoadGameData(phase)')
source = source.replace('SCENES = (("a1", 44000, 27200, 0),)', '''SCENES = (("b1", 68900, 53200, 0),) * 10 + (("a1", 44000, 27200, 0),) + (("b1", 68900, 53200, 0),) * 7 + (("b1", 68900, 54000, 0),) * 5
VIEWS = ((5000, 55, 40),) * 4 + ((3500, 23, 110),) * 5 + ((2500, 65, 40), (4000, 40, 0), (5000, 55, 40)) + ((3500, 20, -70),) * 6 + ((3000, 12, -70),) * 5
QUALITIES = (0, 1, 2, 3, 2, 2, 2, 1, 1, 3, 2, 2, 1, 2, 3, 2, 3, 3, 1, 2, 3, 2, 3)
SUNS = (-1, -1, -1, -1, 0, 1, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1)
NAMES = ("low", "medium", "high", "ultra", "morning", "noon", "evening", "bloom-off", "bloom-on", "shore", "map-a1", "map-b1-return", "reflection-off", "reflection-high", "reflection-ultra", "actor-motion", "ssr-radiance", "ssr-confidence", "near-water-off", "near-water-high", "near-water-ultra", "near-water-walk", "near-water-confidence")
SEQUENCE = %r''' % (sequence,))
source = source.replace('phase = int(elapsed / 60)', 'clock_phase = int(elapsed / 8)\n        phase = SEQUENCE[min(clock_phase, len(SEQUENCE)-1)]')
source = source.replace('if phase >= len(SCENES):', 'if clock_phase >= len(SEQUENCE):')
source = source.replace('elapsed - phase * 60', 'elapsed - clock_phase * 8')
source = source.replace('motion_step = min(3, int(seconds / 15))', 'motion_step = int(phase in (15, 21))')
source = source.replace('px = x + index * 250\n                chr.SetPixelPosition(int(px), int(y), int(background.GetHeight(px, y)))', '''px = x + index * 250
                py = y if phase < 18 else 53200
                if phase >= 18 and index == 0:
                    px, py = 69750, 53450
                chr.SetPixelPosition(int(px), int(py), int(background.GetHeight(px, py)))''')
source = source.replace('step = min(2, int(seconds / 20))', 'step = 0')
source = source.replace('self.camera = (6500, 35, 0) if step == 1 else (1000, 20, 0)', '''self.camera = VIEWS[phase]
        systemSetting.ApplyGraphicsSettings({"style": 1, "water": QUALITIES[phase], "bloom": int(phase == 8), "modernSky": 1, "shadows": 4, "ambientOcclusion": 1, "vegetation": 2, "viewDistance": 25600})
        if hasattr(systemSetting, "TestGraphicsSun"):
            systemSetting.TestGraphicsSun(SUNS[phase])
        if hasattr(systemSetting, "TestWaterTime"):
            systemSetting.TestWaterTime(10.0, 2 if phase in (17, 22) else 1 if phase == 16 else 0)
        if phase == 11 and self.frames % 120 == 0:
            log.write("window-check=%s\\n" % systemSetting.TestGraphicsWindow())
            log.flush()''')
source = source.replace('shot = "far" if step == 1 and seconds >= 27 else "near" if step == 2 and seconds >= 52 else None',
    'shot = NAMES[phase] if seconds >= 6 else None')
source = source.replace('"f1x-world-%d-%s-"', '"g7-view-%d-%s-"')
source = source.replace('"F1-X %s: player / NPC / mob / boss / mount; hair + armor + weapon" % name',
    '"G7 %s: %s" % (name, NAMES[phase])')
source = source.replace('ZiiNAN Asset Runtime F1-X', 'G7 fixed water proof')
source = source.replace('window = World()', 'app.StartSkinningBenchmark()\nwindow = World()')
source = source.replace('        motion_step = int(phase in (15, 21))', '        app.SkinningBenchmarkStage(phase, int(seconds >= 3))\n        motion_step = int(phase in (15, 21))')
wrapped = 'import builtins, traceback\ntry:\n' + '\n'.join('    ' + line for line in source.splitlines()) + '''
except Exception:
    with builtins.old_open('g7-visual-failure.log', 'w') as stream:
        stream.write(traceback.format_exc())
    import app
    app.Exit()
'''
(runtime / 'test-root/root/prototype.py').write_text(wrapped, encoding='utf-8')
print(runtime)
