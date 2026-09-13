# Milestone 13B – funktionalen Legacy-D3D9-Renderer entfernt

Stand: 2026-09-13. Umsetzung auf dem vorhandenen, uncommitteten M13A-Stand.
M13C wurde **nicht** begonnen. Keine Änderungen an Server, Paketen, Animation,
Granny/CPU-Skinning oder Spielregeln. Bestehender Laufzeitclient nicht ersetzt.

**Abnahmestand: M13B abgeschlossen.** Release und Debug erfolgreich; Renderer
Release/Debug jeweils 14/14. Alle 18 registrierten Release-Tests erfolgreich
geprüft: erster Gesamtlauf 17/18, der umgebungsbedingt fehlgeschlagene Zstandard-
Shelltest anschließend unverändert mit Git-sh erfolgreich (1/1).
Beide M13B-Clients sauber beendet; Benutzer bestätigt normale visuelle Abnahme,
Charakterauswahl → Ingame und mehrfaches Resize/Minimize/Restore.

Die abschließenden normalen M13A-Diligent-/Legacy-Sessions wurden auf ausdrücklichen
Wunsch übersprungen. Das bleibt eine Prüfgrenze und wird nicht rückwirkend als
bestandene M13A-Abnahme gewertet. Die nachfolgenden Ergebnisse sind neue M13B-Läufe.
Der vor Änderungen erstellte Einstiegspunkt-/State-Audit steht in
[milestone13b-audit.md](milestone13b-audit.md).

## Architektur und Aufrufketten

`WinMain -> StartupOptions -> CPythonApplication::CreateDevice ->
CreateTerrainPresentation -> DiligentD3D11Backend::Initialize ->
CreateDeviceAndContextsD3D11 / CreateSwapChainD3D11`.

`CPythonApplication::Process -> CScreen::Begin (nur CPU-Zähler) ->
TerrainPresentation::BeginFrame -> bestehende Welt/UI-Produzenten und Bridges ->
Diligent-Renderer -> TerrainPresentation::Present`.

`WM_SIZE -> TerrainPresentation::Resize + CGraphicDevice::ResizeBackBuffer`.
Letzteres ändert nur CPU-Fenstergröße, Viewport, Scissor und Bildschirmmatrix.
Nullgröße pausiert; kein D3D9-Reset und kein Verlust von CPU-Texturbindungen.

`CRenderState -> NativeStateView/CaptureNativeMaterial -> neutrale Draw-Struktur ->
Diligent PSO/SRB`. Die historischen Methoden-/Enumbezeichnungen beschreiben
teilweise weiterhin Materialwerte, aber weder einen Gerätezustand noch einen
Cache zwischen D3D9 und Diligent. Alle Standardwerte entstehen explizit auf der CPU.

## Geforderte 36 Abschlusspositionen

