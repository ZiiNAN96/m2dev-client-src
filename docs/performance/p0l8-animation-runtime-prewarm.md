# P0-L8 – Animation Runtime Preparation / Prewarm Optimization

## P0-L8 SUMMARY

17.09.2026: **FINAL P0-L7 + P0-L7C + P0-L8 = GO im angeforderten kurzen Release-/Offline-Gate.** Genau ein P0-L8-Fix: bereits berechnete Spline-Zwischenwerte bei adaptiver Unterteilung wiederverwenden. Keine neue Parallelisierung, keine geänderten Keys/Toleranzen, keine gekürzten oder entfernten Animationen.

Vollständiger A1 Cold **2354.366 → 2014.610 ms** (−339.756 ms / −14.43 %). Produktiver Prewarm **902.207 → 710.248 ms** (−191.959 ms / −21.28 %). Der gesamte A1-Gewinn ist größer als der direkt gemessene Prewarm-Gewinn; die zusätzliche Differenz wird **nicht** dem Fix zugeschrieben. Andere Ladephasen und normale Run-to-Run-Streuung tragen bei.

First Use bleibt warm: maximaler einzelner Runtime-Motion-Aufruf über alle 36 Einsätze **0.0106 ms**, 0 neue Runtime-Keys/Clip-Misses/GR2-Parses. **234 bestehende Posen plus 468 Posen gegen den eingefrorenen Vorher-Algorithmus identisch**, alle Keys/Bindings auf vier Randvarianten bitgleich, gezielte Tests **7/7 PASS**.

2014,6 ms bis zum stabilen Frame ist **nicht deutlich unter 2 s**. Dieses Wunschziel wird nicht behauptet; die GO-Kriterien verlangen einen klaren Gewinn und erhaltene First-Use-/Korrektheitsgarantien, keinen erzwungenen 2000-ms-Schwellwert.

## Startstand und Messdefinition

Source `m2dev-client-src`, Branch `codex/g56-hdr-atmosphere`, Ausgangs-HEAD **873d989**. L7/L7C lagen ungestagt vor; Anfangsdiff und Dateihashes wurden unter `build-p0l8/start.*` gesichert. Runtime-/Python-Source ist das separate Repository `m2dev-client`, Ausgangs-HEAD **548c43b4**. Fremde Dateien bleiben ungestagt: Source `docs/performance/p0l3-shader-pso-breakdown.md`, Runtime beide vorherigen Deployment-JSONs. Keine Resets, keine gelöschten Änderungen.

Alle Zeitläufe benutzen denselben privaten Release-Probe mit Originalpacks, gefülltem Shadercache, A1 → B1 → A1 und denselben Actors/Settings. Die echte Funktion `CPythonCharacterManager::PrewarmVisibleActors(true)` läuft vor dem World-Render in einem aktiven GPU-Frame. Im Netzwerkprodukt ruft `PythonNetworkStream::PrepareGamePhase` genau diese Funktion bei sichtbarem LoadingWindow auf. Der Offline-Probe ruft den produktiven Funktionskörper über die vorhandene Python-API auf; ein Netzwerklogin wurde wie beauftragt nicht ausgeführt.

FIRST PLAYABLE: erste native World-Präsentation **nach** Rückkehr der vollständigen Vorbereitung. A1 total endet für alle Läufe am bestehenden `stable-present`-Endpunkt. Beide Werte werden unten getrennt angegeben; die rund 35 ms bis Stabilität werden nicht aus After herausgerechnet. Cold = frischer Prozess, nicht geleerter OS-Dateicache. Keine statistische Langzeit-Benchmarkbehauptung.

A0 ist die gesicherte HEAD-873d989-EXE mit damaliger grober Messinstrumentierung; sie hat die ältere lokale Prewarm-Auswahl ohne Damage/Death. Deshalb gibt es zusätzlich **A**, den fairen seriellen Kontrollpfad mit derselben vollständigen L7C-Auswahl wie B/C: unveränderter Vorher-Keyalgorithmus, in der privaten Python-Kopie nur `LoadMotionDataBatch(load)` durch den ursprünglichen seriellen `load()` ersetzt. Die übrige L7-Implementierung ist dort nicht ausgeführt. **A ist kein unveränderter HEAD-Build**; A0 bleibt als exakte HEAD-Referenz ausdrücklich separat. Keine Produktdatei wird für diesen Kontrollpfad umgeschaltet.

