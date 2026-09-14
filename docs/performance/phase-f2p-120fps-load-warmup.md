# F2-P – 120-FPS Load/Warmup Performance Pass

Stand: 2026-09-14. Quick Fix abgeschlossen: Spawn-Fix und korrigierte Namensfarben manuell bestätigt; auch der finale Lauf nach den zusätzlichen Map-Tests endet sauber (Abschnitt 36). Änderungen gestoppt. Granny bleibt Production Default; nativer Reader nur über `--gr2-reader=ziinan`. Kein Commit/Push, kein F3/F4.

## 1. F2-X Baseline

Ausgangspunkt ist `cb4e481` (`feat(assets): add native ZiiNAN GR2 reader`), zu Beginn sauberer Source-Worktree. F2-X hat 9.114/9.166 GR2-Dateien akzeptiert (99,43 %), 52 explizit abgelehnt; das Format bleibt in F2-P unverändert. Der vorhandene dokumenteigene Clipcache beseitigte die dauerhaften Mehrfach-Decodes. Im vorherigen manuellen Lauf blieben 53 aktive Gameplay-Displayintervalle über 50 ms; 52 davon enthielten neue Imports. Ein späterer Mehrfachimport erreichte etwa 0,78 s. Das frühere Audit erfasste nur Intervalle über 20 ms und liefert daher keine gültigen Gesamt-P95/P99-Werte. Siehe [F2-X-Bericht](../assets/phase-f2x-ziinan-gr2-reader.md).

## 2. Testsystem soweit bekannt

Windows 11 Home 10.0.26200; AMD Ryzen 7 9800X3D (8 Kerne/16 Threads); 33.462.632.448 Byte physischer RAM. Primäranzeige laut WMI: NVIDIA RTX 5070 Ti, Treiber 32.0.16.1656, 2560×1440 bei 164 Hz. Zusätzlich AMD-iGPU vorhanden. WMI bezeichnet die Anzeige, nicht einen separat nachgewiesenen Diligent-Adapter. Diligent D3D11, GPU-Skinning; Client 1024×768 im Fenster, vorhandene Konfiguration unverändert in isolierte Testkopien übernommen. Hardwarebeleg: `build/f2p/system.json`.

## 3. 120-FPS Framebudget

120 FPS entsprechen 8,33 ms. Der vorhandene Client aktiviert `CTimer::UseCustomTime()` und taktet `Process()` mit abwechselnd 16/17 ms; `SetFPS()` ändert diese Zeitbasis nicht. Außerdem bleibt `Present(1)` aktiv. Gesamt-Displayintervalle um 16,5 ms sind daher kein Nachweis eines langsamen GR2-Pfads. Diese Arbeit verändert weder Timing noch VSync. Ausgewiesen werden Display-Wallzeit, gemessene Process-Zeit abzüglich Sleep und Present-Wait sowie separate Asset-/Pose-/Palette-Zeiten. Letztere sind CPU-Wall-Scope-Messungen, keine GPU-Timestamps und keine vollständige CPU-Auslastungsmessung.

## 4. Cold vs Warm Definition

Jeder automatisierte Lauf startet einen frischen Prozess mit frischen Dokument-/Clip-/GPU-Caches. Der Betriebssystem-Dateicache wird nicht künstlich geleert. Gleiche A1-Koordinaten (44000/27200), Krieger Race 0, Hair 1001, Weapon 19, 20 Wildhunde Race 101. 21 Instanzen existieren; Sichtbarkeit/LOD begrenzen die tatsächlich pro Frame ausgewerteten/gezeichneten Instanzen.

Phase 0 enthält Start/Map-Laden bis zur ersten Präsentation. Danach: Cold Idle 10 s, erste Walk/Run-Verwendung 10 s, erste Attack/Damage/Knockdown/Death-Sequenz 12 s; anschließend je 15 s Warm Idle/Movement/Combat. Bekannte Hunde werden gelöscht und neu erzeugt. Movement bewegt den betrachteten Weltbereich und wechselt die echten Bewegungsklips. Combat löst originale Animationspfade aus; es simuliert keine Server-KI oder Trefferberechnung. Echte Eingabe/Kämpfe werden separat manuell geprüft.

`--load-warmup-audit` reserviert Speicher vor Messbeginn: maximal 100.000 Displayintervalle, 100.000 Content-Events und 4 MiB Diagnosetext. Kein Schwellenfilter; alle präsentierten Intervalle, einschließlich Ausreißer, werden erfasst. Process-Zeilen werden nicht zusätzlich summiert. CSV/Diagnosetext entstehen nach Ende der Spielschleife. Die bestehenden Spiel-Log-Sinks puffern im Messmodus; laufende Renderer-Diagnosedateien sind in diesem Modus deaktiviert. Startup- und Shutdown-Dateien liegen außerhalb des Gameplay-Fensters. Dropped-Counter müssen null bleiben.

Fokusstatus wird mitgespeichert und nicht nachträglich herausgefiltert. Granny: 4265 inaktive, 0 minimierte Displayintervalle, 4674 gesamte Displayintervalle. Native off: 4652 inaktive, 0 minimierte Displayintervalle, 4652 gesamte Displayintervalle. Native on: 0 inaktive, 0 minimierte Displayintervalle, 4674 gesamte Displayintervalle. Die Fixture rendert bei Fokusverlust weiter; der reale Fenstertest folgt separat.

Der erste Diagnoseversuch `baseline-native-01` besaß eine ungenaue Phasengrenze am ersten Present. Er bleibt Rohbeleg, wird aber nicht für die vergleichenden Tabellen verwendet. Die korrigierte Fixture wartet auf das erste Loading-Present, bevor sie Cold Idle startet.

## 5. Idle Baseline

Warmer Abschnitt: Avg 16.500, Median 16.522, P95 17.688, P99 18.197, Max 18.561 ms. CPU-Process ohne Sleep/Present-Wait P95 0.488 ms; Asset-Max 0.000 ms, 0 Imports. Erster entsprechender Cold-Abschnitt: Asset-Max 10.846 ms, 1 Imports, 0 Displayintervalle >50 ms. Vollständige Schwellenwerte in Abschnitt 26.

## 6. Movement Baseline

