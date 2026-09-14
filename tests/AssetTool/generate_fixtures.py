"""Original tiny E2-X source assets; deterministic, Python standard library only.

No third-party model data. Regeneration is optional; tests use checked-in fixtures.
OBJ uses meters, Y up, front +Z. DAE and FBX declare centimeters explicitly.
"""
import pathlib
import struct
import zlib

OUT = pathlib.Path(__file__).parent / "fixtures"


def write(name, text):
    with (OUT / name).open("w", encoding="utf-8", newline="\n") as stream:
        stream.write(text)


def texture():
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    rows = bytearray()
    for y in range(16):
        rows.append(0)
        for x in range(16):
            # Asymmetric blue/white corner and golden timber grain expose UV orientation.
            color = (30, 85, 190) if x < 4 and y < 4 else ((240, 226, 180) if x > 11 and y < 4 else ((162, 102, 47) if y % 4 else (91, 48, 24)))
            rows.extend(color)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 16, 16, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(bytes(rows))) + chunk(b"IEND", b"")


def stall():
    lines = ["# Original ZiiNAN E2-X stall; meters, Y up, +Z front", "mtllib market_stall.mtl"]
    vertex = 0
    faces = [((-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)), ((1,-1,-1),(-1,-1,-1),(-1,1,-1),(1,1,-1)),
             ((1,-1,1),(1,-1,-1),(1,1,-1),(1,1,1)), ((-1,-1,-1),(-1,-1,1),(-1,1,1),(-1,1,-1)),
             ((-1,1,1),(1,1,1),(1,1,-1),(-1,1,-1)), ((-1,-1,-1),(1,-1,-1),(1,-1,1),(-1,-1,1))]
    def box(name, position, size, material):
        nonlocal vertex
        lines.extend(["o " + name, "usemtl " + material])
        for face in faces:
            for point, uv in zip(face, [(0,0),(1,0),(1,1),(0,1)]):
                lines.append("v " + " ".join(f"{p + v*s/2:.5f}" for p,v,s in zip(position,point,size)))
                lines.append(f"vt {uv[0]} {uv[1]}")
            # Real source quads, no normals: importer must triangulate and generate normals.
            lines.append("f " + " ".join(f"{vertex+i}/{vertex+i}" for i in (1,2,3,4)))
            vertex += 4
    box("counter", (0,.74,0), (2.4,.2,1.15), "wood")
    for x in (-1,1):
        for z in (-.48,.48):
            box(f"post_{x}_{z}", (x,1.18,z), (.13,2.36,.13), "wood")
    box("lower_crossbar", (0,.2,-.48), (2.0,.13,.12), "wood")
    box("awning", (0,2.38,0), (2.9,.18,1.6), "red_canvas")
    for x in (-1.35,-.9,-.45,0,.45,.9,1.35):
        box(f"awning_stripe_{x}", (x,2.29,.79), (.2,.25,.035), "cream_canvas")
    for x in (-.65,.05,.72):
        box(f"crate_{x}", (x,.98,.08), (.52,.29,.63), "wood")
        box(f"produce_{x}", (x,1.15,.08), (.42,.14,.50), "green_produce")
    box("shop_sign", (0,1.91,.64), (1.30,.43,.04), "cream_canvas")
    write("market_stall.obj", "\n".join(lines) + "\n")
    write("market_stall.mtl", """# Original fixture materials; base texture is relative, with Windows separators.
newmtl wood
Kd 1 1 1
map_Kd textures\\wood.png
newmtl red_canvas
Kd 0.72 0.13 0.055
newmtl cream_canvas
Kd 0.95 0.83 0.55
newmtl green_produce
Kd 0.24 0.55 0.10
""")


