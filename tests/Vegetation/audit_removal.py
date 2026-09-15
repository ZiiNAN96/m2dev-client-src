"""Audit production source, the transitive generated link graph, and final AMD64 binaries."""
import argparse
import hashlib
import json
import re
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path

PATTERN = re.compile(r'speedtree|cspeedtree|legacySPT|DiligentTreeRenderer|TreeRenderBridge', re.I)
SDK_MARKERS = (b'CSpeedTree', b'SpeedTreeRT', b'speedtree_static', b'GrannyReadEntireFile', b'GrannyFreeFile', b'GrannySampleModelAnimations', b'granny2.dll')

def audit(source, build, dumpbin):
    report = {'productionFiles': 0, 'productionHits': [], 'classification': {'A': [], 'B': [], 'C': [], 'D': [], 'E': []}, 'sdkArtifacts': [], 'productionProjects': [], 'buildUtilityProjects': [], 'linkHits': [], 'binaries': []}
    files = subprocess.check_output(['git', '-c', f'safe.directory={source.as_posix()}', 'ls-files', '--cached', '--others', '--exclude-standard'], cwd=source, text=True).splitlines()
    for name in sorted(set(files)):
        p = source / name
        if not p.is_file() or p.suffix.lower() in ('.lib', '.json', '.png', '.bmp', '.jpg', '.glb', '.zveg', '.spt'):
            continue
        production = name.startswith('src/') or name.startswith('extern/')
        report['productionFiles'] += production
        for line, value in enumerate(p.read_text(encoding='utf8', errors='replace').splitlines(), 1):
            if PATTERN.search(value):
                category = 'A' if production else 'B' if name.startswith('tools/Vegetation/') else 'D' if name.startswith('docs/') else 'C'
                hit = {'file': name, 'line': line, 'text': value.strip()[:240]}
                report['classification'][category].append(hit)
                if production: report['productionHits'].append(hit)
    for name in ('extern/include/SpeedTreeRT.h', 'extern/library/SpeedTree/speedtree_static.lib', 'extern/library/SpeedTree/speedtree_staticd.lib'):
        if (source/name).exists(): report['sdkArtifacts'].append(name)
    ns = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}
    pending = [build/'src/UserInterface/UserInterface.vcxproj']; seen = set()
    while pending:
        p = pending.pop().resolve()
        if p in seen: continue
        if not p.is_relative_to(build): raise ValueError(f'Project outside fresh build: {p}')
        seen.add(p); text = p.read_text(encoding='utf-8-sig'); root = ET.fromstring(text)
        if all(x.text == 'Utility' for x in root.findall('.//m:ConfigurationType', ns)):
            report['buildUtilityProjects'].append(p.relative_to(build).as_posix())
            continue  # Build-system regeneration is not a static or dynamic linker input.
        report['productionProjects'].append(str(p.relative_to(build)).replace('\\','/'))
        if PATTERN.search(text) or re.search(r'assimp|meshoptimizer|build-f34-clean', text, re.I): report['linkHits'].append(str(p))
        for ref in root.findall('.//m:ProjectReference[@Include]', ns):
            pending.append(p.parent/ref.attrib['Include'].replace('\\','/'))
    # The build graph captures static linker inputs; PE imports alone cannot prove a static SDK was removed.
    for config in ('Release', 'Debug'):
        binaries = [build/f'bin/{config}/Metin2_{config}.exe']
        binaries += list((build/f'bin/{config}').glob('*.dll'))
        binaries += list((build/f'_deps/diligentcore-build/Graphics/GraphicsEngineD3D11/{config}').glob('GraphicsEngineD3D11*.dll'))
        for p in binaries:
            headers = subprocess.check_output([str(dumpbin), '/nologo', '/headers', str(p)], text=True, errors='replace')
            imports = subprocess.check_output([str(dumpbin), '/nologo', '/imports', str(p)], text=True, errors='replace')
            data = p.read_bytes()
            dlls = sorted(set(re.findall(r'^\s+([a-zA-Z0-9_.-]+\.dll)\s*$', imports, re.M)), key=str.lower)
            forbidden = [x for x in dlls if re.search(r'speedtree|granny|^d3d[89]\.dll$|^d3dx', x, re.I)]
            markers = [x.decode() for x in SDK_MARKERS if x.lower() in data.lower()]
            report['binaries'].append({'path': p.relative_to(build).as_posix(), 'sha256': hashlib.sha256(data).hexdigest(), 'amd64': bool(re.search(r'8664\s+machine\s+\(x64\)', headers)), 'imports': dlls, 'forbiddenImports': forbidden, 'sdkMarkers': markers})
    report['pass'] = not (report['productionHits'] or report['sdkArtifacts'] or report['linkHits']) and all(b['amd64'] and not b['forbiddenImports'] and not b['sdkMarkers'] for b in report['binaries'])
    return report

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--dumpbin', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args=parser.parse_args();result=audit(args.source.resolve(),args.build.resolve(),args.dumpbin.resolve())
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
    print(json.dumps({'pass': result['pass'], 'productionFiles': result['productionFiles'], 'sourceHits': len(result['productionHits']), 'sdkArtifacts': len(result['sdkArtifacts']), 'linkProjects': len(result['productionProjects']), 'linkHits': len(result['linkHits']), 'binaries': len(result['binaries'])}))
    raise SystemExit(not result['pass'])
