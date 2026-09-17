# P0-L7 – Animation Loading Bottleneck

Historischer L7-Zwischenstand. Der gemeinsame Abschluss und der faire Production-Vergleich stehen in [P0-L8](p0l8-animation-runtime-prewarm.md): L7/L7C/L8 FINAL GO im dort dokumentierten Gate.

## P0-L7 SUMMARY

17.09.2026: **NO-GO nach dem vollständigen angeforderten kurzen Offline-Gate. Kein Commit, kein Push. STOP nach genau einem Animations-Fix.**

Der Ladefix ist wirksam und korrekt: A1 Cold **2228.658 → 1337.390 ms**, **891.268 ms / 39.991 %** weniger. **92.002 % der GR2-Dekompressions-Wallzeit** entfällt vor dem Fix auf Animationen. Genau ein funktionaler Fix wurde implementiert: begrenzte parallele native GR2-Vorbereitung innerhalb der vorhandenen synchronen Python-Ladestufe.

**GO-Blocker:** Beim ersten Attack/Death bleiben Frame-Spitzen über dem vorhandenen 50-ms-Stabilitätsbudget; Player Death nachher **79.307 ms**. Der Vergleich mit der gesicherten Vorher-EXE zeigt bereits dort entsprechende Spitzen (Player Attack 54.233 ms, Death 70.655 ms). In diesen Fenstern gibt es **0 GR2-Parses**; die Kosten liegen in `Document::BoundClip` / `GR2::BindAnimation`. Der Fix verschiebt keine GR2-Arbeit nach dem First Frame. Trotzdem ist „keine First-Use-Stalls“ nicht erfüllt und wird nicht als PASS umgedeutet. Keine zweite Optimierung an BindAnimation/Prewarm begonnen.

## STARTSTAND / SCOPE

- Source: `m2dev-client-src`, Branch `codex/g56-hdr-atmosphere`, HEAD **873d989**; davor `73dc92e`, `5563d78`.
- Separates Runtime-/Python-Source-Repository: `m2dev-client`, Branch `codex/g56-hdr-colors`, HEAD **548c43b4**.
- Vorbestehend untracked: Source `docs/performance/p0l3-shader-pso-breakdown.md`, Runtime `docs/p0l-map-loading-deployment.json` und `docs/p0l2-shader-loading-deployment.json`. SHA256-identisch erhalten; nichts gestagt.
- Einzige Änderung im Runtime-Repository ist **Python-Source** `assets/root/playersettingmodule.py`: vorhandenen `load()`-Aufruf synchron durch `chrmgr.LoadMotionDataBatch(load)` ausführen, mit `hasattr`-Kompatibilität für ältere Clients.
- Originalclient-EXE und Original-`root.pck` SHA256-identisch. Kein Deployment; die Messungen verwenden private Offline-Probes mit Originalpacks und jeweils einem privaten Root-Pack.
- Area-Parallelismus (`Area.cpp`) und SSE2-Decoder (`GR2Compression.cpp`) SHA256-identisch. Keine Änderungen an Shadercode, Renderer, Terrain, Vegetation, GR2-Format, Clips, Keyframes oder Qualität. Alle **73** gefüllten Shadercache-Dateien nach allen Läufen hash-identisch.

## REALER PRODUKTIVER CALL PATH / THREAD SAFETY

