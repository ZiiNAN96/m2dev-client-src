"""C-LIB-X-C: four authored PBR textures, existing mips, bounded comparisons.

Requires the pinned FlightHelmet glTF-KTX-BasisU files in build/clibx/closure
and the same bundled Pillow/NumPy Python as texture_audit.py. No downloads or
production assets are changed. The DDS baseline is derived, not a game pack.
"""
import argparse
import copy
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import time

# Keep local helper imports from leaving generated files beside source tools.
sys.dont_write_bytecode = True

import numpy as np
from PIL import Image, ImageDraw

from texture_audit import ROOT, BASIS, AUDIT, dds_mips, rgba_dds

BASE = ROOT / "build/clibx/closure"
OUT = BASE / "textures"
COMMIT = "d7a3cc8e51d7c573771ae77a57f16b0662a905c6"
URL = f"https://raw.githubusercontent.com/KhronosGroup/glTF-Sample-Models/{COMMIT}/2.0/FlightHelmet/glTF-KTX-BasisU/"
CASES = [
    ("leather_basecolor", "LeatherPartsMat", "BaseColor", "color"),
    ("leather_normal", "LeatherPartsMat", "Normal", "normal"),
    ("leather_orm", "LeatherPartsMat", "OcclusionRoughMetal", "data"),
    ("lenses_alpha", "LensesMat", "BaseColor", "color"),
]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def save(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2, allow_nan=False) + "\n")


def ktx_info(data):
    assert data[:12] == b"\xabKTX 20\xbb\r\n\x1a\n"
    width, height = struct.unpack_from("<II", data, 20)
    mips = struct.unpack_from("<I", data, 40)[0]
    dfd = struct.unpack_from("<I", data, 48)[0]
    return dict(width=width, height=height, mips=mips, transfer=data[dfd + 14])