Warmer Abschnitt: Avg 16.500, Median 16.518, P95 17.676, P99 18.151, Max 18.797 ms. CPU-Process ohne Sleep/Present-Wait P95 0.546 ms; Asset-Max 0.000 ms, 0 Imports. Erster entsprechender Cold-Abschnitt: Asset-Max 57.826 ms, 4 Imports, 2 Displayintervalle >50 ms. Vollständige Schwellenwerte in Abschnitt 26.

## 7. Combat Baseline

Warmer Abschnitt: Avg 16.499, Median 16.519, P95 17.616, P99 18.054, Max 18.794 ms. CPU-Process ohne Sleep/Present-Wait P95 0.477 ms; Asset-Max 0.000 ms, 0 Imports. Erster entsprechender Cold-Abschnitt: Asset-Max 143.359 ms, 8 Imports, 4 Displayintervalle >50 ms. Vollständige Schwellenwerte in Abschnitt 26.

## 8. First-Use Baseline

Finale Referenz `final-native-off`: erste Bewegung Asset-Maximum 57,826 ms, erste Kampfphase 143,359 ms; 2 bzw. 4 Displayintervalle über 50 ms. Das bestätigt den verbleibenden First-Use-Hotspot trotz bereits funktionierender Wiederverwendung. Die 365 GR2-Dokumente selbst sind im Setup geladen; die späteren Spitzen sind Clipaufbereitung. Der zusätzliche unabhängige `baseline-native-02`-Lauf bestätigte dieselbe Klasse (54,997/144,828 ms), wird nicht mit der finalen Verteilung vermischt.

## 9. Asset Decode Costs

Summen über den gesamten kurzen Prozess einschließlich Start/Loading; Darstellung `Aufrufe / ms`. Die nativen Detailtimer sind keine SDK-internen Granny-Timer.

| Scope | Granny | Native off | Native on |
|---|---:|---:|---:|
| gr2_read | 365 / 8.258 | 365 / 9.743 | 365 / 9.019 |
| decompress | SDK-intern | 994 / 736.462 | 994 / 690.662 |
| container | SDK-intern | 365 / 775.457 | 365 / 729.073 |
| parse | SDK-intern | 365 / 291.573 | 365 / 264.568 |
| animation_decode | SDK-intern | 905 / 579.598 | 2858 / 1526.000 |
| clip_bind | SDK-intern | 929 / 21.968 | 2919 / 58.416 |
| fingerprint | 365 / 8.447 | 0 / 0.000 | 0 / 0.000 |
| skeleton | SDK-intern | 132 / 1.842 | 132 / 1.504 |
| mesh | SDK-intern | 152 / 101.765 | 152 / 93.221 |
| material | SDK-intern | 171 / 4.448 | 171 / 4.170 |
| prewarm | 0 / 0.000 | 0 / 0.000 | 1 / 1545.480 |
| pose | 0 / 0.000 | 29166 / 311.730 | 29283 / 306.582 |
| reference_pose | 28717 / 87.573 | 0 / 0.000 | 0 / 0.000 |
| palette | 0 / 0.000 | 29166 / 51.752 | 29283 / 51.978 |
| gpu_create | 58 / 2.492 | 58 / 2.567 | 58 / 2.301 |
| gpu_upload | 23056 / 18.293 | 22988 / 18.334 | 23083 / 17.417 |
| submission | 4674 / 1278.850 | 4652 / 1500.960 | 4674 / 1490.640 |
| present_wait | 4674 / 201.646 | 4652 / 201.097 | 4674 / 205.242 |
| sleep | 4674 / 75303.600 | 4651 / 74680.200 | 4674 / 75128.000 |

Scopes sind teilweise verschachtelt: Oodle liegt im Container; Skeleton/Mesh/Material liegen im Parse, Material außerdem im Mesh; Palette liegt in Pose. Nur `read + container + parse + animation_decode + clip_bind + fingerprint` wird als exklusive native Asset-Summe ausgewiesen. Import/Prewarm sind übergeordnete Gesamtzeiten und werden nicht nochmals addiert. Granny-interne Dekompressions-/Parse-/Binding-Schritte bleiben SDK-intern; fehlende Detailtimer bedeuten dort nicht null Arbeit. GPU-Werte sind CPU-Aufrufzeiten, keine GPU-Ausführungszeit.

In allen sechs freigegebenen Native-on-Gameplayphasen: jeweils **0 Calls** für Read, Oodle, Container, Parse, AnimationDecode, ClipBind, Skeleton, Mesh, Material und Vollfile-Fingerprint. Die Laufhilfe prüft diese Bedingung für jede einzelne CSV-Zeile.

## 10. Clip Sharing

Grundlage bleibt `GR2::Document::BoundClip`: Schlüssel aus Animationsindex, Skeleton-Bindungs-ID und Trackgruppe; bis zu vier Randvarianten. Clip und Skeleton sind immutable; Zeit, aktive/vorherige Clips, Blendzustand, lokale/world Pose und Palette gehören zur Instanz. Prewarm erzeugt keine temporären Actor-Instanzen und ändert kein Playback.

## 11. Duplicate Decode Audit

Neuer `AssetRuntime.GR2Warmup`-Test: erste Loopvariante genau einmal decodiert, 20 Instanzen nutzen sie ohne zusätzlichen Decode. Finite Variante wird gezielt ergänzt, wiederholte Bindung ist ein Cachehit. Nach Actor-Destroy bleibt der Clip beim Dokument; nach letztem Owner werden Clip und Skeleton frei. Die Szenenfixture erzeugt bekannte Hunde wiederholt neu.

Der reale ResourceManager verwendet den bestehenden normalisierten Dateinamen-Schlüssel und reicht dasselbe Dokument weiter. Das beweist Wiederverwendung derselben geladenen Assetidentität. Bytegleiche Dateien unter beliebigen unterschiedlichen Aliasnamen werden durch diesen dokumentlokalen Cache nicht global dedupliziert. Dafür wurde kein neuer Vollinhalt-Hashcache eingeführt. Diese Grenze darf nicht als globale Inhaltsdeduplizierung ausgegeben werden.

## 12. Fingerprint Costs

