# M8: Audit und Integrationsplan vor Codeänderungen

> Historischer Milestone-Stand. Aktuell seit [M12](renderer-milestone12.md): Diligent D3D11 ist im ON-Build der Default; Legacy wird explizit mit `--renderer=legacy-d3d9` gewählt. OFF-Builds bleiben Legacy-only. Frühere Testergebnisse und Auswahlbeschreibungen unten gelten für ihren damaligen Stand.

Ausgangsstand: `1173df5` (M7), Source-Worktree sauber. Legacy D3D9Ex bleibt Default. Native Draws bleiben bestehen; die Diligent-Welt ist weiterhin eine eigene Oberfläche ohne UI-Komposition.

## 1. Verbleibende World-Pfade

Die Suche umfasst `DrawPrimitive`, `DrawIndexedPrimitive` und UP-Varianten in `src`; zusätzlich wurden die Aufrufer in `CPythonApplication::RenderGame` und `CMapOutdoor::OnRender` verfolgt. Ein verbleibender nativer Draw ist nicht automatisch eine Migrationslücke: M2–M7 führen ihn weiterhin aus.

| Pfad / zentrale Datei | Normales Spiel | Nativer D3D9-Draw | Diligent vor M8 | M8 / Einordnung |
|---|---|---|---|---|
| Terrain / Splatting, MapOutdoorRenderHTP.cpp | ja | indexed strips/lists | M2–M3 | vorhanden; Schatten-Multiplikation bleibt B |
| Software-Terrain, MapOutdoorRenderSTP.cpp | alternativer nativer Modus | indexed | gemeinsame SubmitTerrainSplat/SubmitTerrainGeometry-Hooks aus M2–M3, eigene CPU-Splatattribute | vorhanden, kein neuer Terrainrenderer |
| Gebäude/Props/CameraBlocker, MapOutdoorRender.cpp, ModelInstanceRender.cpp | ja | Granny indexed | M4, explizite SubmitStaticMapObject-Hooks | vorhanden |
| Actors/Attachments/Mounts, ActorRenderBridge, ModelInstanceRender.cpp | ja | Granny indexed | M5 | vorhanden |
| Bäume, SpeedTreeWrapper.cpp | ja | indexed, lists, fans | M6 | vorhanden; Shadow-Caster B |
| EffectLib / WeaponTrace / FlyTrace / Snow | ja | UP bzw. indexed | M7 | vorhanden; Snow-Blur B |
| Wasser, MapOutdoorWater.cpp | bei Wasserzellen | non-indexed triangle list | nein | migrieren |
| Sky-Gradient / Sky-Textur, SkyBox.cpp | Environment-abhängig | quad strips | nein | begrenzter M8-Sonderpfad |
| Cloud-Plane, SkyBox.cpp | Environment-abhängig | quad strips | nein | begrenzter M8-Sonderpfad |
| Ground-Item-Modelle, PythonItem.cpp::Render | bei fallengelassenen Items | ThingInstance → Granny | nein, kein Object/Actor-Hook | bestehenden starren Diffuse-Objectpfad anbinden; kein Item-Text |
| Gildenboden, RenderMarkedArea → DrawPatchAttr | wenn m_bVisibleGuildArea | indexed Terrain mit projizierter Maske | nein | C: eigener Zwei-Stufen-Projektionspfad; separat empfehlen |
| DungeonBlock, Area::RenderDungeon → CDungeonModelInstance::RenderDungeonBlock | bei Property DungeonBlock | Rigid TYPE_BLEND_PNT / zwei UV-Sätze | nein | C: eigener PNT2-Dungeonpfad, nicht Wasser; separat empfehlen |
| DungeonBlockShadow / CharacterShadowTexture | in Shadow-Pässen | eigene native Targets | nein | B: bewusst spätere Schatten |
| LensFlare::DrawBeforeFlare / DrawFlare | Environment-abhängig | orthographische Bildschirmquads | nein | B: Lens-/Screen-Overlay, kein neuer Postprocess in M8 |
| ScreenFilter / AfterFlare / Snow-Blur | Aktivierungs-abhängig | 2D-Quads | nein | B: Bildschirmfilter/Postprocess |
| CTerrainDecal / CDecal | keine aktiven Spiel-Aufrufer im Source gefunden | indexed UP fan | nein | D: unbenutzter Toolpfad; keine neue Nutzung erfinden |
| NEW_DrawWireFrame / DrawWireFrame, MapOutdoorRender.cpp | Debug-Schalter | indexed | nein | D: Terrain-Debug |
| GrpScreen Hilfsgeometrie / Kollisionsvisualisierung | Debug/Tools bzw. 2D-Aufrufer | diverse | nicht pauschal | D bzw. A nach Aufrufer; kein globaler Hook |
| Images/ExpandedImages/Marks, PythonGraphic Cooltime, BlockTexture | UI | quads/fans | nein | A |
| Fonts/TextTail/Minimap/Damagevalue | UI bzw. Bildschirmtext | quads/text/EffectLib | bewusst ausgeschlossen | A |

