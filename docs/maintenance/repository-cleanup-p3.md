# P3 — Repository Cleanup nach Production-GO

18.09.2026. **P3 FINAL + CLEANUP: GO.** Cleanup erst nach frischer Main-Integration,
Production-Prüfung und separatem Deployment-Dokumentationscommit
`e8fe10711e9d8805a6b7b9cd696c5408fcec5214` ausgeführt. Keine Produktänderung,
kein Push, keine Branch-Löschung und keine Folgephase.

## Main und Production

Source-Build aus sauberem main: `542722f36d7cf1c40d39b8fa2a14eb647d4718e4`.
Runtime/UI-Eingaben: `e89424f461efe6596fbace9c649d9b1b229b48eb`. Beide Closure-Branches per
Fast-Forward integriert; origin/main lag jeweils vollständig dahinter.
Closure-Commits: Source `542722f`, Runtime `e89424f4`.
Die anschließenden Cleanup-Commits enthalten nur diesen Bericht bzw. gezielte
Ignore-Regeln. Ihre endgültigen HEADs werden im finalen Git-Receipt und
Abschlussbericht festgehalten; kein selbstreferenzieller Commit-Hash behauptet.

Production-EXE SHA256: `0E45E5F5817062F9AFFC499360BE63F349824F04C331FCAACA6F898EE6176AD9`.
Production-Root-Pack SHA256: `C144F2FBADBBD5A82136208E92F26E10308A51B19982F61BFE60F9396861786A`.
Beide nach Cleanup unverändert. Persönliche Configs bleiben bytegleich zum
vor Arbeitsbeginn gesicherten Original außerhalb des Runtime-Worktrees.

## BEFORE / AFTER

| Repository | Snapshot | Dateien | Logische Bytes | Dezimale Größe |
| --- | --- | ---: | ---: | ---: |
| SOURCE | BEFORE | 119925 | 201146865487 | 201.147 GB |
| SOURCE | AFTER | 25578 | 13018520001 | 13.019 GB |
| RUNTIME | BEFORE | 54709 | 3447638888 | 3.448 GB |
| RUNTIME | AFTER | 54706 | 3447638373 | 3.448 GB |

Snapshots ohne `.git`-Metadaten; Verzeichnis-Junctions/Symlinks werden nicht
verfolgt. Hardlinks zählen je Pfad mit voller Dateilänge. Diese Werte sind
**keine Aussage über physisch freigegebenen Plattenplatz**. AFTER wurde nach
Löschung und Validierung, vor Erstellung dieses Reports aufgenommen. Neue
Receipts/Reports und laufende Buildprotokolle erklären kleinere Änderungen
gegenüber der reinen Subtraktion gelöschter Dateien.

Größte Verzeichnisse im BEFORE-Snapshot:

| Repository | Verzeichnis | Dateien | Logische Größe |
| --- | --- | ---: | ---: |
| source | `build-p0l` | 16251 | 41.854 GB |
| source | `build-g8x` | 16036 | 30.357 GB |
| source | `build-content-tx-p0` | 16652 | 29.005 GB |
| source | `build-h2x` | 13798 | 22.070 GB |
| source | `build-p2` | 6161 | 13.657 GB |
| source | `build-bugfix-x` | 5501 | 13.409 GB |
| source | `build-cleanup-validation` | 8461 | 11.153 GB |
| source | `build-p3-display-closure` | 4804 | 10.749 GB |
| runtime | `assets` | 54468 | 2.220 GB |
| runtime | `pack` | 91 | 1.066 GB |
| runtime | `bgm` | 25 | 0.084 GB |
| runtime | `(root)` | 9 | 0.075 GB |
| runtime | `mark` | 2 | 0.001 GB |
| runtime | `upload` | 100 | 0.001 GB |
| runtime | `docs` | 8 | 0.000 GB |
| runtime | `config` | 5 | 0.000 GB |

Untracked-Dateien außerhalb Ignore-Regeln vor Cleanup: Source und Runtime
jeweils 0. Vollständige getrennte Listen getrackter, untracked und ignorierter
Dateien liegen im Inventar. Kein eindeutig obsoleter getrackter Müll gefunden;
**keine getrackte Datei gelöscht**.

## Klassifizierung und Löschung

Source: **94382 Dateien / 188226127159 logische Bytes**
und 140 Verzeichnisverknüpfungen entfernt.
Runtime: **3 alte Logs / 552 Bytes** entfernt:
`renderer-startup.log`, `vegetation-runtime.log`, `log/syserr.txt`.
Diese drei Logs wurden vorher separat hashgeprüft archiviert.

