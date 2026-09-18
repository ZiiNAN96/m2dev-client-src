# Pinned third-party sources

These submodules reuse the dependency versions already selected by the build.
No generated libraries, object files, shader headers or build caches belong here.
The existing `vendor/` and `extern/` libraries are unchanged and do not duplicate
these four dependencies.

| Directory | Revision | Use |
| --- | --- | --- |
| DiligentCore | `b036337d68be2353c9950a85929acf796b9a6d50` | Renderer |
| DiligentFX | `cb380ac52100672b5762f595acfb6609e0ecc248` | Renderer FX subset |
| assimp | `fb375dd8c0a032106a2122815fb18dffe0283721` (v6.0.2) | Optional offline asset tools |
| meshoptimizer | `6daea4695c48338363b08022d2fb15deaef6ac09` (v0.25) | Optional offline asset tools |

After cloning, initialize the Windows renderer sources:

```sh
git submodule update --init external/DiligentCore external/DiligentFX
git -C external/DiligentCore submodule update --init ThirdParty/xxHash
```

For `M2_BUILD_ASSET_TOOL=ON`, also initialize:

```sh
git submodule update --init external/assimp external/meshoptimizer
```

The Android bootstrap additionally needs Core's pinned `ThirdParty/Vulkan-Headers`,
`ThirdParty/volk`, `ThirdParty/SPIRV-Headers` and `ThirdParty/SPIRV-Cross` submodules.
No renderer, importer or upstream version was upgraded by the structure cleanup.

CMake uses these sources without downloading another copy. Existing explicit
`FETCHCONTENT_SOURCE_DIR_*` overrides remain supported. Configure with
`cmake -S . -B build -A x64`; dependency build products go to `build/deps/`.
The generated, adapted DiligentFX subset remains under `build/src/Renderer/`.
With a different binary directory, both output paths follow that directory.
