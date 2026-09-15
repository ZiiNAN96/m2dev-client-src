"""F5-X: original, deterministic articulated character, CC0; standard library only.

Run with Python 3. Generates a GLB with 19 joints, two textured primitives,
six named TRS clips, and normal character/motion definitions. No third-party art.
Coordinates are glTF meters, +Y up, character front +Z. Animation slots are IDs;
localized clip names deliberately do not match gameplay motion names.
"""
import json
import math
from pathlib import Path
import struct
import zlib

OUT = Path(__file__).parent / "fixtures" / "f5x"


class GLB:
    def __init__(self):
        self.binary = bytearray()
        self.doc = {"asset": {"version": "2.0", "generator": "ZiiNAN F5-X deterministic character v1",
                              "copyright": "Original procedural fixture, CC0-1.0"},
                    "bufferViews": [], "accessors": []}

    def view(self, data):
        self.binary.extend(b"\0" * (-len(self.binary) % 4))
        index = len(self.doc["bufferViews"])
        self.doc["bufferViews"].append({"buffer": 0, "byteOffset": len(self.binary), "byteLength": len(data)})
        self.binary.extend(data)
        return index

    def accessor(self, values, shape, component=5126):
        width = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[shape]
        flat = [v for row in values for v in row] if width > 1 else values
        data = struct.pack("<" + {5126: "f", 5123: "H", 5121: "B"}[component] * len(flat), *flat)
        item = {"bufferView": self.view(data), "componentType": component, "count": len(values), "type": shape}
        if shape in ("SCALAR", "VEC3") and component == 5126:
            rows = values if width > 1 else [[v] for v in values]
            item.update(min=[min(row[i] for row in rows) for i in range(width)],
                        max=[max(row[i] for row in rows) for i in range(width)])
        self.doc["accessors"].append(item)
        return len(self.doc["accessors"]) - 1

    def save(self, path):
        self.doc["buffers"] = [{"byteLength": len(self.binary)}]
        encoded = json.dumps(self.doc, separators=(",", ":"), ensure_ascii=True).encode()
        encoded += b" " * (-len(encoded) % 4)
        payload = self.binary + b"\0" * (-len(self.binary) % 4)
        path.write_bytes(struct.pack("<III", 0x46546C67, 2, 28 + len(encoded) + len(payload)) +
                         struct.pack("<II", len(encoded), 0x4E4F534A) + encoded +
                         struct.pack("<II", len(payload), 0x004E4942) + payload)


def png():
    def chunk(name, data):
        return struct.pack(">I", len(data)) + name + data + struct.pack(">I", zlib.crc32(name + data))
    rows = b"".join(b"\0" + b"".join(bytes((245, 245, 245, 255) if (x // 2 + y // 2) % 2 else
                                             (120, 165, 200, 255)) for x in range(8)) for y in range(8))
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 8, 8, 8, 6, 0, 0, 0)) + \
        chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")


def quaternion(axis, degrees):
    half = math.radians(degrees) / 2
    return [axis[0] * math.sin(half), axis[1] * math.sin(half), axis[2] * math.sin(half), math.cos(half)]