| Stufe | Produktiver Pfad / Zustand | Klassifikation |
| --- | --- | --- |
| Request / MSA | `playersettingmodule.LoadGameData` → `chrmgr.RegisterCacheMotionData` → `CRaceData::RegisterMotionData` → `CRaceMotionData::LoadMotionData`; MSA liefert tatsächlichen GR2-Pfad und Motion-ID | **OWNER THREAD ONLY**: Python/GIL, Textloader, Race-Pools, Effekte |
| Registrierung / Cache lookup | `NEW_RegisterMotion` → `CResourceManager::GetResourcePointer`; unveränderte Pfadnormalisierung/CRC und CResource-Identität | **OWNER THREAD ONLY**: Resource-Maps, statische Pfadbuffer, Motion-Vektoren |
| File read / Input | `CPackManager::GetFile`, pro neu vorbereitetem Pfad eigener Payload-Vektor | Packintern **SHARED LOCKED**, vollständiger Consumer-/Diagnosepfad bewusst **OWNER THREAD ONLY** |
| Header / CRC / Decode | `GR2::File` → `Inspect` → Section-Dekompression; Oodle1-Decoder, adaptive Tabellen und Scratch | **THREAD LOCAL**; unveränderliche CRC-Tabelle nach sicherer statischer Initialisierung **THREAD SAFE** |
| Section parsing | Bounds, Fixups, Relocations, `Types`, Typgraph | **THREAD LOCAL**; Sections einer Datei fertig vor dem Objektgraph |
| Animation parsing / Trackdaten | `Read` → `ReadAnimation` → `ReadCurve`; eigene Gruppen, Namen, Knot-/Control-Vektoren, Counts und Metadaten | **THREAD LOCAL**; kein globaler Clipcache, keine Runtime-Registrierung |
| CPU Skeleton-IR | `RuntimeSkeleton::Initialize` falls im Container vorhanden | Daten **THREAD LOCAL**; Lebensdauerzähler atomar **THREAD SAFE**, BindingId Inhalts-Hash |
| Publish / Cache | Provider `Preparation::Take` → `Document`; `CResource::Load`, `LoadStaticCache`, `CGraphicThing::LoadMotions`, `CGrannyMotion::BindAsset` | **OWNER THREAD ONLY**, ursprüngliche Anfragefolge |
| Skeleton lookup / Bone mapping | Actor `SetRace` / `RegisterMotionThing`; `Instance::SetMotion` → `Document::BoundClip` → `FindTransformTrack` | **OWNER THREAD ONLY**: Actorzustand und mutable Document-Bindings |
| ZiiNAN Runtime preparation | `BindAnimation` → `ConvertCurve` → `RuntimeAnimationClip::Initialize`, Skeleton-Binding und Keyframevorbereitung | weiterhin **OWNER THREAD ONLY**, unverändert; nicht mit GR2-Dateidecode verwechseln |
| IDs / Handles | AssetId aus Pfad; Clip-/Modellindex aus unveränderter Dateireihenfolge; BoneId aus originalem Index; Runtime BindingId aus Skeletoninhalt | deterministisch, keine Vergabe durch Worker-Scheduling; Owner-Publish |
| MapLoadTrace | Worker-eigener TLS-State, Intervall-/Zählermerge erst nach allen Joins | **THREAD LOCAL** bis Join, danach **OWNER THREAD ONLY** |
| AnimationStallAudit | gemeinsame mutable Audit-/Framezähler | **OWNER THREAD ONLY**; bei aktivem Audit ist Preparation deaktiviert und der serielle Pfad bleibt erhalten |
| GPU / Materials / Motion-Adapter | unveränderter Consumerpfad nach dem Join | **OWNER THREAD ONLY**; keine GPU-Aufrufe in Workers |

Temporäre Eingabe-/Section-/Kurvenbuffer gehören jeweils dem Dateijob. `Contents` besitzt den kompletten Output; keine geliehenen GR2-Referenzen überleben den Job. Worker-Ausnahmen werden im Ergebnis gesammelt; der Owner erzeugt weiterhin `InvalidAsset`. Der vorhandene Preparation-Executor joint bereits gestartete Jobs auch bei Submission-/Futurefehlern. Kein Hintergrundjob überlebt die synchrone Ladestufe.

## BEFORE / AFTER

Neue Prozesse, identische Einstellungen und Originalpacks, gefüllter Shadercache. „Cold“ bedeutet Prozess-cold, **kein geleerter OS-Dateicache**. Warm ist A1 nach **A1 → B1 → A1**. Endpunkt: drei stabile World-Presents ohne neue Lade-/Upload-/Compilearbeit; Screenshots folgen danach. Ein akzeptiertes Vorher-/Nachher-Paar, keine statistische Benchmarkserie.

| Messung | Before ms | After ms | Ersparnis ms | % |
| --- | ---: | ---: | ---: | ---: |
| A1 Cold TOTAL | 2228.658 | 1337.390 | 891.268 | 39.991 |
| A1 Warm TOTAL | 165.859 | 165.126 | 0.734 | 0.442 |
| A1 Cold GR2 Wall | 1804.550 | 939.389 | 865.160 | 47.943 |
| Animation Decode Wall | 1025.533 | 520.205 | 505.327 | 49.275 |
| Animation Parse Wall, inkl. Referenzen | 430.777 | 305.865 | 124.912 | 28.997 |
| Animation Parse kumuliert, exklusiv | 410.129 | 426.763 | -16.634 | -4.056 |
| Animation Runtime Preparation | 41.284 | 91.732 | -50.448 | -122.198 |
| GR2 Dekompression Wall gesamt | 1114.688 | 608.492 | 506.195 | 45.411 |
| Animation Decode kumulierte Dauer | 1025.533 | 1069.229 | -43.696 | -4.261 |
| Animation Parse kumulierte Dauer | 430.777 | 447.684 | -16.907 | -3.925 |
| B1 TOTAL | 304.426 | 287.794 | 16.632 | 5.463 |

