# Repository root structure cleanup

18.09.2026. **REPO-STRUCTURE-CLEANUP: FINAL GO.** Beide Linkfehler sind
durch zwei deklarative CMake-Abhängigkeiten behoben. Fast Gate und vollständiger
Release-Build bestehen. Der Abschluss erfolgt in einem Source-Commit auf `main`.
Kein Push, kein Deployment, keine Änderung an Rendering-/Gameplay-Logik oder
Third-Party-Quellcode.

## Startzustand

Source-Checkout: `m2dev-client-src`, Branch `main`, Worktree sauber.
Letzte Commits vor Beginn:

```text
e83dd0a chore(repo): remove generated artifacts and harden ignores
542722f fix(display): stabilize UI layout across display changes
1bd011c feat(display): add configurable resolution and display modes
```

Der separate normale Client wurde ausschließlich zum Schutzvergleich gelesen.
An dessen Git-Index, Programmen, Packs, Assets und Benutzerkonfigurationen wurde
nichts geändert. Keine Branches wurden gelöscht.

## BEFORE: Inventar und Klassifizierung

Alle vier Ordner waren **untracked und vollständig durch `/build-*/` ignoriert**;
kein enthaltener Pfad war im Source-Repository getrackt. Größen sind logische
Dateilängen, einschließlich Hardlinks je Pfad, keine Aussage über physischen
Speichergewinn. Die erste Bestandsaufnahme zählt Git-Metadaten mit.

| Root-Ordner | Dateien | Bytes | Zweck / wichtige Unterordner | Dateitypen |
| --- | ---: | ---: | --- | --- |
| `build-content-tx-p0` | 808 | 240821559 | Terrain-Versuche; `terrain-2k-catalog`, `terrain-test-v4`, `all-maps`, `evidence`, `replacement-texture-cleanup` | JPG, WEBP, JSON, Python, Logs, Markdown, HTML, CSV |
| `build-deps` | 6797 | 344650379 | Vier Dependency-Quellbäume; Core inklusive Git- und xxHash-Metadaten | C/C++, Header, CMake, Lizenzen, upstream Testmodelle, Git-Metadaten |
| `build-h2x` | 7978 | 9919091019 | Alter `msvc`-Build, Offline-Tools, `content`-Priorisierungsbericht, temporäre Skripte | OBJ, TLOG, VCXPROJ, PDB, LIB, EXE, CMake, PPM |
| `build-p3-final-cleanup` | 5699 | 2283110804 | `msvc`, Prüf-/Cleanup-Receipts, ausgepackte Root-Pack-Zwischenstände, Shadercache, aktuelles Rückfall-Backup | OBJ, TLOG, GLB/ZVEG-Packkopien, Python, JSON, Logs, VCXPROJ, Shadercache |

| Ordner | Build-/Test-/Tool-Verwendung | Dauerhafte Inhalte | Vollständig reproduzierbar? |
| --- | --- | --- | --- |
| Content TX | Kein aktiver Build-/Test-/Tool-Verweis; zahlreiche historische Dokumentlinks | Auswahlentscheidungen, Quellen-/Importmetadaten, Lösch- und Restore-Belege, ausgewählte verlinkte Prüfaufnahmen | Nein: historische Entscheidungen und Nachweise sind einmalig; abgeleitete Vorschauen und Arbeitsstände sind entbehrlich |
| Dependencies | Alle vier Quellen wurden zuvor über manuelle `FETCHCONTENT_SOURCE_DIR_*`-Argumente eingebunden; Assimp/meshoptimizer nur bei eingeschalteten Offline-Tools | Gepinnte Drittanbieterquellen und Lizenzen | Ja, anhand der vorhandenen offiziellen Git-Pins; Quellen werden trotzdem erhalten |
| H2 | `tests/Loading/prepare_probe.ps1` las Release-Client und PackMaker; Content-/Testanleitungen nannten den alten Pfad | Zwei Content-Priorisierungsberichte | Buildprodukte ja; historische Berichte werden erhalten |
| P3 Cleanup | Aktueller, aber nicht portabler Cache; alte lokale Prüf-/Deployment-Skripte, keine aktive Buildsystem-Abhängigkeit | Abschlussbelege einschließlich `final-integrity.json`; individuelles Rückfall-Backup | Build-/Pack-Zwischenstände ja; tatsächliche Prüfbelege und Benutzer-Backups nein |

