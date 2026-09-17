# P0-L â€“ Map Loading Bottleneck Analysis

Stand: 17.09.2026. Ein nachgewiesener Flaschenhals behoben. Kein Commit, kein Push. STOP.

## Ergebnis

A1 Cold: **4206.673 â†’ 3976.793 ms**, **229.880 ms weniger (5,46 %)**; Speedup **1,0578x**.

Der groesste Einzelposten vor dem Fix war die **GR2-Sektionsdekompression: 1468.934 ms / 34,92 %**. Derselbe Pfad kostet danach **1205.338 ms**: **263.595 ms / 17,94 % weniger**, 1,2187x fuer die betroffene Phase. Der gesamte GR2-Provider, inklusive Dekompression, CRC, Objektgraph, Meshes und Animationen: **2084.886 â†’ 1821.837 ms**.

Warm: **161,820 â†’ 179,567 ms**, also **17,747 ms langsamer** in diesem einzelnen Vergleich. Es gibt dort keine GR2-Parse-/Dekompressionsaufrufe. Deshalb wird kein Warm-Speedup durch diesen Fix behauptet. Rund 19 ms mehr liegen im separaten Actor-Restblock; Frame-Taktung und erste Frames schwanken ebenfalls. Keine weiteren Messwiederholungen zur Auswahl eines guenstigeren Ergebnisses.

## Messvertrag und Grenzen

- Release x64, vorhandener Diligent-D3D11-Client, native `background.LoadMap`-/Actor-/Renderpfade mit Originalpacks; privates Offline-Startskript. Drei Actors: Spieler, NPC und Mob. Keine Netzwerkanmeldung, kein voller Game-UI-Start und keine Benutzer-Sichtabnahme.
- Start der Map-Messung vor Actor-Registrierung und `background.Initialize`; bei Warm auch vor Abbau der vorherigen Welt. Ende nach drei aufeinanderfolgenden praesentierten Weltframes <=50 ms ohne neue erfasste Datei-/Parse-/GPU-/Shader-Erzeugung. A1 erreicht dies nach vier Presents. Erster Present wird separat gespeichert.
- `std::chrono::steady_clock`, auf diesem Windows-Toolchain hochaufloesend. Exklusive Zeiten ziehen Kindzeiten ab; ihre Summe plus ausgewiesener Rest entspricht TOTAL. Inklusive Elternzeiten niemals erneut aufsummieren.
- Cold bezeichnet **frischen Client-Prozess und dessen erste A1-Ladung**. Der Windows-Dateicache wurde nicht geleert. Warm ist ein echtes Destroy/Initialize/Load im selben Prozess. Kein behaupteter physischer Disk-Cold-Start.
- B1 ist optionaler Zusatz-Smoke: erster B1-Load **nach A1 in derselben Session**, danach Warm. Er ist ausdruecklich kein unabhaengiger B1-Prozess-Cold-Test.
- Die separate `client-setup`-Messung beginnt im privaten Python-Einstieg vor `app.Create`. Sie umfasst keine vorherige Prozess-/Pack-/Python-Bootstrap-Zeit und ist keine vollstaendige Client-Startzeit. Globale bereits vor dem Probe-Start geladene Properties werden nicht als Map-Load-Kosten ausgegeben.
- Die Zuordnung `cpu` bedeutet gemessene Hauptthread-Walltime in CPU-Code, keine hardwarebasierte CPU-Samplingmessung. Pack-ZSTD und moegliche Memory-Mapping-Pagefaults liegen im gemeinsamen `io+cpu`-Block. GPU-API-Zeiten messen den blockierenden CPU-Aufruf, keine unabhaengige GPU-Ausfuehrungsdauer. OS-Disk-Reads, Pagefaults, fremde Threads und Mutex-Wartezeit sind nicht separat mit ETW erfasst.
- Zwei bereits laufende Metin2-Prozesse wurden vorgefunden und nicht beendet. Das und der ungeflushte OS-Cache begrenzen statistische Aussagen. Der direkte Decoder-Bytevergleich und unveraenderte Arbeitszaehler stuetzen die Zuordnung des Gewinns.
- Einstellungen vor/nach gleich: Modern, Custom-Preset 4, Shadows 4, AO 2, Water 3, Vegetation 2, Textures 1, HDR/Bloom/ModernSky an, Sichtweite 25.600. Keine Qualitaetsreduktion.

