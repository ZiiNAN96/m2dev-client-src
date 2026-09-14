# F3/4 GR2 golden references

Captured from the explicit Granny reference provider on 2026-09-14, before
removal. Source baseline: `961401a09297a0f0d7504595681b3bf53eb7c134` (F3-B).
These files contain derived measurements only, not copies of proprietary GR2s.

`gr2-golden-static.txt` covers Warrior, Hair, Weapon, Wolf, Boss, Mount,
Building and Prop. Records preserve source-byte fingerprints, model/mesh/material
counts, complete bone names/order/parents, selected bind transforms, material
references, skin mappings and hashes of the complete vertex/index streams.

`gr2-golden-animation.txt` covers Warrior Idle/Walk/Run/finite Attack plus
Wolf, Boss and Mount clips. At 0%, 25%, 50%, 75%, 100% and just past the end,
it preserves selected world/composite matrices and final positions/normals for
three vertices per skinned mesh. Duration, loop mode and source fingerprint
are fixed. The test retains F1-X limits: world/palette 2e-3, vertex position
5e-3, normal 5e-5. Live parity tests additionally checked full local
translation/rotation/scale and full vertex sets during Gate A. They were removed
with the SDK after Gate A passed; the stored samples are the continuing oracle.

Numbers use the classic locale and nine significant digits (float round trip).
Hashes are FNV-1a 64-bit over source bytes and little-endian canonical copy
streams; they detect changed inputs and are not cryptographic integrity claims.
Paths are relative to the separately installed original-asset root.

`GR2GoldenTest` links only AssetRuntimeGR2 and AnimationRuntime. It rejects
missing, truncated, changed or trailing records and checks all sampled values
for finiteness. Normal tests never generate or overwrite their expectations.
Static and animation tests also require all reader/runtime owners and all
Granny read/pose/import counters to be zero.

The separate `GR2GoldenExport` used the explicitly selected Granny animation
evaluator and refused to replace an existing file. The exporter was removed
with the SDK after capture. Changing a golden requires reviewing the source
change and an independently established reference measurement; replacing the
expectations solely to make a failing test pass is invalid.

Captured SHA256: static `7B1CF0B2F87C193C1301560D3E30ED879ED1675DD7855EA9F745A0FAA2160227`,
animation `36E691EA81588C44512F0D737BE29DD32A1DC313B92808E4C998A7246A20CC9B`.
