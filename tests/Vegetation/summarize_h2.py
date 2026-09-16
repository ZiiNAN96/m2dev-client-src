"""Summarize bounded H2 native/GPU evidence; no profiler or runtime dependency."""
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
    def percentile(p):
        index = (len(values) - 1) * p
        low = int(index)
        return values[low] + (values[min(low + 1, len(values) - 1)] - values[low]) * (index - low)
    return dict(count=len(values), average=statistics.mean(values), median=statistics.median(values),
                p95=percentile(.95), p99=percentile(.99), maximum=max(values))


def performance(root):
    method = ('1024 x 768, fixed B1 camera and six actors; first 180 sample=1 frames per preset after two seconds of warmup. '
              'No outliers removed. Capture at six seconds is outside this window. CPU is elapsed frame work excluding '
              'Present and the existing limiter, not OS CPU accounting; GPU uses the existing asynchronous frame query. '
              'Runs are sequential without a concurrent build or GPU test. One bounded run per build/content combination; '
              'not a statistical benchmark or a 60/120-FPS claim. G8 and H2 use the same moving sky and fixed water time. '
              'A preexisting Metin2 process remained running unchanged; this is not a fully isolated machine benchmark. '
              'Hardware inventory is in evidence/hardware-*.json; it does not identify the adapter selected by Diligent.')
    result = dict(method=method, runs={})
    memory_rows = list(csv.DictReader((root / 'evidence/vram-samples.csv').open(encoding='utf-8-sig')))
    for version in ('baseline', 'legacy', 'performance'):
        directory = root / ('runtime-' + version)
        rows = list(csv.DictReader((directory / 'skinning-benchmark.csv').open()))
        run = dict(sha256=hashlib.sha256((directory / 'Metin2_Release.exe').read_bytes()).hexdigest(), presets={})
        for stage, name in enumerate(('Low', 'Medium', 'High', 'Ultra')):
            warm = [r for r in rows if r['stage'] == str(stage) and r['sample'] == '1'][:180]
            if len(warm) != 180:
                raise ValueError('Incomplete equal warm frame window: ' + str(directory))
            measured = {}
            for field in ('process_cpu_us', 'gpu_frame_us', 'present_us', 'wall_frame_us', 'draws'):
                values = [float(r[field]) / (1 if field == 'draws' else 1000) for r in warm if r[field]]
                measured[field.replace('_us', '_ms')] = stats(values)
            measured['cold_cpu_max_ms'] = max(float(r['process_cpu_us']) / 1000 for r in rows if r['stage'] == str(stage) and r['sample'] == '0')
            memory = [r for r in memory_rows if r['runtime'] == 'runtime-' + version and r['view'] == str(stage)
                      and 2 <= float(r['secondsObservedInView'].replace(',', '.')) <= 4.5 and float(r['dedicatedBytes']) > 0]
            if len(memory) < 2:
                raise ValueError('Insufficient warm GPU memory samples: %s %s' % (version, name))
            for field in ('dedicatedBytes', 'sharedBytes', 'committedBytes'):
                measured[field.replace('Bytes', 'MiB')] = stats([float(r[field]) / 1048576 for r in memory])
            run['presets'][name] = measured
        run['vegetation_samples'] = re.findall(r'^vegetation=.*$', (directory / 'g8-visual.log').read_text(), re.M)
        result['runs'][version] = run
    output = root / 'performance'
    output.mkdir(exist_ok=True)
    (output / 'comparison.json').write_text(json.dumps(result, indent=2) + '\n')
    lines = ['# H2 bounded performance sanity', '', method, '',
             'baseline = committed G8; legacy = H2 with converted trees only; performance = H2 with one modern tree override and mask grass.', '',
             '| Build / content | Preset | CPU avg / median / P95 / P99 ms | GPU avg / median / P95 / P99 ms |',
             '| --- | --- | --- | --- |']
    for version, run in result['runs'].items():
        for name, measured in run['presets'].items():
            cells = [' / '.join('%.3f' % measured[metric][key] for key in ('average', 'median', 'p95', 'p99'))
                     for metric in ('process_cpu_ms', 'gpu_frame_ms')]
            lines.append('| %s | %s | %s | %s |' % (version, name, *cells))
    lines += ['', '## Windows GPU process memory', '',
              'Sampled Windows GPU Process Memory counters, summed across adapters for the exact launcher PID. '
              'Samples use seconds 2 to 4.5 after the observer first sees each view, before capture and teardown. DedicatedUsage is GPU dedicated '
              'memory charged to the whole client; SharedUsage is system memory shared with the GPU. '
              'These are sampled process values, not isolated per-asset residency or the physical card capacity. '
              'The same lightweight observer ran during all three comparison runs.', '',
              '| Build / content | Preset | Samples | Dedicated median / peak MiB | Shared median / peak MiB |',
              '| --- | --- | ---: | --- | --- |']
    for version, run in result['runs'].items():
        for name, measured in run['presets'].items():
            dedicated, shared = measured['dedicatedMiB'], measured['sharedMiB']
            lines.append('| %s | %s | %d | %.2f / %.2f | %.2f / %.2f |' %
                         (version, name, dedicated['count'], dedicated['median'], dedicated['maximum'], shared['median'], shared['maximum']))
    lines += ['', '## Isolated vegetation color submission', '',
              'Real D3D11 duration queries around DrawBatch. These exclude shadows, composite, Present and readback. '
              'Cold shader setup is retained in the raw CSV and is not a steady-state cost. '
              'Visible/draw/triangle counters refer to this color pass, not an entire native frame.', '',
              '| Case | Submitted / visible / culled | Draws | Triangles | Uploads | Instance bytes | CPU ms | GPU color ms |',
              '| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |']
    gpu = list(csv.DictReader((root / 'msvc/h2-evidence/Release/performance.csv').open()))
    for row in gpu:
        if any(key in row['case'] for key in ('repeat', 'dense', 'behind-camera')):
            lines.append('| {case} | {instances} / {visible} / {culled} | {draws} | {triangles} | {uploads} | {instanceBytes} | {cpuMs} | {gpuColorMs} |'.format(**row))
    lines += ['', 'Instance bytes are allocated buffer capacity and can exceed the current visible set. '
              'Process VRAM is measured above; it is not attributed to individual vegetation assets. '
              'Geometry/material/texture sharing and zero shutdown resources are asserted separately.']
    (output / 'comparison.md').write_text('\n'.join(lines) + '\n')


