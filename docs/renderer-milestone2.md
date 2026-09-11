# Milestone 2: Diligent-Terrain-Geometrie

Historischer Abschlussstand. Die nachfolgende einzelne Terrain-Textur ist separat
in [Milestone 3A](renderer-milestone3a.md) dokumentiert; die untenstehenden
M2-Ergebnisse beziehen sich weiterhin auf den damaligen einfarbigen Renderer.

Stand: 11.09.2026. Source: `C:\Users\ZiiNAN\Documents\GitHub\m2dev-client-src`.
Basis: vorhandener, noch uncommitteter Milestone 1; HEAD `a7555110828182f20d0a0306aac0552142cf0039`.

Implementiert ist ausschließlich einfarbige Terrain-Geometrie. Legacy D3D9Ex bleibt
Standard. Kein Commit, Push, Deployment oder Milestone 3. Die vorab dokumentierte
[Analyse](renderer-milestone2-analysis.md) beantwortet die 19 Architekturfragen.

## Start und bewusste Integrationsgrenze

```text
Metin2_Release.exe                           -> bisheriger Legacy-Client
Metin2_Release.exe --renderer=diligent-d3d11  -> normaler Client, experimentelles Terrain
```

Beide benötigen die bestehenden Runtime-Daten als Arbeitsverzeichnis. Diligent
muss mit `M2_ENABLE_DILIGENT_D3D11=ON` gebaut sein; OFF bleibt CMake-Default.
`--renderer-smoke-test` startet weiterhin den isolierten Lifecycle-Test aus M1.
Die Auswahl erfolgt ausschließlich beim Start.

Noch nicht portierte Spielressourcen und Login benötigen D3D9. Deshalb bleibt das
Kompatibilitätsgerät auch beim Diligent-Start bestehen. Ein eigenes, deaktiviertes
Kindfenster erhält die D3D11-Swapchain. Es wird in Welt-Terrainframes eingeblendet
und ohne Terrain wieder ausgeblendet. Eingaben gehören weiterhin zum Elternfenster.
Es gibt keine gemeinsamen D3D9/D3D11-Texturen und keine neue UI-Komposition.

**Einschränkungen:** experimenteller Diligent-Pfad nur im Fenstermodus. Im Terrainbild
sind UI, Charaktere, Wasser, Gebäude und Bäume nicht enthalten. Der bisherige
D3D9-Renderdurchlauf läuft dahinter weiter. Das ist eine Übergangslösung zur
risikoarmen Geometrie-Verifikation, kein vollständiger D3D11-Spielclient und keine
Performance-Optimierung. Terrain-Texturen, Splatting und alle ausgeschlossenen
Subsysteme wurden nicht portiert.

## Dateien und Verantwortlichkeiten

Alle folgenden Codepfade sind relativ zu `src/`.

| Neue M2-Dateien | Aufgabe |
| --- | --- |
| `Renderer/TerrainRenderData.h` | Diligent-freie Upload-/Draw-Grenze, Matrixdaten, besitzende Bufferhandles |
| `Renderer/DiligentTerrainRenderer.h/.cpp` | Immutable VB/IB, Matrixbuffer, VS/konstanter PS, zwei PSOs, DrawIndexed |
| `Renderer/TerrainPresentation.h/.cpp` | Kindfenster, Frame-Lifecycle, Resize, Sichtbarkeit, Besitzer-Lifetime |
| `../tests/Renderer/TerrainGpuTest.cpp` | D3D9/D3D11-Readback, Kamera, Depth, Resize, Ressourcen-/Präsentationstest |
| `../tests/Renderer/terrain_map_smoke.py` | Isolierter Test mit echten Maps und vorhandener Anwendungskamera |
| `../docs/renderer-milestone2-analysis.md`, diese Datei | Vorab-Analyse und Abschluss-/Verifikationsbericht |

