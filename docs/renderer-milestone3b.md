# Milestone 3B – originales Terrain-Splatting

> Historischer Milestone-Stand. Aktuell seit [M12](renderer-milestone12.md): Diligent D3D11 ist im ON-Build der Default; Legacy wird explizit mit `--renderer=legacy-d3d9` gewählt. OFF-Builds bleiben Legacy-only. Frühere Testergebnisse und Auswahlbeschreibungen unten gelten für ihren damaligen Stand.

Stand: 11.09.2026. Basis: `f75c0d7` (abgeschlossener Milestone 3A).
Abnahme: Milestone 3B abgeschlossen. Originales Splatting, automatisierte Tests,
echte Mapläufe, Bildvergleich und reguläre Starts/Shutdown erfolgreich geprüft.

## 1–2. Geänderte und neue Dateien

Pfade relativ zum Source-Repository `C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src`.

| Bestehende Datei | Begrenzte Änderung |
|---|---|
| `src/GameLib/AreaTerrain.h/.cpp` | Neutrale Alpha-Besitzer; fertige originale Alpha-Mips vor Unlock kopieren; bei Ersetzung/Unload freigeben |
| `src/GameLib/MapOutdoor.h/.cpp` | Farbtextur-Handles pro Map statt einzelner 3A-Textur; Freigabe |
| `src/GameLib/MapOutdoorLoad.cpp` | Handle-Liste passend zum originalen TextureSet; Laden erst bei tatsächlich benutztem Layer |
| `src/GameLib/MapOutdoorRender.cpp` | Gemeinsame Kameragrenze behalten; Fern-None-Pass übernimmt TFactor-Farbe |
| `src/GameLib/MapOutdoorRenderHTP.cpp` | Submit direkt vor den unveränderten originalen Base-/Overlay-Draws |
| `src/GameLib/MapOutdoorRenderSTP.cpp` | Bestehende fertige Farb-/Nebel-/UV-Attribute übergeben; dieselben Submit-Punkte |
| `src/Renderer/TerrainRenderData.h` | Neutrale Splat-Material-, Attribut- und Draw-Methoden |
| `src/Renderer/TerrainTextureData.h` | Alpha8 als zusätzliches Uploadformat |
| `src/Renderer/DiligentTerrainRenderer.h/.cpp` | R8-SRVs, Material-SRBs, Sampler, originale Mehrpass-Komposition, Statistik |
| `src/Renderer/TerrainPresentation.cpp` | Separate Splat-/Farb-/Alpha-/Material-Zähler im vorhandenen Diagnoseprotokoll |
| `tests/Renderer/TerrainGpuTest.cpp` | Vorhandenen GPU-Test erweitern; bestehende Tests bleiben erhalten |
| `tests/Renderer/TerrainTextureCpuTest.cpp` | Alpha-Kanal, Pitch, A4, Grenzen und Freigabe prüfen |

Neue Dateien:

- `src/Renderer/TerrainSplatData.h`: backendneutrale Parameter und Handles.
- `src/GameLib/MapOutdoorSplat.cpp`: schmale Übersetzung der tatsächlichen Legacy-States.
- `src/GameLib/TerrainAlphaImage.h/.cpp`: Besitz/Kopie fertiger Alpha-Mips, verzögerter Upload.
- `tests/Renderer/TerrainSplatGpuChecks.h`: D3D9-/D3D11-Pixelvergleich mit originaler Alpha-Erzeugung.
- `tests/Renderer/terrain_splat_smoke.py`: isolierter echter Map-Vergleich; nicht vom normalen Client geladen.
- `docs/renderer-milestone3b-analysis.md`: vor Implementierung dokumentierte 20 Legacy-Fragen.
- Dieses Dokument.

Kein CStateManager-Umbau, keine Änderung an Granny, UI, ursprünglichen Drawcalls,
Layer-Skip-Regeln, Terrain-LOD oder Alpha-Glättung. Keine Commits/Pushes vorgenommen.

## 3. Legacy-Call-Chain

```text
CMapOutdoor::LoadTerrain
 -> CTerrain::RAW_LoadTileMap -> CTerrainImpl::RAW_LoadTileMap(tile.raw)
 -> RAW_AllocateSplats -> RAW_CountTiles -> RAW_GenerateSplat
 -> AddTexture32 -> vorhandene Glättung / vier weitere Mips -> PutImage32/16

CPythonApplication::RenderGame -> BeginEnvironment -> CMapOutdoor::OnRender
 -> RenderTerrain -> HTP/STP Patchloop -> RenderPatchSplat
 -> originale Active-/PatchTileCount-Prüfung -> TextureSet[j] + Splats[j]
 -> ursprüngliche States / Texturmatrizen -> DrawIndexedPrimitive
 -> ursprüngliche Splatstatistik und Limitprüfung
```