`external/` existierte vorher nicht. Die bestehenden Bereiche `vendor/` und
`extern/` enthalten keine zweite Kopie dieser vier Dependencies und bleiben
unverändert. Die Assimp-`.obj`-Dateien sind upstream 3D-Testmodelle, keine
Compilerobjekte; sie wurden unverändert erhalten.

## AFTER

| Ordner | Ergebnis | Begründung |
| --- | --- | --- |
| `build-content-tx-p0` | **deleted**, relevante Belege **moved** nach `docs/content/evidence/` | 167 dauerhafte Textbelege archiviert, vorhandene direkt verlinkte Belege und abschließende Originalterrain-Nachweise separat lesbar; alte Materialvorschauen, Hilfsskripte und Proof-Arbeitsstände entfernt |
| `build-h2x` | **deleted**, zwei Berichte **moved** nach `docs/vegetation/evidence/h2-content/` | Alte Buildprodukte entfallen; Loading-Helfer nutzen den aktuellen Build |
| `build-p3-final-cleanup` | **deleted**, Belege **moved** nach `docs/maintenance/evidence/` | 640 historische Textbelege/Rezepte archiviert, ausgewählte Abschluss-JSONs separat; alte Builds, Packkopien, Shadercache, redundante Hash-Caches und Screenshots entfernt |
| `build-deps` | **source → external/**, alter Root-Ordner entfernt | Keine Buildoutputs im alten Ordner; Quellen ohne zweite Arbeitskopie verschoben und als gepinnte Submodule vorbereitet |

14.485 Dateien der drei alten Arbeitsverzeichnisse wurden nach individuellem
SHA256-Abgleich entfernt. Die Quellenverschiebung ist davon getrennt.
Kein altes Root-`build-*`-Verzeichnis verbleibt.

Das aktuelle Rückfall-Backup sowie historische persönliche Config-Sicherungen
bleiben lokal unter `build/backups/p3-final-cleanup/` erhalten. Dort liegen auch
die früheren Verzeichnis-Metadaten-Snapshots. Diese ausdrücklich klassifizierten
Sicherungen sind ignoriert, kein zweiter produktiver Client und kein Deployment.

## Dependencies und Buildstruktur

Die vier lokalen Quellen stimmen mit den bisherigen Pins überein:

| `external/` | Pin |
| --- | --- |
| `DiligentCore` | `b036337d68be2353c9950a85929acf796b9a6d50` |
| `DiligentFX` | `cb380ac52100672b5762f595acfb6609e0ecc248` |
| `assimp` | `fb375dd8c0a032106a2122815fb18dffe0283721` / v6.0.2 |
| `meshoptimizer` | `6daea4695c48338363b08022d2fb15deaef6ac09` / v0.25 |

Core behält seinen gepinnten xxHash-Unterbaum. Bei Assimp wurden 25 im alten
Archiv fehlende upstream Git-/GitHub-Metadatendateien aus demselben Pin ergänzt;
kein vorhandener Quellinhalt wurde verändert. Alle **6.740 ursprünglichen
Dependency-Dateien ohne Git-Verweisdatei sind SHA256-identisch**. Für die drei
zuvor archivbasierten Quellen wurden Git-Metadaten ergänzt, keine neuen Versionen.

`.gitmodules`, die vier gepinnten Gitlinks und die
[Initialisierungshinweise](../../external/README.md) gehören zum Abschlusscommit.
Die lokalen Dependency-Repositories sind sauber; ihre HEADs entsprechen der
Tabelle. Die bestehenden Quellen wurden ohne zweite Arbeitskopie übernommen.

CMake nutzt `external/` automatisch; bestehende explizite Source-Overrides bleiben
verfügbar. Beim Standardbuild `-B build` liegen DiligentCore-, Assimp- und
meshoptimizer-Intermediates unter `build/deps/`. DiligentFX wird wie bisher nur
als angepasste, reproduzierbar generierte Teilmenge unter `build/src/Renderer/`
gebaut; es erhält keine zweite unveränderte Source-Kopie. Andere Binärverzeichnisse
werden weiterhin relativ zum gewählten CMake-Binary-Root unterstützt.

Die Loading-Helfer verwenden `build/`, optional `-BuildDirectory`, und legen
ihre privaten Ausgaben unter `build/loading/` ab. Die betroffenen Build- und
Content-Anleitungen wurden angepasst. Kein aktiver Build-/Test-/Tool-Verweis
auf einen der vier entfernten Root-Ordner bleibt bestehen.

`.gitignore` ignorierte `build/` bereits vollständig, damit auch `build/deps/`,
Loading-Ausgaben, Profiler-/Shadercache-Ausgaben und lokale Backup-Binaries in
diesem Bereich. Keine redundante oder zusätzliche breite Ignore-Regel wurde
eingeführt. Historische `/build-*/`-Regeln bleiben als Kompatibilitätsschutz.
Rohe Logs und Profiling-Traces werden nicht committed; die bestehenden
Ignore-Regeln dafür bleiben erhalten.
`src/`, `external/`, `tests/`, `docs/`, `tools/`, Content und Goldens werden nicht
durch neue Regeln verborgen. Neue operative Pfade sind repository-relativ;
unveränderte historische Archivbelege behalten ihre damaligen lokalen Pfade.

## Root Cause und gezielter Fix

Der ursprüngliche vollständige Release-Build scheiterte mit `LNK2001/LNK1120`:

- **Diligent-GraphicsEngineD3D11-shared:** Der durch unsere
  `ShaderLoadAudit.cmake` instrumentierte `RenderDeviceD3D11Impl.cpp` verwendet
  über `ShaderLoadAudit.h` fünf `XXH128State`-Methoden. Deren Implementierung
  liegt in `Diligent-GraphicsTools`; diese Library fehlte in der Link-Abhängigkeit
  des instrumentierten statischen D3D11-Targets. Die DLL erbte daher eine
  unvollständige Linkkette. Fix in unserer Integration:
  `target_link_libraries(Diligent-GraphicsEngineD3D11-static PRIVATE Diligent-GraphicsTools)`.
- **WideIndexRenderTest:** `DiligentD3D11Backend.cpp` ruft
  `Platform::Time::MonotonicNanoseconds()` auf, implementiert in `M2Platform`.
  Dem Target `M2RendererDiligent` fehlte diese deklarierte Abhängigkeit.
  Fix am verursachenden Target:
  `target_link_libraries(M2RendererDiligent PRIVATE M2Platform)`.

Die Symbolanbieter sind mit `dumpbin` in den bereits vorhandenen Release-Libraries
nachgewiesen. Im normalen Client wurden sie zuvor indirekt über FX bzw. den
Client/Bootstrap eingebunden. Die beiden alleinstehenden Linkziele hatten diese
zusätzlichen Abhängigkeiten nicht. Nach dem Fix enthalten ihre generierten
Release-Projekte die richtigen Libraries und Build-Reihenfolgen automatisch.

**Kein Umzugsschaden:** Die fehlenden Dependency-Deklarationen bestanden bereits
im Startcommit `e83dd0a`. Source-, Output- und Import-Library-Pfade unter
`external/` bzw. `build/deps/` waren korrekt; keine Referenz auf alte
`build-deps`-/H2-Artefakte und keine falschen Debug-/Release-Namen gefunden.
Kein historischer Baseline-Build behauptet. Keine manuellen Library-Kopien,
kein Test-Sonderlink und kein Upstream-Quellcodefix.

## Abschließende Gates

Vor dem Fast Gate wurden ausschließlich die Release-Intermediates und vorhandenen
Linkausgaben der beiden betroffenen Targets entfernt und frisch erzeugt. Die
Dependency-Quellen blieben erhalten. Anschließend wurden Client und dieselben
Testprogramme neu gebaut. Die Testauswahl ist unverändert; keine zusätzliche
historische Suite, keine Performance- oder Production-Prüfung.

| Gate | Ergebnis |
| --- | --- |
| CMake-Konfiguration / Diligent und DiligentFX gefunden | PASS |
| `Diligent-GraphicsEngineD3D11-shared` inkl. DLL und Import-Library | PASS |
| `WideIndexRenderTest` | PASS |
| Release-Client `UserInterface` und benötigte Tools/Testprogramme | PASS |
| Dieselben 22 Cleanup-Tests einschließlich Fixture-Abhängigkeiten | **22/22 PASS, 6,42 s** |
| Classic-Referenzbilder innerhalb dieser Auswahl | 12/12 bytegleich |
| Vollständiger Release-Build nach bestandenem Fast Gate | **PASS, Exit 0** |
| Root-Struktur, Ignore-Sichtbarkeit und Diff-Prüfung | PASS |

Exakt derselbe zuvor fehlgeschlagene Gesamtbuild wurde in der VS-2022-x64-Umgebung
erneut ausgeführt; beide zuvor fehlgeschlagenen Targets sind enthalten:

```text
cmake --build build --config Release --parallel 6
```

Bekannte Third-Party-/PDB-/LTCG-Warnungen bleiben bestehen; keine Behauptung eines
warnungsfreien Builds. Keine fehlenden Includes oder Libraries im finalen Build.

## Integrität und Commit-Inhalt

Die bereits dokumentierte vollständige Cleanup-Hashprüfung bleibt erhalten:
54.707 Runtime-Dateien, 6.740 ursprüngliche Dependency-Dateien und die geschützten
Source-/Corpus-/Golden-Eingaben waren unverändert. Im Abschlussauftrag wurden
ausschließlich die beiden oben genannten CMake-Linkdeklarationen zusätzlich
geändert; keine produktive C/C++-Implementierung und keine Dependency-Quelle.
Die Dependency-Gitstände und Pins wurden nochmals geprüft. Runtime-EXE,
`pack/root.pck`, `config/graphics.cfg` und `config/metin2.cfg` sind vor/nach dem
Abschluss hashgleich; Runtime-HEAD bleibt `f0c25d5016603cf750b7c5ac5852e9207db274a7`,
Branch `main`, Worktree clean.

Die dauerhaften Archive enthalten Berichte, strukturierte Receipts und
Reproduktionsrezepte. Rohe `.log`-Dateien und Profiling-Traces wurden für den
Commit ausgeschlossen, bleiben aber vollständig und hashgeprüft lokal unter
`build/root-cleanup/final-gate/uncommitted-evidence/` erhalten. Das betrifft auch
das früher direkt verlinkte Terrain-Testprotokoll. Archivierte historische
Auswahlbilder bleiben Dokumentation; keine neue Galerie wurde erzeugt.

Keine Build-Binaries, Shadercaches, Testclients, Backup-EXEs oder temporären Dumps
gehören zum Source-Commit. Die Archive enthalten ebenfalls keine solchen
Nutzlasten. Die lokalen Rückfall-Backups bleiben unter `build/backups/`.

## Belege und Abschluss

- [Maschinenlesbarer Abschluss](evidence/root-structure-cleanup/validation.json)
- [Inventar, Lösch-/Verschiebe-Receipts und Integritätsnachweise](evidence/root-structure-cleanup/audit.zip)
- [Historische Terrain-Belege](../content/evidence/content-tx-p0.zip)
- [Historische P3-Belege](evidence/p3-final-cleanup.zip)
- [Historisches final-integrity.json](evidence/p3-final-cleanup/final-integrity.json)

Vollständige lokale Schutzinventare und Arbeitsprotokolle liegen zusätzlich
unter `build/root-cleanup/`. Historische Belege dokumentieren damalige Zustände,
keine neue Production-Abnahme. Der aktuelle Runtime-Gitstatus ist weiterhin clean.

**Abschlusscommit:** `chore: finalize repository structure cleanup` auf Source
`main`, nach bestandenem Final Gate und geprüftem Diff. Der zugehörige Hash ist
im Git-Log dieses Berichts nachvollziehbar. Keine Runtime-Änderung und kein
Runtime-Commit. Push: **NEIN**. Deployment: **NEIN**. Keine Folgephase. **STOP.**
