"""Inventory actual source SPTs and audit each in a bounded isolated SDK process."""
import argparse
import hashlib
import json
import re
import subprocess
from collections import Counter, defaultdict
from pathlib import Path

def norm(value):
    return value.replace('\\', '/').lower()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--client', type=Path, required=True)
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--probe', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--pack-audit', type=Path)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True)
    code=(a.source/'src/UserInterface/UserInterface.cpp').read_text(encoding='utf8')
    block=code.split('std::vector<std::string> packFiles = {',1)[1].split('};',1)[0]
    order=['root']+re.findall(r'^\s*"([^"]+)"',block,re.M)
    (a.output/'pack-order.txt').write_text('\n'.join(order)+'\n',encoding='utf8')
    paths=sorted(Path(p) for p in subprocess.check_output(['rg','--files',str(a.client/'assets'),'-g','*.spt'],text=True).splitlines())
    rows=[]
    for path in paths:
        rel=path.relative_to(a.client/'assets').as_posix();virtual=rel.split('/',1)[1];virtual='d:/'+virtual if virtual.startswith('ymir work/') else virtual
        data=path.read_bytes();row={'source':str(path),'relative':rel,'legacy':norm(virtual),'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest(),'header':data[:48].hex()}
        try:
            result=subprocess.run([str(a.probe),str(path)],capture_output=True,text=True,timeout=25)
            row['exitCode']=result.returncode
            if result.returncode==0:row['reference']=json.loads(result.stdout)
            else:row['error']=result.stderr[-2000:]
        except subprocess.TimeoutExpired:row['error']='SDK probe exceeded 25 seconds';row['exitCode']='timeout'
        rows.append(row)
    (a.output/'corpus.json').write_text(json.dumps({'count':len(rows),'rows':rows},indent=2)+'\n',encoding='utf8')
    print(json.dumps({'total':len(rows),'ok':sum(r['exitCode']==0 for r in rows),'failed':[r['legacy'] for r in rows if r['exitCode']!=0]}))
    if a.pack_audit:
        packed=json.loads(a.pack_audit.read_text(encoding='utf8',errors='replace'));files={r['path']:r for r in packed['files']}
        props={};refs=defaultdict(list);instances=Counter();map_counts=Counter()
        for key,item in files.items():
            if not key.endswith('.prt'):continue
            text=item['text'];tree=re.search(r'(?im)^\s*treefile\s+"([^"]+)"',text)
            if not tree:continue
            lines=text.splitlines();crc=int(lines[1].strip());props[crc]=(norm(tree[1]),key)
        for key,item in files.items():
            if key.rsplit('/',1)[-1] in ('areadata.txt','objectdata.txt'):
                for match in re.finditer(r'(?is)Start\s+Object\d+\s*(.*?)\s*End\s+Object',item['text']):
                    lines=[line.strip() for line in match[1].splitlines() if line.strip()]
                    if len(lines)<2:continue
                    try:crc=int(lines[1])
                    except ValueError:continue
                    if crc in props:
                        tree,prop=props[crc];instances[tree]+=1;map_counts[key.rsplit('/',2)[0]]+=1
                        if len(refs[tree])<8:refs[tree].append({'map':key,'property':prop,'transform':lines})
            elif key.endswith('.msm'):
                for token in re.findall(r'"([^"\n]+\.spt)"',item['text'],re.I):refs[norm(token)].append({'actor':key})
        # Event trees also enter the production factory through literal source references.
        for filename in subprocess.check_output(['rg','--files',str(a.source/'src'),'-g','*.cpp','-g','*.h'],text=True).splitlines():
            source=Path(filename)
            for line_number,line in enumerate(source.read_text(encoding='utf8',errors='replace').splitlines(),1):
                for token in re.findall(r'"(d:[^"\n]+\.spt)"',line,re.I):
                    refs[norm(token)].append({'source':source.as_posix(),'line':line_number})
        available=set(packed['availablePaths']);production=set(refs)
        for row in rows:
            key=row['legacy'];selected=files.get(key);row['productionReferenced']=key in production;row['instances']=instances[key];row['references']=refs[key]
            row['packed']=None if not selected else {k:v for k,v in selected.items() if k!='text'}
            row['textures']=[]
            if row.get('reference'):
                for kind,name in row['reference']['textures'].items():
                    if not name:continue
                    texture=norm(str(Path(key).parent/Path(name).with_suffix('.dds')))
                    row['textures'].append({'kind':kind,'path':texture,'available':texture in available})
        summary={'totalSPT':len(rows),'packedSPT':sum(k.endswith('.spt') for k in files),'converted':0,
            'sdkParsed':sum(r['exitCode']==0 for r in rows),'productionReferenced':len(production),
            'missingProductionSPT':sorted(production-set(files)),
            'sourcePackMismatches':[r['path'] for r in packed['files'] if not r['identical']],
            'productionInstances':sum(instances.values()),'maps':dict(map_counts),'properties':len(props),
            'missingTextures':[{'asset':r['legacy'],**t} for r in rows for t in r['textures'] if not t['available']],
            'rows':rows}
        (a.output/'corpus.json').write_text(json.dumps(summary,indent=2)+'\n',encoding='utf8')
        print(json.dumps({k:v for k,v in summary.items() if k!='rows'}))

if __name__=='__main__':main()
