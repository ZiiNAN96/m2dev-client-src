"""Summarize native QPC intervals and the bounded P3 A1 matrix (standard library)."""
import bisect, csv, json, math, re, statistics, sys
from pathlib import Path

root = Path(sys.argv[1])
events = [json.loads(line) for line in (root / "p3-run.jsonl").read_text().splitlines()]
frames = [{k: int(v) for k, v in row.items()} for row in csv.DictReader((root / "frame-pacing.csv").open())]
times = [f["start_ns"] for f in frames]
assert len(times) > 100 and all(a < b for a, b in zip(times, times[1:]))
assert "dropped=0" in (root / "frame-pacing-meta.txt").read_text()
starts = {e["stage"]: e for e in events if e["event"] == "start"}
assert any(e["event"] == "completed" and e["stages"] == 24 for e in events)

def percentile(values, fraction):
    ordered = sorted(values)
    offset = (len(ordered) - 1) * fraction
    low = int(offset)
    return ordered[low] + (ordered[min(low + 1, len(ordered) - 1)] - ordered[low]) * (offset - low)

def stats(values):
    assert values and all(math.isfinite(v) and v >= 0 for v in values)
    return dict(n=len(values), mean=statistics.mean(values), median=percentile(values, .5),
                p95=percentile(values, .95), p99=percentile(values, .99), maximum=max(values),
                over8_33=sum(v > 8.33 for v in values), over16_67=sum(v > 16.67 for v in values),
                over20=sum(v > 20 for v in values))

def attack_durations(rows):
    begin = None
    durations = []
    for r in rows:
        if r["attacking"] and begin is None:
            begin = r["game"]
        if not r["attacking"] and begin is not None:
            durations.append(r["game"] - begin)
            begin = None
    return durations

matrix = []
groups = {}
for end in (e for e in events if e["event"] == "end"):
    start = starts[end["stage"]]
    begin_i = bisect.bisect_left(times, start["ns"])
    end_i = bisect.bisect_left(times, end["ns"])
    pairs = [(frames[i], frames[i + 1]) for i in range(begin_i, end_i - 1)
             if frames[i]["present_end_ns"] and frames[i + 1]["present_end_ns"] < end["ns"]]
    assert pairs, "No native samples in QPC range"
    limit, vsync = (60, 120, 0)[end["mode"][0]], end["mode"][1]
    assert all(a["limit"] == limit and a["vsync"] == vsync and a["skipped_ms"] == 0 for a, b in pairs)
    ft = [(b["start_ns"] - a["start_ns"]) / 1e6 for a, b in pairs]
    presents = [(a["present_end_ns"] - a["present_start_ns"]) / 1e6 for a, b in pairs]
    returns = [(b["present_end_ns"] - a["present_end_ns"]) / 1e6 for a, b in pairs]
    waits = [a["wait_ns"] / 1e6 for a, b in pairs]
    rows = end["rows"]
    game = end["game"] - start["game"]
    local = end["state"]["localTime"] - start["state"]["localTime"]
    assert abs(game - end["wall"]) < .035 and abs(local - game) < .035
    assert 240 <= end["updates"] <= 245
    for r in rows:
        assert all(math.isfinite(r[key]) for key in ("x", "y", "z", "localTime", "game", "camera"))
    movement = math.hypot(end["state"]["x"] - start["state"]["x"], end["state"]["y"] - start["state"]["y"])
    rotation = sum((b["camera"] - a["camera"] + 180) % 360 - 180 for a, b in zip(rows, rows[1:]))
    ui_loops = end["ui_loops"] - start["ui_loops"]
    assert 29 <= ui_loops <= 32
    entry = dict(stage=end["stage"], mode=end["mode"], phase=end["phase"], frametime=stats(ft),
                 present=stats(presents), present_return=stats(returns), wait=stats(waits),
                 wall=end["wall"], game=game, animation=local, updates=end["updates"], movement=movement,
                 camera_degrees=rotation, ui_loops=ui_loops, attacks=attack_durations(rows),
                 max_particles=max(r["particles"] for r in rows))
    if end["phase"] == "RUN":
        assert movement > 1000 and all(r["walking"] for r in rows)
    if end["phase"] == "COMBAT":
        assert len(entry["attacks"]) >= 3 and entry["max_particles"] > 400
    if end["phase"] == "CAMERA":
        assert 358 <= rotation <= 368
    worst = max(range(len(ft)), key=ft.__getitem__)
    entry["longest"] = dict(frame_ms=ft[worst], present_ms=presents[worst], wait_ms=waits[worst],
                            other_wall_ms=ft[worst] - presents[worst] - waits[worst])
    matrix.append(entry)
    group = groups.setdefault((limit, vsync), dict(ft=[], presents=[], returns=[], waits=[]))
    for key, values in (("ft", ft), ("presents", presents), ("returns", returns), ("waits", waits)):
        group[key].extend(values)

run_speeds = [e["movement"] / e["game"] for e in matrix if e["phase"] == "RUN"]
assert max(run_speeds) / min(run_speeds) < 1.02, "FPS dependent movement"
attack_means = [statistics.mean(e["attacks"]) for e in matrix if e["phase"] == "COMBAT"]
assert max(attack_means) - min(attack_means) < .035, "FPS dependent attacks"
summary = [dict(limit=k[0], vsync=k[1], frametime=stats(v["ft"]), present=stats(v["presents"]),
                present_return=stats(v["returns"]), wait=stats(v["waits"])) for k, v in groups.items()]
audit = (root / "source-resource-audit.log").read_text()
for name in ("DiligentErrors", "DiligentFatals", "GPUFallbacks", "AllCPUDeformationCalls", "AllCPUDeformationVertices",
             "GraphicsSettingsObjects", "SourceTextures", "SourceBuffers", "SkinMeshes", "BoneRemaps", "BonePalettes",
             "SkinPreparationFailures", "CollisionResources", "VegetationAssets", "VegetationInstances", "VegetationRenderAssets",
             "VegetationGeometry", "VegetationInstanceBuffers", "VegetationFailures", "AssetDocuments", "AnimationInstances",
             "MeshBindings", "GR2ReaderResources", "IndependentAnimationInstances", "AnimationRuntimeFailures",
             "RuntimeSkeletons", "RuntimeAnimationClips", "PrototypeGeometry", "PrototypePalettes"):
    assert re.search(r"\b" + name + r"=0\b", audit), name
shutdown = [line for line in (root / "terrain-renderer.log").read_text().splitlines() if line.startswith("shutdown ")]
assert len(shutdown) == 8 and not re.search(r"=[1-9]", " ".join(shutdown))
assert (root / "log/syserr.txt").stat().st_size == 0
assert json.loads((root / "exit.json").read_text(encoding="utf-8-sig"))["ExitCode"] == 0
result = dict(summary=summary, matrix=matrix, run_speeds=run_speeds, attack_means=attack_means,
              fast_gate="PASS", native_samples=len(frames))
(root / "summary.json").write_text(json.dumps(result, indent=2))
for e in summary:
    s = e["frametime"]
    print("%s/%s: %.1f FPS, median %.3f p95 %.3f p99 %.3f max %.3f; Present p99 %.3f max %.3f" %
          (e["limit"] or "Unlimited", "On" if e["vsync"] else "Off", 1000/s["mean"], s["median"], s["p95"], s["p99"], s["maximum"], e["present"]["p99"], e["present"]["maximum"]))
print("PASS gameplay invariance, six runtime combinations, errors/resources/exit; restart is checked separately")
