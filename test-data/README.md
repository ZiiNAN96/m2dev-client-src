# Local regression inputs

This directory separates irreplaceable local test inputs from disposable builds.
`manifest.json` records each preserved file's SHA256 and byte size. The data
directories are ignored deliberately: these are installed game assets and old
private runtime templates, not newly licensed distributable fixtures.

- `vegetation/vegetation/`: all 237 unchanged registry, ZVEG and GLB files from
  the accepted H-X `build/hx/compiled` conversion (69,521,096 bytes). Retaining
  the complete mapping supports native map smoke tests as well as the nine
  structural and 54 image-signature references. No texture or asset conversion
  is performed by CLEAN-X.
- `legacy-renderer/`: exact root/config templates used by the retained manual
  actor, effects, UI, water and skinning launchers. Copies of binaries, packages,
  logs and screenshots are excluded. Launchers rebuild private packages using
  the preserved inputs and the user's installed game packages.

These files were preserved locally before the old build directories were
removed. A fresh clone must obtain the matching local corpus separately; do not
silently skip tests, download substitute assets, or accept new goldens when it
is missing. `M2_VEGETATION_TEST_DATA` can point CMake at another matching corpus
root containing `vegetation/registry.json`. The optional offline SPT converter
remains available, with its external SDK requirement; no runtime SDK is added.

Redistribution rights for installed game data are not established by this
cleanup. Only the small hash inventory and this explanation are versioned.