| Nr. | Gegenstand | Ergebnis / konkrete Grenze |
|---|---|---|
| 1 | Backend-Klassen | `EterLib/LegacyD3D9Backend.{h,cpp}` gelöscht; zusätzlich CMake-Ausschluss und Architekturtest gegen Wiederaufnahme. In Git wiederherstellbar. |
| 2 | Renderer-Auswahl | `BackendKind` enthält ausschließlich DiligentD3D11; kein Legacy-Konstruktor und kein Fallback. |
| 3 | Device-Erstellung | `GrpDevice.cpp` ohne Direct3DCreate9Ex, CreateDeviceEx, Swapchain und Caps-Abfragen. CGraphicDevice ist nur noch CPU-Kontextbesitzer. |
| 4 | Frame Lifecycle | Native BeginScene/EndScene/Present/Clear/Reset entfernt. IRenderBackend bleibt mit Initialize/BeginFrame/Clear/EndFrame/Present/Resize/Shutdown und genau einer Implementierung erhalten. |
| 5 | StateManager-Kategorien | A: Device-Owner, Gerätesynchronisation und native Dispatch entfernt. B: Matrix-/Licht-/Materialwerte erhalten. C: Bridges lesen ausschließlich explizite CPU-Werte. D: unbenutzte Shadow-/Blur-Helfer stillgelegt; alte native Testreferenzen nicht gebaut. |
| 6 | RenderStates | Keine SetRenderState-Aufrufe auf einem D3D9-Gerät. Blend/Alpha/Cull/Depth/Fog/Materialwerte bleiben als CPU-Produzentenvertrag in CRenderState. |
| 7 | TextureStageStates | Kein nativer TextureStage-Dispatch. Bestehende Combiner-/UV-Parameter bleiben CPU-Materialdaten für die bereits migrierten Shader. |
| 8 | SamplerStates | Kein nativer Sampler-Dispatch oder Device-Seed. Explizite CPU-Defaults, stageweise Save/Restore und Bindung über die vorhandenen Diligent-Sampler. |
| 9 | Draw Helper | CRenderState DrawPrimitive/Indexed/UP führen keinen Draw aus; zählen nur überlebende Kompatibilitätsaufrufe. Die anschließenden bestehenden Bridges führen die tatsächliche Submission aus. GrpBase-Standard-IB/PDT-Helfer ohne native Bufferoperationen. |
| 10 | FVF/Declarations | Kein Device-SetFVF/SetVertexDeclaration. FVF bleibt als CPU-Layoutkennung für vorhandene Dekoder; tote native Felder/Typen bis M13C. |
| 11 | Shader-Pfade | Keine nativen Shader gebunden oder erzeugt. Native Shader-Setter akzeptieren nur null; nötige SpeedTree-Konstanten bleiben CPU-Werte. Diligent-Shader unverändert. |
| 12 | Transform-State | Neues Renderer/MatrixState.h mit 11 neutralen Matrix-Slots. Sämtliche produktiven D3DTS_* und D3DTRANSFORMSTATETYPE entfernt; Save/Restore arbeitet direkt mit CPU-Matrizen. |
| 13 | LostDevice/Reset | D3D9-Lost-Device-Recovery aus Process entfernt. CGraphicDevice::Reset ist nicht mehr funktionsfähig; CScreen-Restoration führt keine Deviceoperation aus. |
| 14 | Neutrale Lifecycle-Hooks | Bestehende CreateDeviceObjects/DestroyDeviceObjects der Bild-/Font-/Buffer-/Model-/LOD-/Thing-Ketten erhalten: M13A-CPU-Ressourcenaufbau und Freigabe, kein D3D9-Reset. Diligent Resize/Shutdown bleiben zuständig. |
| 15 | Renderer-Abstraktion | IRenderBackend und TerrainPresentation bleiben erhalten; nur Legacy-Implementierung und zweiter Devicebesitzer entfallen. Keine Vulkan-/DX12-Implementierung. |
| 16 | Command Line | Ohne Argument und mit --renderer=diligent-d3d11: Diligent. Legacy, unbekannte Werte und widersprüchliche Folgen: klarer Fehler, Exit 2. Andere Argumente unverändert. |
| 17 | Config | Keine Rendererwahl in Config gefunden/eingeführt. Fenster-/Auflösungsoptionen bleiben. Diligent ist weiterhin windowed-only; dynamische Schatten und Schnee-Blur bleiben ausdrücklich unimplementiert. |
| 18 | Tests | Siehe Klassifikation unten: kein ehemaliger nativer Vergleich wird als bestanden ausgegeben. GPU-/CPU-/Lifecycle-Prüfungen ersetzt bzw. erhalten. Neuer NoLegacyArchitecture-Test. |
| 19 | Diligent OFF | Nicht mehr unterstützt. Separater OFF-Konfigurationsversuch endet erwartungsgemäß mit Exit 1 und Erklärung, dass Legacy entfernt wurde. |
| 20 | Resource Layer | UseNeutralResources ist compile-time immer true. TextureBinding besitzt CPU-Texturquellen. Native Allokationswächter blockieren jede Restanforderung; kein produktiver Legacy-Handle benötigt. |
| 21 | D3D9 Device Creations | 0 in Weltlauf und normaler Sitzung; keine Device-Fabrik im Source und kein Direct3DCreate9(Ex)-Import in der geprüften Release-EXE. |
| 22 | D3D9 Draw Calls | Alle protokollierten Frames: draws/states/texture_binds/target_changes = 0. suppressed_draws sind nur gezählte, nicht ausgeführte Kompatibilitätsaufrufe. |
| 23 | D3D9 Resources | Beide Sitzungen: attempts=0, creations=0, blocked=0 für alle zehn erfassten Ressourcenarten. |
| 24 | Login | Normaler Default-Client PID 16528 mit Originalpaketen gestartet und Ingame gelangt; Benutzer bestätigt normale Clientabnahme. |
| 25 | Character Select | Benutzer bestätigt Charakterauswahl sowie Charakterauswahl → Ingame ausdrücklich. |
| 26 | Ingame | Welt-, Actor-, Attachment-, Mount-, Tree-, Effect-, UI-/Textpfade protokolliert; normale Sitzung Exit 0. Benutzer bestätigt Ingame, UI/Schrift und Questdialog. |
| 27 | World Regression | PID 63548: A1 → B1 → A1 → monkeydungeon → guild_01 → A1, completed phases=6, 151,1 s, Exit 0. Sechs Screenshots. |
| 28 | Resize | GPU-Tests mehrfach 320/640/800/1024 sowie Nullgröße und Wiederherstellung; Pixel/Depth und Ressourcen geprüft. Benutzer bestätigt mehrfaches Resize im normalen Client. |
| 29 | Minimize/Restore | Weltlauf dreimal vom Benutzer ausdrücklich bestätigt. GPU-Suspend/Restore ebenfalls erfolgreich. Normale Sitzung ebenfalls vom Benutzer bestätigt. |
| 30 | Relog | Charakterauswahl → Ingame vom Benutzer bestätigt. Kein zusätzlicher automatisierter Netzwerk-Relog oder Langzeittest. |
| 31 | Shutdown | Welt 151,1 s und normale Sitzung 146,0 s: Exit 0; SourceTextures/SourceBuffers und sämtliche protokollierten Rendererressourcen jeweils 0. |
| 32 | Exitcodes | Welt 0; normal 0; Legacy-Parameter 2; unbekannter Parameter 2; abgefangener Initialisierungsfehler 4. Neue Laufzeitfehlerkennung meldet fehlgeschlagene Frames/Resize als 5 statt 0. |
| 33 | Builds | Release und bestehende Debug-Konfiguration erfolgreich, x64. Vorhandene Compiler-/PDB-Warnungen bleiben; OFF ist absichtlich kein Buildziel mehr. |
| 34 | Testsuite | Renderer Release/Debug jeweils 14/14. Gesamter Release-Lauf 17/18; playTests mit korrigierter Shellumgebung 1/1, somit alle 18 registrierten Tests erfolgreich ausgeführt. Details unten. |
| 35 | Rest-Compile-Abhängigkeiten | d3d9.h/d3dx9.h, D3DX-Mathe/Matrixstack, einige native Typen/Felder, D3DRS/D3DTSS/D3DSAMP/FVF-Datentokens sowie Linkerlibs bleiben. Release-Import enthält d3dx9_43.dll und d3d11.dll, keine D3D9-Gerätefabrik. |
| 36 | Empfehlung M13C | Erst nach vollständiger M13B-Abnahme gezielt Resttypen/-Header/-Libs und tote Kompatibilitätssignaturen bereinigen. Kein Rendererwechsel, keine neue Bildfunktion. M13C hier nicht begonnen. |

