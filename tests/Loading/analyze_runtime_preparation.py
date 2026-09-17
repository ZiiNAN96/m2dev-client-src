"""P0-L8: aggregate opt-in native preparation diagnostics, never timing gates."""
import json
import sys
from pathlib import Path
from summarize_trace import read

def analyze(directory):
    result=[]
    resident={}
    for load in read(directory/'map-load-trace.tsv'):
        if load['label'].startswith('first-use-'): continue
        clips={}
        for event in load['events']:
            if not event['kind'].startswith('runtime-') or event['kind'] in ('runtime-cache-hit','runtime-cache-miss','runtime-prewarm-request'): continue
            clip=clips.setdefault(event['name'],{'id':event['name']})
            clip[event['kind']]=event['bytes']
            if event['kind']=='runtime-preparation':
                clip['preparations']=event['count']
                clip['in_prewarm']=event['bytes']!=0
        for clip in clips.values():
            clip['duration']=clip.get('runtime-duration-ns',0)/1e9/clip['preparations']
            clip['ms']=clip.get('runtime-total-ns',0)/1e6
            clip['keys_per_second']=clip['runtime-keys']/clip['duration'] if clip['duration'] else 0
            clip['keys_per_track']=clip['runtime-keys']/clip['runtime-tracks'] if clip['runtime-tracks'] else 0
            clip['expansion']=clip['runtime-keys']/clip['runtime-source-keys'] if clip['runtime-source-keys'] else 0
            resident[clip['id']]=clip.get('runtime-owned-bytes',0)
        selected=[c for c in clips.values() if c['in_prewarm']]
        totals={key:sum(c.get(key,0) for c in selected) for key in set().union(*(c.keys() for c in selected)) if key.startswith('runtime-')}
        wall=next((c['inclusive_ms'] for c in load['costs'] if c['name']=='Production animation prewarm'),0)
        stages=[]
        for key,value in totals.items():
            if not key.startswith('runtime-stage-ns-'): continue
            name=key.removeprefix('runtime-stage-ns-')
            stages.append(dict(stage=name,ms=value/1e6,calls=totals['runtime-stage-calls-'+name],
                max_clip_ms=max(c.get(key,0) for c in selected)/1e6,percent=100*value/1e6/wall if wall else 0))
        row=dict(label=load['label'],wall_ms=wall,unique_prepared=len(selected),
            actual_preparations=sum(c['preparations'] for c in selected),
            all_runtime_bytes=sum(c.get('runtime-owned-bytes',0) for c in clips.values()),
            retained_runtime_bytes=sum(resident.values()),
            peak_temporary_bytes=max((c.get('runtime-temporary-peak',0) for c in clips.values()),default=0),
            requests=load['counters'].get('runtime-prewarm-request',0),cache_hits=load['counters'].get('runtime-cache-hit',0),
            cache_misses=load['counters'].get('runtime-cache-miss',0),totals=totals,
            stages=sorted(stages,key=lambda s:-s['ms']),clips=sorted(clips.values(),key=lambda c:-c['runtime-keys']),
            cache_costs=[c for c in load['costs'] if c['phase']=='Animation prewarm'])
        result.append(row)
    assert any(r['actual_preparations'] for r in result),'Missing native detail capture; enable private probe marker'
    (directory/'runtime-preparation.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return result

if __name__=='__main__':
    for load in analyze(Path(sys.argv[1])):
        print(json.dumps({k:v for k,v in load.items() if k not in ('clips','totals','cache_costs')},indent=2))
