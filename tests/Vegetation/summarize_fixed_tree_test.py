"""Compare equal A1/Trent route windows and audit the fixed-high private client."""
import csv
import hashlib
import json
from pathlib import Path
import re
import statistics
import sys

root = Path(sys.argv[1])
stable_keys = ('terrainLoaded', 'terrainUnloaded', 'areasLoaded', 'areasUnloaded',
               'terrainAssignments', 'treeCreated')
zero_fields = ('GraphicsSettingsObjects SourceTextures SourceBuffers SkinMeshes BoneRemaps BonePalettes '
    'SkinPreparationFailures CollisionResources VegetationAssets VegetationInstances VegetationRenderAssets '
    'VegetationGeometry VegetationInstanceBuffers VegetationFailures VegetationInstanceBytes '
    'AllCPUDeformationCalls AllCPUDeformationVertices GPUFallbacks AssetDocuments AnimationInstances '
    'MeshBindings GR2ReaderResources IndependentAnimationInstances AnimationRuntimeFailures '
    'RuntimeSkeletons RuntimeAnimationClips PrototypeGeometry PrototypePalettes DiligentErrors DiligentFatals').split()

def summarize(name):
    directory = root / ('runtime-' + name)
    assert (directory / 'exit.txt').read_text(encoding='utf-8-sig').strip() == '0'
    assert not (directory / 'bugfix-failure.log').exists()
    assert not (directory / 'log/syserr.txt').read_bytes()
    log = (directory / 'bugfix-route.log').read_text()
    samples = [json.loads(line[7:]) for line in log.splitlines() if line.startswith('sample=')]
    rows = list(csv.DictReader((directory / 'skinning-benchmark.csv').open()))
    result = dict(binary_sha256=hashlib.sha256((directory / 'Metin2_Release.exe').read_bytes()).hexdigest(),
                  frames=int(re.search(r'completed=2 frames=(\d+)', log)[1]), maps={})
    for map_name, residents in [('a1', 20), ('trent', 4)]:
        group = [s for s in samples if s['map'] == map_name]
        first = next(s for s in group if s['seconds'] >= 5)
        last = next(s for s in group if s['seconds'] >= 44)
        for sample in group:
            if sample['seconds'] < 5:
                continue
            world = sample['world']
            assert world['terrainResident'] == world['areasResident'] == residents
            assert world['treeBlockerDraws'] == sample['vegetation']['grassPlacements'] == 0
            assert world['terrainVisible'] == world['terrainDrawn']
            assert all(world[k] == first['world'][k] for k in stable_keys)
        warm = [r for r in rows if first['frame'] < int(r['serial']) <= last['frame']]
        assert len(warm) > 1000
        performance = {}
        for field in ('process_cpu_us', 'gpu_frame_us'):
            values = [float(r[field]) / 1000 for r in warm]
            performance[field.replace('_us', '_ms')] = dict(mean=statistics.mean(values), median=statistics.median(values))
        frames = last['frame'] - first['frame']
        for field in ('draws', 'triangles'):
            performance['vegetation_' + field + '_per_frame'] = (last['vegetation'][field] - first['vegetation'][field]) / frames
        delta = {key: last['world'][key] - first['world'][key]
                 for key in (*stable_keys, 'instanceBufferCreates', 'treeLodSwitches')}
        summary = dict(seconds=group[-1]['seconds'], warm_frames=len(warm), warm_delta=delta,
                       performance=performance, final_sample=group[-1])
        if name == 'final':
            assert group[-1]['seconds'] >= 74
            tail = [s for s in group if s['seconds'] >= 61]
            summary['stationary_tail_buffer_creates'] = tail[-1]['world']['instanceBufferCreates'] - tail[0]['world']['instanceBufferCreates']
            assert summary['stationary_tail_buffer_creates'] == 0
        result['maps'][map_name] = summary
    audit = (directory / 'source-resource-audit.log').read_text()
    for key in zero_fields:
        assert re.search(r'\b' + key + r'=0\b', audit), (name, key)
    shutdown = json.loads(next(line[9:] for line in log.splitlines() if line.startswith('shutdown=')))
    assert shutdown['terrainResident'] == shutdown['areasResident'] == 0
    groups = [s for s in (directory / 'terrain-renderer.log').read_text().splitlines() if s.startswith('shutdown ')]
    assert len(groups) == 8 and not re.search(r'=[1-9]\d*', '\n'.join(groups))
    result.update(shutdown_zero_fields=zero_fields, shutdown_renderer_groups=len(groups))
    return result

result = dict(method='1024x768 Modern High, original packed assets; identical first 45s per map. '
    'Compare every frame between 5s and 44s samples (same three screenshots per map). '
    'Final fixed-high route adds 15s reverse/rotation and 15s stationary per map; 150s total. '
    'Vegetation submissions/triangles include all passes. Shared desktop sanity comparison, not an isolated benchmark.',
    before=summarize('before'), after=summarize('final'))
manual = root / 'runtime-manual/Metin2_Release.exe'
assert hashlib.sha256(manual.read_bytes()).hexdigest() == result['after']['binary_sha256']
(root / 'evidence/result.json').write_text(json.dumps(result, indent=2) + '\n')
for key in ('before', 'after'):
    for name, summary in result[key]['maps'].items():
        print(key, name, json.dumps({k: summary[k] for k in ('warm_delta', 'performance')}))
print('PASS exact manual binary; stable A1/Trent residency; solid trees; grass off; clean exits and zero checked resources')