Kein neuer Vollfile-Fingerprint. Der native Bindungsschlüssel ist die bereits vorhandene Skeleton-Bindungs-ID; Dokumentidentität wird wiederverwendet. Dateiformat-Integritätsprüfung findet beim Load statt. Beide nativen Vergleichsläufe haben insgesamt null Vollfile-Fingerprint-Aufrufe, der bestehende Granny-Pfad 365 beim Load (8,447 ms insgesamt). Alle warmen Vergleichsphasen haben null Fingerprint-Aufrufe.

## 13. Allocations

Der gezielte Test zählt C++-`new`/`new[]` während 120×20 warmen Motion-Wechseln mit Blend und Evaluate. Release und GCC: **0 Motion-Allokationen, 0 Pose-Allokationen, 0 neue Decodes**. MSVC Debug: 2.400 Motion-Allokationen, 0 Pose-Allokationen; der getrennte Kontrollversuch erzeugt genau eine Allokation je leerem `std::string` durch die checked-STL-Containerverwaltung. Genau diese bereits bestehende leere Fehlerdiagnose wird bei jedem Motion-Wechsel konstruiert. Der Debug-Test verlangt diese exakte Kontrollbilanz; Release/GCC verlangen weiterhin strikt null. Ein anfänglich konfigurationsübergreifend formulierter Null-Test wurde damit präzisiert, keine Runtime umgebaut und kein Pose-Limit gelockert.

Pose-/Scratch-/World-/Palette-Vektoren sind pro Instanz vorbereitet. Kein temporärer Clip-Aufbau pro Sample. Der Test ist kein globaler Heap-Profiler des Clients und deckt weder fremde `malloc`-Aufrufe noch jedes Renderer- oder Python-Subsystem ab. Deshalb keine unbelegte Gesamtbehauptung und keine Mikrooptimierung an fremden Pfaden.

## 14. Pose Costs

Native Evaluate und Granny-Referenz-Evaluate werden separat gemessen. Nach Prewarm liegt die warme native Pose-P95 je nach Phase bei 0,083–0,112 ms; das höchste warme Pose-Displayaggregat bei 0,177 ms. Kein belegter 8,33-ms-Blocker in diesem Animationspfad. F1-Sampling, Blend, Hierarchie und Matrixmathematik sind unverändert.

## 15. Bone Palette Costs

Native `BuildPalette` erhält einen eigenen Scope innerhalb der Posemessung; nur Timing und gleichwertige Fehlerprüfung ergänzt. Warme Palette-P95 0,012–0,017 ms, höchste warme Aggregatzeit 0,041 ms. CPU-Palettenmathematik und GPU-Palettenupload sind getrennte Messgrößen.

## 16. GPU Upload Costs

Timer um bestehende Geometrieerzeugung und Palette-Map/Copy; kein geänderter Buffer-, Shader- oder Skinningpfad. Warme CPU-Palettenupload-P95 0,004–0,006 ms; GPU-Ressourcenanlage bei bekannten neu erzeugten Actors maximal 0,101 ms in den warmen Phasen. Neue Actor-Instanzen dürfen ihre kleinen GPU-/Palette-Ressourcen anlegen; das ist nicht mit GR2-Decode gleichzusetzen. GPU-Ausführungszeit ist nicht per Timestamp instrumentiert. CPU-Deformation und unerwartete GPU-Fallbacks sind in den akzeptierten Szenenläufen null.

## 17. Render/Present Costs

Render-Submission, Prepare und Present-Wait separat. Submission/Prepare können sich verschachteln; nicht als unabhängige Kosten addieren. Die bestehende 16/17-ms-Taktung und VSync bleiben bestehen. Nicht gemessene GPU-Ausführungszeit wird nicht als nachgewiesener Flaschenhals bezeichnet.

## 18. Prewarm Architecture

Bestehender Pfad: `introLoading.__StartGame → net.StartGame → GameWindow.Open/SendEnterGame → initiale Actor-Pakete → Server-GAME-Paket → CPythonNetworkStream::SetGamePhase → PrewarmVisibleActors → ActorInstance::PrewarmMotions → PrepareGR2Animation → Document::BoundClip → BindAnimation`. Der Server ruft `CHARACTER::Show/UpdateSectree` vor `SetPhase(PHASE_GAME)` auf. Die Vorbereitung liegt daher am bestätigten Netzwerkübergang Loading→Game, nach den initialen Actor-Paketen und vor dem ersten regulären Gameplay-Update. Das Python-Gamefenster wird bereits für die Eintrittsanfrage aufgebaut; es ist nicht der Nachweis, dass Welt-Actors schon existieren. Kein verändertes Netzwerkprotokoll und keine zusätzliche Eintrittsanfrage.

Der eigene Player hat Priorität, dann bereits bekannte Actorprofile. Verwendet werden ausschließlich vorhandene Body-/LOD-Dokumente und registrierte gemeinsame Clips im aktuellen Motionmodus sowie GENERAL. Idle/Walk/Run/Attack/Combo/Damage/Knockdown/Death und kleine NPC-Sondermotionmenge sind abgedeckt. Hair teilt die Körperpose, starre Weapons benötigen keinen eigenen Clip; vorhandenes Mount wird rekursiv vorbereitet.

Profile unterscheiden Race, Shape, aktuellen Motionmodus und Mount-Race; zwei gleich aussehende Reiter mit unterschiedlichen Mounts werden nicht zusammengefasst. Grenzen: maximal 32 Profile, 512 neue Clip-Decodes, 10 Sekunden zwischen einzelnen Vorbereitungsaufrufen geprüft. Ein einzelner Decode wird nicht mitten in der Arbeit abgebrochen, die Zeitgrenze ist daher keine harte 10-s-Latenzgarantie. Idle/Walk/Run werden entsprechend `SetLoopMotion` als Loop vorbereitet, Kampf/Reaktionen entsprechend `PushOnceMotion` als endliche Variante. Ungewöhnliche andere Nutzungen bleiben bedarfsgesteuert. Es werden nicht alle 9.166 Dateien geladen. `--gr2-prewarm=off` erlaubt den direkten Vergleich; Granny überspringt diesen Pfad vollständig.

## 19. Background Work Decision

Kein Background-Worker: Das gezielte Loading-Prewarm beseitigt in der A1-Fixture alle neuen Asset-/Cliparbeiten ab der ersten freigegebenen Gameplay-Phase. Ein Worker wäre für diesen belegten Fall zusätzliche Komplexität ohne notwendigen Nutzen. Keine Streaming-Engine, keine GPU-Aufrufe von neuen Threads. Der manuelle neue Serverpfad muss diesen Befund noch ergänzen.