Kein eigenständiger River/Sea/Lake/Lava-Renderer gefunden: Wasser kommt aus demselben WaterMap-Pfad; animierte Lava-Effekte im normalen EffectLib-Pfad bleiben M7. Keine unabhängige normale Map-Boundary-Geometrie im RenderGame-Pfad gefunden. Das ist eine Source-Aussage, keine Behauptung über alle Assets. DungeonBlock und Gildenprojektion bleiben ausdrücklich echte Lücken C, nicht als UI umetikettiert.

## 2. Wasserpipeline: 31 Punkte

1. `CMapOutdoor::RenderWater/DrawWater`, `CTerrain`, `CTerrainPatch`, `CTerrainPatchProxy`.
2. `AreaTerrain.cpp::CTerrain::_CalculateTerrainPatch` baut aus vorhandenen Rohdaten beim Patch-Update die Wasservertices; `BuildWaterVertexBuffer` kopiert sie in den nativen VB.
3. `MapOutdoorLoad.cpp` / `AreaLoaderThread.cpp` laden `<map>/<tile>/water.wtr`; `CTerrainImpl::LoadWaterMapFile` liest Magic 5426, Breite/Höhe, Layerzahl, BYTE-Wasserzellindizes und Höhen. 0xff ist trocken, -1 ungültige Höhe. Normalformat LONG32; Legacy-Parser enthält zusätzlich einen WORD16-Zweig und bleibt unverändert.
4. Pro nasser Zelle zwei Dreiecke (TL/BL/TR und TR/BL/BR), vorhandene X/Y-Weltpositionen, negatives Welt-Y, Z = Wasserhöhe × HeightScale. Keine neue Tessellation.
5. Terrain 128×128 Zellen, Patches 16×16, CellScale 200. Maximal 16×16×6 Wasservertices je Patch, nur Zellen mit mindestens einem nichttransparenten Eckpunkt.
6. `SWaterVertex`: float3 Position + DWORD Diffuse (BGRA), 16 Byte. Keine gespeicherten UVs/Normalen. Diligent darf dies verlustfrei in sein vorhandenes 24-Byte-CPU-Vertexformat übertragen.
7. Keine Wasserindices. `DrawPrimitive(TRIANGLELIST, 0, faceCount)`; Diagnostik indices=0.
8. `d:/ymir Work/special/water/01.dds` bis `30.dds`, native CGraphicImageInstance / ResourceManager.
9. 30 Frames.
10. Zwei unveränderte Animationen: Texturframe und globale leichte Wasserhöhenbewegung.
11. Auswahl `(ELTimer_GetMSec()/70)%30`, also 70 ms je Frame. Einmal getroffenen nativen Frame spiegeln, keinen zweiten Timer auswerten.
12. Kein eigener U/V-Scroll im Wasserpfad; Clouds haben einen separaten nativen Scroll.
13. CAMERASPACEPOSITION, COUNT2, `inverseView * scale(waterUVBase,-waterUVBase,0)`; UVBase = 1/(CellScale×4). Tatsächlich gesetzte Matrix übernehmen.
14. Wasser: nur Stage 0; Stage 1 explizit ohne Textur und DISABLE.
15. Nahbereich AlphaBlend TRUE; Src/Dst/BlendOp werden hier nicht neu gesetzt und müssen vom tatsächlichen Draw-State gelesen werden. Ferne Patches: Textur NULL, AlphaBlend FALSE.
16. AlphaTest/Func/Ref sind geerbt; exakt am Draw erfassen, nicht aus angenommenen Defaults ableiten.
17. ZEnable/ZFunc geerbt; explizit erfassen.
18. ZWrite FALSE.
19. Cull NONE.
20. Environment-Fog geerbt; Fogfarbe/Vertexmodus/Range/Start/End/Density erfassen. Zusätzlich teilt `__GetFogDistance()` die beiden Wasser-Drawschleifen; Reihenfolge nicht ändern.
21. Vertexalpha aus Wassertiefe; originales RGB ebenfalls kopieren (insbesondere erster Eckpunkt enthält blaues RGB, nicht überall Weiß).
22. Lighting ist geerbt, keine Normalen im Water-FVF. RGB der nahen Fläche SELECTARG1 TEXTURE, Alpha SELECTARG1 DIFFUSE bei DIFFUSEMATERIALSOURCE COLOR1 und COLORVERTEX TRUE. Fern-/NULL-Textur-Fallback gegen natives D3D9 prüfen.
23. Kein eigenständiger Water-Tint; Environment wirkt über vorhandenen Fog bzw. nativen untexturierten Diffuse-Fallback.
24. Kein Reflection-RenderTarget/-Pass im Wasserpfad.
25. Kein Refraction-RenderTarget/-Pass.
26. Nicht nur Texturframes: native Z-Matrix oszilliert linear zwischen 0 und -random(0..15), Intervalle random(1000..3000 ms); bestehenden berechneten Wert übernehmen.
27. Pro Eckpunkt (unskalierte Wasserhöhe minus Rohterrainhöhe) clamp 0..0.8×OpaqueWaterDepth, Alpha = vorhandene Float→Int-Konvertierung von Tiefe/OpaqueWaterDepth×255. OpaqueWaterDepth initial 400. Keine neue Tiefenmessung im Shader.
28. MIN/MAG ANISOTROPIC, MIP LINEAR. Tatsächliche MaxAnisotropy, MaxMip, LODBias, Border mit erfassen.
29. Vorhandene DDS-Mipkette über den bereits getesteten TextureLoader hochladen; keine Neuberechnung/Modernisierung. Konkrete Asset-Metadaten im Testergebnis ergänzen.
30. WRAP U/V.
31. `RenderGame`: Sky → BeforeLensFlare → Cloud → BeginEnvironment → Background (Objects/Trees/Terrain/BlendArea/GuildMark) → Characters → Water → Snow → AreaEffects → EndEnvironment → EffectManager → GroundItems → Flying → PCBlocker → AfterLensFlare. Neue Draws exakt an vorhandenen Stellen, kein Rendergraph.

