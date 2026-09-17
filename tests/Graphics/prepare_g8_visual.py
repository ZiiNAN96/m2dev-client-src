"""Prepare reproducible native map/preset captures in a private graphics runtime.

Usage: prepare_g8_visual.py RUNTIME baseline|final|integration|performance|effects
Use prepare_runtime.ps1 -Manual first, then rebuild only the private root.pck.
Only installed game data is used; no production asset or configuration is edited.
"""
import json
from pathlib import Path
import sys

runtime, mode = Path(sys.argv[1]), sys.argv[2]
maps = {
    'a1': ('a1', 63500, 59000, (4500, 15, 0)),
    'b1-water': ('b1', 68900, 53200, (5000, 55, 40)),
    'forest': ('trent', 15000, 15000, (4000, 25, 0)),
    'rock': ('n_flame_01', 50000, 50000, (4500, 25, 0)),
    'snow': ('map_n_snowm_01', 30000, 30000, (4000, 25, 0)),
}

def view(label, place='a1', style=1, preset=2, **extra):
    name, x, y, camera = maps[place]
    return dict(label=label, map=name, x=x, y=y, camera=camera,
                style=style, preset=preset, sun=-1, armour=11299, **extra)

if mode == 'integration':
    # Verified water grid layer 1: z=19018, terrain z=18851.5. Keep the
    # camera above water and frame the actual lake, not its dry north bank.
    views = []
    for repeat in range(2):
        for preset, quality in enumerate(('low', 'medium', 'high', 'ultra')):
            item = view('lake-%s-%d' % (quality, repeat), 'b1-water', preset=preset)
            item.update(x=71500, y=55000, camera=(5000, 18, -70), elevation=400)
            views.append(item)
        item = dict(views[-2])
        item.update(label='lake-custom-%d' % repeat, custom=True)
        views.append(item)
        item = dict(views[repeat * 6])
        item.update(label='lake-low-return-%d' % repeat)
        views.append(item)
    for style in (0, 1):
        item = view('coast-' + ('classic', 'modern')[style], 'b1-water', style=style)
        item.update(x=18000, y=80000, camera=(6000, 15, 90), elevation=500)
        views.append(item)
    item = view('landscape-sky')
    item.update(camera=(8000, -5, 0), elevation=4000)
    views.append(item)
elif mode == 'effects':
    views = []
    for place in ('a1', 'b1-water', 'snow'):
        for style in (0, 1):
            item = view(place + '-effects-' + ('classic', 'modern')[style], place, style=style)
            item.update(camera=(1600, 22, 0), effects=True, snow=place == 'snow')
            if place == 'b1-water':
                item.update(x=71500, y=55000, elevation=400)
            views.append(item)
elif mode == 'performance':
    views = [view('b1-' + quality, 'b1-water', preset=preset)
             for preset, quality in enumerate(('low', 'medium', 'high', 'ultra'))]
    # Keep the frozen G7 performance rig byte-for-byte in both runs.
    for item in views:
        item.update(x=68900, y=54000, camera=(3500, 20, -70), animate_sky=True)
elif mode in ('baseline', 'final'):
    views = []
    for place in maps:
        views += [view(place + '-classic', place, style=0), view(place + '-modern', place)]
    views += [view('b1-' + quality, 'b1-water', preset=preset)
              for preset, quality in enumerate(('low', 'medium', 'high', 'ultra'))]
    views += [view('b1-custom', 'b1-water', custom=True), view('b1-low-return', 'b1-water', preset=0)]
    for state, label in enumerate(('morning', 'noon', 'evening')):
        item = view('sky-' + label)
        item.update(sun=state, camera=(4000, -75 if state == 1 else -15, 129 if state == 1 else 97 if state == 0 else -97), elevation=5000)
        views.append(item)
    for style in (0, 1):
        for armour in (11299, 12019):
            item = view('armour-%d-%s' % (armour, ('classic', 'modern')[style]), style=style)
            item.update(armour=armour, camera=(850, 18, 0))
            views.append(item)
    views += [view('a1-return'), view('ultra-window', 'b1-water', preset=3, window=True),
              view('high-window', 'b1-water', window=True)]
else:
    raise ValueError(mode)

source = Path(__file__).with_name('g8_runtime_entry.py').read_text(encoding='utf-8')
source = source.replace('VIEWS = []', 'VIEWS = ' + repr(views))
wrapped = 'import builtins, traceback\ntry:\n' + '\n'.join('    ' + line for line in source.splitlines()) + '''
except Exception:
    with builtins.old_open('g8-visual-failure.log', 'w') as stream:
        stream.write(traceback.format_exc())
    import app
    app.Exit()
'''
compile(wrapped, '<g8-runtime>', 'exec')
(runtime / 'test-root/root/prototype.py').write_text(wrapped, encoding='utf-8')
(runtime / 'views.json').write_text(json.dumps(views, indent=2), encoding='utf-8')
print('%s: %d views' % (mode, len(views)))
