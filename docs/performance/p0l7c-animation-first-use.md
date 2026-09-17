# P0-L7C – Animation First-Use Stall Closure

Historischer L7C-Zwischenstand. Die Runtime-Vorbereitung wurde anschließend mit genau einem Fix in [P0-L8](p0l8-animation-runtime-prewarm.md) beschleunigt; gemeinsamer L7/L7C/L8-Abschluss dort: FINAL GO.

17.09.2026: **First-Use-Closure PASS; FINAL NO-GO wegen Ladezeit-Trade-off. Kein Staging, kein Commit, kein Push. HEAD bleibt 873d989.**

ROOT CAUSE: `Document::BoundClip` erstellt beim ersten Playback noch die immutable `RuntimeAnimationClip`-Keys für Clip/Skeleton/Trackgruppe/Loop-Rand. `ReadAnimation` beim Map Load hat lediglich Quellkurven/Knoten/Controls geparst. Die teure adaptive Spline-Konvertierung liegt in `GR2::BindAnimation` → `ConvertCurve`; neue Datei-Decodes sind nicht die Ursache. Player Death erzeugt **225.456 Keys**, mit **71,816 ms** Runtime-Vorbereitung im instrumentierten P0-L7-Before. Das erklärt den zuvor beobachteten 79,3-ms-Frame derselben Phase; es ist keine nachträgliche exakte Zerlegung des historischen Einzel-Frames.

SELECTED FIX: Genau eine funktionale Änderung erweitert die bestehende lokale Motion-Auswahl in `CActorInstance::PrewarmMotions` um **Damage und Death**. Sie erzeugt ihre realen Runtime-Strukturen über `PrepareGR2Animation`, einschließlich tatsächlichem GENERAL-Fallback, im bestehenden Loading-Prewarm. Kein künstliches Playback, keine weiteren Animationsgruppen, keine Kurven-/Qualitätsänderung, kein zusätzlicher Pool, keine Änderung an P0-L7-Parallelisierung oder Cache-Lebensdauer.

FIRST USE AFTER: Alle 36 Einsätze (2 Actors × 6 Motions × 3 Wiederholungen) haben **0 neue Runtime-Keys, 0 Clip-Misses, 0 gemessene Key-Vektorvergrößerungen und 0 späte GR2-Reads/Parses**. Maximaler einzelner `Instance::SetMotion` **0.0114 ms**. Gesamter Hintergrund-/Actor-/Effekt-Update maximal **0.2223 ms**. Die normalen Probe-Frameabstände von rund 18–19 ms enthalten Frame-Pacing; sie sind nicht einmalige Animationskosten.

A1 COLD FINAL: **2121.153 ms**, gegenüber dem veröffentlichten P0-L7-Wert **1337.390 ms** also **+783.764 ms**. Gegenüber der ursprünglichen seriellen Messung 2228.658 ms verbleiben rechnerisch nur **107.505 ms / 12.1 % des damaligen Gewinns**. Diese historischen Werte hatten allerdings den produktiven Prewarm ausgelassen. Darum weder behaupten, der kleine Closure-Fix koste allein 784 ms, noch daraus einen automatischen GO ableiten.

## Messumfang und korrigierte Ladegrenze

Der alte Offline-Probe und der rohe First-Use-Vergleich rufen `PrewarmVisibleActors` nicht auf. Im Produkt tut `PythonNetworkStream::PrepareGamePhase` dies innerhalb eines aktiven GPU-Frames bei noch sichtbarem LoadingWindow. Die bisherige lokale Auswahl deckt Idle/Walk/Run/Attack/Combos ab, aber **nicht Damage/Death**; umgebende Actors haben bereits eine breitere Common-Motion-Auswahl. Ein gezielter zusätzlicher Before-Lauf mit der bestehenden produktiven Grenze bestätigt: Attack und die Mob-Motions sind dort schon warm; Player Damage/Death bleiben kalt.

Der korrigierte After-Probe ruft denselben bestehenden `chrmgr.PrewarmVisibleActors(True)` vor dem World-Render auf. Er führt kein Prewarm durch Playback ein. Die zusätzliche Vorbereitung ist im A1-Cold-Timer enthalten, nicht außerhalb der Messgrenze versteckt. Die native Ladevorbereitung selbst dauert Before **701,909 ms**, After **832,766 ms**. Der Differenzbetrag **130,857 ms** betrifft die gezielte Erweiterung plus Run-to-Run-Streuung, nicht den gesamten zuvor fehlenden Block.

