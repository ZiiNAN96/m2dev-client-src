# CLEAN-X — Repository / Test / Build Cleanup

Datum: 2026-09-16. Ausschließlich `m2dev-client-src`.

## 1. Baseline

Branch `codex/g56-hdr-atmosphere`, HEAD
`4cb5b9f2c1e33e73cb772c8abde3846fba3d2532`
(`feat(graphics): add modern water rendering`). Source-Working-Tree und Index
waren sauber. G7-X ist committed. Der separate Asset-Tree hatte bereits
`assets/root/uigraphicssettings.py` geändert; dieser Stand wurde nur gelesen
und vor/nach CLEAN-X per Hash/Status kontrolliert.

## 2. Inventar

Vor der ersten Änderung: **157.786 Dateien, 378.680.010.569 Bytes** ohne das
Repository-`.git`. Zehn alte Buildbäume: **153.979 Dateien,
378.471.683.816 Bytes**. Übriger Bestand: 3.807 Dateien, 208.326.753 Bytes.
Dateilängen zählen Hardlinks mehrfach; diese Zahlen sind keine Messung des
physischen Speicherplatzgewinns. Verzeichnisverknüpfungen werden nicht verfolgt.

| Klasse | Entscheidung |
| --- | --- |
| A Production | `src`, `extern`, Vendor-Quellen und Renderer-Buildhelfer unverändert |
| B Regression | Alle bestehenden C++-/CTest-Prüfungen sowie wertvolle manuelle Szenarien behalten |
| C Development Tool | AssetTool, GR2-Corpus-/Reparaturdiagnosen und optionaler Offline-SPT-Konverter behalten |
| D Fixture/Golden | Classic-BMPs gesichert; GR2-/Vegetationsreferenzen und deterministische Fixtures behalten |
| E Milestone-Artefakt | C-LIB-X-Vergleichsframework, alte Galerien, Einmalskripte und Auditdownloads entfernt |
| F Generiert | Alte Builds, EXE/DLL/PDB, Logs, Shaderdumps, Captures, Patches und Zwischenkonvertierungen entfernt |
| G Unklar | Kein unklarer Assetbestand gelöscht oder als neu lizenzierte Fixture übernommen; lokale Spieltestdaten separat bewahrt |

## 3. Removed Generated Artifacts

Entfernt: `build`, `build-c2x-clean`, `build-c3x`, `build-clean`, `build-d1x`,
`build-e2x`, `build-f34-clean`, `build-hx-clean`, `build-hx-common` und
`build-hx-converter`. Darin lagen G1/G2/G3/4/G-DX/G-DX-C/G5/6/G7-, F5-, H-X-,
C-LIB-X- und frühere Renderer-Ausgaben einschließlich kopierter Testclients.
Vorher wurden die benötigten Eingaben und gepinnten Abhängigkeiten gesichert.
Junctions wurden als Verknüpfungen entfernt; ihre Ziele im Asset-Tree blieben
unberührt. Eine alte Zstd-Datei namens `NUL` wurde über ihren wörtlichen
Windows-Pfad entfernt. Keine pauschale Asset-Endungs-Löschung.

## 4. Removed Obsolete Tests

**Kein dauerhafter Regressionstest gelöscht.** Das leere, unversionierte
`tests/Atmosphere`-Verzeichnis entfällt; aktive Atmosphäre-/HDR-Prüfungen liegen
bereits in `tests/Graphics`. Die obsolete C-LIB-X-Messanwendung ist unter
Abschnitt 8 beschrieben. Generierte Test-EXEs werden frisch gebaut. Zwei reine historische GR2-
Berichtsgeneratoren (`prepare_gr2_migration_audit.py`,
`summarize_gr2_compatibility.py`) entfallen: fest auf den überholten Stand
9.166 Dateien / 52 Rejects und alte F3-A-Ausgaben zugeschnitten, ohne CTest-
Anbindung. Ihre fertigen Klassifikationen unter `docs/assets` bleiben erhalten.

## 5. Preserved Regression Tests

