"""Reuse G1 sphere geometry and embedded maps for eight G2 lighting comparisons."""
from pathlib import Path
import copy, json, struct
root=Path(__file__).parent
raw=(root.parent/'Materials/fixtures/material_balls.glb').read_bytes()
length=struct.unpack_from('<I',raw,12)[0]
doc=json.loads(raw[20:20+length]);offset=20+length
binary=raw[offset+8:offset+8+struct.unpack_from('<I',raw,offset)[0]]
old=doc['materials']
materials=[]
for name,metal,rough in [('A rough dielectric',0,.9),('B smooth dielectric',0,.15),('C rough metal',1,.75),('D smooth metal',1,.15)]:
    materials.append(dict(name=name,pbrMetallicRoughness=dict(baseColorFactor=[.65,.4,.16,1],metallicFactor=metal,roughnessFactor=rough)))
materials += [copy.deepcopy(old[2]),copy.deepcopy(old[5]),copy.deepcopy(old[4]),copy.deepcopy(old[0])]
for mat,name in zip(materials[4:],['E normalmapped','F material AO','G emissive','H generic matte']):mat['name']=name
materials[7]['pbrMetallicRoughness']['roughnessFactor']=.8
primitive=copy.deepcopy(doc['meshes'][0]['primitives'][0])
doc['meshes']=[];doc['nodes']=[]
for i,mat in enumerate(materials):
    p=copy.deepcopy(primitive);p['material']=i
    doc['meshes'].append(dict(name=mat['name'],primitives=[p]))
    doc['nodes'].append(dict(mesh=i,translation=[(i%4-1.5)*1.7,2.5-(i//4)*1.7,0]))
doc['materials']=materials;doc['scenes']=[dict(nodes=list(range(8)))]
doc['asset']['generator']='ZiiNAN G2-X lighting proof; unchanged G1 sphere geometry'
encoded=json.dumps(doc,separators=(',',':'),sort_keys=True).encode();encoded+=b' '*((-len(encoded))%4)
(root/'fixtures').mkdir(exist_ok=True)
output=struct.pack('<III',0x46546c67,2,28+len(encoded)+len(binary))+struct.pack('<II',len(encoded),0x4e4f534a)+encoded+struct.pack('<II',len(binary),0x004e4942)+binary
(root/'fixtures/material_balls.glb').write_bytes(output)
print('G2 material fixture:',len(output),'bytes')