def package_glb():
    raw = (BASE / "FlightHelmet.gltf").read_bytes()
    source = json.loads(raw)
    doc = copy.deepcopy(source)
    assert len(doc["buffers"]) == 1
    geometry_name = doc["buffers"][0]["uri"]
    geometry = (BASE / geometry_name).read_bytes()
    assert len(geometry) == doc["buffers"][0]["byteLength"]
    binary = bytearray(geometry)
    manifest = []
    for item in doc["images"]:
        uri = item.pop("uri")
        data = (BASE / uri).read_bytes()
        binary.extend(b"\0" * (-len(binary) % 4))
        item["bufferView"] = len(doc["bufferViews"])
        item["mimeType"] = "image/ktx2"
        doc["bufferViews"].append(dict(buffer=0, byteOffset=len(binary), byteLength=len(data)))
        binary.extend(data)
        manifest.append(dict(uri=uri, sha256=sha(data), bytes=len(data), **ktx_info(data)))
    doc["buffers"] = [dict(byteLength=len(binary))]
    header = json.dumps(doc, separators=(",", ":")).encode()
    header += b" " * (-len(header) % 4)
    binary.extend(b"\0" * (-len(binary) % 4))
    glb = (struct.pack("<III", 0x46546C67, 2, 28 + len(header) + len(binary))
           + struct.pack("<II", len(header), 0x4E4F534A) + header
           + struct.pack("<II", len(binary), 0x004E4942) + binary)
    path = OUT / "FlightHelmet.closure.glb"
    path.write_bytes(glb)
    # Read back the actual container and verify every preserved object/payload.
    read = path.read_bytes()
    assert struct.unpack_from("<I", read, 8)[0] == len(read)
    jl = struct.unpack_from("<I", read, 12)[0]
    rebuilt = json.loads(read[20:20 + jl])
    bin_data = read[28 + jl:]
    assert bin_data[:len(geometry)] == geometry
    for key in source.keys() - {"images", "bufferViews", "buffers"}:
        assert source[key] == rebuilt[key], key
    assert source["bufferViews"] == rebuilt["bufferViews"][:len(source["bufferViews"]) ]
    for original, item in zip(source["images"], rebuilt["images"]):
        view = rebuilt["bufferViews"][item["bufferView"]]
        data = bin_data[view["byteOffset"]:view["byteOffset"] + view["byteLength"]]
        assert data == (BASE / original["uri"]).read_bytes()
    primitives = [p for mesh in doc["meshes"] for p in mesh["primitives"]]
    materials = []
    for material in source["materials"]:
        def texture_uri(index):
            image = source["textures"][index]["extensions"]["KHR_texture_basisu"]["source"]
            return source["images"][image]["uri"]
        pbr = material["pbrMetallicRoughness"]
        materials.append(dict(name=material["name"], alpha=material.get("alphaMode", "OPAQUE"),
                              basecolor=texture_uri(pbr["baseColorTexture"]["index"]),
                              normal=texture_uri(material["normalTexture"]["index"]),
                              rough_metal=texture_uri(pbr["metallicRoughnessTexture"]["index"])))
    result = dict(source_url=URL, commit=COMMIT, license="CC0; Microsoft FlightHelmet",
                  source_gltf_sha256=sha(raw), geometry_sha256=sha(geometry), geometry_file=geometry_name,
                  glb_sha256=sha(glb), glb_bytes=len(glb), meshes=len(doc["meshes"]),
                  primitives=len(primitives), triangles=sum(doc["accessors"][p["indices"]]["count"] // 3 for p in primitives),
                  vertices=sum(doc["accessors"][p["attributes"]["POSITION"]]["count"] for p in primitives),
                  images=manifest, materials=materials, animations=len(doc.get("animations", [])),
                  check="PASS: geometry, all 15 encoded mip chains, materials, samplers and scene unchanged",
                  scope="External authored production-near static PBR model, not a Metin2 production character; offline container/texture proof")
    save("model.json", result)
    return result


def normal_vectors(image):
    normal = np.asarray(image, dtype=np.float32)[:, :, :3] / 127.5 - 1
    return normal / np.maximum(1e-8, np.linalg.norm(normal, axis=-1, keepdims=True))


def normal_preview(image):
    return Image.fromarray(np.rint(np.clip(normal_vectors(image) * .5 + .5, 0, 1) * 255).astype(np.uint8)).convert("RGBA")


def angular(a, b):
    dot = np.sum(a * b, axis=-1)
    angles = np.degrees(np.arccos(np.clip(dot, -1, 1)))
    return dict(angle_mean_deg=float(angles.mean()), angle_p95_deg=float(np.percentile(angles, 95)), angle_max_deg=float(angles.max()))


def prepare():
    OUT.mkdir(parents=True, exist_ok=True)
    package_glb()
    rows = []
    for name, material, suffix, semantic in CASES:
        uri = f"FlightHelmet_Materials_{material}_{suffix}.ktx2"
        source = BASE / uri
        prefix = OUT / name
        args = [str(AUDIT), "--extract-baseline", str(source), str(prefix), semantic]
        extracted = json.loads(subprocess.check_output(args, cwd=ROOT, text=True))
        original = source.read_bytes()
        info = ktx_info(original)
        assert info["transfer"] == (2 if semantic == "color" else 1)
        assert info["mips"] > 1
        assert all(extracted[k] == info[k] for k in ("width", "height", "mips"))
        baseline = Path(str(prefix) + "-baseline.dds")
        data = baseline.read_bytes()
        dxgi = struct.unpack_from("<I", data, 128)[0]
        assert dxgi == {"color": 99, "normal": 98, "data": 98}[semantic]
        images = dds_mips(data)
        assert len(images) == info["mips"]
        normal_baseline = []
        if semantic == "normal":
            for mip, image in enumerate(images):
                authored = np.asarray(Image.open(str(prefix) + f"-original-mip{mip}.png"), dtype=np.float32)[:, :, :3] / 127.5 - 1
                negative = float(np.mean(authored[:, :, 2] < 0))
                authored /= np.maximum(1e-8, np.linalg.norm(authored, axis=-1, keepdims=True))
                normal_baseline.append(dict(mip=mip, negative_z_fraction=negative, **angular(authored, normal_vectors(image))))
        for mip, image in enumerate(images):
            assert image.size == (max(1, info["width"] >> mip), max(1, info["height"] >> mip))
            image.save(str(prefix) + f"-source-mip{mip}.png")
        encoder_input = Path(str(prefix) + "-input.dds")
        encoder_input.write_bytes(rgba_dds(images))
        rows.append(dict(name=name, material=material, semantic=semantic, original=uri,
                         original_sha256=sha(original), baseline=str(baseline), baseline_sha256=sha(data), baseline_dxgi=dxgi,
                         baseline_note="Derived BC7 DDS, not an existing production-pack texture; RGB normal retained including negative Z",
                         encoder_input=str(encoder_input), encoder_input_sha256=sha(encoder_input.read_bytes()),
                         input_mip_sha256=[sha(image.tobytes()) for image in images],
                         normal_baseline_projection=normal_baseline, **info))
        print("prepared", name, info, flush=True)
    save("corpus.json", rows)


def run():
    rows = json.loads((OUT / "corpus.json").read_text())
    encoding = []
    for row in rows:
        for codec in ("etc1s", "uastc"):
            prefix = OUT / f"{row['name']}-{codec}"
            assert sha(Path(row["encoder_input"]).read_bytes()) == row["encoder_input_sha256"]
            args = [str(BASIS), "-file", row["encoder_input"], "-output_file", str(prefix.with_suffix(".ktx2")), "-ktx2",
                    "-srgb" if row["semantic"] == "color" else "-linear", "-no_multithreading"]
            args += ["-etc1s", "-q", "255", "-comp_level", "1"] if codec == "etc1s" else ["-uastc", "-uastc_level", "2"]
            if row["semantic"] == "normal":
                args += ["-normal_map"]
            start = time.perf_counter()
            with prefix.with_suffix(".encode.log").open("w") as log:
                subprocess.run(args, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True)
            ms = (time.perf_counter() - start) * 1000
            target = "BC7"
            with prefix.with_suffix(".timing.jsonl").open("w") as log:
                subprocess.run([str(AUDIT), str(prefix.with_suffix(".ktx2")), row["baseline"], str(prefix), target],
                               cwd=ROOT, stdout=log, check=True)
            encoding.append(dict(case=row["name"], codec=codec, command=args, encode_process_ms=ms,
                                 encoder_input_sha256=row["encoder_input_sha256"], format=target))
            save("encoding.json", encoding)
            print("measured", row["name"], codec, target, flush=True)


def quality():
    rows = json.loads((OUT / "corpus.json").read_text())
    records = []
    sheet = Image.new("RGB", (3 * 360, 4 * 392), (25, 28, 33))
    draw = ImageDraw.Draw(sheet)
    for row_index, row in enumerate(rows):
        references = dds_mips(Path(row["baseline"]).read_bytes())
        entries = [("DDS baseline", references[0])]
        for codec in ("etc1s", "uastc"):
            prefix = OUT / f"{row['name']}-{codec}"
            data = prefix.with_suffix(".ktx2").read_bytes()
            info = ktx_info(data)
            assert all(info[k] == row[k] for k in ("width", "height", "mips", "transfer"))
            target = "BC7"
            decoded = dds_mips(Path(str(prefix) + f"-{target}.dds").read_bytes())
            assert len(decoded) == len(references)
            mip_records = []
            for mip, (ref, candidate) in enumerate(zip(references, decoded)):
                assert ref.size == candidate.size
                a, b = np.asarray(ref, dtype=np.float32), np.asarray(candidate, dtype=np.float32)
                delta = a - b
                mse = float(np.mean(delta[:, :, :3] ** 2))
                record = dict(mip=mip, width=ref.width, height=ref.height,
                              rgb_psnr_db=10 * math.log10(255 ** 2 / mse) if mse else None,
                              rgb_max_abs=float(np.abs(delta[:, :, :3]).max()),
                              alpha_rmse=float(np.sqrt(np.mean(delta[:, :, 3] ** 2))),
                              alpha_max_abs=float(np.abs(delta[:, :, 3]).max()),
                              alpha_coverage_delta_at_128=float(np.mean(b[:, :, 3] >= 128) - np.mean(a[:, :, 3] >= 128)))
                if row["semantic"] == "normal":
                    record.update(angular(normal_vectors(ref), normal_vectors(candidate)))
                    record.pop("rgb_psnr_db")  # Normal quality is directional, not RGB image quality.
                if row["semantic"] == "data":
                    for index, channel in enumerate(("occlusion", "roughness", "metallic")):
                        record[channel + "_rmse"] = float(np.sqrt(np.mean(delta[:, :, index] ** 2)))
                        record[channel + "_max_abs"] = float(np.abs(delta[:, :, index]).max())
                mip_records.append(record)
            timing = [json.loads(line) for line in prefix.with_suffix(".timing.jsonl").read_text().splitlines()]
            assert len(timing) == 2 and timing[1]["supported"] and timing[1]["format"] == target
            assert timing[1]["gpu_payload_bytes"] == Path(row["baseline"]).stat().st_size - 148
            records.append(dict(case=row["name"], codec=codec, format=target, ktx2_sha256=sha(data),
                                ktx_info=info, mip_quality=mip_records, timing=timing,
                                input_sha256=row["encoder_input_sha256"]))
            entries.append((codec + " -> " + target, decoded[0]))
        for column, (label, image) in enumerate(entries):
            if row["semantic"] == "normal":
                image = normal_preview(image)
            thumbnail = image.copy()
            thumbnail.thumbnail((352, 344), Image.Resampling.LANCZOS)
            background = Image.new("RGBA", thumbnail.size, (120, 120, 120, 255))
            background.alpha_composite(thumbnail)
            x, y = column * 360 + 4, row_index * 392
            draw.text((x, y + 4), row["name"] + " / " + label, fill="white")
            draw.text((x, y + 22), f"{row['width']} x {row['height']}, {row['mips']} existing mips", fill="white")
            sheet.paste(background.convert("RGB"), (x, y + 44))
    save("texture-closure.json", dict(model=json.loads((OUT / "model.json").read_text()), corpus=rows,
                                     encoding=json.loads((OUT / "encoding.json").read_text()), comparisons=records,
                                     checks="PASS: 8 comparisons; all existing mip dimensions and transfer functions preserved; equal GPU block bytes",
                                     scope="Offline CPU decode and numerical texture QA; no new GPU render, device or production acceptance"))
    sheet.save(OUT / "quality-contact-sheet.png")
    print("PASS: 8 comparisons; all stored mips preserved; matching linear/sRGB metadata; equal target GPU bytes", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=("prepare", "run", "quality", "all"))
    args = parser.parse_args()
    for phase in ("prepare", "run", "quality") if args.mode == "all" else (args.mode,):
        globals()[phase]()