## TOTAL LOAD TIME BEFORE / AFTER

Positives Delta = eingesparte Zeit. Ein Prozess pro Vergleich mit je einem A1-Cold-/Warm-Paar. Zwei Vorlaeufe dienten ausschliesslich Aktivierungspruefung beziehungsweise adaptiver Unterteilung des >1-s-Postens; keine statistische Benchmarkserie.

| Messung | Before ms | After ms | Delta ms | Speedup |
|---|---:|---:|---:|---:|
| client-setup | 1833.061 | 1844.124 | -11.062 | 0.9940x |
| A1-cold | 4206.673 | 3976.793 | 229.880 | 1.0578x |
| A1-warm | 161.823 | 179.570 | -17.747 | 0.9012x |
| B1-first-in-session | 445.291 | 380.047 | 65.244 | 1.1717x |
| B1-warm | 164.537 | 166.339 | -1.802 | 0.9892x |

A1 erster Present: **4171.865 â†’ 3942.846 ms**. A1 Cold/Warm-Verhaeltnis vor dem Fix 26,0x, danach 22,1x. Die grosse Differenz entsteht hauptsaechlich aus erstmaligem GR2-Laden und Shader-/FX-Erzeugung.

B1s gemessene Gesamtdifferenz ist **kein isolierter Decoder-Gewinn**: der Actor-Restblock des Vorher-Laufs schwankte um rund 60 ms. Dessen GR2-Dekompression sinkt lediglich von 103,161 auf 96,043 ms. Diese Begrenzung bleibt im Report sichtbar.

## PHASE BREAKDOWN â€“ A1 Cold, exklusive Zeiten

Gemeinsam genutzte Verarbeitung steht als eigene Phase, damit Texturen, GR2/GLB, GPU-Erzeugung und I/O nicht zugleich den aufrufenden Terrain-/Objekt-/Actor-Phasen zugerechnet werden. Die Raw-Datei enthaelt zusaetzlich die inklusiven Bereichszeiten.

| Phase | Before ms | Anteil | After ms |
|---|---:|---:|---:|
| Assets | 1692.480 | 40.23% | 1432.399 |
| Shaders / PSOs | 1504.243 | 35.76% | 1497.399 |
| Actors | 550.875 | 13.10% | 540.449 |
| First frames | 173.115 | 4.12% | 183.015 |
| File access | 114.408 | 2.72% | 115.936 |
| Other | 60.676 | 1.44% | 99.233 |
| Terrain | 51.147 | 1.22% | 50.486 |
| GPU resources | 26.232 | 0.62% | 25.180 |
| Vegetation | 19.756 | 0.47% | 18.684 |
| Static objects | 7.112 | 0.17% | 7.203 |
| Textures | 3.507 | 0.08% | 3.634 |
| Synchronization | 1.097 | 0.03% | 1.030 |
| Effects | 0.762 | 0.02% | 0.752 |
| Residency | 0.726 | 0.02% | 0.802 |
| Materials | 0.235 | 0.01% | 0.225 |
| Metadata | 0.199 | 0.00% | 0.260 |
| Sky atmosphere HDR | 0.088 | 0.00% | 0.088 |
| Water | 0.017 | 0.00% | 0.018 |
| **TOTAL** | **4206.673** | **100 %** | **3976.793** |