Entfernte Kategorien: veraltete Testclients, CMake/MSBuild-Outputs,
Compiler-Zwischendateien, P0-L/P1/P2/P3-Profiling, Display-/Layout-Rohartefakte,
Galerien, alte EXE-Backups, Shadercache-Kopien und frühere Deployment-Arbeitsdaten.
Frische Production-Nachweise und das aktuelle Rückfall-Backup bleiben erhalten.
464 historische Berichte, Prompts, Prüf-/Deployment-Receipts
und ausgewählte frühere Config-Backups wurden vorab hashgeprüft aufbewahrt.

Inventarisierte Zielverzeichnisse:

- `build-bugfix-x` — obsolete build/test/profiling/deployment outputs
- `build-cleanup-portable` — obsolete build/test/profiling/deployment outputs
- `build-cleanup-validation` — obsolete build/test/profiling/deployment outputs
- `build-g8x` — obsolete build/test/profiling/deployment outputs
- `build-main-integration-x` — obsolete build/test/profiling/deployment outputs
- `build-p0l` — obsolete build/test/profiling/deployment outputs
- `build-p0l-final-production` — obsolete build/test/profiling/deployment outputs
- `build-p0l2` — obsolete build/test/profiling/deployment outputs
- `build-p0l3` — obsolete build/test/profiling/deployment outputs
- `build-p0l4` — obsolete build/test/profiling/deployment outputs
- `build-p0l5` — obsolete build/test/profiling/deployment outputs
- `build-p0l6` — obsolete build/test/profiling/deployment outputs
- `build-p0l7` — obsolete build/test/profiling/deployment outputs
- `build-p0l7c` — obsolete build/test/profiling/deployment outputs
- `build-p0l8` — obsolete build/test/profiling/deployment outputs
- `build-p1` — obsolete build/test/profiling/deployment outputs
- `build-p2` — obsolete build/test/profiling/deployment outputs
- `build-p2-final-production` — obsolete build/test/profiling/deployment outputs
- `build-p3` — obsolete build/test/profiling/deployment outputs
- `build-p3-display` — obsolete build/test/profiling/deployment outputs
- `build-p3-display-closure` — obsolete build/test/profiling/deployment outputs
- `build-p3-final-production` — obsolete build/test/profiling/deployment outputs
- `build-p3-main-production` — obsolete build/test/profiling/deployment outputs
- `build-p3-ui-layout-closure` — obsolete build/test/profiling/deployment outputs
- `build-h2x/deployment-20260916` — historical H2 runtime/evidence outputs
- `build-h2x/evidence` — historical H2 runtime/evidence outputs
- `build-h2x/gallery` — historical H2 runtime/evidence outputs
- `build-h2x/grass-preload` — historical H2 runtime/evidence outputs
- `build-h2x/performance` — historical H2 runtime/evidence outputs
- `build-h2x/portable` — historical H2 runtime/evidence outputs
- `build-h2x/production-final-20260916` — historical H2 runtime/evidence outputs
- `build-h2x/runtime-baseline` — historical H2 runtime/evidence outputs
- `build-h2x/runtime-deployment-check` — historical H2 runtime/evidence outputs
- `build-h2x/runtime-grass-preload` — historical H2 runtime/evidence outputs
- `build-h2x/runtime-grass-preload-final` — historical H2 runtime/evidence outputs
- `build-h2x/runtime-grass-preload-verified` — historical H2 runtime/evidence outputs
- `build-h2x/runtime-legacy` — historical H2 runtime/evidence outputs
- `build-h2x/runtime-manual` — historical H2 runtime/evidence outputs
- `build-h2x/runtime-native` — historical H2 runtime/evidence outputs
- `build-h2x/runtime-native-final` — historical H2 runtime/evidence outputs
- `build-h2x/runtime-performance` — historical H2 runtime/evidence outputs
- `build-content-tx-p0/all-maps/runtime-audit` — obsolete private terrain test client
- `build-content-tx-p0/all-maps/runtime-audit-v2` — obsolete private terrain test client
- `build-content-tx-p0/all-maps/runtime-proof` — obsolete private terrain test client
- `build-content-tx-p0/brightness-v2/runtime-c` — obsolete private terrain test client
- `build-content-tx-p0/grass004-comparison/darkgreen-2k-v3/runtime-d` — obsolete private terrain test client
- `build-content-tx-p0/grass004-comparison/darkgreen-v2/runtime-d` — obsolete private terrain test client
- `build-content-tx-p0/grass004-comparison/runtime-d` — obsolete private terrain test client
- `build-content-tx-p0/runtime-a` — obsolete private terrain test client
- `build-content-tx-p0/runtime-b` — obsolete private terrain test client
- `build-content-tx-p0/runtime-c` — obsolete private terrain test client
- `build-content-tx-p0/runtime-classic-reference` — obsolete private terrain test client
- `build-content-tx-p0/runtime-manual` — obsolete private terrain test client
- `build-content-tx-p0/terrain-test-v4/all-original-grass-v9/runtime-proof` — obsolete private terrain test client
- `build-content-tx-p0/terrain-test-v4/darkgreen-v5/runtime-proof` — obsolete private terrain test client
- `build-content-tx-p0/terrain-test-v4/grass003-replacement-v7/runtime-proof` — obsolete private terrain test client
- `build-content-tx-p0/terrain-test-v4/moss-replacement-v6/runtime-proof` — obsolete private terrain test client
- `build-content-tx-p0/terrain-test-v4/original-grass-v8/runtime-proof` — obsolete private terrain test client
- `build-content-tx-p0/terrain-test-v4/original-terrain-v10/runtime-proof` — obsolete private terrain test client
- `build-content-tx-p0/terrain-test-v4/production-deployment-v9/runtime-proof` — obsolete private terrain test client
- `build-content-tx-p0/terrain-test-v4/runtime-dungeon-control` — obsolete private terrain test client
- `build-content-tx-p0/terrain-test-v4/runtime-manual` — obsolete private terrain test client
- `build-content-tx-p0/terrain-test-v4/runtime-proof` — obsolete private terrain test client
- `build-content-tx-p0/terrain-test-v4/runtime-proof-final` — obsolete private terrain test client
- `build-content-tx-p0/gallery` — generated terrain gallery
- `build-content-tx-p0/grass004-comparison/gallery` — generated terrain gallery
- `build-p3-final-cleanup/main-layout/pack` — completed pre-deployment test client payload
- `build-p3-final-cleanup/main-layout/config` — completed pre-deployment test client payload
- `build-p3-final-cleanup/main-layout/shader-cache` — completed pre-deployment test client payload
- `build-p3-final-cleanup/pack-step-1` — intermediate root pack, audit TSV retained
- `build-p3-final-cleanup/pack-step-2` — intermediate root pack, audit TSV retained
- `build-p3-final-cleanup/pack-step-3` — intermediate root pack, audit TSV retained
- `build-p3-final-cleanup/pack-step-4` — intermediate root pack, audit TSV retained
- `build-p3-final-cleanup/pack-step-5` — intermediate root pack, audit TSV retained
- `build-p3-final-cleanup/pack-step-6` — intermediate root pack, audit TSV retained
- `build-p3-final-cleanup/pack-step-7` — intermediate root pack, audit TSV retained
- `build-p3-final-cleanup/pack-step-8` — intermediate root pack, audit TSV retained