## Tests: erhalten, angepasst, entfallen

| Klasse | Behandlung |
|---|---|
| A – Architektur | StartupOptions auf ausschließlich Diligent umgestellt. NativeStateView heißt als CTest nun CpuDrawState: explizite CPU-Defaults, verschachtelte Save/Restore-Isolation, Textur-Lifetime, Licht/Viewport/Scissor/Tree-Konstanten und fünf wirkungslose native Draw-Signaturen ohne Device. |
| A – neue Schutzgrenze | NoLegacyArchitecture prüft Produktionsquelle auf Legacy-Fabrik/-Klassen, native Frame/Draw/State-Dispatches, D3D9-Matrix-Slots und rückkehrenden Devicezugriff aus CPU-State/View; Resource-Routing muss unveränderlich neutral sein. Ergänzt, ersetzt nicht die Runtime-Nachweise. |
| B – D3D9-only | Renderer.LegacyD3D9 entfernt: sein Produktbackend existiert nicht mehr. Keine Testumgehung durch Dummy-Legacybackend. |
| B/C – GPU-Parität | TerrainGpuParity kann ohne native Referenz nicht weiter dieselbe Aussage liefern. Der alte TerrainGpuTest.cpp und seine nativen Paritäts-Header bleiben als ungebautes historisches Material; sie werden nicht als grün gewertet. Neuer ProductionGpu benötigt keinerlei D3D9-Gerät. |
| C – GPU erhalten | Diligent Clear-Pixel, Depth-only-Clear, Screenshot-RGB, Resize, Suspend/Restore, doppelter Shutdown/Reinitialize; realer Terrain-CPU-Produzent, vier Kameraposen, überlappende Patches und orderunabhängige Depth. |
| C – Actor erhalten | ActorGpuChecks und ActorStateIsolationChecks: Multiinstanz-/Pose-/Submesh-/Texture-Lifetime, Attachments/Mount-Rider-Teardown, 720 Materialreihenfolgen und 12 Pipelinevarianten. ActorGpuChecks erhielt nur einen neutralen Kamera-Templateparameter. |
| C – Presentation erhalten | Tatsächlicher TerrainPresentation-Besitzer mit texturiertem Terrain, Actor-World-Gate, Größenwechseln, Pause/Resume, Rückkehr zu Loginframe und vollständigem Unbinding/Shutdown. |
| C – neue Diligent-Orakel | Alle sechs WorldPart-Pfade mit Alpha/Blend/Fog und sichtbaren Pixeln; UI/Text-Scissor mit expliziten Innen-/Außenpixeln; Ressourcen nach Shutdown 0. |
| C – unverändert | Actor/Tree/Effect/World/UI/Text-Policy, TerrainTextureCpu und ScreenshotJPEG; ResourceSource-Checks für neutrale Texturen, Buffer, Alpha und GPU-Upload bleiben, Kompatibilitätsdevice entfernt. |
| C – Bibliotheken | fullbench, fuzzer, zstreamtest und playTests nicht entfernt oder verkürzt. Gesamte registrierte Suite wird ausgeführt. |