def dae(skinned=False):
    geometry = """<library_geometries><geometry id="triangle" name="triangle"><mesh>
<source id="pos"><float_array id="pos-array" count="9">0 0 0 100 0 0 0 100 0</float_array><technique_common><accessor source="#pos-array" count="3" stride="3"><param name="X" type="float"/><param name="Y" type="float"/><param name="Z" type="float"/></accessor></technique_common></source>
<vertices id="verts"><input semantic="POSITION" source="#pos"/></vertices>
<triangles count="1"><input semantic="VERTEX" source="#verts" offset="0"/><p>0 1 2</p></triangles>
</mesh></geometry></library_geometries>"""
    controller = """<library_controllers><controller id="skin"><skin source="#triangle">
<bind_shape_matrix>1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1</bind_shape_matrix>
<source id="joints"><Name_array id="joints-array" count="2">root tip</Name_array><technique_common><accessor source="#joints-array" count="2" stride="1"><param name="JOINT" type="Name"/></accessor></technique_common></source>
<source id="binds"><float_array id="binds-array" count="32">1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1 1 0 0 0 0 1 0 -100 0 0 1 0 0 0 0 1</float_array><technique_common><accessor source="#binds-array" count="2" stride="16"><param name="TRANSFORM" type="float4x4"/></accessor></technique_common></source>
<source id="weights"><float_array id="weights-array" count="3">1 0.25 0.75</float_array><technique_common><accessor source="#weights-array" count="3" stride="1"><param name="WEIGHT" type="float"/></accessor></technique_common></source>
<joints><input semantic="JOINT" source="#joints"/><input semantic="INV_BIND_MATRIX" source="#binds"/></joints>
<vertex_weights count="3"><input semantic="JOINT" source="#joints" offset="0"/><input semantic="WEIGHT" source="#weights" offset="1"/><vcount>1 2 1</vcount><v>0 0 0 1 1 2 1 0</v></vertex_weights>
</skin></controller></library_controllers>
<library_animations><animation id="tip-motion" name="tip-motion">
<source id="times"><float_array id="times-array" count="2">0 2</float_array><technique_common><accessor source="#times-array" count="2" stride="1"><param name="TIME" type="float"/></accessor></technique_common></source>
<source id="values"><float_array id="values-array" count="6">0 100 0 0 150 0</float_array><technique_common><accessor source="#values-array" count="2" stride="3"><param name="X" type="float"/><param name="Y" type="float"/><param name="Z" type="float"/></accessor></technique_common></source>
<source id="modes"><Name_array id="modes-array" count="2">LINEAR LINEAR</Name_array><technique_common><accessor source="#modes-array" count="2" stride="1"><param name="INTERPOLATION" type="Name"/></accessor></technique_common></source>
<sampler id="sampler"><input semantic="INPUT" source="#times"/><input semantic="OUTPUT" source="#values"/><input semantic="INTERPOLATION" source="#modes"/></sampler><channel source="#sampler" target="tip/location"/>
</animation></library_animations>""" if skinned else ""
    nodes = """<node id="root" sid="root" name="root" type="JOINT"><matrix>1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1</matrix><node id="tip" sid="tip" name="tip" type="JOINT"><translate sid="location">0 100 0</translate></node></node><node id="mesh" name="mesh"><instance_controller url="#skin"><skeleton>#root</skeleton></instance_controller></node>""" if skinned else """<node id="translated" name="translated"><translate>100 200 300</translate><node id="scaled" name="scaled"><scale>2 1 1</scale><instance_geometry url="#triangle"/></node></node>"""
    return f'''<?xml version="1.0" encoding="utf-8"?>
<COLLADA xmlns="http://www.collada.org/2005/11/COLLADASchema" version="1.4.1">
<asset><contributor><author>ZiiNAN original test fixture</author></contributor><created>2026-09-14T00:00:00Z</created><modified>2026-09-14T00:00:00Z</modified><unit name="centimeter" meter="0.01"/><up_axis>Y_UP</up_axis></asset>
{geometry}{controller}<library_visual_scenes><visual_scene id="scene" name="scene">{nodes}</visual_scene></library_visual_scenes><scene><instance_visual_scene url="#scene"/></scene></COLLADA>
'''


