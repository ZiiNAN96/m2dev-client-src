# P0-L5 – Native GR2 Pipeline Deep Dive + One Fix

## P0-L5 SUMMARY

Stand 17.09.2026. **Technisches GO im angeforderten kurzen Offline-Gate. Genau ein funktionaler Fix: SIMD-Symbolsuche in kurzen Oodle1-CDF-Intervallen. Kein Push. STOP nach Commit.**

Source: `m2dev-client-src`, Branch `codex/g56-hdr-atmosphere`, Basis `5563d78` (davor `611cd50`, `2b5b4cf`). Runtime/assets: getrenntes `m2dev-client`, Branch `codex/g56-hdr-colors`, HEAD `548c43b4`. Der vorbestehende untracked P0-L3-Bericht sowie beide Runtime-Deploymentbelege sind SHA256-identisch erhalten. Keine Assets geändert, gelöscht oder konvertiert. Originalclient-EXE unverändert; kein Deployment behauptet.

**Root cause:** serielle native Oodle1-Decodierung. A1 enthält neu gemessen 831 tatsächliche GR2-Parses und 6648 Sections. Von 1 200.819 ms Dekompression liegen **1 108.359 ms in der Decodierschleife**. Der reale A1/B1-Corpus benötigt 68 238 776 adaptive Symbolabfragen. Die zuvor verwendete `upper_bound`-Suche verzweigt pro Vergleich anhand des Symbols; fast alle eingeengten CDF-Intervalle enthalten höchstens 16 Werte. Der einzige Fix vergleicht dort bis zu vier Werte gleichzeitig, ohne die Decodier- oder Validierungssemantik zu ändern.

## A1 COLD BEFORE / A1 WARM BEFORE / AFTER / SAVING

| Messung | Before ms | After ms | ms saved | saved % |
| --- | --- | --- | --- | --- |
| A1 Cold TOTAL | 2 380.281 | 2 343.773 | 36.508 | 1.534 % |
| A1 Cold GR2 vollständig | 1 984.520 | 1 937.214 | 47.306 | 2.384 % |
| A1 GR2 Provider (Teilmenge) | 1 910.506 | 1 791.460 | 119.045 | 6.231 % |
| A1 GR2 Decompression | 1 200.819 | 1 155.559 | 45.260 | 3.769 % |
| A1 Warm TOTAL | 166.246 | 165.109 | 1.137 | 0.684 % |
| A1 Warm GR2 | 1.225 | 1.240 | -0.015 | -1.208 % |
| A1 Warm Decompression | 0.000 | 0.000 | 0.000 | — |
| B1 TOTAL | 368.451 | 386.488 | -18.038 | -4.896 % |
| B1 Decompression | 96.335 | 93.131 | 3.204 | 3.326 % |

Das Messpaar zeigt **36.508 ms / 1.534 % kürzeren A1-Cold-Load** und **45.260 ms / 3.769 % weniger Dekompression**. Dies ist ein kleiner Effekt in genau einem akzeptierten Vorher-/Nachher-Paar, keine statistisch abgesicherte Beschleunigung jedes Starts. Die isolierte finale Decoderprüfung bestätigt dieselbe Richtung: 1338.33 → 1295.61 ms, 42.72 ms weniger über 903 Dateien; auch dies keine Benchmarkserie.

Andere Anteile schwanken: A1-Vertexverarbeitung 141.965 → 78.483 ms, AnimationRuntime-Vorbereitung 37.616 → 109.272 ms (3 → 5 Bind-Aufrufe). Deshalb wird der größere reine Provider-Unterschied von 119.045 ms **nicht** vollständig dem Fix zugeschrieben. B1 total wird 18.038 ms langsamer, obwohl seine Dekompression 3.204 ms sinkt; dabei kommen 12.201 ms Clip-Vorbereitung hinzu. Kein Warm-Speedup durch Dekompression behauptet: Warm hat vorher/nachher 0 Parses und 0 Dekompression. Keine zweite Optimierung daraus abgeleitet.

Messfenster: neuer Prozess mit vorhandenem Shadercache; Warm nach A1 → B1 → A1 derselben Session. Originalpacks, identische Grafikoptionen (Modern 4, Shadows 4, AO 2, Water 3, Vegetation 2, Textures 1, ViewDistance 25600, HDR/Bloom/Sky an, Fog aus). OS-Dateicache nicht geleert. Ende: drei World-Presents ≤50 ms ohne neue erfasste Lade-/Upload-/Compilearbeit. Screenshotkosten liegen danach. CPU-Wandzeiten, keine GPU-Ausführungszeiten.

Akzeptierte Prozesse: Before PID 42788, After PID 19948, beide Exit 0. Vorher-Binary SHA256 `48EBA7AB50E8ECBAC14DC5ABB9B06B87DF4BC6D918C7C36EE95B940B4BB15733`; Nachher `7FB3CD9DF25B40A0C09FB14E9F16E228FD5F494ABFEB8301DD822163CDE3410D`. Zwischen diesen Builds ändert sich an der gemessenen Decoderimplementierung genau der Aufruf von `std::upper_bound` zu `SymbolUpperBound`; dieselbe Diagnose ist in beiden aktiv.

