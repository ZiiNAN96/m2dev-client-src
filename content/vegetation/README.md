# H2 demonstration vegetation

Original procedural beech, grass and bush authored for this repository by
`tools/Vegetation/ModernAssets.cpp`. No external art, downloaded textures or
Unity/SDK assets are used. These original assets may be used, modified and
redistributed with this project. Existing converted game assets retain their
separate provenance and are not included here.

Regenerate with the optional offline target `ZiiNANModernVegetationAssets` and
an output directory. It uses the existing AssetTool scene, meshoptimizer cache /
fetch optimization, GLB writer and production GlTF provider validation. LODs are
authored at three densities; alpha cards are not blindly simplified. Eight-view
far atlases are rasterized offline from the high LOD geometry and source alpha.

The native H2 preparation helper installs a single optional registry override
for `d:/ymir work/tree/b1_beech_rt4.spt`. All other keys keep their converted
assets. Grass uses `vegetation/modern/grass.zveg` if supplied. The bush is a
controlled demonstration asset, ready for a suitable authored registry mapping;
it does not automatically replace unrelated trees.

Geometry and ZVEG files keep their `vegetation/modern/` runtime names. Texture
pack keys are `d:/ymir work/vegetation/modern/*.dds`; PackMaker obtains these
from `ymir work/vegetation/modern/` below its input directory. The preparation
helper creates these files in a private runtime, preserving production packs.

Reproduction (from an existing configured build with `M2_BUILD_ASSET_TOOL=ON`):

```powershell
cmake --build build --config Release --target ZiiNANModernVegetationAssets
build/tools/AssetTool/Release/ZiiNANModernVegetationAssets.exe build/generated/vegetation
```

The target's actual executable location is shown by CMake; test fixture setup
`Vegetation.ModernAssetGeneration` also generates the same layout automatically.
`manifest.json` records the delivered Windows-generated runtime file hashes.
