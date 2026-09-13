# Milestone 6 — Analyse vor Produktionsänderungen

> Historischer Milestone-Stand. Aktuell seit [M12](renderer-milestone12.md): Diligent D3D11 ist im ON-Build der Default; Legacy wird explizit mit `--renderer=legacy-d3d9` gewählt. OFF-Builds bleiben Legacy-only. Frühere Testergebnisse und Auswahlbeschreibungen unten gelten für ihren damaligen Stand.

Baseline `288f4da` (5D), sauberer Source-Checkout. Ausschließlich vorhandene SpeedTree-Objekte in der bestehenden Diligent-Weltfläche. Legacy D3D9Ex bleibt Default. Keine Effekte, Wasser, UI, neue Vegetation, GPU-Optimierung oder Granny-/Skinningänderung.

## Runtime, Daten und Besitz (Auftrag 1–15)

1. Der tatsächlich eingebundene `extern/include/SpeedTreeRT.h` bezeichnet sich als **Release 1.6.0, 19.12.2003**. Statisch gelinkt werden `extern/library/SpeedTree/speedtree_static.lib` bzw. Debug `speedtree_staticd.lib` über das importierte CMake-Target `SpeedTree`. Es gibt hier keine Runtime-Version-Abfrage; eine zusätzliche exakte interne Versionsnummer des vorgebauten Archivs ist aus dem Header allein nicht belegbar. Keine neue SDK-Version installieren.
2. `src/UserInterface/CMakeLists.txt` linkt `SpeedTreeLib` (Clientwrapper) und `SpeedTree` (SDK). `src/SpeedTreeLib/CMakeLists.txt` sammelt die bestehenden Wrapper. D3D9/D3DX9-Abhängigkeiten liegen im Wrapper/Forest, nicht in der öffentlich sichtbaren `CSpeedTreeRT`-Geometrie-API.
3. Klassen: `CSpeedTreeRT` als proprietäre Runtime; `CSpeedTreeWrapper : CGraphicObjectInstance` als Modell/Instanz; `CSpeedTreeForest` mit CRC→MainTree-Map; `CSpeedTreeForestDirectX` für die vorhandene D3D9-Ausgabe; `CMapManager` besitzt den Forest.
4. Laden: `CArea::__SetObjectInstance_SetTree` liest die Property `TreeFile` → `TObjectInstance::SetTree` → `CSpeedTreeForest::CreateInstance/GetMainTree` → `CPackManager::GetFile` → `CSpeedTreeWrapper::LoadTree` → `CSpeedTreeRT::LoadTree(bytes)` (Dateipfad als bestehender Fallback) → `Compute(nullptr,seed=1,false)` → `SetupBuffers`.
5. Formate: AreaData-Text mit Property-CRC, `.prt`-Properties, binäre `.spt`, originale `.dds`-Texturen. Branch/Composite/SelfShadow-Dateinamen kommen aus `STextures`, relativ zum SPT-Verzeichnis und mit `.dds`-Extension. Keine eigenen SPT-Parser.
6. `MakeInstance` ruft die SDK-Instanzierung auf und teilt Materialien, GeometryCache und native D3D9-Buffer mit dem MainTree. MainTree und Instanz halten derzeit gegenseitig shared_ptr; der vorhandene `DeleteInstance` entfernt die Instanz aus dem Parent. `CArea::__Clear`/ObjectInstance-Löschung → Forest.DeleteInstance, dann Map.Destroy → DestroyArea → Forest.Clear. Neue Ressourcen müssen denselben Besitz-/Abbaupfad verwenden, keine globale besitzende Instanzliste.
7. Transform: Die Tree-Property-Verarbeitung übergibt **nur x/y/z + HeightBias**. Area-Yaw/Pitch/Roll sowie TreeSize/Variance-Propertywerte werden in diesem Aufruf nicht angewandt. `LoadTree` kann eine Größenabweichung entgegennehmen, normale Map-Aufrufe nutzen aber die SPT-Größe. `PositionTree` setzt ausschließlich die Translation aus `GetTreePosition` als D3D-Weltmatrix und als Shaderkonstante c52. Keine nachträgliche Map-Rotation/Scale erfinden.
8. Die Runtime liefert CPU-Pointer in `SGeometry`, keine D3D9-Buffer: Branches/Fronds als `SIndexed`, Leaves0/1 als `SLeaf`, Billboard0/1 und optional HorizontalBillboard als `SBillboard`. Der Client erstellt und befüllt seine Buffer selbst. Daher kein GPU-Readback oder Runtime-Ersatz nötig.

