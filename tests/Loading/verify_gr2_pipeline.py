"""Check the narrow before/after contract without requiring a chosen speedup."""
import json
import sys
from pathlib import Path
from gr2_pipeline import analyze
from summarize_trace import read

before,after=map(Path,sys.argv[1:3])
analyses=[analyze(path) for path in (before,after)]
traces=[read(path/'map-load-trace.tsv') for path in (before,after)]
for path,loads in zip((before,after),traces):
    assert json.loads((path/'fast-gate.json').read_text())['status']=='PASS'
    assert [x['label'] for x in loads]==['client-setup','A1-cold','B1-first-in-session','A1-warm']
    for load in loads:
        c=load['counters']
        assert not c.get('shader-runtime-compile',0)
        assert not c.get('shader-cache-miss',0)
        assert c.get('shader-cache-hit',0)==c.get('shader-cache-request',0)
    assert not loads[-1]['counters'].get('gr2-parse',0)
    assert not next(c for c in analyses[(before,after).index(path)] if c['label']=='A1-warm')['decompression_ms']

invariants={'gr2-parse','gr2-payload-crc','gr2-section-compressed','gr2-section-expanded',
            'gr2-models','gr2-meshes','gr2-skeletons','gr2-animations',
            'gr2-request','gr2-lookup-hit','gr2-lookup-miss','gr2-CopyVertices','gr2-CopyIndices',
            'shader-cache-bytecode'}
for a,b in zip(*traces):
    events=lambda load:sorted((e['kind'],e['name'],e['count'],e['bytes']) for e in load['events'] if e['kind'] in invariants)
    assert events(a)==events(b),a['label']
result={'status':'PASS','maps':[]}
for a,b in zip(*analyses):
    row={'label':a['label']}
    for key in ('total_load_ms','gr2_total_ms','provider_ms','decompression_ms'):
        old,new=a[key],b[key]
        row[key]={'before':old,'after':new,'saved_ms':old-new,'saved_percent':100*(old-new)/old if old else 0}
    result['maps'].append(row)
(after/'gr2-comparison.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result,indent=2))
