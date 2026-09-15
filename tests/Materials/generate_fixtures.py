"""Small deterministic G1-X material proof; no production asset modifications."""
import json, math, pathlib, struct, zlib

ROOT = pathlib.Path(__file__).parent / "fixtures"
ROOT.mkdir(exist_ok=True)

def png(pixels, width=8, height=8):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    rows = b"".join(b"\0" + bytes(pixels[y*width*4:(y+1)*width*4]) for y in range(height))
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width,height,8,6,0,0,0)) + chunk(b"IDAT",zlib.compress(rows,9)) + chunk(b"IEND",b"")

binary=bytearray(); views=[]; accessors=[]; images=[]; meshes=[]; nodes=[]
def view(data):
    while len(binary)%4: binary.append(0)
    offset=len(binary); binary.extend(data)
    views.append(dict(buffer=0,byteOffset=offset,byteLength=len(data)))
    return len(views)-1
def accessor(values, kind, components):
    flat=[v for row in values for v in row] if components>1 else values
    index=view(struct.pack("<"+("f" if kind==5126 else "H")*len(flat),*flat))
    a=dict(bufferView=index,componentType=kind,count=len(values),type={1:"SCALAR",2:"VEC2",3:"VEC3",4:"VEC4"}[components])
    if components==3 and kind==5126:a.update(min=[min(v[i] for v in values) for i in range(3)],max=[max(v[i] for v in values) for i in range(3)])
    accessors.append(a);return len(accessors)-1

for name, pixel in [
    ("normal",lambda x,y:(200 if x<4 else 56,128,230,255)),
    ("metal_rough",lambda x,y:(24,40 if x<4 else 220,255,255)),
    ("emissive",lambda x,y:(255,128 if y<4 else 255,64,255)),
    ("occlusion",lambda x,y:(32 if (x//2+y//2)%2 else 255,200,77,255)),
]:
    data=png([c for y in range(8) for x in range(8) for c in pixel(x,y)])
    (ROOT/(name+".png")).write_bytes(data)
    images.append(dict(name=name,mimeType="image/png",bufferView=view(data)))

materials=[
    dict(name="A nonmetal rough",pbrMetallicRoughness=dict(baseColorFactor=[.65,.3,.12,1],metallicFactor=0,roughnessFactor=.9)),
    dict(name="B metal smooth",pbrMetallicRoughness=dict(baseColorFactor=[.95,.65,.2,1],metallicFactor=1,roughnessFactor=.17)),
    dict(name="C normal mapped",pbrMetallicRoughness=dict(baseColorFactor=[.5,.55,.65,1],metallicFactor=0,roughnessFactor=.5),normalTexture=dict(index=0,scale=.85)),
    dict(name="D roughness variation",pbrMetallicRoughness=dict(baseColorFactor=[.75,.75,.75,1],metallicFactor=.8,roughnessFactor=1,metallicRoughnessTexture=dict(index=1))),
    dict(name="E emissive",pbrMetallicRoughness=dict(baseColorFactor=[.07,.04,.03,1],metallicFactor=0,roughnessFactor=.8),emissiveFactor=[.9,.18,.02],emissiveTexture=dict(index=2)),
    dict(name="F material AO",pbrMetallicRoughness=dict(baseColorFactor=[.6,.6,.6,1],metallicFactor=0,roughnessFactor=.8),occlusionTexture=dict(index=3,strength=.9)),
]
for index in range(6):
    p=[];n=[];uv=[];t=[];indices=[];segments=24;rings=16
    for y in range(rings+1):
        theta=math.pi*y/rings
        for x in range(segments+1):
            phi=2*math.pi*x/segments
            normal=(math.sin(theta)*math.cos(phi),math.cos(theta),math.sin(theta)*math.sin(phi))
            n.append(normal);p.append(tuple(.66*v for v in normal));uv.append((x/segments,y/rings))
            t.append((-math.sin(phi),0,math.cos(phi),1))
    for y in range(rings):
        for x in range(segments):
            a=y*(segments+1)+x;b=a+segments+1
            if y:indices.extend((a,a+1,b))
            if y<rings-1:indices.extend((a+1,b+1,b))
    attrs=dict(POSITION=accessor(p,5126,3),NORMAL=accessor(n,5126,3),TEXCOORD_0=accessor(uv,5126,2),TANGENT=accessor(t,5126,4))
    meshes.append(dict(name=materials[index]["name"],primitives=[dict(attributes=attrs,indices=accessor(indices,5123,1),material=index)]))
    nodes.append(dict(mesh=index,translation=[(index%3-1)*1.7,2.5-(index//3)*1.7,0]))
doc=dict(asset=dict(version="2.0",generator="ZiiNAN G1-X deterministic material proof"),scene=0,scenes=[dict(nodes=list(range(6)))],nodes=nodes,meshes=meshes,
         buffers=[dict(byteLength=len(binary))],bufferViews=views,accessors=accessors,images=images,textures=[dict(source=i) for i in range(4)],materials=materials)
def write_glb(path, doc, binary):
    data=json.dumps(doc,separators=(",",":"),sort_keys=True).encode();data+=b" "*((-len(data))%4)
    binary=bytes(binary)+b"\0"*((-len(binary))%4)
    path.write_bytes(struct.pack("<III",0x46546c67,2,28+len(data)+len(binary))+struct.pack("<II",len(data),0x4e4f534a)+data+struct.pack("<II",len(binary),0x004e4942)+binary)
write_glb(ROOT/"material_balls.glb",doc,binary)

# Existing F5-X animated character geometry/skin/animation bytes unchanged, one PBR material.
original=pathlib.Path(__file__).parents[1]/"AssetRuntime/fixtures/f5x/character.glb"
raw=original.read_bytes();length=struct.unpack_from("<I",raw,12)[0]
character=json.loads(raw[20:20+length]);start=20+length
binlen=struct.unpack_from("<I",raw,start)[0];character_bin=raw[start+8:start+8+binlen]
normal_bytes=(ROOT/"normal.png").read_bytes()
normal_view=len(character['bufferViews'])
character['bufferViews'].append(dict(buffer=0,byteOffset=len(character_bin),byteLength=len(normal_bytes)))
normal_image=len(character.setdefault('images',[]))
character['images'].append(dict(bufferView=normal_view,mimeType='image/png'))
normal_texture=len(character.setdefault('textures',[]))
character['textures'].append(dict(source=normal_image))
character_bin+=normal_bytes
character['buffers'][0]['byteLength']=len(character_bin)
for mat in character.get("materials",[]):
    mat['normalTexture']=dict(index=normal_texture,scale=.6)
    mat.setdefault("pbrMetallicRoughness",{}).update(metallicFactor=.55,roughnessFactor=.28)
write_glb(ROOT/"character_pbr.glb",character,character_bin)
print("Generated six material balls and one existing animated-character material variant")

(ROOT/"hand_prop.glb").write_bytes((original.parent/"hand_prop.glb").read_bytes())
