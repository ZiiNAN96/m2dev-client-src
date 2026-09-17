"""GR2-only attribution from the opt-in native trace; no asset mutation.

GR2COST is a second view of COST, not an additional phase in LOAD totals.
"""
import argparse
import json
import statistics
from collections import defaultdict
from pathlib import Path
from summarize_trace import read


def category(path, animation):
    if animation:
        return 'Animation'
    for token, label in (('/weapon/', 'Weapon'), ('/mount/', 'Mount'),
                         ('/npc', 'NPC'), ('/monster', 'Mob'),
                         ('/pc/', 'Player'), ('/pc2/', 'Player'),
                         ('/armor/', 'Armor'), ('/effect/', 'Effect attachment'),
                         ('/tree/', 'Vegetation'), ('/building/', 'Gebaeude'),
                         ('/house/', 'Gebaeude'), ('/outdoor/', 'Props'), ('/zone/', 'Props')):
        if token in path.lower():
            return label
    return 'Sonstige'


def analyze(directory):
    loads=read(directory/'map-load-trace.tsv')
    by_label={load['label']:load for load in loads}
    details=defaultdict(list)
    for line in (directory/'map-load-trace.tsv').read_text(encoding='latin1').splitlines():
        p=line.split('\t')
        if p[0]=='LOAD':
            label=p[1]
        elif p[0]=='GR2COST':
            details[label].append(dict(path=p[1],phase=p[2],kind=p[3],name=p[4],
                                      inclusive_ms=float(p[5]),exclusive_ms=float(p[6]),calls=int(p[7])))
    results=[]
    for label,load in by_label.items():
        paths=defaultdict(lambda:dict(costs=[],events=[]))
        totals=defaultdict(lambda:dict(inclusive_ms=0.,exclusive_ms=0.,calls=0))
        for cost in details[label]:
            paths[cost['path']]['costs'].append(cost)
            for key in ('inclusive_ms','exclusive_ms','calls'):
                totals[cost['name']][key]+=cost[key]
        sections=[]
        expanded={e['name']:e['bytes'] for e in load['events'] if e['kind']=='gr2-section-expanded'}
        for e in load['events']:
            if e['kind'].startswith('gr2-section-'):
                path,index=e['name'].rsplit('|',1)
                if e['kind']=='gr2-section-compressed':
                    sections.append(dict(path=path,index=int(index),compressed=e['bytes'],expanded=expanded[e['name']]))
            if e['name'].lower().endswith('.gr2'):
                paths[e['name'].lower().replace('\\','/')]['events'].append(e)
        assets=[]
        for path,data in paths.items():
            costs={c['name']:c for c in data['costs']}
            events={e['kind']:e for e in data['events']}
            duration=lambda name:costs.get(name,{}).get('inclusive_ms',0)
            chunks=[s for s in sections if s['path']==path]
            total=sum(c['exclusive_ms'] for c in data['costs'])
            if total<0:
                raise AssertionError((path,total))
            asset=dict(path=path,total_ms=total,file_bytes=events.get('gr2-parse',{}).get('bytes',0),
                       expanded_bytes=sum(s['expanded'] for s in chunks),sections=len(chunks),
                       decompression_ms=duration('GR2 section decompression'),
                       parse_ms=duration('GR2 object graph')-duration('GR2 runtime skeleton'),
                       conversion_ms=duration('GR2 document conversion')+duration('GR2 runtime skeleton'),
                       gpu_preparation_ms=duration('GR2 GPU preparation'),costs=data['costs'],events=data['events'])
            asset['category']=category(path,events.get('gr2-animations',{}).get('bytes',0)>0)
            assets.append(asset)
        categories=defaultdict(lambda:dict(ms=0.,files=0))
        for a in assets:
            categories[a['category']]['ms']+=a['total_ms']
            categories[a['category']]['files']+=a['sections']>0
        total=sum(a['total_ms'] for a in assets)
        for cost in totals.values():
            cost['percent']=100*cost['exclusive_ms']/total if total else 0
        dec=totals['GR2 section decompression']['inclusive_ms']
        output=sum(s['expanded'] for s in sections)
        result=dict(label=label,total_load_ms=load['total_ms'],gr2_total_ms=total,
                    provider_ms=totals['GR2 parse']['inclusive_ms'],decompression_ms=dec,
                    breakdown=totals,counters=load['counters'],bytes=load['bytes'],categories=categories,
                    assets=sorted(assets,key=lambda a:-a['total_ms']),sections=sections,
                    compressed_bytes=sum(s['compressed'] for s in sections),expanded_bytes=output,
                    throughput_MBps=output/dec/1000 if dec else 0,
                    median_section_bytes=statistics.median(s['expanded'] for s in sections) if sections else 0,
                    median_nonempty_section_bytes=statistics.median(s['expanded'] for s in sections if s['expanded']) if sections else 0,
                    thread_ids=[e['name'] for e in load['events'] if e['kind']=='gr2-thread'],
                    thread_cpu_ms=load['bytes'].get('gr2-thread-cpu-100ns',0)/10000)
        results.append(result)
    (directory/'gr2-pipeline.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
    return results


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    args=parser.parse_args()
    for result in analyze(args.directory):
        print(result['label'], 'load',round(result['total_load_ms'],3),'GR2',round(result['gr2_total_ms'],3),
              'decompress',round(result['decompression_ms'],3),'thread CPU',result['thread_cpu_ms'])
        for name,cost in sorted(result['breakdown'].items(),key=lambda x:-x[1]['exclusive_ms'])[:12]:
            print(' ',name,round(cost['exclusive_ms'],3),cost['calls'])
