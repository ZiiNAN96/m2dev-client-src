# P3 – Frame Pacing / FPS Settings / VSync

Die zusätzliche Anzeigeverwaltung für Auflösung, Fenstermodus und sicheren Live-Apply ist separat in [P3 – Anzeigeeinstellungen](p3-display-settings.md) dokumentiert. Die folgenden Pacing-Messwerte sind die ursprüngliche P3-Matrix und keine nachträgliche Display-Abnahme.

17.09.2026. **P3: GO – Fast Gate PASS.** 60 / 120 / Unbegrenzt und VSync Aus / Ein sind unabhängig, im vorhandenen Grafikmenü auswählbar, live anwendbar und persistent. Kein Production Deployment, kein Push. Nach P3 STOP.

## Umfang und Ausgangsstand

- Source: `m2dev-client-src`, Ausgangscommit `4b2477db3e2653279a49ba86f0684b987e05d8b7` (P2).
- Assets: getrenntes `m2dev-client`, Ausgangscommit `d54637a4b323b9d40f0e200951c40d4d226a2cfc`. Nur `assets/root/uigraphicssettings.py` und die FPS-abhängige Chat-Animation in `assets/root/uichat.py` geändert. Die vorher vorhandene Änderung an `docs/production-milestone-deployment.md` bleibt separat erhalten.
- Keine Änderungen an Qualitätswerten, Shadern, Shadows, GTAO, Vegetation, Texturen oder Sichtweitenalgorithmen. Der Smoke verwendet die vorhandenen Modern-Einstellungen und den gleichen expliziten Sichtweitenwert 25600 wie P2.
- Prüfung mit privatem Release-Client und privaten Root-Packs. Produktive EXEs, Root-Pack und `config/graphics.cfg` wurden nicht durch P3 ersetzt.

## Alte Frame-Taktung

`CPythonApplication::Process` rief bisher je Schleifendurchlauf `CTimer::Advance` und das komplette Game-/UI-Update auf. Custom Time alterniert **17 / 16 ms**, also tatsächlich ungefähr **60,606 Spielticks/s**. Ein millisekundenbasierter nächster Framezeitpunkt steuerte `SleepMilliseconds`; bei Verspätung wurden Renderframes unterdrückt, ab 500 ms wurde die Spieluhr korrigiert. Das Diligent-D3D11-Backend rief fest `swapChain->Present(1)` auf. Das bisherige `app.SetFPS` speicherte lediglich eine unbenutzte Zahl.

Ein bloßes Entfernen des Sleeps hätte Spieluhr, Kamera, Animation und UI pro zusätzlichem Frame beschleunigt. P2 umging das nur in seiner temporären Profiling-Instrumentierung; P3 integriert die Trennung regulär.

## Neue Limiter-Architektur

`Graphics/FramePacing.h` trennt zwei Verantwortungen:

1. `FramePacer`: monotone Nanosekunden und absolute Deadlines, mit 1/60 bzw. 1/120 s Periodendauer. Schlafverspätungen werden nicht auf jede Folgeperiode addiert. Nach vollständig verpassten Perioden wird die nächste Deadline neu gesetzt, ohne sofortige Render-Aufholserie. Ein Limitwechsel setzt den Zeitplan neu.
2. `SimulationClock`: der bisherige alternierende 17/16-ms-Spielschritt läuft unabhängig von Renderframes. Null, ein oder mehrere Spielupdates pro Renderframe; maximal acht Aufholschritte je Process-Aufruf. Nach mindestens 500 ms Rückstand wird wie bisher Zeit übersprungen, jetzt in ganzen 33-ms-Paaren und ohne Wechsel der Schrittparität. `CTimer::Adjust` aktualisiert Sekunden- und Millisekundenansicht konsistent.

Windows wartet auf einen **High-Resolution Waitable Timer**, mit normalem Waitable Timer bzw. blockierendem Sleep als Fallback. **Keine Spin-Schleife** und keine künstliche CPU-Restwartephase. Das Timer-Handle hat eine RAII-Lebensdauer am ausführenden Thread. Android besitzt eine `steady_clock::sleep_until`-Implementierung; ein Android-Build war nicht Teil dieses Windows-Fast-Gates.

