# P0-L6 – Bounded file-level GR2 preparation

## P0-L6 SUMMARY

17.09.2026: **GO im angeforderten kurzen Offline-Gate.** Genau ein funktionaler Fix: begrenzte, synchrone Vorbereitung unabhängiger GR2-Dateien aus der vorhandenen Gebäude-/Prop-Liste eines Areas. A1 Cold **2261.090 → 2219.191 ms**, gemessene Ersparnis **41.900 ms / 1.853 %**. Kein Push; STOP nach Commit.

Source: `m2dev-client-src`, Branch `codex/g56-hdr-atmosphere`, Basis **73dc92e**. Die drei Startcommits waren `73dc92e`, `5563d78`, `611cd50`. Runtime/assets: separates `m2dev-client`, Branch `codex/g56-hdr-colors`, HEAD `548c43b4`. Vorbestehende untracked Dateien: Source-Bericht `p0l3-shader-pso-breakdown.md` und beide Runtime-Deploymentbelege. Sie sind SHA256-identisch erhalten und werden nicht gestagt. Kein Deployment in den Originalclient.

**Bewusst begrenzter produktiver Scope:** A1 **98 von 831** GR2-Dateien parallel, B1 **67 von 72**; insgesamt **165 von 903**. Die 693 Animationen werden im aktuellen Player-Startup einzeln über Python → `RegisterCacheMotionData` → `LoadStaticCache` synchron angefordert. Sie bleiben seriell. Ihre Parallelisierung würde eine zusätzliche Gruppierungs-/API-Grenze benötigen; der synchrone Vertrag wird nicht durch implizites Streaming verändert. Der vorhandene Area-Instanzaufbau hat bereits eine vollständige Liste und einen sicheren Wartepunkt. Es wird nicht behauptet, alle 903 Dateien produktiv parallelisiert zu haben.

## THREAD SAFETY

Vor Implementierung geprüft: `GameThreadPool`, `FileLoaderThread`, `AreaLoaderThread`, `ResourceManager`, `Resource`, `Thing`, GR2 File/Compression/Types/Reader/Provider und `RuntimeSkeleton::Initialize`.

| Stufe / Zustand | Klassifikation und Entscheidung |
| --- | --- |
| Pack/VFS und Eingabepuffer | **MAIN/OWNER THREAD ONLY in diesem Fix.** PackManager besitzt einen Mutex, aber der vollständige Datei-/Diagnosepfad wird nicht als parallel sicher vorausgesetzt. Ein Read pro neu vorbereiteter Datei; eigener Payloadbesitz bis zum Ende des Batches. |
| Header/CRC, Sections, Oodle1/SSE2, Relocations | **THREAD LOCAL.** `File`, Section-Vektoren, Decoder, adaptive Symboltabellen und Scratchdaten gehören jeweils einem Dateijob. CRC-Tabelle ist nach thread-sicherer statischer Initialisierung unveränderlich. Der P0-L5-Decodercode bleibt bytegleich. |
| Types, Mesh-/Material-Metadaten, Skeleton-/Kurvenparsing | **THREAD LOCAL.** Lokale Maps/Vektoren/Referenzen; keine Materialregistry oder GPU-Aufrufe. Eingabereferenzen werden vor Rückgabe in besitzende `Contents` überführt. |
| CPU RuntimeSkeleton | **THREAD LOCAL / THREAD SAFE counters.** Bone-Reihenfolge unverändert; BindingId ist ein deterministischer Inhalts-Hash, kein globaler Vergabezähler. Lebensdauerzähler sind atomar. |
| MapLoadTrace | **THREAD LOCAL.** Eigener Workerzustand; Rückgabe und Zusammenführung erst nach allen `future.get()`. Keine Worker-Dateiausgabe. Bestehender Threadzustand wird per RAII wiederhergestellt. |
| AnimationStallAudit | **MAIN THREAD ONLY.** Gemeinsame mutable Diagnose-/Framezähler. Bei aktivem Audit bleibt der komplette bestehende serielle Pfad erhalten; kein Worker greift schreibend darauf zu. Kein Umbau der Audit-Architektur. |
| ResourceManager, Pfadbuffer, CResource-State, Referenzen, Cache | **MAIN/OWNER THREAD ONLY.** Keine neuen Workerzugriffe. Bestehende CResource-Identität und `STATE_EXIST`/`STATE_ERROR` bleiben maßgeblich. |
| Document-Erzeugung, Clip-Binding/Cache, Motion-/Materialadapter | **MAIN/OWNER THREAD ONLY.** Erst nach abgeschlossenem CPU-Batch in ursprünglicher Instanz-/Ladefolge. |
| Diligent/GPU-Erzeugung und Upload | **RENDER/OWNER THREAD ONLY.** Unveränderter bestehender Aufrufpfad; kein Rendererobjekt und kein GPU-API-Aufruf im Job. |
| Fehler, Logging | Dateilokales `exception_ptr`; Fehlerdiagnose erst beim Owner-Publish. Provider liefert weiter `InvalidAsset`, Resource wird `STATE_ERROR`. Executor-/Futurefehler werden nach Join aller schon gestarteten Jobs weitergeworfen. |

