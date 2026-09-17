# Classic image references

Twelve unchanged BMP captures from the existing GLB and animated-GLB render
tests. Their SHA256 values were frozen before CLEAN-X and match the G-DX,
G5/6 and G7 results. CLEAN-X copied the existing `build-hx-clean/tests/AssetRuntime`
captures only after checking every hash against `build/g7x/classic-Release.json`.

The models are the repository's deterministic, self-authored market-stall and
F5-X character fixtures. These are test inputs, not visual-proof galleries.
Do not regenerate references as part of a normal test run.

`Renderer.ClassicGoldens` runs after `AssetRuntime.GlTFRender` and
`AssetRuntime.GlTFCharacterRender`. It checks the stored references and newly
rendered captures against `manifest.json`, with no pixel tolerance. The same
references apply to Release and Debug on the established D3D11 test machine.
