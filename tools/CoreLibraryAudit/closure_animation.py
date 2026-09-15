"""Targeted C-LIB-X-C regression; no production build or full library benchmark."""
import json
import math
import os
from pathlib import Path
import subprocess

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'build/clibx/closure'
EXE=ROOT/'build/clibx/animation-build/tools/CoreLibraryAudit/Release/CoreAnimationAudit.exe'

def run(name,rows,hz=60,poison=False,warm=False):
    manifest=OUT/(name+'.tsv');manifest.write_text('\n'.join(rows)+'\n')
    env=dict(os.environ,CLIB_CHECK_METRIC_ORDER='1')
    if poison:env['CLIB_POISON_ACL_ALLOCATIONS']='1'
    command=[str(EXE),str(ROOT.parent/'m2dev-client/assets'),str(manifest),str(hz)]
    if not warm:command.append('quality-only')
    with (OUT/(name+'.jsonl')).open('w') as output,(OUT/(name+'-errors.log')).open('w') as error:
        result=subprocess.run(command,stdout=output,stderr=error,env=env,cwd=ROOT)
    assert result.returncode==0,(name,result.returncode)
    records=[json.loads(s) for s in (OUT/(name+'.jsonl')).read_text().splitlines()]
    assert len(records)==len(rows)*(4 if warm else 1)
    def finite(value):
        if isinstance(value,float):assert math.isfinite(value),name
        elif isinstance(value,dict):
            for item in value.values():finite(item)
        elif isinstance(value,list):
            for item in value:finite(item)
    finite(records)
    return {'name':name,'poison':poison,'hz':hz,'exit':result.returncode,'records':records}

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    rows=[s for s in (ROOT/'tools/CoreLibraryAudit/animation-corpus.tsv').read_text().splitlines() if s and not s.startswith('#')]
    glb=[s for s in rows if s.startswith('glb_fixture_')]
    controls=[s for s in rows if s.split('\t')[0] in ('player_attack','mob_run','boss_attack','mount_run')]
    results=[run('fixed-glb60',glb),run('fixed-glb240',glb,240),run('fixed-gr2-controls',controls)]
    # Reuse the same six clips with different legal initial allocator contents.
    # Compare deterministic payload sizes/errors, not noisy timing fields.
    for i in range(3):results.append(run('fixed-poison-'+str(i),glb,poison=True))
    fields=['acl_bytes','acl_error','acl_quantization_error','resample_error','acl_reordered_tracks']
    reference=[{key:r[key] for key in fields} for r in results[0]['records']]
    for result in results[3:]:assert [{key:r[key] for key in fields} for r in result['records']]==reference,result['name']
    assert all(r['acl_reordered_tracks']==21 for r in results[0]['records'])
    assert all(r['acl_reordered_tracks']==0 for r in results[2]['records'])
    results.append(run('fixed-glb1-warm',[s for s in glb if s.startswith('glb_fixture_1\t')],warm=True))
    assert all(r['warm_allocations']==0 for r in results[-1]['records'] if r['kind']=='warm')
    (OUT/'animation-closure.json').write_text(json.dumps({'runs':results,'allocator_pattern_independent':True},indent=2)+'\n')
    print('PASS: 7 targeted runs; 35 clip checks, 3 warm actor-count cases; deterministic under allocator poisoning.')

if __name__=='__main__':main()
