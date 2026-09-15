"""Summarize bounded native G34 captures and timings without modifying images."""
import argparse
import csv
import html
import json
import os
from pathlib import Path
import statistics

parser = argparse.ArgumentParser()
parser.add_argument("runtime", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
output = root / "build/g34x"
runtime = args.runtime.resolve()
captures = [json.loads(line[8:]) for line in (runtime / "g34x-run-0.log").read_text().splitlines() if line.startswith("capture=")]
rows = list(csv.DictReader((runtime / "benchmark-0.csv").open()))
subjects = ["Player / NPC / Weapon / Hair", "Bear / Boss", "A1 terrain / mounts", "B1 building", "B1 vegetation", "GLB idle", "GLB walk", "GLB attack", "Live settings / window"]
modes = ["Classic", "Modern G2", "Modern + Shadows", "Modern + Shadows + AO"]
fields = ["process_cpu_us", "world_cpu_us", "gpu_frame_us", "shadow_draws", "shadow_casters", "shadow_cpu_us", "ao_cpu_us", "ao_gpu_us", "shadow_bytes", "ao_bytes"]
summary = []
for stage in range(36):
    # Leave compilation/allocation and the stage transition out of the warm sanity sample.
    sample = [r for r in rows if int(r["stage"]) == stage][10:-2]
    entry = dict(stage=stage, subject=subjects[stage // 4], mode=modes[stage % 4], frames=len(sample))
    for field in fields:
        values = sorted(float(r[field]) for r in sample if r[field])
        entry[field] = dict(median=statistics.median(values), p95=values[min(len(values)-1, int(len(values)*.95))], samples=len(values)) if values else None
    summary.append(entry)
(output / "performance.json").write_text(json.dumps(summary, indent=2), encoding="utf-8")
table = ["| Scene | Mode | Frames | CPU frame ms | GPU frame ms | Shadow draws / casters | Shadow CPU ms | AO CPU / GPU ms |", "|---|---|---:|---:|---:|---:|---:|---:|"]
def metric(entry, key, scale=1):
    item = entry[key]
    return "unavailable" if item is None else f'{item["median"]/scale:.3f}'
for entry in summary:
    if entry["stage"]//4 not in (0, 3, 4):
        continue
    table.append(f'| {entry["subject"]} | {entry["mode"]} | {entry["frames"]} | {metric(entry,"process_cpu_us",1000)} | {metric(entry,"gpu_frame_us",1000)} | {metric(entry,"shadow_draws")} / {metric(entry,"shadow_casters")} | {metric(entry,"shadow_cpu_us",1000)} | {metric(entry,"ao_cpu_us",1000)} / {metric(entry,"ao_gpu_us",1000)} |')
(output / "performance.md").write_text("# G34 performance sanity\n\nMedian of warm frames at 1024 x 768, VSync on. CPU frame excludes Present and frame-limit waiting. Short local samples; p95 and exact sample counts are in performance.json.\n\n" + "\n".join(table) + "\n", encoding="utf-8")

def link(path):
    return html.escape(os.path.relpath(path, output).replace("\\", "/"), quote=True)
page = ['<!doctype html><meta charset="utf-8"><title>G3/4-X visual evidence</title><style>body{background:#11191f;color:#eee;font:16px system-ui;margin:30px}h1,h2{color:#b8dccb}button{padding:10px;margin:4px;border:1px solid #52786d;background:#243c34;color:white;cursor:pointer}section{margin-bottom:32px}.grid{display:grid;grid-template-columns:repeat(4,1fr);gap:10px}figure{margin:0}img{width:100%;height:auto}figcaption{padding:6px;background:#253139}a{color:#a8dbed}.single{grid-template-columns:1fr;max-width:1200px}.single figure{display:none}.single figure.selected{display:block}.proof{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}</style><h1>G3/4-X · Shadows & Ambient Depth</h1><p>Native A1 → B1 → A1. Original captures, fixed camera and wind. Idle subjects keep their pose; Walk/Attack are live animation samples.</p><p>Click an image for its full resolution.</p><nav><button onclick="pick(-1)">All four modes</button>']
for i, mode in enumerate(modes):
    page.append(f'<button onclick="pick({i})">{mode}</button>')
page.append('</nav>')
for subject, title in enumerate(subjects):
    page.append(f'<section><h2>{title}</h2><div class="grid">')
    for mode in range(4):
        capture = next(c for c in captures if c["step"] == subject*4+mode)
        src = link(runtime / capture["image"])
        page.append(f'<figure data-mode="{mode}"><a href="{src}"><img loading="lazy" src="{src}"></a><figcaption>{modes[mode]}</figcaption></figure>')
    page.append('</div></section>')
page.append('<h2>GPU pixel proofs</h2><p>Direction, elevation, alpha masking, same bone palette, AO-only, terrain relief and native GLB hand attachment.</p><div class="proof">')
for name in ["g34-left-shadow", "g34-right-shadow", "g34-low-shadow", "g34-ao-only", "g34-leaf-mask-shadow", "g34-skin-posed", "g34-terrain-hill-off", "g34-terrain-hill-shadow", "g34-native-clip-3-shadow"]:
    path = root / "build-hx-clean/tests/Shadows" / (name + ".bmp")
    if path.exists():
        src = link(path)
        page.append(f'<figure><a href="{src}"><img loading="lazy" src="{src}"></a><figcaption>{name}</figcaption></figure>')
page.append('</div><script>function pick(mode){document.querySelectorAll(".grid").forEach(g=>{g.classList.toggle("single",mode>=0);g.querySelectorAll("figure").forEach(f=>f.classList.toggle("selected",+f.dataset.mode===mode))})}</script>')
(output / "gallery.html").write_text("\n".join(page), encoding="utf-8")
print(output / "gallery.html")
print(output / "performance.md")