Shadercache: **A1 77/77 Hits, 0 Misses, 0 Compiles, 0 ms Compilezeit**, Clientsetup zusätzlich 48/48 Hits. Alle 73 Cachedateien einschließlich Hashes unverändert. Shadercode, Shadercache, PSOs, DiligentFX und sämtliche Qualitätsparameter unverändert.

## GRANNY-ZERO-PROOF

`.gr2 → GetGR2AssetProvider → ZiiNAN GR2::File/Read → AssetDocument/RuntimeSkeleton/RuntimeAnimationClip → bestehendes GPU-Skinning → Diligent D3D11`.

`NoGrannyDependencyTest.cmake`: PASS, produktive SDK-Includes/-Typen/-API-Calls/-Bibliotheken/-Targets 0. Der finale EXE-Importaudit enthält keine Granny-DLL/-API-Imports. `Providers.cpp` dispatcht `.gr2` direkt zum nativen Reader, ohne SDK-Fallback. Startup bestätigt `GR2Reader=ziinan`, `AnimationRuntime=ziinan`, `Skinning=gpu`, `Selection=default API=d3d11`. Shutdown: NativeGR2Reads=903, GrannyFileReads=0. Die alten `CGrannyModel`-/`CGrannyMesh`-/`CGrannyMotion`-Namen in `EterGrnLib` sind native Consumeradapter, keine Granny Runtime. Tests, alte Dokumente und Formatbezeichnungen sind keine produktiven SDK-Pfade.

## BREAKDOWN – A1 Cold before, vollständige GR2-Zuordnung

GR2 TOTAL **1 984.520 ms** umfasst Cache-Lookups, GR2-Packzugriffe, Provider, Adapter, AnimationRuntime-Vorbereitung und zugeordnete spätere GPU-Aufrufe vor dem Messendpunkt. Davon nativer Provider **1 910.506 ms**, GR2 DECOMPRESSION **1 200.819 ms**. Diese inklusiven Teilmengen nicht zum Total addieren.

Die folgenden **exklusiven** Stufen summieren sich exakt zum GR2 Total. `calls` zählt Instrumentierungsbereiche; bei zusammengesetzten Zeilen die Summe der genannten Teilbereiche, nicht erneut die Zahl der Dateien. Messverwaltung und zwei Zeitabfragen pro Referenz sind enthalten. `GR2COST` ist eine alternative Zuordnung der normalen `COST`-Daten und wird nie nochmals zu Map TOTAL addiert. Verschachtelte Texturressourcen suspendieren die GR2-Zuordnung.

| Stufe | total ms exklusiv | calls | % GR2 total | Bytes / Einordnung |
| --- | --- | --- | --- | --- |
| A Filesystem open | 0.000 | 0 | 0.000 % | 0 erfolgreiche GR2-Einzeldatei-Opens |
| B Pack/VFS-Zugriff | 22.644 | 1 662 | 1.141 % | 34 134 958 GR2-Dateibytes |
| C Header/Tabellen + CRC | 53.946 | 831 | 2.718 % | 34 134 958 |
| D Dekompression ohne separat erfasste Output-Allokation | 1 197.202 | 12 186 | 60.327 % | 52 261 004 |
| E Relocations/Fixups | 44.518 | 831 | 2.243 % | — |
| F Interne Referenzauflösung | 25.645 | 389 905 | 1.292 % | — |
| G Mesh-Metadaten | 5.202 | 178 | 0.262 % | 178 Meshes |
| H Material-Parsing | 3.187 | 178 | 0.161 % | — |
| I Skeleton-Parsing | 1.116 | 145 | 0.056 % | 626 Bones |
| J Animation-Parsing/Kurven | 404.845 | 693 | 20.400 % | 693 Animationen |
| K Vertex/Index-Verarbeitung | 149.632 | 356 | 7.540 % | inkl. vorhandener Normalen |
| L Separate Tangent-/Normalenerzeugung | 0.000 | 0 | 0.000 % | nicht vorhanden im nativen GR2-Reader |
| M AssetRuntime-Objektgraph/Document | 8.791 | 1 662 | 0.443 % | Besitzübergabe per move |
| N AnimationRuntime-Vorbereitung | 37.940 | 148 | 1.912 % | Skeleton-Initialisierung + Clip-Bindings |
| O Erfasste Output-Allokation und Upload-Kopien | 5.047 | 2 932 | 0.254 % | 16 192 354 kopierte Uploadbytes |
| P GPU-Buffer-Vorbereitung CPU | 2.045 | 709 | 0.103 % | — |
| Q GPU-Erzeugung/Upload CPU-API | 0.779 | 39 | 0.039 % | 19 Buffer-Erzeugungen |
| R Resource-/Cache-Lookup | 3.936 | 3 224 | 0.198 % | 3224 Lookup-Aufrufe |
| S Other/Lifetime/Adapter/Diagnose | 18.045 | 10 150 | 0.909 % | Rest vollständig ausgewiesen |

