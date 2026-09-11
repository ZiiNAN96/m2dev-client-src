# Milestone 3A – Analyse vor Implementierung

11.09.2026, Basis `8f11bcb` (M1/M2 committed, Worktree sauber).
Pfade relativ zum Source-Repository. Ausschließlich eine Terrain-Textur; kein 3B.

## Legacy-Texturpipeline: die 18 Fragen

1. Herkunft: `GameLib/MapOutdoorLoad.cpp::LoadSetting` liest `TextureSet` aus der
   Map-`Setting.txt`, lädt `m_TextureSet` und bindet ihn an `CTerrainImpl`.
2. Echte Ressourcen: A1 nutzt `textureset/metin2_A1.txt`, B1 `metin2_B1.txt`.
   Beide enthalten als Texture001 `d:/ymir work/terrainmaps/b/field/field 01.dds`,
   UScale/VScale=5, Offset=0. Die Quelle liegt unter Runtime `assets/Terrain` sowie
   als Override unter `assets/patch1`. Der Pack-Manager bestimmt den tatsächlich
   aufgelösten Inhalt; keine hartcodierte Wahl eines Asset-Verzeichnisses.
3. Besitzer: `PRTerrainLib/TextureSet.h/.cpp`, `CTextureSet` besitzt
   `vector<TTerrainTexture>`. Ein Eintrag enthält Dateiname, `CGraphicImageInstance`,
   D3D9-Texturzeiger, Scale/Offset, Höhen-/Splat-Metadaten und `m_matTransform`.
   Index 0 ist der leere Eraser, nicht die erste verwendbare Textur.
4. Formate: Der erste DDS liegt als DXT1 vor: Terrain-Version 256×256/6 Mips,
   Patch1-Version 512×512/5 Mips (Header direkt geprüft). Im Terrain-Assetbestand
   existieren weitere DDS/TGA. Der allgemeine Loader unterstützt zusätzliche Formate;
   das bedeutet nicht, dass alle diese Formate hier tatsächlich Terrain-Material sind.
5. Laden: `CGraphicImage::OnLoad -> CGraphicImageTexture::CreateFromMemoryFile`
   versucht `CreateFromDDSTexture`, danach `CreateFromSTB`, danach D3DX-Fallback.
   DDS verwendet den vorhandenen Microsoft-Loader in
   `EterImageLib/DDSTextureLoader9.cpp`. STB liefert RGBA; D3D9 swizzelt nach BGRA.
6. Resource-Abstraktion: `CResourceManager -> CResource::Load -> CGraphicImage ->
   CGraphicImageTexture/CGraphicTexture`, gehalten durch `CGraphicImageInstance`.
   `CResource::Load` liest über `CPackManager::GetFile`; temporäre Dateidaten werden
   anschließend verworfen. `CGraphicTexture` behält Größe und D3D9-Zeiger, keine Pixel.
7. D3D9-Erzeugung: DDS über `DirectX::CreateDDSTextureFromMemoryEx` mit DEFAULT-Pool
   und vorhandenen Mips, intern SYSTEMMEM-Staging und UpdateTexture. STB über
   `device->CreateTexture(..., 1, ..., A8R8G8B8/X8R8G8B8, MANAGED)` und LockRect.
   D3DX-Fallback in `GrpImageTexture.cpp`; Low-memory-Konvertierung nur dort.
8. HTP-UVs: keine gespeicherten UVs. `MapOutdoorRenderHTP.cpp` verwendet
   CAMERASPACEPOSITION, COUNT2 und Texture0 = ViewInverse * TerrainTextureTransform.
   Daraus wird mathematisch `worldPosition * TerrainTextureTransform`. Im STP
   entstehen `kTexTile=(x/640,-y/640)` im bestehenden `_SetTransform`.
9. `TTerrainSplatPatch` enthält keine Vertices/UVs, sondern Splat-/Tile-Metadaten.
   Die 24-Byte-M2-Quelle bleibt Position+Normal. Auch die STP-Quelle (28 Bytes) enthält
   keine UVs; erst `SoftwareTransformPatch_STLVertex` enthält berechnete UV-Sets.
10. STP-Sets: `kTexTile` Farbkacheln, `kTexAlpha` patch-/terrainlokale Alphamaske,
    `kTexStaticShadow` und `kTexDynamicShadow` Schatten. HTP Stage0 = Farbkachel,
    Stage1 = Splat-Alpha; Schattenpässe verwenden andere Matrizen/Textures. Nur Farbe
    gehört zu 3A, keines der anderen Sets wird portiert.
11. `TextureSet.cpp::SetTexture/Reload` erzeugt die Matrix:
    diag(base*UScale, -base*VScale, 0), Translation(UOffset,-VOffset,0).
    HTP bindet ViewInverse * diese Matrix. Alpha benutzt terrainlokale Translation
    und `m_matSplatAlpha`; Schatten eigene Matrizen. Keine Verwechslung mit World.
12. `base=1/(PATCH_XSIZE*CELLSCALE)=1/3200`. Texture001 mit Scale5 ergibt UV pro
    640 Welteinheiten. STP verwendet fest ±1/640 und ignoriert TextureSet-Scale/Offset;
    diese bestehende HTP/STP-Abweichung wird nicht im Legacy-Code korrigiert.