Die bestehenden funktionalen Verzeichnisse bleiben erhalten: Platform,
Renderer, Graphics, AssetRuntime, AnimationRuntime, Vegetation und AssetTool.
Abgedeckt bleiben GR2/Corpus/Rejections, GLB/Animated GLB, Animation, Skinning,
Material/PBR, DiligentFX, Licht/Schatten/SSAO, HDR/Bloom/Atmosphäre, Wasser,
Ressourcen und Fensterlebenszyklus. Kein Frameworkwechsel, kein Rename-Churn.
`tests/Graphics/g7-fast-tests.txt` heißt nun `tests/fast-tests.txt`: dieselben
66 Tests plus der dauerhafte Classic-Vergleich. Siehe `tests/README.md`.

## 6. Preserved Goldens

Zwölf vorhandene BMPs nach `tests/goldens/classic/` kopiert, jeweils vorab gegen
die eingefrorenen G7-/G-DX-SHA256-Werte geprüft. `manifest.json` behält exakt
diese Werte. `Renderer.ClassicGoldens` prüft sowohl die gespeicherten Dateien
als auch frisch erzeugte Renderausgaben, ohne Pixeltoleranz oder Auto-Update.
CTest-Fixtures erzwingen die beiden erzeugenden GLB-Tests als Voraussetzung.
GR2-Textgoldens und Vegetations-JSON-Signaturen bleiben unverändert.

## 7. Preserved Fixtures

Alle selbst erstellten GLB-/Skin-/PBR-/Error-/Offline-Import-Fixtures und ihre
Generatoren bleiben erhalten. Kleine Wasser-, Schatten- und SSAO-Szenen sind
weiterhin im Testcode definiert. Keine unbekannten Downloads übernommen.

237 konvertierte H-X-Dateien (69.521.096 Bytes) und 189 ältere Root-/Config-
Testeingaben wurden unverändert nach `test-data/` kopiert und hashgeprüft.
Sie bleiben **lokale, ignorierte Spieltestdaten**; Herkunft, Nutzungsgrenzen
und vollständige Dateihashes sind in `test-data/README.md` und `manifest.json`
dokumentiert. Keine neuen Verteilungsrechte werden angenommen. Ein frischer
Clone benötigt diesen lokalen Corpus zusätzlich. CMake erlaubt einen
alternativen Pfad über `M2_VEGETATION_TEST_DATA`.

## 8. Tool Cleanup

Die zwölf Dateien von `tools/CoreLibraryAudit/` und ihre deaktivierte
CMake-Option entfallen. Das Framework diente dem abgeschlossenen Vergleich
von ozz/ACL, Mesh- und Texturcodecs, benötigt separate umfangreiche Downloads
und ist keine Produktionsregression. Entscheidungen und versionierte
Messevidenz unter `docs/core/` bleiben erhalten; Code bleibt im Git-Checkpoint.

AssetTool, GR2-Scanner/Kompatibilitäts-/Reparaturwerkzeuge, Skinning-/Animation-
Benchmarkcode und Offline-Vegetationswerkzeuge bleiben gezielt erhalten.

## 9. CMake Cleanup

Auditoption und Unterverzeichnis entfernt. Eigene Einbindung der upstream
Zstd-Fuzzer/Benchmarks mit `ZSTD_BUILD_TESTS=OFF` deaktiviert; Vendor-Code und
Produktionsbibliotheken unverändert. Vegetationstests lesen den dauerhaften
lokalen Corpus. Classic-Vergleich als kleiner CMake-/CTest-Test ergänzt.
Keine verwaisten Auditziele, kein neues Testframework.

## 10. Gitignore Changes

Doppelte Buildregeln entfernt; vorhandenes `/build-*/` bleibt. Ergänzt:
lokale Corpus-Unterverzeichnisse, Logs/PDB/ILK/TMP/Cache/Pythoncache und klar
benannte Ausgabeordner. Keine globalen PNG/DDS/GLB/JSON-Regeln. Auch `*.obj`
wird nicht pauschal ignoriert, weil echte Wavefront-Testinputs vorhanden sind.
Prüfung: Goldens und Assetfixtures bleiben sichtbar, erzeugte Ausgaben ignoriert.

## 11. Docs Changes