Es gibt keinen neuen Pool. Der vorhandene `CGameThreadPool` besitzt hier 16 Threads auf 16 logischen CPUs. Für einen Batch werden höchstens **min(Poolgröße, 4, max(1, hardware_concurrency−2), Dateizahl)** Lanes eingereiht. Eine atomare Dateiindexvergabe verteilt die Jobs auf diese Lanes. Kein Thread pro Datei, keine parallelen Sections derselben Datei, keine verschachtelten Worker-Jobs.

## PARALLEL DESIGN

1. Die vorhandene sortierte Area-Objektliste liefert ausschließlich `PROPERTY_TYPE_BUILDING` samt den schon bisher verwendeten, existierenden LOD-Pfaden. Baum-/Vegetations-, Terrain- und Effektpfade bleiben unverändert.
2. Bereits geladene oder fehlgeschlagene CResources werden nicht neu vorbereitet. `Preparation::Add` normalisiert den Pfad und vereinigt doppelte Anforderungen vor dem Read. Maximal 32 Dateiergebnisse pro Area; ab 16 MiB angesammelten Inputs werden keine weiteren aufgenommen (die letzte Datei kann diese Schwelle überschreiten). Übrige Dateien benutzen den bisherigen seriellen Pfad. Kein externes Manifest, persistenter Cache oder Contentwechsel.
3. Owner liest die Payloads. Worker führen ausschließlich `File(bytes)` und `Read(file)` aus. `Contents` enthält eigene CPU-Daten; kein halb fertiges Dokument wird veröffentlicht.
4. Alle Lanes werden synchron beendet. Danach läuft die ursprüngliche Instanzschleife in unveränderter Reihenfolge. `CResource::Load` nutzt die bereits gelesenen Bytes; der native Provider übernimmt genau einmal das vorbereitete Ergebnis und erzeugt erst jetzt das Document. Nachfolgende Requests verwenden die normale CResource. IDs, Materialslots, Bone-/Animationsindices und Handles hängen nicht von der Worker-Reihenfolge ab.
5. Beim Verlassen des Area-Aufbaus verschwinden die temporären Inputs/Ergebnisse. Kein Job überlebt den Scope, keine Hintergrundlast nach dem Ladefenster, kein Pop-in.

**Single flight:** mehrere Platzierungen bzw. normalisierte Pfadalias-Anforderungen teilen denselben Batch-Eintrag und damit genau einen Read/Decode. Der vorhandene ResourceManager bleibt für Veröffentlichung und spätere Anfragen zuständig. `Add` und Cachezugriffe sind Owner-Thread-APIs; dieser Fix erklärt nicht beliebige konkurrierende ResourceManager-Aufrufe von Fremdthreads für sicher. Worker verlangen keine Ressourcen nach. Im nativen Vorher-/Nachher-Vergleich sind alle Parsepfade eindeutig; je Datei ein Parse und insgesamt 903 native Reads, auch nach A1 → B1 → A1.

## BEFORE / AFTER / SAVING

Ausgewertet wird ein finales Vorher-/Nachher-Paar mit je einem neuen Prozess, identischen Originalpacks und Grafikeinstellungen, gefüllter Shadercache. Cold bedeutet Prozess-cold; OS-Dateicache nicht geleert. Warm ist die Rückkehr nach A1 → B1 → A1. Endpunkt unverändert: drei stabile World-Presents ohne neue Lade-/Upload-/Compilearbeit. Screenshotkosten liegen danach.