## PRODUCTION BASELINE / P0-L7/P0-L7C / AFTER

Keine Detailmarker in diesen Zeitläufen. Angaben in ms; GR2 wall enthält die native Runtime-Bindung, Spalten deshalb nicht addieren.

| Variante | A1 total | First playable | GR2 wall | Animation decode wall | Animation parse wall | Runtime Prewarm |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| A0: exakte serielle HEAD-Referenz | 2938.448 | 2904.226 | 2515.151 | 1004.631 | 434.023 | 661.852 |
| A: seriell, vollständige L7C-Auswahl | 3162.098 | 3126.811 | 2732.603 | 1051.575 | 438.115 | 891.768 |
| B: L7/L7C vor L8 | 2354.366 | 2319.674 | 1901.120 | 620.203 | 398.436 | 902.207 |
| C: nach L8 | 2014.610 | 1979.622 | 1601.256 | 525.623 | 318.830 | 710.248 |

C ist gegenüber A **1147.489 ms / 36.29 %** schneller. Gegenüber der exakten, aber weniger vollständig vorbereitenden HEAD-Referenz A0 beträgt die Verbesserung **923.838 ms / 31.44 %**. Der frühere vereinfachte 1337-ms-Wert wird nicht als fairer Production-Vergleich verwendet.

B und C behalten 646 Animation-Dateijobs, maximal vier Worker und alle 693 Animations-Parses. A hat null Animation-Batchjobs; vorhandener Area-Parallelismus bleibt auch in A erhalten. Änderungen an Decode/Parse-Zeiten in dieser Tabelle sind keine weiteren L8-Fixes.

## Produktiver Callpath und Sharing

`playersettingmodule.LoadGameData` → L7-Batch → ownerseitiger Pack-Read → vorhandene Worker `GR2::File`/Decode/`ReadAnimation` → ownerseitiger Document-Publish mit Quellkurven. Noch keine vollständigen Runtime-Keyarrays für fremde Skeletons.

Nach Actor-Erzeugung: `PrepareGamePhase` → `PrewarmVisibleActors(true)` → `CActorInstance::PrewarmMotions` → `PrepareGR2Animation` → Modell-/Skeleton-/Trackgruppe prüfen → `Document::BoundClip`. Cache-Schlüssel bleibt Animationindex + **Skeleton.BindingId** + Trackgruppenindex, mit vier Boundary-Slots. Ein Treffer liefert denselben immutable Clip. Nur bei Miss: Track/Bone-Zuordnung → `ConvertCurve` → `RuntimeAnimationClip::Initialize` → Cache-Publish. First Use findet diese Struktur direkt.

Im diagnostizierten A1: **82 gültige Prewarm-Requests**, insgesamt **28 neue eindeutige Bindungen einschließlich initialer Idle-Bindungen**, **0 doppelte echte Bindungen** pro Schlüssel. B1 und A1 Warm: jeweils 82 Prewarm-Requests, **0 neue Bindungen**, vorhandene Dokumente behalten ihre Keyarrays. Cache-Limit bleibt 64 MiB pro Dokument / 64 Bindings; kein neuer globaler Cache, keine zusätzlichen Actor-Kopien.

Die anfängliche zufällige Idle-Variante verschiebt genau eine Bindung zwischen Actor-Erzeugung und Prewarm: Diagnose Before **23 neue Prewarm-Bindungen + 5 initiale**, After **24 + 4**. Beide haben dieselben 28 Schlüssel, Source-/Runtime-Daten und Gesamtspeichermengen. Diese Verteilung wird nicht als zusätzliche Arbeitseinsparung ausgegeben. Die First-Use-Prüfung pinnt wie L7C denselben registrierten Clip für die drei Wiederholungen.

## PREWARM BREAKDOWN