| Angepasste Dateien | Minimaler Änderungspunkt |
| --- | --- |
| `GameLib/TerrainPatch.h/.cpp` | GPU-Handle; Upload am bestehenden CPU-Vertexübergabepunkt; Freigabe in Clear |
| `GameLib/MapOutdoor.h/.cpp` | Drei Indexhandles, bestehende LOD merken, Freigabe mit Map |
| `GameLib/MapOutdoorIndexBuffer.cpp` | Bestehende temporäre WORD-Arrays vor deren Freigabe hochladen |
| `GameLib/MapOutdoorRender.cpp` | Tatsächliche Matrizen/States abgreifen; neutraler Submit hinter bestehender Auswahl |
| `GameLib/MapOutdoorRenderHTP.cpp`, `MapOutdoorRenderSTP.cpp` | Je ein zusätzlicher Submit in Splat-/None-Patchpfaden |
| `UserInterface/PythonApplication.h/.cpp` | Startup-Auswahl halten, zusätzliche Präsentation vor Maps erzeugen, Frames/Shutdown anbinden |
| `UserInterface/PythonApplicationProcedure.cpp` | WM_SIZE an die optionale Präsentation; nur bei Diligent Fenster-X/WM_CLOSE in reguläres Exit übersetzen |
| `UserInterface/UserInterface.cpp` | Diligent-Normalstart durch bestehende Anwendung; Smoke-Bootstrap nur explizit |
| `Renderer/DiligentD3D11Backend.h` | Privaten Zugriff für den Terrain-Renderer erlauben |
| `Renderer/CMakeLists.txt`, `../tests/Renderer/CMakeLists.txt` | Optionale Implementierung und zusätzlichen Test einbinden |
| `../docs/renderer-milestone1.md` | Historisches Startverhalten kennzeichnen und auf M2 verweisen |

Die Änderungen an der Terrain-Erzeugung selbst, den Indexalgorithmen, der Frustum-
und Distanzprüfung sowie den bestehenden D3D9-Drawcalls sind **null**. Die Hooks
ergänzen nur Upload und Submission. CStateManager, Granny und Runtime-UI-Dateien
wurden nicht umgebaut. Die Test-Pythondatei ist kein Bestandteil des normalen Root-Packs.

## Exakte neue Call-Chains

```text
MapManager::LoadMap -> MapOutdoor::Load -> Update / LoadTerrain
 -> CTerrain::CalculateTerrainPatch -> _CalculateTerrainPatch
 -> CTerrainPatch::BuildTerrainVertexBuffer
 -> ITerrainRenderer::UploadVertices -> Diligent immutable VB

MapOutdoor::SetIndexBuffer
 -> vorhandene drei WORD-Arrays -> ITerrainRenderer::UploadIndices -> immutable IB

CPythonApplication::Process
 -> Legacy Begin/Clear + TerrainPresentation::BeginFrame (D3D11 Begin/Clear)
 -> OnUIRender -> bestehender Python-Callback -> CPythonApplication::RenderGame
 -> CCullingManager::Process (bestehende View/Projection aktualisieren)
 -> CPythonBackground / CMapManager::Render -> CMapBase::Render
 -> CMapOutdoor::OnRender -> RenderTerrain
 -> vorhandene Frustum-/Quadtree-Auswahl -> vorhandenes Sortieren
 -> BeginTerrain (World/View/Projection und Legacy-State-Prüfung)
 -> HTP oder STP -> bestehende Distanz-/Splatlimits -> SelectIndexBuffer
 -> RenderPatchSplat oder RenderPatchNone -> SubmitTerrainGeometry
 -> DiligentTerrainRenderer::DrawTerrain -> SetVB/IB/PSO/SRB -> DrawIndexed
 -> Legacy End/Present -> TerrainPresentation::Present -> D3D11 End/Present
```

Kein zweiter Maploader, keine zweite Patch-Auswahl, keine neue Kamera. Auch die
bestehende adaptive Sichtweite und LOD-Entscheidung bleiben wirksam; deshalb können
Drawzahlen zwischen getrennten Läufen mit unterschiedlicher Bildrate variieren.

## Daten, Konventionen und Pipeline

- `CTerrainImpl` besitzt Height-/Normal-/Tile-Rohdaten; `CTerrain` besitzt 8×8
  `CTerrainPatch`. `TTerrainSplatPatch` enthält Textur-/Splat-Metadaten, keine Geometrie.
- Originaler `HardwareTransformPatch_SSourceVertex`: Position float3 bei Offset 0,
  Normal float3 bei Offset 12, Stride 24, 17×17 = 289 Vertices pro Patch. Normalen
  bleiben unverändert im Buffer, werden vom minimalen Shader nicht gelesen.
- X positiv, Map-Y wird zum negativen Render-Y; Z = bestehende WORD-Höhe × HeightScale.
  Zellabstand 200, Patchbreite 3200. Keine neue Höhenberechnung.