## 20. Single-flight Decision

Der bestehende Actor-/Motionpfad und die gezielte Vorbereitung laufen seriell auf dem Main-Thread. Der erste Request legt immutable Daten im Dokument ab, nachfolgende Requests verwenden sie. Es gibt keinen neuen parallel laufenden Auftrag, daher auch keinen neuen Future-/Jobcache. Der vorhandene dokumentlokale Cache wird nicht als threadsichere Worker-API ausgegeben.

## 21. Implemented Optimizations

Gezieltes Loading-Prewarm vorhandener Actor-/LOD-Clipbindungen; weiterhin dokumenteigene Wiederverwendung. Die Auswahl bereitet die tatsächlich üblichen Loop-/Einmal-Controls vor. Ein pauschales Vorbereiten beider Varianten hatte eine unbenötigte Loop-Variante von Warrior `combo_04` am bestehenden Spline-Refinement-Limit abgewiesen und den frühen Vorladeversuch abgebrochen. Die Variantenauswahl wurde korrigiert, keine Reader-Toleranz erhöht. Der finale Test verlangt null Prewarmfehler/Limitabbrüche/Cachebypässe und prüft jede Gameplayzeile auf null Assetarbeit.

Ergänzt sind umschaltbare Prewarm-Option, begrenztes All-Frame-Audit mit gepufferten Diagnosen, reproduzierbare A1-Fixture, Verteilungsanalyse und Sharing-/Lifetime-/Warm-Allokationstest. Keine Formatänderung, keine neue Asset-/Animationsarchitektur, keine PBR-/Vegetations-/Rendereroptimierung.

## 22. Idle After

Warmer Abschnitt: Avg 16.500, Median 16.521, P95 17.765, P99 18.230, Max 18.791 ms. CPU-Process ohne Sleep/Present-Wait P95 0.490 ms; Asset-Max 0.000 ms, 0 Imports. Erster entsprechender Cold-Abschnitt: Asset-Max 0.000 ms, 0 Imports, 0 Displayintervalle >50 ms. Vollständige Schwellenwerte in Abschnitt 26.

## 23. Movement After

Warmer Abschnitt: Avg 16.499, Median 16.527, P95 17.573, P99 18.055, Max 19.149 ms. CPU-Process ohne Sleep/Present-Wait P95 0.553 ms; Asset-Max 0.000 ms, 0 Imports. Erster entsprechender Cold-Abschnitt: Asset-Max 0.000 ms, 0 Imports, 0 Displayintervalle >50 ms. Vollständige Schwellenwerte in Abschnitt 26.

## 24. Combat After

Warmer Abschnitt: Avg 16.502, Median 16.512, P95 17.716, P99 18.113, Max 18.689 ms. CPU-Process ohne Sleep/Present-Wait P95 0.501 ms; Asset-Max 0.000 ms, 0 Imports. Erster entsprechender Cold-Abschnitt: Asset-Max 0.000 ms, 0 Imports, 0 Displayintervalle >50 ms. Vollständige Schwellenwerte in Abschnitt 26.

## 25. Cold After

Automatischer Native-on-Lauf: erstes Loading-Displayintervall 3.703,247 ms ab Auditstart; darin gezieltes Prewarm 1.545,480 ms. Insgesamt 365 GR2-Reads und 46 Clip-Decodes, alle vor Phase 1. 156 vorbereitende Binding-Requests, null Fehler, null Limitabbruch, null Cachebypässe. Der frische Prozess ist kein Festplatten-Coldcache-Test. Im ersten Idle bleibt ein einzelnes 20,755-ms-Displayintervall ohne neue Assetarbeit; erste Bewegung/Kampf maximal 19,118/18,779 ms.

Prewarm entfernt nicht die Gesamtkosten: Es bereitet mehr häufige Clips vor, als die kurze Fixture später tatsächlich abspielt. Automatisches Loading und echter frischer Login werden getrennt dokumentiert; manuelle Ergänzung folgt.

## 26. P95/P99/max

Alle Werte in ms; Schwellen strikt `>`. Keine Ausreißer entfernt.

| Pfad / Phase | N | Avg | Median | P95 | P99 | Max | >8,33 | >16,67 | >20 | >50 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Granny / loading | 1 | 1725.324 | 1725.324 | 1725.324 | 1725.324 | 1725.324 | 1 | 1 | 1 | 1 |
| Granny / cold_idle | 607 | 16.480 | 16.519 | 17.877 | 18.212 | 18.669 | 606 | 245 | 0 | 0 |
| Granny / cold_movement | 607 | 16.500 | 16.516 | 17.671 | 18.056 | 18.544 | 607 | 249 | 0 | 0 |
| Granny / cold_combat | 728 | 16.499 | 16.511 | 17.946 | 18.225 | 18.631 | 728 | 285 | 0 | 0 |
| Granny / warm_idle | 910 | 16.501 | 16.524 | 17.770 | 18.113 | 18.746 | 910 | 365 | 0 | 0 |
| Granny / warm_movement | 910 | 16.500 | 16.520 | 17.903 | 18.223 | 18.590 | 910 | 364 | 0 | 0 |
| Granny / warm_combat | 911 | 16.501 | 16.537 | 18.005 | 18.258 | 18.610 | 911 | 363 | 0 | 0 |
| Native off / loading | 1 | 2388.160 | 2388.160 | 2388.160 | 2388.160 | 2388.160 | 1 | 1 | 1 | 1 |
| Native off / cold_idle | 607 | 16.494 | 16.523 | 17.698 | 18.358 | 34.181 | 606 | 244 | 1 | 0 |
| Native off / cold_movement | 602 | 16.636 | 16.509 | 17.660 | 18.021 | 75.740 | 600 | 242 | 2 | 2 |
| Native off / cold_combat | 711 | 16.896 | 16.514 | 17.657 | 18.309 | 161.977 | 709 | 265 | 4 | 4 |
| Native off / warm_idle | 910 | 16.500 | 16.522 | 17.688 | 18.197 | 18.561 | 910 | 359 | 0 | 0 |
| Native off / warm_movement | 910 | 16.500 | 16.518 | 17.676 | 18.151 | 18.797 | 910 | 372 | 0 | 0 |
| Native off / warm_combat | 911 | 16.499 | 16.519 | 17.616 | 18.054 | 18.794 | 911 | 371 | 0 | 0 |
| Native on / loading | 1 | 3703.247 | 3703.247 | 3703.247 | 3703.247 | 3703.247 | 1 | 1 | 1 | 1 |
| Native on / cold_idle | 607 | 16.492 | 16.521 | 17.827 | 18.336 | 20.755 | 607 | 237 | 1 | 0 |
| Native on / cold_movement | 607 | 16.500 | 16.519 | 17.809 | 18.351 | 19.118 | 607 | 249 | 0 | 0 |
| Native on / cold_combat | 728 | 16.500 | 16.544 | 17.616 | 18.086 | 18.779 | 728 | 295 | 0 | 0 |
| Native on / warm_idle | 910 | 16.500 | 16.521 | 17.765 | 18.230 | 18.791 | 910 | 367 | 0 | 0 |
| Native on / warm_movement | 910 | 16.499 | 16.527 | 17.573 | 18.055 | 19.149 | 910 | 374 | 0 | 0 |
| Native on / warm_combat | 911 | 16.502 | 16.512 | 17.716 | 18.113 | 18.689 | 911 | 354 | 0 | 0 |

