"""Summarize existing native G8 captures/counters without adding a profiler.

Usage: summarize_g8.py build-g8x [--performance-only]
All generated files stay in the supplied ignored evidence directory.
"""
import csv
import hashlib
import html
import json
from pathlib import Path
import re
import statistics
import sys


def stats(values):
    values = sorted(values)
    def percentile(fraction):
        index = (len(values) - 1) * fraction
        low = int(index)
        return values[low] + (values[min(low + 1, len(values) - 1)] - values[low]) * (index - low)
    return dict(count=len(values), average=statistics.mean(values), median=statistics.median(values),
                p95=percentile(.95), p99=percentile(.99), maximum=max(values))


def performance(root):
    result = {'method': 'First 180 sample=1 frames per preset, starting after two seconds of warmup. '
              'The screenshot occurs after six seconds and is outside this equal frame window. '
              'No timing outliers removed. CPU is steady elapsed frame work excluding Present and frame limiter; '
              'it is not OS process CPU accounting. GPU is the existing asynchronous frame duration query. '
              'Draws are actor frontend draws, not all renderer passes. One bounded run per build; no confidence interval.',
              'runs': {}}
    for version in ('baseline', 'final'):
        directory = root / ('performance-baseline' if version == 'baseline' else 'performance-acceptance')
        rows = list(csv.DictReader((directory / 'skinning-benchmark.csv').open()))
        run = {'sha256': hashlib.sha256((directory / 'Metin2_Release.exe').read_bytes()).hexdigest(), 'presets': {}}
        for stage, name in enumerate(('Low', 'Medium', 'High', 'Ultra')):
            warm = [row for row in rows if row['stage'] == str(stage) and row['sample'] == '1'][:180]
            if len(warm) != 180:
                raise ValueError('Incomplete warm frame window: ' + str(directory))
            measured = {}
            for field in ('process_cpu_us', 'gpu_frame_us', 'present_us', 'wall_frame_us', 'draws'):
                values = [float(row[field]) / (1 if field == 'draws' else 1000) for row in warm if row[field]]
                measured[field.replace('_us', '_ms')] = stats(values)
            startup = [float(row['process_cpu_us']) / 1000 for row in rows
                       if row['stage'] == str(stage) and row['sample'] == '0']
            measured['warmup_cpu_max_ms'] = max(startup)
            run['presets'][name] = measured
        renderer = (directory / 'gdx-renderer.log').read_text().splitlines()[-1]
        run['whole_run_renderer_counters'] = {key: float(value) for key, value in
                                             re.findall(r'(\w+)=([0-9.]+)', renderer)}
        run['first_use_events'] = (directory / 'gdxc-first-use.log').read_text().splitlines()
        result['runs'][version] = run
    output = root / 'performance'
    output.mkdir(exist_ok=True)
    (output / 'baseline.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    lines = ['# G8 / P-X performance baseline', '', result['method'], '',
             '1024 x 768, D3D11, VSync and existing 16/17 ms limiter; six actors on the same B1 camera. '
             'G8 uses the production moving-cloud clock. G7 has no animated atmosphere clouds. '
             'Water time is fixed in both runs. GPU gates and capture series ran sequentially; no build ran during the measurements. '
             'An earlier private normal-client launch remained idle with no world-frame log and Windows denied cleanup access; '
             'this is a sanity measurement, not a fully isolated benchmark. Hardware inventory is in evidence/hardware-*.json; '
             'the existing counter does not identify the selected adapter.', '',
             '| Build | Preset | CPU avg / median / P95 / P99 ms | GPU avg / median / P95 / P99 ms | Actor draws avg |',
             '| --- | --- | --- | --- | ---: |']
    for version, run in result['runs'].items():
        for name, measured in run['presets'].items():
            cells = [' / '.join('%.3f' % measured[metric][key] for key in ('average', 'median', 'p95', 'p99'))
                     for metric in ('process_cpu_ms', 'gpu_frame_ms')]
            lines.append('| %s | %s | %s | %s | %.0f |' % (version, name, *cells, measured['draws']['average']))
    lines += ['', '## Whole-run CPU submission counters', '',
              'These include first-use initialization and preset changes; they are not isolated GPU pass timings or warm percentiles. '
              'Nested totals are not additive. GPU pass-specific timers do not exist in this harness.', '',
              '| Pass | G7 total ms | G8 total ms |', '| --- | ---: | ---: |']
    for key in ('shadowCpuSubmitMs', 'aoCpuSubmitMs', 'atmosphereCpuSubmitMs', 'bloomCpuSubmitMs',
                'toneMapCpuSubmitMs', 'compositeCpuSubmitMs', 'waterCpuSubmitMs', 'ssrCpuSubmitMs'):
        lines.append('| %s | %.3f | %.3f |' % (key, *(result['runs'][version]['whole_run_renderer_counters'][key]
                                                   for version in ('baseline', 'final'))))
    lines += ['', '## First use and resources', '',
              'The direct-map fixture bypasses network LoadingWindow, so its cold first-use frames include compilation. '
              'The existing normal-client loading prewarm invokes the same atmosphere constructor and shader. '
              'A hitch-free network relog is not established by this fixture.', '',
              '| Build | Low / Medium / High / Ultra warmup maximum CPU ms | Atmosphere target bytes | Frames |',
              '| --- | --- | ---: | ---: |']
    for version, run in result['runs'].items():
        counters = run['whole_run_renderer_counters']
        lines.append('| %s | %s | %d | %d |' % (version, ' / '.join('%.1f' % value['warmup_cpu_max_ms']
                     for value in run['presets'].values()), counters['atmosphereTargetBytes'], counters['frames']))
    lines += ['', 'Low gains a real 512 shadow cascade in G8, explaining 45 versus 28 actor draws. '
              'Medium/High/Ultra retain 56 actor draws. The larger sky atlas adds 786432 bytes (0.75 MiB). '
              'No gross regression is indicated by this bounded rig; it does not replace later P-X profiling.', '',
              'Full counters, shader/PSO creation events, binary hashes and all frame statistics are in baseline.json. '
              'Raw captures remain in performance-baseline and performance-acceptance.']
    (output / 'baseline.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print('\n'.join(lines[:15]))


def gallery(root):
    output = root / 'gallery'
    output.mkdir(exist_ok=True)
    sections = []
    for title, old, new in (('Maps, presets, sky and armour', 'runtime-baseline-v2', 'acceptance-main'),
                            ('Lake, coast and repeated live apply', 'integration-baseline', 'acceptance-integration'),
                            ('Original fire, magic, weapon trails and snow', 'effects-baseline-v2', 'acceptance-effects')):
        views = json.loads((root / new / 'views.json').read_text())
        cards = []
        for index, view in enumerate(views):
            images = []
            for label, directory in (('G7', old), ('G8 final', new)):
                matches = list((root / directory).glob('g8-%02d-%s-*.jpg' % (index, view['label'])))
                if not matches and old == 'effects-baseline-v2' and directory == old and view.get('snow'):
                    failure = (root / old / 'renderer-failure.log').read_text()
                    if 'non-finite effect vertex position' in failure:
                        images.append('<figure><figcaption>G7</figcaption><p class="notice">'
                                      'G7 brach beim Schnee mit einer ungültigen Partikelposition ab. '
                                      'Kein Vergleichsbild verfügbar; G8 initialisiert die Partikelbasis.</p></figure>')
                        continue
                if len(matches) != 1:
                    raise ValueError('Missing or ambiguous capture: ' + directory + ' ' + view['label'])
                relative = '../' + matches[0].relative_to(root).as_posix()
                images.append('<figure><figcaption>%s</figcaption><a href="%s"><img loading="lazy" src="%s" alt="%s"></a></figure>' %
                              (label, relative, relative, html.escape(view['label'] + ' ' + label)))
            details = 'Map %s | %s | camera %s | x/y %s/%s | preset %s' % (
                view['map'], ('Classic', 'Modern')[view['style']], view['camera'], view['x'], view['y'], view['preset'])
            cards.append('<article><h3>%02d · %s</h3><p>%s</p><div class="pair">%s</div></article>' %
                         (index, html.escape(view['label']), html.escape(details), ''.join(images)))
        sections.append('<section><h2>%s</h2>%s</section>' % (title, ''.join(cards)))
    document = '''<!doctype html><html lang="de"><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>G8 – finale Visual Gallery</title><style>
body{margin:0;background:#121820;color:#edf1f5;font:16px/1.5 system-ui}header,main{max-width:1600px;margin:auto;padding:24px}
h1,h2,h3{line-height:1.2}h2{margin-top:48px}p{color:#b9c6d2}article{margin:24px 0 48px}
.pair{display:grid;grid-template-columns:1fr 1fr;gap:12px}figure{margin:0}figcaption{padding:8px;background:#253242}
img{display:block;width:100%;height:auto}a{color:#85caff}nav{display:flex;gap:24px;flex-wrap:wrap}
@media(max-width:850px){.pair{grid-template-columns:1fr}}.notice{border-left:3px solid #e0b05e;padding:8px 18px}
</style><header><h1>G8 · Final Visual Integration</h1><p>47 feste Ansichten je Build. Originale Client-Aufnahmen, ohne Farbkorrektur.
Links G7, rechts G8. Classic/Modern-Paare stehen direkt untereinander. Bild anklicken für volle Auflösung.</p>
<p class="notice">Nutzerabnahme ausstehend. Kamera und Himmels-/Wasserzeit sind fest; Figuren, Partikel und Originalanimationen laufen weiter.
JPGs sind visuelle Evidenz. Classic-Bytegleichheit wird separat im Golden-Test geprüft.</p>
<nav><a href="../performance/baseline.md">Performance</a><a href="../texture-audit/texture-impact.md">Texture Impact</a>
<a href="../../docs/graphics/phase-g8x-final-visual-integration.md">Abschlussbericht</a></nav></header><main>'''
    (output / 'index.html').write_text(document + ''.join(sections) + '</main></html>', encoding='utf-8')
    print(output / 'index.html')


if __name__ == '__main__':
    root = Path(sys.argv[1])
    performance(root)
    if '--performance-only' not in sys.argv[2:]:
        gallery(root)
