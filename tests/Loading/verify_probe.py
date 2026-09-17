"""Focused P0-L exit/resource checks; no broad renderer or benchmark suite."""
import json
import re
import sys
from pathlib import Path
from summarize_trace import read

directory=Path(sys.argv[1])
exit_info=json.loads((directory/'exit.json').read_text(encoding='utf-8-sig'))
assert exit_info['ExitCode']==0,exit_info
assert not (directory/'p0l-failure.log').exists()
assert (directory/'log/syserr.txt').stat().st_size==0
expected_loads=1 if '--a1-only' in sys.argv else (3 if '--shader-lifecycle' in sys.argv else 4)
assert f'completed={expected_loads}' in (directory/'p0l-smoke.log').read_text()
audit=(directory/'source-resource-audit.log').read_text()
zero_keys=('DiligentErrors DiligentFatals AllCPUDeformationCalls AllCPUDeformationVertices GPUFallbacks '
           'GraphicsSettingsObjects SourceTextures SourceBuffers SkinMeshes BoneRemaps BonePalettes '
           'SkinPreparationFailures CollisionResources VegetationAssets VegetationInstances '
           'VegetationRenderAssets VegetationGeometry VegetationInstanceBuffers VegetationInstanceBytes '
           'VegetationFailures AssetDocuments AnimationInstances MeshBindings GR2ReaderResources '
           'RuntimeSkeletons RuntimeAnimationClips IndependentAnimationInstances AnimationRuntimeFailures '
           'PrototypeGeometry PrototypePalettes').split()
for key in zero_keys:
    assert re.search(r'\b'+key+r'=0\b',audit),key
gdx=(directory/'gdx-renderer.log').read_text()
for key in ('WaterRenderers','ModernRenderers','ssrFallbacks'):
    assert re.search(r'\b'+key+r'=0\b',gdx),key
terrain=(directory/'terrain-renderer.log').read_text()
for key in ('terrain_vertices','terrain_indices','terrain_textures','object_geometry','object_textures','mount_geometry','mount_textures'):
    matches=re.findall(r'\b'+key+r'=(\d+)',terrain)
    if matches: assert matches[-1]=='0',(key,matches[-1])
assert (directory/'static-object-adapter.log').read_text().strip().endswith('live_objects=0')
failure=(directory/'renderer-failure.log').read_text()
assert failure.strip()=='Renderer failure diagnostics enabled',failure
loads=read(directory/'map-load-trace.tsv')
if '--animation-smoke' in sys.argv:
    loads=[load for load in loads if not load['label'].startswith('animation-smoke-')]
if '--animation-first-use' in sys.argv:
    loads=[load for load in loads if not load['label'].startswith('first-use-')]
assert len(loads)==expected_loads+1
for load in loads[1:]:
    assert load['endpoint']=='stable-present',load['label']
    assert load['counters'].get('grass-source',0)==0
    assert not [e for e in load['duplicates'] if e['kind'] in ('gr2-parse','glb-parse','texture-decode','area-build','registry-parse')]
result=dict(status='PASS',exit=exit_info,zero_keys=zero_keys,stable_loads=expected_loads,
            evidence_scope='Native offline map/actor/render paths; no network login or user visual acceptance')
(directory/'fast-gate.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result))
