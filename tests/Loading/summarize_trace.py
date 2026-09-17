"""Read exclusive native timings and retain the full per-file duplicate evidence."""
import json
import sys
from collections import defaultdict
from pathlib import Path

def read(path):
    loads=[]
    for line in path.read_text(encoding='latin1').split('\n'):
        line=line.rstrip('\r')
        p=line.split('\t')
        if p[0]=='LOAD':
            load=dict(label=p[1],total_ms=float(p[2]),first_present_ms=float(p[3]),frames=int(p[4]),endpoint=p[5],costs=[],events=[])
            loads.append(load)
        elif p[0]=='COST':
            load['costs'].append(dict(phase=p[1],kind=p[2],name=p[3],inclusive_ms=float(p[4]),exclusive_ms=float(p[5]),calls=int(p[6])))
            if len(p)>7: load['costs'][-1]['max_ms']=float(p[7])
        elif p[0]=='EVENT':
            load['events'].append(dict(kind=p[1],name=p[2],count=int(p[3]),bytes=int(p[4])))
    for load in loads:
        phases=defaultdict(float); kinds=defaultdict(float); counters=defaultdict(int); byte_counts=defaultdict(int)
        for c in load['costs']:
            phases[c['phase']]+=c['exclusive_ms']; kinds[c['kind']]+=c['exclusive_ms']
            c['percent']=100*c['exclusive_ms']/load['total_ms']
        for e in load['events']:
            counters[e['kind']]+=e['count']; byte_counts[e['kind']]+=e['bytes']
        assert abs(sum(phases.values())-load['total_ms'])<0.01
        assert min(c['exclusive_ms'] for c in load['costs'])>=-0.001
        load['phases']=dict(sorted(phases.items(),key=lambda p:-p[1]))
        load['kinds']=dict(kinds);load['counters']=dict(counters);load['bytes']=dict(byte_counts)
        load['costs'].sort(key=lambda c:-c['exclusive_ms'])
        load['duplicates']=[e for e in load['events'] if e['count']>1 and e['kind'] in ('gr2-parse','glb-parse','texture-decode','terrain-texture-load','pack-read','registry-parse','area-build')]
        load['duplicates'].sort(key=lambda e:-e['count'])
    return loads

if __name__=='__main__':
    directory=Path(sys.argv[1])
    loads=read(directory/'map-load-trace.tsv')
    (directory/'summary.json').write_text(json.dumps(loads,indent=2),encoding='utf-8')
    for load in loads:
        print('\n',load['label'],'TOTAL',round(load['total_ms'],2),'first present',round(load['first_present_ms'],2),load['endpoint'])
        print('PHASES', {k:round(v,2) for k,v in load['phases'].items()})
        for c in load['costs'][:10]: print(round(c['exclusive_ms'],2),round(c['percent'],1),c['phase'],c['name'],'calls',c['calls'],'inclusive',round(c['inclusive_ms'],2))
        print('COUNTERS',load['counters'])
        print('DUPLICATES',load['duplicates'][:5])