| Messung | Before ms | After ms | Ersparnis ms | Ersparnis % |
| --- | ---: | ---: | ---: | ---: |
| A1 Cold TOTAL | 2261.090 | 2219.191 | 41.900 | 1.853 |
| A1 Cold GR2 Wall | 1865.546 | 1814.050 | 51.496 | 2.760 |
| A1 Cold Decompression Wall | 1148.657 | 1139.957 | 8.700 | 0.757 |
| A1 Cold Animation parse, exklusiv | 392.693 | 404.897 | -12.204 | -3.108 |
| A1 Cold GR2 kumulierte CPU-Bereichsdauer | 1865.546 | 1910.918 | -45.373 | -2.432 |
| A1 Cold Decompression kumulierte Bereichsdauer | 1148.657 | 1188.474 | -39.816 | -3.466 |
| A1 Cold grobe Thread-CPU | 1781.250 | 1843.750 | -62.500 | -3.509 |
| A1 Warm TOTAL | 215.784 | 163.743 | 52.041 | 24.117 |
| A1 Warm GR2 | 61.167 | 10.500 | 50.667 | 82.834 |
| A1 Warm Dekompression / Parses / Jobs | 0 / 0 / 0 | 0 / 0 / 0 | — | — |
| B1 TOTAL | 386.914 | 271.245 | 115.669 | 29.895 |
| B1 GR2 Wall | 222.067 | 113.239 | 108.828 | 49.007 |
| B1 Decompression Wall | 93.864 | 54.134 | 39.730 | 42.327 |

**Keine Verwechslung von CPU und Wall:** Worker-Bereiche werden nicht in die exklusiven Main-Thread-`COST`-Summen eingefügt. Die getrennte `GR2COST`-Ansicht enthält kumulierte CPU-Bereichsdauern. GR2 Wall = diese Summe minus Worker-Bereichsdauer plus synchrones Batch-Wartefenster. Decompression Wall = serielle Decodezeit plus Vereinigung der zeitlich überlappenden Worker-Decodeintervalle. Bereichsdauern messen verstrichene Zeit innerhalb CPU-Code, nicht reine Scheduler-CPU. Die gesonderten Windows-Threadzeiten sind grob quantisiert und können bei vielen kurzen Jobs über- oder unterschätzen. Die Windows-Zeitabfrage wird als Callback vom Consumer übergeben; der GR2-Core bleibt ohne Plattform-Includes.

Das akzeptierte finale Messpaar zeigt einen moderaten Gewinn; keine statistische Benchmarkbehauptung. Ein früherer Zwischenbuild ergab 104.305 ms / 4.613 %, wird aber nicht als finales Resultat gewählt: Nach Abschlussreview wurden Diagnose-Allokationen hinter alle Joins verlegt, die CPU-Zeitabfrage aus dem GR2-Core in den Windows-Consumer verschoben und die Bibliotheksabhängigkeit explizit gemacht. Danach erneuter Release-Build und genau ein finaler Nachher-Lauf: maßgeblich sind die kleineren 41.900 ms / 1.853 %. **Warm-Ersparnis wird nicht dem Parallelfix zugerechnet:** beide Warm-Läufe decodieren nichts. Bestehende Clip-Binding-/Vorbereitungsarbeit schwankt 59.916 → 9.405 ms; A1 Cold 51.738 → 39.088 ms. Auch deshalb nicht jede eingesparte Millisekunde als isolierten Parallelisierungseffekt ausgeben. Die After-Batches selbst benötigen für A1 64.882 ms statt 161.954 ms aufsummierter Jobdauer; die kumulierte Dekompressionsdauer liegt gleichzeitig 39.816 ms / 3.466 % höher als in der Baseline. Diese Schwankung reduziert den verbleibenden Gesamtgewinn.

## CONCURRENCY / CPU-NUTZUNG

