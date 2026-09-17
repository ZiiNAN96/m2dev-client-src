"""Bounded BUGFIX-X evidence, with equal warm windows and strict shutdown checks."""
import csv
import hashlib
import json
from pathlib import Path
import re
import statistics
import sys

root = Path(sys.argv[1])

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def summarize(name):
    directory = root / ('runtime-' + name)
    assert (directory / 'exit.txt').read_text(encoding='utf-8-sig').strip() == '0'
    assert not (directory / 'bugfix-failure.log').exists()
    assert (directory / 'log/syserr.txt').stat().st_size == 0
    log = (directory / 'bugfix-route.log').read_text()
    samples = [json.loads(line[7:]) for line in log.splitlines() if line.startswith('sample=')]
    shutdown = json.loads(next(line[9:] for line in log.splitlines() if line.startswith('shutdown=')))
    assert shutdown['terrainResident'] == shutdown['areasResident'] == 0
    first = next(s for s in samples if s['seconds'] >= 5)
    last = next(s for s in samples if s['seconds'] >= 44)
    audit = (directory / 'source-resource-audit.log').read_text()
    zeros = ('GraphicsSettingsObjects SourceTextures SourceBuffers SkinMeshes BoneRemaps BonePalettes '
             'SkinPreparationFailures CollisionResources VegetationAssets VegetationInstances VegetationRenderAssets '
             'VegetationGeometry VegetationInstanceBuffers VegetationFailures VegetationInstanceBytes '
             'AllCPUDeformationCalls AllCPUDeformationVertices GPUFallbacks AssetDocuments AnimationInstances '
             'MeshBindings GR2ReaderResources IndependentAnimationInstances AnimationRuntimeFailures '
             'RuntimeSkeletons RuntimeAnimationClips PrototypeGeometry PrototypePalettes DiligentErrors DiligentFatals').split()
    for key in zeros:
        assert re.search(r'\b' + key + r'=0\b', audit), (name, key)
    renderer = (directory / 'terrain-renderer.log').read_text()
    shutdown_lines = [line for line in renderer.splitlines() if line.startswith('shutdown ')]
    assert len(shutdown_lines) == 8
    assert not re.search(r'=[1-9]\d*', '\n'.join(shutdown_lines))
    rows = list(csv.DictReader((directory / 'skinning-benchmark.csv').open()))
    warm = [row for row in rows if row['sample'] == '1'][180:780]
    assert len(warm) == 600
    performance = {}
    for field in ('process_cpu_us', 'gpu_frame_us'):
        values = [float(row[field]) / 1000 for row in warm if row[field]]
        assert len(values) == 600, (name, field, len(values))
        performance[field.replace('_us', '_ms')] = dict(mean=statistics.mean(values), median=statistics.median(values), p95=sorted(values)[569])
    terrain_draws = [int(draws) for frame, draws in re.findall(r'^frame=(\d+) terrain=\d+ draws=(\d+)', renderer, re.M) if 180 <= int(frame) <= 780]
    performance['terrain_draws_sample_mean'] = statistics.mean(terrain_draws)
    deltas = {key: last['world'][key] - first['world'][key] for key in (
        'terrainLoaded', 'terrainUnloaded', 'areasLoaded', 'areasUnloaded', 'terrainAssignments',
        'instanceBufferCreates', 'treeBlockerDraws', 'treeLodSwitches', 'treeCreated')}
    tail = [s for s in samples if s['seconds'] >= 120]
    result = dict(binary_sha256=sha(directory / 'Metin2_Release.exe'), frames=int(re.search(r'completed=1 frames=(\d+)', log)[1]),
                  seconds=samples[-1]['seconds'], warm_first=first, warm_last=last, warm_delta=deltas,
                  performance=performance, shutdown=shutdown, shutdown_zero_fields=zeros,
                  final_sample=samples[-1], resource_checks='PASS')
    if name == 'final':
        assert len(samples) >= 140 and samples[-1]['seconds'] >= 149
        for sample in samples:
            if sample['seconds'] < 5:
                continue
            assert sample['world']['terrainResident'] == sample['world']['areasResident'] == 20
            assert sample['world']['treeBlockerDraws'] == 0
            assert sample['vegetation']['grassPlacements'] == 0
            for key in ('terrainLoaded', 'terrainUnloaded', 'areasLoaded', 'areasUnloaded', 'terrainAssignments', 'treeCreated'):
                assert sample['world'][key] == first['world'][key], key
            assert sample['world']['terrainVisible'] == sample['world']['terrainDrawn'], sample
        result['last_30_seconds_buffer_creates'] = tail[-1]['world']['instanceBufferCreates'] - tail[0]['world']['instanceBufferCreates']
        result['last_30_seconds_world_rebuilds'] = tail[-1]['world']['terrainAssignments'] - tail[0]['world']['terrainAssignments']
    return result

result = dict(method='1024x768 High Modern, same A1 route for the first 45s; final continues in A1 town to 150s. '
                    'World deltas use second 5 to second 44. CPU/GPU use 600 warm sample=1 frames after skipping '
                    '180 warm frames; no outlier deletion. Existing other clients remained untouched: sanity only, '
                    'not an isolated benchmark. Draw sample is terrain draws, not whole-frame draws. '
                    'Actor benchmark draws are deliberately not mislabeled as whole-frame draws.',
              before=summarize('before'), after=summarize('final'))
assert result['after']['binary_sha256'] == sha(root / 'runtime-manual/Metin2_Release.exe')
(root / 'evidence/result.json').write_text(json.dumps(result, indent=2) + '\n')
for name in ('before', 'after'):
    run = result[name]
    print(name, 'deltas=', run['warm_delta'], 'performance=', run['performance'])
print('PASS complete A1 route, stable residency, complete terrain coverage, no transparent trees, grass disabled, zero shutdown resources')