Separater opt-in Diagnose-Lauf mit privater Datei `animation-preparation-trace.enabled`, nicht für den Performance-Gewinn verwendet. Feine Timer pro Kurvenauswertung/Fehlerprüfung beeinflussen die Laufzeit deutlich: Before **2226.253 ms**, After **1899.223 ms** statt der oben genannten Zeitläufe. Prozentwerte beziehen sich auf diese diagnostische Prewarm-Wallzeit. Max clip = größter aggregierter Stagewert eines Clips, kein einzelner Key.

| Block Before | Total ms | Calls | Max clip ms | % Prewarm |
| --- | ---: | ---: | ---: | ---: |
| source curve evaluation | 752.388 | 18303537 | 65.056 | 33.796 |
| refinement error checks | 481.696 | 15647220 | 42.075 | 21.637 |
| key count and store | 60.601 | 2604000 | 5.172 | 2.722 |
| clip validation binding | 30.487 | 23 | 2.564 | 1.369 |
| key allocation relocation | 22.100 | 22415 | 1.699 | 0.993 |
| timeline temporary buffer | 1.451 | 991 | 0.112 | 0.065 |
| track binding | 0.598 | 1198 | 0.043 | 0.027 |
| track storage | 0.323 | 1198 | 0.030 | 0.015 |
| model skeleton and track group lookup (ganzer A1-Start) | 0.118 | 82 | n/a | n/a |
| cache memory count and insertion (ganzer A1-Start) | 0.036 | 28 | n/a | n/a |
| clip cache lookup (ganzer A1-Start) | 0.035 | 87 | n/a | n/a |

**Other / Scope- und Timer-Overhead / Actor-Auswahl / nicht einzeln getimte Arbeit:** Rest **876.609 ms** im diagnostischen Prewarm, inklusive nicht aus den Stage-Timern separierbarer Messkosten. Diese Restzeit ist keine belegte Optimierungsreserve. Globale Cache-/Lookup-Timer oben umfassen auch initiales Actor-Binding; sie werden nicht nochmals von diesem Prewarm-Rest abgezogen.

Vollständige Zuordnung der angeforderten A–R-Blöcke:

| Block | Befund / Messung |
| --- | --- |
| A Clip lookup | eigener Cache-Lookup-Timer und Hit/Miss-Zähler; keine Suche pro Key |
| B Skeleton lookup | `model skeleton and track group lookup`; ownerseitiger Handle-/Gruppencheck vor BoundClip |
| C Track → Bone | 1198 Zuordnungen im Before-Prewarm, 0,598 ms; pro Bone/Track, nicht pro Key |
| D Key count | inkrementelles Zählen plus Budgetcheck im `key count and store`-Timer; kein separater Vollpass |
| E Key allocation | `key allocation relocation`; 22.415 Key-Vektorvergrößerungen im Before-Prewarm |
| F Key generation | adaptive Probeauswertung + Fehlerprüfung + Keyablage, oben getrennt |
| G Key conversion | `source curve evaluation` enthält B-Spline-Basis, float-Konvertierung und Quaternion-Normalisierung; kein zusätzlicher Kopier-/Konvertierungspass |
| H Sorting | **0** Runtime-Sort-Aufrufe: Rekursion emittiert monoton; Source-Sortiertheitsprüfung geschieht beim Parse |
| I Sampling/index table | **0** zusätzliche Tabellen; Runtime verwendet direkt `upper_bound` auf Keys |
| J Interpolation metadata | konstantes Linear-Metadatum je Kanal; in Clip-Validierung/Trackaufbau enthalten, kein großer Extra-Pass |
| K Duration/timeline | eigener Timer für Source-Knotenzeit-Vektor; keine feste Raster-/Frame-Schleife |
| L Duplicate/static tracks | Statistik pro Quellkanal; vorhandener <=1-Knoten-Pfad emittiert bereits nur einen Key |
| M Temporary buffers | Knotzeit-Vektoren und temporär gehaltene alte Vektorspeicher bei Wachstum erfasst |
| N Alloc/realloc | dynamische Key-, Track- und Zeitvektoren sowie Clip-Objekt gezählt; Grenzen unten |
| O Copies/memcpy | Vektor-Relocation-Bytes gezählt und in Allocation/Track/Timeline-Zeiten enthalten; vollständige Keyarrays werden beim Clip-Publish verschoben, nicht kopiert |
| P Cache insertion | eigener `cache memory count and insertion`-Timer; unveränderter Documentcache |
| Q Synchronization/locks | **0** neue Runtime-Preparation-Locks/Worker-Waits; Bindung läuft weiter synchron auf dem Owner |
| R Other | separat ausgewiesener Rest oben; keine versteckte Zeitgutschrift |

