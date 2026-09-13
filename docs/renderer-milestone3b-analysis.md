# Milestone 3B – Analyse vor Codeänderungen

> Historischer Milestone-Stand. Aktuell seit [M12](renderer-milestone12.md): Diligent D3D11 ist im ON-Build der Default; Legacy wird explizit mit `--renderer=legacy-d3d9` gewählt. OFF-Builds bleiben Legacy-only. Frühere Testergebnisse und Auswahlbeschreibungen unten gelten für ihren damaligen Stand.

11.09.2026, Basis `f75c0d7`, sauberer Worktree. Pfade relativ zu `src/`.
Nur originale Terrain-Splat-Komposition; keine Schatten-/Objekt-/UI-Portierung.

## Die 20 Legacy-Fragen

1. `PRTerrainLib/TerrainType.h`: 256 Slots, Index0 Eraser, maximal255 verwendbare
   Layer pro Patch. Tatsächlich begrenzen TextureSet, Active, PatchTileCount und
   globales Splatlimit. Terrain128×128 Zellen, 8×8 Patches zu16×16 Zellen.
2. `CTerrain::RAW_CountTiles` in `GameLib/AreaTerrain.cpp` zählt vorhandene Tile-
   Indices pro Terrain und Patch, einschließlich der bestehenden Randnachbarn.
   Beide RenderPatchSplat-Loops benutzen diese Ergebnisse, nicht bSplat/Höhenbereiche.
3. `CTerrainImpl::RAW_LoadTileMap` liest `Map/XXXXXX/tile.raw` über CPackManager in
   m_abyTileMap: 258×258 BYTE-Indices. TextureSet[j] adressiert die Farbtextur.
4. Alpha liegt in `m_lpAlphaTexture[j]`, referenziert durch
   `m_TerrainSplatPatch.Splats[j].pd3dTexture`; eine Maske pro Terrain und Layer,
   nicht eine separate Maske pro Patch. CPU-Ausgangsdaten sind temporär.
5. `CTerrain::AddTexture32`: 256×256, fünf Mips, A8R8G8B8 wenn ms_bSupportDXT,
   sonst A4R4G4B4. Name „DXT“ bedeutet hier keine komprimierte Alpha-Map.
6. RAW_LoadTileMap -> RAW_AllocateSplats -> RAW_CountTiles -> RAW_GenerateSplat ->
   AddTexture32. Gleicher Tileindex gibt255, kleinerer Index0; bei höherem Index
   ergibt ein Nachbar des aktuellen Layers255. Die vorhandene 8-Nachbarprüfung
   bleibt unverändert. AddTexture32 glättet258→256 mit (Mittel8Nachbarn+Zentrum)/2,
   anschließend vier vorhandene 2×2-Boxfilter-Mips128,64,32,16. PutImage32 schreibt
   Alpha in die oberen8Bits; PutImage16 quantisiert effektiv auf obere4Bits.
7. HTP hat Position+Normal, generiert Farbe/Alpha aus CAMERASPACEPOSITION. STP
   berechnet kTexTile, kTexAlpha, kTexStaticShadow, kTexDynamicShadow. Nur erste2
   gehören zu3B. Diffuse/Fog aus vorhandener STP-CPU-Arbeit nicht neu berechnen.
8. HTP Texture0=ViewInverse*TextureSet[j].m_matTransform;
   Texture1=ViewInverse*Translation(-TerrainX,+TerrainY)*m_matSplatAlpha.
   Schattenmatrizen sind getrennt und ausgeschlossen.
9. HTP pro Layer Scale(base*UScale,-base*VScale), Offset(UOffset,-VOffset),
   base1/3200. STP für alle Layer fest(+1/640,-1/640), ignoriert TextureSet-Scale.
10. HTP: BlendEnableTRUE, SrcAlpha/InvSrcAlpha, Add, AlphaTestGREATER0, DepthWriteTRUE,
    LessEqual, CullCW. Blendfaktoren aus StateManager-Default, Schattenpass stellt
    SrcAlpha/InvSrcAlpha wieder her. STP Apply/RestoreRenderState sind leer und
    erben die aktuellen States; SetGameRenderState setzt BlendEnableFALSE.
