"""P0-L7 focused data/smoke gate. A remaining first-use hitch keeps NO-GO."""
import json
import sys
from pathlib import Path
from animation_loading import analyze_animations, normalized
from summarize_trace import read

before, after, smoke_before, smoke_after = map(Path, sys.argv[1:5])
old, new = analyze_animations(before), analyze_animations(after)
assert old[1]['label'] == new[1]['label'] == 'A1-cold'
for key in ('requests', 'unique', 'loads', 'parses', 'cache_hits', 'compressed_bytes', 'expanded_bytes'):
    assert old[1]['metrics'][key] == new[1]['metrics'][key], key
assert new[1]['metrics']['parses'] == new[1]['metrics']['loads'] == new[1]['metrics']['unique'] == 693
assert 1 < new[1]['concurrency']['peak'] <= 4
assert old[-1]['metrics']['parses'] == new[-1]['metrics']['parses'] == 0
assert new[1]['total_ms'] < old[1]['total_ms'], 'cold improvement required'
invariants = ('gr2-parse', 'gr2-payload-crc', 'gr2-section-compressed', 'gr2-section-expanded',
              'gr2-models', 'gr2-meshes', 'gr2-skeletons', 'gr2-animations', 'gr2-request',
              'gr2-lookup-hit', 'gr2-lookup-miss', 'gr2-CopyVertices', 'gr2-CopyIndices',
              'shader-cache-bytecode', 'animation-registration', 'animation-actor-reference')
for a, b in zip(read(before / 'map-load-trace.tsv'), read(after / 'map-load-trace.tsv')):
    volume = lambda load: sorted((e['kind'], e['name'], e['count'], e['bytes']) for e in load['events'] if e['kind'] in invariants)
    assert volume(a) == volume(b), a['label']
    for load in (a, b):
        assert not load['counters'].get('shader-runtime-compile', 0)
        assert not load['counters'].get('shader-cache-miss', 0)

smokes = []
for directory in (smoke_before, smoke_after):
    assert json.loads((directory / 'fast-gate.json').read_text())['status'] == 'PASS'
    rows = json.loads((directory / 'animation-smoke.json').read_text())
    loads = read(directory / 'map-load-trace.tsv')
    references = {}
    for e in loads[1]['events']:
        if e['kind'] == 'animation-registration':
            path, race, mode, index = e['name'].rsplit('|', 3)
            references.setdefault(normalized(path), set()).add((int(race), int(index)))
    trace = {load['label']: load for load in loads if load['label'].startswith('animation-smoke-')}
    assert len(rows) == len(trace) == 12
    for row in rows:
        load = trace['animation-smoke-' + row['actor'] + '-' + row['motion']]
        assert load['endpoint'] == 'animation-smoke-complete'
        assert not load['counters'].get('gr2-parse', 0), 'no deferred GR2 reads'
        used = {normalized(e['name']) for e in load['events'] if e['kind'].startswith('animation-use-')}
        identity = (0 if row['actor'] == 'player' else 101, row['index'])
        assert any(identity in references.get(path, set()) for path in used), (row, used)
        row['runtime_preparation_ms'] = sum(c['inclusive_ms'] for c in load['costs'] if c['name'] == 'GR2 animation preparation')
        row['played_files'] = sorted(used)
    smokes.append(rows)
# 50 ms is the existing probe's stable-frame budget, not a claim that 49 ms
# is imperceptible. Exceeding it rules out an unqualified "no first-use stall".
stalls = [r for r in smokes[1] if r['max_frame_ms'] > 50]
result = dict(status='FAIL' if stalls else 'PASS', decision='NO-GO' if stalls else 'GO',
              correctness='PASS', data_invariants='PASS', all_motions_played='PASS',
              first_use_budget_ms=50, remaining_first_use_stalls=stalls,
              smoke_before=smokes[0], smoke_after=smokes[1],
              cold_saved_ms=old[1]['total_ms']-new[1]['total_ms'],
              cold_saved_percent=100*(old[1]['total_ms']-new[1]['total_ms'])/old[1]['total_ms'])
(after / 'animation-gate.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print(json.dumps({k: v for k, v in result.items() if not k.startswith('smoke_')}, indent=2))
sys.exit(1 if stalls else 0)