CPU-Scopes nach Prewarm (ms pro Displayintervall):

| Phase | CPU P95 / Max | Asset Max | Pose P95 / Max | Palette P95 / Max | GPU-Create Max | GPU-Upload P95 / Max | Submission P95 | Present-Wait P95 / Max |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| loading | 2230.576 / 2230.576 | 2587.081 | 1.005 / 1.005 | 0.344 / 0.344 | 0.000 | 0.000 / 0.000 | 2.287 | 1.134 / 1.134 |
| cold_idle | 0.624 / 9.096 | 0.000 | 0.145 / 0.284 | 0.025 / 0.046 | 0.531 | 0.008 / 0.471 | 0.578 | 0.056 / 0.820 |
| cold_movement | 0.750 / 1.950 | 0.000 | 0.179 / 0.249 | 0.031 / 0.067 | 0.207 | 0.012 / 0.121 | 0.619 | 0.070 / 0.133 |
| cold_combat | 0.442 / 1.650 | 0.000 | 0.076 / 0.141 | 0.010 / 0.021 | 0.055 | 0.003 / 0.010 | 0.353 | 0.055 / 0.257 |
| warm_idle | 0.490 / 1.333 | 0.000 | 0.083 / 0.175 | 0.015 / 0.025 | 0.000 | 0.004 / 0.018 | 0.411 | 0.055 / 0.130 |
| warm_movement | 0.553 / 1.243 | 0.000 | 0.112 / 0.177 | 0.017 / 0.041 | 0.101 | 0.006 / 0.020 | 0.453 | 0.057 / 0.136 |
| warm_combat | 0.501 / 1.079 | 0.000 | 0.090 / 0.125 | 0.012 / 0.023 | 0.032 | 0.004 / 0.017 | 0.407 | 0.056 / 0.085 |

CPU = gemessene Process-Wallzeit minus Sleep und Present-Wait; keine reine Thread-CPU-Zeit. Loading kann Arbeit vor dem ersten Process enthalten, daher ist dort die Asset-Summe nicht durch diesen CPU-Wert begrenzt. Scopes sind verschachtelt.

## 27. Granny vs ZiiNAN

Identische isolierte A1-Fixture, Originalpacks, 1024x768 im Fenster, gleiche Release-EXE. Delta = Native on minus Granny (ms); keine statistische Signifikanzbehauptung aus einem kurzen Lauf je Konfiguration.

| Metric | Granny | ZiiNAN mit Prewarm | Delta |
|---|---:|---:|---:|
| loading p95 | 1725.324 | 3703.247 | +1977.923 |
| loading p99 | 1725.324 | 3703.247 | +1977.923 |
| loading max | 1725.324 | 3703.247 | +1977.923 |
| cold_idle p95 | 17.877 | 17.827 | -0.051 |
| cold_idle p99 | 18.212 | 18.336 | +0.124 |
| cold_idle max | 18.669 | 20.755 | +2.086 |
| cold_movement p95 | 17.671 | 17.809 | +0.138 |
| cold_movement p99 | 18.056 | 18.351 | +0.296 |
| cold_movement max | 18.544 | 19.118 | +0.574 |
| cold_combat p95 | 17.946 | 17.616 | -0.331 |
| cold_combat p99 | 18.225 | 18.086 | -0.139 |
| cold_combat max | 18.631 | 18.779 | +0.147 |
| warm_idle p95 | 17.770 | 17.765 | -0.005 |
| warm_idle p99 | 18.113 | 18.230 | +0.117 |
| warm_idle max | 18.746 | 18.791 | +0.045 |
| warm_movement p95 | 17.903 | 17.573 | -0.330 |
| warm_movement p99 | 18.223 | 18.055 | -0.169 |
| warm_movement max | 18.590 | 19.149 | +0.559 |
| warm_combat p95 | 18.005 | 17.716 | -0.289 |
| warm_combat p99 | 18.258 | 18.113 | -0.146 |
| warm_combat max | 18.610 | 18.689 | +0.079 |

Separater First-Use-Vergleich der nativen Assetarbeit:

| Phase | Native off Asset-Max | Native on Asset-Max | Delta | Off / On Imports |
|---|---:|---:|---:|---:|
| cold_idle | 10.846 | 0.000 | -10.846 | 1 / 0 |
| cold_movement | 57.826 | 0.000 | -57.826 | 4 / 0 |
| cold_combat | 143.359 | 0.000 | -143.359 | 8 / 0 |
| warm_idle | 0.000 | 0.000 | +0.000 | 0 / 0 |
| warm_movement | 0.000 | 0.000 | +0.000 | 0 / 0 |
| warm_combat | 0.000 | 0.000 | +0.000 | 0 / 0 |

## 28. Remaining Hitches