## RUNTIME DATA / MEMORY

Für einen fairen Mengenvergleich umfasst diese Tabelle **alle 28 Bindungen des kalten A1-Starts**, da initiales Idle und Prewarm ineinander übergehen. Es gibt keine Mengenreduktion durch den Fix.

| Größe | Before | After |
| --- | ---: | ---: |
| Gebundene Tracks / Bone-Referenzen | 1460 | 1460 |
| Source-Knoten der referenzierten Tracks | 43741 | 43741 |
| Runtime-Keys | 2817672 | 2817672 |
| Runtime-Clip-/Track-/Key-Kapazitätsbytes | 83651352 | 83651352 |
| Gezählte Allokationsanforderungen | 38087 | 38087 |
| Davon Vektor-Reallokationen | 32472 | 32472 |
| Kumulierte angeforderte Kapazitätsbytes | 252203608 | 252203608 |
| Kumulierte temporäre Timeline-Kapazitätsbytes | 1314224 | 1314224 |
| Relokierte Elementbytes | 168116640 | 168116640 |
| Source-Kurvenauswertungen | 19615700 | 11706976 |
| Peak temporäre Heap-Kapazität | 292440 | 292440 |

Runtime-Array-/Clip-Speicher **83.651.352 Bytes / 79.776 MiB**, unverändert. Gezählt werden tatsächliche Vektorkapazitäten und `sizeof` der Clip-/Trackobjekte. Dies ist **keine Prozess-RSS-Messung**: Allocatorheader/Shared-Pointer-Controlblock, Namens-String-Heap und das Validierungs-Bitset sind nicht enthalten; ebenso nicht die separat gehaltenen GR2-Quelldokumente. Allokationscounts sind die instrumentierten Strukturen, kein globaler Heap-Hook. Gleiche IR und gleicher Vorbereitungsumfang schließen ein Entfernen von Quelldaten als Gewinnquelle aus.

Peak temporary ist das Maximum gleichzeitig lebender Timeline-Kapazität und alter Kapazität während einer Key-Reallokation; kein aufsummierter Peak über Clips. Der Fix braucht zusätzlich ein festes Array aus drei Proben pro Rekursionsebene: höchstens **2040 Bytes Probe-Nutzdaten auf dem Stack** bei Dimension 9 und maximal 17 aktiven Ebenen, zuzüglich compilerabhängigem Frame-Overhead. Kein zusätzlicher Heap, keine zusätzliche dauerhafte Runtime-Memory.

## DEATH ANALYSIS – 225.456 Keys

Datei `d:/ymir work/pc/warrior/general/dead.gr2`, Boundary 0. Source **75 Transform-Tracks, 3709 Source-Knoten insgesamt**; davon **74 tatsächlich an Bones gebundene Tracks mit 3582 Knoten**. Der zusätzliche ungebundene Track wird unverändert nicht als Skelettkanal abgetastet. Dauer **2.966666936 s**, Metadatum TimeStep **0,033333335 s** (~30 Hz Autorentakt).

Runtime **225.456 Keys**, Expansion **60.786×** gegenüber allen Source-Knoten bzw. **62.941×** gegenüber den gebundenen Source-Knoten. Aufteilung: **17.351 Translation, 208.031 Rotation, 74 Scale/Shear**. Durchschnitt **75996.4 Keys/s über alle Kanäle**, **3046.7 Keys/Track**; dies ist ausdrücklich **keine feste Samplerate**.

