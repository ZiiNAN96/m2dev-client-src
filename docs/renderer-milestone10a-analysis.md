# Milestone 10A – Analyse vor Implementierung

> Historischer Milestone-Stand. Aktuell seit [M12](renderer-milestone12.md): Diligent D3D11 ist im ON-Build der Default; Legacy wird explizit mit `--renderer=legacy-d3d9` gewählt. OFF-Builds bleiben Legacy-only. Frühere Testergebnisse und Auswahlbeschreibungen unten gelten für ihren damaligen Stand.

Stand: 2026-09-12. M9 ist uncommitted vorhanden; seine technische Prüfung ist dokumentiert,
die normale Diligent-Ingame-Abnahme war wegen fehlender Schrift blockiert.
Nur GPU-Ausgabe von Basistext ergänzen, keine neue Font-/Layout-/Input-Engine.

## Native Befunde (25 Punkte)

| Nr. | Thema | Bestehender Pfad / Verhalten |
|---|---|---|
| 1 | Klassen | `EterLib/FontManager`, `CGraphicText` (`GrpText`), `CGraphicFontTexture` (`GrpFontTexture`), `CGraphicTextInstance` (`GrpTextInstance`). Ressourcen über `CResourceManager`, Widgets über `UI::CTextLine`. |
| 2 | Library | Bereits FreeType: `FT_Init_FreeType`, LCD-Filter DEFAULT. Keine Neueinführung. |
| 3 | Loading | `CGraphicText::OnLoad` zerlegt z.B. `Arial:18.fnt`; `CFontManager::CreateFace` löst Namen auf lokale `fonts/` und Windows-Fonts (.ttf/.ttc) auf. Eigene FT_Face je FontTexture, vorhandener Pfadcache. |
| 4 | Glyph-Bitmap | `GetCharacterInfomation` → `UpdateCharacterInfomation` → FT_Get_Char_Index, FT_Load_Glyph TARGET_LCD, FT_Render_Glyph MODE_LCD. Drei Coverage-Bytes je Pixel. |
| 5 | Atlas | Bestehende Map wchar_t→Metriken/UV/Page. 256/512-Pixel-Seiten, zeilenweise mit 1 Pixel Abstand. Ein CPU-Puffer für aktuelle Seite, abgeschlossene Seiten bleiben native GPU-Texturen. Vor Seitenwechsel wird UpdateTexture aufgerufen. |
| 6 | Format | D3DFMT_A8R8G8B8, CPU DWORD = A=max(R,G,B), RGB=LCD-Coverage; im Speicher BGRA8. Nicht weiße RGB plus Graualpha! |
| 7 | Vertices | `GrpBase.h::SVertex`: float xyz, DWORD color, float uv = 24 Bytes; FVF XYZ/DIFFUSE/TEX1. Dasselbe Layout wie EffectVertex. |
| 8 | Quads | Render berechnet Bearing, gekernte Advance, Baseline und -0.5 Pixel-Offset; sechs Vertices je Glyph als TriangleList. Batch je nativer Atlas-Textur. Keine neue Quad-Erzeugung nötig. |
| 9 | UV | `TCharacterInfomation::{left,top,right,bottom,index}`; normalisierte Atlas-Koordinaten, direkt in SVertex. |
| 10 | Size | Vorhandener Fontname-Parameter; Default 12, Absolutwert → FT_Set_Pixel_Sizes. Ascender/LineHeight aus Face. |
| 11 | Bold/Italic | Bold über bestehende Fontnamen/-dateien (z.B. Arial Bold). Italic-Suffix i aktiviert vorhandene FT-Shear-Matrix xy=0x5800. |
| 12 | Farbe | m_dwTextColor bzw. m_dwColorInfoVector pro Glyph; bestehende Texttags ändern Farben; SetColor aktualisiert passende Einträge. BGRA-Packed-Vertexfarbe. |
| 13 | Outline | Vier Kopien ±1 Pixel x/y, vor Haupttext; eigene Outlinefarbe. Schwarze Outline nur LCD-Pass 1, farbige Outline beide Pässe. |
| 14 | Shadow | Kein separater Shadow-Shader im Basistext. Bestehende Outline oder vom Aufrufer separat versetzte Instanzen bleiben gleich; kein neuer Schatteneffekt. |
| 15 | Alpha | Wichtige Legacy-Eigenschaft: LCD-RGB-Blending berücksichtigt diffuse Alpha NICHT als Deckkraftfaktor. Alpha bleibt im Vertex und Combiner (auch Alpha-Test); RGB-Schreibmaske sperrt Zielalpha. Das wird übernommen, nicht als Font-Redesign korrigiert. |
| 16 | Mehrzeilig | LimitWidth plus m_isMultiLine: bei Breitenüberschreitung neuer Lauf mit maximaler Glyphhöhe. Bestehende Umbruch-/Taglogik in Update/Render bleibt identisch. |
| 17 | Alignment | Horizontal links/mitte/rechts, vertikal oben/mitte/unten; bestehende RTL-Anker und Ganzzahlrundungen; keine Neuberechnung in Diligent. |
| 18 | Clipping | Native Device-Scissor aus CWindowScopedScissorRect (M9) gilt auch für Text. Render(RECT*) hat zusätzlich bestehenden groben Zeilenausschluss `fCurY <= top`, keinen vollständigen neuen RECT-Clip. Whisper benutzt diesen Pfad. Beide Verhaltensweisen erhalten, keine Layoutkorrektur. |
| 19 | Edit/Caret | root/ui.py::EditLine → wndMgr/CTextLine → TextInstance; CIME liefert Cursor, Composition und Selection. Drei vorhandene Indexed-Rechteck-Ausgaben für Selection/Caret/IME-Unterstrich benötigen eigene Hooks. Password-Maskierung bleibt in Update. |
| 20 | Unicode | UTF-8 → UTF-16 via MultiByteToWideChar, strict dann lenient; bestehendes utf8.h Arabic/BiDi/Chat-Mapping und logische/visuelle Positionen. wchar_t-Cache, fehlende Glyph → Leerzeichen. Keine Erweiterung der bestehenden Fontabdeckung oder Supplementary-Plane-Behandlung. |
| 21 | Blend | Save SRCALPHA/INVSRCALPHA; LCD 1: ZERO/INVSRCCOLOR, SELECTARG1(TEXTURE); LCD 2: ONE/ONE, MODULATE(TEXTURE,DIFFUSE). COLORWRITEENABLE=RGB. AlphaBlendEnable, BlendOp, AlphaTest/Cull/Depth aus tatsächlich aktivem Devicezustand. Selection vor Text; Caret nach LCD unter den dann vorhandenen States (auch Sonderfälle erhalten). |
| 22 | Sampler | Render setzt keinen eigenen Sampler; daher tatsächliche Address/Min/Mag/Mip/LOD/Aniso/Border pro Draw aus Device lesen, nicht pauschal LINEAR annehmen. |
| 23 | Draws | Je Atlasbatch DrawPrimitiveUP TRIANGLELIST (1/2 LCD-Pässe), dazu DrawIndexedPrimitive (Selection/Caret) und DrawIndexedPrimitiveUP (IME-Unterstrich). Originalcalls bleiben bestehen. |
| 24 | Gemeinsam | CWindow::Render → CTextLine::OnRender → CGraphicTextInstance::Render. Python TextLine/EditLine/Buttonlabels/Tooltip/Skill/HUD/Inventar. CPythonChat::Render und CWhisper::Render verwenden dieselbe Engine; ChatLine ist EditLine. |
| 25 | Sonderfälle | CPythonTextTail (Actor-/Item-/Chat-Nameplates) und Minimap/Atlas bleiben durch M9-Scope ausgeschlossen. CPythonEventManager::RenderEventSet (Quest/Empire/Create-Beschreibungen) explizit ausgeschlossen. CTextBar ist separater FT→DIB/BlockTexture-Pfad, bleibt unangetastet. UI::CNumberLine zeichnet vorhandene Ziffernbilder (u.a. Drag-Count), kann M9-Imagepfad verwenden; keine Damage-Number-Migration. Character-Select-3D-Vorschau bleibt separater nicht migrierter Sonderpfad, Labels sind Basistext. |