Zusätzlich entfernt: die abgeschlossene private Main-Gate-EXE, eindeutig
generierte Python-Caches und alte Terrain-Backup-EXEs. Die Protokolle und
Bilder des neuen Main-Gates bleiben erhalten; dessen Pack-Hardlinks wurden
entfernt, ohne ihre produktiven Ziele anzutasten.

Kein `git clean -fdx`, keine pauschale rekursive Löschung. Jeder einzelne
Dateipfad wurde vorab auf Repository-Grenzen, Tracking, Größe, SHA256 und
Dateiidentität geprüft. Windows-Datei-IDs/Hardlinkzahlen wurden über `stat`
direkt am Dateipfad erfasst (Verzeichnis-Enumeration liefert dort teilweise 0).
Gelöscht wurden nur die aufgelisteten Dateien; Junctions wurden als Links
entfernt und leere Verzeichnisse einzeln entfernt. Keine Verknüpfung verfolgt.

## Bewusst behalten

- `build-deps`
- `build-h2x/msvc (existing tests/Loading launcher and offline tools)`
- `build-h2x/content`
- `test-data`
- `tests/goldens`
- `build-p3-final-cleanup (current build, proof and rollback backup)`
- `build-content-tx-p0 catalog/import/material inputs and original-restoration receipts`
- `all production runtime assets/packs/configs/executables/DLLs`