Die finale A1-Fixture hat nach Freigabe null Asset-Spitzen und null Displayintervalle über 50 ms. Das erste Cold-Idle-Intervall dauert 20,755 ms: davon 10,774 ms Sleep, 8,358 ms Render-Submission, 0,820 ms Present-Wait; Pose 0,122 ms, Assetarbeit 0. Die ineinanderliegenden GPU-Erzeugungsaufrufe benötigen 0,531 ms. Daraus folgt kein Anlass, in F2-P Renderer oder GPU-Skinning umzubauen. In den warmen Phasen liegt das höchste Displayintervall bei 19,149 ms, wesentlich von der vorhandenen Taktung geprägt.

Unbekannte später auftauchende Rassen, neu ausgerüstete Körpermodelle/Motionmodi, seltene ungeprewarmte Skills, unübliche Randvarianten oder überschrittene Prewarm-/Dokumentcache-Grenzen können weiterhin Cold-Arbeit auslösen. Die Arbeit verspricht keine Hitchfreiheit jeder Map oder jedes Ausrüstungswechsels. Ein späterer Production-Switch muss die unveränderten 52 Corpus-Rejects und diese Grenzen separat berücksichtigen. Der manuelle Spawn-Befund und dessen bestätigte Korrektur sind in Abschnitt 36 dokumentiert.

## 29. Release/Debug/GCC

Release-Client und alle vom gezielten Gate benutzten Testprogramme gebaut: **43/43 Tests bestanden, 109,32 s** (`build/f2p/release-gate-build.log`, `release-gate-tests.log`). Enthalten sind Reader-Safety/Unabhängigkeit/Realasset-/GPU-Parität, neues Prewarm/Sharing, F1-Mathematik-/Granny-Parität, echte Actor-/Hair-/LOD-Pfade, Provider-/Lifetime-Verträge und kleine Renderer-/Plattformregressionen.

Debug-Client und alle Gate-Testprogramme gebaut. Der erste Gesamtlauf bestand 42/43 in 194,28 s; allein der neue pauschale Null-Allokationstest erfasste die Debug-STL-Proxys. Nach dem getrennten Kontrollversuch besteht dieser Test ebenfalls (0,85 s), damit sind alle 43 gezielten Testfälle erfolgreich nachgewiesen. Der gesamte Debug-Lauf wurde nicht unnötig wiederholt. Details: `debug-gate-build.log`, `debug-gate-tests.log`, `debug-warmup-final.log`; Allokationsgrenze siehe Abschnitt 13.

Nach Verlegung des Netzwerk-Einhängepunkts wurden beide Clients erneut gebaut (`release-entry-build.log`, `debug-entry-build.log`) und der neue Warmuptest in Release/Debug/GCC erneut bestanden. Der A1-Fixture-Aufbau und die dort gemessene Vorbereitungsarbeit sind unverändert. Der anschließend korrigierte echte Netzwerkübergang ist mit der finalen manuellen Release-Kopie bestätigt (Abschnitt 36); die früheren Debug-/GCC-Nachweise werden nicht als Wiederholung des späteren Quick Fix ausgegeben.

GCC 12.4/Cygwin LP64: vollständiger vorhandener portabler Build, **14/14 Tests bestanden, 0,50 s**, einschließlich neuem Warmup/Sharing-Test (`final-gcc-build.log`, `final-gcc-tests.log`); finaler Warmuptest erneut in 0,05 s bestanden. Keine vollständige Corpuswiederholung bei unverändertem Format, keine 583er Suite und kein Fuzzer. Bekannte externe LNK4099-PDB-Warnungen (Python/zlib), LTCG-Neustarthinweise und Debug LNK4075/LNK4098 (Incremental/LIBCMT) bleiben. Kein warning-free-Anspruch.

## 30. Runtime Smoke

Die drei finalen Läufe `build/f2p/runtime/final-granny`, `final-native-off`, `final-native-on` verwenden dieselbe Release-EXE mit SHA-256 `9F533797E1F84DE9E71E187AE9D662AB418879333092184C8F06EC161263C305`. Jeweils frischer Prozess, Originalpacks, vollständige A1-Welt und 21 Actor-Instanzen, unter 90 Sekunden pro Lauf. Granny liest 365 Dateien über Granny und null über Native; beide Native-Läufe lesen 365 über Native und null über Granny. Keine CPU-Deformation, keine unerwarteten GPU-Fallbacks, keine Runtime-/Skin-Preparation-Fehler.

Rohbelege pro Lauf: `artifact.txt`, `exit.txt`, `load-warmup-fixture.json`, `load-warmup-frames.csv`, `metrics.json`, `animation-stall-summary.txt`, `load-warmup-diagnostics.log`, `source-resource-audit.log`. Reproduktion: `tests/AssetRuntime/run_load_warmup.ps1 -Name <frischer-name> -Reader granny|ziinan -Prewarm off|on`, danach `analyze_load_warmup.py <laufverzeichnis>`. Automatisierte Motionfolge und echter manueller Serverpfad sind getrennte Nachweise.

## 31. Manual Result

Erster F2-P-Serverlauf in `build/f2p/runtime/manual-native-01`, Release-SHA-256 `489102C86652F024F63782C305D0C721FECADEBE509D8113BC46B89019DF1460`. Originalroot und echte Serververbindung; `--load-warmup-audit --gr2-reader=ziinan --animation-runtime=ziinan --gr2-prewarm=on`. Die vorherigen Benutzerbestätigungen betreffen F2-X und ersetzen diesen Test nicht.

Benutzerbefund: Welt bereits sichtbar, eigener Player erst nach etwa 3–5 Sekunden vollständigem Bildstillstand; danach flüssig. Deshalb keine manuelle Freigabe dieses Builds. Exitcode 0 nach 204,713 s; Ressourcen, CPU-Deformation und GPU-Fallbacks jeweils null. Der manuelle Analyzer bezeichnet automatische Aktivitätsklassen als `manual_idle/movement/combat`; sie mischen erste und spätere Aktionen und werden nicht als getrennte Cold-/Warm-Fixture-Verteilungen ausgegeben.

Die anschließenden Quick-Fix-Läufe und die finale manuelle Bestätigung einschließlich Namensfarben und sauberem Shutdown stehen in Abschnitt 36.

## 32. Shutdown