11. HTP Stage0 RGB=Texture*Current (ohne Vertexfarbe/Lighting effektivweiß),
    Alpha=TextureAlpha. Stage1 RGB=Current, Alpha=MaskAlpha; im ersten Pass
    Stage1 AlphaOPDISABLE. STP Stage0 RGB=Texture oder BLENDDIFFUSEALPHA(Texture,
    TFactor) für Nebel; erster Pass Stage1 SELECTARG2 (DefaultArg2DIFFUSE), danach
    SELECTARG1(Texture). Stage1 COLOROP kann geerbt DISABLE sein, was die gesamte
    Stage deaktiviert. Deshalb tatsächliche States am Submit übernehmen.
12. Layerindex j=1..GetNumTextures()-1, aufsteigend, nach bestehenden Skip-Regeln.
13. Base ist der erste tatsächlich gezeichnete aktive Layer eines Patches, nicht
    pauschal Texture001. Er nutzt nicht dessen Splat-Maske. HTP-Base-Alpha wird
    gegen die tatsächliche D3D9-Ausgabe geprüft, siehe API-Einschränkung unten.
14. Weitere aktive Layer werden separat mit ihrer Maske gezeichnet. Der optionale
    nachfolgende Schattenpass ist kein Splat-Layer und wird nicht migriert.
15. Gemeinsames Frustum/Distanz/LOD und Splatlimit; PatchProxy.isUsed, PatchNum>=0,
    TerrainNum!=0xff, gültige Terrain/VB; pro Layer Active und PatchTileCount>0.
    Fernbereich wird ggf. ohne Textur in FogColor gezeichnet (HTP+1600, STP+800;
    DensityFog verschiebt die Fern-Grenze). Diese Auswahl wird nicht dupliziert.
16. Erstellung nur bei TileCount>0, Rendering nur PatchTileCount>0; keine weitere
    CPU-Alpha-Epsilon-Prüfung. HTP-Pixeltest GREATER0. Keine neuen Skip-Schwellen.
17. m_iRenderedSplatNum pro Layer; Schatten erhöht ebenfalls, None-Pass nicht.
    m_iRenderedPatchNum pro Splat-Patch, SqSum aus dessen Splatdifferenz;
    m_RenderedTextureNumVector eindeutige verwendete Indices. Splatlimit nach jedem
    Layer und Patch. Diligent-Zahlen separat: Schatten bewusst nicht mitgezählt.
18. PatchTileCount bestimmt unterschiedliche Layerlisten/ersten Pass. Zuordnung
    PatchProxy -> TerrainNum/PatchNum; Alpha gehört zum Terrain, UV-Raum ebenfalls.
19. Farbe globales Tiling, Alpha terrainlokal. m_matSplatAlpha diag(±1/25600,0),
    Bias je Achse base*4.6. STP lokaler Ursprung zusätzlich base*12.30769 vor Scale.
    Vorhandene Matrizen/CPU-UVs direkt weitergeben, keine vereinfachte Neuformel.
20. HTP FarbeWrap, AlphaClamp; SetBestFiltering auf beiden: Min/MagAnisotropic,
    MipLinear, MaxAnisotropy nirgends angehoben (Default1). STP erbt Samplerzustände.
    Kein sRGB. DDS-Farbmips aus3A erhalten; Alpha exakt fünf bestehende Mips.

## Konkrete Call-Chains

```text
CMapOutdoor::LoadTerrain -> CTerrain::RAW_LoadTileMap
 -> CTerrainImpl::RAW_LoadTileMap(tile.raw)
 -> RAW_AllocateSplats -> RAW_CountTiles -> RAW_GenerateSplat -> AddTexture32
 -> vorhandene Glättung/Mips -> PutImage32/16 -> D3D9 Alpha-Texture

CPythonApplication::RenderGame -> BeginEnvironment -> CMapOutdoor::OnRender
 -> RenderTerrain -> HTP/STP Patchloop -> RenderPatchSplat
 -> Active/PatchTileCount -> TextureSet[j] + Splats[j]
 -> SetTransform/SetTexture/StageAlphaBaseOderMask -> DrawIndexedPrimitive
 -> bestehendes Splatlimit/Statistik -> optionaler Schattenpass (nicht3B)
```

