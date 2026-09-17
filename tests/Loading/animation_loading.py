"""P0-L7 per-file animation evidence, using native registrations and requests."""
import json
import sys
from collections import defaultdict
from pathlib import Path
from gr2_pipeline import analyze
from summarize_trace import read

def normalized(path):
    return path.lower().replace('\\', '/')

def clip_type(index):
    if index in (1, 22, 32): return 'Idle'
    if index == 2: return 'Walk'
    if index == 3: return 'Run'
    if 5 <= index <= 10: return 'Damage'
    if index in (11, 12): return 'Death'
    if 13 <= index <= 21: return 'Attack'
    if 50 <= index < 305: return 'Skill'
    return 'Other'

def analyze_animations(directory):
    loads = read(directory / 'map-load-trace.tsv')
    pipelines = analyze(directory)
    known = {}
    registrations = defaultdict(set)
    result = []
    for load, pipeline in zip(loads, pipelines):
        events = load['events']
        for e in events:
            if e['kind'] == 'animation-registration':
                path, race, mode, index = e['name'].rsplit('|', 3)
                registrations[normalized(path)].add((int(race), int(mode), int(index)))
        used = {normalized(e['name']) for e in events if e['kind'] == 'animation-use-before-present'}
        modes = {tuple(map(int, e['name'].split('|'))) for e in events if e['kind'] == 'animation-active-mode'}
        near = set()
        for e in events:
            if e['kind'] == 'animation-actor-reference':
                path, race, mode, index = e['name'].rsplit('|', 3)
                if (int(race), int(mode)) in modes and int(index) in (1, 2, 3):
                    near.add(normalized(path))
        groups = defaultdict(lambda: dict(files=0, decode_cumulative_ms=0., compressed_bytes=0, expanded_bytes=0))
        animations = []
        for a in pipeline['assets']:
            path = a['path']
            count = next((e['bytes'] for e in a['events'] if e['kind'] == 'gr2-animations'), 0)
            if a['sections']:
                # Parsed content decides animation vs model; VFS namespace gives model role.
                known[path] = ('Animation' if count else 'Actor/Model' if any(x in path for x in ('/pc/', '/pc2/', '/monster', '/npc', '/weapon/', '/mount/'))
                               else 'Area/Prop' if '/zone/' in path or '/building/' in path else 'Other')
            category = known.get(path, 'Other')
            g = groups[category]
            g['files'] += int(a['sections'] > 0)
            g['decode_cumulative_ms'] += a['decompression_ms']
            compressed = sum(s['compressed'] for s in pipeline['sections'] if s['path'] == path)
            g['compressed_bytes'] += compressed
            g['expanded_bytes'] += a['expanded_bytes']
            if category != 'Animation': continue
            costs = {c['name']: c for c in a['costs']}
            a['compressed_bytes'] = compressed
            a['animation_parse_ms'] = costs.get('GR2 animation curves', {}).get('inclusive_ms', 0)
            a['animation_parse_exclusive_ms'] = costs.get('GR2 animation curves', {}).get('exclusive_ms', 0)
            a['runtime_prep_ms'] = costs.get('GR2 animation preparation', {}).get('inclusive_ms', 0)
            a['registration_references'] = sorted(registrations[path])
            a['clip_types'] = sorted({clip_type(ref[2]) for ref in registrations[path]})
            a['first_playable'] = 'CRITICAL' if path in used else 'NEAR' if path in near else 'DEFERABLE'
            animations.append(a)
        paths = {a['path'] for a in animations}
        count_events = lambda kind: sum(e['count'] for e in events if e['kind'] == kind and normalized(e['name']) in paths)
        ns = lambda kind: load['bytes'].get(kind, 0) / 1e6
        requested = {normalized(e['name']) for e in events if e['kind'] == 'gr2-request'} & paths
        metrics = dict(requests=count_events('gr2-request'), unique=len(requested), loads=count_events('resource-load'),
                       parses=count_events('gr2-parse'), cache_hits=count_events('gr2-lookup-hit'),
                       compressed_bytes=sum(a['compressed_bytes'] for a in animations),
                       expanded_bytes=sum(a['expanded_bytes'] for a in animations),
                       decode_cumulative_ms=sum(a['decompression_ms'] for a in animations),
                       parse_cumulative_ms=sum(a['animation_parse_ms'] for a in animations),
                       parse_exclusive_ms=sum(a['animation_parse_exclusive_ms'] for a in animations),
                       runtime_prep_ms=sum(a['runtime_prep_ms'] for a in animations))
        metrics['decode_wall_ms'] = metrics['decode_cumulative_ms'] - ns('animation-worker-decode-cumulative-ns') + ns('animation-worker-decode-wall-ns')
        metrics['parse_wall_ms'] = metrics['parse_cumulative_ms'] - ns('animation-worker-parse-cumulative-ns') + ns('animation-worker-parse-wall-ns')
        first = {}
        for role in ('CRITICAL', 'NEAR', 'DEFERABLE'):
            subset = [a for a in animations if a['sections'] and a['first_playable'] == role]
            first[role] = dict(files=len(subset), cumulative_total_ms=sum(a['total_ms'] for a in subset), decode_ms=sum(a['decompression_ms'] for a in subset), parse_ms=sum(a['animation_parse_ms'] for a in subset))
        groups['Area/Prop']['decode_wall_ms'] = groups['Area/Prop']['decode_cumulative_ms'] - (ns('gr2-worker-decode-cumulative-ns') - ns('animation-worker-decode-cumulative-ns')) + (ns('gr2-worker-decode-wall-ns') - ns('animation-worker-decode-wall-ns'))
        for name, g in groups.items():
            if name != 'Area/Prop': g['decode_wall_ms'] = metrics['decode_wall_ms'] if name == 'Animation' else g['decode_cumulative_ms']
        concurrency = {e['kind']: load['bytes'][e['kind']] for e in events if e['kind'].startswith('animation-worker') or e['kind'] in ('animation-jobs', 'animation-single-flight-joins')}
        concurrency['peak'] = max((int(e['name']) for e in events if e['kind'] == 'animation-worker-peak'), default=0)
        concurrency['average'] = ns('animation-worker-busy-ns') / ns('animation-worker-wall-ns') if ns('animation-worker-wall-ns') else 0
        result.append(dict(label=load['label'], total_ms=load['total_ms'], first_present_ms=load['first_present_ms'],
                           gr2_wall_ms=pipeline['gr2_total_ms'] - ns('gr2-worker-cost-ns') + ns('gr2-wall-ns'),
                           metrics=metrics, categories=groups, first_playable=first,
                           concurrency=concurrency,
                           animations=sorted(animations, key=lambda a: -a['total_ms'])))
    (directory / 'animation-loading.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
    return result

if __name__ == '__main__':
    for row in analyze_animations(Path(sys.argv[1])):
        print(json.dumps({k: v for k, v in row.items() if k != 'animations'}, indent=2))
