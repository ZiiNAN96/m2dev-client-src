"""P0-L2: aggregate central compiler/device timings without nested double counting."""
import json
import sys
from collections import defaultdict
from pathlib import Path
from summarize_trace import read

def category(name):
    domain=name.split(' | ')[0]
    if domain.startswith('shadow'): return 'Shadow'
    if domain=='PBR' or domain=='BRDF-IBL': return 'PBR / IBL'
    if domain=='vegetation': return 'Vegetation'
    if domain=='terrain' or 'terrain' in name.lower(): return 'Terrain'
    if domain in ('water','G7-SSR-first') or 'Water' in name: return 'Water / SSR'
    if domain in ('PostFX-SSAO-first','SSAO-first') or 'ScreenSpaceAmbientOcclusion' in name or 'PostFXContext' in name: return 'SSAO / PostFX'
    if domain=='bloom-first' or 'Tone' in name or 'tone mapping' in name: return 'HDR / Bloom / Tone'
    if domain=='atmosphere-tables' or 'G56 FX' in name: return 'Sky / atmosphere'
    if 'Composite' in name or 'composition' in name: return 'Shadow / AO composition'
    return 'UI / legacy / effects'

def aggregate(costs):
    return dict(count=sum(c['calls'] for c in costs),
                inclusive_ms=sum(c['inclusive_ms'] for c in costs),
                exclusive_ms=sum(c['exclusive_ms'] for c in costs),
                max_ms=max((c.get('max_ms',0) for c in costs),default=0))

def summarize(directory):
    result=[]
    for load in read(directory/'map-load-trace.tsv'):
        costs=[c for c in load['costs'] if c['phase']=='Shaders / PSOs']
        operations=defaultdict(list)
        categories=defaultdict(lambda:defaultdict(list))
        for c in costs:
            operations[c['kind']].append(c)
            categories[category(c['name'])][c['kind']].append(c)
        result.append(dict(label=load['label'],total_ms=load['total_ms'],shader_pso_ms=load['phases'].get('Shaders / PSOs',0),
            operations={k:aggregate(v) for k,v in operations.items()},
            categories={k:{op:aggregate(v) for op,v in group.items()} for k,group in categories.items()},
            counters={k:v for k,v in load['counters'].items() if any(s in k for s in ('shader','pso','modern-vs'))},
            shaders=[c for c in costs if c['kind']=='shader-compilation'],
            fx_initialization=[c for c in costs if c['kind']=='fx-total'],
            duplicates=[e for e in load['events'] if e['kind'] in ('shader-bytecode-identical','shader-input-duplicate','pso-descriptor-duplicate')]))
    (directory/'shader-summary.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return result

if __name__=='__main__':
    for argument in sys.argv[1:]:
        directory=Path(argument)
        for load in summarize(directory):
            print(directory.name,load['label'],'total',round(load['total_ms'],3),'shader/PSO',round(load['shader_pso_ms'],3),
                  'compile',load['operations'].get('shader-compilation',{}))
