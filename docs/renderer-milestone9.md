# M9 – nicht-textuelle Original-UI über Diligent

Aktualisierung 12.09.2026 im ausdrücklich beauftragten M10A: Der fehlende Basistext ist implementiert.
Die normale Diligent- und Legacy-Ingame-Abnahme wurde vom Nutzer nachgeholt und bestätigt;
Details und erfolgreich abgeschlossene finale ON/OFF-Prüfung: [renderer-milestone10a.md](renderer-milestone10a.md).
Minimap/Atlas, Auswahl-3D-Vorschau und andere dokumentierte Sonderpfade bleiben ausgeschlossen.
Separater Netzwerk-Relog und normale Shop-/Storage-Erreichbarkeit wurden nicht separat bestätigt.

## Historischer M9-Abschlussstand vor M10A

Basis `618d75f`. Zu diesem Zeitpunkt war die normale Ingame-Abnahme durch die fehlende Schrift blockiert.
Nutzerbestätigung: „kann ich nicht ohne schrift“. Die nachfolgenden M9-Zahlen und damaligen offenen Punkte
sind als historischer Nachweis erhalten; die M10A-Abnahme oben ersetzt den Readability-Blocker.

Damals galt: Keine weitere Anmeldung ohne sichtbare Schrift verlangen; ein Textpfad benötigt eine
gesonderte Umfangsfreigabe. Diese Freigabe erfolgte anschließend mit dem separaten M10A-Auftrag.

Der vor Änderungen erstellte 35-Punkte-Audit und die Komponentenmatrix stehen in `renderer-milestone9-analysis.md`.

## Architektur und deterministischer Bind

`CPythonApplication::Process → Legacy BeginFrame → TerrainPresentation::BeginFrame → SetInterfaceRenderState → CWindowManager::Render → bestehende Python/C++-Widgets → Image/Expanded/Mark/CScreen/PythonGraphic → unveränderter nativer Draw + UIRenderBridge::Submit → IUIRenderer → DiligentUIRenderer → DiligentEffectRenderer-Binder → vorhandene Diligent-Oberfläche → Present`.

`game.py::OnRender → app.RenderGame` bleibt an seiner ursprünglichen Stelle. `SetGameRenderState` beendet den UI-Modus, `SetInterfaceRenderState` stellt ihn wieder her. Mouse/Cursor bleiben hinter der Window-Liste. Text, NumberLine, TextTail und der komplette Minimap-/Atlas-Sonderrenderer unterdrücken nur die neuen UI-Submissions per RAII; ihre nativen Draws bleiben bestehen.

Jeder Draw setzt PSO/Topologie, Cull, BlendOp/Src/Dst, Depth-Test/Write aus, SRV oder null, Sampler, Viewport, Scissor an/aus, Konstanten einschließlich Farbe/Alpha/Texture-Combiner/Matrizen. Scissor aus wird durch eine eigene PSO definiert, nicht durch den alten Rect-Inhalt. Keine neue Sortierung, kein globaler Reset pro Widget. UI besitzt einen separaten Binder samt Texture-/Bufferzählern; die M7/M8-Welt-Defaults bleiben unverändert.

### Gefundener und korrigierter M9-Integrationsfehler

Das native Bild-Indexbuffer verwendet `0,2,1,2,3,1`, die ungeindexierten Bar-Quads dagegen eine andere Strip-Reihenfolge. Der erste GPU-Vergleich zeigte deshalb bei einer negativ skalierten ExpandedImage mit Culling eine fehlende Diligent-Fläche. Ausschließlich die kopierten Bildvertices werden jetzt als `IndexedQuad` mit vertauschten Vertices 1/2 eingereicht. Native Vertices, Indexbuffer, StateManager und Draws bleiben unverändert. Der erneute Vergleich besteht alle 32 UI-Fälle mit mittlerer RGB-Abweichung 0 und unverändertem Depthbuffer.

## Abschlussmatrix – 35 Punkte

