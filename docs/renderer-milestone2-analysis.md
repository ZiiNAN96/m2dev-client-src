# Milestone 2 - Analyse vor den Codeaenderungen

Stand: 11.09.2026, auf dem uncommitteten Milestone 1. Pfade relativ zum Source-Repository.

## Daten und Erzeugung (Fragen 1-7)

- `GameLib/MapOutdoorLoad.cpp`, `CMapOutdoor::Load` liest `Setting.txt`, erstellt Patch-Proxies/Quadtree und laedt ueber `Update` / `LoadTerrain` die echten Mapdateien. `LoadTerrain` erstellt `CTerrain`, setzt Koordinaten und Settings, liest Height/Water/Attr/Tile-Dateien und ruft `CalculateTerrainPatch` auf.
- `PRTerrainLib/Terrain.h`, `CTerrainImpl` besitzt die Rohdaten: WORD-Heightmap mit Rand, signed-char-Normalmap, Tile-/Attr-/Water-Arrays und `m_TerrainSplatPatch`. `LoadHeightMap` in `PRTerrainLib/Terrain.cpp` kopiert `height.raw`. Die abgeleitete Implementierung in `GameLib/AreaTerrain.cpp` berechnet daraus die Normalmap.
- **`TTerrainSplatPatch` ist keine Geometriestruktur.** `PRTerrainLib/TerrainType.h`: TileCount, PatchTileCount, Splats und Updateflag. `CTerrain::RAW_LoadTileMap -> RAW_AllocateSplats -> RAW_CountTiles / RAW_GenerateSplat` befuellt diese Texturverwaltung. Sie wird in M2 nicht veraendert.
- `GameLib/AreaTerrain.h`: `CTerrain` besitzt 8x8 `CTerrainPatch`. `CalculateTerrainPatch -> _CalculateTerrainPatch` erzeugt je Patch ein lokales Array mit 17x17 = 289 Vertices, setzt Bounds/Typ/Updateflag und uebergibt es an `CTerrainPatch::BuildTerrainVertexBuffer`.
- Position: X = Terrain-X * 128 * 200 + Zell-X * 200; Y = negatives entsprechendes Map-Y; Z = WORD-Hoehe * `m_fHeightScale`. Patchbreite 16*200=3200 Einheiten. Heightmap-Zugriff beruecksichtigt den Ein-Zellen-Rand. Keine neue Hoeheninterpolation erforderlich.
- `GameLib/TerrainPatch.h`: `HardwareTransformPatch_SSourceVertex` = float3 Position, float3 Normal, 24 Bytes (Offsets 0/12). Softwarequelle = dieselben Felder plus DWORD diffuse, 28 Bytes. Beide sind gepackt. `TerrainPatch.cpp` kopiert die Hardwarequelle in einen D3D9-VB oder in die vorhandene Softwarequelle. **Im Hardwarepfad bleibt keine CPU-Vertexkopie erhalten.**
- `GameLib/MapOutdoorIndexBuffer.cpp`, `SetIndexBuffer`: drei gemeinsame WORD-Indexarrays pro `CMapOutdoor`, NICHT pro Patch. Nach dem D3D9-Upload werden die temporaeren CPU-Arrays wieder geloescht. M2 muss sie daher am existierenden Uploadpunkt konsumieren. LOD0 ist ein Zickzack-TriangleStrip mit degenerierten Verbindungen; LOD1/2 sind TriangleLists mit vorhandenen Randanschluessen (`ADDLvl1*`, `ADDLvl2*`). Kein Neuberechnen/Umordnen der Indizes.

## Draw-Chain und Auswahl (Fragen 8-10)

```
CPythonApplication::Process
 -> OnUIRender (bestehender Python-Spiel-Rendercallback)
 -> CPythonApplication::RenderGame
 -> CPythonBackground / CMapManager::Render
 -> CMapBase::Render -> CMapOutdoor::OnRender
 -> RenderTerrain
 -> BuildViewFrustum / __RenderTerrain_RecurseRenderQuadTree
 -> __RenderTerrain_AppendPatch -> sort(m_PatchVector)
 -> __RenderTerrain_RenderHardwareTransformPatch ODER SoftwareTransformPatch
 -> SelectIndexBuffer (bestehende LOD-Umschaltung)
 -> __HardwareTransformPatch_RenderPatchSplat / RenderPatchNone
    ODER __SoftwareTransformPatch_RenderPatchSplat / RenderPatchNone
 -> SetStreamSource / CStateManager::DrawIndexedPrimitive -> D3D9
```

