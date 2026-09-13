# M10A – bestehender Basistext über Diligent

Stand: 12.09.2026. M10A abgeschlossen: Implementierung, normale Nutzerabnahme und
vollständige ON/OFF-Testsuiten bestanden. Release ON wiederhergestellt und alle elf
Renderer-Tests erneut bestanden. Die durch fehlenden Basistext blockierte M9-Ingame-
Abnahme wurde im beauftragten Umfang nachgeholt; gesonderte Grenzen sind unten benannt.
Die vor Änderungen erstellte Analyse steht in [renderer-milestone10a-analysis.md](renderer-milestone10a-analysis.md).

## Umsetzung / 30-Punkte-Bericht

| Nr. | Thema | Umsetzung und Nachweis |
|---|---|---|
| 1 | Font-Architektur | CFontManager → CGraphicText → CGraphicFontTexture → CGraphicTextInstance. Keine neue Font-Engine. |
| 2 | Library | Bereits vorhandenes FreeType, LCD-Rasterisierung und Windows-/lokale Fontauflösung unverändert. |
| 3 | Glyph-Cache | Dieselbe wchar_t-Map, dieselben Metriken/Bearings/Advances/Kerning und FT_Faces. Keine zweite Glyph-Map. |
| 4 | Atlas | Vorhandener CPU-Seitenpuffer wird beim nativen UpdateTexture synchron als BGRA8 hochgeladen. Diligent-Handle je nativer Seite am FontTexture-Owner. Immutable Ersatz nur bei dirty Update; alte Seiten vor CPU-Puffer-Recycling publiziert. Leere aktuelle Seite bei Bedarf transparent. GPU-Test prüft Mehrseitenatlas, Cachepointer-Stabilität und Freigabe ersetzter Handles. |
| 5 | Call-Chain | Python UI/TextLine/EditLine bzw. CPythonChat/CWhisper → CGraphicTextInstance::Render → unveränderte D3D9-Draws + TextRenderBridge::Submit → ITextRenderer/DiligentTextRenderer → vorhandener Effect-Materialbinder → dieselbe M9-World/UI-Oberfläche. |
| 6 | Vertexformat | Originales SVertex/TPDTVertex XYZ, DWORD BGRA, float UV, 24 Bytes. Batches und Reihenfolge bleiben erhalten. Nur Indexed-Rechtecke erhalten die zum nativen Indexbuffer passende kopierte Strip-Reihenfolge. |
| 7 | Textureformat | Native A8R8G8B8 = Speicher BGRA8; RGB enthält LCD-Subpixel-Coverage, A deren Maximum. Keine Konvertierung in Graustufen/weiße Glyphen. |
| 8 | Blendstate | LCD-Pass 1 ZERO/INVSRCCOLOR mit TEXTURE, Pass 2 ONE/ONE mit TEXTURE×DIFFUSE. RGB-Schreibmaske 7 erhält Zielalpha. Tatsächliche States, keine angenommenen Samplerwerte. Maske ist validierter PSO-Key-Bestandteil; Native 0xFFFFFFFF wird auf die wirksamen 4 Bits normalisiert. |
| 9 | Outline | Originale vier um 1 Pixel verschobene Quads; schwarz nur erster LCD-Pass, farbig beide. Pixelvergleiche bestanden. |
| 10 | Shadow | Kein neuer Schatten-Shader; bestehende Outline bzw. versetzte Aufruferdraws unverändert. |
| 11 | Color/Alpha | Originalfarben/Texttags und Alpha übernommen. Bestehende LCD-Eigenschaft bleibt: diffuse Alpha steuert nicht die RGB-Deckkraft der ZERO/INVSRCCOLOR- und ONE/ONE-Pässe. Kein heimliches Alpha-/Font-Redesign. Zielalpha im GPU-Test unverändert. |
| 12 | Alignment | Bestehende Positions-/Baseline-/Bearing-/Alignment-/RTL-/Umbruchrechnung; M9-Ortho und Half-Pixel-Regeln. Linke/zentrierte/rechte Ausrichtung und Style-/Größenvarianten im nativen Vergleich. |
| 13 | Clipping | Device-Scissor/Viewport vollständig je Draw gebunden; M9-Elternclip bleibt erhalten. Render(RECT*) behält nativen groben Zeilenfilter, keine neue Clip-Engine. Bewegter Text im echten verschachtelten Widgetclip sichtbar geprüft. |
| 14 | Eingabefelder | Unverändertes CIME/Python EditLine. Bestehende Selection-, Caret- und Composition-Unterstrich-Rechtecke zusätzlich eingereicht. Pixeltest umfasst gefülltes/leeres Feld, Selection und Password; leeres Feld hat 42 sichtbare Caretpixel. Normale Anmeldung und Bedienung durch Nutzer bestätigt. Vollständige IME-Kandidaten-/Kompositionsinteraktion mit koreanischer Eingabesprache nicht separat manuell geprüft. |
| 15 | Chat | Bestehender CPythonChat-/CWhisper-Textpfad; lokale Chat-Fixture ohne Netzwerkversand, Original-Chat-Ingame vom Nutzer bestätigt. Keine neue Chatdarstellung. |
| 16 | Login | Normaler Diligent-Client mit unveränderten Originalpaketen, Nutzer authentifiziert selbst; Texte/Inputs/Buttons und Anmeldung bestätigt. |
| 17 | Character Select | Namen/Labels und Auswahl vom Nutzer bestätigt. Separate 3D-Vorschau bleibt der bereits ausgeschlossene Sonderpfad, kein Actor-Ausbau. |
| 18 | HUD | Originale Taskbar/Labels/Quickslots mit Welt; Offline- und normale Nutzerabnahme. |
| 19 | Inventory | Originales Inventar, Slot-/Tooltip-Text, Item-Dragging/Hover vom Nutzer bestätigt. UI-NumberLine-Ziffernbilder nutzen M9-Imagepfad, kein Damage-Number-Pfad. |
| 20 | Skills | Originale Skillseite mit zwölf Grade-Icons in Fixture; normale Skills/UI-Bedienung vom Nutzer bestätigt. |
| 21 | Tooltips | Originale Tooltip-Geometrie plus Text/Outline/Texttags; sichtbare Fixture und normale Nutzerabnahme. |
| 22 | Resize | Native GPU-Tests 640/800/1024/1920 und zurück, lebende Atlasseiten; normaler Nutzer bestätigt Größenwechsel. Zusätzliche reale 800/1920-Fixtures jeweils Legacy und Diligent vollständig bestanden. Keine neue DPI-/Layout-/Window-Mode-Logik. |
| 23 | Minimize/Restore | Native Backend-Suspend/Resume, isolierter Legacy-Nutzertest und beide normalen Nutzerabnahmen bestätigt. Für den ersten isolierten Diligent-Test liegt keine separate manuelle Antwort vor; nicht aus Legacy-Antwort ableiten. |
| 24 | Mapwechsel | Isolierte Läufe echte Asset-Maps A1 → B1 → A1, jeweils 12 UI-Phasen. Nutzer bestätigt zusätzlich den angefragten normalen Diligent-A1/B1/A1-Test; serverseitiger Wechsel hier Nutzerbeobachtung, nicht automatischer Teleportnachweis. |
| 25 | Shutdown | Normales Diligent X, PID 55132: Exit 0/80.0 s. Normales Legacy Beenden, PID 64896: Exit 0/44.7 s. Isoliert Diligent PID 45836: Exit 0/181.1 s; Legacy PID 63340: Exit 0/180.8 s. |
| 26 | Ressourcen | Diligent jeweils `shutdown text_textures=0 text_buffers=0`, `ui_textures=0 ui_buffers=0`; Welt-/Actor-/Effektressourcen ebenfalls 0. Kein RenderTexture-/Font-Crosscache; ResetFrame löst SRV-Bindings, Shutdown eigene PSOs/Sampler/VB/CB. |
| 27 | Release ON/OFF | ON und OFF Release erfolgreich; finale ON-Wiederherstellung ebenfalls Exit 0, Cache steht auf ON. Kein Deploy der normalen Nutzerinstallation. Bereits bekannte Compiler-/Python-zlib-PDB-Warnungen bleiben, keine neuen Buildfehler. |
| 28 | Testsuite | ON 15/15 (444.82 s), darin Renderer 11/11. OFF 13/13 (440.07 s), darin Renderer 9/9. Nach finaler ON-Wiederherstellung nochmals Renderer 11/11 (13.20 s). 28/28 neue Text-Pixelvergleiche bestanden, Channel-/Depth-/Alpha-Errors 0; 32/32 bisherige UI-Vergleiche und sämtliche bestehenden Welt-GPU-Tests bestanden. Alle Testaufrufe Exit 0, keine Tests deaktiviert. |
| 29 | M9-Abnahme | Nutzer bestätigt Login/Auswahl, HUD, Inventar, Skills, Charakter-/ESC-Menü, Chat/Tooltip, Hover/Drag/Slots/Scroll, Fensterzustände und Mapwechsel unter Diligent sowie normale Legacy-Regression. Readability-Blocker behoben. Separater Netzwerk-Relog und normale Shop-/Storage-Erreichbarkeit nicht separat bestätigt; Originalfenster in Offline-Fixture geprüft. |
| 30 | Sonderpfade | Nameplates/Actor-/Mob-/Itemnamen, schwebende Chat-Tails, Damage Numbers, CTextBar/DIB, EventSet/Quest-/Empire-/Create-Beschreibungen, Minimap/Atlas und separate Auswahl-3D-Vorschau bleiben ausgenommen. Keine neue Fontabdeckung für fehlende Glyphen; nativer Leerzeichen-Fallback unverändert. Keine M10B-Technik. |

