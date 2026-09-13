# M9 – UI-Audit vor der Implementierung

Basis: `618d75f` (M8), Source-Worktree sauber. D3D9Ex bleibt Default; keine Python-/Asset-/Layoutänderung. M8-Restpfade (Gildenprojektion, DungeonBlock, Actor-Kategorien) bleiben außerhalb M9.

## Aufrufkette und Integrationspunkte

`CPythonApplication::Process → Legacy BeginFrame → TerrainPresentation::BeginFrame → CPythonGraphic::SetInterfaceRenderState → OnUIRender → CWindowManager::Render → Root/Layer/CWindow::Render → OnRender → C++-Primitive bzw. Python OnRender → grp/grpImage/wndMgr → C++-Primitive → ursprünglicher D3D9-Draw`.

Anschließend `OnMouseRender → mouseController.Render → grpImage/ExpandedImage` und Present. `game.py::OnRender` ruft innerhalb der GAME-Layer `app.RenderGame` auf und kehrt danach mit `grp.SetInterfaceRenderState` zur UI zurück. Daher kein pauschaler globaler Draw-Hook: Der Adapter benötigt eine explizite UI-/Game-Modusgrenze.

Die bisherige Diligent-Kindoberfläche wird nur bei `HasTerrain` präsentiert. M9 muss dieselbe Oberfläche auch für Login/Loading/UI präsentieren, ohne Eingabe vom vorhandenen Parent-Fenster zu übernehmen. Text wird im Diligent-Bild bewusst ausgelassen, nicht aus D3D9 kopiert.

## Komponentenmatrix

| UI-Typ | Native Ausgabe / Dateien | M9 |
|---|---|---|
| Image / Icon | CGraphicImageInstance::OnRender, PDT-Quad, Default-Fill-IB | ja, originale Vertices/UV/Farbe |
| Expanded Image / AniImage | GrpExpandedImageInstance.cpp, CPU-Rotation/Scale/RenderingRect, wechselnde Originalbilder | ja |
| Button/Radio/Toggle/DragButton | PythonWindow.cpp, gewähltes up/over/down/disabled CGraphicImageInstance | ja, keine Interaktionsänderung |
| Slot/GridSlot | PythonSlotWindow.cpp, Bilder, Highlight-Bars, Cooltime-Fan, AniImages | ja; Mengen-/Cooldown-Text nicht M9 |
| Bar/Rect/Line/Bar3D/Gradient | PythonWindow.cpp → CScreen::Render*2d → vorhandene PDT-Daten in GrpScreen.cpp | ja |
| Tooltip/Rahmen | normale Python-Boards/Bars/ImageBox-Teile | ja; Text nicht M9 |
| Cursor / Drag-Icon | mousemodule.py → ImageInstance/ExpandedImage; OS-Cursor alternativ nativ | Softwarebilder ja, OS-Cursor unverändert |
| UI-Gildenwappen | CMarkBox → CGraphicMarkInstance, Ausschnitt des Wappenatlas | ja; TextTail-Wappen nicht M9 |
| Minimap | PythonMiniMap.cpp::Render, neun Terraintexturen + Stage-1-Maske + generated coordinates + Punktmarker | separater Sonderrenderer B, M9B/M10; umgebende normale UI-Frames/Buttons ja |
| Atlas | PythonMiniMap.cpp::RenderAtlas, eigenes Karten-/Marker-/Gildenareal-System | zusammen mit Minimap separat, keine Teilmigration der Marker |
| Text / TextLine / NumberLine / TextBar | GrpTextInstance, bitmapbasierte NumberLine, GDI/TextBar/BlockTexture | **NICHT M9**, auch keine indirekte Glyphenmigration |
| Nameplates / Damage Numbers | PythonTextTail, ausgeschlossener Damagevalue-Effectpfad | **NICHT M9** |
| Charakterauswahl-3D-Vorschau | introselect.py / introcreate.py → chr.Render, eigenes Viewport, SetOmniLight | kein 2D-Primitive; bisheriger Actorpfad dort gesperrt, Umfang separat mit Benutzer klären |
| Film/Logo | MovieMan.cpp, DirectShow/DirectDraw; OnLogoRender-Pythonbindung ist leer | außerhalb normaler UI-Primitive, keine neue Video-/Webpipeline |

## Legacy-Analyse – 35 Fragen