## Vertex-/Materialvertrag (Auftrag 16–21, 27–31)

- Aktive Konfiguration in `SpeedTreeConfig.h`: `WRAPPER_USE_STATIC_LIGHTING`, `WRAPPER_USE_NO_WIND`, `WRAPPER_USE_GPU_LEAF_PLACEMENT`, `WRAPPER_FLIP_T_TEXCOORD`, Z-up, `WRAPPER_BILLBOARD_MODE`, `WRAPPER_RENDER_SELF_SHADOWS`, `WRAPPER_USE_FOG`. HorizontalBillboard ist ausgeschaltet.
- Branch/Frond: `SFVFBranchVertex` = Position float3, D3DCOLOR, UV0 float2, SelfShadow-UV1 float2 (32 Bytes). SDK-Normalen existieren, werden bei der aktiven statischen Beleuchtung nicht als Vertexattribut verwendet. SDK-Binormal/Tangent bleiben ungenutzt.
- Indizes: SDK uint16 Triangle-Strips. `SetupBranch/FrondBuffers` speichert LOD0-Strips hintereinander, dazu pro LOD die Strip-Längen. Die vorhandenen Draws verwenden die LOD0-Offsets und aktuelle Längen. Nicht stillschweigend andere Topologie auswählen.
- Leaves: `SFVFLeafVertex` = Zentrum float3, D3DCOLOR, UV float2, Windindex/-gewicht und LeafPlacementIndex/Scalar (40 Bytes). Windfelder sind in der aktiven NoWind-Variante ungenutzt. Je Blatt sechs Vertices `0,1,2,0,2,3`; TriangleList ohne Indexbuffer. Shaderposition = Zentrum + LeafTable[Cluster*4+Corner] * nativer LOD-Scalar + Treeposition.
- Besonderheit: `SetupLeafBuffers` fragt im LOD-Loop keine neue Blattgeometrie ab, sondern verwendet dieselbe beim Setup aktive Leaves0-Tabelle mit unterschiedlichen Scalars. Die effektive Welt-LOD ist ohnehin 1.0 (siehe unten). Keine verdeckte Legacy-LOD-Korrektur im Rahmen der Migration.
- Billboards: vier Position/UV-Vertices, native Kameraausrichtung/UVs aus Runtime; TriangleFan 0/1/2/3, zwei vertikale Varianten. Kein neuer Impostoralgorithmus.
- Forest-Reihenfolge: alle Branches aller Modelle/Instanzen → Fronds → Leaves → Billboards. Nur `isShow()`-Instanzen werden gezeichnet. Native Sonderpfade `OnRender` und `OnRenderPCBlocker` zeichnen dieselben Komponenten pro Objekt.
- Welt-Basis: Lighting FALSE, ColorVertex TRUE, Stage0 RGB/Alpha = Texture * Diffuse; Branches Cull CW, Fronds/Leaves/Boards Cull None. Alpha-Test GREATER, **AlphaRef jeweils DWORD des SDK-Wertes**, nicht pauschal 84. Zwei aktive Leaf-/Billboard-LOD-Sets erhalten jeweils ihren eigenen AlphaRef.
- Stage1 moduliert bei Branches/Fronds die vorhandene SelfShadow-Textur; Leaves setzen Texture1 null. Dies ist vorhandene Baumtexturierung, keine neue Schattenberechnung. Frond-Setup beachtet das Branch-`ms_bSelfShadowOn`-Flag nicht separat; keine Legacy-Korrektur dafür.
- Blend, Src/Dst, BlendOp, Z-Test, Z-Write und Z-Funktion werden vom Forest nicht vollständig gesetzt, sondern vom aufrufenden Weltpfad geerbt. Für Diligent müssen sie an der tatsächlichen Drawstelle vollständig gelesen, geprüft und explizit gebunden werden; nie State aus einem vorherigen Diligent-Draw voraussetzen.
- `CMapOutdoor::RenderPCBlocker`: blendende kameranahe Objekte werden vor der Hauptausgabe versteckt und später wieder gezeigt. Tree.OnRenderPCBlocker setzt AlphaBlend TRUE, AlphaTest GREATER, Fog FALSE und Stage1-Alpha-MODULATE. Anders als `SetupLeafForTreeType` lässt das dort inline ausgeführte Leaf-Setup die Kamera-Maske gebunden. Der Tree-Pfad darf hier nicht verschwinden oder zusätzlich zur regulären Ausgabe erscheinen; die tatsächliche Leaf-Sampling-Semantik wird gegen den Originalshader getestet.
- Fog: Branches/Fronds verwenden Fixed-Function-Vertexfog und vorhandene RenderStates. Leaves verwenden den eigenen VS1.1-Fog-Ausgang aus **Clip-Z** und `m_afFog` (c85), nicht nachträglich vereinheitlichtes View-Z. Static Vertex Colors bleiben die ursprünglichen D3DCOLOR-Werte. Billboards besitzen keinen Color-Vertex. Legacy-Sonderzustände müssen durch native GPU-Vergleiche bestätigt werden.