## 3. Kleinste geplante Änderungspunkte

- `Renderer/WorldRenderData.h`, `DiligentWorldRenderer.h`: eigener World-Owner/Counter-Vertrag, kompaktes Adapterobjekt über dem getesteten M7-Fixed-Function-Renderer; keine zweite Shader-/Texturedecoder-Kopie. Wasser/Sky/Cloud getrennt zählen.
- `EterLib/NativeMaterialSnapshot.*`: vorhandenen read-only M7-State-Snapshot gemeinsam nutzen. `WorldRenderBridge.*`: native Textur, Material und CPU-Vertices synchron an den World-Renderer übergeben.
- `GameLib/TerrainPatch.*`: vorhandene Wasser-CPU-Vertices nur bei aktivem Diligent festhalten; Lebensdauer an Patch binden. `MapOutdoorWater.cpp/MapOutdoor.h`: ausgewählten Originalframe, vorhandene Drawschleifen und Map-Texture-Owner anbinden.
- `EterLib/SkyBox.*`: vorhandene finale Quadvertices und Texturen anbinden; Ressourcen bei Unload/Mapwechsel freigeben. Nur originalen Cloud-Combiner ergänzen.
- `Renderer/TerrainPresentation.cpp`: World-Lifecycle im bestehenden Frame/Surface, bounded Zähler und Shutdown-Nachweis.
- `GameLib/StaticObjectBridge.*`, `UserInterface/PythonItem.cpp`: starren Ground-Item-Diffusepfad ohne globalen Statewechsel anbinden; Release vor ThingInstance.Clear. Komplexe nicht unterstützte Itemmaterialien ausdrücklich diagnostizieren.
- `tests/Renderer`: native D3D9-Pixelorakel für Wasser/Fog/Alpha/Depth/Clouds, State-Isolation, Texture-Frames/Lifetime und private Originalmap-Fixtures. Normalstart/Originalpakete unverändert lassen.

Kein CStateManager-Umbau, keine Mapdatenänderung, keine Granny-/Skinning-/Gameplay-/UI-/Schattenänderung. Nach den vereinbarten M8-Tests stoppen.
