"""Focused P0-L6 invariants and wall/CPU separation; does not require a speedup."""
import json
import sys
from pathlib import Path
from gr2_pipeline import analyze
from summarize_trace import read

before, after = map(Path, sys.argv[1:3])
traces = [read(path / 'map-load-trace.tsv') for path in (before, after)]
analyses = [analyze(path) for path in (before, after)]
invariants = {
    'gr2-parse', 'gr2-payload-crc', 'gr2-section-compressed', 'gr2-section-expanded',
    'gr2-models', 'gr2-meshes', 'gr2-skeletons', 'gr2-animations',
    'gr2-request', 'gr2-lookup-hit', 'gr2-lookup-miss', 'gr2-CopyVertices',
    'gr2-CopyIndices', 'shader-cache-bytecode',
}


def volume(load, kind):
    return sum(e['bytes'] for e in load['events'] if e['kind'] == kind)


def peak(load, kind):
    return max((int(e['name']) for e in load['events'] if e['kind'] == kind), default=0)


def measurement(load, analysis):
    ms = lambda kind: volume(load, kind) / 1e6
    wall = ms('gr2-wall-ns')
    busy = ms('gr2-busy-ns')
    return dict(
        total_ms=analysis['total_load_ms'],
        gr2_wall_ms=analysis['gr2_total_ms'] - ms('gr2-worker-cost-ns') + wall,
        gr2_cumulative_duration_ms=analysis['gr2_total_ms'],
        decompression_wall_ms=analysis['decompression_ms'] - ms('gr2-worker-decode-cumulative-ns') + ms('gr2-worker-decode-wall-ns'),
        decompression_cumulative_duration_ms=analysis['decompression_ms'],
        thread_cpu_ms=volume(load, 'gr2-thread-cpu-100ns') / 1e4 + ms('gr2-worker-cpu-ns'),
        jobs=volume(load, 'gr2-jobs'), completed=volume(load, 'gr2-files-completed'),
        workers=peak(load, 'gr2-workers'), peak=peak(load, 'gr2-peak'),
        average=busy / wall if wall else 0, worker_busy_ms=busy, preparation_wall_ms=wall,
        worker_cpu_ms=ms('gr2-worker-cpu-ns'),
        single_flight_joins=volume(load, 'gr2-single-flight-joins'),
        waits=load['counters'].get('gr2-completion-waits', 0),
    )


for path, loads in zip((before, after), traces):
    assert json.loads((path / 'fast-gate.json').read_text())['status'] == 'PASS'
    assert [x['label'] for x in loads] == ['client-setup', 'A1-cold', 'B1-first-in-session', 'A1-warm']
    for load in loads:
        c = load['counters']
        assert not c.get('shader-runtime-compile', 0)
        assert not c.get('shader-cache-miss', 0)
        assert c.get('shader-cache-hit', 0) == c.get('shader-cache-request', 0)
    assert not loads[-1]['counters'].get('gr2-parse', 0)
    assert not volume(loads[-1], 'gr2-jobs')

result = dict(status='PASS', maps=[])
for index, (old, new) in enumerate(zip(*traces)):
    events = lambda load: sorted((e['kind'], e['name'], e['count'], e['bytes']) for e in load['events'] if e['kind'] in invariants)
    assert events(old) == events(new), old['label']
    a, b = measurement(old, analyses[0][index]), measurement(new, analyses[1][index])
    assert b['jobs'] == b['completed']
    assert b['peak'] <= b['workers'] <= 4
    saving = {}
    for key in ('total_ms', 'gr2_wall_ms', 'decompression_wall_ms'):
        saving[key] = dict(ms=a[key] - b[key], percent=100 * (a[key] - b[key]) / a[key] if a[key] else 0)
    result['maps'].append(dict(label=old['label'], before=a, after=b, saving=saving))
assert result['maps'][1]['after']['peak'] > 1
(after / 'gr2-parallel-comparison.json').write_text(json.dumps(result, indent=2))
print(json.dumps(result, indent=2))
