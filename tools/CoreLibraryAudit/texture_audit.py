"""Bounded texture corpus, real pack compression, native transcode and image QA.

Run with the bundled Python containing Pillow and NumPy. All generated files
stay in build/clibx. --prepare only creates reference inputs; --run invokes
the pinned encoder and native audit sequentially, without concurrent builds.
"""
import argparse
import hashlib
import io
import json
import math
from pathlib import Path
import struct
import subprocess
import time

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
ASSETS = ROOT.parent / "m2dev-client/assets"
OUT = ROOT / "build/clibx/textures"
BASIS = ROOT / "build/clibx/deps/basis_universal-2_50/bin/basisu.exe"
AUDIT = ROOT / "build/clibx/animation-build/tools/CoreLibraryAudit/Release/CoreTextureAudit.exe"
CASES = {
    "grass": "Terrain/ymir work/terrainmaps/war/grass01.dds",
    "dirt_field": "Terrain/ymir work/terrainmaps/war/field01.dds",
    "rock": "metin2_patch_dragon_rock_texcache/ymir work/terrainmaps/dawnmistwood/dawnmistwood_rock001.dds",
    "gr2_character": "PC/ymir work/pc/warrior/warrior_4-1.dds",
    "vegetation_leaf": "Zone/ymir work/zone/b/tree/1/compositemap.dds",
    "ui": "ETC/ymir work/ui/intro/login/vkey/key_normal.tga",
}

def image_from_glb(path):
    data = path.read_bytes()
    length = struct.unpack_from("<I", data, 12)[0]
    doc = json.loads(data[20:20+length])
    view = doc["bufferViews"][doc["images"][0]["bufferView"]]
    offset = 28 + length + view.get("byteOffset", 0)
    return data[offset:offset+view["byteLength"]]