Unbegrenzt überspringt den Limiter vollständig. Hohe CPU-Last in diesem Modus kann aus tatsächlicher Renderarbeit entstehen. Ein minimiertes Fenster erhält unabhängig davon eine kurze Hintergrundpause. Die alten `SetFrameSkip`-Aufrufe bleiben als kompatibler Einstieg bestehen; die neue feste Simulation ersetzt das alte Render-Skipping. `app.SetFPS(60/120/0)` ist ein Adapter auf dieselben zentralen Settings, keine zweite FPS-Konfiguration.

## Settings, UI und Persistenz

`GraphicsSettings` und `GraphicsRuntimeConfig` enthalten `FrameRateLimit` und `VSync`. Enum-Werte und die Übersetzung in Hertz/Present-Intervall stehen zentral in `GraphicsSettings.h`.

| Config-Schlüssel | Werte | Fehlender/ungültiger Wert |
| --- | --- | --- |
| `FRAME_RATE_LIMIT` | 0 = 60, 1 = 120, 2 = Unbegrenzt | 60 |
| `VSYNC` | 0 = Aus, 1 = Ein | Ein |

Das vorhandene versionierte `config/graphics.cfg` und der atomare Schreibpfad bleiben erhalten. Unbekannte kompatible Felder/Kommentare bleiben erhalten; zukünftige nicht unterstützte Versionen werden weiterhin nicht überschrieben. Die Formatversion bleibt 1, weil beide Schlüssel additive optionale Felder sind.

Das vorhandene `GraphicsDialog` besitzt zwei separate ComboBoxen. Die Anordnung bleibt auch in einem 600 Pixel hohen Client innerhalb des Fensters. Schließen speichert über den bestehenden Mechanismus. Grafik-Presets behalten FPS/VSync; reine Pacing-Änderungen verändern weder Qualitäts-Preset noch Grafikqualität.

Änderungen werden am nächsten Process-Beginn in den Runtime-Snapshot übernommen. `FramePacingChanged` löst keinen Shadow-/Vegetation-/View-Distance-Refresh aus. SwapChain, Device und Renderer werden dafür nicht neu erstellt.

## Tatsächlicher VSync-Pfad und Monitor

`DiligentD3D11Backend::Present` übergibt `Graphics::PresentInterval(config.vsync)`: **0 für Aus, 1 für Ein**. Der im Checkout gepinnte Diligent-Code reicht dieses Argument an `IDXGISwapChain::Present(SyncInterval, 0)` durch. Diligent führt vorher weiterhin `FinishFrame`, Resource-Unbinds und `WaitForFrame` am DXGI-Frame-Latency-Waitable-Object aus. Diese vorhandene Queue-Regel wird nicht abgeschaltet oder umgebaut.

Live ausgelesen: **NVIDIA GeForce RTX 5070 Ti**, Treiber **32.0.16.1656**, Desktop **2560 × 1440 bei gemeldeten 164 Hz** (`Win32_VideoController`). Die gemessene unlimitierte VSync-Kadenz beträgt etwa **164,8 Hz**. Die ganzzahlige WMI-Angabe ist keine exakte rationale Refresh-Messung und keine Annahme über andere Monitore. Testfenster: 1024 × 768, D3D11 windowed.

FPS-Limit und VSync überschreiben sich nicht. Bei 60-Hz-Hardware kann 120 + VSync Ein effektiv bei ungefähr 60 liegen. Windowed/DWM-/Treiber-/VRR-Verhalten beeinflusst die wirkliche Ausgabe; Present(0) garantiert keine Tearing-Freigabe und keine physisch sichtbaren 873 Bilder/s. Unterhalb der Refresh Rate kann Present(1) kurz zurückkehren, solange die Queue Platz hat. **Present-Rückkehr ist kein Scanout-Nachweis.**

## Messverfahren und Testmatrix

Finaler Release SHA256: `B8EE8712648CE9EE5FF8A3A1364D6AD4A58FC90FC519EF37F63FBB0F6F036923`.