def main():
    (OUT / "textures").mkdir(parents=True, exist_ok=True)
    (OUT / "textures" / "wood.png").write_bytes(texture())
    stall()
    write("simple.obj", "# Quad, meters, no source normals\nv 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nf 1/1 2/2 3/3 4/4\n")
    write("transformed_cm.dae", dae())
    write("skinned_animation.dae", dae(True))
    write("simple_cm.fbx", '''; FBX 7.4.0 project file - original ZiiNAN fixture
FBXHeaderExtension: {
 FBXHeaderVersion: 1003
 FBXVersion: 7400
 Creator: "ZiiNAN fixture"
}
GlobalSettings: {
 Version: 1000
 Properties70: {
  P: "UpAxis", "int", "Integer", "",1
  P: "UpAxisSign", "int", "Integer", "",1
  P: "FrontAxis", "int", "Integer", "",2
  P: "FrontAxisSign", "int", "Integer", "",1
  P: "CoordAxis", "int", "Integer", "",0
  P: "CoordAxisSign", "int", "Integer", "",1
  P: "UnitScaleFactor", "double", "Number", "",1
  P: "OriginalUnitScaleFactor", "double", "Number", "",1
 }
}
Definitions: {
 Version: 100
 Count: 2
 ObjectType: "Model" { Count: 1 }
 ObjectType: "Geometry" { Count: 1 }
}
Objects: {
 Geometry: 1001, "Geometry::triangle", "Mesh" {
  Vertices: *9 { a: 0,0,0,100,0,0,0,100,0 }
  PolygonVertexIndex: *3 { a: 0,1,-3 }
  GeometryVersion: 124
 }
 Model: 1002, "Model::translated", "Mesh" {
  Version: 232
  Properties70: {
   P: "Lcl Translation", "Lcl Translation", "", "A",100,200,300
   P: "Lcl Rotation", "Lcl Rotation", "", "A",0,0,0
   P: "Lcl Scaling", "Lcl Scaling", "", "A",1,1,1
  }
  Shading: T
  Culling: "CullingOff"
 }
}
Connections: {
 C: "OO",1001,1002
 C: "OO",1002,0
}
''')
    write("missing_texture.obj", "mtllib missing_texture.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 0 1\nusemtl missing\nf 1/1 2/2 3/3\n")
    write("missing_texture.mtl", "newmtl missing\nKd 1 1 1\nmap_Kd textures/not-present.png\n")
    write("alpha.obj", "mtllib alpha.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nusemtl translucent\nf 1//1 2//1 3//1\n")
    write("alpha.mtl", "newmtl translucent\nKd 0.2 0.6 0.8\nd 0.4\n")
    write("README.md", """# Original E2-X import fixtures

All geometry and texture pixels are authored in `generate_fixtures.py` for this repository; no external asset license is required.

- `simple.obj`: source quad with UV0, absent normals.
- `market_stall.obj` / `.mtl`: 21 named pieces, 4 used materials, 126 quads, 504 source face-corner vertices; 2.9 m wide, 2.47 m high. Wood image has an asymmetric blue/white corner to expose UV changes. OBJ uses meters and Y up, +Z front. MTL deliberately uses a Windows backslash in the relative texture path.
- `transformed_cm.dae`: centimeters, Y up; nested translation (100, 200, 300) cm and x-scale 2. Expected meter bounds (1,2,3) to (3,3,3).
- `simple_cm.fbx`: original ASCII FBX 7.4, centimeters, Y up, translated triangle. Expected meter bounds (1,2,3) to (2,3,3).
- `skinned_animation.dae`: two joints and inverse binds, per-vertex weights, 2-second translation animation. Metadata proof only; no runtime animation evaluator.
- `missing_texture.obj`: explicit missing-image failure.
- `alpha.obj`: authored normals, diffuse color and 0.4 material opacity.

Regenerate with Python 3 standard library: `python tests/AssetTool/generate_fixtures.py`. No regeneration is required during CMake/CTest. Converted GLBs and render evidence belong in ignored build directories.
""")


if __name__ == "__main__":
    main()