- Upload erfolgt synchron aus den vorhandenen CPU-Arrays. Keine neue dauerhafte
  CPU-Kopie. Vertices pro Patch; drei gemeinsame Indexbuffer pro Map. Indexformat
  UINT16. LOD0 TriangleStrip inklusive ursprünglicher Degenerates; LOD1/2 TriangleList.
- World entspricht HTP-World ohne Texture-Offset, beim STP Identität. View/Projection
  kommen aus dem vorhandenen StateManager nach Aktualisierung durch die echte Kamera.
  Rechtshändig, Z-Up, row-major, Zeilenvektoren, `position * World * View * Projection`.
  HLSL markiert alle drei Matrizen explizit `row_major`; kein Transponieren, kein Y-/Z-Fixup.
- Cull: BACK; FrontCounterClockwise=true entspricht Legacy D3DCULL_CW. Solid,
  DepthClip=true. DepthTest/Write=true, LESS_EQUAL, ClearDepth=1, NDC-Tiefe 0..1.
- Zwei PSOs für Strip/List; RGBA8_UNORM und D24_UNORM_S8_UINT. Kein Blending, Stencil,
  DepthBias, Licht, Fog, Textur oder Schatten. PS-Farbe RGBA=(0.72,0.82,0.38,1).
- Patch-Clear gibt den VB frei, Map-Clear die IBs. Maps/Handles werden vor dem
  Präsentationsbesitzer, dessen PSOs und dessen Gerät zerstört. Upload-/Statefehler
  werden erkannt und beenden den experimentellen Lauf mit Fehlerdiagnostik.

## Verifikation

Release x64, Visual Studio 2022, Windows SDK 10.0.26100. Beide vollständigen Builds
ON und OFF: **0 Fehler**. Je 236 bestehende Linkwarnungen wegen fehlender Python-/zlib-
PDB-Dateien; keine Vendor-Änderung zur Unterdrückung. Logs unter `build/`:
`milestone2-build-final.log`, `milestone2-build-off.log`.
Nach der beim Nutzertest gefundenen Close-Lücke wurden beide vollständigen Builds
nochmals erfolgreich ausgeführt: `milestone2-close-build-on.log`,
`milestone2-close-build-off.log`, `milestone2-close-build-final.log`.
Die Test-Builds nutzten vorübergehend einen anderen Ausgabeordner, weil die alte
EXE noch lief. Anschließend wurde der normale CMake-Ausgabepfad wiederhergestellt.
Finale EXE: `build/bin/Release/Metin2_Release.exe`, 29.554.688 Bytes,
SHA256 `8A83D304949BFF7563F4408721FE5A400610532BBA6265FE3C3EB89DC9F446E5`.

Bestehende Tests wurden nicht entfernt oder deaktiviert. OFF-Renderer-Tests: 2/2.
Die finale ON-Suite umfasst die sieben vorhandenen Tests plus TerrainGpuParity:
**8/8 bestanden**, 467,40 s Gesamtlaufzeit. Fuzzer: 467,38 s; fullbench: 143,83 s;
zstreamtest: 116,93 s; playTests: 25,56 s. StartupOptions und beide bisherigen GPU-
Tests ebenfalls bestanden. Log: `build/milestone2-final-tests.log`. Die danach
erweiterte Präsentationsprüfung wurde zusätzlich separat erfolgreich wiederholt
(`build/milestone2-terrain-gpu-test.log`). Diese vollständige Suite lief vor der
abschließenden kleinen Close-Korrektur. Danach wurden die betroffenen Renderer-Tests
erneut ausgeführt; die unveränderten Zstd-Tests wurden nicht nochmals gestartet.
Finale Renderer-Regression: **4/4 bestanden**, 1,24 s; inklusive TerrainGpuParity
(0,41 s), Legacy und Diligent. Log: `build/milestone2-close-renderer-tests.log`.
Auch die OFF-Konfiguration bestand danach erneut ihre zwei Renderer-Tests.

`TerrainGpuParity` inklusive letzter Erweiterung: bestanden, 0,44 s.