Sechs Einstellungen im selben Prozess über die echten UI-ComboBox-Callbacks. Je Kombination vier kurze Abschnitte: Idle, native Kameradrehung, native Laufbewegung, Combo-Angriff mit originalen samyeon_d-/palbang_spin-Effekten. **Je Abschnitt 1 s Warmup + 4 s Messung**, damit **16 s gemessene Aktivität je Kombination**. Gesamter Clientlauf einschließlich Laden/Wechsel: 123,24 s. Keine parallelen Builds oder zweiten Clients im finalen Messlauf.

Die opt-in Diagnose `--frame-pacing-capture` sammelt QPC-Zeitstempel für Process-Anfang, Diligent-Present-Aufruf und Limiter. Sie reserviert einmal Speicher, ist auf 400000 Samples begrenzt und schreibt erst beim Prozessende. Finale Aufnahme: **27793 Samples, 0 Drops**. Normale Starts aktivieren diese Aufnahme nicht.

Primäre Frametime = Abstand zweier Process-Anfänge mit gültigem Present im selben Messabschnitt, entsprechend der P2-Definition. P95/P99 linear interpoliert, keine Ausreißer entfernt. Screenshots und Abschnittswechsel liegen außerhalb der Messfenster. Zusätzlich wird die Rückkehrkadenz zweier Presents angegeben. Das ist CPU-Wallclock, keine GPU-Zeit, Display-Scanout- oder Input-Latenzmessung.

### Zusammengefasste Frameintervalle

Alle Zeiten in ms. Schwellwerte sind strikt `> 8,33`, `> 16,67`, `> 20`; bei einem 60-FPS-Ziel liegen selbstverständlich alle Frames über 8,33 ms. Kleine positive/negative Timerabweichungen um den Zielwert sind keine ausgefallenen Frames.

| FPS / VSync | Intervalle | FPS Ø | Median | P95 | P99 | Max | >8,33 | >16,67 | >20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 60 / Aus | 955 | 60.0 | 16.602 | 17.055 | 17.108 | 17.405 | 955 | 358 | 0 |
| 120 / Aus | 1916 | 120.0 | 8.443 | 8.594 | 8.749 | 9.100 | 1130 | 0 | 0 |
| Unbegrenzt / Aus | 13993 | 873.1 | 1.112 | 1.329 | 1.576 | 6.238 | 0 | 0 | 0 |
| 60 / Ein | 955 | 60.0 | 16.588 | 17.059 | 17.125 | 17.405 | 955 | 351 | 0 |
| 120 / Ein | 1915 | 120.0 | 8.445 | 8.593 | 8.739 | 9.060 | 1135 | 0 | 0 |
| Unbegrenzt / Ein | 2636 | 164.8 | 6.067 | 6.119 | 6.139 | 6.200 | 0 | 0 | 0 |

### Alle 24 Abschnitte

| FPS / VSync | A1-Szenario | Intervalle | Median ms | P95 | P99 | Max |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| 60 / Aus | IDLE | 238 | 16.620 | 17.050 | 17.106 | 17.129 |
| 60 / Aus | CAMERA | 239 | 16.584 | 17.058 | 17.108 | 17.151 |
| 60 / Aus | RUN | 239 | 16.613 | 17.024 | 17.105 | 17.154 |
| 60 / Aus | COMBAT | 239 | 16.589 | 17.065 | 17.151 | 17.405 |
| 120 / Aus | IDLE | 479 | 8.425 | 8.599 | 8.746 | 8.780 |
| 120 / Aus | CAMERA | 479 | 8.469 | 8.584 | 8.760 | 8.814 |
| 120 / Aus | RUN | 479 | 8.454 | 8.589 | 8.822 | 9.100 |
| 120 / Aus | COMBAT | 479 | 8.428 | 8.604 | 8.685 | 8.783 |
| Unbegrenzt / Aus | IDLE | 3137 | 1.257 | 1.382 | 1.530 | 2.147 |
| Unbegrenzt / Aus | CAMERA | 3607 | 1.084 | 1.279 | 1.666 | 1.926 |
| Unbegrenzt / Aus | RUN | 3784 | 1.046 | 1.163 | 1.240 | 1.813 |
| Unbegrenzt / Aus | COMBAT | 3465 | 1.122 | 1.285 | 1.724 | 6.238 |
| 60 / Ein | IDLE | 239 | 16.616 | 17.029 | 17.098 | 17.126 |
| 60 / Ein | CAMERA | 239 | 16.590 | 17.038 | 17.094 | 17.111 |
| 60 / Ein | RUN | 239 | 16.572 | 17.063 | 17.123 | 17.141 |
| 60 / Ein | COMBAT | 238 | 16.585 | 17.109 | 17.277 | 17.405 |
| 120 / Ein | IDLE | 479 | 8.460 | 8.589 | 8.698 | 8.821 |
| 120 / Ein | CAMERA | 479 | 8.452 | 8.632 | 8.801 | 8.937 |
| 120 / Ein | RUN | 478 | 8.418 | 8.588 | 8.706 | 8.773 |
| 120 / Ein | COMBAT | 479 | 8.446 | 8.574 | 8.711 | 9.060 |
| Unbegrenzt / Ein | IDLE | 659 | 6.066 | 6.120 | 6.144 | 6.188 |
| Unbegrenzt / Ein | CAMERA | 659 | 6.067 | 6.117 | 6.134 | 6.164 |
| Unbegrenzt / Ein | RUN | 659 | 6.067 | 6.120 | 6.141 | 6.151 |
| Unbegrenzt / Ein | COMBAT | 659 | 6.068 | 6.115 | 6.135 | 6.200 |