## Reproduzierbare Prüfung

Nach Release ON, immer frische Testnamen:

```powershell
tests/Renderer/setup_ui.ps1 -Milestone milestone10a -Name neuer-text-test
tests/Renderer/run_effects.ps1 -TestRoot build/milestone10a/neuer-text-test -Backend diligent
tests/Renderer/run_effects.ps1 -TestRoot build/milestone10a/neuer-text-test -Backend legacy
```

`-Normal` verwendet unveränderte Originalpakete; nur Nutzer-Login. Für echte 800/1920-Fenster
zusätzlich `-UiConfiguration tests/Renderer/ui_resolution_800.json` bzw. `ui_resolution_1920.json`.
Der normale Test nutzt ausschließlich das vorhandene Startargument `--renderer=diligent-d3d11`;
ohne Argument bleibt Legacy Standard. Keine Backendumschaltung während der Laufzeit.

Artefakte: `build/milestone10a/`. `gpu-text-baseline.log` ist der erste vollständige gültige
Textvergleich; `captures/text-case*-d3d9.bmp`/`d3d11.bmp` enthalten alle 28 Paare.
RGB-Mittelabweichungen ca. 0.286–0.333/255 inklusive Clear-Farben-Rundung; kein Kanalfehler
über der bestehenden Toleranz 4. Für M9-UI weiterhin Mittelabweichung 0.