Der optionale Schattenpass ist weiterhin ausschließlich Legacy und kein Splat-Layer.

## 4. Diligent-Call-Chain

```text
AddTexture32 -> nach PutImage32/16: TerrainAlphaImage::Capture
  (keine zweite Alpha-Erzeugung, noch kein D3D11-Upload)

Originaler RenderPatchSplat-Loop, unmittelbar vor jedem bestehenden Draw:
 -> CMapOutdoor::SubmitTerrainSplat
 -> Farbtextur einmal pro benutztem TextureSet-Index laden
 -> CTerrain::GetSplatMaterial -> TerrainAlphaImage::Material
 -> Alpha8 UploadTexture + CreateSplatMaterial (einmal pro Terrain/Layer)
 -> vorhandene HTP-Matrizen bzw. fertige STP-Attribute und States übernehmen
 -> DiligentTerrainRenderer::DrawSplat
 -> PSO / VB / IB / SRB -> DrawIndexed -> vorhandenes Present
```

Die alte 3A-Test-API bleibt für ihre bisherigen Regressionstests erhalten. Echte
Maps benutzen jetzt die originale Layerliste, nicht mehr überall Texture001.

## 5–6. Layer und Alpha-Format

256 Slots, davon Index 0 Eraser: maximal 255 nutzbare Layer pro Patch. Tatsächlich
begrenzen TextureSet, Active, PatchTileCount, Sichtbarkeit und Splatlimit die Zahl.
Eine Maske gehört zu einem Terrain und Layer, nicht zu einem einzelnen Patch.

Legacy: A8R8G8B8 oder A4R4G4B4, 256×256. Die jeweils tatsächlich geschriebenen
Alpha-Werte werden gelesen. A4 wird mit `(nibble * 17)` nach R8_UNORM expandiert;
die ursprüngliche Quantisierung bleibt erhalten. Keine D3D9-Texture-AddRefs:
deren vorhandene Freigabelogik wird nicht verändert.

## 7–8. UV-Regeln und Texture Transforms

- HTP Farbe: tatsächliche `D3DTS_TEXTURE0 = ViewInverse * TextureSet[j].m_matTransform`.
- HTP Alpha: tatsächliche `D3DTS_TEXTURE1 = ViewInverse * TerrainTranslation * m_matSplatAlpha`.
- Farbe: vorhandenes pro Layer eingestelltes Tiling und Offset, negative V-Achse.
- Alpha: terrainlokaler Raum, vorhandene Skalierung ±1/25600 und Bias 4.6/3200.
- STP: unverändert berechnete `kTexTile` und `kTexAlpha` werden kopiert. Insbesondere
  bleibt dessen feste Farbkachelregel ±1/640 und zusätzlicher Alpha-Ursprungsversatz
  erhalten; sie wird nicht durch die abweichende HTP-Regel ersetzt.
- Geometrie, Kameramatrizen, Indexbuffer, LOD- und Patchauswahl stammen weiter aus M2/3A.