### Present und Wartezeit

| FPS / VSync | Present Median | P95 | P99 | Max | Limiter Ø | Rückkehr Median | P95 | P99 | Max |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 60 / Aus | 0.038 | 0.047 | 0.069 | 0.199 | 15.175 | 16.632 | 17.271 | 17.638 | 19.588 |
| 120 / Aus | 0.038 | 0.043 | 0.058 | 0.116 | 6.938 | 8.372 | 8.893 | 9.112 | 9.463 |
| Unbegrenzt / Aus | 0.021 | 0.037 | 0.042 | 5.013 | 0.000 | 1.112 | 1.328 | 1.577 | 6.236 |
| 60 / Ein | 0.038 | 0.044 | 0.053 | 0.122 | 15.219 | 16.608 | 17.305 | 17.554 | 17.998 |
| 120 / Ein | 0.038 | 0.043 | 0.055 | 0.100 | 6.942 | 8.368 | 8.891 | 9.077 | 9.447 |
| Unbegrenzt / Ein | 4.769 | 4.930 | 4.977 | 5.032 | 0.000 | 6.067 | 6.117 | 6.138 | 6.200 |

Die als 0,000 ms gerundeten Unlimited-Werte enthalten nur Diagnose-/Scope-Aufwand; der Limiter ruft dort keine Wartefunktion auf. Auch bei der gemessenen Present-Rückkehrkadenz bleiben alle begrenzten Modi unter 20 ms; das Maximum beträgt 19,588 ms bei 60/Aus.

### P2-Present-Spitzen

Bei Unbegrenzt/Aus bleibt ein einzelner langer Present sichtbar: im längsten Combat-Intervall **6,238 ms**, davon **5,013 ms Present**, **0 ms Limiter**, etwa **1,225 ms übrige Wallclock**. Der Normalbereich ist wesentlich kleiner: zusammengefasst Present P99 **0,042 ms**. Mit VSync Ein ist das regelmäßige Warten bei Unbegrenzt erwartetes Queue-/Refresh-Backpressure: Present P99 **4,977 ms**, insgesamt etwa 164,8 FPS.

Mit den 60/120-Limits wurden in dieser A1-Matrix Present-Maxima von höchstens **0,199 ms** gemessen. Daraus folgt kein allgemeiner Treiber-Fix: P2s Traversal-/C1-Routen wurden nicht wiederholt. Die konkrete Zerlegung von Diligents `WaitForFrame` gegenüber dem anschließenden DXGI-Present ist nicht separat instrumentiert; OS-/DWM-/Treiberanteile bleiben daher kausal offen. P3 verbessert die belegte begrenzte Ausgabekadenz, beseitigt aber nicht pauschal alle möglichen Present-Ausreißer.

## FPS-unabhängige Spiel- und UI-Logik