**CPU und Wall getrennt:** Decode-Wall ist die Vereinigung überlappender Decodeintervalle je Batch plus serielle Decodezeit. Parse-Wall entsprechend für `ReadAnimation`. Decode und Parse verschiedener Dateien können überlappen: ihre Wallwerte **nicht addieren**. GR2 Wall verwendet kumulierte exklusive GR2-Dauern minus Worker-Dauern plus Batch-Wartefenster. Kumulierte Bereichsdauer ist verstrichene Zeit innerhalb CPU-Code, nicht reine Thread-CPU. Die zusätzlich erfassten Windows-Threadzeiten sind grob quantisiert.

Die früher genannten ~405 ms waren **exklusive** Kurvenparsezeit. Hier ist die vergleichbare neue Before-Zahl 410.129 ms; für die echte Stage-Wall-Messung einschließlich Referenzauflösung werden 430.777 ms verwendet. Die kumulierte CPU-Bereichsdauer steigt unter Parallelität etwas; der Gewinn stammt aus Überlappung. Runtime-Preparation schwankt wegen normaler Zufallsauswahl von Idle-Varianten; diese 41.284 → 91.732 ms sind kein zusätzlicher Fix. Warm decodiert und parst weder vorher noch nachher eine GR2-Datei. Seine kleine Differenz wird nicht als Parallelgewinn ausgegeben.

## ANIMATION FILES

| A1 Cold | Before | After |
| --- | ---: | ---: |
| requests | 2099 | 2099 |
| unique | 693 | 693 |
| loads | 693 | 693 |
| parses | 693 | 693 |
| cache_hits | 1406 | 1406 |
| compressed_bytes | 25720482 | 25720482 |
| expanded_bytes | 44040216 | 44040216 |

Requests sind tatsächliche `GetResourcePointer`-Aufrufe, Cache hits bestehende CResource-Lookups; ein Lookup-Hit allein bedeutet nicht zwingend bereits decodierten Inhalt. „Loads“ zählt tatsächliche leere CResources, „Parses“ tatsächliche native GR2-File/Read-Ausführungen. Keine doppelten Parses; auch über A1 → B1 → A1 insgesamt unverändert 903 Dateien. A1 Warm: Requests/Loads/Parses/Decode jeweils 0; vorhandene Motion-/Document-Referenzen bleiben nutzbar.

## DECOMPRESSION BY CATEGORY

Animation wird durch den **tatsächlichen geparsten AnimationCount** erkannt, nicht durch Clipnamen. Die übrigen Container werden anhand ihres realen VFS-Bereichs den Area-/Prop- oder Actor-/Model-Pfaden zugeordnet. Alle A1-Dateien sind klassifiziert, Other=0. Bytes beziehen sich auf Section-Payloads; Dateiheader/Typmetadaten außerhalb komprimierter Sections sind nicht „compressed bytes“.

| Kategorie | Dateien | Compressed B | Expanded B | Before kumuliert ms | Before Wall ms | After Wall ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Area/Prop | 129 | 1915467 | 7326112 | 123.155 | 75.109 | 74.776 |
| Animation | 693 | 25720482 | 44040216 | 1025.532 | 1025.533 | 520.205 |
| Actor/Model | 9 | 283345 | 894676 | 14.046 | 14.046 | 13.511 |
| Other | 0 | 0 | 0 | 0.000 | 0.000 | 0.000 |

**Exakte Antwort für die frische Baseline:** Animationen verursachen **1025.533 von 1114.688 ms = 92.002 %** der Dekompressions-Wallzeit. Kumuliert sind es 1025.533 von 1162.733 ms. Die historischen ~1140 ms von P0-L6 werden nicht nachträglich geschätzt oder mit dem neuen Lauf vermischt.

## FIRST PLAYABLE / SOFORT ALLE CLIPS?

Ja, der Player-Startup ruft `RegisterCacheMotionData` für beide Geschlechter aller vier Klassen auf. MSA-Dateien werden registriert und ihre GR2-Dateien synchron in den Static Cache geladen, auch wenn der sichtbare Actor diese Motion nicht abspielt. Weitere nur registrierte Emote-/NPC-/Mob-Clips bekommen spätestens bei `CActorInstance::SetRace` über `RegisterMotionThing` eine Resource-Referenz und werden dabei geladen. `RegisterRaceName` allein lädt dagegen nicht alle NPC-Dateien.