Zu A/B: GR2s werden aus bereits gemappten Packs gelesen. **0 neue GR2-Einzeldatei-Opens und 0 explizite Einzeldatei-Read-Aufrufe**; die B-Zeit misst VFS-Lookup, Pack-ZSTD/Decrypt/Kopien für 831 vollständige Payloads. Physische Disk-I/O bzw. Pagefault-Zeit lässt sich aus diesen API-Zählern nicht separat bestimmen und wird nicht mit 0 ms erfunden. Pack-Mapping liegt vor dem A1-Fenster. Keine Seek-Schleife pro GR2, kein wiederholtes Öffnen derselben GR2. Nur 22.644 ms: kein I/O-Fix und kein behaupteter Antivirus-Befund.

Zu L: vorhandene Normalen werden bei K gelesen und unverändert kopiert; der native GR2-Pfad generiert hier keine zusätzlichen Tangenten/Normalen. Zu O: nur explizite Output-Allokation und Vertex-/Index-Uploadkopien sind separat getimt. Weitere kleine Containerallokationen gehören zur jeweiligen Parser-/Decoderstufe; dafür folgt unten ein eigener Allocation-Audit. Keine erfundene unabhängige Gesamtdauer aller `malloc`-/`memcpy`-Aufrufe. Zu Q: CPU-Zeit für Resource-Erzeugung/Upload, inklusive 19 `CreateBuffer`-Aufrufen; GPU-Ausführungszeit und interne Treiberkopien sind nicht isoliert gemessen.

Decoder-Unterteilung (inklusiver Block 1 200.819 ms): Modellaufbau **61.310 ms / 2769 Blocks**, Schleife **1 108.359 ms / 2769**, Output-Allokation **3.616 ms**, übrige Decodierung/Modellabbau **27.532 ms**. Output-Allokation steht in der Haupttabelle ausschließlich bei O.

## GR2 requests / unique / loads / parses / cache hits

| Fenster | Requests | unique request paths | Resource hits | Resource misses | Pack payload loads | actual parses | loose FS loads |
| --- | --- | --- | --- | --- | --- | --- | --- |
| A1 Cold | 3224 | 1040 | 2184 | 1040 | 831 | 831 | 0 |
| A1 Warm | 625 | 134 | 625 | 0 | 0 | 0 | 0 |

Alle 831 A1-Parsepfade sind eindeutig, je genau ein Parse; A1+B1 zusammen 903. Ein Resource-Miss erzeugt zunächst einen Ressourcenhalter, nicht zwingend einen Payload-Load: 1040 erstmals angefragte Pfade, davon 831 tatsächlich geladene/geparste Dateien. 209 Halter bleiben in diesem Fenster ungeladen. Eine Cache-Anfrage ist nicht mit `CResource::Load` gleichzusetzen. Vor A1 findet die Item-/Clientregistrierung separat statt; deren 8420 GR2-Lookups werden nicht als A1-Parses gezählt.

Warm: bestehende geparste Dokumente bleiben in Sessionbesitz, **keine erneute Section-Dekompression**. Clip-Bindings besitzen einen eigenen Document-Cache und können bei neuem Skeleton/Boundary neu vorbereitet werden; das ist kein erneutes Lesen oder Dekomprimieren der GR2. Kein persistenter GR2-Cache implementiert.

## BY CATEGORY – A1 Cold before

| Kategorie | geparste Dateien | GR2 ms inkl. zugeordneter Requests | % GR2 |
| --- | --- | --- | --- |
| Gebaeude | 31 | 200.140 | 10.085 % |
| Props | 98 | 91.475 | 4.609 % |
| Vegetation | 0 | 0.000 | 0.000 % |
| NPC | 1 | 2.750 | 0.139 % |
| Mob | 2 | 3.879 | 0.195 % |
| Player | 5 | 19.562 | 0.986 % |
| Weapon | 1 | 1.226 | 0.062 % |
| Armor | 0 | 0.000 | 0.000 % |
| Mount | 0 | 0.000 | 0.000 % |
| Animation | 693 | 1 665.488 | 83.924 % |
| Effect attachment | 0 | 0.000 | 0.000 % |
| Sonstige | 0 | 0.000 | 0.000 % |

Grobe, reproduzierbare Inhaltszuordnung: Dateien mit Animationen zuerst „Animation“, übrige nach virtuellem Pfad. Körper/Armor/Haar der sichtbaren Player sind unter „Player“ enthalten; 0 bei Armor bedeutet keine separat klassifizierte Armor-Datei, nicht fehlende Rüstung. Vegetation im aktuellen A1 benutzt ZVEG/GLB und gehört nicht zu dieser GR2-Tabelle. Nicht geladene Kategorien erhalten keinen geschätzten Kostenanteil.

## TOP 30 ASSETS – sortiert nach gesamter GR2-Zuordnung