Alle drei finalen automatischen Läufe: Exitcode 0; `GR2ReaderResources`, `AssetDocuments`, `AnimationInstances`, `MeshBindings`, `RuntimeSkeletons`, `RuntimeAnimationClips`, `IndependentAnimationInstances`, `SourceTextures`, `SourceBuffers`, `SkinMeshes`, `BoneRemaps`, `BonePalettes`, `PrototypeGeometry`, `PrototypePalettes`, `StaticSkinMeshes` jeweils 0. Full-Frame-Capture aktiv, null verworfene Frames/Content-Events/Diagnosebytes. Native on: 46 vorbereitete/neue Clip-Decodes insgesamt, 1.835 gemeinsame Cachehits, null Cachebypässe. Auch `manual-native-01` endet sauber; dessen Spawn-Verhalten wird damit nicht freigegeben.

Finale Quick-Fix-Kopie `local-spawn-fix-03`: Exitcode 0 nach 252,180 s. Alle oben genannten Ressourcen sowie CPU-Deformation, GPU-Fallbacks, Runtime-/Skin-Preparation-Fehler, Prewarm-Fehler/-Limits und Cachebypässe jeweils 0. Kein verworfenes Auditmaterial. Der Prozess ist beendet.

## 33. Resource Lifetime

Keine globale zusätzliche Cacheownership, kein Worker und kein zyklischer Clip→Document-Verweis. Der bisherige Cache bleibt auf 64 Binding-Schlüssel und 64 MiB pro Dokument begrenzt. Instanzen halten ihre benötigten Clips; endgültiges Dokumentende gibt vorbereitete Clips frei. Cache-Bypass-/Prewarm-Failure-/Limit-Zähler werden im Shutdown geprüft. Ein Limit bedeutet unvollständiges Warmup, nicht stillschweigende vollständige Vorbereitung.

## 34. Git Diff

Gezieltes Prewarm in Actor/CharacterManager/Netzwerk-Gameübergang, kleine Startup-/Python-Brücken, dokumenteigene Clipvorbereitung, optionale Timer/Diagnosepuffer, Tests/Laufhilfen und dieser Bericht. Die bestehende Dokumentcache-/Reader-/Animationsmathematik bleibt Grundlage. Der Quick Fix ergänzt die Freigabebedingung und verwendet die bestehenden Actor-GPU-/Texture-Pfade zur Vorbereitung der ersten Pose. Original-Pythondateien und Packs bleiben unverändert.

`git diff --check` ist sauber. Vollständiger lesbarer Patch einschließlich neuer Tests und Bericht: `build/f2p/review.patch`, nach finaler Abnahme aktualisiert. Alle Builds, Laufkopien und Rohmessungen liegen unter ignoriertem `build/f2p`. Originalclient und Packs werden nicht ersetzt; kein Commit/Push.

## 35. GO/NO-GO für F3-X

Quick Fix GO: Der Benutzer bestätigt den behobenen Spawn-Freeze samt Bewegung/Angriff und Hair/Weapon sowie anschließend die korrigierten Mob-Namensfarben. Beide abgeschlossenen Läufe `local-spawn-fix-02` und `local-spawn-fix-03` weisen CPU-Deformation, GPU-Fallbacks und Shutdown-Ressourcen jeweils 0 nach. Die finale Kopie wurde nach den zusätzlichen Map-Tests beendet. Implementierungsstopp gemäß Quick-Fix-Auftrag; Granny bleibt Default, kein F3/F4 und keine pauschale Freigabe aller Maps/Assets.

## 36. Quick Fix – Local Player Spawn Freeze

Gezielter Befund aus `manual-native-01`: Das Eintrittsintervall bei 40.881,970 ms dauerte 6.555,937 ms. Darin 149 neue Clip-Imports, 5.499,667 ms Animationsdecode, 234,220 ms Trackbinding, 1,182 ms Skeleton-Aufbau und 4,509 ms GPU-Erstellung. Der enthaltene Prewarm dauerte 4.817,978 ms; diese verschachtelten Zeiten sind nicht zu addieren. CPU-Aufbereitung der Clips dominiert den Block; GPU-Erstellung erklärt ihn nicht. Der alte Ablauf erzeugte das Python-GameWindow und schloss LoadingWindow, bevor `GameWindow.Open()` ENTERGAME sendete und die initialen Actors synchron geladen/vorbereitet wurden.

Native mit aktiviertem F2-P-Prewarm sendet ENTERGAME jetzt aus dem vorhandenen StartGame-Übergang, während LoadingWindow bestehen bleibt. Nach Server-GAME und vorhandenem lokalem Actor wird der vorhandene `PrewarmVisibleActors` aufgerufen: lokaler Player zuerst, nur Idle/Walk/Run, normaler Angriff und aktuell benutzte Standard-Combo, einschließlich bereits registrierter Varianten und tatsächlicher Motion-Fallbacks. Keine vollständige Player-Animationsbibliothek wird kompiliert. Der bestehende Reader lädt weiterhin die registrierten Quelldokumente; das ist keine neue Lazy-Load-Architektur.

Die erste lokale Pose, Palette, Skinning-Bindings, Body/Armor/Hair/Weapon-Geometrie und Materialtexturen werden im vorhandenen Actor-Preview-/GPU-Pfad vorbereitet. Dieser Schritt liegt nach BeginFrame (vom bestehenden GPU-Pfad vorausgesetzt), aber vor der Python-Phasenfreigabe; es wird dabei keine Welt gezeichnet. Der spätere ENTERGAME-Aufruf von GameWindow wird für diesen Eintritt unterdrückt. Bei verspätetem lokalem Actor bleibt der Ladebildschirm erhalten; bei fehlerhafter lokaler Vorbereitung wird die Welt nicht freigegeben. Offline/erneutes Loading setzen den Übergang zurück. Ein optionales Budgetlimit bei Umgebungsactors verhindert nicht die Freigabe eines vollständig vorbereiteten lokalen Players. Granny und Native ohne Prewarm behalten ihren bisherigen Startablauf.

Das vorhandene Audit erhält ausschließlich eine begrenzte lokale Aufschlüsselung (`local-player-spawn.csv`, höchstens 64 Einträge): Race/Model, Body/Armor, Hair, Weapon, gesamtes Create, minimale Clips, erste Pose/Ressourcen und Freigabe. Darin die bereits vorhandenen GR2-/Decode-/Skeleton-/Binding-/GPU-Timer als Differenzen. Kein Datei-I/O während des Spawns.