| Bereich | Umsetzung / Prüfung |
| --- | --- |
| Bewegung | Native Actor-Update-/Transform-/MovementProcess-Kette im festen Tick; `MoveToDestPosition` läuft tatsächlich, keine vom Test pro Frame gesetzte Laufstrecke. |
| Kamera | Bestehende pro Tick arbeitende Roll/Pitch/Zoom-/Blend-Logik bleibt im festen Update. `RotateCamera(1)` wird nativ gemessen. |
| Animation / Interpolation | Native Actor-LocalTime und Pose-Sampling laufen auf derselben Spieluhr. Renderframes lesen den aktuellen Zustand. Die bestehende Clip-/Blend-Interpolation bleibt; es wurde keine neue Interpolation zwischen Spielticks ergänzt. |
| Angriffe | Native Combo-Motion, IsAttacking-Dauer und Root-Motion bleiben zeitlich gleich. Offline-Kampf-/Effekt-Smoke, keine serverbestätigte Schadensprüfung. |
| Effekte / Partikel | Effect-/Particle-Updates bleiben in der Simulation. Schnee-Deform/Emission aus dem Render- in den Updatepfad verschoben; zusätzliche Render-/Shadow-Pässe erzeugen keine zusätzlichen Emissionsschritte. |
| Waffen-Spuren | Bei identischem Spielzeitpunkt keine doppelten Spline-Stützpunkte; verhindert auch die in P2 beobachtete Division durch null bei zusätzlichen Renderframes. |
| Legacy-Schimmer | Die unveränderte Schrittweite wird einmal pro Spieltick angewendet. |
| UI-Animation | AniImageBox arbeitet weiterhin pro UI-Tick. Slot-Nummer-/Cooldown-Endeffekt-Updates aus OnRender nach OnUpdate verschoben. Chatfenster-Höhenanimation ebenfalls nach OnUpdate. |
| Timer / Cooldown | Bestehende Spiel-/Server-/Wallclock-basierte Timer behalten ihre Zeitbasis; Cooldown-Prozentanzeige liest die Spielzeit. Repeated Draws erhöhen keine Timer. |

### Native Timing-Ergebnisse

Run-Spalten beziehen sich jeweils auf den Laufabschnitt; Kamera auf Camera, Angriff und UI-Loops auf Combat. Die letzte vollständige Angriffsmotion wird vor Abschnittsende ausgewertet. Kleine Unterschiede entstehen durch die Abschnittsgrenze innerhalb eines 16/17-ms-Spielschritts.

| FPS / VSync | Run Wand s | Spiel s | Animation s | Laufstrecke | Strecke/Spiels | Kamera ° | Angriff Ø s | UI-Loops |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 60 / Aus | 4.0166 | 4.0100 | 4.0090 | 1759.48 | 438.772 | 363.0 | 1.0063 | 30 |
| 120 / Aus | 4.0082 | 4.0100 | 4.0090 | 1759.48 | 438.772 | 363.0 | 1.0063 | 30 |
| Unbegrenzt / Aus | 4.0096 | 4.0100 | 4.0090 | 1759.48 | 438.772 | 363.0 | 1.0067 | 30 |
| 60 / Ein | 4.0165 | 4.0260 | 4.0260 | 1767.12 | 438.929 | 364.5 | 1.0067 | 30 |
| 120 / Ein | 4.0001 | 3.9930 | 3.9930 | 1752.02 | 438.772 | 363.0 | 1.0063 | 30 |
| Unbegrenzt / Ein | 4.0102 | 4.0090 | 4.0100 | 1759.67 | 438.929 | 363.0 | 1.0063 | 30 |

Über alle Abschnitte: 242–244 Updates in ungefähr 4 s; maximale Spiel-/Wandzeitdifferenz **9,96 ms**. Laufgeschwindigkeit **438,772–438,929 Einheiten/Spiels** (Streuung <0,04 %); Angriffsmittel **1,0063–1,0067 s**. Das zeigt keine Beschleunigung mit Render-FPS.

Der zusätzliche kurze native Chat-Test prüft die **exakte Höhenfolge pro Update** bei 60/120/Unbegrenzt, während zwischen Updates unterschiedlich viele Renderaufrufe stattfinden: **PASS in allen drei Modi**, 17,41 s einschließlich Laden. Er verwendet die finalen UI-Quelldateien. Der größere Messlauf vor diesem letzten gezielten Chat-Fix bleibt separat unter `final-matrix` erhalten; der Chat-Test ist kein nachträglich in die Frametime-Tabelle gemischtes Sample.

