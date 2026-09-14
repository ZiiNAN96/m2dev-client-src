"""Analyze every captured display interval, without discarding cold outliers.
CPU scopes are nested; only the explicitly exclusive asset sum is added.
Python standard library only (including Python 3.6 / LP64).
"""
import csv
import json
import math
import pathlib
import statistics
import sys

PHASES = {0: "loading", 1: "cold_idle", 2: "cold_movement", 3: "cold_combat",
          4: "warm_idle", 5: "warm_movement", 6: "warm_combat", 7: "transition"}
ASSET = ("gr2_read", "container", "parse", "animation_decode", "clip_bind", "fingerprint")
FORBIDDEN_WARM = ("gr2_read", "decompress", "container", "parse", "animation_decode",
                  "clip_bind", "skeleton", "mesh", "material", "fingerprint")


def percentile(values, fraction):
    values = sorted(values)
    index = (len(values) - 1) * fraction
    low, high = int(math.floor(index)), int(math.ceil(index))
    return values[low] + (values[high] - values[low]) * (index - low)


def distribution(values):
    return {"count": len(values), "average": statistics.mean(values),
            "median": statistics.median(values), "p95": percentile(values, .95),
            "p99": percentile(values, .99), "max": max(values),
            "over_8_33": sum(x > 8.33 for x in values),
            "over_16_67": sum(x > 16.67 for x in values),
            "over_20": sum(x > 20 for x in values), "over_50": sum(x > 50 for x in values)}


def analyze(directory):
    with (directory / "load-warmup-frames.csv").open(newline="") as source:
        rows = list(csv.DictReader(source))
    summary = (directory / "animation-stall-summary.txt").read_text()
    if "FullFrameCapture=1" not in summary or "DroppedFrames=0 " not in summary:
        raise ValueError("Complete all-frame capture required")
    artifact = directory / "artifact.txt"
    manual = artifact.exists() and "Manual=True" in artifact.read_text(encoding="utf-8-sig")
    phases = {0: "loading", 1: "manual_idle", 2: "manual_movement", 3: "manual_combat"} if manual else PHASES
    results = {"source": str(directory), "phases": {}, "manual": manual,
               "method": "All display rows; no outlier removal. " + ("Manual activity labels combine cold and warm use; they are not fixture cold/warm phases. " if manual else "Explicit fixture phases. ") + "CPU work = process minus Sleep and Present wait; asset sum excludes nested subscopes. Fixed 16/17 ms engine pacing and VSync remain unchanged."}
    for phase, name in phases.items():
        selected = [r for r in rows if int(r["phase"]) == phase]
        if not selected:
            continue
        values = lambda field: [float(r.get(field, 0)) for r in selected]
        work = [max(0, float(r["process_ms"]) - float(r["sleep_ms"]) - float(r["present_wait_ms"])) for r in selected]
        asset = [sum(float(r.get(key + "_work_ms", 0)) for key in ASSET) for r in selected]
        data = {"frame_ms": distribution(values("frame_ms")), "cpu_work_ms": distribution(work),
                "asset_ms": distribution(asset), "pose_ms": distribution(values("pose_ms")),
                "reference_pose_ms": distribution(values("reference_pose_work_ms")),
                "palette_ms": distribution(values("palette_ms")),
                "gpu_create_ms": distribution(values("gpu_create_work_ms")),
                "gpu_upload_ms": distribution(values("gpu_upload_work_ms")),
                "submission_ms": distribution(values("submission_ms")),
                "present_wait_ms": distribution(values("present_wait_ms")),
                "calls": {key: int(sum(values(key + "_calls"))) for key in FORBIDDEN_WARM},
                "total_work_ms": {key[:-8]: sum(values(key)) for key in selected[0] if key.endswith("_work_ms")},
                "minimized_frames": sum(r["minimized"] == "1" for r in selected),
                "inactive_frames": sum(r["inactive"] == "1" for r in selected),
                "imports": int(sum(values("imports"))), "shared_hits": int(sum(values("instance_hits"))),
                "cpu_deforms": int(sum(values("cpu_deforms"))), "gpu_fallbacks": int(sum(values("gpu_fallbacks")))}
        results["phases"][name] = data
        f, cpu, a = data["frame_ms"], data["cpu_work_ms"], data["asset_ms"]
        print("{} {:14s} n={} avg={:.3f} p95={:.3f} p99={:.3f} max={:.3f} >50={} cpuP95={:.3f} assetMax={:.3f} imports={}".format(
            directory.name, name, f["count"], f["average"], f["p95"], f["p99"], f["max"], f["over_50"], cpu["p95"], a["max"], data["imports"]))
    (directory / "metrics.json").write_text(json.dumps(results, indent=2))
    return results


if __name__ == "__main__":
    for path in sys.argv[1:]:
        analyze(pathlib.Path(path))