Abschließende Build-/Testprotokolle im selben Artefaktordner:
`build-off.log`, `build-on-final.log`, `ctest-on-full.log`,
`ctest-off-full.log` und `ctest-renderer-on-final.log`. Die zugehörigen
`ctest-*-details.log` enthalten die ungekürzten Testausgaben einschließlich Pixelorakel.

Private Bytes im dreiminütigen Vergleich: Legacy maximal 375.4 MiB, zuletzt 368.5;
Diligent maximal 547.1 MiB, zuletzt 537.5. Normaler Diligent-Test maximal 727.0,
zuletzt 712.7. Begrenzte Stichproben, kein Langzeit-Leak-Beweis.

| Privater Test | Legacy | Diligent | Ergebnis |
|---|---|---|---|
| text-fixture-first, 1024×768 | PID 63340, 180.8 s | PID 45836, 181.1 s | 12 UI-Phasen, A1/B1/A1, beide Exit 0 |
| text-800, native 800×600 | PID 67532, 60.9 s | PID 66036, 61.2 s | 12 UI-Phasen, A1/B1/A1, beide Exit 0 |
| text-1920, native 1920×1080 | PID 61376, 60.8 s | PID 47012, 61.2 s | 12 UI-Phasen, A1/B1/A1, beide Exit 0 |
| normal-text-acceptance, Originalpakete | PID 64896, 44.7 s | PID 55132, 80.0 s | Normale Nutzerabnahme, beide Exit 0 |
| text-off, tatsächliches Release-OFF-Binary | PID 60652, 180.9 s | nicht enthalten | 12 UI-Phasen, A1/B1/A1, Exit 0 |