def gallery(root):
    from PIL import Image, ImageDraw
    output = root / 'gallery'
    output.mkdir(exist_ok=True)
    cards = []
    contact = []
    def card(label, path, detail):
        relative = '../' + path.relative_to(root).as_posix()
        cards.append('<article><h2>%s</h2><p>%s</p><a href="%s"><img loading="lazy" src="%s" alt="%s"></a></article>' %
                     (html.escape(label), html.escape(detail), relative, relative, html.escape(label)))
        contact.append((label, path))
    directory = root / 'runtime-native-final'
    for index, view in enumerate(json.loads((directory / 'views.json').read_text())):
        images = list(directory.glob('g8-%02d-%s-*.jpg' % (index, view['label'])))
        if len(images) != 1:
            raise ValueError('Missing or ambiguous native capture: ' + view['label'])
        card(view['label'], images[0], '%s; preset %s; camera %s' % (view['map'], view['preset'], view['camera']))
    for source in sorted((root / 'msvc/h2-evidence/Release').glob('*.ppm')):
        destination = output / (source.stem + '.png')
        with Image.open(source) as im:
            im.save(destination)
        card(source.stem, destination, 'Controlled asset proof, real Diligent renderer. Plain background; no map placement implied.')
    for version in ('baseline', 'legacy', 'performance'):
        directory = root / ('runtime-' + version)
        for index, view in enumerate(json.loads((directory / 'views.json').read_text())):
            images = list(directory.glob('g8-%02d-%s-*.jpg' % (index, view['label'])))
            if len(images) != 1:
                raise ValueError('Missing comparison capture: ' + str(directory))
            card(version + ' ' + view['label'], images[0], 'Same fixed B1 camera, bounded performance run.')
    header = '''<!doctype html><html lang="de"><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>H2 Vegetation 2.0</title><style>body{background:#111a20;color:#eef3ed;font:16px/1.5 system-ui;margin:24px auto;max-width:1500px;padding:0 24px}main{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:28px}img{width:100%;height:auto}p{color:#b6c6ba}a{color:#9ad3ad}h2{font-size:20px}@media(max-width:800px){main{grid-template-columns:1fr}}</style>
<h1>H2 · Vegetation 2.0</h1><p>Unveränderte Client-Aufnahmen und kontrollierte GPU-Aufnahmen. Originale Demonstrationsassets: Buche, Gras, Busch. Die finale Nutzerabnahme steht aus.</p>
<p><a href="../performance/comparison.md">Performance</a> · <a href="../content/content-priority.md">Content-Priorität</a> · <a href="../../docs/vegetation/phase-h2x-vegetation-2.md">Bericht</a></p><main>'''
    (output / 'index.html').write_text(header + ''.join(cards) + '</main></html>', encoding='utf-8')
    # Pages stay legible and are used for visual QA; originals remain untouched.
    for start in range(0, len(contact), 12):
        page = Image.new('RGB', (1200, 4 * 328), '#111a20')
        draw = ImageDraw.Draw(page)
        for cell, (label, path) in enumerate(contact[start:start + 12]):
            x, y = (cell % 3) * 400, (cell // 3) * 328
            with Image.open(path) as im:
                im.thumbnail((396, 297))
                page.paste(im, (x, y + 26))
            draw.text((x + 4, y + 4), label, fill='white')
        page.save(output / ('contact-%02d.jpg' % (start // 12)), quality=90)
    print(output / 'index.html')


if __name__ == '__main__':
    root = Path(sys.argv[1])
    performance(root)
    gallery(root)