Private Release-Testclients mit Originalpacks, unverändert gefülltem Shadercache und privatem Root-Pack. Cold bedeutet frischer Prozess; der OS-Dateicache ist nicht kontrolliert. Jede Probe: A1 Cold → B1 → A1 Warm → Messungen → Exit, jeweils etwa 20 s. Keine Benchmarkschleife, kein Netzwerklogin, keine vollständige Suite, keine manuelle visuelle Abnahme behauptet. Jede Motion wird im vollständig geladenen letzten A1 dreimal unmittelbar nacheinander gestartet, ohne Instance-/Assetcache-Reset. Ein 0,35-s-Fenster misst den Beginn; die gesamte Clip-Zeitachse wird zusätzlich durch den Posevergleich abgedeckt.

Die real registrierten Zufallsvarianten werden nur im Test auf Subindex 0 gepinnt. Native erfolgreiche `animation-use`-Events und Race/Motion-Registrierungen beweisen für **alle Läufe** denselben Clip pro Actor/Motion/Wiederholung. Idle ist durch das normale Erzeugen der Actors schon gebunden; seine „First Use“ ist der erste explizite Testeinsatz, keine künstlich kalte Idle-Bindung.

## Serielle Referenz gegen P0-L7

**P0-L7 REGRESSION: no – keine verursachte oder systematische Verschlechterung des First-Use-Stalls nachgewiesen.** Der serielle HEAD-873d989-Referenzclient zeigt dieselbe `GR2 animation preparation`-Phase: Player Attack 42,493 ms und Death 70,100 ms; P0-L7 42,686 / 71,816 ms. Wiederholungen liegen nahe null. Ein zweiter unvorbereiteter Kontrolllauf mit finaler Diagnose zeigt Attack 41,639 / Death 72,021 ms und Mob Attack 45,569 ms gegenüber 47,946 ms seriell. Der erste instrumentierte Mob-Attack-Wert 53,925 ms ist somit kein stabiler Parallelisierungsaufschlag. Das sind begrenzte Einzelmessungen mit Diagnosekosten, kein statistischer Nichtunterlegenheitsnachweis.

Der serielle Referenz-Build ist die gesicherte L7-Before-EXE von HEAD 873d989 mit damaliger grober Trace-Diagnostik; sie besitzt keine feinen L7C-Untertimer. Im privaten Python-Source greift dort der vorhandene `hasattr`-Kompatibilitätspfad. Die Split-Timer messen ausschließlich `first-use-*`-Fenster; keine neue allgemeine Profiling-Infrastruktur.

## First / Second / Third Use

Je Feld: **First / Second / Third**, Millisekunden inklusive nativer `GR2 animation preparation` (Cache-Lookup + ggf. Konvertierung). Kein Python-Sleep/Render/Pacing enthalten. 0,0000 ist die Trace-Auflösung bzw. kein Dokument-Lookup beim unverändert laufenden Idle.