Zusätzlich gleicht ausschließlich das Diligent-Backend die unterschiedliche
Pixelzentrum-Konvention aus: Clip-XY wird um `(1/Width, -1/Height) * ClipW`
verschoben. Welt-/View-Matrizen und UVs bleiben unverändert. Diese Korrektur gilt
auch für die untexturierten Fern-Patches; Resize benutzt die aktuelle Swapchain-Größe.
Hintergrund: [Microsoft, Koordinatensysteme D3D9 gegenüber D3D10+](https://learn.microsoft.com/en-us/windows/win32/direct3d10/d3d10-graphics-programming-guide-resources-coordinates).

## 9. Blend-/Stage-States

HTP verwendet SrcAlpha/InvSrcAlpha + Add, AlphaTest GREATER 0, LessEqual und
DepthWrite. Erster tatsächlich sichtbarer Layer: Farbtextur-Alpha, keine Splatmaske.
Weitere Layer: Alpha aus der jeweiligen Maske. Nicht pauschal „Base immer opak“.
RGB der Farbtextur bleibt erhalten; der originale HTP-Current/Diffuse-Wert ist weiß.

STP übernimmt seine tatsächlichen geerbten Zustände: blend an/aus, Stage1 an/aus,
Diffuse-Alpha bzw. Texture×Diffuse-Alpha sowie BLENDDIFFUSEALPHA mit TFactor.
Die leeren Legacy-Apply/Restore-Funktionen werden nicht repariert oder ersetzt.
Nur die schon vorhandenen Diffuse-/Nebelwerte werden übernommen; kein neues Licht.

Bekannte nicht unterstützte Zustände führen zum bestehenden Diligent-Fehlerpfad,
nicht zu einem stillen Wechsel auf ein anderes Backend. HTP-Base mit ALPHAOPDISABLE
bei aktivem ColorOp ist laut Microsoft undefiniert; die gewählte Interpretation
ist auf dieser Maschine durch tatsächliche D3D9-Pixelvergleiche gedeckt, aber keine
Garantie für beliebige alte Treiber. Siehe Quellen im Analysedokument.

## 10–11. Sampler und Mips

Farbe Wrap, Alpha Clamp; Min/Mag linear entsprechend dem vorhandenen HTP-
Anisotropic-State mit unverändertem MaxAnisotropy 1, Mip linear. Die Brücke kann
auch die tatsächlich geerbten Point-/Linear- und Mip-aus-Zustände übernehmen.
Kein sRGB-Wechsel, keine Gamma-/Materialkorrektur.

Farbtexturen behalten ihre Original-DDS-Mipketten aus dem vorhandenen Loader.
Alpha behält exakt fünf Stufen: 256, 128, 64, 32, 16. Diligent erzeugt keine neuen
Alpha-Mips. Die Tests prüfen jede Stufe und die A4-Quantisierung.

## 12. Resource Lifetime

- Map besitzt benutzte Farbtexturen, Terrain/Layer besitzt Alpha + Material.
- Material hält SRBs und beide Texturen; keine globale, mapübergreifende Cache-Liste.
- CPU-Mipkopien werden nach erfolgreichem Upload/Materialaufbau freigegeben.
- Regeneration, Terrain-Clear, Tile-Unload und Map-Destroy lösen die Besitzer.
- Beim Release werden gebundene D3D11-Ressourcen aus dem Context gelöst; ein
  laufender Frame erhält danach wieder seine RenderTargets.
- Tests verlangen abgelaufene Weak-Handles und null lebende Alpha-/Materialbesitzer
  nach Unload; suspend/resize lässt weiter benötigte Handles gültig.
- Aktueller Client lädt diese Terrains synchron. Keine Aktivierung oder Umgestaltung
  des unbenutzten Hintergrundloaders in diesem Milestone.

## 13. Drawcalls und Diagnostik

Ein Diligent-Draw pro tatsächlich vom alten Loop ausgewähltem Splat-Layer,
zuzüglich original ausgewählter untexturierter Fern-Patches. Kein neuer Pass
für übersprungene/unsichtbare Layer. Keine Texturarrays oder neues Batching.

`terrain-renderer.log` enthält Draws, Splat-Draws, lebende Farb-/Alpha-Texturen,
Materialpaare und kumulative Uploads. Das Map-Testprotokoll ergänzt Patchzahl,
Original-Splatstatistik und tatsächlich ausgewählte Layerindices.
Kumulative Uploads dürfen bei Mapwechsel steigen, die lebenden Besitzer nicht
unbegrenzt wachsen. Schatten sind bewusst nicht in Diligent-Splat-Draws enthalten.

Gemessene fixierte Testansichten, HTP, Sichtweite 25600:

| Ansicht | Sichtbare Splat-Patches | Splat-Draws |
|---|---:|---:|
| A1, (50000,60000,18116.5), Distanz 7000, Pitch 5, Rotation 0 | 43 | 200 |
| A1, gleiche Position, Distanz 7000, Pitch 10, Rotation 95 | 40 | 167 |
| A1, gleiche Position, Distanz 3500, Pitch 45, Rotation 180 | 13 | 64 |
| A1, (54000,63000,19957.0625), Distanz 10000, Pitch 5, Rotation 270 | 46 | 202 |
| B1, (50000,60000,19136.5), Distanz 7000, Pitch 5, Rotation 0 | 43 | 177 |
| Zurück zu A1, erste Ansicht | 43 | 200 |

A1 startet mit 8 Farbtexturen/27 Alpha-Texturen/27 Materialpaaren. B1 hat
11/29/29; nach Rückkehr zu A1 wieder 8/27/27. Die Uploadsumme steigt dabei
von 35 über 81 auf 116, nicht aber die Zahl lebender Besitzer im gleichen Zustand.

## 14–15. Builds und Tests

- Release `M2_ENABLE_DILIGENT_D3D11=ON`: erfolgreich.
- Release `...=OFF`: erfolgreich, danach alle drei dort vorhandenen Renderer-Tests erfolgreich.
- Finale Arbeitskonfiguration wieder ON, Legacy bleibt Standard beim Start.
- Vollständige vorhandene Testsuite: **9/9 bestanden**, 471,40 Sekunden.
  `fullbench`, `fuzzer`, `zstreamtest`, `playTests`, StartupOptions, TerrainTextureCpu,
  DiligentD3D11, TerrainGpuParity, LegacyD3D9. Kein Test deaktiviert.
- Nach den letzten Testerweiterungen nochmals alle fünf ON-Renderer-Tests bestanden.
- Nach der letzten D3D11-Pixelzentrum-Korrektur ebenfalls alle fünf Renderer-Tests
  bestanden. Die nicht-rendernden Library-Tests sind davon unverändert; diese
  finale D3D11-Quelldatei wird im OFF-Build nicht kompiliert.
- Erweiterungen: Original-Alpha-Erzeugung, unbenutzte Layer, A8/A4, fünf Mips,
  Base-Alpha, leere Masken, Layerreihenfolge, HTP/STP, geerbtes Blend-aus,
  Nebel, Resize/Suspend/Restore, CPU-/GPU-Ressourcenfreigabe.

Bekannte bestehende Buildwarnungen: fehlende PDBs der Python-/zlib-Static-Libraries
und Pointer-zu-DWORD-Warnung in MapManager. Nicht Teil dieser Rendereränderung.

## 16. Legacy-Regression

Der Quellvergleich zeigt unveränderte D3D9-Drawcalls und Stateänderungen.
Legacy-Backend, Auswahl beim Start, UI, Granny und Netzwerk bleiben unverändert.
Der echte Legacy-Maplauf ohne Diligent wurde mit Exitcode 0 beendet.
Normaler Start ohne Argument: unveränderte Serverauswahl, vom Nutzer regulär
geschlossen, Exitcode 0. Normaler Start mit `--renderer=diligent-d3d11`:
ebenfalls Serverauswahl, keine Terrain-Ressourcen vor Mapload, regulärer Exitcode 0.
Kein Login erforderlich. Alle gestarteten Testclients sind beendet.

## 17–18. Bildvergleich und verbleibende Unterschiede

GPU-Referenz: 18 Mehrpass-Fälle, neun Ansichten/Zustände jeweils für A8 und A4.
Mittlere RGB-Abweichung über gemeinsam bedeckte Terrainpixel: **0 bis 0,911 / 255**.
Reines HTP-Splatting ohne Nebel: 0 bis 0,000031; leere Masken: 0. Verbleibende
Sub-Graustufen-Abweichungen betreffen die Fog-/STP-Diffuse-Kombinationen. Grenzwert
verschärft auf unter 1,5. Alle getesteten Silhouetten haben **0 abweichende Pixel**.
Die vorhandenen Hochfrequenz-/Mip-Texturproben liegen nach Pixelzentrum-Korrektur
unter **0,003 / 255** statt zuvor 6–7; ihr Grenzwert wurde von 12 auf 0,1 verschärft.

Echte Maps: A1 und B1, dieselben Positionen, Kamerawinkel und Zoomstufen.
Ein erster Lauf benutzt den normalen RenderGame-Pfad inklusive A→B→A und beendet
beide Clients mit Exitcode 0. Diligent-Minimize/Restore wurde vom Nutzer bestätigt.

Für strenge A/B-Bilder setzt das isolierte Skript danach die Sichtweite auf 25600
und übergibt die Kamera unmittelbar vor dem vorhandenen Background-Renderaufruf.
Grund: Die ursprüngliche Anwendung reduziert sonst die Sichtweite abhängig von
der gemessenen Renderzeit. Der spätere PCBlocker-Pass kann ausgeblendete Bäume
erneut zeichnen. Beides ist ein Testaufbau-Unterschied, keine Splat-Korrektur;
Produktionscode hierfür unverändert.

Der finale Diligent-Lauf durch A1 → B1 → A1 wurde ebenfalls mit Exitcode 0 beendet.
Der Terrain-Vergleich nutzt den Computer-Use-Skill zur Fensterprüfung und für
unveränderte Screenshots, nicht für Änderungen an Client-UI oder Desktop-Sicherheit.

Vergleichbares A1-Bildpaar: Original-D3D9 und finale Diligent-Version, gleiche erste
Position und Kamera aus der Tabelle. Fenstergröße 1025×800 einschließlich Rahmen,
Client 1024×768. Metrik über 581754 gemeinsam bedeckte Terrainpixel, ohne Fenster-
rahmen, Hintergrund und sichtbaren Mauszeigerbereich:

- Mittlere RGB-Abweichung **1,077 / 255** (vor Korrektur 8,096).
- 95. Perzentil der Kanalabweichung: **3 / 255**.
- 2,08 % dieser Pixel haben in mindestens einem Kanal mehr als 10 Stufen Differenz.
- Gras/Erde/Fels liegen an denselben Stellen; Tiling-Richtung und große/schmale
  Layer-Übergänge passen. Keine sichtbaren offenen Patch-Nähte oder fehlenden Layer.
- Einzelne feine Texturdetails und Übergangspixel sind nicht bitidentisch. Die
  Messung stammt aus Fenster-Screenshots, nicht aus einem automatisierten Fullmap-
  GPU-Readback. Keine pauschale Pixelidentität aller Maps/Treiber behauptet.
- Der Hintergrund ist im Legacy-Test schwarz, bei Diligent dunkelblau; das ist
  die bereits vorhandene Clear-Farbe bei ausgeblendetem Himmel, kein Terrainfehler.
- B1, weitere Winkel/Zooms und A→B→A wurden live geprüft/protokolliert; der strenge
  gespeicherte numerische Fullmap-Vergleich bezieht sich auf die genannte A1-Ansicht.

Die Originalbilder und Protokolle liegen beim ausgelieferten Bericht unter
`testnachweise/milestone3b/`. Kein Nachschärfen, Resampling oder Retuschieren der
Nachweisbilder; die statistische Auswertung verändert die Bilddateien nicht.

[A1 – Legacy](C:/Users/ZiiNAN/Documents/Codex/2026-09-11/referenced-chatgpt-conversation-this-is-an/outputs/testnachweise/milestone3b/a1-legacy.png)
· [A1 – Diligent](C:/Users/ZiiNAN/Documents/Codex/2026-09-11/referenced-chatgpt-conversation-this-is-an/outputs/testnachweise/milestone3b/a1-diligent.png)

Schatten, Objekte, Wasser, Figuren und UI sind absichtlich nicht Bestandteil der
Terrain-Bildparität. Kein vollständiger modernisierter Spielrenderer zugesagt.

## 19. Spätere Möglichkeiten – nicht implementiert

Erst nach gesonderter Freigabe: weniger redundante Konstanten-/SRB-Bindings,
Pooling pro Terrain-Lifetime, zusätzliche automatisierte Map-Readback-Szenen.
Texture Arrays, neues Batching, Instancing und GPU Culling bleiben ausdrücklich aus.
Keine Gebäudeschritte, keine Charakter-/Granny- oder Beleuchtungsmodernisierung.

## Reproduzierbarkeit und Diff

Build-/Testprotokolle: `build/milestone3b-*.log`.
Isolierte lokale Laufzeitkopien und Bilder: `build/milestone3b/` (nicht ausgeliefert).
Optional erzeugt `M2_TERRAIN_TEST_CAPTURE_DIR` im GPU-Test BMP-Readbacks; keine
Dateiausgabe in normalen CTest-Läufen oder im Produktivclient.
Geprüfte finale Client-EXE (Build und beide Testkopien identisch), SHA256:
`E49ACFF3F40CE35C93A784987223F9DE5F5FD4098DBEA49EE1C3C6D3ABAC6E7F`.
`git diff --check` erfolgreich. 15 bestehende Dateien geändert, 8 neue Dateien
einschließlich Tests und Dokumentation. Die wesentliche Änderung ist der Splat-
Adapter plus Diligent-Mehrpasspfad; die bestehenden Auswahl-/Draw-Schleifen erhalten
nur kleine Hooks. Arbeitsstand bleibt uncommitted.

Abschluss bei 3B. Keine Arbeit am nächsten Milestone begonnen.