Die Diagnose zeichnet `animation-registration` als **GR2-Pfad | Race-ID | Motion-Mode | Motion-ID** auf; `animation-actor-reference` belegt tatsächlich instanziierte Rassen, `animation-active-mode` den angeforderten Actor-Mode und `animation-use-before-present` einen erfolgreichen nativen `SetMotion` vor dem ersten World-Present. Cliptypen im Report stammen aus den Motion-ID-Enums, nicht aus Dateinamen.

| Klasse im beobachteten A1-Start | Dateien | Before GR2-Dateikosten ms | Decode ms | Parse ms |
| --- | ---: | ---: | ---: | ---: |
| CRITICAL | 4 | 49.859 | 5.712 | 2.108 |
| NEAR | 6 | 9.643 | 6.475 | 2.289 |
| DEFERABLE | 683 | 1573.755 | 1013.346 | 426.380 |

- **CRITICAL:** die vier tatsächlich vor dem Present gebundenen Clips, einschließlich anfänglichem General- und anschließendem Waffen-Idle. Sie werden vom realen Startpfad benötigt.
- **NEAR:** sechs weitere registrierte Idle-/Walk-/Run-Referenzen der tatsächlich vorhandenen Actors in ihren angeforderten Modes.
- **DEFERABLE:** alle übrigen 683 Dateien ohne vorzeitigen Playback-Request; die vollständige Datei-Zuordnung mit Race-/Motion-Referenzen steht im lokalen `animation-loading.json` und `build-p0l7/animation-files.csv`. Das ist eine Beobachtung dieses A1-Starts, **keine Garantie** für beliebige Gameplay-/Netzwerksituationen oder sicheres späteres Streaming.

Die noch nicht abgespielten Dateien kosten vor dem Fix **1573.755 ms / 70.614 % A1 Cold**. Das ist theoretisches Verschiebepotenzial, keine als sicher freigegebene Lazy-Loading-Ersparnis. Der nachgewiesene Runtime-First-Use-Hitch spricht zusätzlich gegen aggressives Verschieben.

## SELECTED FIX / SINGLE FLIGHT / DETERMINISMUS

**A) bounded parallel animation decode/parse**, genau ein Fix. Vorhandenen `GR2::Preparation`-Mechanismus in eine explizite synchrone Python-Ladegrenze eingebunden. `RegisterMotionData` und MSA-/Race-Registrierung laufen unverändert sofort auf dem Owner; nur die bisher unmittelbar anschließenden Static-Cache-Ladevorgänge werden innerhalb dieser Callback-Grenze in Gruppen von maximal 32 Anforderungen gesammelt. Der letzte Teilbatch wird auch bei einer Python-Ausnahme abgearbeitet, bevor der Fehler zurückgegeben wird. Außerhalb dieser expliziten Grenze bleibt die bisherige API seriell.

Der Owner liest neue, noch nicht geladene GR2-Payloads. Der bestehende CGameThreadPool verarbeitet File/Read mit höchstens **min(Poolgröße, 4, max(1, Hardwarethreads−2), Dateizahl)** Lanes. Maximal 32 Ergebnisdateien / bestehende 16-MiB-Input-Aufnahmeschwelle je Batch; der letzte Input kann die Schwelle überschreiten, weitere Inputs fallen auf den bisherigen seriellen Pfad zurück. Kein neuer Threadpool, keine Section-Parallelisierung, keine verschachtelten Pool-Jobs. Gemessener Pool: 16 logische CPUs / 16 Poolthreads, maximal 4 aktive Jobs.

Nach allen Joins erfolgen Static-Cache-Loads/Provider-Publish in ursprünglicher Anfragereihenfolge. Vorbereitungsschlüssel normalisieren Slash-/Großschreibung und vereinigen doppelte Pfade **vor Read/Parse**. Wiederholungen nach Veröffentlichung nutzen den bestehenden Cache. Kein Worker greift auf ResourceManager, Python, mutable Clipcache oder IDs/Handles zu. Neue Implementierung verändert weder Tracks noch deren Reihenfolge noch die Kurven-/Runtimealgorithmen.

Produktiver Scope: **646 von 693** A1-Animationsdateien in den Batches. Die übrigen 47 Animationen aus den bisherigen nicht gecachten Registrierungs-/Actorpfaden bleiben seriell; insbesondere `emotion.py` benutzt `RegisterMotionData`, nicht `RegisterCacheMotionData`. Alle 693 sind weiterhin vor dem First Frame geladen. Keine Clips ausgelassen/gelöscht/verkürzt, kein Deferring, keine neue Backgroundlast nach der Ladestufe. Der vorhandene Area-Batch-Scope bleibt 98 A1-Dateien / 67 B1-Dateien.