## Minimaler Implementierungsplan / betroffene Dateien

- `Renderer/TerrainSplatData.h`, `TerrainRenderData.h`: neutrale Materialhandles,
  zwei UV-Sets, bestehende Blend-/Alpha-/Samplerwerte; M2/3A-Testpfade bleiben.
- `GameLib/AreaTerrain.h/.cpp` + kleiner Alpha-Adapter: vorhandene fertig gepackte
  CPU-Mips unmittelbar vor Unlock abgreifen, keine zweite Alpha-Erzeugung. R8-
  Upload erhält exakt die A8- bzw. expandierten A4-Alphawerte. GPU-Upload erst beim
  Rendern, nicht im Hintergrundloader. Besitz pro Terrain/Layer, Freigabe bei
  Ersetzung, Tile-Unload und Mapabbau; kein Renderer-globaler Materialcache.
- `GameLib/MapOutdoor.h/.cpp`, `MapOutdoorLoad.cpp`, zusätzliche kleine Splat-
  Brücke: Farbhandles pro Map, Paare aus Farbe/Alpha pro Terrain. Alte Layerauswahl
  bleibt maßgeblich. HTP/STP-Hooks unmittelbar an den bisherigen Layerdraws ersetzen
  den einzelnen3A-Submit; D3D9-Drawcalls/States unverändert.
- `MapOutdoorRenderHTP.cpp`, `MapOutdoorRenderSTP.cpp`, `MapOutdoorRender.cpp`:
  bestehende UV-Matrizen bzw. fertige STP-Attribute weiterreichen. Fern-None-Pass
  erhält Originalfarbe, keine willkürliche Single-Texture mehr. Bestehenden Nebel
  nur für Kompositionsparität abbilden; keine neue Licht-/Fog-Modernisierung.
- `Renderer/DiligentTerrainRenderer.*` + Diagnose: R8, SRVs, Layer-SRBs,
  Wrap-/Clamp-Sampler, Blend-Pipeline, Original-Alphaquelle/AlphaTest, bestehende
  Fog-/Diffuse-Werte. Keine Beleuchtung berechnen, keine Schatten portieren.
- Tests: CPU-Alpha/A4/Mips, echte D3D9/D3D11-Mehrpass-Readbacks, Layerreihenfolge,
  Base-Alpha, leere Masken, Nähten, Resize und Ressourcenfreigabe. Echte Maps mit
  identischer Kamera; Schatten/Objekte für isolierte Splat-Parität im Test abschalten,
  unveränderte Normalstarts separat prüfen. ON/OFF und gesamte Testsuite.

## Belegte API-Einschränkung / Paritätsgrenze

Microsoft dokumentiert ALPHAOPDISABLE bei aktivem ColorOp als undefiniert:
[D3DTEXTUREOP](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dtextureop).
Diese Kombination existiert im HTP-Base-Pass. Sie wird nicht im Legacy geändert.
Die Diligent-Auslegung muss daher durch einen tatsächlichen GPU-Vergleich gedeckt
sein, nicht als garantiert portables D3D9-Verhalten ausgegeben werden.
Bestehender Vertexfog benutzt Kamera-Z oder Range, je übernommenem State:
[Fog formulas](https://learn.microsoft.com/en-us/windows/win32/direct3d9/fog-formulas).

STP ist hier kein sauber isolierter Stateblock: die leeren Apply/Restore-Funktionen
werden nicht stillschweigend repariert. Bildvergleiche müssen Transformpfad,
Umgebungszustand und ausgeschlossene Schatten/Objekte explizit benennen.

Dieses Dokument wurde vor Implementierungsänderungen geschrieben. Es ist kein
vorweggenommenes Testergebnis. Nach erfolgreicher3B-Verifikation stoppen.
