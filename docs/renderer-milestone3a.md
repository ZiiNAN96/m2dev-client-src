# Milestone 3A: eine echte Terrain-Textur

> Historischer Milestone-Stand. Aktuell seit [M12](renderer-milestone12.md): Diligent D3D11 ist im ON-Build der Default; Legacy wird explizit mit `--renderer=legacy-d3d9` gewählt. OFF-Builds bleiben Legacy-only. Frühere Testergebnisse und Auswahlbeschreibungen unten gelten für ihren damaligen Stand.

Stand: 11.09.2026. Basis: `8f11bcb` (abgeschlossene M1/M2).
Source: `C:\Users\ZiiNAN\Documents\GitHub\m2dev-client-src`.
Die [Vorab-Analyse](renderer-milestone3a-analysis.md) beantwortet die 18 Legacy-Fragen.

## Ergebnis und Grenze

Diligent D3D11 stellt echte Terrain-Geometrie mit genau **Texture001 des geladenen
TextureSets** dar. Das ist keine Auswahl des lokal ersten aktiven Splat-Layers:
dieselbe echte Map-Textur liegt über allen sichtbaren Patches. Kein Alpha, kein
Splatting, keine Beleuchtung, keine Schatten und keine neuen Terrain-Algorithmen.

```text
Metin2_Release.exe                          -> Legacy D3D9Ex (unverändert Default)
Metin2_Release.exe --renderer=diligent-d3d11 -> experimentelles Single-Texture-Terrain
```

Keine neue Startoption, kein Hot-Switching, keine Änderung der Backend-Auswahl.
Der optionale Diligent-Build benötigt weiterhin `M2_ENABLE_DILIGENT_D3D11=ON`.
Der CMake-Default bleibt OFF. Die M2-Übergangslösung bleibt bestehen: normales D3D9-
Kompatibilitätsgerät und separate D3D11-Kindfenster-Präsentation im Fenstermodus.
Login bleibt D3D9. Nicht migrierte UI/Objekte sind im Diligent-Terrainbild weiterhin
nicht enthalten. Dies ist ausdrücklich noch kein vollständiger D3D11-Spielclient.

## Dateien und konkrete Änderungspunkte

Pfade relativ zum Source-Repository.

| Neue Dateien | Verantwortung |
| --- | --- |
| `src/Renderer/TerrainTextureData.h` | Neutrale Formate, geliehene Mipviews, besitzendes Texturhandle |
| `src/EterLib/TerrainTextureLoader.h/.cpp` | Bestehenden Pack-/DDS-/STB-Zugriff an Terrain-Interface adaptieren |
| `tests/Renderer/TerrainTextureCpuTest.cpp` | CPU-Layouts, Original-Mips, Kanalorientierung, ungültige Dateien |
| `tests/Renderer/TerrainTextureFixtures.h` | Kleine asymmetrische DDS/TGA-Testdaten und farbcodierte Mipstufen |
| `docs/renderer-milestone3a-analysis.md`, dieser Bericht | Analyse vor Code und Abschlussnachweis |

| Geänderte Dateien | Minimaler Eingriff |
| --- | --- |
| `src/EterImageLib/DDSTextureLoader9.h/.cpp` | Additives `GetDDS2DView`, bestehende DDS-Prüfung/Format-/Mipberechnung wiederverwenden |
| `src/Renderer/TerrainRenderData.h` | Upload/Release, UV-Matrix und Texturparameter ergänzen |
| `src/Renderer/DiligentTerrainRenderer.h/.cpp` | Texture/SRV/SRB/Sampler, UV-VS und Sample-PS, Texturzähler |
| `src/Renderer/TerrainPresentation.cpp` | Vorhandene begrenzte Diagnose um Texturstatistik erweitern |
| `src/GameLib/MapOutdoor.h/.cpp` | Ein Map-Texturhandle, Freigabe in Destroy |
| `src/GameLib/MapOutdoorLoad.cpp` | Texture001 nach bestehendem TextureSet-Laden hochladen, nur bei Diligent |
| `src/GameLib/MapOutdoorRender.cpp` | Bestehende UV-Transformation und Handle an die M2-Grenze reichen |
| `tests/Renderer/CMakeLists.txt` | Zusätzlichen CPU-Test auch im OFF-Build registrieren |
| `tests/Renderer/TerrainGpuTest.cpp` | D3D9-Texturreferenz, Kamera/Patchgrenzen/Mips/Lifetime und texturierten Resize ergänzen |
| `tests/Renderer/terrain_map_smoke.py` | Testfenstertitel milestone-neutral; vorhandener A1/B1/A1-Test unverändert |
| `docs/renderer-milestone2.md` | Historischen Stand mit Link auf 3A kennzeichnen |