Größe = komprimierte **GR2-Datei inklusive Tabellen/Fixups**, nicht äußere Pack-ZSTD-Größe. Expanded = Summe der Section-Ausgaben. Parse = Objektgraph einschließlich Mesh/Material/Skeleton/Animation, ohne separat ausgewiesene RuntimeSkeleton-Konvertierung. Conversion = RuntimeSkeleton + Document-Besitzübergabe. GPU prep = CPU-Aufbereitung; Total enthält zusätzlich I/O, Header, Fixups, Cache, spätere Clip-/GPU-Arbeit und Rest.

| # | path | file B | expanded B | sections | decompress ms | parse ms | conversion ms | GPU prep ms | total ms |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | `d:/ymir work/zone/a/building/a1-009-hotel.gr2` | 194987 | 689672 | 8 | 8.925 | 74.747 | 0.004 | 0.200 | 84.427 |
| 2 | `d:/ymir work/pc/warrior/action/dance_6.gr2` | 792913 | 1252276 | 8 | 27.721 | 9.584 | 0.002 | 0.000 | 39.010 |
| 3 | `d:/ymir work/pc/warrior/action/dance_1.gr2` | 647456 | 1103640 | 8 | 23.215 | 7.785 | 0.001 | 0.000 | 32.382 |
| 4 | `d:/ymir work/pc/warrior/action/dance_3.gr2` | 561164 | 868384 | 8 | 19.579 | 6.192 | 0.001 | 0.000 | 26.962 |
| 5 | `d:/ymir work/pc/warrior/action/dance_4.gr2` | 533370 | 851624 | 8 | 18.862 | 6.196 | 0.001 | 0.000 | 26.129 |
| 6 | `d:/ymir work/monster/stray_dog/00.gr2` | 35776 | 50408 | 8 | 1.273 | 0.405 | 0.001 | 0.000 | 20.792 |
| 7 | `d:/ymir work/pc/warrior/action/dance_5.gr2` | 336913 | 518708 | 8 | 12.848 | 3.752 | 0.001 | 0.000 | 17.314 |
| 8 | `d:/ymir work/zone/a/building/a1-010-bank.gr2` | 114877 | 468592 | 8 | 5.421 | 6.586 | 0.002 | 0.243 | 12.610 |
| 9 | `d:/ymir work/npc/goods/wait.gr2` | 51192 | 76944 | 8 | 1.888 | 0.608 | 0.001 | 0.000 | 12.439 |
| 10 | `d:/ymir work/pc/warrior/onehand_sword/wait_1.gr2` | 29974 | 41604 | 8 | 1.040 | 0.408 | 0.001 | 0.000 | 10.520 |
| 11 | `d:/ymir work/zone/a/building/a1-033-guesthouse-01.gr2` | 91523 | 392088 | 8 | 4.301 | 5.429 | 0.002 | 0.159 | 10.207 |
| 12 | `d:/ymir work/pc/warrior/action/banter.gr2` | 176426 | 271368 | 8 | 6.309 | 2.227 | 0.001 | 0.000 | 8.982 |
| 13 | `d:/ymir work/pc/warrior/action/forgive.gr2` | 169160 | 268108 | 8 | 5.969 | 1.949 | 0.001 | 0.000 | 8.329 |
| 14 | `d:/ymir work/zone/a/building/a1-009-hotel_lod_01.gr2` | 74452 | 288776 | 8 | 3.427 | 3.805 | 0.005 | 0.059 | 7.571 |
| 15 | `d:/ymir work/pc2/sura/general/dead.gr2` | 136637 | 215948 | 8 | 5.078 | 1.775 | 0.001 | 0.000 | 7.496 |
| 16 | `d:/ymir work/pc/warrior/action/sad.gr2` | 143867 | 226216 | 8 | 5.096 | 1.717 | 0.001 | 0.000 | 7.207 |
| 17 | `d:/ymir work/pc2/shaman/fishing/fishing_catch.gr2` | 130651 | 193508 | 8 | 4.803 | 1.669 | 0.001 | 0.000 | 6.870 |
| 18 | `d:/ymir work/zone/a/building/a1-034-stonedoor.gr2` | 67621 | 230392 | 8 | 3.075 | 3.174 | 0.002 | 0.086 | 6.577 |
| 19 | `d:/ymir work/pc/sura/general/dead.gr2` | 128037 | 195328 | 8 | 4.600 | 1.638 | 0.001 | 0.000 | 6.567 |
| 20 | `d:/ymir work/pc/sura/fishing/fishing_fail.gr2` | 124993 | 185280 | 8 | 4.464 | 1.561 | 0.001 | 0.000 | 6.383 |
| 21 | `d:/ymir work/pc/warrior/warrior_4-1.gr2` | 86598 | 203252 | 8 | 2.874 | 2.316 | 0.022 | 0.112 | 6.180 |
| 22 | `d:/ymir work/pc2/sura/fishing/fishing_catch.gr2` | 110356 | 165788 | 8 | 3.885 | 1.715 | 0.001 | 0.000 | 5.906 |
| 23 | `d:/ymir work/zone/a/building/a1-024-10m-bridge.gr2` | 39794 | 216360 | 8 | 1.941 | 3.547 | 0.003 | 0.106 | 5.802 |
| 24 | `d:/ymir work/zone/a/building/a1-011-workhouse.gr2` | 60034 | 198244 | 8 | 2.698 | 2.679 | 0.002 | 0.126 | 5.727 |
| 25 | `d:/ymir work/zone/a/building/a1-008-house5.gr2` | 45111 | 165616 | 8 | 2.498 | 2.248 | 0.002 | 0.089 | 5.026 |
| 26 | `d:/ymir work/pc/warrior/action/french_kiss_with_shaman.gr2` | 97938 | 142016 | 8 | 3.492 | 1.174 | 0.001 | 0.000 | 4.974 |
| 27 | `d:/ymir work/pc/warrior/action/shy.gr2` | 93880 | 143672 | 8 | 3.444 | 1.214 | 0.001 | 0.000 | 4.940 |
| 28 | `d:/ymir work/pc/warrior/action/congratulation.gr2` | 98869 | 146448 | 8 | 3.452 | 1.144 | 0.001 | 0.000 | 4.913 |
| 29 | `d:/ymir work/zone/a/building/a1-middledam-01.gr2` | 51277 | 164852 | 8 | 2.379 | 2.236 | 0.002 | 0.076 | 4.891 |
| 30 | `d:/ymir work/pc/warrior/action/cheerup.gr2` | 99078 | 147216 | 8 | 3.440 | 1.154 | 0.001 | 0.000 | 4.875 |