def dds_mips(data):
    header=list(struct.unpack_from("<32I",data));width=header[4];height=header[3]
    fourcc=data[84:88];extended=fourcc==b"DX10";offset=148 if extended else 128
    dxgi=struct.unpack_from("<I",data,128)[0] if offset==148 else None
    block=8 if fourcc==b"DXT1" or dxgi in (71,72) else 16
    result=[]
    for mip in range(max(1,header[7])):
        w=max(1,width>>mip);h=max(1,height>>mip);size=((w+3)//4)*((h+3)//4)*block
        single=header.copy();single[3]=h;single[4]=w;single[5]=size;single[7]=1
        isolated=struct.pack("<32I",*single)+(data[128:148] if extended else b"")+data[offset:offset+size]
        result.append(Image.open(io.BytesIO(isolated)).convert("RGBA"));offset+=size
    if offset!=len(data): raise ValueError("DDS payload/mip size mismatch")
    return result

def rgba_dds(images):
    h=[0]*32;h[0]=0x20534444;h[1]=124;h[2]=0x2100F;h[3]=images[0].height;h[4]=images[0].width
    h[5]=images[0].width*4;h[7]=len(images);h[19]=32;h[20]=0x41;h[22]=32
    h[23:27]=[0xff,0xff00,0xff0000,0xff000000];h[27]=0x401008
    return struct.pack("<32I",*h)+b"".join(image.tobytes() for image in images)

def prepare():
    OUT.mkdir(parents=True, exist_ok=True)
    rows = []
    for name, relative in CASES.items():
        path = ASSETS / relative
        data = path.read_bytes()
        image = Image.open(path).convert("RGBA")
        image.save(OUT / f"{name}-source.png")
        dds = data[:4] == b"DDS "
        encoder_input=OUT/f"{name}-input.dds" if dds else OUT/f"{name}-source.png"
        reference_mips=dds_mips(data) if dds else [image]
        if dds: encoder_input.write_bytes(rgba_dds(reference_mips))
        for mip,reference in enumerate(reference_mips): reference.save(OUT/f"{name}-source-mip{mip}.png")
        row = {"name": name, "source": str(path), "sha256": hashlib.sha256(data).hexdigest(),
               "encoder_input":str(encoder_input),
               "width": image.width, "height": image.height, "source_bytes": len(data),
               "source_mips": max(1, struct.unpack_from("<I", data, 28)[0]) if dds else 1,
               "source_fourcc": data[84:88].decode("ascii", errors="replace") if dds else None,
               "real_asset": True, "linear": False}
        # DDS payload size excludes its container, not GPU allocation alignment.
        row["source_gpu_payload_bytes"] = len(data)-128 if dds else image.width*image.height*4
        rows.append(row)
    glb = ROOT / "tests/AssetRuntime/fixtures/f5x/character.glb"
    embedded = image_from_glb(glb)
    path = OUT / "glb_basecolor_fixture-source.png"
    image = Image.open(io.BytesIO(embedded)).convert("RGBA"); image.save(path)
    rows.append({"name":"glb_basecolor_fixture", "source":str(path), "parent_glb":str(glb),
                 "sha256":hashlib.sha256(path.read_bytes()).hexdigest(), "width":image.width,"height":image.height,
                 "source_bytes":path.stat().st_size,"source_mips":1,"real_asset":False,"linear":False,
                 "source_gpu_payload_bytes":image.width*image.height*4})
    # Supplemental semantic probe, explicitly NOT a real production normal map.
    y, x = np.mgrid[:64,:64].astype(float)
    normal = np.stack((.45*np.sin(x/8), .45*np.cos(y/8), np.ones_like(x)), axis=-1)
    normal /= np.linalg.norm(normal, axis=-1, keepdims=True)
    rgb = np.rint((normal*.5+.5)*255).astype(np.uint8)
    image = Image.fromarray(rgb).convert("RGBA");path=OUT/"normal_semantic_probe-source.png";image.save(path)
    rows.append({"name":"normal_semantic_probe","source":str(path),"sha256":hashlib.sha256(path.read_bytes()).hexdigest(),
                 "width":64,"height":64,"source_bytes":path.stat().st_size,"source_mips":1,"real_asset":False,"linear":True,
                 "source_gpu_payload_bytes":64*64*4})
    (OUT/"corpus.json").write_text(json.dumps(rows,indent=2)+"\n")
    print(json.dumps(rows, indent=2))

def run(only=None):
    rows=json.loads((OUT/"corpus.json").read_text())
    encoding=json.loads((OUT/"encoding.json").read_text()) if only and (OUT/"encoding.json").exists() else []
    if only:
        rows=[row for row in rows if row["name"]==only]
        encoding=[row for row in encoding if row["case"]!=only]
    for row in rows:
        name=row["name"]
        for codec in ("etc1s", "uastc"):
            prefix=OUT/f"{name}-{codec}"
            # Decode original DDS blocks offline into a lossless RGBA DDS
            # retaining every stored mip; upstream encoder rejects BC input.
            args=[str(BASIS),"-file",row.get("encoder_input",row["source"]),"-output_file",str(prefix.with_suffix(".ktx2")),"-ktx2",
                  "-linear" if row["linear"] else "-srgb", "-no_multithreading"]
            args += ["-etc1s","-q","255","-comp_level","1"] if codec=="etc1s" else ["-uastc","-uastc_level","2"]
            if row["linear"]: args += ["-normal_map","-separate_rg_to_color_alpha"]
            started=time.perf_counter()
            with prefix.with_suffix(".encode.log").open("w") as log:
                subprocess.run(args,stdout=log,stderr=subprocess.STDOUT,check=True,cwd=ROOT)
            encoding.append({"case":name,"codec":codec,"encode_process_ms":(time.perf_counter()-started)*1000,"command":args})
            with prefix.with_suffix(".timing.jsonl").open("w") as log:
                subprocess.run([str(AUDIT),str(prefix.with_suffix(".ktx2")),row["source"],str(prefix)],stdout=log,check=True,cwd=ROOT)
            print(name,codec,"done",flush=True)
    (OUT/"encoding.json").write_text(json.dumps(encoding,indent=2)+"\n")

def quality():
    rows=json.loads((OUT/"corpus.json").read_text()); results=[]
    sheet=Image.new("RGB",(4*260,len(rows)*286),(25,28,33));draw=ImageDraw.Draw(sheet)
    for i,row in enumerate(rows):
        name=row["name"];source=Image.open(OUT/f"{name}-source.png").convert("RGBA");a=np.array(source).astype(float)
        entries=[("Current",source)]
        for codec in ("etc1s","uastc"):
            prefix=OUT/f"{name}-{codec}"
            ktx=prefix.with_suffix(".ktx2").read_bytes()
            levels=struct.unpack_from("<I",ktx,40)[0];dfd=struct.unpack_from("<I",ktx,48)[0]
            if levels!=row["source_mips"]: raise ValueError("Stored mip count changed")
            if ktx[dfd+14]!=(1 if row["linear"] else 2): raise ValueError("Incorrect KTX2 transfer function")
            target="BC5" if row["linear"] else "BC3" if np.any(a[:,:,3]!=255) else "BC1"
            for fmt in sorted({target} if row["linear"] else {target,"BC7"}):
                image=Image.open(str(prefix)+f"-{fmt}.dds").convert("RGBA");b=np.array(image).astype(float)
                mse=float(np.mean((a[:,:,:3]-b[:,:,:3])**2));alpha_mse=float(np.mean((a[:,:,3]-b[:,:,3])**2))
                result={"case":name,"codec":codec,"format":fmt,"real_asset":row["real_asset"],
                    "rgb_psnr_db":10*math.log10(255*255/mse) if mse else None,"rgb_max_abs":float(np.abs(a[:,:,:3]-b[:,:,:3]).max()),
                    "alpha_rmse":math.sqrt(alpha_mse),"alpha_coverage_delta_at_128":float(np.mean(b[:,:,3]>=128)-np.mean(a[:,:,3]>=128))}
                mip_results=[]
                for mip,decoded in enumerate(dds_mips(Path(str(prefix)+f"-{fmt}.dds").read_bytes())):
                    refpath=OUT/f"{name}-source-mip{mip}.png"
                    ref=Image.open(refpath).convert("RGBA") if refpath.exists() else source
                    if ref.size!=decoded.size: raise ValueError("Mip dimensions differ")
                    ma=np.array(ref).astype(float);mb=np.array(decoded).astype(float);mmse=float(np.mean((ma[:,:,:3]-mb[:,:,:3])**2))
                    mip_results.append({"mip":mip,"rgb_psnr_db":10*math.log10(255*255/mmse) if mmse else None,
                                        "alpha_coverage_delta_at_128":float(np.mean(mb[:,:,3]>=128)-np.mean(ma[:,:,3]>=128))})
                result["mips"]=mip_results
                if row["linear"]:
                    na=a[:,:,:3]/127.5-1;nb=b[:,:,:3]/127.5-1
                    if fmt=="BC5": nb[:,:,2]=np.sqrt(np.maximum(0,1-nb[:,:,0]**2-nb[:,:,1]**2))
                    na/=np.maximum(1e-8,np.linalg.norm(na,axis=-1,keepdims=True));nb/=np.maximum(1e-8,np.linalg.norm(nb,axis=-1,keepdims=True))
                    angles=np.degrees(np.arccos(np.clip(np.sum(na*nb,axis=-1),-1,1)))
                    result.update(normal_angle_mean=float(angles.mean()),normal_angle_max=float(angles.max()))
                results.append(result)
                if fmt==target:
                    preview=image
                    if row["linear"]: preview=Image.fromarray(np.rint(np.clip(nb*.5+.5,0,1)*255).astype(np.uint8)).convert("RGBA")
                    entries.append((f"{codec} -> {fmt}",preview))
            # Additional high quality BC7 preview for both color and normal probes.
            if codec=="uastc" and not row["linear"]: entries.append(("UASTC -> BC7",Image.open(str(prefix)+"-BC7.dds").convert("RGBA")))
        for c,(label,image) in enumerate(entries[:4]):
            thumb=image.copy();thumb.thumbnail((252,248),Image.Resampling.NEAREST)
            background=Image.new("RGBA",thumb.size,(110,110,110,255));background.alpha_composite(thumb)
            sheet.paste(background.convert("RGB"),(c*260+4,i*286+30));draw.text((c*260+4,i*286+4),name+" / "+label,fill="white")
    (OUT/"quality.json").write_text(json.dumps(results,indent=2)+"\n");sheet.save(OUT/"quality-contact-sheet.png")
    print(json.dumps(results,indent=2))

if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("mode",choices=["prepare","run","quality"]);parser.add_argument("--only")
    args=parser.parse_args()
    if args.mode=="run": run(args.only)
    else: globals()[args.mode]()