## Kleinster Implementierungsplan

1. `Renderer/TextRenderData.h` + `DiligentTextRenderer.h`: UI-gebundener synchroner Drawvertrag,
   separater Text-Ressourcenzähler über bewährtem Materialbinder. UI-Suppression weiter beachten.
2. `GrpFontTexture.h/.cpp`: Diligent-Seitenhandles beim bestehenden Atlas-Owner.
   Nur bei nativer UpdateTexture dieselben aktuellen CPU-Pixel hochladen (immutable Ersatz je dirty Update).
   Keine zweite Glyph-Map, keine zweite Rasterisierung, kein GPU-Readback; bei Destroy/DeviceObjects mit freigeben.
   Renderer wird in PythonApplication::Create vor DefaultFont_Startup installiert; alte Seiten sind vor
   CPU-Puffer-Recycling hochgeladen. Leere aktuelle Seite bei Bedarf transparent hochladen.
3. `EterLib/TextRenderBridge.h/.cpp` + additive Hooks in `GrpTextInstance::Render`:
   exakt bestehende Batchvertices/Drawreihenfolge, Atlasowner, tatsächliche Blend-/Sampler-/Alpha-/Viewport-/Scissorstates.
   Glyphen und Eingaberechtecke teilen die M9-Orthoprojektion. Keine globalen Resets.
4. `EffectRenderData`, `NativeMaterialSnapshot`, `DiligentEffectRenderer`: Farbe-Schreibmaske vollständig
   erfassen, validieren und im PSO-Key/RenderTargetWriteMask binden. Default 15 erhält Welt-/UI-Verhalten.
5. `TerrainPresentation`: Text init/frame/failure/present/shutdown und begrenzte Summary-Zähler.
   `PythonWindow`: nur NumberLine-M9-Ausschluss entfernen; `PythonEventManager`: Sondertext-Scope beibehalten.
6. Neue Policy-/native Pixelvergleichstests: echte vorhandene FT-Glyphen, Outline, Farbe/Alpha, Clip,
   Sampler, Alignment, MultiLine, Password, Caret/Selection, Atlaswachstum/Mehrseiten/Release, UI-Text-Statewechsel,
   Größenwechsel. Original-UI-Testfixture mit lesbarem Text, anschließend echte manuelle M9-Abnahme.

Unverändert: Fonts, Glyph-Metriken/-Map, Shadertechnik (existierender Fixed-Function-Combiner), CPU-Skinning,
Granny, Weltpfade, Backend-Auswahl, Default Legacy, Pakete/UI-Scripts des normalen Clients.
Kein M10B. Nicht durchführbare manuelle Schritte werden nicht als bestanden ausgegeben.
