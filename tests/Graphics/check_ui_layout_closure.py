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
assert events[-1].get("mode") == "closure"
required = ("drift_pass", "scroll_pass", "rollback_pass", "keep_reopen_pass",
            "user_position_bag_pass", "modal_center_pass", "tooltip_bounds_pass")
for event in required:
    assert any(e["event"] == event for e in events), event
layouts = [e for e in events if e["event"] == "layout"]
passes = [e for e in events if e["event"] == "layout_pass"]
assert len(layouts) == len(passes) == 29, len(layouts)
modes = {(1024,768), (1280,720), (1920,1080), (2560,1440)}
for path in ("closed-resize-open-", "open-resize-"):
    tested = {tuple(e["viewport"]) for e in layouts if e["label"].startswith(path)}
    assert modes <= tested, (path, tested)
for layout, check in zip(layouts, passes):
    assert layout["label"] == check["label"]
    g = check["geometry"]
    for prefix in ("Client", "Backbuffer", "UIScreen", "CPUViewport", "RenderViewport"):
        assert [g[prefix+"Width"], g[prefix+"Height"]] == layout["viewport"], (prefix, layout)
    assert g["UIDraws"] > 0
assert len([e for e in events if e["event"] == "character_tab_pass"]) == 12
popups = [e for e in events if e["event"] == "popup_bounds_pass"]
assert {tuple(e["viewport"]) for e in popups} == modes
assert all(e["controls"] >= 10 for e in popups)
assert next(e for e in events if e["event"] == "drift_pass")["cycles"] == 5
assert next(e for e in events if e["event"] == "rollback_pass")["elapsed"] >= 15
first = layouts[0]
for layout in layouts:
    if layout["label"].startswith("open-resize-cycle") and layout["viewport"] == [1024,768]:
        for name, control in first["controls"].items():
            assert control["globalPos"] == layout["controls"][name]["globalPos"], name
            assert control["size"] == layout["controls"][name]["size"], name
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
result = dict(status="PASS", exit=exit_info, mode=events[-1].get("mode"), zero_keys=zero_keys,
              layoutSnapshots=len(layouts), controlsPerSnapshot=len(first["controls"]),
              driftCycles=5, modes=sorted(modes), requiredEvents=required,
              reflows=sum(e["event"]=="reflow" for e in events))
(run / "fast-gate.json").write_text(json.dumps(result, indent=2))
print(json.dumps(result))