Erster Quick-Fix-Build (`local-spawn-fix-01`, SHA-256 `1F561730D03D454201786A64D2C4BE623D17A8D4D816A88863786098F966D6E1`): Release gebaut, zwei kurze vorhandene Tests bestanden (0,17 s). Manueller Versuch fehlgeschlagen: Rücksprung zum Login, anschließend Exit `0xc0000005`. Die neue Mindestprüfung verlangte fälschlich den NPC-NormalAttack-Eintrag auch beim Krieger, der seine Standardangriffe als Combo registriert. Korrigiert auf die erste Motion der tatsächlich registrierten Combo, falls kein separater NormalAttack-Eintrag existiert.

Die Absturzadresse wurde mit der passenden Release-PDB auf `CBaseCollisionInstance::Destroy` aufgelöst, Aufrufer `CGraphicObjectInstance::Initialize`/Destruktor. Bei Abbruch aus LoadingWindow fehlt das reguläre `GameWindow.Close`, das die Welt sonst früh abbaut. `CPythonApplication::Destroy` gab den Kollisionspool vor den noch lebenden Weltobjekten frei. Der bestehende Pool-Shutdown steht jetzt nach der Freigabe seiner Besitzer. Kein Umbau der Ressourcenownership.

Zweiter Release-Build bestanden (`local-spawn-build-02.log`). Ein ausgeblendeter Ein-Actor-Probeversuch erreichte keinen Abschluss innerhalb seines 25-s-Limits; Windows verweigerte anschließend das Schließen/Beenden des gestarteten Prozesses. Dieser Versuch ist kein bestandener Test. Der Benutzer bestätigte das Beenden von PID 68388, bevor die neue manuelle Kopie gestartet wurde.

`local-spawn-fix-02`, SHA-256 `22BC70D37E68DB471CD8AD54273DEA4184E16A244935B86245734BC9DA773CA3`: Benutzer bestätigt Welt-Eintritt ohne mehrsekündigen Freeze, Player direkt nach Loading sichtbar, Bewegung/Angriff und Hair/Weapon korrekt ("jaa das passt"). Exit 0 nach 110,481 s; CPU-Deformation 0, GPU-Fallbacks 0, alle erfassten Shutdown-Ressourcen 0, NativePrewarmFailures/Limited/BoundClipBypasses jeweils 0. Keine verworfenen Auditdaten.

Gezielte lokale Messung aus `local-player-spawn.csv`:

| Lokale Stufe | Wallzeit |
|---|---:|
| Race/Model-Quelldokumente | 303,762 ms |
| Body/Armor | 0,740 ms |
| Hair | 0,045 ms |
| Weapon | 1,913 ms |
| Create gesamt (enthält obige Stufen) | 315,721 ms |
| Minimale Player-Clips | 237,843 ms |
| Erste Pose und Renderressourcen | 2,027 ms |

Create enthält 2,002 ms GR2-Read, 242,831 ms Containerarbeit (davon 233,889 ms Dekompression), 59,716 ms Parse, 8,577 ms Animationdecode, 0,339 ms Binding und 0,013 ms neuen Skeleton-Aufbau. Die minimalen Clips enthalten 228,264 ms Decode und 9,496 ms Binding. Erste Pose/Ressourcen enthalten 0,017 ms Pose, 0,005 ms Palettenberechnung, 0,289 ms GPU-Erstellung und 0,004 ms Palettenupload. Bestehende Dokumente/Skelette aus Character Select werden wiederverwendet; verschachtelte Zeiten nicht addieren.

Lokaler Player bereit bei 17.155,540 ms; Freigabeaufruf erst bei 21.476,751 ms. Dazwischen liegt weiterhin das bestehende Warmup der bekannten Umgebungsactors (etwa 4,32 s). Der lokale Pfad allein erklärt daher keinen 3–5-s-Block: Der sichtbare Freeze entstand durch die zu frühe Weltfreigabe vor dem gesamten synchronen Eintritts-Warmup. Dieses verbleibt nun hinter LoadingWindow; keine zusätzliche Umgebungsoptimierung.

Einziger gemeldeter Folgefehler: schwarze Mob-Namen. `LoadingWindow` registriert Grundfarben, aber `GameWindow.Open` wählt erst später die aktive Palette über `SetEmpireNameMode`. Dessen vorhandener Refresh aktualisierte nur PCs; früher angelegte Mobs/NPCs behielten die vorherige schwarze Farbe. Der bestehende Palette-Aufruf aktualisiert jetzt alle vorhandenen Actor-Namen. Keine geänderten Farbwerte und kein neuer Namensrenderer.

Finaler Release-Build bestanden (`local-spawn-color-build.log`, weiterhin bekannte externe LNK4099-Warnungen). `local-spawn-fix-03`, SHA-256 `F484BE166DEB305A8141FD1A9A1AB26BF4C29E5D9DDB421E966DD1F3681741CE`: Benutzer bestätigt auf die Frage nach roten Mob-Namen und weiterhin freeze-freiem Eintritt: "sieht alles gut aus ich teste noch andere maps", anschließend "erledigt". Die Kopie (PID 63640) ist beendet: Exitcode 0 nach 252,180 s. Die finale eigene Ressourcenprüfung bestätigt CPU-Deformation 0, GPU-Fallbacks 0 und alle erfassten Shutdown-Ressourcen 0; Runtime-/Skin-Preparation-Fehler, Prewarm-Fehler/-Limits und Cachebypässe ebenfalls 0. 962 Native-GR2-Reads, 0 Granny-Reads, 504 Prewarm-Requests; keine verworfenen Frames, Content-Events oder Diagnosebytes. Belege: `exit.txt`, `source-resource-audit.log`, `animation-stall-summary.txt` im Laufverzeichnis. Die zusätzlich getesteten Maps wurden nicht einzeln benannt; keine pauschale Map-Abnahme. Keine weitere Optimierung begonnen. `git diff --check` sauber, vollständiger Patch unter `build/f2p/review.patch` aktualisiert.

Keine erneute A/B-Reihe, keine lange Benchmark-/Corpus-/Debug-Suite. Nach diesem Quick Fix Stop gemäß Benutzerauftrag.