def generate():
    OUT.mkdir(parents=True, exist_ok=True)
    glb = GLB()
    # Parent indices are stable source IDs, not sorted names.
    bones = [("Pelvis", -1, (0, .9, 0)), ("Spine", 0, (0, 1.12, 0)),
             ("Chest", 1, (0, 1.38, 0)), ("Neck", 2, (0, 1.53, 0)), ("Head", 3, (0, 1.65, 0)),
             ("LeftShoulder", 2, (-.24, 1.42, 0)), ("LeftArm", 5, (-.32, 1.38, 0)),
             ("LeftForearm", 6, (-.39, 1.04, 0)), ("LeftHand", 7, (-.42, .75, 0)),
             ("RightShoulder", 2, (.24, 1.42, 0)), ("RightArm", 9, (.32, 1.38, 0)),
             ("RightForearm", 10, (.39, 1.04, 0)), ("RightHand", 11, (.42, .75, 0)),
             ("LeftThigh", 0, (-.14, .88, 0)), ("LeftShin", 13, (-.14, .49, 0)),
             ("LeftFoot", 14, (-.14, .1, .025)), ("RightThigh", 0, (.14, .88, 0)),
             ("RightShin", 16, (.14, .49, 0)), ("RightFoot", 17, (.14, .1, .025))]
    nodes = []
    for i, (name, parent, pos) in enumerate(bones):
        local = [pos[k] - bones[parent][2][k] for k in range(3)] if parent >= 0 else list(pos)
        nodes.append({"name": name, "translation": local})
        if parent >= 0:
            nodes[parent].setdefault("children", []).append(i)
    # A transformed mesh node is intentionally ignored by skinning per glTF.
    nodes += [{"name": "CharacterMesh", "mesh": 0, "skin": 0, "translation": [7, 8, 9]},
              {"name": "ModelRoot", "children": [0, 19]}]
    glb.doc.update(nodes=nodes, scene=0, scenes=[{"nodes": [20]}])
    inverse = []
    for _, _, pos in bones:
        inverse.append([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -pos[0], -pos[1], -pos[2], 1])
    glb.doc["skins"] = [{"name": "F5XHumanoid", "skeleton": 0, "joints": list(range(19)),
                         "inverseBindMatrices": glb.accessor(inverse, "MAT4")}]
    groups = [{key: [] for key in ("positions", "normals", "uv", "joints", "weights", "indices")} for _ in range(2)]

    def box(center, size, joint, other=None, group=0):
        g = groups[group]
        # Outward CCW faces with explicit per-face normals and UVs.
        corners = [(-1, -1, -1), (1, -1, -1), (1, 1, -1), (-1, 1, -1),
                   (-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)]
        faces = [((0, 3, 2, 1), (0, 0, -1)), ((4, 5, 6, 7), (0, 0, 1)),
                 ((0, 4, 7, 3), (-1, 0, 0)), ((1, 2, 6, 5), (1, 0, 0)),
                 ((0, 1, 5, 4), (0, -1, 0)), ((3, 7, 6, 2), (0, 1, 0))]
        for face, normal in faces:
            base = len(g["positions"])
            for c, uv in zip(face, ((0, 1), (0, 0), (1, 0), (1, 1))):
                point = [center[k] + size[k] * corners[c][k] / 2 for k in range(3)]
                weight = .15 if other is not None and corners[c][1] > 0 else 0
                g["positions"].append(point); g["normals"].append(normal); g["uv"].append(uv)
                g["joints"].append([joint, other if other is not None else 0, 0, 0])
                g["weights"].append([1 - weight, weight, 0, 0])
            g["indices"].extend(base + i for i in (0, 1, 2, 0, 2, 3))

    box((0, .94, 0), (.37, .22, .23), 0)
    box((0, 1.17, 0), (.40, .27, .22), 1, 0)
    box((0, 1.40, 0), (.47, .23, .24), 2, 1)
    box((0, 1.55, 0), (.13, .13, .13), 3, 2, 1)
    box((0, 1.70, .015), (.27, .25, .24), 4, 3)
    # A thin contrasting face plate makes front/back orientation obvious.
    box((0, 1.71, .144), (.22, .105, .02), 4, group=1)
    for side, arm, forearm, hand, thigh, shin, foot in ((-1, 6, 7, 8, 13, 14, 15), (1, 10, 11, 12, 16, 17, 18)):
        box((side * .355, 1.215, 0), (.14, .31, .16), arm, bones[arm][1], 1)
        box((side * .405, .89, 0), (.12, .28, .14), forearm, arm)
        box((side * .42, .71, -.005), (.14, .13, .17), hand, forearm, 1)
        box((side * .14, .685, 0), (.18, .38, .21), thigh, 0, 1)
        box((side * .14, .295, 0), (.15, .37, .17), shin, thigh)
        box((side * .14, .085, .065), (.19, .13, .30), foot, shin, 1)
    primitives = []
    for index, group in enumerate(groups):
        attrs = {"POSITION": glb.accessor(group["positions"], "VEC3"),
                 "NORMAL": glb.accessor(group["normals"], "VEC3"), "TEXCOORD_0": glb.accessor(group["uv"], "VEC2"),
                 "JOINTS_0": glb.accessor(group["joints"], "VEC4", 5121), "WEIGHTS_0": glb.accessor(group["weights"], "VEC4")}
        primitives.append({"attributes": attrs, "indices": glb.accessor(group["indices"], "SCALAR", 5123), "material": index})
    glb.doc["meshes"] = [{"name": "Original articulated test humanoid", "primitives": primitives}]
    glb.doc["images"] = [{"bufferView": glb.view(png()), "mimeType": "image/png"}]
    glb.doc["textures"] = [{"source": 0}]
    glb.doc["materials"] = [{"name": name, "pbrMetallicRoughness": {"baseColorTexture": {"index": 0},
                              "baseColorFactor": color, "metallicFactor": 0, "roughnessFactor": 1}}
                             for name, color in (("Orange shell", [1, .55, .12, 1]), ("Blue joints", [.20, .45, .85, 1]))]
    names = ["Ruhe", "Gehen", "Laufen", "Schlag", "Treffer", "Fallen"]
    durations = [2., 1.2, .7, .8, .55, 1.3]
    animations = []
    for index, (name, duration) in enumerate(zip(names, durations)):
        animation = {"name": name, "samplers": [], "channels": []}
        times = [0, duration * .25, duration * .5, duration * .75, duration]
        time_accessor = glb.accessor(times, "SCALAR")

        def channel(bone, path, values, interpolation="LINEAR"):
            slot = len(animation["samplers"])
            animation["samplers"].append({"input": time_accessor, "output": glb.accessor(values, "VEC4" if path == "rotation" else "VEC3"), "interpolation": interpolation})
            animation["channels"].append({"sampler": slot, "target": {"node": bone, "path": path}})

        bob = .015 if index == 0 else .025 if index in (1, 2) else 0
        channel(0, "translation", [[0, .9 + bob * math.sin(k * math.pi / 2), 0] for k in range(5)])
        channel(2, "scale", [[1, 1 + (.015 if index == 0 else .005) * math.sin(k * math.pi / 2), 1] for k in range(5)])
        if index == 0:
            channel(2, "rotation", [quaternion((0, 0, 1), a) for a in (0, 2, 0, -2, 0)])
        elif index in (1, 2):
            amplitude = 24 if index == 1 else 43
            for bone, sign in ((13, 1), (16, -1), (6, -1), (10, 1)):
                channel(bone, "rotation", [quaternion((1, 0, 0), sign * amplitude * v) for v in (0, 1, 0, -1, 0)])
            for bone, sign in ((14, 1), (17, -1)):
                channel(bone, "rotation", [quaternion((1, 0, 0), max(0, sign * v) * amplitude) for v in (0, 1, 0, -1, 0)])
        elif index == 3:
            channel(10, "rotation", [quaternion((1, 0, 0), a) for a in (0, -65, 35, 10, 0)])
            channel(11, "rotation", [quaternion((1, 0, 0), a) for a in (0, -75, -15, -5, 0)])
            channel(2, "rotation", [quaternion((0, 1, 0), a) for a in (0, -15, 20, 5, 0)])
        elif index == 4:
            channel(1, "rotation", [quaternion((1, 0, 0), a) for a in (0, -22, -12, -4, 0)])
        else:
            channel(0, "rotation", [quaternion((1, 0, 0), a) for a in (0, -20, -55, -85, -90)])
            # Actual root translation remains captured in the pose, never gameplay motion.
            animation["channels"].pop(0); animation["samplers"][0]["output"] = glb.accessor([[0, y, 0] for y in (.9, .75, .45, .22, .16)], "VEC3")
            animation["channels"].insert(0, {"sampler": 0, "target": {"node": 0, "path": "translation"}})
        animations.append(animation)
    glb.doc["animations"] = animations
    glb.save(OUT / "character.glb")
    prop = GLB()
    groups[0] = {key: [] for key in ("positions", "normals", "uv", "joints", "weights", "indices")}
    box((0, -.23, 0), (.065, .52, .065), 0)
    geometry = groups[0]
    prop.doc.update(scene=0, scenes=[{"nodes": [0]}], nodes=[{"mesh": 0, "name": "HandProp"}],
                    materials=[{"pbrMetallicRoughness": {"baseColorFactor": [.1, 1, .8, 1]}}],
                    meshes=[{"primitives": [{"attributes": {"POSITION": prop.accessor(geometry["positions"], "VEC3"),
                              "NORMAL": prop.accessor(geometry["normals"], "VEC3"), "TEXCOORD_0": prop.accessor(geometry["uv"], "VEC2")},
                              "indices": prop.accessor(geometry["indices"], "SCALAR", 5123), "material": 0}]}])
    prop.save(OUT / "hand_prop.glb")
    for index, name in enumerate(("idle", "walk", "run", "attack", "damage", "death")):
        (OUT / (name + ".msa")).write_text('ScriptType MotionData\nMotionFileName "f5x/character.glb"\nMotionClipIndex %d\nMotionDuration %.6f\n' % (index, durations[index]))
    (OUT / "character.msm").write_text('ScriptType RaceData\nBaseModelFileName "f5x/character.glb"\n')
    (OUT / "motions.json").write_text(json.dumps({name.upper(): {"clip": i, "name": names[i], "loop": i < 3}
                                                for i, name in enumerate(("idle", "walk", "run", "attack", "damage", "death"))}, indent=2) + "\n")
    print("Generated", OUT / "character.glb", (OUT / "character.glb").stat().st_size, "bytes")


if __name__ == "__main__":
    generate()