`build-h2x/msvc` bleibt bewusst erhalten: `tests/Loading/prepare_probe.ps1`
verwendet dort weiterhin Release-EXE und PackMaker; außerdem liegen dort
Offline-Asset-Tools. Dieser Pfad wird durch Cleanup nicht stillschweigend
umgebogen. Der neue Main-Release liegt unabhängig davon im aktuellen
`build-p3-final-cleanup/msvc`.

Terrain-Kataloge, Import-/Materialeingaben, ursprüngliche Terrain-Restore-
Receipts und unklare Inhalte außerhalb erkannter Testclients bleiben bestehen.
Produktive Assetquellen mit historisch benannten `.bak`-/Backup-Dateinamen
sind Content und wurden nicht als Müll behandelt. Die vorhandene Debug-EXE,
alle DLLs, `config.exe` und die zusätzliche alte Root-Config bleiben erhalten.

## Schutz und Validierung

**Protected runtime check: PASS.** 54706 produktive
Runtime-Dateien nach Cleanup bytegleich; alle 91 Packs, Assetquellen, Musik,
Mark-/Upload-Dateien, Programme und Configs geschützt. Zusätzlich
11916 dauerhafte Source-/Tool-/Dokumentations-/Inputdateien
hashgleich. 426 lokale Referenzinputs und 12 Classic-Goldens separat vor und
nach Cleanup gegen ihre bestehenden Manifeste geprüft. Kein Golden neu erzeugt.

Nach Cleanup: Release-Build PASS, dieselben **10/10 fokussierten P3-Tests PASS**.
Normaler installierter Production-Client erneut auf A1 gestartet; Settings
vorhanden, FPS-/VSync-Werte korrekt, sauberer Exit 0 und Fast Gate PASS.
Keine historische Gesamtsuite oder erneute Performancearbeit ausgeführt.

Source- und Runtime-Gitstatus nach den reinen Cleanup-/Doku-Commits clean.
Runtime behält die bereits zuvor vorhandene lokale `skip-worktree`-Ausnahme
für genau `config/graphics.cfg` und `config/metin2.cfg`; echte Benutzerbytes
sind separat hashgeprüft und keine committed Defaults. Vor Main-Integration
waren beide Worktrees ohne diese Ausnahme tatsächlich sauber.

## Ignore-Regeln und Branches

Source: bestehende `/build-*/`-, Cache-/Log-/Screenshot-Regeln reichen aus;
keine neue breite Regel. Runtime: nur `/frame-pacing.csv` und
`/map-load-trace.tsv` als exakt benannte optionale Profiling-Ausgaben ergänzt.
Keine getrackte Datei mit Ignore versteckt; keine Assets/Docs/Formate ignoriert.
Die bewusst geänderte `.gitignore` und neuen Reports sind die einzigen
trackbaren Cleanup-Änderungen; keine P3-Produktdatei im Cleanup-Commit.

Vollständig in main enthaltene Branches (nur gemeldet, alle behalten):

- Source: `codex/g56-hdr-atmosphere`, `codex/p3-display-closure`, `codex/p3-frame-pacing`, `feature/gdx-diligentfx`, `feature/p2-runtime-performance`, `main`, `remotes/origin/HEAD -> origin/main`, `remotes/origin/codex/g56-hdr-atmosphere`, `remotes/origin/feature/gdx-diligentfx`, `remotes/origin/main`, `remotes/origin/rtw1x1-patch-1`, `remotes/upstream/HEAD -> upstream/main`, `remotes/upstream/main`, `remotes/upstream/rtw1x1-patch-1`
- Runtime: `codex/g56-hdr-colors`, `codex/p3-display-closure`, `codex/p3-frame-pacing`, `main`, `remotes/origin/HEAD -> origin/main`, `remotes/origin/codex/g56-hdr-colors`, `remotes/origin/main`, `remotes/upstream/HEAD -> upstream/main`, `remotes/upstream/main`

## Nachweise

Lokale vollständige Inventare, Hash-/Hardlink-Löschliste, Einzeldatei-Receipt,
Protected-Manifest, erhaltene historische Receipts, Build-/CTest-Logs und
A1-Smoke liegen unter `m2dev-client-src/build-p3-final-cleanup/`:
`inventory-before.json`, `inventory-after.json`, `cleanup-plan.json`,
`cleanup-deleted.jsonl`, `cleanup-result.json`, `cleanup-protected-before.json`,
`cleanup-protected-result.json`, `cleanup-validation.json`,
`cleanup-smoke/fast-gate.json` und `final-integrity.json`.

**Userconfigs erhalten. Branches behalten. Push: NEIN. Client neu starten. STOP.**