Die Drawmethoden validieren Proxy, Patch-/Terrainnummer und vorhandene Buffer. Splatting zeichnet dieselbe Geometrie mehrfach; M2 braucht genau einen zusaetzlichen Geometrie-Draw je ausgewaehltem Patch, keine Splat-/Schattenkopien.

`RenderTerrain` prueft Terrain-Sichtbarkeitsflags, Proxy-Liste und aktuelle Kamera. `MapOutdoor.cpp::BuildViewFrustum` extrahiert sechs normalisierte Ebenen aus View*Projection (Near = Spalte 3, Far = Spalte 4 minus 3). Die Quadtree-Pruefung negiert Center-Y, testet Kugelradius gegen alle Ebenen und unterscheidet NONE/PART/ALL; bei ALL entfallen Kindtests. Blattdistanz ist max(abs(dx),abs(dy)), nicht euklidische Entfernung. Nur benutzte Proxies kommen in die sortierte Liste.

`MapOutdoorUpdate.cpp::UpdateTerrain -> ConvertTerrainToTnL` ordnet die vorhandenen Patch-Proxies um den Spieler zu; `UpdateQuadTreeHeights` berechnet Kugeln aus den Patch-Min/Max-Werten. Keine zweite Auswahl in M2.

HTP/STP behalten ihre existierenden Schleifen: LOD-Grenzen `__GetNoFogDistance = 200*ViewRadius*0.5`, `__GetFogDistance = 200*ViewRadius*0.75`. Fog-Near/Far-Partitionen und Splatlimit begrenzen die tatsaechlichen Draws; STP-Fernbereich hat zusaetzlich `IsFastTNL` und LOD2. M2 haengt sich hinter diese Entscheidungen, nicht an eine neue Interpretation davon. `SelectIndexBuffer`: LOD0 primitiveCount=indexCount-2, LOD1/2 primitiveCount=indexCount/3.

## Matrizen und Zustaende (Fragen 11-19)