| Nr. | Thema | Umsetzung / Nachweis / Grenze |
|---|---|---|
| 1 | UI Call-Chain | Siehe obige Kette; `PythonApplication`, `PythonWindowManager`, `PythonWindow`, `PythonGraphic`, finale Primitive. |
| 2 | Klassen | `CWindow`, `CImageBox`, `CExpandedImageBox`, `CButton`, `CSlotWindow`, `CGridSlotWindow`, `CGraphicImageInstance`, `CGraphicExpandedImageInstance`, `CGraphicMarkInstance`, `CScreen`, neue `IUIRenderer`/`DiligentUIRenderer`. |
| 3 | Python/C++ | `wndMgr`, `grp`, `grpImage` bleiben unverändert; kein neuer Python-Commandstream und keine Produktions-Pythonänderung. |
| 4 | Widgets | Image/AniImage, Expanded, Button/Radio/Toggle/DragButton, Slot, Bar/Box/Line/Gradient, Boards/Tooltip/Cursor. Text ausdrücklich NICHT M9. |
| 5 | Vertexformat | Originales XYZ + packed BGRA-Diffuse + UV, 24 Bytes; synchron kopiert. Strip/List/Line; Fan wird nur im Adapter zur TriangleList expandiert. |
| 6 | Ortho | Originale Device-Matrizen; `D3DXMatrixOrthoOffCenterRH(0,width,height,0,0,zres)`, Ursprung links oben. |
| 7 | Pixelregeln | Native Image-Vertices mit −0.5 bleiben erhalten. D3D11-Clipoffset `(+1/viewportWidth,−1/viewportHeight)*w`. 32 native GPU-Vergleiche, auch Teilviewport/Spiegelung/Linien, RGB-Abweichung 0. |
| 8 | Texturen | Lazy Handle am ursprünglichen `CGraphicImage`; `.sub` teilt den Parentatlas. Bestehender DDS/STB-Upload für DDS/TGA/PNG; keine Assetkonvertierung. Vorhandene Mips/Filter/Address übernommen. Clear/Reload/Device-Lifetime invalidiert den Handle. |
| 9 | Blend | Tatsächlich wirksamer nativer Zustand, normalerweise SRCALPHA/INVSRCALPHA; auch Expanded SCREEN/COLOR_DODGE und MODULATE. AlphaTest/Ref/Func und Farbkombination vollständig gebunden. |
| 10 | Clipping | Native `CWindow::Render`/`ScopedScissorRect`-Logik unverändert; tatsächlicher Rect je Draw. Leere Clips werden ausgelassen. Verschachtelte reale Window-/Child-Clips in Fixture, Pixelorakel für Rect-Wechsel. Native Einschränkung: nächster Clip-Parent, keine neu eingeführte All-Ahnen-Schnittlogik. |
| 11 | Z-Order | GAME → UI_BOTTOM → UI → TOP_MOST → CURTAIN; bestehende Childliste/SetTop; Cursor zuletzt. Keine Sortierung/Überlappungsoptimierung. |
| 12 | Dynamischer Buffer | Eigener UI-Binder mit wiederverwendbarem Dynamic-VB, mindestens 4096 Bytes, bedarfsgerecht wachsend, MAP_DISCARD je Draw; bestehende Constant-Buffer-/SRB-Strategie. Kein neuer UI-Ringbuffer nötig. |
| 13 | Slots | Originales ItemSlot/GridSlot, Item-/Equipment-/Quickslotbilder, Slot-Highlight und Cooltime-Geometrie. Mengen-/Zahlentext ausgelassen. Testdaten nur in privaten Offline-Fixtures, keine Inventar-/Serveränderung. |
| 14 | Cursor | Originales `mouseModule.mouseController`, Hotspots und halbtransparente Drag-Icons; OS-Cursor unverändert. Automatische Attach/Detach-Folge und erfasste echte Slot-Hover/Drag-Ereignisse im 1024er Test. Vollständige manuelle Normalclient-Abnahme offen. |
| 15 | Tooltip | Originaler `uiToolTip.ToolTip`/ThinBoard und Linien; Background/Layout sichtbar, Text nicht. Normales Hover-Verhalten nicht umgeschrieben. |
| 16 | Minimap | Kategorie B: eigener maskierter 2-Texture-Stage-Sonderrenderer. Minimap-/Atlas-Inhalt samt Markern bewusst vollständig aus M9 ausgeschlossen; normale umgebende Frames/Buttons migriert. |
| 17 | Login | Originalskript offline über Diligent geprüft, UI-only Present ohne Terrain. Normaler Diligent-Start mit Originalpaketen erfolgreich; die eigentliche Anmeldung ist laut Nutzer ohne Schrift nicht durchführbar und daher nicht abgenommen. |
| 18 | Charakterauswahl | Originale Frames/Buttons/Icons in Fixture. Separate 3D-Vorschau mit eigenem Viewport/Spot-/Punktlicht bleibt nicht migriert; bestehender Actor-Gate war dort bereits gesperrt. Kein stiller Ausbau des Actor-Scopes. |
| 19 | Ingame HUD | Originale Taskbar, HP/SP/ST, Quickslots und Buttons über gerenderter Welt in Offline-Maptests. Normale Diligent-Ingame-Abnahme wegen fehlender Schrift blockiert. |
| 20 | Inventory | Originales InventoryWindow, Equipment/Items, UV/Alpha/Position in 800/1024/1920-Fixtures; Nutzerabnahme im normalen Diligent-Spiel noch offen. |
| 21 | Character Window | Originales CharacterWindow mit ausgewählter Charakterseite in Fixtures; keine Layoutänderung. |
| 22 | Skill Window | Originale Skillseite; finaler Test verwendet die native `SetSkillSlotNew`-Grade-Auswahl, nicht das für aktive Skills ungeeignete alte `SetSkillSlot`. Zwölf Grade-Icons explizit verfügbar/bindbar, Diligent-Lauf vollständig und fehlerfrei; normale Ingame-Sichtabnahme offen. |
| 23 | Shop/Storage | Originales ShopDialog und SafeboxWindow; Lager-Grid wie im bestehenden Safebox-Code, offline ohne Serveroperationen. Reale Erreichbarkeit im normalen Spiel nicht bestätigt. |
| 24 | Resize | GPU-Orakel: 800×600 → 1024×768 → 1920×1080 → 1280×720 und Suspend/Resume. Original-UI zusätzlich in nativen 800×600-, 1024×768- und 1920×1080-Fenstern. Kein neuer Layout-Resize-Mechanismus. |
| 25 | Windowmodus/DPI | Windowed geprüft. Diligent-Fullscreen ist bereits im bestehenden Application-Start explizit gesperrt; nicht umgangen. Kein neuer Borderless-Modus, keine DPI-Awareness-/Skalierungsänderung; gesonderter Multi-DPI-Test nicht erfolgt. |
| 26 | Minimize/Restore | Native Backend-Suspend/Resume automatisiert bestanden. Dreifache manuelle Window-Prüfung für Legacy/Diligent angefragt; noch keine eindeutige Antwort im aktuellen Arbeitsstand erfasst. |
| 27 | Mapwechsel | Offline echte Maps A1 → B1 → A1 mit fortbestehendem UI, Actors/Trees/Effects/Water. Nicht mit einem serverseitigen Teleport gleichsetzen. |
| 28 | Relog | Offline UI-only → World+UI → Login-UI → World+UI erfolgreich. Echter Netzwerk-Relog wegen fehlender Schrift nicht durchführbar; wird nicht durch die Fixture ersetzt. |
| 29 | Shutdown | Bisherige vollständige Fixtures und normale Starttests Exitcode 0; konkrete Prozesse unten. |
| 30 | Ressourcen | `shutdown ui_textures=0 ui_buffers=0`; auch Actor/Attachment/Mount/Tree/World/Effect/Object-Ressourcen 0. Parent-/SubImage-Atlas-Sharing und echte Clear-Freigabe separat im GPU-Test. |
| 31 | Build ON/OFF | Release ON und OFF erfolgreich. Abschließend wieder ON gebaut (`build-on-final.log`), alle zehn Renderer-Tests erneut bestanden (12.87 s). Legacy bleibt Standard, Diligent benötigt weiterhin das Startargument. Bestehende Python-zlib-PDB-Linkwarnungen, kein neuer Buildfehler. |
| 32 | Testsuite | ON 14/14 (470.45 s), Renderer darin 10/10. OFF 12/12 (485.88 s), Renderer darin 8/8. Keine Tests deaktiviert. Details: `ctest-on-full.log`/`ctest-off-full.log` und gesicherte `*-details.log`. |
| 33 | Legacy Regression | Native Draws/States und Standardauswahl unverändert; OFF-Build und native Pixelreferenzen erfolgreich. Normaler Legacy-Prozess endete 0 und erreichte Ingame laut Log; vollständige manuelle M9-Bestätigung noch offen. |
| 34 | Sonderfälle | Text/Fonts, Nameplates/Damage Numbers, Minimap/Atlas, separate Auswahl-3D-Vorschau, MovieMan-Sonderpfad; außerdem unveränderte M8-Welt-Restpfade. Kein Anspruch, diese bereits darzustellen. |
| 35 | M10 | Die fehlende Schrift verhindert die vollständige M9-Abnahme. Als gesondert freizugebenden Folgeschritt den vorhandenen Textpfad analysieren und den Textumfang begrenzen; danach die offene M9-Ingame-Abnahme nachholen. Keine automatische Font-/Layout-Neuentwicklung. Minimap/Atlas und Auswahl-3D-Vorschau separat priorisieren. M10 hier nicht begonnen. |