| Actor | Motion | Seriell HEAD | P0-L7 roh | Closure, produktiver Prewarm |
| --- | --- | ---: | ---: | ---: |
| player | Idle | 0.0007 / 0.0000 / 0.0000 | 0.0025 / 0.0000 / 0.0000 | 0.0030 / 0.0000 / 0.0000 |
| player | Walk | 23.5361 / 0.0003 / 0.0003 | 23.2909 / 0.0028 / 0.0019 | 0.0023 / 0.0026 / 0.0020 |
| player | Run | 29.5823 / 0.0005 / 0.0009 | 29.4809 / 0.0016 / 0.0059 | 0.0023 / 0.0018 / 0.0060 |
| player | Attack | 42.4926 / 0.0006 / 0.0003 | 42.6859 / 0.0017 / 0.0019 | 0.0032 / 0.0020 / 0.0020 |
| player | Damage | 21.8061 / 0.0005 / 0.0004 | 22.2489 / 0.0018 / 0.0023 | 0.0022 / 0.0016 / 0.0021 |
| player | Death | 70.0997 / 0.0005 / 0.0003 | 71.8162 / 0.0021 / 0.0032 | 0.0024 / 0.0023 / 0.0022 |
| mob | Idle | 0.0000 / 0.0000 / 0.0000 | 0.0000 / 0.0000 / 0.0000 | 0.0000 / 0.0000 / 0.0000 |
| mob | Walk | 19.1590 / 0.0006 / 0.0003 | 19.8439 / 0.0033 / 0.0019 | 0.0020 / 0.0021 / 0.0020 |
| mob | Run | 27.0143 / 0.0005 / 0.0003 | 28.2895 / 0.0017 / 0.0020 | 0.0022 / 0.0021 / 0.0025 |
| mob | Attack | 47.9455 / 0.0008 / 0.0005 | 53.9247 / 0.0020 / 0.0018 | 0.0024 / 0.0020 / 0.0016 |
| mob | Damage | 19.3418 / 0.0004 / 0.0005 | 20.0227 / 0.0024 / 0.0018 | 0.0018 / 0.0020 / 0.0016 |
| mob | Death | 38.8460 / 0.0003 / 0.0003 | 40.1919 / 0.0023 / 0.0017 | 0.0018 / 0.0017 / 0.0016 |

## Frames und exklusive Animationskosten

Framewerte sind das Maximum des gesamten Beobachtungsfensters; `SetMotion max` umfasst den nativen einzelnen Kontrollwechsel einschließlich Cache-Lookups, ohne normale Render-/Posearbeit. Diese Trennung verhindert, den ungefähr 18-ms-Takt des Harness als First-Use-Stall zu bewerten.

| Actor | Motion | Frame Before ms | Frame After ms | SetMotion max After ms | Update max After ms |
| --- | --- | ---: | ---: | ---: | ---: |
| player | Idle | 19.180 | 18.179 | 0.0103 | 0.0680 |
| player | Walk | 23.449 | 18.707 | 0.0078 | 0.2223 |
| player | Run | 29.614 | 18.627 | 0.0072 | 0.0782 |
| player | Attack | 42.904 | 18.027 | 0.0114 | 0.1018 |
| player | Damage | 22.366 | 18.045 | 0.0070 | 0.1013 |
| player | Death | 71.995 | 18.192 | 0.0097 | 0.1154 |
| mob | Idle | 18.042 | 18.617 | 0.0033 | 0.0544 |
| mob | Walk | 19.962 | 19.566 | 0.0071 | 0.0867 |
| mob | Run | 28.419 | 18.262 | 0.0072 | 0.0668 |
| mob | Attack | 54.135 | 17.992 | 0.0073 | 0.1718 |
| mob | Damage | 20.172 | 18.072 | 0.0069 | 0.1121 |
| mob | Death | 40.348 | 18.050 | 0.0070 | 0.1250 |

## Spike-Zerlegung

Letzte unvorbereitete Kontrolle mit **derselben finalen EXE** wie After, nur ohne Lade-Prewarm: Player Death Frame **72.1880 ms**, native Runtime-Vorbereitung **72.0212 ms**. Die funktionale Änderung greift ausschließlich in der Ladevorbereitung. Dieser Lauf dient der getrennten Runtime-/Skeleton-Lookup-Diagnose und behauptet keinen Produktfehler trotz ausgeführtem Prewarm.