## LOD, Billboards, Wind, Sichtbarkeit (Auftrag 22–26, 32–34)

`LoadTree` setzt LodLimits = Baumhöhe × 2 / × 9 und DropToBillboard TRUE. Aber `CSpeedTreeWrapper::Advance` ruft **ComputeLodLevel(); SetLodLevel(1.0f);** auf. Auch OnRender/OnRenderPCBlocker setzen 1.0. Der aktuelle normale Client schaltet dadurch nicht entfernungsabhängig herunter. M6 übernimmt diesen tatsächlich vorhandenen Zustand unverändert; kein neuer High-LOD-Zwang im Diligent-Code und keine Aktivierung eines anderen LOD-Verhaltens. SDK-LOD-Daten/aktive Flags bleiben die Draw-Quelle. Ein isolierter SDK-Test kann zusätzliche LOD-/Billboard-Daten prüfen, ohne die Produktionsauswahl zu ändern.

`LoadTree` setzt Branch/Leaf/Frond `WIND_NONE`, aber **LeafRockingState TRUE** und eine RockingGroup. `Forest.UpdateSystem` übergibt Zeit an `CSpeedTreeRT::SetTime`; `GetLeafBillboardTable` liefert die aktuelle Kamera-/Rocking-Tabelle. Die aktive Leaf-Shaderformel wird minimal übernommen. Es werden keine neuen Ast-Windmatrizen berechnet: `SetupWindMatrices` enthält zwar vier native Matrixberechnungen, Upload/CPU-Anwendung sind unter den aktiven Defines ausgeschaltet. Ob und wie stark die gelieferten LeafTables zeitlich variieren, wird mit denselben echten SPTs gemessen, nicht aus dem NoWind-Namen abgeleitet.

`CreateInstance` registriert die native BoundingSphere. `CCullingManager::Process` führt den vorhandenen Sphere/Frustum-Test aus; `VisibilityCallback` setzt Hide/Show. Forest-Render prüft diese Sichtbarkeit. CameraBlocker/ShadowReceiver verändern Hide/Show innerhalb der existierenden Weltreihenfolge. Die Integration folgt den tatsächlichen nativen Draws; keine neue Area-/Tree-Cullingarchitektur.