Terrain ist weiter in Height/Normals, Tile/Splats, Geometrie und GPU-Erzeugung zerlegt; Materialien in Terrain-Setup, Material-Erzeugung und Modern-Lookup. Objekt-Placement-Parsing und Aufbau sind getrennt. Vegetation hat Registry-Parse, Registry-/Override-Lookup, Asset-Load, Geometrie-/Material-Vorbereitung und Instanzerzeugung. Gemeinsame Buffer-/Textur-API-Zeiten werden separat ausgewiesen; diese gemeinsame Summe ist keine exakte Aufteilung nach jedem Besitzer.

Shader/FX: eigene Compile-/PSO-Aufrufe sowie benannte First-Use-Ereignisse. SSAO-/Shadow-/PostFX-Restarbeit ohne eigenen First-Use-Scope verbleibt in `First frames`; deshalb ist dies keine vollstaendige isolierte GPU-Pass-Aufschluesselung. Wasser-/Atmosphaeren-Setup wird erfasst; an den festen Kameras gibt es keine Wasser-Draws. Kein erfundener Nullkostenbeleg fuer unbenutzte Features.

## TOP 10 COSTS â€“ A1 Cold vor dem Fix

Nach exklusiver Hauptthread-Zeit sortiert, damit keine Eltern-/Kind-Doppelzaehlung entsteht.

| Kosten | Dauer ms | TOTAL % | Ursache | Einmalig / wiederholt | Main-thread blocking? | Cacheable / Fixpotenzial |
|---|---:|---:|---|---|---|---|
| 1. GR2 section decompression | 1468.934 | 34.92% | Oodle1: binaere Suche im adaptiven Symbolmodell | einmal pro Sektion; 6.648 Aufrufe | ja | session-intern bereits vermieden; einziger Fix: engere Suchintervalle |
| 2. CreateShader | 1251.504 | 29.75% | synchrone HLSL-Kompilierung, 36 eigene Shader-Aufrufe | erste Weltnutzung; warm 0 | ja | Session-Pipelines bestehen bereits; weiteres Potenzial; unveraendert |
| 3. GR2 animation curves | 414.751 | 9.86% | 693 Animationsdateien in neutrale Kurven uebertragen | einmal pro Asset | ja | Warm bereits gecacht; separater spaeterer Ansatz |
| 4. G7-SSR-first | 181.696 | 4.32% | erste DiligentFX-SSR-Vorbereitung | einmal; warm 0 | ja | bereits sessionweit; unveraendert |
| 5. world render | 173.115 | 4.12% | erste Weltframes, verbleibende Draw-/FX-Arbeit | 4 praesentierte Frames | ja | nur teilweise; unveraendert; inklusive SSAO-Restkosten |
| 6. Actors | 134.876 | 3.21% | Python-Registrierung, Actor-Erzeugung und sonstige Actor-Arbeit | 3 umschlossene Bloecke | ja | teilweise bereits sessionweit; unveraendert |
| 7. GR2 mesh | 94.442 | 2.25% | 178 Mesh-Lesevorgaenge und Validierung | einmal pro geladenem Mesh | ja | Warm bereits gecacht; unveraendert |
| 8. mapped read decompress decrypt | 79.850 | 1.90% | ZSTD/Entschluesselung/Kopie aus gemappten Packs | 6.471 logische Reads | ja | teilweise; kleine MSA-Wiederholungen; unveraendert |
| 9. unattributed / frame pacing | 60.676 | 1.44% | Frame-Taktung, Python zwischen Scopes, Messverwaltung | bis zum stabilen Present | Hauptthread; Rest nicht als CPU belegt | nicht allgemein; kein isolierter Fix abgeleitet |
| 10. GR2 header checksum | 54.024 | 1.28% | Pruefung der 831 GR2-Header und CRCs | einmal pro Datei | ja | Warm bereits vermieden; Sicherheitspruefung bleibt erhalten |

## ROOT CAUSE #1 und einziger Fix

831 GR2-Dateien werden einmalig verarbeitet, darunter 693 Animationsdateien. 6.648 Sektionsaufrufe kosten zusammen 1,469 s. Im adaptiven Oodle1-Symbolmodell wurde fuer jedes Symbol eine binaere Suche ueber die gesamte aktuelle kumulative Wahrscheinlichkeitstabelle ausgefuehrt.