**Geänderte Abdeckung:** Keine native D3D9-Pixelreferenz mehr für Splat-/Static-/
Actor-/Tree-/Effect-/UI-/Text-/Floating-/Damage-Parität. Spezifische alte
Pixelvergleiche entfallen mit dieser Referenz; sie wurden nicht vollständig in
analytische Diligent-Orakel übersetzt. Das ist eine dokumentierte Verringerung der
Cross-Backend-Paritätsabdeckung, keine bestandene Paritätsprüfung. Unabhängige
CPU-/Diligent-Invarianten und reale Produktionsläufe bleiben das M13B-Orakel.

Während der Testumstellung korrigiert: UI-Testquad brauchte seine orthografische
Testmatrix. Der zusätzliche Debug-Test deckte einen doppelten Kamera-Singleton im
Testaufbau auf (Camera.cpp besitzt bereits einen); nach Entfernung des zweiten
Testobjekts bestand Debug inklusive aktiver Assertions. Keine Assertion deaktiviert.

## Runtime-Evidenz

Alle Ordner relativ zu `build/milestone13b`, ausschließlich private Testkopien.

| Testordner | PID | Prozessresultat |
|---|---:|---|
| world-final/default | 63548 | 0, 151,1 s, alle sechs Phasen |
| normal-final/default | 16528 | 0, 146,0 s, Originalpakete |
| reject-legacy/default | 47984 | 2, vor Spiel-/Deviceinitialisierung |
| reject-invalid/default | 51236 | 2, vor Spiel-/Deviceinitialisierung |
| reject-init/default | 16040 | 4, kein Fallback trotz von Python abgefangenem Fehler |
| explicit-final-smoke | 51204 | 0, explizites Diligent, Initialize + Resize + 5 Frames + Shutdown |

Pro Lauf: `*-exit.txt`, `*-resources.csv`, `*-runtime/renderer-startup.log`,
`native-resource-audit.log`, `native-render-audit.log`, `terrain-renderer.log`.
Welt zusätzlich `special-world-test.log` und `m11-world-00…05-*.jpg`.

- Welt: private Speicherbelegung maximal 425,3 MiB, zuletzt 359,2 MiB;
  Handles maximal 927, zuletzt 915. Keine stetig steigende Kurve über die
  sechs Szenenwechsel; begrenzte Laufdauer, kein Langzeit-Leaknachweis.
- Normale Sitzung: maximal 506,3 MiB, zuletzt 494,7 MiB; Handles maximal 963,
  zuletzt 953. Alle Renderer-/CPU-Besitzer nach Shutdown 0.
- Keine nativen Allokationsverletzungen und keine Renderer-/Devicefehler in den
  beiden Release-Clientläufen gefunden.
- LegacyRendererInitializations=0 ist zusätzlich durch die nicht mehr vorhandene
  Klasse/Fabrik abgesichert, kein dynamischer Auswahlpfad mit verstecktem Fallback.
- Native Geräteslots bleiben null; GPU-Tests prüfen dies ausdrücklich. Die
  Ressourcenwächter zählen Versuche unabhängig vom Erfolg.