## Bisherige Testevidenz

Artefakte unter `build/milestone9/` (lokal, nicht versioniert):

| Test | Legacy | Diligent | Ergebnis |
|---|---|---|---|
| Original UI, 1024×768, 12 Phasen, 3 Minuten | PID 58420, 180.8 s, Exit 0 | PID 58200, 181.2 s, Exit 0 | A1/B1/A1, UI-only/World-Übergänge, UI-Ressourcen 0 |
| Native 800×600-Fenster, 12 Phasen | PID 50632, 60.8 s, Exit 0 | PID 50732, 61.2 s, Exit 0 | `ui-800-device`, UI-Ressourcen 0 |
| Native 1920×1080-Fenster, 12 Phasen | PID 66372, 60.8 s, Exit 0 | PID 52132, 61.1 s, Exit 0 | `ui-1920-device`, UI-Ressourcen 0 |
| Normalstart, Originalpakete | PID 50348, 69.2 s, Exit 0 | PID 55468, 45.8 s, Exit 0 | Diligent nur Login/Auswahl nachgewiesen, noch kein normales Ingame |
| Finale Fixture mit zwölf Skill-Grade-Icons | PID 8592, 180.8 s, Exit 0 | PID 65356, 181.1 s, Exit 0 | `ui-final-interactions`, alle 12 Phasen, A1/B1/A1 und UI-Ressourcen 0; native Grade-Icons zusätzlich visuell angesehen |

