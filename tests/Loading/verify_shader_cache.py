"""Compare distinct native processes; never confuse persistent hits with map warmth."""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from summarize_trace import read


def process(directory):
    exit_info = json.loads((directory / 'exit.json').read_text(encoding='utf-8-sig'))
    gate = json.loads((directory / 'fast-gate.json').read_text(encoding='utf-8-sig'))
    assert exit_info['ExitCode'] == 0 and gate['status'] == 'PASS'
    loads = read(directory / 'map-load-trace.tsv')
    run = json.loads((directory / 'cache-run.json').read_text(encoding='utf-8-sig'))
    expected_directory = Path(run['CacheDirectory']).as_posix().lower()
    evidence = {}
    metrics = []
    for load in loads:
        events = load['events']
        count = lambda kind: sum(e['count'] for e in events if e['kind'] == kind)
        duration = lambda kind: sum(c['inclusive_ms'] for c in load['costs'] if c['kind'] == kind)
        requests, hits, misses = (count('shader-cache-' + kind) for kind in ('request', 'hit', 'miss'))
        if requests:
            directories = {e['name'].lower() for e in events if e['kind'] == 'shader-cache-directory'}
            assert directories == {expected_directory}, (directories, expected_directory)
        assert requests == hits + misses
        assert misses == count('shader-runtime-compile')
        assert count('shader-cache-invalid') == count('shader-cache-write-failure') == 0
        metrics.append(dict(label=load['label'], total_ms=load['total_ms'],
                            shader_block_ms=load['phases'].get('Shaders / PSOs', 0),
                            requests=requests, hits=hits, misses=misses,
                            compiles=count('shader-runtime-compile'),
                            compile_ms=duration('shader-compilation'), read_ms=duration('shader-cache-read'),
                            key_ms=duration('shader-cache-key'), write_ms=duration('shader-cache-write'),
                            bytes_loaded=sum(e['bytes'] for e in events if e['kind'] == 'shader-cache-hit')))
        for event in events:
            if event['kind'] != 'shader-cache-bytecode':
                continue
            name, rest = event['name'].rsplit(' | key=', 1)
            key, checksum = rest.split(' | dxbc=')
            previous = evidence.setdefault(key, dict(dxbc=checksum, names=[]))
            assert previous['dxbc'] == checksum
            if name not in previous['names']:
                previous['names'].append(name)
    hashes = json.loads((directory / 'hashes.json').read_text(encoding='utf-8-sig'))
    exe = next(h['Hash'] for h in hashes if h['Path'].endswith('.exe'))
    return dict(pid=exit_info['PID'], executable_sha256=exe, loads=metrics, evidence=evidence)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('empty', type=Path)
    parser.add_argument('warm', type=Path)
    parser.add_argument('--breakdown', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    empty, warm = process(args.empty), process(args.warm)
    assert empty['pid'] != warm['pid']
    assert empty['executable_sha256'] == warm['executable_sha256']
    start = json.loads((args.empty / 'cache-run.json').read_text(encoding='utf-8-sig'))
    assert start['ExistingEntries'] == 0
    assert json.loads((args.warm / 'cache-run.json').read_text(encoding='utf-8-sig'))['ExistingEntries'] > 0
    a = next(v for v in empty['loads'] if v['label'] == 'A1-cold')
    b = next(v for v in warm['loads'] if v['label'] == 'A1-cold')
    assert a['compiles'] > 0 and a['requests'] == b['requests']
    assert all(v['misses'] == v['compiles'] == 0 for v in warm['loads'])
    assert empty['evidence'].keys() == warm['evidence'].keys()
    for key, value in empty['evidence'].items():
        assert value['dxbc'] == warm['evidence'][key]['dxbc'], key
    examples = {}
    selectors = {
        'Actors/GPU Skinning ModernVS': ('ModernVS | ModernVS', 'GDX_SKIN=1'),
        'Terrain': ('TerrainVS | TerrainVS', 'GDX_SHADOW=0'),
        'Vegetation': ('ModernVS | ModernVS', 'H2_INSTANCED=1'),
        'Water': ('WaterCompositePS | WaterCompositePS',),
        'PBR': ('PrecomputeBRDF_PS | PrecomputeBRDF_PS',),
        'PostFX/SSAO': ('ComputeAmbientOcclusionPS | ComputeAmbientOcclusionPS',),
    }
    for category, tokens in selectors.items():
        matches = [(key, value) for key, value in empty['evidence'].items()
                   if any(all(t in name for t in tokens) for name in value['names'])]
        assert len(matches) == 1, (category, matches)
        key, value = matches[0]
        data = (Path(start['CacheDirectory']) / (key + '.shadercache')).read_bytes()
        magic, version, stage, low, high, size, checksum_low, checksum_high = struct.unpack('<8sIIQQQQQ', data[:56])
        assert magic == b'M2DXBC01' and version == 1 and size == len(data) - 56
        assert f'{high:016x}{low:016x}' == key
        assert f'{checksum_high:016x}{checksum_low:016x}' == value['dxbc']
        examples[category] = dict(key=key, stage=stage, dxbc_xxh128=value['dxbc'],
                                  dxbc_sha256=hashlib.sha256(data[56:]).hexdigest(), bytes=size,
                                  cached_matches_compiled=True)
    result = dict(status='PASS', empty=empty, warm=warm, representative_bytecode=examples,
                  difference=dict(ms_saved=a['total_ms'] - b['total_ms'],
                                  percent_saved=100 * (1 - b['total_ms'] / a['total_ms']),
                                  compile_ms_saved=a['compile_ms'] - b['compile_ms']))
    if args.breakdown:
        final = process(args.breakdown)
        assert final['pid'] not in (empty['pid'], warm['pid'])
        assert final['executable_sha256'] == empty['executable_sha256']
        assert len(final['loads']) == 2 and final['loads'][1]['label'] == 'A1-cold'
        assert all(v['compiles'] == v['misses'] == 0 for v in final['loads'])
        assert final['evidence'].keys() == empty['evidence'].keys()
        for key, value in final['evidence'].items():
            assert value['dxbc'] == empty['evidence'][key]['dxbc']
        result['final_breakdown'] = final
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in result.items() if k in ('status', 'difference')}, indent=2))
    for label in ('empty', 'warm', 'final_breakdown'):
        if label in result:
            print(label, json.dumps(result[label]['loads']))


if __name__ == '__main__':
    main()