| Prüfung | Ergebnis/Evidenz |
| --- | --- |
| D3D9/D3D11-Kamera-/Silhouettenvergleich | Vier bestehende Kameraposen, 640×480/800×600; 236002 / 225349 / 337690 / 342737 bedeckte Pixel |
| Rasterkanten | 270 / 630 / 612 / 603 abweichende Randpixel; innerhalb des dokumentierten Half-Pixel-Toleranzbereichs |
| Depth | GPU-Readback: Terrain schreibt Tiefe; nahe/ferne überlappende Patches liefern in beiden Zeichenreihenfolgen denselben Depthbuffer |
| Cull, Strip/List | Frontflächen sichtbar, beide PSOs gezeichnet; keine invertierte Orientierung im Vergleich |
| Ressourcen | Echter Patch-Upload; Handle nach Patch-Clear freigegeben; Präsentation bindet/unbindet die Terrain-Grenze |
| Präsentations-Resize | 640×480 → 800×600 → 320×240 → 1024×768 → 0×0 → 640×480; Begin/Draw/Present und Rückkehr zum Login-Frame erfolgreich |
| Bestehender Lifecycle | Legacy-/Diligent-Smoke-Tests inklusive Clear, Resize, Suspend/Resume und Shutdown bestanden |

### Echte Maps und normaler Start

`terrain_map_smoke.py` läuft ausschließlich in einem separaten Test-Root-Pack unter
`build/milestone2/runtime`. Alle anderen Packs verwenden die echten vorhandenen
Runtime-Daten. Es verwendet `background.LoadMap`, `app.RenderGame` und die vorhandenen
`app.SetCamera` / `SetCenterPosition`-Schnittstellen. Keine Netzwerkzugangsdaten,
keine Serveränderung und keine Ersatz-Terrain-Erzeugung.

90-Sekunden-Sequenz: A1 mit Drehung, Zoom und Positionswechsel; danach B1 und zurück
nach A1. Beide finalen Läufe (Legacy-only-Build ohne Argument und Diligent-ON-Build
mit Opt-in) endeten mit **Exitcode 0**. Syserr der Mapläufe leer. A1-Höhe am Testpunkt
(50000,60000) = 18116,5; zweiter A1-Punkt = 19957,0625; B1 = 19136,5. Nach Rückkehr
nach A1 wieder 18116,5 und gültige Patches. Legacy blieb sichtbar texturiert;
Diligent zeigte grüne Terrain-Silhouetten auf blauem Clear-Hintergrund.

Repräsentativer Diligent-Lauf: 13 Draws bei steiler Pose, 19 bei der versetzten
A1-Pose, 21 auf B1, 20 nach A1-Rückkehr. Im selben versetzten A1-Testabschnitt meldete
der vorhandene Zähler 19 Patches / 84 Splats; Diligent meldete 19 Geometrie-Draws.
`terrain-renderer.log` enthält höchstens alle 120 Frames und bei Sichtbarkeitswechseln
eine Zeile. Logs liegen im jeweiligen Test-Arbeitsverzeichnis, nicht in der Konsole.

Der erste Testentwurf verwendete einen falschen Y-Parameter bzw. keinen vollständigen
Game-Phase-Abbau. Das wurde **nur im Test** korrigiert: Python-SetCenterPosition
benötigt Render-Y; Mapwechsel und Ende folgen `GameWindow.Close` mit
`background.Destroy` und erneutem `background.Initialize` vor dem nächsten Load.
Die Testfixture benötigte außerdem die vorhandenen Timer-/Kamera-Singletons.
Frühere Fehlversuche sind keine erfolgreichen Tests; maßgeblich sind die finalen Läufe.

Normaler Start mit unveränderten Root-Packs ebenfalls geprüft: sowohl Default-Legacy
als auch opt-in Diligent erreichen die bisherige Serverauswahl. Diligent bleibt dort
bei terrain=0/draws=0 und verdeckt den Login nicht. **Der Nutzer hat sich anschließend
im Diligent-Client eingeloggt und Map1 geladen**, ausdrücklich im Chat bestätigt.
Die Diagnose dieses normalen Laufs meldete terrain=1/draws=20. Zugangsdaten wurden
vom Agenten weder eingegeben noch übernommen. Die kontrollierten 90-Sekunden-Maptests
sind davon getrennte lokale Integrationsprüfungen. Das normale Start-Log
enthält einen Eintrag `invalid idx 0` aus der bestehenden Guild-Mark-Verwaltung
(`UserInterface/MarkManager.cpp`); keine Änderung daran im Terrain-Milestone.

