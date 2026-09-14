"""Join the frozen F3/4 inventory with fresh F3-A field/corpus/pack evidence."""
import argparse
import collections
import csv
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    root = args.source
    baseline = root / "docs/assets/phase-f34-rejected-classification.json"
    corpus = root / "build/f3a/corpus-verified/files.tsv"
    probe = root / "build/f3a/root-cause-probe.log"
    pack = root / "build/f3a/packed-audit.tsv"
    old = json.loads(baseline.read_text(encoding="utf-8"))["rows"]
    with corpus.open(encoding="utf-8", errors="surrogateescape", newline="") as stream:
        scanned = {p: (status, reason) for p, status, reason in csv.reader(stream, delimiter="\t")}
    with pack.open(encoding="utf-8-sig", newline="") as stream:
        packed = {r["source"]: r for r in csv.DictReader(stream, delimiter="\t") if r["kind"] == "gr2"}
    blocks = {}
    current = None
    for line in probe.read_text(encoding="utf-8-sig", errors="replace").splitlines():
        if line.startswith("FILE "):
            current = line[5:]
            blocks[current] = []
        elif current is not None:
            blocks[current].append(line)
    assert len(old) == len(packed) == len(blocks) == 52
    assert len(scanned) == 9166
    assert all(r["identical"] == "1" for r in packed.values() if r["raw_native"] == "1")
    rows = []
    for previous in old:
        path = previous["path"]
        asset = root.parent / "m2dev-client/assets" / path
        assert hashlib.sha256(asset.read_bytes()).hexdigest() == previous["sha256"], f"Source changed: {path}"
        reason = previous["reason"]
        if "periodic" in reason:
            cluster, field, reader_site = "periodic_loop", "TrackGroup.PeriodicLoop", "GR2AnimationReader.cpp: ReadAnimation"
        elif "exactly one" in reason:
            cluster, field, reader_site = "skeleton_forest", "Skeleton.Bones[].ParentIndex", "GR2ModelReader.cpp: Read / RuntimeSkeleton::Initialize"
        elif "bone names" in reason:
            cluster, field, reader_site = "duplicate_bone_names", "Skeleton.Bones[].Name", "GR2ModelReader.cpp: Read / RuntimeSkeleton::Initialize"
        elif "track target" in reason:
            cluster, field, reader_site = "duplicate_track_names", "TrackGroup.TransformTracks[].Name", "GR2AnimationReader.cpp: ReadAnimation / BindAnimation"
        elif "empty mesh" in reason:
            cluster, field, reader_site = "empty_mesh", "Mesh.PrimaryVertexData.Vertices / PrimaryTopology", "GR2MeshReader.cpp: ReadMesh"
        elif "nonfinite" in reason:
            cluster, field, reader_site = "nonfinite_curve", "TransformTracks[].{Position,Orientation,ScaleShear}Curve.Controls[]", "GR2AnimationReader.cpp: ReadCurve -> Types::Reals -> File::Float"
        elif "file size" in reason:
            cluster, field, reader_site = "overridden_bad_raw_size", "Header.FileSize at byte 36 (before sections)", "GR2File.cpp: Inspect"
        else:
            raise ValueError(f"Unclassified cause: {reason}")
        lines = blocks[path]
        evidence = [s for s in lines if s.startswith(("BONE ", "TRACK ", "PeriodicLoop ", "TEXT_OUTSIDE ", "MESH ", "GROUP "))]
        if cluster == "nonfinite_curve":
            evidence = [s for s in lines if s.startswith("NONFINITE ")][:8]
        if cluster == "overridden_bad_raw_size":
            evidence = [s for s in lines if s.startswith("ERROR ")]
        status, diagnostic = scanned[path]
        usage = "production" if previous["used_by_production_client"].startswith("yes") else "usage unresolved" if previous["used_by_production_client"].startswith("unknown") else "pack overridden raw"
        row = dict(path=path, sha256=previous["sha256"], asset_type=previous["category"], usage_before=usage, usage_after=usage,
                   cluster=cluster, reader_site=reader_site, field=field, before=previous["reader_status"], before_error=reason,
                   after=status, after_error=diagnostic, packed_native=packed[path]["native"], granny_load=packed[path]["reference"],
                   section_type_field_evidence=evidence,
                   granny_reference_evidence=list(dict.fromkeys(s for s in lines if s.startswith(("REF_BIND ", "REF_SAMPLE ", "REF_NAN_BONE ", "REF_GROUP ")))),
                   original_usage_evidence=previous.get("asset_references", []),
                   motion_list_evidence=previous.get("motion_list_references", []))
        rows.append(row)
    failures = [p for p, (status, _) in scanned.items() if status != "parsed"]
    old_paths = {r["path"] for r in old}
    new_failures = [p for p in failures if p not in old_paths]
    counts = collections.Counter(status for status, _ in scanned.values())
    cluster_counts = {}
    for cluster in sorted({r["cluster"] for r in rows}):
        members = [r for r in rows if r["cluster"] == cluster]
        cluster_counts[cluster] = dict(total=len(members), production=sum(r["usage_after"] == "production" for r in members),
                                      unresolved=sum(r["usage_after"] == "usage unresolved" for r in members), parsed=sum(r["after"] == "parsed" for r in members))
    result = dict(baseline_commit="500986389ef91742e0c7a6b96e67f4276aa4c791", total=len(scanned), counts=dict(counts),
                  production_referenced_unsupported=sum(r["usage_after"] == "production" and r["packed_native"] != "1" for r in rows),
                  usage_unresolved=sum(r["usage_after"] == "usage unresolved" for r in rows),
                  usage_unresolved_rejected=sum(r["usage_after"] == "usage unresolved" and r["after"] != "parsed" for r in rows),
                  pack_overridden_raw_copies=sum(r["usage_after"] == "pack overridden raw" for r in rows),
                  new_rejects=new_failures, clusters=cluster_counts,
                  evidence_sha256={p.relative_to(root).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in (baseline, corpus, probe, pack)}, rows=rows)
    output = root / "docs/assets/phase-f3a-compatibility-results"
    output.with_suffix(".json").write_text(json.dumps(result, ensure_ascii=True, indent=2) + "\n", encoding="utf-8")
    fields = [key for key in rows[0] if key not in ("original_usage_evidence", "motion_list_evidence")]
    with output.with_suffix(".csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields, extrasaction="ignore")
        writer.writeheader()
        for row in rows:
            writer.writerow({key: " | ".join(value) if isinstance(value, list) else value for key, value in row.items() if key in fields})
    print(json.dumps({k: v for k, v in result.items() if k not in ("rows", "evidence_sha256")}, indent=2))
    if new_failures:
        raise SystemExit("New corpus rejects require review")


if __name__ == "__main__":
    main()
