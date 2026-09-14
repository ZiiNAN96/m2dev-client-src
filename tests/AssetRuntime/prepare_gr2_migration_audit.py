"""Build a conservative Gate-A reference inventory; never infer unused from no hit.

Run from any directory with Python 3. The source assets are read only. Pack bytes
and reference configurations must subsequently be checked by GR2MigrationAudit.
"""
import argparse
import csv
import hashlib
import json
import re
import subprocess
from collections import Counter, defaultdict
from pathlib import Path


def normalized(value):
    return value.replace("\\", "/").lower()


def virtual_path(path):
    path = normalized(str(path).split("/", 1)[1])
    return "d:/" + path if path.startswith("ymir work/") else path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--corpus", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--packed-audit", type=Path)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[2]
    assets = source.parent / "m2dev-client/assets"
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    startup = (source / "src/UserInterface/UserInterface.cpp").read_text(encoding="utf-8")
    block = startup.split("std::vector<std::string> packFiles = {", 1)[1].split("};", 1)[0]
    order = ["root"] + re.findall(r'^\s*"([^"]+)"', block, re.M)
    (output / "pack-order.txt").write_text("\n".join(order) + "\n", encoding="utf-8")
    priorities = {name.lower(): index for index, name in enumerate(order)}
    files = [Path(p) for p in subprocess.check_output(
        ["rg", "--files", str(assets)], text=True, encoding="utf-8").splitlines()]
    effective = {}
    for path in files:
        rel = path.relative_to(assets).as_posix()
        if "/" not in rel or rel.split("/")[0].lower() not in priorities:
            continue
        key = virtual_path(rel)
        if key not in effective or priorities[rel.split("/")[0].lower()] > priorities[effective[key].split("/")[0].lower()]:
            effective[key] = rel
    # The historical scanner wrote Windows narrow filenames. All 52 rejected
    # paths are ASCII; preserve undecodable bytes of unrelated parsed paths.
    with args.corpus.open(encoding="utf-8", errors="surrogateescape", newline="") as stream:
        corpus = list(csv.reader(stream, delimiter="\t"))
    rejected = [row for row in corpus if row[1] != "parsed"]
    if len(corpus) != 9166 or len(rejected) != 52:
        raise ValueError("This frozen baseline requires 9166 entries and 52 rejects")
    targets = {virtual_path(row[0]) for row in rejected}
    refs = defaultdict(list)
    text_files = {}
    for key, rel in effective.items():
        if Path(rel).suffix.lower() not in {".msm", ".msa", ".prd", ".prb", ".txt", ".py"}:
            continue
        text = (assets / rel).read_text(encoding="utf-8", errors="replace")
        text_files[key] = text
        bases = re.findall(r'(?im)^\s*PathName\s+"([^"]+)"', text)
        for number, line in enumerate(text.splitlines(), 1):
            if line.lstrip().startswith(("#", "//")):
                continue
            for token in re.findall(r'"([^"\n]+\.gr2)"', line, re.I):
                token = normalized(token)
                candidates = {token, key.rsplit("/", 1)[0] + "/" + token}
                candidates.update(normalized(base).rstrip("/") + "/" + token for base in bases)
                for target in candidates & targets:
                    refs[target].append({"path": rel, "virtual_path": key, "line": number, "text": line.strip(), "kind": "explicit"})
                if "basemodelfilename" in line.lower():
                    lod = token[:-4] + "_lod_01.gr2"
                    if lod in targets:
                        refs[lod].append({"path": rel, "virtual_path": key, "line": number, "text": line.strip(), "kind": "implicit LOD via CRaceData::GetLODModelThing"})
    races = defaultdict(list)
    npc_key = "npclist.txt"
    for number, line in enumerate(text_files[npc_key].splitlines(), 1):
        fields = line.split()
        if len(fields) >= 2 and fields[0].isdigit() and int(fields[0]) > 0:
            races[fields[1]].append({"race": int(fields[0]), "line": number})
    # Exact first-column proto IDs and final-column single-mob regen IDs only;
    # arbitrary numeric grep hits (coordinates/group IDs) are not spawn proof.
    server = source.parent / "m2dev-server/share"
    server_refs = defaultdict(list)
    for number, line in enumerate((server / "conf/mob_proto.txt").read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        fields = line.split()
        if fields and fields[0].isdigit():
            server_refs[int(fields[0])].append(f"share/conf/mob_proto.txt:{number}")
    for path in (server / "locale/english/map").rglob("*.txt"):
        if path.name not in {"regen.txt", "npc.txt", "boss.txt", "stone.txt"}:
            continue
        for number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            fields = line.split("#", 1)[0].split()
            if len(fields) == 11 and fields[0].lower() in {"m", "r", "s"} and fields[-1].isdigit():
                value = f"share/{path.relative_to(server).as_posix()}:{number}"
                if len(server_refs[int(fields[-1])]) < 8:
                    server_refs[int(fields[-1])].append(value)
    rows = []
    dependencies = {npc_key}
    for path, status, reason in rejected:
        key = virtual_path(path)
        owner = key.rsplit("/", 2)[-2]
        if "/hair/" in key:
            category = "player hair"
        elif "/pc/" in key or "/pc2/" in key:
            category = "player animation" if "/general/" in key else "player model"
        elif "/zone/" in key:
            category = "dungeon building/blocker"
        elif "pig_young" in key:
            category = "pet (NPC skeleton)"
        elif "/npc" in key:
            category = "NPC model/animation"
        elif "boss" in key or "general" in key:
            category = "boss model/animation"
        else:
            category = "mob model/animation"
        references = refs[key]
        motion_links = []
        for ref in references:
            dependencies.add(ref["virtual_path"])
            if ref["virtual_path"].endswith(".msa"):
                motlist = ref["virtual_path"].rsplit("/", 1)[0] + "/motlist.txt"
                for number, line in enumerate(text_files.get(motlist, "").splitlines(), 1):
                    if ref["virtual_path"].rsplit("/", 1)[1] in normalized(line.split("#", 1)[0]).split():
                        motion_links.append({"path": effective[motlist], "line": number, "text": line.strip()})
                        dependencies.add(motlist)
        race_rows = races[owner]
        race_model_key = key.rsplit("/", 1)[0] + "/" + owner + ".msm"
        race_model = effective.get(race_model_key) if race_rows else None
        if race_model:
            dependencies.add(race_model_key)
        native_race_path = key.startswith(("d:/ymir work/npc/", "d:/ymir work/npc2/", "d:/ymir work/monster/", "d:/ymir work/monster2/"))
        registered = bool(race_rows and race_model and native_race_path and references and (motion_links or any(not r["virtual_path"].endswith(".msa") for r in references)))
        player_hair = category == "player hair" and any(r["path"] == "root/msm/warrior_m.msm" for r in references)
        used = "yes - registered client configuration" if registered or player_hair else "unknown - no complete active reference chain proved"
        rows.append({"path": path, "sha256": hashlib.sha256((assets / path).read_bytes()).hexdigest(),
                     "virtual_path": key, "category": category, "reader_status": status, "reason": reason,
                     "used_by_production_client": used, "effective_source_candidate": effective.get(key),
                     "race_references": race_rows, "asset_references": references,
                     "race_model_config": race_model,
                     "server_references": sorted({ref for race in race_rows for ref in server_refs[race["race"]]}),
                     "motion_list_references": motion_links,
                     "disposition": "BLOCK migration; implement and verify native semantics" if used.startswith("yes") else "unresolved; do not declare unused or safe to remove"})
    with (output / "inputs.tsv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, delimiter="\t", lineterminator="\n")
        writer.writerows(("gr2", row["path"], row["virtual_path"]) for row in rows)
        writer.writerows(("config", effective[key], key) for key in sorted(dependencies))
    inventory = {"baseline_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=source, text=True).strip(),
                 "corpus_counts": dict(Counter(row[1] for row in corpus)), "rows": rows,
                 "limits": "Configuration reachability is not a visual test or proof of a live server spawn. Unknown is never treated as unused. Source candidates require packed byte comparison."}
    if args.packed_audit:
        with args.packed_audit.open(encoding="utf-8", newline="") as stream:
            packed = {row["source"]: row for row in csv.DictReader(stream, delimiter="\t")}
        for key in dependencies:
            result = packed.get(effective[key])
            if not result or result["identical"] != "1":
                raise ValueError("unverified packed configuration: " + key)
        for row in rows:
            result = packed[row["path"]]
            row["packed_result"] = result
            if result["identical"] == "0" and result["native"] == "1":
                row["used_by_production_client"] = "no - rejected raw bytes not selected by production packs"
                row["disposition"] = "raw size-invalid copy; selected packed replacement is native-readable; keep raw rejection"
        inventory["packed_configuration_comparisons"] = len(dependencies)
        inventory["confirmed_production_rejects"] = sum(row["used_by_production_client"].startswith("yes") and row["packed_result"]["native"] == "0" for row in rows)
        columns = ["path", "category", "used_by_production_client", "reader_status", "reason", "race_ids", "map_race_item_references", "packed_native", "packed_reference", "disposition"]
        with (output / "rejected-classification.csv").open("w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=columns, lineterminator="\n")
            writer.writeheader()
            for row in rows:
                record = {key: row[key] for key in columns if key in row}
                record.update(race_ids=";".join(str(r["race"]) for r in row["race_references"]),
                              map_race_item_references="; ".join([f'{r["path"]}:{r["line"]}' for r in row["asset_references"]] + row["server_references"]),
                              packed_native=row["packed_result"]["native"], packed_reference=row["packed_result"]["reference"])
                writer.writerow(record)
    (output / "rejected-classification.json").write_text(json.dumps(inventory, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print("Rejected:", len(rows), "Usage:", dict(Counter(row["used_by_production_client"] for row in rows)))
    print("Config byte comparisons:", len(dependencies))


if __name__ == "__main__":
    main()