Root cause der Expansion: Source enthält kompakte GR2-B-Spline-Kurven mit float-Knoten/Controls; Runtime speichert lineare Zeit/Wert-Keys. `ConvertCurve` prüft jedes Knotenintervall bei 1/4, 1/2, 3/4 gegen die Runtime-Interpolation und halbiert adaptiv. Toleranzen bleiben Translation **1e-5**, Rotation/Scale **5e-7** einschließlich bestehender float-Rundungskorrektur, maximale Tiefe 16. Für Death wird Tiefe **9** erreicht. 221.801 innere Splits / 225.234 Blattintervalle erklären die hohe Ausgabe, überwiegend Rotation. Es wird weder pro Frame vorab gerastert noch pro Bone die ganze Animation kopiert.

Death hat **23 vollständig statische gebundene Tracks**, konstante/Identity-Kanäle Translation **68/74**, Rotation **23/74**, Scale **74/74**. 73 Identity-Kanäle + 92 explizit konstante Kanäle = 165 mathematisch konstante Kanäle; dieselben 165 werden bereits im Ein-Key-Pfad behandelt. Kein zusätzlicher Mehrknoten-Konstantfall wurde in diesem Clip gefunden. Statische Scale-Tracks verursachen damit 74, nicht hunderttausende Keys.

Die teure Redundanz liegt **innerhalb** der Key-Erzeugung: ein Parent hat die Mitte bereits als Prüfprobe berechnet, berechnet sie bei Split aber erneut; seine Viertelproben werden danach als Kind-Mittelpunkte wieder berechnet. Vorher Death **1.569.994**, nachher **988.506 Source-Auswertungen**: **581.488 identische Auswertungen entfallen**. Sämtliche Fehlerprüfungen und 225.456 Ausgabekeys bleiben erhalten.

## Source vs. Runtime – repräsentative Clips

Source-Repräsentation im nativen GR2-Reader: eigene float-Knotenzeit-/Control-Arrays, Grad 0–2. Der Reader unterstützt `legacy-f32` und GR2-Format 1; eine separate Häufigkeit dieser beiden Containerlayouts wird hier nicht behauptet. Runtime: `Track<array<float,N>>`, Zeit als double, Werte als float, unveränderte lineare Interpolationsmetadaten (Quaternion-Normalisierung/Nlerp im vorhandenen Sampler). Tabellenzeit ist Clipdauer, Keys sind einzelne Kanalknoten, nicht ganze Skelettposen.

| Motion, Warrior | Source tracks gesamt / gebunden | Source keys gesamt / gebunden | Runtime tracks | Runtime keys | Dauer s | Keys/s | Keys/Track |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Idle | 76 / 74 | 2807 / 2787 | 74 | 32601 | 2.000000 | 16300.5 | 440.6 |
| Walk | 76 / 74 | 1568 / 1502 | 74 | 71668 | 0.800000 | 89585.0 | 968.5 |
| Run | 76 / 74 | 873 / 839 | 74 | 88204 | 0.666667 | 132306.0 | 1191.9 |
| Attack | 76 / 74 | 1635 / 1569 | 74 | 132257 | 1.000000 | 132257.0 | 1787.3 |
| Damage | 76 / 74 | 869 / 835 | 74 | 69454 | 0.533333 | 130226.2 | 938.6 |
| Death | 75 / 74 | 3709 / 3582 | 74 | 225456 | 2.966667 | 75996.4 | 3046.7 |

## TOP 30 CLIPS nach Runtime-Keys

Es gibt nur **28** tatsächlich benötigte neue Bindungen in diesem A1-Start; alle 28 werden gezeigt. Kein Auffüllen mit nicht vorbereiteten Assets. Pfade sind unter `d:/ymir work/`, Boundary 0 = Once, 3 = Loop. Werte Before/After identisch.