| Messwert | A1 Cold | B1 | A1 Warm |
| --- | ---: | ---: | ---: |
| GR2-Dateijobs submitted / completed | 98 / 98 | 67 / 67 | 0 / 0 |
| Maximal verwendete Pool-Worker / Peak Jobs | 4 / 4 | 4 / 4 | 0 / 0 |
| Durchschnitt während Batch-Wartefenstern | 2.496 | 2.552 | 0 |
| Worker busy duration ms | 161.954 | 202.670 | 0 |
| Batch Wall ms | 64.882 | 79.414 | 0 |
| Grobe Worker Thread-CPU ms | 140.625 | 140.625 | 0 |
| Single-flight joins, doppelte Batch-Anforderungen | 76 | 64 | 0 |
| Pool-Lanes / Completion-Waits | 34 | 30 | 0 |

Die 34/30 Lanes verteilen sich auf mehrere nacheinander abgearbeitete Area-Batches; niemals gleichzeitig 34/30 Worker. Average ist die Summe aktiver Jobdauern geteilt durch die Summe der Batch-Wall-Fenster. Auf 16 logischen CPUs entspricht die aktive Jobbelegung grob 15.6 % / 16.0 % der Maschinenkapazität; das ist kein gemessener Task-Manager-Prozentwert. Die groben Threadzeiten entsprächen etwa 2.17 / 1.77 CPU-Kernen, sind wegen kurzer quantisierter Messintervalle insbesondere bei B1 ungenau. Keine künstliche Dauerlast, kein beobachteter Hänger; Hauptthread wartet wie zuvor innerhalb des synchronen Ladevorgangs. Keine separate interaktive OS-Reaktionszeitmessung behauptet.

## CORRECTNESS

**PASS, frisch aus dem gemessenen A1/B1-Corpus:**

- 903 Dateien, 7224 Sections, **60.042.028 expandierte Bytes byte-identisch** zwischen serieller Referenz und nebenläufigen File-Decodes. Alle lokalen Inputs erneut gegen Größe und gültige GR2-Payload-CRC der produktiven Packloads geprüft.
- Der produktive `Preparation`-Pfad liefert bitidentisch serialisierte relevante IR-Felder: **217 Modelle, 281 Meshes, 282 Materialien, 217 Skeletons, 729 Bones, 693 Animationen**. Vergleich umfasst Namen/IDs, originale Indices, Materialbindings/-gruppen, Skin-Bindings, Vertex-/Indexdaten, Transformations-/Bind-Matrizen, RuntimeSkeleton BindingId/EvaluationOrder sowie vollständige Kurven/Tracks/Events. Kein Pointer-/Paddingvergleich und keine bloße Count-Prüfung.
- **903 halbierte GR2-Dateien** im selben parallelen Batch wie gültige Dateien sauber abgelehnt; gültige Nachbarjobs bleiben korrekt. Synthetische malformed/truncated Eingaben propagieren bis zum Provider als `InvalidAsset` mit Diagnose. Keine halb fertigen Documents oder Ressourcenreste.
- Zusätzlicher Vergleich gegen den Decoder aus Commit `73dc92e`: 903 Dateien / 7224 Sections / 60.042.028 Bytes gleich; **2226 halbierte Section-Payloads** sowie **2226 mit passend verkürzter deklarierter Größe** abgelehnt. P0-L5-SSE2-Source SHA256-identisch erhalten.
- Vier kurze wiederholte Batch-Läufe mit je 24 gültigen Dateien, Pfadalias-/Duplikatanforderungen und Fehlerdateien; zusätzlich injizierter Executor-Submissionfehler mit Join bereits gestarteter Arbeit. Kein beobachtetes Race-Symptom, Future-Hängen oder Deadlock. Kein ThreadSanitizer im Windows-Gate.
- Native Traces vor/nach: gleiche Request-/Cache-/Parsepfade, CRCs, Sectionbytes, Mesh-/Skeleton-/Animationscounts, Vertex-/Indexkopien und Shaderbytecodeidentitäten. Keine ausgelassenen Assets oder zusätzlichen Warm-Reloads.

## FAST GATE — PASS