Keine Änderungen an `CStateManager`, `GrpImageTexture`, `CImageDecoder`, TextureSet-
Verwaltung, Granny, UI, HTP/STP-Drawcalls, Terrain-Vertices, Indexalgorithmen, LODs,
Culling oder Spiel-/Serverdaten. Keine neue Abhängigkeit und keine Optimierung.

## Call-Chains und Loader

Legacy bleibt:

```text
CMapOutdoor::LoadSetting -> CTextureSet::Load -> SetTexture
 -> CResourceManager -> CResource::Load -> CPackManager::GetFile
 -> CGraphicImage::OnLoad -> CGraphicImageTexture::CreateFromMemoryFile
 -> vorhandener DDS-/STB-/D3DX-Lader -> IDirect3DTexture9

RenderTerrain -> HTP/STP RenderPatchSplat -> GetTexture(j)
 -> Texture0-Transform -> STATEMANAGER.SetTexture(0, Farbe)
 -> SetTexture(1, Alpha) -> bestehender D3D9-Drawcall
```

3A ergänzt ausschließlich hinter derselben Map-/Geometrie-Grenze:

```text
LoadSetting -> GetTexture(1).stFilename -> LoadTerrainTextureFile
 -> CPackManager::GetFile -> LoadTerrainTextureMemory
    DDS: GetDDS2DView (vorhandene DDS-Headerprüfung/GetD3D9Format/GetSurfaceInfo)
    andere Bilder: vorhandener CImageDecoder -> STB -> RGBA8
 -> ITerrainRenderer::UploadTexture -> immutable Diligent ITexture
 -> Default SRV -> je ein SRB für TriangleStrip/TriangleList

RenderTerrain -> BeginTerrain(World/View/Projection, vorhandene UV-Matrix)
 -> bestehende Patch-/LOD-Auswahl -> SubmitTerrainGeometry
 -> DrawTerrain(Geometrie, Indices, Texturhandle)
 -> PSO + CommitShaderResources -> DrawIndexed -> Present
```

Die normale Resource-Abstraktion hält keine dekodierten CPU-Pixel. Der bestehende
`CImageDecoder` hält DDS als gesamte kodierte Datei, nicht als dekodiertes RGBA.
Deshalb wird die eine ausgewählte Textur einmal pro Map erneut über denselben Pack-
Manager gelesen. DDS-Blöcke und Mipviews werden ohne Dekomprimierung direkt
hochgeladen. STB wird unverändert wiederverwendet. Kein neuer Bilddecoder, kein
permanenter CPU-Pixelcache, kein Readback von D3D9, keine neue Mipmap-Erzeugung.

Unterstützter kleiner 3A-Uploadumfang: DDS DXT1/3/5 als BC1/2/3_UNORM,
A8R8G8B8 als BGRA8_UNORM, X8R8G8B8 als BGRX8_UNORM, A8B8G8R8 als RGBA8_UNORM;
STB-Bilder als RGBA8_UNORM. Kein sRGB-Decode/Encode. Nur 2D bis 8192×8192,
geprüfte Mipanzahl/Strides/Puffergrenzen. Cube, Volume, DX10-DDS und nicht zugeordnete
Formate werden abgelehnt. DDS/TGA wurden gezielt getestet; für PNG/JPG/BMP wird
derselbe bestehende STB-Pfad verwendet, ohne separate neue Format-Verifikationssuite.
Unbekannt/ungültig ist ein expliziter Backendfehler, kein stiller einfarbiger Fallback.