Top 30 tragen **20.761 %** des GR2 Total; das größte Asset **4.254 %**. Einige große Tanzclips und das Hotel fallen auf, erklären aber nicht allein die Last. Überwiegend **breit verteilte Decodierarbeit vieler Dateien**, besonders 693 Animationen; kein Beleg für einen einzelnen dominierenden Großasset. Der Hotel-Vertexblock schwankt im Messpaar deutlich und wird nicht zur Decoderersparnis gerechnet.

## DECOMPRESSION

- Compressed section bytes A1: **27 919 294**; gesamte GR2-Dateibytes: **34 134 958**.
- Decompressed section bytes: **52 261 004**.
- Sections: **6648**, davon **1938 nicht leer**, **8 pro Datei**.
- Total decompression: **1 200.819 ms**; effective throughput **43.521 MB/s** (dezimal, Outputbytes/Wandzeit).
- Median Section-Outputgröße inklusive leerer Sections: **0 B**; nur nichtleere: **6910 B**.
- Nachher: gleiche Byte-/Sectionzahlen, **45.226 MB/s**.

Größte Sections, frisch aus dem A1-Trace:

| Asset | section | compressed B | expanded B | section ms inkl. Diagnose |
| --- | --- | --- | --- | --- |
| `d:/ymir work/pc/warrior/action/dance_6.gr2` | 0 | 784777 | 1246836 | 27.650 |
| `d:/ymir work/pc/warrior/action/dance_1.gr2` | 0 | 639518 | 1098200 | 23.144 |
| `d:/ymir work/pc/warrior/action/dance_3.gr2` | 0 | 553248 | 862944 | 19.508 |
| `d:/ymir work/pc/warrior/action/dance_4.gr2` | 0 | 525430 | 846184 | 18.789 |
| `d:/ymir work/zone/a/building/a1-009-hotel.gr2` | 1 | 147807 | 554432 | 6.783 |
| `d:/ymir work/pc/warrior/action/dance_5.gr2` | 0 | 328925 | 513268 | 12.776 |
| `d:/ymir work/zone/a/building/a1-010-bank.gr2` | 1 | 79997 | 378656 | 3.816 |
| `d:/ymir work/zone/a/building/a1-033-guesthouse-01.gr2` | 1 | 63980 | 317760 | 3.042 |
| `d:/ymir work/pc/warrior/action/banter.gr2` | 0 | 168392 | 265928 | 6.196 |
| `d:/ymir work/pc/warrior/action/forgive.gr2` | 0 | 161126 | 262668 | 5.901 |

Die Decoder-Corpusdiagnose über die aktuell gemessenen A1/B1-Pfade zählt **68 238 776** Symbolabfragen. **11 749 525** Suchintervalle sind leer (17.22 %); mehr als 99.96 % haben höchstens 16 Einträge. Leere Intervalle müssen weiterhin den korrekten `upper_bound`-Zeiger liefern, ohne zu dereferenzieren. Der vorhandene 65-Bucket-Index wird unverändert beim CDF-Rebuild aktualisiert.