Der letzte normale Diligent-Test mit dem abschließenden ON-Build unter `normal-ui-final` (PID 60764) endete nach 83.7 s mit Exitcode 0. Verifiziert: `shutdown ui_textures=0 ui_buffers=0`, auch die übrigen protokollierten Rendererressourcen 0; `syserr.txt` leer. Es wurde kein Ingame-Weltbild nachgewiesen. Der Nutzer bestätigt, dass die Anmeldung/Abnahme ohne Schrift nicht möglich ist. Dieser Start-/Shutdown-Test ersetzt deshalb keine Ingame-Abnahme. SHA256 des finalen Release-Clients: `5FBE209A2C3501F1882AAAB3D0D09344A98385165FA6E1B4CF7B8F2A67EE8A72`.

### Reproduzierbare Befehle

Im Source-Verzeichnis, nach einem Release-ON-Build:

```powershell
ctest --test-dir build -C Release -R '^Renderer\.' --output-on-failure
tests/Renderer/setup_ui.ps1 -Name neuer-ui-test
tests/Renderer/run_effects.ps1 -TestRoot build/milestone9/neuer-ui-test -Backend diligent
tests/Renderer/run_effects.ps1 -TestRoot build/milestone9/neuer-ui-test -Backend legacy
```

Für die Auflösungsprüfung zusätzlich `-UiConfiguration tests/Renderer/ui_resolution_800.json` bzw. `ui_resolution_1920.json` an `setup_ui.ps1`; die jeweilige benachbarte `.cfg` setzt auch native Device-/Fenstermaße. Immer frische Testnamen benutzen; Originalpakete bleiben unverändert. Für normale Login-/Ingame-Tests `setup_ui.ps1 -Normal` verwenden; Authentifizierung erfolgt ausschließlich durch den Nutzer.