| # | Clip | Boundary | Runtime Keys | Runtime Bytes | Keys/s | Keys/Track |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| 1 | `pc/warrior/general/dead.gr2` | 0 | 225456 | 6800608 | 75996.4 | 3046.7 |
| 2 | `monster/stray_dog/00_2.gr2` | 3 | 192164 | 5629784 | 52408.4 | 5056.9 |
| 3 | `monster/stray_dog/20_1.gr2` | 0 | 184167 | 5396888 | 138125.2 | 4846.5 |
| 4 | `pc/warrior/onehand_sword/combo_04.gr2` | 0 | 158734 | 4635112 | 125316.3 | 2145.1 |
| 5 | `monster/stray_dog/20.gr2` | 0 | 152627 | 4487336 | 163528.9 | 4016.5 |
| 6 | `pc/warrior/onehand_sword/combo_01.gr2` | 0 | 132257 | 4010560 | 132257.0 | 1787.3 |
| 7 | `monster/stray_dog/37.gr2` | 0 | 131681 | 4074656 | 112869.4 | 3465.3 |
| 8 | `monster/stray_dog/31.gr2` | 0 | 130504 | 3924128 | 156604.8 | 3434.3 |
| 9 | `pc/warrior/onehand_sword/combo_02.gr2` | 0 | 130087 | 3800248 | 139378.9 | 1757.9 |
| 10 | `pc/warrior/onehand_sword/combo_03.gr2` | 0 | 124244 | 3650080 | 116478.7 | 1679.0 |
| 11 | `monster/stray_dog/35.gr2` | 0 | 118275 | 3369176 | 101378.6 | 3112.5 |
| 12 | `monster/stray_dog/32.gr2` | 0 | 116807 | 3433880 | 100120.3 | 3073.9 |
| 13 | `pc/warrior/onehand_sword/run.gr2` | 3 | 88204 | 2570656 | 132306.0 | 1191.9 |
| 14 | `monster/stray_dog/34_1.gr2` | 0 | 88017 | 2662520 | 105620.4 | 2316.2 |
| 15 | `monster/stray_dog/37_1.gr2` | 0 | 87298 | 2615000 | 109122.5 | 2297.3 |
| 16 | `monster/stray_dog/03.gr2` | 3 | 85677 | 2564912 | 171354.0 | 2254.7 |
| 17 | `monster/stray_dog/33.gr2` | 0 | 76532 | 2205632 | 76532.0 | 2014.0 |
| 18 | `pc/warrior/onehand_sword/damage_1.gr2` | 0 | 75989 | 2317864 | 142479.4 | 1026.9 |
| 19 | `pc/warrior/onehand_sword/walk.gr2` | 3 | 71668 | 2070232 | 89585.0 | 968.5 |
| 20 | `pc/warrior/onehand_sword/damage.gr2` | 0 | 69454 | 2108944 | 130226.2 | 938.6 |
| 21 | `monster/stray_dog/30.gr2` | 0 | 64675 | 1910432 | 77610.0 | 1702.0 |
| 22 | `monster/stray_dog/34.gr2` | 0 | 61945 | 1851704 | 74334.0 | 1630.1 |
| 23 | `monster/stray_dog/00.gr2` | 3 | 61547 | 1889720 | 26377.3 | 1619.7 |
| 24 | `monster/stray_dog/02.gr2` | 3 | 59953 | 1787480 | 81754.1 | 1577.7 |
| 25 | `monster/stray_dog/00_1.gr2` | 3 | 38512 | 1168832 | 14442.0 | 1013.5 |
| 26 | `pc/warrior/onehand_sword/wait.gr2` | 3 | 32601 | 971656 | 16300.5 | 440.6 |
| 27 | `npc/goods/wait.gr2` | 3 | 31114 | 910016 | 9334.2 | 818.8 |
| 28 | `pc/warrior/onehand_sword/wait_1.gr2` | 3 | 27483 | 833296 | 20612.2 | 371.4 |

## SELECTED FIX / Alternativen / Thread-Safety

**Ein Fix: Wiederverwendung bereits geprüfter dyadischer Kurvenproben.** Drei lokale Probe-Records halten Zeit + Samplewert. Beim Split wird die vorhandene Mittelprobe als Intervallgrenze weitergereicht; Viertelproben werden den Kindern als bekannte Mittelproben angeboten. Wiederverwendung nur bei **exakt gleicher float-Zeit**. Bei abweichender Rundung wird normal neu ausgewertet. Keine Änderung an Unterteilungsentscheidung, Anzahl/Reihenfolge der drei Fehlerprüfungen, Tiefe, Toleranz, Knotengrenze, Fehlerablehnung oder Key-Publish.