1. Klassen: UI::CWindow/CWindowManager, CImageBox/CExpandedImageBox/CAniImageBox/CButton/CSlotWindow/CGridSlotWindow/CBar/CBox/CLine/CBar3D/CMarkBox, CPythonGraphic, CGraphicImageInstance, CGraphicExpandedImageInstance, CGraphicMarkInstance, CScreen.
2. Framebeginn: PythonApplication.cpp::Process; OnUIRender und OnMouseRender laufen zwischen BeginFrame und EndFrame.
3. Grenze: Python-Module `wndMgr`, `grp`, `grpImage` rufen vorhandene C++-Window-/Graphic-Funktionen; CWindow::OnRender ruft umgekehrt Python-Handler. Kein neuer Python-Commandstream.
4. Widgets: siehe Matrix, zusätzlich TextLine/NumberLine, Edit-/List-/Scroll-/Board-Konstrukte überwiegend als Python-Komposition vorhandener C++-Primitive.
5. Eigene Draws erzeugen die finalen Image/Expanded/Mark- und CScreen-Funktionen sowie PythonGraphic::RenderAlphaImage/RenderCoolTimeBox; Button/Slot setzen diese zusammen.
6. Bilder: vorhandene atlasbezogene TPDTVertex[4], `SetPDTStream`, DEFAULT_IB_FILL_RECT, TRIANGLELIST. `.sub` hält ein Parent-CGraphicImage und teilt über CreateFromTexturePointer dessen native Textur.
7. Rechtecke: RenderBox2d → RenderBox3d erzeugt vier Linienpaare; RenderBar2d → RenderBar3d erzeugt vier Stripvertices. Originale inklusive +1-Endpunktkorrektur der Box behalten.
8. Linien: RenderLine2d → RenderLine3d, LINELIST mit zwei PDT-Vertices, keine eigene CPU-Antialias-Geometrie.
9. Buttons: CButton::OnRender rendert m_pcurVisual; Hover/Disabled/Pressed wählen bestehende Bilder/Farben. Keine neue Zustandslogik.
10. Icons: normale ImageInstances, einschließlich .sub-Ausschnitten; Texture- und Bildabmessungen sind nicht grundsätzlich identisch.
11. Slots: Basisbild/Itembild/Highlight/Cooltime-Fan/animierte Slot-Effekte in vorhandener Reihenfolge. RenderCoolTimeBox verwendet dynamische TRIANGLEFAN-Vertices; Slot-Mengen werden als NumberLine gezeichnet und bleiben M10.
12. Clipping: CWindow::Render + ScopedScissorRect setzt D3DRS_SCISSORTESTENABLE und RECT, liest/sichert beide und stellt beide im Destruktor wieder her. Adapter übernimmt den tatsächlich wirksamen Device-Scissor pro Draw.
13. Parent/Child: GetParentScissorWindow sucht den nächsten Scissor-Parent; CWindow schneidet dessen Widgetrechteck mit dem eigenen. Kinder ohne eigenes Scissor erben den aktuellen Zustand. Mehrfach überstehende Enkel schneiden im bestehenden Code nicht explizit alle Ahnenrechtecke; M9 ändert diese Legacy-Layout-/Cliplogik nicht und darf deren Verhalten nicht als neue allseitige Clip-Engine verkaufen.
14. Alpha: Image-Diffusefarbe/Widget-Opacity, transparente Originaltexturen, Slot-/Bar-Farben; tatsächliche AlphaTest/Ref/Func pro Draw erfassen.
15. Blend: SetInterfaceRenderState setzt SRCALPHA/INVSRCALPHA. ExpandedImage SCREEN/COLOR_DODGE setzt INVDESTCOLOR/ONE, MODULATE ZERO/SRCCOLOR; danach Restore. Tatsächlichen BlendOp und Faktoren lesen.
16. Sampler: SetInterfaceRenderState fordert MIN/MAG/MIP NONE an; D3D9 kann ungültige Filterwerte ablehnen. Deshalb tatsächlichen Devicezustand erfassen, nicht den angeforderten Cachewert raten. Address/Filter/Mips bei jedem Draw vollständig binden.
17. Color: SetBlendOperation liefert gewöhnlich TEXTURE×DIFFUSE; andere bestehende grp-Operationen nutzen SELECTARG/TFACTOR. Keine neuen Materialfeatures.
18. Gradient: RenderGradationBar3d interpoliert Start-/Endfarbe über vier Originalvertices; AlphaImage hat linke/rechte Alphawerte.
19. Rotation/Scale: ExpandedImage berechnet Positionen, Origin, Rotation und negative Skalierung bereits auf CPU; Culling wird nativ bei Spiegelung angepasst. Diese finalen Daten verwenden.
20. Masking: normale UI benutzt Asset-Alpha und Scissor; Minimap verwendet eine separate zweite Texturstufe. Kein allgemeines neues Stencil-Masking.
21. Targets: normale UI zeichnet in den bisherigen Backbuffer, kein eigenes UI-RenderTarget im untersuchten Aufrufpfad. Diligent verwendet seine vorhandene World-Oberfläche plus Depth-off-UI-Draws.
22. Tooltips: Boards/Rahmen/Bars/Icons sind normale UI-Primitive, Text separat. Kein Tooltip-Layoutumbau.
23. Cursor: Software-Cursor und angehängtes Item/Skill-Icon werden nach Root-UI gerendert. Position/Hotspot kommen aus mousemodule.py. Alternativer Hardwarecursor bleibt Windows-Aufgabe.
24. Minimap/Atlas: eigener UI/World-Sonderrenderer, siehe Matrix. Ausschluss muss auch seine indirekt genutzten ImageInstances umfassen.
25. Video/Web: vorhandener MovieMan-Logo-/Filmweg ist DirectShow/DirectDraw, kein ImageBox-UI-Backend. Kein aktiver allgemeiner Web-Widgetpfad im untersuchten normalen Window-Renderer gefunden.
26. Direkte UI-Draws: GrpImageInstance.cpp, GrpExpandedImageInstance.cpp, GrpMarkInstance.cpp, GrpScreen.cpp, PythonGraphic.cpp; Minimap/Text/GDI-Code separat ausgeschlossen.
27. Aktive normale Bild-/Bar-/Linien-/Cooltimepfade benutzen den vorhandenen PDT-Dynamic-VB. Zahlreiche UP-Zeilen dort sind auskommentierte Altvarianten; aktive UP-Textpfade bleiben M10. Keine allgemeine StateManager-Interception.
28. Vertexformat: XYZ(float3)+DIFFUSE(packed BGRA)+TEX1(float2), 24 Bytes; SPDTVertexRaw/TPDTVertex. Quad-IB bzw. Strip, Linie oder Fan. Keine neuen Widgetvertices erfinden.
29. Koordinaten: absolute Widget-/Bildpixel, links oben; CWindow berechnet Parentpositionen/Alignments bereits vor Render. Native Bildschirm-/UI-Maße verwenden.
30. Projektion: CGraphicBase::SetOrtho2D → D3DXMatrixOrthoOffCenterRH(0,width,height,0,0,zres); tatsächliche World/View/Projection übernehmen.
31. Half-Pixel: Image/Expanded/Mark erzeugen x/y−0.5; andere Primitive nicht. D3D11 braucht die bereits im M7-Binder getestete Clipspacekorrektur (+1/ViewportWidth,−1/ViewportHeight)×w, keine zusätzliche pauschale Vertexverschiebung. Pixelorakel bei mehreren Auflösungen ist Pflicht.
32. Reihenfolge: bestehende Layer/Child-Reihenfolge plus Python-OnRender, World an ihrer bisherigen Stelle, danach UI und Mouse. Kein Sortieren/Batching über Drawgrenzen hinweg.
33. Z-Order: Layer GAME → UI_BOTTOM → UI → TOP_MOST → CURTAIN, innerhalb Childlisten; SetTop verschiebt bestehende Listeneinträge. Diligent-UI Depth-Test/Write aus, Drawreihenfolge entscheidet.
34. States: komplette PSO, Blend, Rasterizer/Cull, Depth-off, Sampler, SRV/null, Scissor an/aus+Rect, Viewport, Texture-Combiner, Farbe/Alpha und Matrizen je Draw. Kein globaler Reset nach jedem Widget.
35. World-Isolation: UI übernimmt kein World-Depth/Fog/Lighting; Game-Modus und ausgeschlossene Systeme dürfen keine UI-Submissions auslösen. Nächster World-Frame behält vorhandene World-Renderer/Defaulttargets. DPI-/Windowmodus bleiben nativ; keine neue DPI-Awareness/Layoutskalierung setzen. Konkrete getestete DPI-/Modusbedingungen im Ergebnisbericht benennen.