| Stufe | Inklusive ms im 0,35-s-Fenster | Exklusive ms | Max. einzelner Aufruf ms | Aufrufe |
| --- | ---: | ---: | ---: | ---: |
| actor animation cache lookup | 0.0025 | 0.0025 | 0.0012 | 8 |
| instance animation cache lookup | 0.0005 | 0.0005 | 0.0004 | 2 |
| clip and skeleton cache lookup | 0.0004 | 0.0004 | 0.0004 | 1 |
| AnimationRuntime lookup | 0.0017 | 0.0017 | 0.0007 | 3 |
| skeleton and track group lookup | 0.0017 | 0.0017 | 0.0006 | 3 |
| track to bone mapping | 0.0313 | 0.0313 | 0.0012 | 74 |
| spline to runtime keyframes | 69.3778 | 67.7536 | 2.8557 | 74 |
| keyframe allocations and relocation | 1.6242 | 1.6242 | 0.0452 | 1374 |
| clip binding and sampler initialization | 2.5445 | 2.5445 | 2.5445 | 1 |
| pose buffer initialization | 0.0000 | 0.0000 | 0.0000 | 0 |
| pose evaluation | 1.1605 | 0.7288 | 0.0456 | 108 |
| bone matrix generation | 0.4317 | 0.4317 | 0.0175 | 108 |
| GPU skinning binding preparation | 0.0085 | 0.0085 | 0.0002 | 126 |
| GPU skinning palette preparation | 0.0894 | 0.0894 | 0.0034 | 108 |
| attachment update | 0.0178 | 0.0178 | 0.0017 | 66 |
| runtime motion state | 72.0342 | 0.0091 | 72.0282 | 3 |
| Present vsync | 0.7607 | 0.7607 | 0.0549 | 18 |
| other preparation / cache publication | 72.0212 | 0.0672 | 72.0212 | 1 |

- `ConvertCurve` erzeugt 225,456 Keys; die gemessenen **1374 Key-Vektorvergrößerungen** summieren sich auf **20,369,712 angeforderte Kapazitätsbytes**. Das ist eine kumulative Kapazität, kein Peak-RAM-Wert. Diese Timer enthalten Allocate/Relocation sowie geringe Diagnosekosten. Andere temporäre Speicherarbeit (Knotzeit-Vektor, Track-Vektor, Validierungs-Bitset, Shared-Clip) verbleibt im jeweiligen Elternscope; kein vollständiger globaler Allocator-Trace wird behauptet.
- Eine separate Pose-Sampler-Initialisierung bzw. zusätzliche Index-/Sampling-Tabelle existiert hier **nicht**: `SampleTrack` sucht mit `upper_bound` direkt in den immutable Keys. Der Timer „clip binding and sampler initialization“ misst tatsächlich `RuntimeAnimationClip::Initialize`: validierte Tracks, Keywerte und Skeleton-BindingId. Deshalb kein erfundener separater Sampler-Zeitwert. Posebuffer entstehen beim Instance-Bau; im First-Use-Fenster **0 Aufrufe** ihrer Initialisierung.
- Hierarchie-/Palettenmatrizen sind von Pose-Sampling getrennt gemessen. Pose-/GPU-/Attachment-Werte summieren sich über alle vorhandenen Actors und Frames; sie sind keine zusätzlichen einmaligen Death-Kosten. Inklusive und verschachtelte Unterkosten dürfen nicht addiert werden.
- Late GR2 file access / decompression / parsing: **0 / 0 / 0** in sämtlichen 180 Beobachtungsfenstern. Die Bindung benutzt bereits gehaltene Modell-/Animationsdokumente.
- GPU/resource creation beim kontrollierten ersten Death: **0**. Synchronisierung besteht aus dem separat vorhandenen Present-Timer, kein neuer Worker-/Clip-Wait. Sonstige Render-/Pacing-/Dateikosten bleiben vollständig in `other_window_costs` des JSON erhalten; sie werden nicht der Key-Konvertierung zugerechnet.

## Late File IO und Ressourcen

Es gibt keine späten Zugriffe auf bereits geladene GR2-Animationsassets. Normale Motion-Events laden bei Bedarf Sound bzw. den Fußstaub-Effekt. Diese kleinen, separaten Zugriffe werden nicht verschwiegen oder als Animation-Decode umgedeutet. Beispiel After:

| Actor / Motion / Use | Datei | Logische gelesene Bytes |
| --- | --- | ---: |
| player / Walk / 1 | `d:/ymir work/effect/etc/dust/dust.dds` | 4764 |
| player / Walk / 1 | `sound/common/walk_dirt_n.wav` | 6983 |
| player / Attack / 1 | `sound/common/swing/w_1h_c_1.wav` | 12855 |
| player / Attack / 1 | `sound/pc/warrior/voice/1_soft1.wav` | 17138 |
| player / Damage / 1 | `sound/pc/warrior/voice/damage_1.wav` | 19963 |
| player / Death / 1 | `sound/pc/warrior/voice/dead_1.wav` | 35680 |
| mob / Walk / 1 | `d:/ymir work/effect/etc/dust/dust.dds` | 4764 |
| mob / Attack / 1 | `sound/monster/stray_dog/attack_1.wav` | 31588 |
| mob / Damage / 1 | `sound/monster/stray_dog/damage_1.wav` | 15198 |
| mob / Death / 1 | `sound/monster/stray_dog/dead_1.wav` | 30408 |
| mob / Death / 1 | `sound/monster/stray_dog/drop_2.wav` | 17330 |