In allen Diligent-Testumgebungen abschließend Text-/UI-Texturen und -Buffer 0.
Getestetes Release-ON-Clientbinary SHA256:
`1FBECF3CA248C43266E66BDFB4D47A0D388CB46692F56D31F61D1F1826CBBAD3`.
Separates Release-OFF-Binary SHA256:
`216A3E9DC9E381D45DCC2FD8B10407619E08CB7B5650F4E29F29F658D92402F3`.
Abschließend neu gelinktes ON-Binary SHA256:
`CA26F91E7BFF84B7FC7110551432737BF46DBE19B4F700B353BDFC559E1B7C4F`.
Zwischen den normalen Nutzerabnahmen und dieser ON-Wiederherstellung erfolgte keine
weitere Produktionscodeänderung; der finale Renderer-Lauf prüft den neu gebauten Stand.
Die Computer-Use-Sichtprüfung ergänzte Nutzerbestätigungen und Pixelorakel; sie führte
keine Anmeldung durch. Keine nicht bestätigten Eingaben als erfolgreiche Interaktion zählen.

## Untersuchungsbefunde und Grenzen

- Erste Maskenfassung lehnte den vorhandenen Legacy-Wert 0xFFFFFFFF ab. Korrigiert auf
  dessen wirksame unteren vier Bits; vollständiger M9- und Weltvergleich anschließend erfolgreich.
- Vorläufige neue Testläufe waren kein vollständiger Pass: Test-Textfarbe musste wie im
  echten Widget explizit initialisiert werden. Außerdem ignoriert der bestehende StateManager
  bei SetRenderState das Force-Flag seines Restore; nach Test-Defaultreset blieb ein Clip aktiv.
  Nur der Test setzt den Device-Ausgangszustand ausdrücklich. Kein StateManager-Umbau.
  Die Sichtbarkeitsassertion für den leeren Cursor bleibt aktiv und besteht (42 Pixel).
- Vorhandene Sound-Meldungen über leere Dateinamen in Offline-Maps; normaler Diligent-Start
  einmal `MarkManager: invalid idx 0`, außerdem bestehende Damage-Diagnostik. Keine Text-/UI-
  Binderfehler in normalen und vollständigen isolierten Läufen; keine fremde Sound/Mark-Korrektur.
- Windowed geprüft. Vorhandene Diligent-Fullscreen-Sperre bleibt; Multi-DPI/IME-Spezialfälle
  und gesonderter Netzwerk-Relog nicht als geprüft deklarieren.

## Git-Diff / Scope

M9 liegt bereits uncommitted über Basis `618d75f`; dessen Änderungen bleiben erhalten.
M10A ergänzt vier kleine Interface-/Binder-/Bridge-Dateien, Atlas-Owner-
Handles und additive Batch-/Cursorhooks. Der vorhandene Materialbinder erhält ausschließlich
die Farbschreibmaske; TerrainPresentation Text-Lifecycle/Zähler. NumberLine nutzt bestehende
UI-Bilder, EventSet erhält einen reinen Ausschluss-Scope. Neue TextPolicy/TextGpuChecks und
private Text-Fixture, kleine rückwärtskompatible Ergänzung der M9-Testvorbereitung.

Keine Änderungen an FontManager, Font-Loading/Rasterisierung/Metriken/Layout/Mapping,
Produktions-Python/UI-Scripts, Fonts/Assets, Netzwerk/Server, StateManager, Granny/Skinning
oder weiteren Weltpfaden. Keine neue Shadertechnik. `git diff --check` bestanden.
Die bereits vorhandene Änderung an `m2dev-client/config/channel.inf` bleibt unangetastet.
Keine eigenen Testclients mehr geöffnet; fremde Spielclients unverändert. Kein Commit/Push.
Hier gestoppt: kein M10B, keine Nameplates, Damage Numbers oder moderne Font-Technik.