Kandidatenentscheidung: Parallelisierung nutzt den größten gemessenen Block (1025.533 ms Decode plus 430.777 ms Parse) bei begrenztem Consumer-Umbau und vorhandener deterministischer IR. Deferring hätte größeres theoretisches Verschiebepotenzial, aber keinen sicheren First-Use-Nachweis. Ein Parser-Mikrofix hätte nur einen Teil der 430.777 ms beeinflusst und wurde nicht zusätzlich umgesetzt.

## ANIMATION CONCURRENCY

| Messwert, nur A1-Animation-Batches | Wert |
| --- | ---: |
| Dateijobs | 646 |
| Peak concurrency | 4 |
| Average concurrency | 3.434 |
| Worker busy ms | 1366.642 |
| Batch wall ms | 397.916 |
| Grobe kumulierte Worker Thread-CPU ms | 1187.500 |
| Single-flight joins | 138 |

Average = Summe aktiver Jobdauern / Summe der Batch-Wartefenster. Joins zählen identische normalisierte Pfade innerhalb eines Batches; spätere Cachetreffer sind getrennt erfasst. B1 und Warm haben 0 Animation-Jobs. Keine Oversubscription durch zusätzliche Pools; die normalen vorhandenen Pool- und Owner-Grenzen bleiben erhalten.

## TOP 30 ANIMATION FILES – BEFORE

Total ist die Summe exklusiver dateizuordbarer Bereiche einschließlich vorhandener Owner-Preparation. Decode / Parse / Prep sind ihre gesonderten Stage-Dauern; Parse enthält Referenzauflösung. Deshalb nicht sämtliche Spalten blind addieren. „Race“ stammt aus realer Registrierung; Pfad liefert zusätzlich den Actornamen. Größen sind komprimierte Sectionbytes bzw. expandierte Sectionbytes.

