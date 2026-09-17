"""Short A1 before/after comparison; final route and lifetime checks are strict."""
import csv
import hashlib
import json
from pathlib import Path
import re
import statistics
import sys

root = Path(sys.argv[1])

def summarize(name):
    directory = root / ('runtime-' + name)
    assert (directory / 'exit.txt').read_text(encoding='utf-8-sig').strip() == '0'
    assert not (directory / 'bugfix-failure.log').exists()
    assert not (directory / 'log/syserr.txt').read_bytes()
    log = (directory / 'bugfix-route.log').read_text()
    samples = [json.loads(line[7:]) for line in log.splitlines() if line.startswith('sample=')]
    first = next(s for s in samples if s['seconds'] >= 5)
    last = next(s for s in samples if s['seconds'] >= 44)
    stable = ('terrainLoaded', 'terrainUnloaded', 'areasLoaded', 'areasUnloaded', 'terrainAssignments', 'treeCreated')
    for sample in samples:
        if sample['seconds'] < 5:
            continue
        world = sample['world']
        assert world['terrainResident'] == world['areasResident'] == 20
        assert world['treeBlockerDraws'] == sample['vegetation']['grassPlacements'] == 0
        assert world['terrainVisible'] == world['terrainDrawn']
        assert all(world[k] == first['world'][k] for k in stable)
    audit = (directory / 'source-resource-audit.log').read_text()
    zero_fields = ('GraphicsSettingsObjects SourceTextures SourceBuffers SkinMeshes BoneRemaps BonePalettes '
        'SkinPreparationFailures CollisionResources VegetationAssets VegetationInstances VegetationRenderAssets '
        'VegetationGeometry VegetationInstanceBuffers VegetationFailures VegetationInstanceBytes '
        'AllCPUDeformationCalls AllCPUDeformationVertices GPUFallbacks AssetDocuments AnimationInstances '
        'MeshBindings GR2ReaderResources IndependentAnimationInstances AnimationRuntimeFailures '
        'RuntimeSkeletons RuntimeAnimationClips PrototypeGeometry PrototypePalettes DiligentErrors DiligentFatals').split()
    for key in zero_fields:
        assert re.search(r'\b' + key + r'=0\b', audit), (name, key)
    shutdown = json.loads(next(line[9:] for line in log.splitlines() if line.startswith('shutdown=')))
    assert shutdown['terrainResident'] == shutdown['areasResident'] == 0
    groups = [s for s in (directory / 'terrain-renderer.log').read_text().splitlines() if s.startswith('shutdown ')]
    assert len(groups) == 8 and not re.search(r'=[1-9]\d*', '\n'.join(groups))
    rows = list(csv.DictReader((directory / 'skinning-benchmark.csv').open()))
    warm = [r for r in rows if first['frame'] < int(r['serial']) <= last['frame']]
    assert len(warm) > 1500
    performance = {}
    for field in ('process_cpu_us', 'gpu_frame_us'):
        values = [float(r[field]) / 1000 for r in warm]
        performance[field.replace('_us', '_ms')] = dict(mean=statistics.mean(values), median=statistics.median(values))
    frames = last['frame'] - first['frame']
    for field in ('draws', 'triangles'):
        performance['vegetation_' + field + '_per_frame'] = (last['vegetation'][field] - first['vegetation'][field]) / frames
    delta = {key: last['world'][key] - first['world'][key] for key in (*stable, 'instanceBufferCreates', 'treeLodSwitches')}
    result = dict(binary_sha256=hashlib.sha256((directory / 'Metin2_Release.exe').read_bytes()).hexdigest(),
        seconds=samples[-1]['seconds'], frames=int(re.search(r'completed=1 frames=(\d+)', log)[1]),
        warm_frames=len(warm), warm_delta=delta, performance=performance,
        final_sample=samples[-1], shutdown_zero_fields=zero_fields, shutdown_renderer_groups=8)
    if name == 'final':
        assert samples[-1]['seconds'] >= 149
        tail = [s for s in samples if s['seconds'] >= 120]
        result['stationary_tail_buffer_creates'] = tail[-1]['world']['instanceBufferCreates'] - tail[0]['world']['instanceBufferCreates']
        assert result['stationary_tail_buffer_creates'] == 0
    return result

result = dict(method='1024x768 High Modern; identical first 45s across three A1 town trees. '
    'Comparison uses every recorded frame between samples at 5s and 44s, including identical screenshot cadence. '
    'Vegetation draws/triangles include all renderer passes, not whole-frame draws. '
    'Final run continues with reverse motion/camera rotation to 120s and a stationary final 30s. '
    'Short sanity comparison on a shared desktop, not an isolated benchmark.',
    before=summarize('before'), after=summarize('final'))
manual = root / 'runtime-manual/Metin2_Release.exe'
assert hashlib.sha256(manual.read_bytes()).hexdigest() == result['after']['binary_sha256']
(root / 'evidence/result.json').write_text(json.dumps(result, indent=2) + '\n')
for key in ('before', 'after'):
    print(key, json.dumps({k: result[key][k] for k in ('warm_delta', 'performance')}, indent=2))
print('PASS exact manual binary; stable A1 world; solid tree path; grass disabled; clean exit and zero checked resources')