13. Farbe: U/V WRAP. Alpha und Schatten: CLAMP. Kein MIRROR im Terrainpfad.
14. HTP ruft `CStateManager::SetBestFiltering`: MIN/MAG ANISOTROPIC, MIP LINEAR.
    StateManager setzt seine Best-Werte auf ANISOTROPIC; DefaultState verwendet
    LINEAR/LINEAR/LINEAR. STP-ApplyRenderState ist leer und erbt vorhandene Zustände.
15. DDS-Mips werden geladen, keine Erzeugung (`generateMipsIfMissing=false`).
    STB erzeugt genau eine Mipstufe. Der D3DX-Fallback hat eigene Default-Mipregeln;
    keine neue Mip-Generierung ist für 3A erforderlich.
16. Kein Code setzt MAXANISOTROPY. D3D9-Default ist 1, obwohl HTP ANISOTROPIC als
    Filtertyp auswählt. 3A nutzt trilineare Filterung mit MaxAnisotropy=1, keine
    Qualitäts-/Performance-Modernisierung. Laufzeit/GPU-Vergleich prüft diese Wahl.
17. Kein Terrain-/Clientcode setzt SRGBTEXTURE oder SRGBWRITEENABLE. D3D9-Defaults:
    keine sRGB-Dekodierung/-Ausgabe. Für 3A daher UNORM, nicht UNORM_SRGB; originale
    komprimierte Blöcke bleiben erhalten. Sampler-Defaults siehe
    [Microsoft](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dsamplerstatetype),
    Ausgabe-Default siehe [Gamma](https://learn.microsoft.com/en-us/windows/win32/direct3d9/gamma).
18. Die konkrete Call-Chain folgt unten. Die M2-Sichtbarkeits-/LOD-Auswahl bleibt gleich.

```text
LoadSetting -> CTextureSet::Load -> SetTexture
 -> CResourceManager::GetResourcePointer -> ImageInstance::SetImagePointer
 -> CResource::Load -> CPackManager::GetFile -> CGraphicImage::OnLoad
 -> CGraphicImageTexture::CreateFromMemoryFile -> DDS / STB / D3DX -> D3D9 texture

CPythonApplication::RenderGame -> Background/MapManager::Render -> MapOutdoor::OnRender
 -> RenderTerrain -> vorhandene Patchauswahl/LOD -> HTP/STP RenderPatchSplat
 -> GetTerrainSplatPatch -> Active/PatchTileCount -> m_TextureSet.GetTexture(j)
 -> HTP: SetTransform(TEXTURE0, ViewInverse * m_matTransform)
 -> SetTexture(0, rTexture.pd3dTexture), SetTexture(1, Alpha)
 -> DrawIndexedPrimitive (erster Layer und weitere Splat-/Schattenpässe)
```

## Kleinste saubere Erweiterung

Der vorhandene `EterLib/CImageDecoder` kann RGBA liefern, behält bei DDS aber nur
die gesamte DDS-Datei – keine dekodierten Mipviews. Die normale Resource-Kette
behält überhaupt keine CPU-Pixel. Deshalb wird genau die gewählte Map-Textur einmal
erneut über denselben Pack-Manager gelesen. Kein permanenter CPU-Cache, kein neuer
Bilddecoder, kein GPU-Readback.

- Additiver CPU-View-Zugriff im vorhandenen DDS-Loader nutzt dessen Headerprüfung,
  Formatmapping und GetSurfaceInfo, ohne ein D3D9-Gerät zu erzeugen oder den alten
  D3D9-Ladeweg zu ändern. Nur 2D, validierte Subresources; unsupported ist ein Fehler.
- Neues `EterLib/TerrainTextureLoader.*` adaptiert DDS-Mipviews beziehungsweise
  CImageDecoder-RGBA an die neutrale Renderer-Grenze. Daten werden synchron konsumiert.
- `Renderer/TerrainTextureData.h`, `TerrainRenderData.h`: Format/Mipviews, Handle,
  Texture-Upload/Freigabe, Texturtransform und optionale Texture-Referenz beim Draw.
- `GameLib/MapOutdoor.h/.cpp`, `MapOutdoorLoad.cpp`, `MapOutdoorRender.cpp`: ein
  mapbesessenes Texture001-Handle; originale HTP-Matrix bzw. STP-Tiling weitergeben;
  Freigabe/Entbinden beim Mapabbau. Keine neue Layer-/Patchtexturentscheidung.
- `DiligentTerrainRenderer.*`: immutable Texture mit allen gelieferten Mips,
  SRV/SRB, Wrap-/Linear-Sampler, minimaler UV-VS + Texture.Sample-PS. M2-Geometrie-
  Testmodus bleibt erhalten; normales 3A-Maprendering verlangt die echte Textur.
- Tests: bestehende Tests erhalten; neue CPU-Loader-/Mipvalidierung und GPU-UV-
  Referenz mit asymmetrischer Textur, Patchnähten, Kamera, Mips, Resize und Lifetime.
  A1/B1/A1 mit echten Packs separat; beide Release-Varianten und normale Startpfade.

Die Loader-/Renderer-Implementierung, diese vier Mapdateien und Tests bilden den
Scope. Kein CStateManager-Umbau, keine UI-/Granny-/Materialmodernisierung. Geometrie,
Culling, LODs und D3D9-Texturstates bleiben unverändert. Ressourcen werden bei jedem
Mapwechsel neu erstellt/freigegeben, auch wenn A1/B1 denselben Dateinamen verwenden.

Status dieses Dokuments: vor Codeänderungen geschrieben; Testergebnisse folgen im
Abschlussbericht. Nicht mit bereits bestandenen 3A-Tests verwechseln.