**Fehlerfallgrenze:** Initialisierung mit ungültigen Parametern wird im Backendtest
geprüft. Der echte Prozess-Exit-4-Test nutzt den bestehenden nicht unterstützten
Fullscreenmodus. Kein Hardwareausfall, fehlender Treiber oder Device-Removal
wurde künstlich erzeugt. Der Laufzeit-Exit-5-Zweig ist implementiert, aber nicht
durch absichtlichen GPU-Ausfall im normalen Client ausgelöst.

## Build-/Testprotokolle und Prüfgrenzen

- Finale Gesamtbuilds: `release-verified.log`, `debug-verified.log`, beide Exit 0.
- Gesamtsuite: `full-release.log`, 690,87 s, erster Lauf Exit 8 (17/18).
  fullbench 148,44 s, fuzzer 431,70 s, zstreamtest 97,63 s: bestanden.
- playTests scheiterte zunächst, weil Cygwin `/cygdrive/c/…` an die native
  Windows-zstd.exe übergab. Dieselbe unveränderte CTest-Prüfung mit bereits
  installiertem `C:/Program Files/Git/usr/bin/sh.exe` bestand in 23,98 s.
  Nur PATH des Testprozesses geändert, keine globale Konfiguration und kein
  Vendor-Code. `playtests-git-shell.log` und `playtests-git-shell-details.log`.
- Debug: `renderer-debug.log` / `renderer-debug-details.log`, 14/14, Exit 0.
  Release: alle 14 Rendererfälle im Gesamtlauf erfolgreich, einschließlich
  erweitertem ProductionGpu (Depth-Reihenfolge, texturierter Presentation-Lifecycle).
- Erster Debug-GPU-Versuch: Assertion wegen des doppelten Test-Kameraobjekts,
  Exit 3 nach Abbruch; Korrektur nur in DiligentGpuTest.cpp. Wiederholung Exit 0,
  danach gesamter Debug-Renderersatz grün.
- OFF: `off-rejection.log`, erwarteter Konfigurationsfehler, Exit 1.
- Importnachweis der tatsächlich live getesteten EXE: `production-imports.txt`.
  Welt und normale Sitzung verwendeten dieselbe SHA256:
  `EE14227C7DFF3C2DFCE40D8D45726BBC539F117A94F9B01DFF90F369867B48B6`.
  Danach keine funktionale Produktionsänderung: nur vier Einrückungen,
  GPU-Testergänzungen und Dokumentation; abschließende Builds erneut erfolgreich.
- Benutzer bestätigt die angeforderten normalen Hauptprüfungen insgesamt mit
  „ja“. Seltene Varianten einzelner Bodenitems, Damage-Arten oder sämtliche
  Quests sind dadurch nicht erschöpfend getestet. Kein mehrstündiger Soak-Test.
- `git diff --check` erfolgreich. Keine M13B-Testprozesse mehr aktiv.

## Git-Diff und unveränderte Grenzen

Vorhandene M13A-Änderungen wurden bewahrt; der Gesamtdiff gegen HEAD enthält daher
**M13A und M13B**, nicht nur diese Implementierung. Kein Commit/Push/Reset/Stash.
Gesamter getrackter Diff gegen HEAD: 84 Dateien, +871/-2471 Zeilen, zusätzlich die
neuen M13A/M13B-Ressourcen-, Test- und Dokumentationsdateien. Ein beim direkten
GPU-Test erzeugtes Root-Protokoll wurde in den M13B-Belegordner verschoben.

M13B-Schwerpunkte: Backend-Dateien löschen; StartupOptions/Bootstrap/Application
vereinfachen; GrpDevice/GrpScreen/GrpBase auf CPU-/Diligent-Verantwortung begrenzen;
StateManager als CRenderState ohne Gerätebesitz; neutrale Matrix-Slots und
mechanische Produzentenanpassungen; Windows-Displayenumeration statt D3D9;
native Shadow-/Blur-Lifecycles deaktivieren; immutable Resource-Routing;
Testklassifikation, neue GPU-/Architekturprüfungen und private Startfall-Runner.

Keine neue Darstellung, kein Granny-/Skinning-Umbau und kein globales Entfernen
von SDK-Headern, Libraries, Enumtokens oder Typedefs. Alte Schatten-Configwerte
erzeugen keine dynamischen Schatten. Der produktive bisherige Client und seine
Pakete wurden nicht überschrieben; M13B ist als Build und private Testkopie vorhanden.