- Finaler Release-x64-Client gebaut, Exit 0. Vorhandene Vendor-/Linkerwarnungen; kein warning-free Build behauptet. Sandbox verweigerte zunächst SDK-Zugriff; der Build lief anschließend in der vorhandenen autorisierten VS-x64-Umgebung erfolgreich.
- Gezielte Tests **6/6 PASS**: GR2Preparation, GR2Independence, GR2Safety, GR2Compatibility, GR2Golden.static, GR2Golden.animation. Corpus- und Fehlerprüfungen zusätzlich PASS. Keine volle Suite, kein Debug/GCC/LP64, kein langer Benchmark.
- Neuer Before-Prozess PID **36716**, After PID **26700**, jeweils **Exit 0**, A1 Cold → B1 → A1 Warm. Vorherstart unter Sandbox scheiterte mit `0xc0000142`; akzeptierter Start lief unverändert außerhalb dieser Einschränkung. Kein Manifest-/Admin-Umbau.
- **Diligent ERROR/FATAL = 0, GPU fallback = 0, CPU deformation = 0, alle 30 geprüften Shutdown-/Fehlerzähler = 0.** NativeGR2Reads=903, GrannyFileReads=0. Startup bestätigt native GR2/AnimationRuntime und GPU-Skinning über D3D11.
- **Runtime Shader Compile = 0, Misses = 0** in sämtlichen Messfenstern; A1 77/77 Cachetreffer, Setup 48/48. Alle 73 Shadercache-Dateien mit identischen Hashes. Keine Änderung an Shadercode, Assets, GR2-Format, Terrain, Vegetation oder Grafik-/Animationsqualität.
- Kurze Sichtprüfung des A1-Before/After-Paars: Player, NPC, Mob und Umgebung vorhanden, keine sichtbare Änderung außer Animationszeitstand. Keine Visual-Galerie, kein Netzwerklogin, keine vollständige manuelle Originalclient-/UI-Abnahme behauptet.

EXE SHA256 Before: `7FB3CD9DF25B40A0C09FB14E9F16E228FD5F494ABFEB8301DD822163CDE3410D`.
After: `EAC38F283DA10A5567032DB7BB72AD41A800500DDF3C40E70FBA77AAE04B3A9C` (entspricht finalem Release-Build).

## REPRODUKTION / BELEGE

Lokale, nicht zu commitende Belege: `build-p0l/l6-before`, `build-p0l/l6-after-final` (Native-Logs, Trace, Fast-Gate und `gr2-parallel-comparison.json`), `build-p0l6` (Build-/Testlogs, frisch verifizierte Corpusliste, Byte-/IR-Vergleiche und Erhaltungs-Hashes). Source/Test/Dokumentation sind die einzigen Commit-Inhalte.

```powershell
# Vorhandene VS-x64-Umgebung, gefuellter Cache, jeweils frischer Probe-Name:
cmake --build build-h2x/msvc --config Release --target UserInterface GR2PreparationTest GR2SafetyTest GR2CompatibilityTest GR2GoldenTest --parallel 6
ctest --test-dir build-h2x/msvc -C Release -R '^AssetRuntime.GR2(Safety|Compatibility|Golden\.(static|animation)|Preparation|Independence)$' --output-on-failure
build-h2x/msvc/tests/AssetRuntime/Release/GR2PreparationTest.exe build-p0l6/decoder-files.txt
tests/Loading/run_gr2_decoder_parity.ps1 -CorpusList build-p0l6/decoder-files.txt -OutputDirectory build-p0l6/fresh-parity -Baseline 73dc92e
tests/Loading/run_shader_cache_probe.ps1 -Name fresh-after -CacheDirectory C:/absolute/filled-cache -ShaderLifecycle
python tests/Loading/verify_probe.py build-p0l/fresh-after --shader-lifecycle
python tests/Loading/verify_gr2_parallel.py build-p0l/l6-before build-p0l/fresh-after
```

## NEXT BOTTLENECK / STOP

Größter verbleibender Block ist der weitgehend serielle Animationsdatei-Pfad: **1139.957 ms Dekompressions-Wall insgesamt**, danach **404.897 ms exklusives Animationskurvenparsing**. Kumulierte Decodierschleife 1088.925 ms. Nur dokumentiert; kein Animationsparser-Fix, kein persistenter GR2-Cache, kein P1 und keine zweite Optimierung.

Commit bei diesem GO: `perf(gr2): parallelize area asset preparation`. **Kein Push. Danach STOP.**