Alle 28 Bindungen zusammen: **19.615.700 → 11.706.976 Source-Auswertungen**, **7.908.724 / 40.318 %** weniger. Der Diagnose-Gate prüft pro Clip: alte Samplecalls − neue Samplecalls = gemeldete Wiederverwendungen. Key-/Split-/Blatt-/Fehlerprüfungszahlen, Laufzeiten, IDs und Speichergrößen bleiben gleich.

Nicht gewählt: konstante Tracks sind schon kompakt; Reserve/Allocation und Bone-Lookups sind nicht der größte Kostenblock; Sort-/Index-Tabellenarbeit existiert nicht; Cache-Sharing funktioniert. Eine kompakte Source-Spline-Auswertung direkt in jedem Runtime-Frame wäre eine andere Repräsentation mit neuem Sampler-/Semantiknachweis. Sie wurde nicht gebaut. Keine Toleranzlockerung oder Keyreduktion.

BindAnimation rechnet überwiegend mit lokalem Scratch und unveränderlichen Source-/Skeletondaten. Actor-/Resource-/Documentcache und optionaler AnimationStallAudit bleiben ownerseitig; ihre gemeinsame Nutzung wäre zusätzliche Thread-Safety-Arbeit. Weil ein algorithmischer Redundanz-Fix belegt ist, wird **keine zweite Parallelisierung** gewählt. Der vorhandene L7-Workerpool bleibt ausschließlich für GR2-Dateivorbereitung zuständig.

## FIRST USE AFTER

Einzelner maximaler Runtime-Motion-Aufruf innerhalb des ersten Beobachtungsfensters; umfasst nativen Kontrollwechsel und Lookup, nicht den ~18-ms-Frame-Pacing-Takt. Alle drei Einsätze desselben Clips bleiben ohne neue Bindung/Keys/GR2-I/O.

| Actor | Motion | First Use runtime ms | Runtime preparation ms | Max Gesamtframe ms |
| --- | --- | ---: | ---: | ---: |
| player | Attack | 0.0070 | 0.0022 | 18.022 |
| player | Damage | 0.0063 | 0.0020 | 18.032 |
| player | Death | 0.0068 | 0.0021 | 18.500 |
| mob | Attack | 0.0059 | 0.0019 | 18.184 |
| mob | Damage | 0.0068 | 0.0020 | 18.100 |
| mob | Death | 0.0065 | 0.0019 | 18.211 |

A1 Warm nach dem Mapwechsel: **166.905 ms**; B1 **271.876 ms**. Warme Prewarm-Aufrufe erzeugen keine zusätzlichen Clipdaten. Normale Sounds/Staub können kleine eigene Loads auslösen; keine späten GR2-Assetloads und keine Rückkehr der 50–80-ms-Animation-Spitzen.

## CORRECTNESS / FAST GATE