`GR2Compression.cpp` baut nun **nur beim ohnehin erforderlichen Tabellen-Rebuild** ein kleines Verzeichnis von 65 Intervallgrenzen auf. Bei Modellen ab 16 Eintraegen sucht dieselbe `upper_bound`-Operation nur noch im mathematisch passenden Teilintervall. Kleine Modelle behalten die vorherige Suche. Das Verzeichnis ist lokal zum Decoder; es ist kein dauerhafter Assetcache und kein neuer Importpfad. Alle Dateigrenzen, CRCs, Decoder-Checks, Formate, Animationsdaten und Runtime-Grenzen bleiben erhalten.

Direkter Vergleich des alten und neuen Decoders: **903 reale Dateien, 7.224 Sektionen, 60.042.028 ausgegebene Bytes exakt gleich**. Alle 903 Dateipfade und Groessen entsprechen den im A1/B1-Trace verwendeten GR2s; der Vergleich verwendet deren lose Originaldateien. Keine SHA-Identitaet dieser losen Dateien mit jedem gepackten Payload behauptet. **2.226 gekuerzte nichtleere Sektionen von beiden Implementierungen abgelehnt**. Ein Durchlauf: **1.611,31 â†’ 1.342,22 ms**, ohne I/O im Decoder-Timer. Das ist ein gezielter Kompatibilitaetsnachweis und kein langer Benchmark oder eine komplette Testsuite.

## I/O, Cache und doppelte Arbeit

A1 Cold, vor/nach identische Zaehler:

| Zaehler | Wert |
|---|---:|
| logische File-Requests | 7.388 |
| Existenzabfragen | 1.245 |
| Pack-Reads | 6.471 |
| komprimierte Pack-Payloadbytes | 50.479.778 |
| ausgegebene Pack-Bytes | 89.464.817 |
| lose Open-Versuche | 917 |
| erfolgreiche lose Payload-Reads | 0 |
| Shared-Resource-Lookup-Aufrufe | 6.508 |
| tatsaechliche Resource-Loads | 1.204 |
| GR2-Parse / eindeutige Pfade | 831 / 831 |
| GLB-Parse / eindeutige Pfade | 15 / 15 |
| Texture-Source-Decode-Aufrufe / eindeutige Pfade | 243 / 243 |
| direkte Terrain-Texture-Loads | 7 |
| Vegetation-Requests / tatsaechliche Loads | 2.782 / 15 |
| Vegetation-Registry-Parse | 1 |
| Area-Builds / eindeutige Sektoren | 20 / 20 |
| erfasste GPU-Buffer-/Texturerzeugungen | 1.969 / 64 |

Die Packs sind gemappt. Ein logischer Read ist keine neue physische Dateioeffnung und keine Aussage ueber Disk-Reads. 917 lose Open-Versuche sind keine 917 erfolgreich geoeffneten Dateien. Alle per-Datei-Zaehler liegen im TSV/JSON; physische OS-Oeffnungen/Reads wurden nicht nachtraeglich aus diesen Werten erfunden.

Vegetation hat damit 2.767 aus dem geladenen Bestand beantwortete Anfragen, keine Ladefehler. A1 Warm: 2.590 Anfragen, 15 Loads nach dem Map-Abbau, 2.575 Cache-Antworten innerhalb dieses Loads. Resource-Load-Trigger Warm: 421 Anfragen, 302 Hits, 119 Misses/Loads. Diese Trigger sind nicht mit allen `GetResourcePointer`-Abfragen gleichzusetzen. Pro-GR2-Requestzahlen aller bereits referenzierten Ressourcen sind nicht separat aufgezeichnet; die **tatsaechlichen** Parsezahlen sind exakt.

