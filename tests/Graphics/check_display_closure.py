"""Check native private-client closure evidence and its short shutdown gate."""
import json
from pathlib import Path
import re
import sys

run = Path(sys.argv[1])
exit_info = json.loads((run / "exit.json").read_text(encoding="utf-8-sig"))
assert exit_info["ExitCode"] == 0, exit_info
assert not (run / "closure-failure.log").exists()
assert (run / "log/syserr.txt").stat().st_size == 0
events = [json.loads(line) for line in (run / "closure-run.jsonl").read_text().splitlines()]
assert events[-1]["event"] == "completed"
if events[-1].get("mode") == "closure":
    for event in ("explicit_center_pass", "scroll_pass", "rollback_pass", "keep_reopen_pass"):
        assert any(e["event"] == event for e in events), event
    layouts = [e for e in events if e["event"] == "layout"]
    first, last = layouts[0], layouts[-1]
    assert first["viewport"] == last["viewport"] == [1024, 768]
    for name, control in first["controls"].items():
        assert control["globalPos"] == last["controls"][name]["globalPos"], name
    assert first["chatRect"] == last["chatRect"]
audit = (run / "source-resource-audit.log").read_text()
zero_keys = ("DiligentErrors DiligentFatals AllCPUDeformationCalls AllCPUDeformationVertices "
             "GPUFallbacks GraphicsSettingsObjects SourceTextures SourceBuffers SkinMeshes "
             "BoneRemaps BonePalettes SkinPreparationFailures CollisionResources VegetationAssets "
             "VegetationInstances VegetationRenderAssets VegetationGeometry VegetationInstanceBuffers "
             "VegetationInstanceBytes VegetationFailures AssetDocuments AnimationInstances MeshBindings "
             "GR2ReaderResources RuntimeSkeletons RuntimeAnimationClips IndependentAnimationInstances "
             "AnimationRuntimeFailures PrototypeGeometry PrototypePalettes").split()
for key in zero_keys:
    assert re.search(r"\b" + key + r"=0\b", audit), key
gdx = (run / "gdx-renderer.log").read_text()
for key in ("WaterRenderers", "ModernRenderers", "ssrFallbacks"):
    assert re.search(r"\b" + key + r"=0\b", gdx), key
terrain = (run / "terrain-renderer.log").read_text()
for key in ("terrain_vertices", "terrain_indices", "terrain_textures", "object_geometry",
            "object_textures", "mount_geometry", "mount_textures"):
    values = re.findall(r"\b" + key + r"=(\d+)", terrain)
    if values:
        assert values[-1] == "0", (key, values[-1])
assert (run / "static-object-adapter.log").read_text().strip().endswith("live_objects=0")
assert (run / "renderer-failure.log").read_text().strip() == "Renderer failure diagnostics enabled"
result = dict(status="PASS", exit=exit_info, mode=events[-1].get("mode"), zero_keys=zero_keys)
(run / "fast-gate.json").write_text(json.dumps(result, indent=2))
print(json.dumps(result))
