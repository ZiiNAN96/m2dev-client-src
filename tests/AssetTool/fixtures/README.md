# Original E2-X import fixtures

All geometry and texture pixels are authored in `generate_fixtures.py` for this repository; no external asset license is required.

- `simple.obj`: source quad with UV0, absent normals.
- `market_stall.obj` / `.mtl`: 21 named pieces, 4 used materials, 126 quads, 504 source face-corner vertices; 2.9 m wide, 2.47 m high. Wood image has an asymmetric blue/white corner to expose UV changes. OBJ uses meters and Y up, +Z front. MTL deliberately uses a Windows backslash in the relative texture path.
- `transformed_cm.dae`: centimeters, Y up; nested translation (100, 200, 300) cm and x-scale 2. Expected meter bounds (1,2,3) to (3,3,3).
- `simple_cm.fbx`: original ASCII FBX 7.4, centimeters, Y up, translated triangle. Expected meter bounds (1,2,3) to (2,3,3).
- `skinned_animation.dae`: two joints and inverse binds, per-vertex weights, 2-second translation animation. Metadata proof only; no runtime animation evaluator.
- `missing_texture.obj`: explicit missing-image failure.
- `alpha.obj`: authored normals, diffuse color and 0.4 material opacity.

Regenerate with Python 3 standard library: `python tests/AssetTool/generate_fixtures.py`. No regeneration is required during CMake/CTest. Converted GLBs and render evidence belong in ignored build directories.