## Tatsächliche Call-Chain (Auftrag 35)

`CPythonApplication::RenderGame` → `CCullingManager::Process` / bestehende Background-Ausgabe → `CMapOutdoor::OnRender` → `RenderArea` → **RenderTree** → Terrain → BlendArea → spätere Actor-/Blocker-Ausgabe.

`RenderTree` (PART_TREE sichtbar) → `CSpeedTreeForestDirectX::Render(Forest_RenderAll)` → `UpdateSystem` → `UpdateCompundMatrix` (SDK.SetCamera + c0) → alle Instanzen.Advance → native Komponenten-Setup/Draw → `RenderBranches/Fronds/Leaves/Billboards` → StateManager.Draw*. Shadow-/Minimap-Renderbits sind getrennt und dürfen keine Welt-Tree-Draws auf Diligent auslösen.

## Komponentenmatrix und repräsentative Assets

| Komponente | Geometrie / Alpha | Aktiver Wind | LOD | Billboard | M6-Grenze |
| --- | --- | --- | --- | --- | --- |
| Branches | CPU Position/Color/2UV, uint16-Strips, SDK AlphaRef | keine Astdeformation | SDK diskret, Welt effektiv 1.0 | nein | reguläre native Draws übernehmen |
| Fronds | wie Branch, Composite/SelfShadow, Cull None | keine Fronddeformation | SDK diskret, Welt effektiv 1.0 | nein | vorhandene Map-Nutzung zunächst messen; gleicher minimaler Indexed-Vertrag |
| Leaves | Center/Color/UV/Cluster/Scalar, 6 Vertices, SDK AlphaRef | Runtime LeafTable/Rocking | SDK Leaves0/1, Welt effektiv 1.0 | Blatt-Quads | ursprüngliche Leaf-Shaderplatzierung übernehmen |
| Billboards | SDK Kamera-Quad/UV, SDK AlphaRef | Runtime-Ausrichtung | native aktive Flags; durch Welt-LOD gewöhnlich inaktiv | ja | nur existierende aktive Quads, keine neue Umschaltung |

Read-only Property/AreaData-Abgleich: **A1 368 Platzierungen / 14 SPT-Dateien**, **B1 317 / 15**. Familien: Beech, Pagoda, MontereyCypress, Sassafras; B1 zusätzlich Baobab. A1 dichtester einzelner Sektor `001000` (37), B1 `001004` (32); geladene Nachbarsektoren vergrößern den tatsächlichen sichtbaren Bestand. Daten aus `assets/OutdoorA1`, `OutdoorB1` und `Property`, nicht frei erfundene Fixture-Platzierung. Die eigentlichen Geometrie-/Frond-/LeafTable-Zahlen dieser Dateien werden vor Integration mit der vorhandenen SDK-Bibliothek diagnostisch erhoben.

## Minimale geplante Änderungspunkte