Player Walk erzeugt einmal die Staubtextur (`CreateTexture`: **0,0608 ms**); kein Shader/PSO wird dafür kompiliert. Der gesamte Dateizugriff dieses Fensters beträgt **0.0938 ms**, bei zwei Dateien. Dateipfade/Bytes und aggregierte I/O-Dauern je Fenster sind im JSON erhalten; die existierenden I/O-Timer ordnen die Dauer nicht jeder einzelnen Datei zu. Sounds/Effekte sind außerhalb des beauftragten Animations-Fixes geblieben.

## A1 Cold und Ladezeit-Trade-off

| Messung | A1 Cold ms | Bedeutung |
| --- | ---: | --- |
| Historisch vor P0-L7 | 2228.658 | seriell, Offline-Probe ohne produktiven Prewarm |
| Historisch nach P0-L7 | 1337.390 | parallel, derselbe unvollständige Probe |
| Neue serielle Referenz | 2381.792 | identische First-Use-Testabfolge, ohne Prewarm |
| P0-L7 roh, vor Closure | 1309.303 | feine First-Use-Diagnose, ohne Prewarm |
| P0-L7 mit bestehendem produktiven Prewarm | 2012.634 | Attack/Mob warm, lokales Damage/Death noch kalt |
| Closure mit vollständigem Prewarm | 2121.153 | alle angeforderten First Uses warm |
| Finale EXE, unvorbereitete Diagnosekontrolle | 1303.167 | bestätigt erhaltene P0-L7-Ladeparallelisierung |

Der faire Vergleich innerhalb der **gleichen produktiven Ladegrenze** wächst von 2012.634 auf 2121.153 ms, also **108.519 ms**. Ein serieller Gesamtstart mit produktivem Prewarm wurde nicht zusätzlich gemessen; daraus wird kein neuer vollständiger Produkt-Speedup extrapoliert. Die absolute Vorgabe ungefähr 1337 ms ist mit der nun vollständig erfassten Vorbereitung verfehlt. Daher **kein FINAL GO und kein Commit**, obwohl die First-Use-Schließung funktioniert. Keine zweite Optimierung an Binding, Auswahl, Hintergrund-Scheduling oder anderen Ladebereichen begonnen.

P0-L7 bleibt unverändert aktiv: **646 Animations-Dateijobs**, Peak **4**, 693 Animations-Parses vor dem Spiel, unveränderte Requests/Cachehits/Bytes/CRCs/Registrierungen. Finaler B1-Start **270.578 ms**, A1 Warm **164.434 ms**.

## Korrektheit und FAST GATE