- **Release Build PASS**, exakt finaler C++-Baum; keine Debug-/GCC-/Gesamtsuite. Die letzte Änderung danach betrifft nur Test-Harness-Auswahl/Analyse und Dokumentation.
- **903 GR2-Dateien / 7224 Sections / 60.042.028 Bytes bytegleich**, vollständige relevante IR identisch; 903 halbierte Inputs weiterhin abgelehnt.
- **13 reale Modell-/Clip-Paare**, Keys/Zeiten/Dauer/Tracks/Bone-Bindings/Interpolation bitgleich für alle vier Boundary-Slots gegen `GR2AnimationPreparationReference.h`. Diese eingefrorene Testreferenz enthält den Vorher-Algorithmus und wird nicht in den Client gelinkt.
- **234 bestehende Runtime-Posevergleiche PASS**, zusätzlich **468 Vorher/Nachher-Sampler-/World-Transform-/Palette-Vergleiche PASS**. Alle Warm-First-Use-Bindungen ohne weitere Konvertierung. Kein sichtbarer Unterschied aus einem ungemessenen manuellen Test behauptet.
- **7/7 gezielte Tests PASS**, einschließlich Warmup-Allokation/Lebensdauer, Golden static/animation, Safety, Compatibility, Preparation, Independence.
- **A1 → B1 → A1, 36 First/Second/Third Uses, Exit 0 PASS**. Diligent ERROR/FATAL = **0**, GPU fallback = **0**, CPU deformation = **0**, Shutdown-Ressourcen = **0**, NativePrewarmFailures/Limited = **0**.
- Native Before/After-Mapdaten, Requests/Hits, Section-CRCs/Bytes, Registrierungen, Kopierbytes und Shaderbytecode identisch. Kein ausgelassener Clip; gleiche 28 echte Bindungen. Keine beobachteten Race-/Deadlocksymptome. Kein ThreadSanitizer-Nachweis behauptet.
- Gefüllter Shadercache unverändert, kein Shader-Miss/Runtime-Compile. Originalclient-EXE/Rootpack, Area-Parallelismus und SSE2-Decoder bleiben geschützt. Kein Deployment, kein Netzwerklogin, keine Visual-Galerie, kein weiterer Performancebereich verändert.

**FAST GATE PASS. FINAL GO:** vollständiger Prewarm nachweisbar günstiger, A1 Production Cold klar schneller als beide seriellen Referenzen, L7-Parallelisierung und stallfreier First Use erhalten, alle Korrektheits-/Ressourcenprüfungen bestanden. Kein zweiter Runtime-Fix wird begonnen.

## Evidenz / Reproduktion / Commit

`build-p0l8/build-release.log`, `corpus-runtime.log`, `focused-tests.log`, `start.diff`, `start-hashes.json`; pro Lauf unter `build-p0l/<Name>/` native Trace, `first-use-analysis.json`, `runtime-preparation.json` (nur Profile), `fast-gate.json`, Exit/Audit und Probe-Hashes. `l8-before-detail` hatte keine aktivierten Detailzähler und wird als Diagnose verworfen, nicht als PASS umgedeutet. Die gültigen Detaildaten stammen ausschließlich aus `l8-before-profile` / `l8-after-profile`.

`prepare_probe.ps1 -ShaderLifecycle -AnimationFirstUse -LoadPrewarm` für Zeitläufe; zusätzlich `-RuntimePreparationDetails` nur für Mengendiagnose. Der serielle vollständige Kontrollpfad nutzt `-SerialAnimationLoading` **und die gesicherte Vorher-EXE**. Ressourcen-/Daten-Gate: `verify_runtime_preparation.py <before> <after> <profile-before> <profile-after>`. Frische Namen erforderlich; keine bestehenden Beweise überschreiben.

| Lauf | Release-EXE SHA256 |
| --- | --- |
| `l8-serial` | `54DABF9B24CBCECFBE97D52C04A5DF1405C29E735F35D62569327363367F949B` |
| `l8-serial-complete` | `7AA34075F7633E052DDE83EF4368F0A5EFF6B5FB88A216C05B5A412E3637A2CE` |
| `l8-before` | `2E6B32A29F909B07990143C7E140D5963FB27A7F9229D0CCE125E2C5070C4C67` |
| `l8-after` | `A82536CC3132341E749B871E922CBC726963A7549D3563A38DBD86CD860E1D61` |
| `l8-before-profile` | `7AA34075F7633E052DDE83EF4368F0A5EFF6B5FB88A216C05B5A412E3637A2CE` |
| `l8-after-profile` | `A82536CC3132341E749B871E922CBC726963A7549D3563A38DBD86CD860E1D61` |

Commit erst nach diesem GO, nur Source/Tests/Doku von L7/L7C/L8. Die Python-Ladegrenze gehört zum separaten Runtime-Source-Repository und benötigt dort einen eigenen Commit. Keine Logs, Traces, Builds, Cachedateien, Testclients oder Backup-EXEs stagen. Die beiden Abschluss-Hashes werden im Abschlussbericht der Task angegeben; keine Commit-Selbstreferenz in diesem Dokument. Kein Push. **STOP nach P0-L8.**