Keine mehrfachen GR2-/GLB-Parses, Texture-Source-Decodes, Registry-Parses oder Area-Builds innerhalb eines Loads. Kleine `.msa`-Motion-Skripte werden mehrfach aus Packs gelesen, teils achtmal, z.B. `pc/assassin/action/slap_hit.msa`. Die gesamte Pack-Verarbeitung kostet nur 79,850 ms. Kein weiterer Cache eingebaut.

Warm werden Vegetation-GLBs nach dem Map-Abbau erneut gelesen, A1 rund 12 ms; das ist Arbeit zwischen zwei Loads, kein 1.000-faches Parsing desselben Assets innerhalb eines Loads. Die erste Sichtbarkeitsaktualisierung fuehrt keinen zweiten vollstaendigen Terrain-/Area-Aufbau aus: 20 Sektoren, keine wiederholten Area-Pfade; Terrain-Geometrie ebenfalls 20 Builds.

## Vegetation / Grass

Die zuvor entfernte vollstaendige 3D-Gras-Vorbereitung ist im Ausgangsstand bereits weg: `CMapOutdoor::Load` ruft sie nicht mehr auf. `GrassCandidates`, `PlaceGrass` und `LoadGrassData` besitzen im Produktionsquelltext keine Aufrufer; der instrumentierte Grass-Source-Pfad wird im Test nicht betreten. Kein Grass-Asset wird vom Trace geladen. Baeume und Buesche bleiben im bestehenden Registry â†’ ZVEG â†’ GLB â†’ Runtime-Pfad.

Der Lauf erzeugt insgesamt 1.370 Vegetationsinstanzen ueber die vier Map-Loads und meldet 132 Instanzuploads; keine Vegetation-Fehler und am Shutdown keine Assets, Instanzen, Geometrien oder Instanzbuffer mehr. Diese Gesamtlaufzaehler werden nicht als per-Map-GPU-Unterzeit ausgegeben. Kein Grasfix war erforderlich und es wurden keine Baeume oder Buesche reduziert.

## Shader / Texturen / Hauptthread

- Client-Setup: 48 direkte Shader-Compile-Aufrufe und 68 direkte PSO-Erzeugungen. A1 Cold: 36 / 24. A1 Warm: 0 / 2; B1 erster Load 0 / 2; B1 Warm 0 / 0. Namen und Einzelzeiten im First-Use-Log. Zwei ModernVS-Varianten liegen bei etwa 300 ms. Die Direct-API-Zaehler erfassen eigene Rendereraufrufe, **nicht** jede interne DiligentFX-Shader-/PSO-Erzeugung. Deren Kosten sind in den benannten FX-Gesamtzeiten beziehungsweise ersten Frames enthalten.
- Die teuren Modern-, Sky-, HDR-/Bloom- und SSR-Initialisierungen entstehen nicht bei jedem Map-Load erneut. Nach dem Fix ist die Shader-Kompilierung der groesste verbliebene Einzelposten. Sie wurde nicht optimiert.
- Die aktuellen Originalclients verwenden bereits wieder die Originalterraintexturen. Es gibt keine aktiven neuen 2K-Terrain-Overrides; kein Vergleich gegen geloeschte Ersatzdateien. Die aktuelle Texturverarbeitung kostet zusammen nur 3,515 ms exklusiv (vorher). DDS-Mips werden als bestehende Subresources durchgereicht. Nicht-DDS wird ueber den bestehenden STB-Pfad dekodiert; kein nachgewiesener teurer Mip-/Formatumbau und kein kompletter Texturpipeline-Umbau.
- Hauptthread-Klassifikation A1 vor dem Fix: CPU-Code-Walltime 4.000,467 ms, File-Lookup/loose-I/O 34,559 ms, gemappte Pack-Verarbeitung inkl. Dekompression 79,850 ms, GPU-API 30,025 ms, erfasster Present-Wait 1,097 ms, Rest 60,676 ms. Die Summe ist TOTAL. Das ist keine Behauptung, saemtliche Pagefault-/Scheduler-/Treiberwartezeit physisch getrennt zu haben.
- `WaitForIdle` im eigenen D3D11-Backend liegt bei Screenshot und Shutdown; beide sind nach dem Messendpunkt. `Flush` liegt im Shutdown. Kein solcher Wait wurde aus dem Ladepfad entfernt. Kein nachgewiesenes blockierendes Future/Join und kein aufgeloester Mutex-Konflikt im untersuchten synchronen Pfad. Mutex-Wartezeiten wurden nicht gesondert gesampelt.