- **PASS Release Build** des finalen C++-Baums; keine Debug-/GCC-/Vollsuite. Bestehende Drittanbieter-Linkerhinweise, kein Buildfehler im finalen Build.
- **PASS 903 Dateien / 7224 Sections / 60.042.028 Bytes** seriell vs. parallel bytegleich; komplette relevante IR identisch, einschließlich 217 Modelle, 281 Meshes, 282 Materialien, 217 Skeletons, 729 Bones und 693 Animationen. 903 halbierte Inputs sauber abgelehnt.
- **PASS 13 reale Modell-/Clip-Paare**, alle Runtime-Keys/BindingIds/Zielbones/Dauern/Loop-Ränder identisch, **234 Posen exakt gleich**. Der erweiterte Test bereitet den echten Clip über `PrepareGR2Animation` vor und fordert beim ersten `SetMotion` ausdrücklich **keinen erneuten BindAnimation-Aufruf**. Kein Scheduling-abhängiger ID-Wechsel.
- **PASS 7/7 gezielte GR2-Tests**: Compatibility, Golden static/animation, Preparation, Warmup, Safety, Independence. Warmup prüft immutable Vorbereitung/Playbackzustand, geteilte Dokument-Lebensdauer und allokationsfreie warme Motion-/Poseaufrufe.
- **PASS 36/36 finale Motion-Einsätze**: Idle/Walk/Run/Attack/Damage/Death je Player/Mob und je First/Second/Third. Native Pfadidentität bestätigt; maximale einzelne Runtime-Motion-Kosten deutlich unter 8,33 ms, keine neue Key-Konvertierung.
- **PASS A1 → B1 → A1 und Exit 0**, finale `syserr.txt` leer. Diligent ERROR/FATAL **0**, GPU fallback **0**, CPU deformation **0**, alle geprüften Shutdown-Ressourcen **0**. Derselbe Ressourcen-Gate besteht auch für die vier Vergleichsläufe.
- **PASS native Daten-Invarianten** zwischen allen fünf Läufen für Maploads: gleiche GR2-Requests/Cachehits/Parses, CRCs/Sectionbytes, Modell-/Skeleton-/Animationscounts, Kopierbytes, Registrierungen und Shaderbytecodes. Shader misses/runtime compiles **0**.
- **PASS Erhaltungsprüfung:** sieben geschützte Dateien (einschließlich Original-EXE, Original-root.pck, fremde Doku, Area.cpp und Decoder) und alle 73 gefüllten Shadercache-Dateien hashgleich. Kein Deployment in den Originalclient. Runtime-Python bleibt die vorhandene L7-Änderung.
- **FINAL PERFORMANCE GATE: NO-GO.** Der vollständige Ladepreis bleibt deutlich außerhalb des beauftragten P0-L7-Zielbereichs. Dieser Blocker wird nicht durch den bestanden funktionalen First-Use-/Ressourcen-Gate ersetzt.

## Reproduzierbare lokale Evidenz

Build/Test: `build-p0l7c/build-release.log`, `corpus-runtime.log`, `focused-tests.log`, `preservation-after.json`, `shader-after.json`. Anfangsstand gesichert in `p0l7-start.diff` und `p0l7-start-hashes.json`.

Je Probe unter `build-p0l/<Name>/`: `map-load-trace.tsv`, `animation-first-use.json`, `first-use-analysis.json`, `fast-gate.json`, `exit.json`, `source-resource-audit.log`, `p0l-smoke.log`, private EXE und Pack-Manifest. Diese Build-/Trace-/Cache-/Testclient-Dateien bleiben ungestagt.

| Probe | Release-EXE SHA256 |
| --- | --- |
| `l7c-reference` | `54DABF9B24CBCECFBE97D52C04A5DF1405C29E735F35D62569327363367F949B` |
| `l7c-before` | `1ABC04909B086A3C152B479B9F764BADD3EFFD9B840115B3D49DE626EDFAFB12` |
| `l7c-before-prewarm` | `1ABC04909B086A3C152B479B9F764BADD3EFFD9B840115B3D49DE626EDFAFB12` |
| `l7c-after` | `746B9F3D3ADBE184EF1BE7C61794C678DFC8084A25EAF6073B77A8BA1B756055` |
| `l7c-unprepared` | `746B9F3D3ADBE184EF1BE7C61794C678DFC8084A25EAF6073B77A8BA1B756055` |

Analyse: `python tests/Loading/analyze_first_use.py build-p0l/l7c-after --expect-prepared`; Ressourcen-Gate: `python tests/Loading/verify_probe.py build-p0l/l7c-after --shader-lifecycle --animation-first-use`. Vorbereitung über `prepare_probe.ps1 -ShaderLifecycle -AnimationFirstUse -LoadPrewarm`; Kontrolllauf lässt nur `-LoadPrewarm` weg. Alle CLI-Evidenznamen müssen für native Wiederholung frisch sein.

## Git / STOP

Source-Branch `codex/g56-hdr-atmosphere`, HEAD **873d989**; Runtime-Branch `codex/g56-hdr-colors`, HEAD **548c43b4**. Keine Änderungen zurückgesetzt, keine fremden Dateien angefasst, Index leer. **Commit-Hash: keiner**, weil FINAL GO nicht erreicht ist. Kein Push. STOP nach dieser einen Closure und ihrer Dokumentation; keine weitere Ladeoptimierung/P1.