Alle 107 vorhandenen Dokumentations-/Evidenzdateien unverändert behalten.
Neu: dieser Bericht, Testübersicht, Classic-Provenienz und lokaler Corpus-
Leitfaden. Alte Berichte dürfen weiterhin historische Pfade in inzwischen
entfernte Buildausgaben nennen; die Abschlussberichte selbst bleiben erhalten.

## 12. Repository Size Before/After

| Messpunkt | Dateien | Logische Dateigröße |
| --- | ---: | ---: |
| Before | 157.786 | 378,680 GB |
| After, inklusive neuer Validierung | 19.809 | 11,829 GB |
| Reduktion | 137.977 | 366,851 GB (96,88 %) |

Verbleibender Bestand:

| Teil | Dateien | Dateigröße |
| --- | ---: | ---: |
| `build-cleanup-validation/` | 8.460 | 11.152,613 MB |
| `build-cleanup-portable/` | 314 | 31,569 MB |
| `build-deps/` | 6.797 | 344,650 MB |
| `test-data/` | 428 | 72,827 MB |
| Source / Tests / Docs / sonstiger Bestand | 3.810 | 227,712 MB |

GB/MB sind dezimal. Snapshot nach den Gates, ohne Repository-`.git`,
mit derselben Messmethode wie Before. Aktuelle Buildprodukte, aktuelle
Smoke-Captures/Logs und Dependencycache bleiben bewusst erhalten. Hardlinks
werden nach Dateilänge gezählt; die Reduktion ist kein physischer
Plattenplatznachweis. Kleine nachfolgende Bericht-/Auditmetadaten ändern
die gerundeten GB-Werte nicht.

## 13. Fresh Configure

Neuer `build-cleanup-validation`, VS 2022 x64, D3D11, AssetTool ON: **PASS**.
Nach verweigerter SDK-Erkennung in der Sandbox wurde mit `cmake --fresh` im
vorhandenen VS-Entwicklerkontext neu konfiguriert. Kein alter Buildcache.
Separater neuer `build-cleanup-portable`, Cygwin GCC/Ninja/LP64: **PASS**.

`build-deps/` enthält ausschließlich die bisherigen gepinnten Quellabhängigkeiten.
DiligentCore: sauberer Pin `b036337d68be2353c9950a85929acf796b9a6d50`.
FX/Assimp/meshoptimizer: Archivhash und sämtliche 305/3.112/110 Quelldateien
gegen die gepinnten Archive geprüft. Keine Dependency-Version verändert.

## 14. Release

Fresh Build **PASS**, Fast Gate **67/67 PASS in 149,26 s**, einschließlich
Classic 12/12. Vorhandene Compiler-/Drittanbieter-PDB-Warnungen bleiben
(u. a. LNK4099, C4477/C4313 und Macro-/Optionswarnungen); keine Behauptung
eines warnungsfreien Builds.

## 15. Debug

Fresh Build **PASS**, Fast Gate **67/67 PASS in 206,00 s**, Classic **12/12
bytegleich**, Diligent **0 ERROR/FATAL**. Bestehende LNK4099/LNK4098/LNK4075-
und Compilerwarnungen bleiben ausdrücklich dokumentiert. Die isolierten GR2-
Renderprüfungen melden weiterhin bekannte nicht aufgelöste DDS-Texturpfade;
der erwartete Invalid-Image-Test meldet seine Rejection. Diese Meldungen sind
keine Diligent-ERROR/FATAL-Ereignisse; beide nativen Smoke-Fehlerlogs sind leer.

## 16. GCC/LP64

Fresh Configure und Build **PASS**; **39/39 PASS in 5,82 s**: 35 Fast-Gate-
Prüfungen plus vier portable Platform-/ABI-/Lifecycle-Prüfungen, keine Vendor-
Tests. GCC bestätigt `pointer=8 long=8 size_t=8`. Bekannter GCC-Optimiererhinweis
im unveränderten GLB-Code bleibt dokumentiert. Kein Android-Geräte- oder
Vulkan-Renderingnachweis.

## 17. Classic Goldens

Sicherung vor Cleanup **12/12 SHA256-identisch**. Frische Release- und Debug-
Vergleiche jeweils **12/12 bytegleich**. Erwartete Hashes unverändert.