Geprüft: adaptive Symbol-/Tabellensuche, Rebuild/Rescale, Arithmetik-Renormalisierung, verzweigte Schleifen, Bounds Checks, Byte-Lookahead und LZ-Backreferences. Die überlappende Byte-für-Byte-LZ-Kopie besitzt echte Abhängigkeiten und bleibt unverändert. Auch Eingabegrenzen, terminales Lookahead, Kapazitäten, Output-/Backreference-Grenzen, Header/CRC und Referenzprüfungen bleiben erhalten. Kein Sicherheitscheck entfernt. Die kleinen diagnostischen Zeitstichproben im Decoder sind durch Timerkosten beeinflusst und begründen keine erfundene nanosekundengenaue Hotspotverteilung; Beleg sind Schleifen-Wandzeit, Suchanzahlen/-breiten und der bytegleiche A/B-Vergleich.

## SERIAL/PARALLEL und SECTION-PARALLELISMUS

Der gemessene Produktionspfad lädt und dekomprimiert **seriell auf einem Trace-/Ladethread**, Before Thread **19016**, 831 Dateipayloads, 1 910.506 ms Provider-Wandzeit. `GetThreadTimes` über Resource-Loads ergibt grob **2 046.875 ms** Thread-CPU (After 1 796.875 ms). Windows quantisiert diese kurzen Zeitfenster; die aufsummierte Näherung kann die separat zugeordnete Wandzeit übersteigen und ist keine präzise CPU-Auslastungsmessung. Decoder-/Parserarbeit ist CPU-dominiert, nicht durch neue GR2-Dateiöffnungen blockiert.

Container, Decoder, Section-Ausgaben und `Types` sind dateilokal. 831 Dateien sind auf Container-/Dekompressionsstufe unabhängig. Innerhalb einer Datei sind die Sections unabhängig dekomprimierbar: jede startet ihren eigenen `Decoder`. **Die bis zu drei Blocks innerhalb einer Section sind nicht unabhängig**; sie teilen Arithmetic-State und Outputposition. Fixups/Typgraph brauchen danach fertige Sections.

Theoretisch parallelisierbar: besitzgesicherte Payloadreads, Section-Dekompression, lokale Container-/Mesh-/Kurvenanalyse. Datei-Level wäre vorzuziehen, um Tasks zu bündeln. Thread-Safety des kompletten Produktionsladepfads ist **nicht bewiesen**: ResourceManager-Maps/statische Pfadbuffer, optionale Audit-Sammelzustände, mutable Document-Clipcaches, Lebensdauer, GPU-/Materialconsumer und Fehlerrückgabe benötigen erst ein explizites Konzept. Daher keine Worker, keine Thread-Flut, kein neuer Single-Flight-Mechanismus; der bestehende serielle Cache bleibt unverändert. Parallel-CPU-Peakmessung ist nicht anwendbar.

## COPY/ALLOCATION FINDINGS

Reale Besitzkette: gemapptes Pack → ggf. eigener verschlüsselter Packblock/Decrypt → ein vollständiger GR2-Payloadbuffer → geliehene Section-Spans → eigene expandierte Section-Vektoren → validierte Typ-/Referenzansichten und normalisierte Mesh-/Kurvendaten → per move an Document → bestehende CPU-Vertex-/Index-Snapshots → Diligent-Resource/Upload.

- Keine Kopie des gesamten expandierten GR2 zwischen `File`, `Read` und Document gefunden. Sections werden in den Besitzer verschoben; `Types` hält Referenzen. Normalisierte Vertex-/Kurvenstrukturen sind echte Datenkonvertierung, keine zweite identische Vollkopie.
- Native Decoderdiagnose, 903 aktuelle A1/B1-Dateien: **4 375 481 Heap-Allokationsaufrufe, 547 767 984 kumulativ angeforderte Bytes** (kein Peak-RAM). Enthält adaptive Vektoren/Reallokationen; nur Decoderfenster, kein globaler Allocatorumbau. Modellaufbau des A1 liegt separat bei 61.310 ms, daher nicht der größte belegte Block.
- Pro nichtleerer komprimierter Section ein Outputvektor; Decoder erzeugt je Block zwei Offsetmodelle, `highMaximum` mittlere Modelle, vier Literal- und 65 Längenmodelle. Jedes `Window` startet drei kleine Vektoren; deren Wachstum/Rescale erklärt viele kleine Allokationen. Reallokationen separat nicht exakt gezählt; keine erfundene Gesamtzahl.
- Mesh-Parsing materialisiert u.a. `Types::Array`-Ansichten und kleine `Reals`-Vektoren. Vertexdaten werden in `GR2::Vertex` normalisiert, Indizes in uint32. Diese Kosten liegen bei K/J/G; kein zusätzlicher Allocatorfix.
- Uploadkopien A1: **461 CopyVertices / 14 598 424 B**, **533 CopyIndices / 1 593 930 B**, zusammen **1.431 ms**. Repräsentative Wiederholungen sind der normale CPU-Buffer, statischer/Actor-Snapshot und Skinning-Sidecar. Skinning konvertiert zusätzlich uint32-Indizes in den bestehenden uint16-Sidecar; GPU-interne Kopien sind nicht direkt gezählt.
- Repräsentativ geprüft: Hotel/Props (starre Meshes und StaticObject-Snapshot), Warrior-Körper/Haar/Waffe (Skinning-/Actor-Sidecar) und Tanzclip (Kurven, keine Vertex-/GPU-Geometriekopie). Es gibt damit begrenztes Copy-/Allocation-Potenzial, aber keinen gemessenen mehrfachen Vollcopy-Block von annähernd 1.2 s.