| # | Path | Compressed B | Expanded B | Sections | Decode ms | Animation parse ms | Runtime prep ms | Total ms | Race IDs | Cliptyp |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | --- |
| 1 | `d:/ymir work/pc/warrior/action/dance_6.gr2` | 785405 | 1252276 | 8 | 26.963 | 9.534 | 0.000 | 38.184 | 0 | Other |
| 2 | `d:/ymir work/pc/warrior/action/dance_1.gr2` | 640140 | 1103640 | 8 | 21.197 | 8.104 | 0.000 | 30.668 | 0 | Other |
| 3 | `d:/ymir work/pc/warrior/action/dance_3.gr2` | 553872 | 868384 | 8 | 18.898 | 6.763 | 0.000 | 26.924 | 0 | Other |
| 4 | `d:/ymir work/pc/warrior/action/dance_4.gr2` | 526054 | 851624 | 8 | 18.378 | 6.244 | 0.000 | 25.785 | 0 | Other |
| 5 | `d:/ymir work/pc/warrior/action/dance_5.gr2` | 329549 | 518708 | 8 | 10.808 | 3.831 | 0.000 | 15.452 | 0 | Other |
| 6 | `d:/ymir work/monster/stray_dog/00_1.gr2` | 28458 | 47848 | 8 | 1.181 | 0.392 | 11.934 | 13.658 | 101 | Idle |
| 7 | `d:/ymir work/pc/warrior/onehand_sword/wait.gr2` | 42385 | 69448 | 8 | 1.722 | 0.621 | 10.658 | 13.248 | 0 | Idle |
| 8 | `d:/ymir work/npc/goods/wait.gr2` | 46108 | 76944 | 8 | 1.840 | 0.644 | 9.715 | 12.397 | 9003 | Idle |
| 9 | `d:/ymir work/pc/warrior/onehand_sword/wait_1.gr2` | 22490 | 41604 | 8 | 0.969 | 0.451 | 8.978 | 10.555 | 0 | Idle |
| 10 | `d:/ymir work/pc/warrior/action/banter.gr2` | 169014 | 271368 | 8 | 6.358 | 2.139 | 0.000 | 8.988 | 0 | Other |
| 11 | `d:/ymir work/pc/warrior/action/forgive.gr2` | 161748 | 268108 | 8 | 5.713 | 2.473 | 0.000 | 8.625 | 0 | Other |
| 12 | `d:/ymir work/pc/warrior/action/sad.gr2` | 136455 | 226216 | 8 | 5.061 | 1.866 | 0.000 | 7.350 | 0 | Other |
| 13 | `d:/ymir work/pc2/sura/general/dead.gr2` | 127953 | 215948 | 8 | 4.513 | 1.873 | 0.000 | 6.761 | 6 | Death |
| 14 | `d:/ymir work/pc2/shaman/fishing/fishing_catch.gr2` | 122699 | 193508 | 8 | 4.219 | 1.559 | 0.000 | 6.181 | 7 | Other |
| 15 | `d:/ymir work/pc/sura/fishing/fishing_fail.gr2` | 116477 | 185280 | 8 | 4.115 | 1.614 | 0.000 | 6.132 | 2 | Other |
| 16 | `d:/ymir work/pc/sura/general/dead.gr2` | 119473 | 195328 | 8 | 4.152 | 1.580 | 0.000 | 6.087 | 2 | Death |
| 17 | `d:/ymir work/pc2/sura/fishing/fishing_catch.gr2` | 101636 | 165788 | 8 | 3.796 | 1.376 | 0.000 | 5.505 | 6 | Other |
| 18 | `d:/ymir work/pc/warrior/action/congratulation.gr2` | 91457 | 146448 | 8 | 3.330 | 1.218 | 0.000 | 4.864 | 0 | Other |
| 19 | `d:/ymir work/pc/warrior/action/joy.gr2` | 89004 | 147260 | 8 | 3.345 | 1.189 | 0.000 | 4.828 | 0 | Other |
| 20 | `d:/ymir work/pc/warrior/action/cheerup.gr2` | 91666 | 147216 | 8 | 3.232 | 1.224 | 0.000 | 4.760 | 0 | Other |
| 21 | `d:/ymir work/pc/warrior/action/angry.gr2` | 86465 | 143240 | 8 | 3.156 | 1.267 | 0.000 | 4.715 | 0 | Other |
| 22 | `d:/ymir work/pc/warrior/action/shy.gr2` | 86468 | 143672 | 8 | 3.132 | 1.179 | 0.000 | 4.661 | 0 | Other |
| 23 | `d:/ymir work/pc/warrior/action/french_kiss_with_shaman.gr2` | 90514 | 142016 | 8 | 3.103 | 1.184 | 0.000 | 4.567 | 0 | Other |
| 24 | `d:/ymir work/pc/warrior/action/attractive.gr2` | 86928 | 142288 | 8 | 3.078 | 1.142 | 0.000 | 4.565 | 0 | Other |
| 25 | `d:/ymir work/pc/warrior/action/french_kiss_with_assassin.gr2` | 90539 | 142096 | 8 | 3.090 | 1.115 | 0.000 | 4.487 | 0 | Other |
| 26 | `d:/ymir work/pc/warrior/fishing/fishing_fail.gr2` | 82867 | 132892 | 8 | 3.053 | 1.115 | 0.000 | 4.464 | 0 | Other |
| 27 | `d:/ymir work/pc2/shaman/general/dead.gr2` | 65149 | 110584 | 8 | 2.727 | 1.417 | 0.000 | 4.422 | 7 | Death |
| 28 | `d:/ymir work/pc2/warrior/fishing/fishing_catch.gr2` | 85435 | 130804 | 8 | 2.965 | 1.122 | 0.000 | 4.378 | 4 | Other |
| 29 | `d:/ymir work/pc/shaman/intro/selected.gr2` | 69605 | 113828 | 8 | 2.818 | 1.218 | 0.000 | 4.335 | 3 | Other |
| 30 | `d:/ymir work/pc2/assassin/intro/selected.gr2` | 63667 | 103452 | 8 | 2.521 | 1.522 | 0.000 | 4.309 | 5 | Other |

## CORRECTNESS