## 18. Modern Smoke

**PASS** mit der frisch gebauten Release-EXE, SHA256 in der lokalen Evidenz:

- Water-Helfer: vier bestehende Ansichten (2/8/10/11), GR2-Actors, Vegetation,
  HDR/Bloom, Wasser/SSR, B1 → A1 → B1 und Resize/Restore; Exit 0 nach 37,89 s,
  928 native GR2-Reads, 1.615 Water-Frames, 1.130 SSR-Frames.
- Animated-GLB-Helfer: 20 Figuren, sechs Clips und Attachment, neun Phasen /
  neun Aufnahmen, 861 Frames; Exit 0 nach 18,42 s, Modern/HDR aktiv.
- Statisches GLB und moderne GLB-/Character-GPU-Pfade im Release-/Debug-Gate.

Das sind kurze automatisierte native Smokes; keine neue persönliche
Sichtabnahme, kein Netzwerk-Login/Relog und kein Performance-Gate.

## 19. Resource Lifetime

**PASS** für beide nativen Smokes: 32 explizit geprüfte Ressourcen-/Fehler-
Zähler jeweils null, einschließlich CPU-Deformation, GPU-Fallback, Diligent
ERROR/FATAL, Texturen/Buffer, GR2/Animation/Vegetation und Runtime-Skeletons/Clips.
Zusätzlich alle acht Renderer-Shutdown-Zeilen ausschließlich null;
`WaterRenderers=0`, `ModernRenderers=0`, `ssrFallbacks=0`. Beide
Diligent-Diagnosedateien und `syserr.txt` leer. Testclient-EXEs per SHA256
identisch zur frischen Release-EXE. Dies belegt die instrumentierten Zähler,
kein externes vollständiges Treiber-Leak-Capture.

## 20. Final Git Diff

Finale Prüfung: `src`, `extern`, `vendor`, `buildtool`, alle bestehenden
C++-Regressionstests/Fixtures und alle 107 bisherigen Docs/Evidenzdateien
unverändert. `git diff --check` PASS. Alle PowerShell-Testhelfer syntaktisch
geprüft; beide geänderten aktuellen Smoke-Helfer erfolgreich ausgeführt.
Alle 66 bisherigen Fast-Gate-Namen vollständig erhalten.

Der Diff enthält ausschließlich Ignore-/CMake-Bereinigung, zwei entfernte
einmalige Berichtsskripte, das entfernte C-LIB-X-Tool, Testpfade/-übersicht,
die umbenannte Fast-Gate-Liste, gesicherte Classic-Goldens sowie lokale
Corpus-Dokumentation/Hashes. Keine Buildprodukte oder lokalen Spielassets
werden committed. Der vorhandene Asset-Tree-Diff bleibt identisch.

Aktuelle lokale Evidenz: [Validierung](../../build-cleanup-validation/evidence/validation-summary.json),
[Release](../../build-cleanup-validation/evidence/release-tests.log),
[Debug](../../build-cleanup-validation/evidence/debug-tests.log),
[LP64](../../build-cleanup-validation/evidence/portable-tests.log),
[Runtime/Shutdown](../../build-cleanup-validation/evidence/runtime-validation.json).

## 21. Known Remaining Clutter

Bewusst behalten: aktuelle Validierungsbuilds samt Logs, gepinnter Sourcecache,
lokale Spieltestdaten und ältere nützliche manuelle Testhelfer. Nicht jede alte
manuelle Szene wird neu ausgeführt. Deren Eingabetemplates sind bytegleich
bewahrt; der OriginalWorld-Helfer baut sein privates Paket daraus neu auf.
Kein Vendor-Rewrite, keine Lizenzübernahme lokaler Spielassets, kein G8.

## 22. GO/NO-GO

**CLEAN-X = GO.** Alle verpflichtenden Build-/Test-/Classic-/Smoke-/Lifetime-
Gates bestanden; kein Production-Rendering geändert. Genau ein eigener
Cleanup-Commit mit `chore(repo): clean up tests and generated artifacts`.
Kein Push. Nach CLEAN-X STOP; kein G8 und keine weitere Grafik-/Performancephase.