## FAST GATE und Auslieferung

**PASS:** relevanter frischer Release-Build; A1 Cold/Warm; optional B1 erster/Warm; kurzer nativer World-/Actor-Smoke; ExitCode 0. Kein Debug-Build, keine komplette Testsuite, kein langer Benchmark und kein umfangreicher Visual-Test.

**0:** Diligent ERROR/FATAL, CPU-Deformation, GPU-Fallbacks sowie saemtliche 30 in `fast-gate.json` aufgefuehrten Fehler-/Lifetimezaehler. Zusaetzlich Modern/Water-Renderer am Shutdown 0, statische Adapterinstanzen 0 und leeres `syserr.txt`. Eigene Legacy-Lifetime-Logs ebenfalls ohne verbliebene gepruefte Geometrie-/Texturressourcen. Bestehende Vendor-PDB-/LTCG-Linkerwarnungen sind kein warning-free Build.

Arbeitszaehler pro Map sind vor/nach gleich; das belegt, dass keine Terrainsektoren, Assets oder direkten GPU-Erzeugungen zur Zeitersparnis weggelassen wurden. A1-Vorher/Nachher und B1-Nachher wurden kurz gesichtet; Terrain, Vegetation und Actors sind vorhanden. Keine Benutzer-Sichtabnahme behauptet.

Getesteter Release in den **normalen Originalclient** uebertragen, Hashvergleich erfolgreich:

`061ff2302aab15b915380bb444d899c0a1b650a3debff160f714b4155b1e2a1a`

Vorherige EXE gesichert unter `build-p0l/deployment/backup/Metin2_Release.exe`, SHA256 `6ed6c0047cbd7dd6f4a8deca35b3a35c72b7040e4a4580bbabfc798f4afda1ce`. Alle **97 geschuetzten Pack-/Konfigurations-/Debug-Dateien unveraendert**. Der normale Client wurde nicht fuer eine Netzwerkanmeldung gestartet; der native Messlauf liegt im isolierten Probeverzeichnis, die installierte EXE ist bytegleich.

**Client neu starten**, damit der normale Client den neuen Release verwendet.

## Nachweise

- [Vorher: vollstaendiger Trace](../../build-p0l/detail-before/map-load-trace.tsv), [JSON](../../build-p0l/detail-before/summary.json), [Fast Gate](../../build-p0l/detail-before/fast-gate.json).
- [Nachher: vollstaendiger Trace](../../build-p0l/after/map-load-trace.tsv), [JSON](../../build-p0l/after/summary.json), [Fast Gate](../../build-p0l/after/fast-gate.json).
- [Erste Bottleneck-Ermittlung](../../build-p0l/measured-before/summary.json): A1 4.204,616 ms; anschliessend nur GR2 weiter unterteilt.
- [Decoder-Bytevergleich](../../build-p0l/decoder-compare.txt), [Eingabeliste](../../build-p0l/decoder-files.txt), [Groessenabgleich](../../build-p0l/decoder-input-size-parity.json).
- [Release-Buildlog](../../build-p0l/build-final.log), [Deployment-Receipt](../../build-p0l/deployment/receipt.json).
- [A1 vorher](../../build-p0l/detail-before/p0l-A1-cold-0917_094027.jpg), [A1 nachher](../../build-p0l/after/p0l-A1-cold-0917_094650.jpg).

Weitere Hinweise: Shader-Kompilierung, Kurvenaufbereitung, kleine doppelte MSA-Reads und erneuter Vegetation-Load nach Map-Abbau. **Nicht weiter behoben.** Kein Commit. Kein Push. STOP.