- **PASS:** frisch gegen die gemessenen A1/B1-Pfade validierter Corpus: 903 Dateien, 7224 Sections, **60.042.028 expandierte Bytes bytegleich** seriell vs parallel. Inputgrößen und native Payload-CRCs gegen den Packtrace geprüft.
- **PASS:** komplette relevante IR-Felder bitgleich: 217 Modelle, 281 Meshes, 282 Materialien, 217 Skeletons, 729 Bones, **693 Animationen**. Namen, IDs, Dauer/TimeStep, Track-/Gruppenreihenfolge, Bone-Referenzen, alle Knot-/Controlwerte, Events, Bind-Matrizen und Skeleton-EvaluationOrder im Vergleich. Kein Pointer-/Paddingvergleich.
- **PASS:** 13 reale Modell-/Clip-Paare: Warrior Idle/Walk/Run/Attack/Damage/Death, Wildhund dieselben sechs Typen, Goods-NPC Idle. Alle Runtime-Keys, Zielbones, Track-Interpolation, Duration, Looping und BindingIds bitgleich für Once- und Loop-Ränder; anschließend **234 exakte Provider-/Runtime-Posevergleiche**. Prepared Provider-Publish führt keinen zweiten Parse aus; AssetId und Handle-Indizes gleich.
- **PASS:** 903 halbierte Dateien zusammen mit gültigen Dateien sauber abgelehnt. Synthetische malformed/truncated Dateien weiter `InvalidAsset`; vier wiederholte Batches mit Pfadalias-/Duplikatanfragen sowie injizierter Executorfehler. Keine beobachteten Race-/Deadlocksymptome, alle Jobs gejoint, Lebensdauerzähler 0. Kein ThreadSanitizer-Lauf behauptet.
- **PASS:** native Before-/After-Traces: identische Requests/Hits/Misses, Registrierungen/Actorreferenzen, Parsepfade, CRCs, Sectionbytes, Modell-/Skeleton-/Animationscounts, Kopierbytes und Shaderbytecodes. Keine ausgelassene Datei und keine zusätzliche Warm-Decodierung.

## FAST GATE – FAIL wegen verbleibender First-Use-Stalls

| Check | Ergebnis |
| --- | --- |
| Release Build, finale gemessene EXE | PASS, Exit 0; vorhandene Vendor-/Linkerwarnungen |
| Animation-Korrektheit / focused tests | PASS, 7/7: Preparation, Independence, Safety, Compatibility, static/animation Golden, Warmup |
| A1 Cold / A1 Warm / A1 → B1 → A1 | PASS, je frischer Before-/After-Prozess |
| Kurzer Actor-/Mob-Smoke | PASS für vorhandene/ausgeführte Idle, Walk, Run, Attack, Damage, Death; reale erfolgreiche Native-Requests für beide Actors überprüft |
| Keine First-Use-Stalls | **FAIL**: 4 After-Fenster >50 ms; Runtime-Vorbereitung bleibt synchron beim ersten Binding |
| Diligent ERROR/FATAL | 0 / 0 |
| GPU fallback / CPU deformation | 0 / 0 |
| Shutdown resources / Exit | alle 30 geprüften Fehler-/Resourcezähler 0, alle vier Prozesse Exit 0 |
| Shadercompile / Cachemiss | 0 / 0 im Messpaar; Cachedateien hash-identisch |

Die „fast-gate.json“-Dateien des älteren Map-/Resource-Verifiers melden nur dessen begrenzte Teilprüfungen PASS. Das vollständige **P0-L7-Gate** ergänzt die First-Use-Prüfung und steht in `l7-after/animation-gate.json` ausdrücklich auf **FAIL / NO-GO**.

Smoke je Motion eine Sekunde, nach dem Map-Gate, ohne Netzwerklogin; Locomotion über normale Loop-API, Attack/Damage/Death über normale Once-Queue. Erfolgreiche Playbackpfade werden gegen reale Race-/Motion-Registrierungen abgeglichen. Screenshot-I/O ist aus den gemeldeten nachfolgenden Frameintervallen ausgenommen. 50 ms ist das schon vorhandene Probe-Stabilitätsbudget, keine Behauptung, 49 ms seien unsichtbar. Ein 79-ms-Frame mit 79 ms Runtime-Preparation erlaubt kein uneingeschränktes „keine sichtbaren Hänger“. Kurze Screenshot-Sichtprüfung bestätigt Player/NPC/Mob und Posen; keine manuelle Vollprodukt-/Originalclient-Abnahme behauptet.

| Actor / Motion | Before max frame ms | After max frame ms | After Runtime prep im Fenster ms |
| --- | ---: | ---: | ---: |
| player / Idle | 19.050 | 18.568 | 0.001 |
| player / Walk | 23.089 | 23.370 | 23.204 |
| player / Run | 28.147 | 28.516 | 28.382 |
| player / Attack | 54.233 | 52.566 | 44.319 |
| player / Damage | 22.396 | 27.635 | 27.399 |
| player / Death | 70.655 | 79.307 | 79.078 |
| mob / Idle | 60.047 | 19.051 | 0.001 |
| mob / Walk | 19.573 | 19.545 | 19.407 |
| mob / Run | 26.845 | 27.038 | 26.883 |
| mob / Attack | 55.059 | 55.094 | 54.918 |
| mob / Damage | 19.191 | 19.585 | 19.466 |
| mob / Death | 59.624 | 51.367 | 41.812 |