1. Neue `Renderer/TreeRenderData.h`: kleine CPU-Vertex-/Strip-/Draw-Verträge und abstrakte Tree-Handles. Kein Diligent/SpeedTree/D3D9 im Datenvertrag. Native Modelle besitzen die neuen Ressourcen, Instanzen teilen sie wie bisher.
2. Neuer `SpeedTreeLib/TreeRenderBridge.{h,cpp}`: SDK-/Wrapper-CPU-Daten und tatsächliche native Drawstates in diesen Vertrag überführen. DDS-/Packloader aus `EterLib/StaticObjectTextureLoader` wiederverwenden. Keine direkten Diligent-Aufrufe im Wrapper.
3. `SpeedTreeWrapper.{h,cpp}`: wenige Source-/Draw-Hooks an den bestehenden Setup-/Drawstellen. `SpeedTreeForestDirectX.cpp`: begrenzter Welt-Render-Scope, um Shadow/Minimap auszuschließen. Wenn für die vorhandene CameraBlocker-Maske nötig, ein Scope am bestehenden Aufrufer `MapOutdoorRender.cpp`; keine Änderung der Sichtbarkeitslogik.
4. Neuer `Renderer/DiligentTreeRenderer.{h,cpp}`: gemeinsame vorhandene D3D11-Weltoberfläche/Depth; explizite PSO/SRV/Sampler/Alpha/Depth/Cull/Fog-Bindings pro Draw, minimale Übersetzung des vorhandenen Leaf-Shaders. Keine Material-/Lighting-Modernisierung.
5. `Renderer/TerrainPresentation.cpp`, `Renderer/CMakeLists.txt`: Startup, Framegrenze, begrenzte Diagnosezähler, Shutdown. Keine neue Oberfläche oder Backend-Auswahl.
6. `tests/Renderer`: SDK-Assetprobe, CPU-/native GPU-Parität, Originalmap-Fixture, wiederholte Map-/Fenster-/Ressourcentests. Alte Tests bleiben aktiv. Abschließend Release ON/OFF, komplette Suite, Renderer ON/OFF, normale Legacy-/Diligent-Logins durch den Benutzer. Noch keine Abnahme behaupten.

## SDK-Probe vor Integration

`tests/Renderer/SpeedTreeAssetProbe.cpp`, gebaut gegen dieselbe vorhandene statische SDK-Library, Exit 0; vollständige Ausgabe `build/milestone6/tree-assets.log`. Alle 15 A1/B1-SPTs besitzen **Branches, Fronds und Leaves**. Fronds sind damit zwingend Teil von M6 (56–188 Vertices, 14–47 LOD0-Strips). Branches: 282–865 Vertices; Leaves: 70–171 Quads. Alle Proben liefern sechs Branch-, vier Frond- und vier Leaf-LODs.

Bei LOD 1 sind die drei 3D-Komponenten aktiv und Billboard0/1 inaktiv; bei SDK-LOD 0.25/0 wird Billboard0 aktiv. Die Normalwelt erzwingt weiterhin ihren vorhandenen Wert 1.0. AlphaRef ist dort 84, Übergangswerte der SDK-Probe sind aber tatsächlich verschieden — weiterhin pro Draw übernehmen.

**Windnachweis:** alle drei Windmethoden liefern `WIND_NONE` (2), LeafRocking TRUE; die 64 Float-Werte der LeafTable verändern sich bei feststehender Kamera zwischen Zeit 0 und 1.25 um maximal 1.63–4.90 lokale Einheiten je nach Asset. Deshalb unbedingt dieselbe aktuelle Tabelle/Scalar wie im Legacy-Leaf-Shader verwenden. Diese Tabellenanimation ist nicht durch ein neues Windmodell zu ersetzen.

Branch-Texturen: BeechBark, PagodaTreeBark, SassafrasBark, MontereyCypressBark, BaobabBark; CompositeMapB1 und bei b3_beech_rt3 CompositeMapB3; zugehörige CompositeShadowMapB1/B3. Alle werden über den vorhandenen DDS-/Packpfad geladen.

Präzisierung der Grenze: Die Bridge erfasst den bestehenden CPU-Uploadvertrag beim Tree-Setup und übernimmt unmittelbar an den bestehenden nativen Drawstellen die vollständig angewandten Materialstates. Die Leaf-Konstanten werden aus dem bereits gesetzten nativen VS-Konstantenblock gelesen (keine erneute Runtime-Zeit-/Kameraauswertung). So verwenden beide Backends denselben Blattzustand. Der Renderer besitzt keine SDK-Pointer. Legacy-Aufrufe und ihr Reihenfolgeverhalten bleiben erhalten.

STOP nach verifiziertem M6. Kein weiterer Rendererpfad wird begonnen.