## UVs, Sampler und Mips

Die 24-Byte-Quelle bleibt Position+Normal. Es werden keine neuen Vertex-UV-Daten
erfunden: HTP erzeugt sie schon im Legacy-Fixed-Function-Pfad aus Position und
Texturmatrix. Der VS übernimmt dieselbe Abbildung:

```text
HTP: UV = (WorldPosition * TextureSet[1].m_matTransform).xy
     entspricht CameraPosition * (ViewInverse * TextureTransform)
STP: UV = (WorldX / 640, -WorldY / 640), wie vorhandenes kTexTile
Clip = Position * World * View * Projection
```

HTP-Matrix: Scale(base*UScale,-base*VScale,0), Offset(UOffset,-VOffset,0),
base=1/3200. Auf A1/B1 ergibt Scale5 eine Kachel pro 640 Welteinheiten.
Die bestehende STP-Abweichung (feste Skalierung, ignorierter TextureSet-Offset)
wird nicht korrigiert. Alpha- und Schatten-UVs werden nicht übernommen.

Sampler: U/V/W Wrap, Min/Mag/Mip Linear (trilinear), MaxAnisotropy=1.
Legacy HTP fordert ANISOTROPIC an, setzt aber nirgends MAXANISOTROPY über dessen
Default1; STP erbt Filterstates. Die 3A-Wahl ist explizit, keine anisotrope
Qualitätsmodernisierung. Ein bitidentisches Ergebnis auf jedem Treiber wird nicht
behauptet. Die [Analyse](renderer-milestone3a-analysis.md) enthält die Primärquellen.

Originale DDS-Mips bleiben vollständig erhalten, auch unvollständige Mipketten.
STB hat wie dessen Legacy-Pfad genau eine Stufe. Keine Generierung, kein Bias,
keine Texture Arrays. Der Pixelshader liefert Sample.rgb und Alpha1; Alphablending
ist aus. Der einfarbige M2-PS bleibt für vorhandene isolierte Geometrietests erhalten.

## Ownership und Freigabe

`CMapOutdoor` besitzt genau ein `TerrainTexturePtr`. Die GPU-Ressource besitzt die
Textur und zwei SRBs; der Sampler ist immutable in den PSOs. Die SRBs halten die SRV-
Referenz. Temporäre Pack-/RGBA-Daten leben bis zum synchronen Upload und werden
direkt danach verworfen. Renderer-Zähler halten keine Texturreferenz.

`Destroy` bzw. erneutes `LoadSetting` ruft `ReleaseTexture` auf. Der D3D11-Context
wird über `InvalidateState` entbunden, damit alte SRV-/SRB-Referenzen nicht über den
Mapwechsel hängen bleiben; innerhalb eines Frames werden die Renderziele erneut
gebunden. Danach wird das Map-Handle zurückgesetzt. Der normale Shutdown zerstört
Maps vor TerrainRenderer/Backend. Kein globaler Texturcache.

Der GPU-Test prüft ein aktives Handle, Ablauf einer weak_ptr nach Release,
LiveCount0 und anschließenden Upload einer anderen Textur. Der reale A1/B1/A1-Lauf
zeigt UploadCount1→2→3, aber LiveCount konstant1. Beide Maps benennen dieselbe
Texture001: der Austausch unterschiedlicher Inhalte wird ergänzend im GPU-Test
geprüft. Das ist ein begrenzter Lifetime-Nachweis, kein vollständiger Leak-Audit
aller unveränderten Client-Subsysteme.

## Build und Testprotokoll

