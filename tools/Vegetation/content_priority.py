"""H2 content priority from effective pack-selected properties and terrain masks."""
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re
import sys
from texture_impact import legacy_strings, norm

source, corpus, output = map(Path, sys.argv[1:4])
output.mkdir(parents=True, exist_ok=True)
data = legacy_strings(json.loads(source.read_text(encoding='utf-8', errors='surrogateescape')))
files = {norm(item['path']): item for item in data['files']}
registry = json.loads((corpus / 'vegetation/registry.json').read_text())['entries']
properties = {}
for path, item in files.items():
    if not path.endswith(('.prt', '.prb')):
        continue
    text = item.get('text', '')
    match = re.search(r'(?im)^\s*treefile\s+"([^"]+)"', text)
    if match:
        properties[int(text.splitlines()[1].strip())] = norm(match[1])
counts = defaultdict(Counter)
positions = defaultdict(list)
important = ('metin2_map_a1', 'metin2_map_b1', 'metin2_map_trent')
for path, item in files.items():
    if path.rsplit('/', 1)[-1] not in ('areadata.txt', 'objectdata.txt'):
        continue
    mapname = path.rsplit('/', 2)[0]
    for block in re.findall(r'(?is)Start\s+Object\d+\s*(.*?)\s*End\s+Object', item.get('text', '')):
        lines = [line.strip() for line in block.splitlines() if line.strip()]
        if len(lines) < 2 or not lines[1].isdigit():
            continue
        key = properties.get(int(lines[1]))
        if key:
            counts[key][mapname] += 1
            if mapname in important:
                positions[key].append(dict(map=mapname, tile=path.rsplit('/', 1)[0], position=lines[0]))
rows = [dict(type=key, maps=dict(sorted(maps.items())), instances=sum(maps.values()),
             important_instances=sum(maps[name] for name in important), supported=key in registry,
             positions=positions[key]) for key, maps in counts.items()]
rows.sort(key=lambda row: (-row['important_instances'], -row['instances'], row['type']))
references = set(counts)
for path, item in files.items():
    if path.endswith('.msm'):
        references.update(norm(value) for value in re.findall(r'"([^"\n]+\.spt)"', item.get('text', ''), re.I))
for path in (Path(__file__).resolve().parents[2] / 'src').rglob('*'):
    if path.suffix in ('.cpp', '.h'):
        references.update(norm(value) for value in re.findall(r'"(d:[^"\n]+\.spt)"', path.read_text(encoding='utf-8', errors='replace'), re.I))
grass = defaultdict(lambda: dict(maps=set(), tiles=0, samples=0, important_samples=0))
mapsets = {}
for path, item in files.items():
    if path.endswith('/setting.txt'):
        match = re.search(r'(?im)^\s*TextureSet\s+([^\r\n]+)', item.get('text', ''))
        if match:
            name = norm(match[1])
            mapsets[path.rsplit('/', 1)[0]] = name if name.startswith('textureset/') else 'textureset/' + name
for path, item in files.items():
    if 'tileCounts' not in item:
        continue
    mapname = path.rsplit('/', 2)[0]
    definitions = files.get(mapsets.get(mapname), {}).get('text', '')
    layers = {int(index): norm(texture) for index, texture in re.findall(r'(?is)Start\s+Texture(\d+)\s+"([^"]+)"', definitions)}
    for index, samples in enumerate(item['tileCounts']):
        texture = layers.get(index, '')
        if samples and index and 'grass' in texture:
            row = grass[texture]
            row['maps'].add(mapname)
            row['tiles'] += 1
            row['samples'] += samples
            row['important_samples'] += samples if mapname in important else 0
grass_rows = [dict(texture=key, **{**value, 'maps': sorted(value['maps'])}) for key, value in grass.items()]
grass_rows.sort(key=lambda row: (-row['important_samples'], -row['samples'], row['texture']))
report = dict(input_sha256=hashlib.sha256(source.read_bytes()).hexdigest(), important_maps=important,
              registry_types=len(registry), referenced_types=len(references), placed_types=len(rows), unsupported=sorted(references-set(registry)),
              non_placement_references=sorted(references-set(counts)),
              placements=sum(row['instances'] for row in rows), vegetation=rows, grass=grass_rows,
              missing_packs=data['missingPacks'])
(output / 'content-priority.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
lines = ['# H2 vegetation content priority', '',
         'Prioritized by actual placements in A1, B1 and Trent, then all authored maps. Map count does not mean server activity. Terrain samples include the existing duplicated tile border.', '',
         f"Registry: {len(registry)}; referenced types including actor/event trees: {len(references)}; placed types: {len(rows)}; placements: {report['placements']}; unsupported: {len(report['unsupported'])}.", '',
         '| Priority | Type | Maps | Instances | A1 / B1 / Trent |', '| ---: | --- | ---: | ---: | ---: |']
for i, row in enumerate(rows):
    lines.append(f"| {i+1} | `{row['type']}` | {len(row['maps'])} | {row['instances']} | {row['important_instances']} |")
lines += ['', '## Grass mask impact', '',
          'The installed maps use categorical terrain splat indices. The runtime derives density from grass layers; there is no claim of a separate greyscale mask in these maps. Water, collision and slope exclusions reduce actual placement.', '',
          '| Texture | Maps | Tile files | Samples | A1 / B1 / Trent samples |', '| --- | ---: | ---: | ---: | ---: |']
for row in grass_rows:
    lines.append(f"| `{row['texture']}` | {len(row['maps'])} | {row['tiles']} | {row['samples']} | {row['important_samples']} |")
(output / 'content-priority.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
print(json.dumps({key: report[key] for key in ('registry_types', 'referenced_types', 'unsupported', 'placements')}))
print('Top vegetation:', [(row['type'], row['important_instances']) for row in rows[:6]])