Alle Smoke-Fenster haben **0 neue GR2-Parses**. Zufällige Idle-/Attack-/Damagevarianten können zwischen Prozessen wechseln; die gleichen verfügbaren Referenzen und deren Inhalte sind separat exakt geprüft. Die First-Use-Spitzen sind also schon im seriellen Referenzpfad vorhanden, aber nicht beseitigt.

## REPRODUKTION / BELEGE

Lokale, nicht zu commitende Belege:

- `build-p0l/l7-before`: neue Baseline, PID 6304, Exit 0; `animation-loading.json`, `gr2-pipeline.json`, Native-Logs, Bilder, Resource-Gate.
- `build-p0l/l7-after`: finaler funktionaler Fix, PID 19324, Exit 0; A/B- und vollständiges P0-L7-Gate.
- `build-p0l/l7-smoke-before` / `build-p0l/l7-smoke`: identischer Smoke mit gesicherter Before-/After-EXE, PIDs 8608 / 37128, beide Exit 0; jeweils `animation-smoke.json` und native Tracefenster.
- `build-p0l7`: Release-/Test-/Corpuslogs, `decoder-files.txt`, `runtime-pairs.txt`, komplette Datei-Klassifikation `animation-files.csv`, Hashbelege. Unbekannte vorherige Dateien erhalten.

EXE SHA256 Before: `54DABF9B24CBCECFBE97D52C04A5DF1405C29E735F35D62569327363367F949B`.
After, Smoke und aktueller Release-Build: `E1826294B2E24FAB400B94502432B704A27DAFFD9E4BDC7B2ACB808259ED254F`.

```powershell
# Vorhandene VS-x64-Umgebung; Python-Source im separaten Runtime-Checkout gehört zum Fix.
cmake --build build-h2x/msvc --config Release --target UserInterface GR2PreparationTest GR2SafetyTest GR2CompatibilityTest GR2GoldenTest GR2WarmupTest --parallel 6
ctest --test-dir build-h2x/msvc -C Release -R '^AssetRuntime.GR2(Safety|Compatibility|Golden\.(static|animation)|Preparation|Independence|Warmup)$' --output-on-failure
build-h2x/msvc/tests/AssetRuntime/Release/GR2PreparationTest.exe build-p0l7/decoder-files.txt build-p0l7/runtime-pairs.txt
tests/Loading/run_shader_cache_probe.ps1 -Name fresh-l7 -CacheDirectory C:/absolute/filled-cache -ShaderLifecycle
python tests/Loading/verify_probe.py build-p0l/fresh-l7 --shader-lifecycle
python tests/Loading/animation_loading.py build-p0l/fresh-l7
tests/Loading/prepare_probe.ps1 -Name fresh-l7-smoke -ShaderLifecycle -AnimationSmoke
tests/Loading/run_probe.ps1 -Name fresh-l7-smoke -ShaderCacheDirectory C:/absolute/filled-cache
python tests/Loading/verify_probe.py build-p0l/fresh-l7-smoke --shader-lifecycle --animation-smoke
python tests/Loading/verify_animation_loading.py build-p0l/l7-before build-p0l/l7-after build-p0l/l7-smoke-before build-p0l/l7-smoke
# Letzter Check liefert bewusst Exit 1 / NO-GO wegen der belegten First-Use-Spitzen.
```

Der erste Buildversuch scheiterte am sandboxbedingten Zugriff auf das vorhandene Windows SDK; der Build in der autorisierten VS-Umgebung war erfolgreich. Native Probe-Starts unverändert über den vorhandenen Windows-Startpfad, kein Manifest-/Admin-Umbau.

## NEXT BOTTLENECK / COMMIT / STOP

Größter verbleibender gemessener GR2-Ladeblock: **608.492 ms GR2-Dekompressions-Wall gesamt**, davon **520.205 ms Animationen**; Animationsparse-Wall **305.865 ms** (zwischen Worker-Stufen überlappend, nicht additiv). Daneben ist **späte Runtime-Clip-Vorbereitung bis 79.078 ms im Death-Fenster** der konkrete GO-Blocker. Nur dokumentiert.

**Commit-Hash: keiner. HEAD bleibt 873d989; Runtime-HEAD bleibt 548c43b4.** Änderungen liegen prüfbar und ungestagt in beiden Source-Bereichen. Kein Commit trotz Cold-Gewinn, weil das explizite GO-Kriterium fehlt. Keine Logs, Traces, Buildoutputs, Cachedateien, Testclients oder Backups gestagt; kein Push. Nach genau diesem einen Fix STOP. Kein persistenter GR2-Cache, kein P1, keine weitere Shader-/Parser-/Prewarm-Optimierung.