Release ON und OFF gebaut, beide erfolgreich. ON-Renderer-Tests 5/5, OFF 3/3.
Die vollständige Suite besteht **9/9 Tests in 460,87 Sekunden**: fullbench,
fuzzer, zstreamtest, playTests, Renderer.StartupOptions, Renderer.TerrainTextureCpu,
Renderer.DiligentD3D11, Renderer.TerrainGpuParity, Renderer.LegacyD3D9.
Bestehende LNK4099-Warnungen zu externen Python/zlib-PDBs sind weiterhin vorhanden;
kein neuer Buildfehler. Die letzte Diff-Prüfung hat nur die ursprüngliche SAL-
Analyseannotation am alten DDS-Einstieg wiederhergestellt (kein Laufzeitcode).
Nach dieser Korrektur wurden beide Builds erneut erfolgreich ausgeführt:
OFF-Renderer-Tests 3/3 in 0,51 Sekunden, abschließender ON-Renderer-Lauf 5/5 in
1,38 Sekunden. Das vollständige 9/9-Protokoll bleibt separat erhalten.

Reproduzierbare Befehle (PowerShell, installierte VS-Toolchain):

```powershell
cmake -S . -B build -DM2_ENABLE_DILIGENT_D3D11=ON
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release -R Renderer --output-on-failure
# OFF analog, anschließend wieder ON konfigurieren/bauen.
```

Die gesamte vorhandene Suite läuft über Git Bash mit `/usr/bin` zuerst im PATH,
damit der vorhandene `playTests`-Shelltest nicht versehentlich Cygwin verwendet:
`ctest --test-dir build -C Release --parallel 3 --timeout 900 --output-on-failure`.
Keine vorhandenen Tests entfernt, übersprungen oder deaktiviert.

### Automatisierte Nachweise

- CPU: BC1/2/3-Blocklayouts, kleine/ungerade Mips, vollständige Original-Mips,
  TGA-Kanal-/Top-Origin-Prüfung; leere/abgeschnittene/ungültige DDS abgelehnt.
- GPU: ursprüngliche M2-Geometrie-, Depth- und Kameratests unverändert erhalten.
- Texturvergleich gegen echtes D3D9: asymmetrische BGRA-Textur, ungleiche U/V-Scale,
  positive/negative Offsets, vier echte Kameraposen mit Rotation/Zoom/Bewegung,
  zwei benachbarte Patches, TriangleList und TriangleStrip.
- Farbcodierte siebenstufige BC1-Mipkette belegt Sampling kleinerer Mips;
  über 90% der bedeckten Pixel stammen nicht ausschließlich aus der roten Basisstufe.
- Mittlerer RGB-Fehler D3D9/D3D11 je Pose: ca. 6,0–7,1/255 bei asymmetrischen
  Kachelgrenzen, ca. 0,20–0,25/255 bei BC1-Mips; Toleranz <12/255. Keine bitweise
  Parität behauptet, D3D9-Halbpixelrasterung ist im Test ausdrücklich berücksichtigt.
- Textur-Freigabe/Ersetzung sowie texturiertes Present nach Resize
  640×480→800×600→320×240→1024×768→0×0→640×480; danach Freigabe/Shutdown.

### Reale Maps und normale Starts

Isolierte Testverzeichnisse unter `build/milestone3a/`, vorhandene echte Packs,
unveränderter Maploader und Anwendungskamera. Kein Serverlogin und keine neuen
synthetischen Maps. Nur das isolierte Test-Root startet `terrain_map_smoke.py`.
Die eigentlichen Runtime-Packs bleiben unangetastet.

- Diligent erster Lauf, PID60336: A1→B1→A1, Texture001 sichtbar; 256×256 DXT1 mit
  **6 Mips** tatsächlich aus dem Pack aufgelöst, nicht der 512er Asset-Override.
  Kamera-/Lageänderung sichtbar. Exitcode0, syserr leer, LiveCount1 bei drei Uploads.
- Legacy-OFF-Build ohne Startargument, PID48424: gleiche Maps/Kamera mit
  ursprünglichen Texturen/Objekten. Minimieren/Wiederherstellen vom Nutzer mit
  „sehr gut“ bestätigt. A1→B1→A1 vollständig, Exitcode0, syserr leer.
- Finaler ON-Build, Diligent-Fenstertest PID58308: erneut A1→B1→A1, 6 Mips,
  LiveCount1/UploadCount3. Nutzer bestätigt korrekt sichtbare Textur nach
  Minimieren/Wiederherstellen mit „ja“. Normaler automatischer Shutdown, Exitcode0,
  syserr leer. Dieser Lauf prüft den abschließenden ON-Rebuild nach OFF.