- Kamera: `EterLib/Camera.cpp::CCamera::SetViewMatrix` verwendet `D3DXMatrixLookAtRH`. `EterLib/GrpBase.cpp::UpdateViewMatrix` uebernimmt genau `CCameraManager::GetCurrentCamera()->GetViewMatrix()` nach `ms_matView` und D3DTS_VIEW.
- Projektion: `CPythonApplication::RenderGame -> SetPerspective(30, aspect, 100, backgroundFarClip) -> D3DXMatrixPerspectiveFovRH -> UpdateProjMatrix -> D3DTS_PROJECTION`.
- Ergaenzung aus der Verifikation: `RenderGame` ruft vor dem Terrain `CCullingManager::Process` auf. Dieses aktualisiert View und Projection erneut (`EterLib/CullingManager.cpp:105`). Die vorher vom UI gesetzten Interface-Matrizen sind deshalb nicht die beim Terrain abgegriffenen Matrizen.
- World: HTP setzt vor der Patchschleife `m_matWorldForCommonUse` mit genulltem X/Y-Offset (Identitaetsbasis aus `CMapOutdoor::Initialize`; Water stellt seinen temporaeren Z-Offset wieder auf null). Spaetere X/Y-Aenderungen innerhalb der Splatfunktion dienen **nur Texturtransformationen**, nicht dem Geometrie-Worldtransform. STP transformiert die bereits weltpositionierten Vertices direkt mit View*Projection, entsprechend World=Identitaet.
- D3DX: zusammenhaengende Zeilen, Translation `_41/_42/_43`, Zeilenvektoren `position * World * View * Projection`. STP bestaetigt dies durch `D3DXVec3Transform(..., View*Proj)`. HLSL verwendet explizit `row_major` und `mul(position, matrix)`, ohne Transponieren.
- Rechtshaendige View/Projection, Z-Up in den Spielkamera-Settern (`SetPositionCamera`, `SetAroundCamera`). Nur die vorhandene Umwandlung Map-Y -> negatives Render-Y wird uebernommen. D3D-Tiefenintervall 0..1, kein OpenGL-Y-/Z-Umbau.
- Erstes LOD0-Dreieck 0,17,1: mit +X/-Y-Gitter zeigt sein Kreuzprodukt nach +Z. Legacy-Grundzustand `EterLib/StateManager.cpp::SetDefaultState`: `D3DCULL_CW` (CW wird verworfen), ZENABLE=true, ZWRITE=true, ZFUNC=LESSEQUAL. Die Terrainmethoden ersetzen diese Werte nicht; `RenderArea` stellt ZWRITE wieder her, SpeedTree stellt CULLMODE wieder her. Zur Laufzeit werden die tatsaechlichen Werte am Terrain-Einstieg geprueft, um unerkannte externe State-Leaks nicht als Paritaet auszugeben.
- Minimales Diligent-PSO: Backface-Culling, FrontCounterClockwise=true; DepthEnable/Write=true, LESS_EQUAL; DepthClip=true, Solid, Blend=false, Stencil=false, kein DepthBias. TriangleStrip und TriangleList brauchen getrennte PSOs. RGBA8_UNORM / D24_UNORM_S8_UINT entsprechen dem M1-Swapchain. Keine Textur-, AlphaTest-, Fog-, Licht- oder Schattenstates fuer die einfarbige Geometrie.
- Diligent-Konvention ebenfalls anhand des gepinnten `Graphics/GraphicsEngine/interface/RasterizerState.h` geprueft; [offizielle Referenz](https://diligentgraphics.com/doc/struct_diligent_1_1_rasterizer_state_desc.html) definiert FrontCounterClockwise im Render-Target.

## Kleine Integrationsgrenze / geplante Aenderungspunkte

1. `Renderer/TerrainRenderData.h`: neutrale Upload-Views, ressourcenbesitzende abstrakte Handles, Matrix-/Draw-Uebergabe. Backend wird einmal vor Map-Erzeugung gebunden. Kein Diligent-Header in GameLib, kein neues Terrainformat.
2. `TerrainPatch.cpp/.h`: vorhandene 24-Byte-Quelldaten beim Build synchron hochladen; Handle mit Patch freigeben. Keine dauerhafte zusaetzliche CPU-Kopie. `MapOutdoorIndexBuffer.cpp`, `MapOutdoor.cpp/.h`: entsprechende drei Indexhandles, Freigabe mit Map, aktuelle LOD aus `SelectIndexBuffer`.
3. `MapOutdoorRender.cpp`, `MapOutdoorRenderHTP.cpp`, `MapOutdoorRenderSTP.cpp`: echte Matrizen und bereits ausgewaehlte Draws an die neutrale Grenze. Existierende Draws/Schleifen bleiben unveraendert.
4. `Renderer/DiligentTerrainRenderer.*`: immutable VB/IB, dynamischer Matrix-Constantbuffer, minimaler VS/konstanter PS, zwei PSOs, Ressourcenfehler sichtbar machen.
5. `Renderer/TerrainPresentation.*`, `UserInterface.cpp`, `PythonApplication.*`: normaler Start auch bei Diligent. D3D9-Kompatibilitaetsdevice bleibt fuer unportierte Initialisierung, Login und UI erforderlich. Eigenes nicht-interaktives Kind-HWND fuer D3D11 verhindert konkurrierende Swapchains auf demselben HWND. Nur waehrend Welt-Terrainframes sichtbar; Eingaben bleiben beim bestehenden Elternfenster. Keine UI-/Text-Komposition. Resize/Minimize/Shutdown mit Besitzer-Lifetime. M1-Smoke-Bootstrap bleibt expliziten Smoke-Tests vorbehalten.

Bewusste technische Schuld: Legacy-Renderarbeit laeuft bei Diligent vorerst weiter. Das ist kein weiterer Portierungs- oder Optimierungsschritt. UI ist im Terrainbild nicht sichtbar; Login bleibt Legacy. Vollbild ist fuer diesen experimentellen Kindfensterpfad nicht freigegeben. Standardstart/Legacy-Dateien ausserhalb dieser Anbindung bleiben funktional unveraendert.

## Verifikation (noch ausstehend beim Schreiben dieser Analyse)

Beide Release-Konfigurationen; saemtliche bisherigen Tests inkl. Zstd; echte Map mit bestehenden Lade-/Kameraklassen; Vertex-/Index-Paritaet und GPU-Readback; mehrere Kamerastellungen, Depth, Patchgrenzen, vorhandene Auswahl/LODs; Resize/Minimize/Restore und Ressourcenabbau. Normaler Clientstart separat. Netzwerk-Login/Mapwechsel nur soweit vorhandene Testumgebung ohne weitere Systeme erlaubt. Kein Abschlussstatus ohne diese Trennung.
