"""Narrow P0-L7C playback attribution using existing native trace windows."""
import json
import sys
from pathlib import Path
from summarize_trace import read

def analyze(directory):
    rows=json.loads((directory/'animation-first-use.json').read_text())
    loads=read(directory/'map-load-trace.tsv')
    refs={}
    for load in loads:
        for e in load['events']:
            if e['kind']=='animation-registration':
                path,race,mode,index=e['name'].rsplit('|',3)
                refs.setdefault(path.lower().replace('\\','/'),set()).add((int(race),int(index)))
    uses={l['label']:l for l in loads if l['label'].startswith('first-use-')}
    assert len(rows)==len(uses)==36
    paths={}
    for row in rows:
        label='first-use-%s-%s-%d'%(row['actor'],row['motion'],row['use'])
        load=uses[label]
        assert load['endpoint']=='first-use-complete'
        identity=(0 if row['actor']=='player' else 101,row['index'])
        played={e['name'].lower().replace('\\','/') for e in load['events'] if e['kind'].startswith('animation-use-')}
        target={p for p in played if identity in refs.get(p,set())}
        assert len(target)==1,(label,target)
        key=(row['actor'],row['motion'])
        if row['use']==1: paths[key]=target
        assert target==paths[key],(label,'must repeat the identical clip',target,paths[key])
        costs={c['name']:c for c in load['costs']}
        row['path']=next(iter(target))
        row['runtime_preparation_ms']=costs.get('GR2 animation preparation',{}).get('inclusive_ms',0)
        row['stages']={c['name']:dict(inclusive_ms=c['inclusive_ms'],exclusive_ms=c['exclusive_ms'],max_ms=c.get('max_ms',0),calls=c['calls']) for c in load['costs'] if c['phase']=='Animation first use'}
        row['late_file_requests']=[e for e in load['events'] if e['kind']=='file-request']
        row['late_reads']=[e for e in load['events'] if e['kind'] in ('pack-read','loose-read')]
        row['late_io_costs']=[c for c in load['costs'] if c['phase']=='File access']
        row['late_gr2_parses']=load['counters'].get('gr2-parse',0)
        row['late_decode_ms']=costs.get('GR2 section decompression',{}).get('inclusive_ms',0)
        row['late_parse_ms']=costs.get('GR2 object graph',{}).get('inclusive_ms',0)
        row['gpu_resource_calls']={name:load['counters'].get(name,0) for name in ('CreateBuffer','CreateTexture','CreateShader','CreateGraphicsPipelineState')}
        row['waits']=[c for c in load['costs'] if c['phase']=='Synchronization']
        row['allocation_requests']=load['counters'].get('first-use-key-allocation',0)
        row['allocation_bytes']=load['bytes'].get('first-use-key-allocation',0)
        row['runtime_keys']=load['bytes'].get('first-use-runtime-keys',0)
        row['clip_misses']=load['counters'].get('first-use-clip-miss',0)
        row['runtime_motion_max_ms']=row['stages'].get('runtime motion state',{}).get('max_ms')
        row['other_window_costs']=[c for c in load['costs'] if c['phase']!='Animation first use']
    result=dict(maps=[{k:l[k] for k in ('label','total_ms','first_present_ms')} for l in loads if not l['label'].startswith('first-use-')],rows=rows)
    (directory/'first-use-analysis.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return result

if __name__=='__main__':
    data=analyze(Path(sys.argv[1]))
    if '--expect-prepared' in sys.argv:
        for row in data['rows']:
            assert row['runtime_motion_max_ms'] is not None,'instrumented native build required'
            assert row['clip_misses']==row['runtime_keys']==row['allocation_requests']==row['late_gr2_parses']==0,row
            assert row['runtime_motion_max_ms']<8.33 and row['runtime_preparation_ms']<8.33,row
            assert not any(e['name'].lower().endswith('.gr2') for e in row['late_file_requests']),row
            assert row['late_decode_ms']==row['late_parse_ms']==0,row
            # Ordinary motion effects can create a small texture (e.g. dust).
            # Record those calls; the animation gate requires no late GR2 work,
            # not a blanket prohibition on unrelated effect resources.
    for row in data['rows']:
        print(row['actor'],row['motion'],row['use'],'frame',round(row['max_frame_ms'],3),'prep',round(row['runtime_preparation_ms'],3),'keys',row['runtime_keys'])