## Kleinster Implementierungsumfang

- Neue API-neutrale `Renderer/UIRenderData.h`, eigener `DiligentUIRenderer` als schmaler Nutzer des bestehenden Material-/Texture-/Dynamic-Upload-Binders; nur Linie, Viewport und Scissor als dafür benötigte optionale Bindereigenschaften ergänzen, bestehende Defaults bewahren.
- `EterLib/UIRenderBridge.*`: UI-/Game-Grenze, synchrone native Material-/Scissor-/Viewport-Snapshots, originale Quad/Strip/Fan/Line-Daten. `CGraphicImage/GrpSubImage`: lazy UI-GPU-Handle am echten Image-Owner; Parentatlas teilen, bei nativer Bildfreigabe/Reload freigeben.
- Kleine finale Draw-Hooks in Image/Expanded/Mark/GrpScreen/PythonGraphic; keine Pythonänderungen. Reine Ausschluss-RAII für Text/NumberLine/TextTail/Minimap, damit allgemeine Image-Hooks nicht deren Sonderpfade übernehmen.
- `TerrainPresentation.cpp`: UI init/frame/present/shutdown auf derselben Diligent-Oberfläche und getrennte Ressourcenzähler. Kein Backend-Hot-Switch, keine zweite neue Oberfläche.
- Tests: API/Scope-/Ownerpolitik auch OFF; native Pixelorakel inklusive Bild/Gradient/Linien/Fan/Clips/Materialreihenfolge; originale UI-Assets und Windows/Slots in isolierten Fixtures; reale Nutzerabnahme Login/Select/Ingame/Interaktion. Keine Tests deaktivieren.

Keine Fonts, Nameplates, Damage Numbers, neue UI-Optik, Materialeffekte, Welt-Restmigration, Granny- oder Gameplayänderungen. Nach M9 stoppen.