## Fast Gate

| Prüfung | Ergebnis |
| --- | --- |
| Finaler Windows Release-Build | PASS |
| Gezielt: FramePacing, Presets, Custom, Invalid, LiveApply, Persistence, WriteRestart, ReadRestart | PASS, 8/8; keine lange Suite |
| Alle sechs UI-Kombinationen / Runtime-Snapshot / unveränderte Qualitätswerte | PASS |
| A1 Idle / Kamera / native Bewegung / Combo + Effekte | PASS, 24 kurze Abschnitte |
| Speichern über UI-Schließen / Reload | PASS |
| Neuer Prozess lädt Unbegrenzt + Ein | PASS, Exit 0 |
| Weiterer neuer Prozess lädt Unbegrenzt + Aus | PASS, Exit 0 |
| Finale native Chat-Animation bei drei Renderlimits | PASS |
| Diligent ERROR / FATAL | 0 / 0 |
| GPU fallback / CPU deformation | 0 / 0 |
| SkinPreparationFailures / AnimationRuntimeFailures | 0 / 0 |
| Überwachte Renderer-/Asset-/Settings-/Effekt-Ressourcen beim Shutdown | 0 in allen finalen Läufen |
| Python-Fehlerlog / Prozessende | leer / Exit 0 in allen finalen Läufen |
| Produktive EXEs / Root-Pack / Grafikconfig | Hashprüfung unverändert |

Der erste eingeschränkte Build konnte die installierten D3D11-/SDK-Header nicht sehen. Der erfolgreiche Release lief in der vorhandenen Visual-Studio-x64-Umgebung mit SDK-Zugriff. Der private Client benötigte wie die früheren Smokes einen autorisierten Desktop-Start. Beides ist von Produktfehlern getrennt dokumentiert.

## Grenzen und Belege

- **Zusätzliche Render-FPS sind keine zusätzliche Simulationsfrequenz.** Kamera/Charakterzustände aktualisieren weiterhin ungefähr 60,606-mal pro Sekunde; bei 120/Unbegrenzt können mehrere ausgegebene Frames denselben Zustand zeigen. Eine neue Render-Interpolation ist nicht Teil von P3.
- Keine Netzwerk-/Relog-/vollständige UI-Abnahme, kein serverseitiger Treffer-/Cooldown-Nachweis, kein Production Deployment. Der GO gilt für den angeforderten kurzen lokalen Fast Gate.
- Kein PresentMon-/ETW-Scanout-Nachweis und keine allgemeine Garantie identischer P95/P99 auf anderer Hardware oder anderen Maps. VSync-Kombinationen unterliegen dem realen Monitor und der Present-Kette.
- Shader-/Grafikqualität und bestehende adaptive Sichtweite wurden nicht umgestaltet. Die getestete A1-Sichtweite ist explizit 25600.
- `build-p3/build-release.log`, `tests.log`, `display.json`, `protected-before-final.json` und `integrity.json`: Build, gezielte Tests, Display und Integrität.
- `build-p3/final-matrix/`: unveränderte native Frametime-CSV, Metadaten, Gameplay-JSONL, `summary.json`, Exit-/Shutdown-Belege der sechs Kombinationen. `build-p3/final/` enthält den privaten Client, Screenshots, ursprüngliche Launch-/Hashbelege und die späteren Restart-Artefakte; die ursprünglichen Messdaten liegen ausdrücklich im Matrix-Archiv.
- `build-p3/restart-on/` und `restart-off/`: gespeicherte Erwartung, native Zustände, Prozess-/Hash-/Shutdown-Belege.
- `build-p3/ui-final/`: finaler privater UI-Stand, drei Chat-Timingprüfungen, Screenshots und sauberes Shutdown.
- Reproduzierbare Entry-/Auswertelogik: `tests/Graphics/p3_runtime_entry.py`, `tests/Graphics/analyze_p3.py`; private Vorbereitung/Start/Neustart unter `build-p3/prepare.ps1`, `run.ps1`, `restart.ps1`. Nur private Root-Packs werden erzeugt.

**Nach P3 STOP. Kein UI 2.0, kein World Editor, kein Push, kein automatisches Deployment.**