## FIRST PLAYABLE FRAME – nur Analyse

- **CRITICAL:** Geometrie, Materialien, Skeleton/Pose und Startmotion des sichtbaren lokalen Warriors, NPCs und Mobs; sichtbare Weltgeometrie im aktuellen Kamerabereich. Ohne diese Daten ist der erste korrekte Frame nicht möglich.
- **NEAR:** naheliegende Gebäude/Props und die nächsten üblichen Bewegungs-/Kampfanimationen der tatsächlich sichtbaren Actors.
- **DEFERABLE (Kandidaten, kein Ladeumbau):** weit entfernte Objektressourcen, nicht sichtbare Rassen/Equipmentvarianten und seltene Tanz-/Emote-/Skillclips. Das Probe-Startup registriert alle vier Playerklassen synchron. Die teuren `dance_1` bis `dance_6` werden für den sichtbaren Idle-Frame nicht alle benötigt.

Der Trace erfasst kein vollständiges Visibility-/Gameplay-Abhängigkeitsorakel; deshalb keine erfundenen exakten CRITICAL-/NEAR-/DEFERABLE-Dateizahlen. Nicht alle synchron vorbereiteten GR2 sind nachweislich für den ersten sichtbaren Frame nötig. Streaming, Lazy Loading, Async oder Pop-ins wurden nicht eingeführt.

## SELECTED FIX und Kandidatenpriorität

1. **Ausgewählt: Symbolsuche innerhalb der 1108.359-ms-Decodierschleife.** 68.2 Mio. kleine Suchen im aktuellen Corpus; exakt überprüfbarer, lokaler Algorithmuswechsel, geringer/mittlerer Implementierungsumfang. `SymbolUpperBound` verwendet unaligned SSE2-Vierervergleiche nur bei Intervalllänge ≤16, niemals außerhalb `[first,last)`. Signed SIMD ist für validierte CDF-Werte 0…16384 exakt. Größere Intervalle sowie Plattformen ohne SSE2 verwenden weiterhin `std::upper_bound`. Keine andere Decoderoperation geändert.
2. Allokationen/Copies: Output-Allokation 3.616 ms, Uploadkopien 1.431 ms, Modellsetup 61.310 ms. Kleineres belegtes direktes Potenzial; nicht umgesetzt.
3. Kurvenkonvertierung 404.845 ms: nächster großer separater GR2-Schritt, anderer Fixbereich; unverändert.
4. Sichere Datei-/Sectionparallelität: theoretisch größeres Wandzeitpotenzial, aber Produktions-Thread-Safety und Cache/Fehlerkonzept ungeklärt; nicht gewählt.
5. Deferring/Offline-Representation/Persistent-Cache: Bedarf vorhanden, aber außerhalb dieses Fixes bzw. explizit ausgeschlossen. I/O mit 22.644 ms ebenfalls nicht verfolgt.

Eine zunächst geprüfte skalare Variante derselben Symbolsuche war langsamer und wurde verworfen (1331.60 → 1411.92 ms). Die finale begrenzte SIMD-Variante ist der **einzige verbleibende funktionale Fix**. Kein zweiter Optimierungsbereich wurde geändert. Der frühe native Diagnoselauf `l5-before` bestätigte den Hotspot, erfasste aber Cachetreffer/späte GPU-Aufrufe noch nicht vollständig; er wird nicht als finales A/B-Paar verwendet. Nach Diagnoseergänzung genau ein akzeptierter Before- und ein After-Prozess. Kein nachträgliches Auswählen aus zehn Läufen.

## KORREKTHEIT

**PASS:** 183 523 deterministische Upper-Bound-Abfragen einschließlich leerer/duplizierter CDF-Einträge, Randwerte und unaligned Teilintervalle. Vergleich gegen die vorherige Implementierung aus `5563d78`:

- **903 reale Dateien / 7224 Sections / 60 042 028 Outputbytes byte-for-byte identisch.** Die Pfade stammen frisch aus A1/B1; lokale Corpusdateien stimmen in Größe und validierter GR2-Payload-CRC mit den produktiven Packloads überein. Keine zusätzliche kryptographische Identität des kompletten Packpayloads behauptet.
- **2226 halbierte nichtleere Section-Payloads korrekt abgelehnt**; zusätzlich **2226 halbierte Payloads mit passend verkürzter deklarierter Größe** abgelehnt. Letzteres erreicht die Decoder-/Streamprüfungen statt lediglich den Längenvergleich. Header-/Offset-/CRC-/Modell-/Referenzfehler werden zusätzlich durch GR2Safety/Compatibility geprüft.
- Beide getrennt gelinkten Corpusreader melden pro Datei identische Counts: **217 Modelle, 281 Meshes, 217 Skeletons, 729 Bones, 693 Animationen**. Vollständige per-Datei-Countlisten SHA256-identisch: `195AB5931C8F994ADCDF77C20FBABB4069C730467F2420FFE9D8E101BAF8A473`.
- Produktions-Traces: gleiche GR2-Pfade, Dateigrößen, CRCs, Sectionbytes, Parse-/Mesh-/Skeleton-/Animationscounts, Requests/Hits/Misses sowie Vertex-/Index-Kopien und Shaderbytecodeidentitäten. Kein Arbeitsvolumen zur Zeitersparnis weggelassen.

