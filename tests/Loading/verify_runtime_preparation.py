"""Focused L8 data/first-use gate, separate from the reported timing decision."""
import json
import subprocess
import sys
from pathlib import Path
from analyze_runtime_preparation import analyze
from analyze_first_use import analyze as first_use
from summarize_trace import read

before,after,profile_before,profile_after=map(Path,sys.argv[1:5])
old,new=analyze(profile_before),analyze(profile_after)
a={c['id']:c for c in old[1]['clips']}; b={c['id']:c for c in new[1]['clips']}
assert a.keys()==b.keys()
unchanged=('runtime-source-tracks','runtime-tracks','runtime-source-keys','runtime-keys','runtime-owned-bytes',
           'runtime-allocations','runtime-reallocations','runtime-allocated-bytes','runtime-relocated-bytes',
           'runtime-temporary-allocated','runtime-temporary-peak','runtime-split-nodes','runtime-leaf-nodes',
           'runtime-key-channel-0','runtime-key-channel-1','runtime-key-channel-2',
           'runtime-stage-calls-refinement error checks','runtime-duration-ns')
for key in a:
    assert a[key]['preparations']==b[key]['preparations']==1,(key,'duplicate binding')
    for field in unchanged: assert a[key][field]==b[key][field],(key,field)
    assert a[key]['runtime-sample-calls']-b[key]['runtime-sample-calls']==b[key]['runtime-reused-samples'],key
assert sum(c['runtime-reused-samples'] for c in b.values())>0
for series in (old,new):
    assert series[2]['actual_preparations']==series[3]['actual_preparations']==0
    assert series[1]['retained_runtime_bytes']==series[3]['retained_runtime_bytes']

uses=[first_use(directory) for directory in (before,after)]
assert [r['path'] for r in uses[0]['rows']]==[r['path'] for r in uses[1]['rows']]
subprocess.run([sys.executable,str(Path(__file__).with_name('analyze_first_use.py')),str(after),'--expect-prepared'],check=True,stdout=subprocess.DEVNULL)
invariants=('gr2-parse','gr2-payload-crc','gr2-section-compressed','gr2-section-expanded','gr2-models','gr2-meshes',
            'gr2-skeletons','gr2-animations','gr2-request','gr2-lookup-hit','gr2-lookup-miss','gr2-CopyVertices','gr2-CopyIndices',
            'shader-cache-bytecode','animation-registration','animation-actor-reference')
loads=lambda directory:[l for l in read(directory/'map-load-trace.tsv') if not l['label'].startswith('first-use-')]
for left,right in zip(loads(before),loads(after)):
    assert left['label']==right['label']
    for kind in invariants:
        volume=lambda load:sorted((e['name'],e['count'],e['bytes']) for e in load['events'] if e['kind']==kind)
        assert volume(left)==volume(right),(left['label'],kind)
    assert not right['counters'].get('shader-runtime-compile',0)
    assert not right['counters'].get('shader-cache-miss',0)
for directory in (before,after,profile_before,profile_after):
    subprocess.run([sys.executable,str(Path(__file__).with_name('verify_probe.py')),str(directory),'--shader-lifecycle','--animation-first-use'],check=True,stdout=subprocess.DEVNULL)
result=dict(correctness='PASS',first_use='PASS',map_data_invariants='PASS',resources='PASS',
            unique_bindings=len(a),runtime_keys=sum(c['runtime-keys'] for c in b.values()),
            runtime_bytes=new[1]['retained_runtime_bytes'],eliminated_curve_evaluations=sum(c['runtime-reused-samples'] for c in b.values()))
(after/'runtime-preparation-gate.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))