### Manuelle Abnahme und visuelle Grenzen

- Computer-Use-Eingaben zum Minimieren/Schließen griffen nicht zuverlässig. Diese
  Versuche werden ausdrücklich nicht als bestandene manuelle Windows-Prüfung gewertet.
  Der Nutzer hat die erbetene manuelle Fensterprüfung anschließend mit "ja" bestätigt.
  Die automatisierte Suspend-/Resume-/Resize-Prüfung ist unabhängig davon bestanden.
- Das normale Clientfenster hat kein freies Resize/Maximieren; dafür wurde weder
  dessen Stil noch die UI geändert. Mehrfache Größenwechsel wurden direkt an Backend
  und tatsächlichem Terrain-Präsentationsbesitzer getestet.
- Der normale Legacy-Start-Testclient ist ebenfalls mit Exitcode 0 beendet. Der
  Nutzer fand beim anschließenden Live-Map1-Test eine echte Lücke: Das unportierte
  Exit-Menü liegt hinter dem Terrainfenster. Deshalb wurde ausschließlich bei
  vorhandener Diligent-Präsentation HTCLOSE/WM_CLOSE an `CPythonApplication::Exit`
  angebunden. Es folgt der normale Weg `app.Loop -> mainStream.Destroy ->
  GameWindow.Close -> background.Destroy`, keine Änderung an der UI und kein
  erzwungener Prozessabbruch im Produkt. Legacy behält seinen bisherigen Handler.
  Windows verweigerte dem Agenten trotz Nutzerfreigabe die Terminierung des alten
  Testprozesses. Dieser wurde anschließend extern beendet (Exitcode 1); dies ist
  ausdrücklich kein Nachweis eines sauberen Shutdowns.
  Reguläres Shutdown mit entladenen echten Maps ist bereits für beide Backends mit
  Exitcode 0 belegt. **Der Nutzer schloss anschließend den korrigierten echten
  Terrain-Test über Fenster-X und bestätigte den Erfolg.** Prozess 57248 endete
  während Pose 2, deutlich vor dem automatischen 90-Sekunden-Ende, mit Exitcode 0;
  das Log bestätigt die normale Loop-Rückkehr und den Mapabbau, Syserr blieb leer.
  Keine formale Live-Object-Leak-Analyse mit D3D-Debug-Layer durchgeführt.
- In den betrachteten Ansichten keine offensichtlichen versetzten Patches, Spiegelung
  oder Terrainlöcher. Konstante Farbe verdeckt interne Kanten; dies ist keine exhaustive
  Prüfung aller Maps/LOD-Übergänge. GPU-Parität ist quantitativ an der kontrollierten
  Fixture geprüft, echte Maps visuell und anhand der unverändert übernommenen Daten.
- D3D9/D3D11-Rasterkanten können um Pixel differieren. Fehlende Texturen, Fog, Schatten,
  Weltobjekte und UI im Diligent-Bild sind die ausdrücklich gewählte Scope-Grenze.

## Technische Schulden und Git-Diff

Keine Optimierungen eingebaut. Für separat beauftragte spätere Arbeiten bleiben:
D3D9-Kompatibilitätsarbeit ablösen, Darstellung unportierter Inhalte planen,
Fenster-/Vollbildunterstützung erweitern, umfassendere Map-/Treiber-Abnahme und erst
danach Texturen/Splatting. Ein global gebundener neutraler Terrain-Empfänger passt
zum bestehenden Single-Application-Lifecycle, ist aber keine Multi-Client-Abstraktion.

Der Git-Diff gegen HEAD enthält **M1 und M2**, weil M1 bereits uncommittet vorlag.
Die acht GameLib-Dateien enthalten zusammen 50 zusätzliche Zeilen für Handles,
Matrixübergabe und Submission, keine entfernten Legacy-Drawcalls. Ergänzt sind die
isolierten Renderer-/Testdateien und die genannten Start-/Lifecycle-Hooks. Bestehende
M1-Arbeit bleibt erhalten. `git diff --check` ohne Whitespace-Fehler; Git meldet nur
die vorhandene LF→CRLF-Konvertierung. Keine Runtime-Packs, Serverdateien oder
Granny-Implementierungen im Produkt geändert. **Hier endet Milestone 2; kein M3.**