## FAST GATE – PASS

- Finaler Release x64 Client und fokussierte GR2-Testtargets gebaut, Exit 0. Vorhandene Vendor-/Python-PDB-/LTCG-Linkerwarnungen; kein warning-free Build behauptet.
- **A1 Cold, B1, A1 Warm, sauberer Exit 0** vor/nach. Shadercompile 0 in allen finalen Fenstern.
- **Diligent ERROR/FATAL = 0; CPU deformation = 0; GPU fallback = 0; alle 30 geprüften Fehler-/Resourcezähler = 0.** Zusätzlich Modern-/Water-/SSR-Prüfungen 0 und leeres Python-Fehlerlog.
- Byte correctness / invalid-truncated corpus PASS; gezielte Tests **4/4 PASS**: GR2Safety, GR2Compatibility, GR2Golden.static, GR2Golden.animation. Keine volle 70+-Suite, kein Debug/GCC/LP64, kein Netzwerklogin.
- Kurze Sichtprüfung von A1 Before/After und B1 After: Terrain, Vegetation, Player, NPC und Mob vorhanden; keine sichtbare Qualitätsänderung. Animationszeitstände unterscheiden sich. Byte-/Countgleichheit und bestehende Goldens stützen unveränderten Assetoutput; keine vollständige manuelle Originalclient-/UI-Abnahme behauptet.

## Reproduktion und lokale Belege

`tests/Loading/gr2_pipeline.py` erstellt Breakdown, Dateirangliste und Sectionstatistik. `verify_gr2_pipeline.py` prüft gleiche Arbeit und Shadercache-/Warm-Bedingungen, ohne einen gewünschten Zeitgewinn zu erzwingen. `run_gr2_decoder_parity.ps1` erzeugt die Referenz aus dem angegebenen Commit ausschließlich in einem frischen Ausgabeverzeichnis; keine Referenzimplementierung im Produktionspfad.

```powershell
# VS x64 Developer Shell, bestehendes konfiguriertes Release-Buildverzeichnis:
cmake --build build-h2x/msvc --config Release --target UserInterface GR2SafetyTest GR2CompatibilityTest GR2GoldenTest --parallel 6
ctest --test-dir build-h2x/msvc -C Release -R '^AssetRuntime.GR2(Safety|Compatibility|Golden\.(static|animation))$' --output-on-failure
tests/Loading/run_shader_cache_probe.ps1 -Name fresh-gr2-name -CacheDirectory C:/absolute/filled-cache -ShaderLifecycle
python.exe tests/Loading/gr2_pipeline.py build-p0l/fresh-gr2-name
python.exe tests/Loading/verify_probe.py build-p0l/fresh-gr2-name --shader-lifecycle
python.exe tests/Loading/verify_gr2_pipeline.py build-p0l/before-name build-p0l/after-name
tests/Loading/run_gr2_decoder_parity.ps1 -CorpusList build-p0l5/decoder-files.txt -OutputDirectory build-p0l5/fresh-parity -Baseline 5563d78
```

Native Starts benötigen den vorhandenen autorisierten Windows-Start; keine Manifest-/Adminänderung. Die frühere sandboxbedingte Windows-SDK-Zugriffsverweigerung war ein Umgebungsfehler; final normal mit VS-x64 gebaut. Beim Aktivieren des gesicherten Fixes wurde der Source-Zeitstempel aktualisiert und die tatsächliche erneute Kompilation von `GR2Compression.cpp` im Buildlog sowie der geänderte EXE-Hash geprüft.

Nicht zu stagen: `build-p0l/l5-before-final`, `build-p0l/l5-after-final` (je Trace, gr2-pipeline.json, Fast-Gate, Screenshots, Logs), `build-p0l5` (Buildlogs, Referenz, Corpusliste, Bytevergleich, Countlisten, Hashes, verworfene Diagnosevarianten), Shadercache und sämtliche Testclients. Staging umfasst nur Source, Tests und diesen Bericht. Der ursprüngliche Client samt vorbestehenden Belegen blieb unverändert.

**Nächster größter Flaschenhals bleibt die GR2-Decodierschleife: 1 061.860 ms; gesamte Dekompression 1 155.559 ms. Danach Animation-Kurvenparsing 395.837 ms. Nur dokumentiert.**

Commit bei diesem GO: `perf(gr2): accelerate native decoder symbol lookup`. **Kein Push. STOP; kein zweiter GR2-Fix, kein P1/Content-TX.**