Die vollständigen ON/OFF-Suiten wurden einschließlich Fuzzer/Kompressionstests ausgeführt. `playTests` benötigt den vorhandenen Git-Bash-Pfad `/usr/bin` im Testprozess. Die zugehörigen vollständigen Protokolle sind gesichert; keine Tests wurden hierfür ausgenommen.

Die Speicherstichproben des vollständigen 1024er Tests liegen bei Legacy maximal 374.3 MiB, letzte Stichprobe 366.5 MiB; Diligent maximal 549.9 MiB, zuletzt 531.2 MiB. Nach initialem Laden verschiedener Fenster/Maps kein durchgehend steigender Verlauf. Das ist eine begrenzte Laufzeitbeobachtung, kein Langzeit-Leak-Beweis. CSV-Zahlen mit deutscher Dezimaldarstellung korrekt einlesen.

`gpu-ui-winding.log`: 32/32 native UI-Pixelvergleiche, Mean RGB 0, Channel-/Depth-Errors 0; alle bisherigen Welt-GPU-Vergleiche ebenfalls erfolgreich. `captures/ui-case*-d3d9.bmp`/`d3d11.bmp` dokumentieren die Pixelorakel. Die Sichtprüfung der Originalfenster erfolgte zusätzlich per Computer-Use-Skill; UI-Eingaben ohne bestätigtes Runtime-Ereignis werden nicht als erfolgreich gezählt.

### Nicht als erfolgreiche Abnahme zählen

- `original-ui-first`: zwei falsche Testasset-/Slotkonstanten; anschließend korrigiert. Kein Produktionsfehler, aber kein gültiger kompletter UI-Test.
- `resolution-800`: Prozessumgebung nicht wirksam; tatsächlich 1024×768.
- `ui-800-final`: nur UI-Maße geändert, native Devicekonfiguration noch 1024×768. Durch echte `*-device`-Tests ersetzt; Assertions und explizite private `.cfg` verhindern eine Wiederholung.
- Frühere Fixture-Skillfälle ohne Grad-Iconwahl bewiesen nur den Rahmen. Der finale Fixture-Stand prüft zwölf tatsächliche Grade-Icons explizit.
- Bereits bestehende Meldungen über leere Sound-Dateinamen in Offline-Maps sind kein neuer UI-Rendererfehler. Keine verdeckte Soundkorrektur.

## Git-Diff / Scope

- Neue kleine Schnittstelle, UI-Binder und UI-Bridge; reale Image-/SubImage-Owner halten UI-GPU-Handles.
- Kleine additive Hooks an finalen Bild-/Mark-/Rechteck-/Linien-/Fan-Draws, UI-/Game-Modusgrenze und reine Ausschluss-Scopes für Nicht-M9-Systeme.
- Bestehender Effect-Binder nur um optionale Line/Scissor/Viewport-Unterstützung erweitert; World-Defaults erhalten.
- Vorhandene Diligent-Oberfläche für UI-only und World+UI, Lifecycle/Zähler ergänzt.
- Neue Policy-/GPU-Tests sowie private Original-UI-Fixtures. `water_smoke.py` erhält nur optionale M9-Testparameter; sein Standard-M8-Test bleibt gleich.
- Keine Änderungen an Produktions-Python, UI-Skripten, Assets, Layout, StateManager, Granny, Shader-/Materialfeatures der Welt, Netzwerk oder Server. Runtime `config/channel.inf` war bereits vorher geändert und bleibt unberührt.
- Kein Commit/Push, kein M10. Noch offene manuelle Abnahmepunkte nicht als bestanden deklarieren.
