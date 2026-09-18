# Regression tests

Keep the existing CTest/C++ harnesses and functional directories. Historical
milestone labels identify provenance; they do not make a regression obsolete.

| Directory | Retained coverage |
| --- | --- |
| Platform | ABI, LP64, pointer roundtrip, boundaries, window/input and lifecycle |
| Renderer | GPU skinning, policies, terrain, actors, effects, text/UI and resources |
| AssetRuntime | GR2 safety/corpus/compatibility/goldens, GLB, animated GLB and GPU rendering |
| AnimationRuntime | Pose/clip contracts, public boundary, load/stall diagnostics |
| Graphics | Materials/PBR, DiligentFX, lighting/shadows/SSAO, HDR/atmosphere and water |
| Vegetation | SDK-free contracts, malformed input, structural and image-signature goldens |
| AssetTool | Offline conversion, CLI errors, roundtrip and runtime dependency isolation |
| goldens/classic | Twelve frozen BMP references and strict SHA256 comparison |

## Fast gate

`fast-tests.txt` is the unchanged 66-test G7 selection plus the newly registered
`Renderer.ClassicGoldens` comparison. It excludes upstream fuzzers, benchmarks
and long stress runs. Use an absolute list path because CTest changes directory:

```powershell
$gate = (Resolve-Path tests/fast-tests.txt).Path
cmake --build build-cleanup-validation --config Release --parallel 6
ctest --test-dir build-cleanup-validation -C Release --tests-from-file $gate --output-on-failure
cmake --build build-cleanup-validation --config Debug --parallel 6
ctest --test-dir build-cleanup-validation -C Debug --tests-from-file $gate --output-on-failure
```

Fresh configuration requires `-DM2_BUILD_ASSET_TOOL=ON` for this full gate.
Use the installed Visual Studio x64 developer shell. Keep the normal production
CMake defaults and configure a new build directory, never copy an old cache.

The Classic CTest fixture runs both producing render tests before comparison,
including when selecting only `Renderer.ClassicGoldens`. Test runs never update
the references. GPU tests use the established Windows D3D11 environment.

For portable GCC/LP64, configure a separate directory with Windows client and
renderer tests OFF, platform tests ON, and the optional asset tool ON. Run the
39 portable checks (35 fast checks plus four platform/ABI/lifecycle checks);
they are not Android device validation.

## Fixtures, local data and outputs

- Self-authored GLB, animation and offline-import fixtures stay beside their
  tests, including their generators and provenance notes.
- Small GR2 and vegetation signature references stay in their existing fixture
  directories. No expected values were changed by CLEAN-X.
- Installed game assets remain an external prerequisite. Preserved converted
  vegetation and older manual-launcher templates live under `test-data/`; see
  its README and file manifest. They must survive deletion of generated builds.
- Graphics preparation and animated-character smoke accept `-OutputDirectory`
  to keep current runtime evidence under the active validation directory.
- Native smoke helpers create private runtime roots and packages. They read
  the sibling game checkout; they must never overwrite its files.
- Logs, captures, converted outputs and build products belong in ignored build
  or output directories. Historical phase reports remain in `docs/`; their old
  links into deleted build directories are historical evidence locations.

The manual renderer/skin benchmark helpers remain available because their
scenario coverage still has value. CLEAN-X parses their scripts and preserves
their exact input templates; it does not rerun every historical manual scenario.

## Dependency sources

The existing DiligentCore, DiligentFX, Assimp and meshoptimizer pins are tracked
as submodules under `external/`; see [initialization](../external/README.md).
CMake selects them automatically, with dependency outputs under `build/deps/`
for the standard `-B build` configuration. Explicit `FETCHCONTENT_SOURCE_DIR_*`
overrides remain available. Vendor versions are unchanged; upstream Zstd
fuzzers/benchmarks are disabled in our build.

The completed C-LIB-X comparison tools were removed. Their conclusions and
versioned evidence remain under `docs/core/`; their implementation remains in
Git history at the G7 checkpoint `4cb5b9f`.