- ON-Build ohne Renderer-Argument, normale unveränderte Packs, PID51024:
  ursprüngliche Serverauswahl sichtbar, kein Diligent-Diagnosefile angelegt.
  Nutzer beendet über „Beenden“, Exitcode0, syserr leer. Kein Login nötig.
- Normaler Diligent-Start, PID54896: ursprüngliche Serverauswahl sichtbar,
  Diagnose terrain0/textures0 bis zum Laden einer Map. Nutzer bestätigt „funktioniert“
  und beendet über „Beenden“; Exitcode0, syserr leer.

Die Computer-Use-Sichtprüfung bestätigt echte Textur und normale Serverauswahl.
Automatisierte Aktivierung/Minimierung meldete zweimal `failed to activate captured
window`; deshalb beruhen die nativen Minimieren/Wiederherstellen-Ergebnisse auf
der ausdrücklichen Nutzerbestätigung, nicht auf diesen fehlgeschlagenen Eingaben.
Mehrfaches Resize mit Textur erfolgt im echten Presentation-/Backend-GPU-Test;
der normale Client hat keinen frei ziehbaren Größenrahmen. Keine Fensterstyles
wurden nur für einen Test verändert. Kein vollständiger Online-Gameplay-Test und
kein pixelweiser Vergleich ganzer Legacy-Maps: dort Sichtprüfung, Nutzerbestätigung
und unveränderter Legacy-Code, ergänzt durch kontrollierte GPU-Referenztests.

Originale Build-/Test-/Clientlogs verbleiben unter `build/milestone3a*` im lokalen
Source-Checkout. Keine Zugangsdaten werden für die Tests benötigt; die isolierte
Konfiguration verwendet SAVE_ID0, und es wurde kein Login durchgeführt.

## Bewusste visuelle Abweichungen und Erkenntnisse für 3B

Eine einzige Textur ersetzt bewusst noch nicht Gras-/Weg-/Fels-Mischungen. Daher
wirkt das Diligent-Terrain durchgängig braun, ohne Legacy-Beleuchtung/Fog/Schatten.
Steile Hänge strecken die vorhandene XY-Projektion; das ist keine neue UV-Regel.
Bei flachem Blickwinkel ist die bewusst einfache Filterung sichtbar. Fehlende
Objekte/Wasser/Charaktere/UI sind unveränderte M2-Grenzen, keine 3A-Portierung.

Für eine spätere, separat beauftragte 3B-Analyse liegen Farbtextur-Mapping und
Lebensdauer nun vor. Zusätzlich wären aktive Layer/PatchTileCount, Alpha-Koordinaten
und deren Clamp-Sampler sowie originale Blend-Reihenfolge zu übernehmen. HTP/STP-
Unterschiede vorher ausdrücklich entscheiden. Diese Beobachtungen wurden **nicht**
implementiert. Keine Texture Arrays, kein Batching, Streaming oder Threading.

## Git-Abschluss

Alle Änderungen sind uncommittiert. Kein Push, keine Änderung von Branch/History,
kein Deployment und keine Änderung an Runtime-Assets. Der bereits vorhandene
Runtime-Diff `config/channel.inf` wurde nicht angefasst. `git diff --check` ist
fehlerfrei. 14 vorhandene Dateien geändert, 7 neue Dateien: insgesamt 13
Produktionscode-Dateien, 5 Testdateien und 3 Dokumente. Die Dateitabelle oben ist
der vollständige Scope; untracked Dateien sind darin ausdrücklich mitgezählt.

Alle fünf eigenen interaktiven Testclients wurden regulär mit Exitcode0 beendet;
kein eigener Client bleibt offen. Vorher laufende fremde Spielclients wurden nicht
verändert. Abschließender Build bleibt Diligent-fähig (ON); Laufzeitdefault bleibt
trotzdem Legacy. Keine zusätzlichen Modernisierungsschritte begonnen.

**STOP nach 3A. Milestone 3B ist nicht implementiert.**
