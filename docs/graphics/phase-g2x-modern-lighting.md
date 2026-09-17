> Historischer G1/G2/G3/4-Bericht vom frueheren main. Die damalige eigene Renderer-Implementierung wurde durch den [DiligentFX-Neuaufbau](phase-gdx-diligentfx-rebuild.md) und G8/H2/P0 ersetzt. Dieser Bericht beschreibt keine aktuelle Implementierung oder erneute Abnahme. Bei MAIN-INTEGRATION-X als Historie erhalten.

# G2-X — Modern Lighting

Abnahmebericht, 2026-09-15. Ausgangsstand G1: `1835e66`. **G2-X: GO.** Automatisierte und neue manuelle G2-Abnahme bestanden. Kein Stage/Commit/Push; STOP nach G2.

## 1. Lighting Baseline Audit

Vor Implementierung erhoben:

| Quelle | Runtime-Wert | Shader / Ergebnis in G1 |
|---|---|---|
| `GameLib/MapUtil.cpp`, Environment `direction`, `diffuse`, `ambient` | Separate `DirLights[BACKGROUND/CHARACTER]`; Defaults Richtung (.5,.5,-.5), Diffuse 1, Ambient .5 | `MapManager::BeginEnvironment` setzt Background-Licht; `PythonBackground::SetCharacterDirLight` wechselt für Figuren |
| Environment `Material`, DrawState-Ambient | Material-Diffuse × Licht-Diffuse; Emissive + Material-Ambient × (State-Ambient + Light-Ambient) | `StaticObjectBridge` löst diese Werte pro Draw auf; Licht und inverse-transponierte Normalenmatrix im View Space |
| GR2-/GLB-Vertexnormalen; Model-/Attachment-Worldmatrix | Rigid: inverse transpose(World×View); GPU: gewichtet mit Bone-Matrizen, dann World×View | Classic `VS` berechnet quantisierte Vertexbeleuchtung; G1 `PBRPS` verwendet dieselben draw-lokalen Lichtwerte für GGX und konstantes Ambient |
| G1 BaseColor/Emissive | sRGB-Views, sonst einmalige Shader-Dekodierung; Materialfaktoren linear | G1 PBR linear, finale sRGB-Kodierung in LDR; Material-AO nur Ambient |
| Terrain-Heightmapnormalen, alter Splat-Pfad | HTP 24-Byte-Position/Normal; STP `TerrainPatch::SoftwareTransformPatch_UpdateTerrainLighting` berechnet quantisierte diffuse Vertexfarbe | Diligent-Splat benutzt alte Farbe/Alpha/Fog; vorhandene Meshnormalen ungenutzt. HTP-Texturpfad ohne eigene Directional-Berechnung |
| Native Vegetation-GLB-Auxiliary-Streams | Vorhandene Vertexfarben, Kartenbasis, Wind, optionale Schattenmodulation | `AuxiliaryVS` ersetzt Legacy-Licht mit Vertexfarbe; `PS` multipliziert Diffuse und bestehende Schattenfarbe. Blätter/Fronds ohne Cull |
| Environment-Fog, G0 Fog-Einstellungen | Bestehende Entfernung/Dichte/Farbe | Fog nach PBR-sRGB-Ausgabe; Classic- und Leaf-Fog separat. Kein neuer Fog-/Sky-/Water-Pfad |
| Auswahlbildschirm | Bestehender DrawState-Spotlight | Classic bleibt bestehen; Modern benötigt initialisierte neutrale Szenenwerte auch ohne Map |

## 2. Classic Baseline

Vor Änderungen wurden die zwölf aktuellen Classic-BMPs (drei Market-Stall-Blendkameras, neun F5-Bilder) und der G1-Release-Client unter `build/g2x/baseline` gesichert. Source-HEAD liegt daneben. Finaler Release-Vergleich: **12/12 bytegleich**, einschließlich SHA-256 in [classic-comparison.json](../../build/g2x/classic-comparison.json).

## 3. Architecture

`Map/Environment → Graphics::SceneLightingState → ein Backend-Lightbuffer → PBR / Terrain / Vegetation`. MaterialAsset, Geometry, Normals und SceneLighting sind die Eingaben; der Shader kennt weder GR2 noch GLB, Race-IDs oder Dateinamen. Classic benutzt weiterhin seine bisherigen DrawState-Lichtwerte.

## 4. SceneLighting Model

`src/Graphics/SceneLighting.h/.cpp` enthält `DirectionalLight`, `SceneLighting`, den Adaptereingang `MapLighting`, Validierung und einen revisionsgezählten Zustand. Öffentliche Datentypen bestehen aus Standard-C++-Arrays, Float, Bool und Integer. Keine Windows-, D3D-, Diligent-, Python- oder Provider-Typen. Der Core gehört zum bestehenden CMake-Target `M2GraphicsSettings`.

## 5. Map Environment Conversion

`CMapManager::BeginEnvironment` übernimmt das Background-Directional-Light und das globale Environment-Material. Ein zentraler Adapter negiert die alte Strahlrichtung zur Richtung Oberfläche→Sonne, normalisiert und dekodiert alte Farbeingaben. Direkte Farbe = lineares LightDiffuse × lineares MaterialDiffuse. Sky-Fill = lineares LightAmbient × lineares MaterialAmbient + lineares globales MaterialEmissive, begrenzt auf 1.

A1/B1 speichern ihr Umgebungs-Fülllicht im globalen Material-Emissive, während Background-Ambient null ist. Dessen Übernahme verhindert dunkle Karten trotz vorhandener alter Fülllichtdaten. Material-Emissive einzelner Assets bleibt davon getrennt. Fehlende Environmentdaten und Map-Destroy setzen initialisierte Defaults; der alte Environmentzeiger wird beim Destroy gelöscht. Keine Mapdatei wurde geändert.

## 6. Directional Light

Eine normalisierte Richtung im World Space, Z nach oben, von der Oberfläche zur Sonne. Lichtfarbe linear, Intensität zentral. View-/Kameramatrizen drehen die Sonne nicht mit. Alle Modern-Pfade lesen dieselben Konstanten; für Player und Terrain gibt es keine getrennten modernen Sonnen.

## 7. Linear Lighting

G1-Regeln bleiben erhalten: BaseColor und Emissive aus sRGB-Views, Normal/Roughness/Metallic/AO linear, BRDF und Hemisphere vollständig linear. Erst die Ausgabe wird einmal nach sRGB kodiert. Die vorhandene Fog-Komposition erfolgt anschließend wie bisher in der LDR-Pipeline.

Terrain und Vegetation benutzen jetzt ebenfalls sRGB-Views für Farbeingaben, damit die Dekodierung **vor** Texturfilterung erfolgt. Classic bindet die UNORM-Ansicht derselben Allokation; Terrain-Alpha bleibt linear. Ein GPU-Test vergleicht den gefilterten Schwarz-Weiß-Mittelpunkt mit sRGB 188, also ungefähr linear 0,5, getrennt für Boden, Vegetation und deren Schatten-Farbmodulation. Ein weiterer Fall prüft, dass eine Vertexfarbe 0,5 ein linearer Faktor bleibt und nicht zusätzlich dekodiert wird. Das folgt dem [COLOR_0-Vertrag von glTF](https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/Specification.adoc). Das bereits unterstützte B5G5R5A1-Objektformat ohne sRGB-View behält den vorhandenen Shader-Fallback; dort wird nach der Filterung dekodiert.

## 8. Normal Transformation

Modern transformiert geometrische Normalen mit der invers-transponierten 3×3-Worldmatrix, ohne Translation. Die gemeinsame Shaderfunktion verwendet skalierte Kofaktoren, berücksichtigt das Determinantenvorzeichen und liefert bei singulären Matrizen einen endlichen Normalen-Fallback. Statische Objekte, Actor-Worldmatrix und Attachments folgen derselben Konvention. Classic-Normalenberechnung bleibt erhalten.

## 9. Tangent-space Normals

Tangenten werden als Richtungen transformiert, gegen die transformierte Normale orthogonalisiert; die Bitangente wird mit dem gespeicherten Vorzeichen rekonstruiert. Gespiegelte World-/Skin-Matrizen ändern das Vorzeichen zusätzlich. Vorhandene G1-Tests prüfen gespiegelte UVs und Normalmaps. Neue GPU-Tests prüfen die analytisch erwartete deformierte und gespiegelte Skinning-Tangentbasis. Bei fehlenden Tangenten verwendet der bestehende G1-Fallback Ableitungen von Worldposition und UV.

## 10. GR2 Normals

Native GR2-Vertexnormalen erreichen denselben Materialshader wie GLB. Der Bären-Proof benutzt den vorhandenen Mob 110 mit Original-Geometrie, Diffuse und Animation ohne Sidecar. Kein neues Mesh, keine Normalmap und keine Sonderbehandlung für Bären. [SHA-256 und Vergleich mit Asset-HEAD](../../build/g2x/asset-proof-hashes.json) belegen unverändertes Bären-Mesh, dessen Diffuse und Warrior-Mesh. Schattenseiten und Flächenrichtung werden durch Geometrienormalen, Sonne und Hemisphere sichtbar.

## 11. PBR Direct Lighting

Der vorhandene G1-GGX/Smith/Schlick-BRDF benutzt jetzt `SceneSunDirection` und `SceneSunColor`: N·L, N·V, Half Vector, Metallic und Roughness. Worldposition und Kamera-Worldposition bestimmen den Blickvektor. Die alten draw-lokalen Lichtfelder wurden aus den PBR-Konstanten entfernt; kein zweiter G1-Lichtterm wird addiert.

## 12. Diffuse

Nichtmetallische Diffuse bleibt Lambert mit 1/π, Fresnel-Energieanteil und `(1-metallic)` aus G1. BaseColor moduliert diese Antwort. Terrain/Vegetation verwenden eine begrenzte Diffuse-Foundation mit demselben Sonnen- und Hemisphere-Modell; kein neues Terrain-PBR-System.

## 13. Specular

Die G2-Materialkugeln erweitern die G1-Fixture auf acht Kugeln. Raues/glattes Nichtmetall und raues/glattes Metall stehen nebeneinander; vier Sonnenrichtungen zeigen unterschiedliche Highlightbreite und Stärke. Weitere Kugeln decken Normalmap, AO, Emission und matte Defaults ab. Die Importsemantik bleibt unverändert.

## 14. Legacy Defaults

G1-Defaults bleiben: Metallic 0, Roughness 0,8, AO 1, geometrische Normale, vorhandene BaseColor. Keine Stoff-/Fell-/Metall-Erkennung anhand von Dateinamen, keine neuen Defaultprofile. Das ergibt eine konservative matte Antwort, ersetzt aber keine ausdrücklich authorierten Materialwerte.

## 15. Material Overrides

Das vorhandene `.zmat`-Sidecar-System bleibt die einzige GR2-Override-Quelle. Ein zusätzlicher privater nativer Lauf verwendet das G1-Warrior-Sidecar mit Normal/MR/AO-Fixtures. Das originale `warrior_novice.gr2` wird vor und nach Vorbereitung gehasht. Neue Overrides werden nur im privaten Test-Root erzeugt; Produktionsassets werden nicht migriert.

## 16. Hemisphere Ambient

`lerp(ground, sky, saturate(normal.z*0.5+0.5))` liefert World-Z-abhängiges Fülllicht. Nach unten gerichtete Flächen bekommen Ground-Fill, nach oben gerichtete Sky-Fill. Tests schalten die Sonne aus und prüfen einen deutlichen, endlichen Helligkeitsunterschied. Keine Cubemap, keine globale Normalenaufhellung.

## 17. Map Ambient

Ground übernimmt 40 % des zentral aufgelösten Sky-Fills bei gleicher Farbe. Nur vollständig schwarzes altes Map-Fülllicht bekommt kompatible neutrale Werte 0,03/0,012 linear. Explizite `SceneLighting`-Werte dürfen für Tests oder spätere Steuerung null bleiben. Fehlende Mapdaten verwenden die dokumentierten Standardwerte des Core-Modells.

## 18. Material AO

Material-AO multipliziert ausschließlich die indirekte Hemisphere-Näherung. Die direkte Sonne wird nicht mit AO verdunkelt. Der weiterlaufende G1-Kanaltest trennt direkte Sonne, Ambient und AO; die Materialkugeln zeigen den Kanal zusätzlich visuell.

## 19. Emissive

Authorierte Emissive-Map wird einmal von sRGB nach linear dekodiert und unabhängig von Sonne und AO addiert. Der vorhandene G1-Emissive-Kanaltest bleibt grün. Ausgabe bleibt LDR; kein Bloom oder HDR-Glow.

## 20. IBL Decision

Bewusst nicht implementiert: Environment-Cubemaps, Irradiance, Prefiltered Specular, BRDF-LUT und Reflection Probes. Hemisphere ist lediglich die G2-Ambient-Foundation. Metalle erhalten dadurch keine neue Umgebungsreflexion.

## 21. Shadow Boundary

Keine neuen Shadowmaps, Cascades, PCF/PCSS oder Contact Shadows. Bestehende Alpha-/Shadow-Bindings, Shadow-Modulation und Draw-Reihenfolge bleiben erhalten. Neues Lighting darf vorhandene Schatten nicht als zweite Sonne interpretieren.

## 22. AO Boundary

Kein SSAO/GTAO oder anderer Screen-Space-AO-Pass. Nur der G1-Material-AO-Kanal ist aktiv.

## 23. HDR Boundary

Keine HDR-Render-Targets, Exposure, Tonemapping oder Bloom. Die bestehende LDR-Ausgabe begrenzt Werte auf den darstellbaren Bereich; starke direkte Beleuchtung kann deshalb clippen. Das ist keine HDR-Simulation.

## 24. Intensity Convention

Alle SceneLighting-Farben linear RGB [0,1]. Sun-Intensity ist ein dimensionsloser Multiplikator [0,4], Ambient-Intensity [0,2], jeweils Default 1; keine Lux/Nit-Einheiten. Die GPU-Konstanten enthalten bereits Farbe×Intensität. Kein willkürlicher zusätzlicher Intensitätsfaktor an einzelnen Drawstellen.

## 25. Graphics Settings

Der zentrale G0-Style schaltet Materials **und** Lighting. Im bestehenden Menü heißt die Zeile jetzt „Grafik und Licht“, mit Klassisch/Modern und kurzer Erklärung. Kein redundanter Beleuchtungsschalter, keine neue Config-Version. Die vorhandenen Runtime-Apply-/Persistenzpfade werden weiterverwendet.

## 26. Presets

Low/Medium/High/Ultra behalten ihre vorhandene Auflösung. Der Style bleibt eine eigenständige zentrale Auswahl. G2 implementiert nur eine Modern-Lichtqualität und erfindet keine künstlichen High-/Ultra-Unterschiede. Portable Tests prüfen alle Presets und Style-Persistenz.

## 27. Live Switching

World, Actor, Attachment, Terrain und Vegetation wählen ihren Pfad aus derselben aufgelösten GraphicsRuntimeConfig. PBR-, Terrain- und Vegetations-PSOs werden bei Rendererinitialisierung erstellt. Style-Toggle wechselt gecachte Varianten/Bindings; Shaderkompilierung ist nicht Bestandteil des Toggles.

## 28. Static World

Buildings, Props und Rocks verwenden den normalen Materialpfad samt World-Normalmatrix. GLB-Market-Stall und native A1/B1-Gebäude sind abgedeckt. Wände und Dach erhalten unterschiedliche, geometrisch gerichtete Antwort; der matte Default bleibt für Legacy-Diffuse-only aktiv.

## 29. Actors

Die native Fixture enthält Player, NPC, Bär, Boss, Mount, berittenen Player und animierten GLB. Geometry-/Skeleton-/Animation-Import und GPU-Positions-Skinning bleiben unverändert. Modern verändert die Normalentransformation und Lichtauswertung, nicht die Pose.

## 30. Attachments

Waffe und Hair stehen im Player-A/B und in bestehenden F5-Hand-Attachment-Tests. Sie verwenden denselben SceneLightbuffer und dieselbe World-Space-Konvention. Keine separate Sonnenrichtung für Kopf, Waffe oder Mount.

## 31. Animated GLB

Der F5-Character kombiniert native GLB-PBR-Materialien, unabhängige Animation, GPU-Skinning und SceneLighting. Bestehende sechs Clip-Proofs, Referenzpose, Hand-Attachment und 20-Actor-Test bleiben erhalten. Der native Lauf zeigt Idle sowie kurze Walk-/Attack-Stufen; hierfür wird die für A/B eingefrorene Pose wieder aktualisiert.

## 32. Terrain

Der produktive Legacy-Splat-Pfad liest die bereits vorhandenen Heightmapnormalen aus dem 24-Byte-Geometriestream. Modern benutzt dessen Normale, sRGB-Farbtextur und gemeinsame Directional/Hemisphere-Beleuchtung. Die zuvor gebackene STP-Diffuse-Farbe wird dort nicht nochmals als Licht multipliziert. Alpha, Splats, Sampler, Blending und Fog bleiben erhalten.

Classic behält sein ursprüngliches Layout und den ursprünglichen Shaderzweig. Ein GPU-Test variiert SceneLighting und fordert identisches Classic-Terrainbild. Die diagnostischen Solid-/Single-Texture-Pfade erhalten kein neues Terrain-Materialsystem; der produktive Splat-Pfad ist G2s Integrationspunkt.

## 33. Vegetation

Native ZiiNAN-Vegetation benutzt vorhandene Auxiliary-Streams, Wind, Leaf-Card-Basis und Texturen. Die Normale folgt Wind/Card-Transformation und der World-Normalmatrix. Vertexfarbe bleibt bestehender linearer Modulationsfaktor; die neue Lichtantwort kommt aus SceneLighting. Eine zunächst zusätzliche sRGB-Dekodierung dieses Faktors dunkelte Blätter zu stark ab und wurde nach der Baum-Nahaufnahme korrigiert; der neue Pixeltest verhindert diese Regression. Keine neuen Bäume, Gräser oder Vegetation-Materialdaten. A1/B1, eine gezielte B1-Baum-A/B-Aufnahme und bestehende Native-Vegetation-Paritätstests sind abgedeckt; SpeedTree bleibt entfernt.

## 34. Double-sided Lighting

Modern behält bei ausgeschaltetem Culling dieselbe Front-Winding-Konvention wie die übliche Frontseite. Ein während der Abnahme gefundener G1-Fehler wurde korrigiert: Cull=None durfte nicht die Definition von Front/Back vertauschen. Ein exakter Bildvergleich zwischen Cull=None und der passenden einseitigen Variante deckt dies ab; Classic-Rasterzustände bleiben unverändert.

Auf Backfaces wird die **fertige** geometrische oder normalgemappte Normale invertiert. Erst innerhalb der TBN-Rekonstruktion nur N umzudrehen wäre bei Normalmaps falsch. Dünne Flächen und Vegetationsrückseiten erhalten so konsistente Beleuchtung plus Hemisphere. Referenz: [glTF 2.0 – Materials/doubleSided und Transformationskonventionen](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html).

## 35. Light Rotation Test

`Lighting.Render` prüft Front/Side/Back/Top mit bekanntem N·L gegen echte D3D11-Pixel. Ein Camera-Roll bei konstantem World-Licht ändert den zentralen Pixel höchstens um die erlaubte Quantisierung. Die acht Materialkugeln werden zusätzlich mit vier World-Sonnenrichtungen aufgenommen; Original-BMPs in der Galerie.

## 36. Normal Matrix Test

Eine gedrehte, ungleichmäßig skalierte Fixture wird gegen eine analytisch erwartete Normalenfarbe geprüft. Separate Translation muss das Ergebnis unverändert lassen. Gespiegelte Worldmatrix plus Backface-Policy wird ebenfalls gegen bekannte Komponenten geprüft. Diese Tests lesen den echten Produktionsshader aus, keine CPU-Nachbildung als Rendererersatz.

## 37. Skinned Normal Test

Eine echte GPU-Palette deformiert einen Arm-Fixture mit nichtuniformer Bone-Matrix. Erwartete Normalen werden nach Posewechsel numerisch geprüft; weitere Fälle aktivieren eine authorierte Tangente und Normalmap sowie eine Spiegelung. Das prüft Normalmatrix, Tangentenrichtung, Bitangent-Sign und vollständigen Backface-Flip gemeinsam. SkinVertex-Positionen und bestehende F5-Positionsparität bleiben erhalten.

## 38. Legacy Mob Proof

[Galerie, Bereich B](../../build/g2x/visual-proof.html): derselbe native Bär, dieselbe Diffuse, dieselbe Kamera und eingefrorene Pose, Classic neben Modern. Modern zeigt mattere gerichtete Flächen und lesbare Schattenseiten. Der Diffuse-only-Lauf muss `MaterialOverrides=0` erfüllen. Damit ist die Lichtverbesserung ohne Asset-Remaster belegt.

## 39. Building Proof

Die B1-A/B-Paarung zeigt bestehende Dach-/Wandflächen zusammen mit Vegetation. Geometrie/Textur/Environment sind zwischen den Aufnahmen gleich. Keine Kategorieerkennung, kein erzwungenes Metall. Originalbilder sind in voller Größe verlinkt; die alte Gebäudedetailqualität bleibt sichtbar.

## 40. Player Proof

Der native Nahvergleich enthält Player mit bestehender Rüstung/Haut, Haar und Waffe. Zusätzlich gibt es denselben Vergleich im separaten privaten G1-Sidecar-Lauf. Die Standardaufnahme benutzt konservative Roughness und Metallic 0; kein Chrom-/Plastikprofil wird automatisch zugewiesen.

## 41. Light Buffer

Ein 64-Byte-Constantbuffer je Backend: vier float4 für SunDirection, SunRadiance, SkyRadiance und GroundRadiance. Backendinitialisierung erzeugt ihn, alle Modern-PSOs binden ihn, Shutdown gibt ihn frei. `SyncSceneLighting` lädt nur bei geänderter Revision hoch. `LightBuffers`, `LightBufferUpdates`, `SceneLightingResources` und `LightingPipelines` sind auditierbar.

## 42. Hotpath

Environment-Auflösung erfolgt am bestehenden Map-/Environment-Einstieg; `SetMap` cached die rohen Eingaben und überspringt unveränderte Konvertierung. Pro Modern-Draw bleibt ein einfacher Revisionstest. Kein Lesen/Parsen von Mapdateien, keine Farbumrechnung und keine Konstruktion eines SceneLighting-Objekts je Actor. Materialauflösung, PSO- und Textur-Caches aus G1 bleiben aktiv.

## 43. Validation

Neue Tests: `Lighting.Validation`, `MapConversion`, `Revision`, `Settings`, `HeaderBoundary`, `Render`, `MaterialBalls`. Geprüft werden Null-/NaN-/Infinity-/FLT_MAX-Richtungen, negative und nichtfinite Intensitäten, ungültige Farbkanäle, fehlende Mapdaten, einheitliche Normalisierung, unterschiedliche A→B→A-Werte, unveränderte Revision bei identischen Eingaben, Presets und neutrale Header. Farb-/Intensitätsvalidierung geschieht vor dem GPU-Upload.

## 44. Classic Pixel Regression

**12/12 bytegleich zur vorab gesicherten G1-Baseline.** Drei Blendkamera-Bilder des statischen GLB und neun F5-Character/Attachment/Window-Bilder. Der Vergleich prüft vollständige BMP-Dateien samt SHA-256; keine „fast gleich“-Toleranz. Zusätzlich prüft `Lighting.Render` Classic-Terrain und Classic-Vegetation bei wechselnden modernen Lichtwerten bytegenau.

## 45. Modern Visual Proof

[A/B-Galerie](../../build/g2x/visual-proof.html) enthält Materialkugeln, Legacy-Bär, Legacy-Player, Gebäude, animierten PBR-GLB, Vegetation und Terrain. Dazu originale Normalenbilder, Walk-/Attack-Stufen und das native Grafikmenü. Bilder werden weder retuschiert noch nachbelichtet. Ein Klick öffnet die Originaldatei.

## 46. A/B Method

Private Test-Roots teilen unveränderte Produktionspacks nur lesend; lediglich ihr eigenes Root-Paket enthält das Testscript/optionale Sidecar. Beide Bilder eines Paares verwenden dieselbe Kamera, Position, Map und SceneLighting-Werte. Actor-Updates pausieren zwischen A/B-Captures; diagnostisch wird die vorhandene Windzeit auf 1,25 s fixiert. Diese Funktion ist nur im Renderer-Testbuild verfügbar.

Die nativen Aufnahmen sind JPEG-Readbacks, keine pixelgenauen Animation-Goldens. Der exakte Classic-Nachweis benutzt die separaten verlustfreien BMP-Gates. Die anfänglichen ungeeigneten Unterwasser-Fixturepositionen wurden für die finalen Belege auf trockene Standorte verschoben, ohne Mapassets zu verändern.

## 47. Performance Sanity

Die [kurze Messung](../../build/g2x/performance-summary.json) verwendet je 30 warme Frames am Ende derselben A1-Paarung, Classic/Modern mit fester Kamera/Pose/Environment. Erfasst sind CPU-Process, CPU-World, Wall-Frame, vorhandene GPU-Frame-Zeit und Actor-Drawcount. `draws` im vorhandenen Benchmark zählt Actor-Submissions, nicht sämtliche World-/UI-Pässe; Rohdaten bleiben erhalten. Es ist eine kleine lokale Plausibilitätsmessung, keine allgemeine FPS- oder Optimierungszusage.

Der kontrollierte 20-Draw-GPU-Test hält 24 PBR-PSOs, einen Lightbuffer und konstante Lighting-PSOs über Classic→Modern→Classic→Modern. Nach Warmup entstehen **0 zusätzliche Lightuploads**. Die CPU-Submit-Zeiten dieses Tests sind getrennt von tatsächlichen nativen Frame-Zeiten zu verstehen.

| Finale A1-Paarung, Median | Classic | Modern |
|---|---:|---:|
| CPU Process ohne Present/Framelimit | 503,0 µs | 496,8 µs |
| CPU World | 364,9 µs | 367,1 µs |
| GPU-Frame-Zeit des vorhandenen Queries | 32,112 µs | 40,192 µs |
| Wall-Frame einschließlich VSync/Limit | 16,456 ms | 16,515 ms |
| Actor-Draws pro Frame | 20 | 20 |

Die GPU-p95-Werte liegen bei 192,768/199,808 µs und zeigen die Streuung dieser kurzen Stichprobe. Beide Stile bleiben in diesem Lauf nahe dem vorhandenen 60-Hz-Limit. Hauptprozess inklusive mehrerer Style-/Map-Wechsel: **4** Lightbuffer-Uploads insgesamt; der unveränderte warme Scenezustand lädt nicht pro Actor erneut hoch.

## 48. Multi-Actor

Der vorhandene F5-Test rendert 20 unabhängige GLB-Actors und prüft nach zwei Modern-Passes unveränderte SceneLight-Uploadzahl und genau einen Buffer. Assetsharing, eigene Animation und bisherige Pose-/Draw-Nachweise bleiben erhalten. Ein zweiter kleiner Test prüft 20 Submissions über mehrere Style-Wechsel und unveränderte PSO-Zahlen. Ressourcen werden anschließend vollständig freigegeben.

## 49. Map Transition

Nativer Ablauf A1→B1→A1, jeweils mit Modern-Captures; Destroy setzt den Scenezustand zurück und löscht den alten Environmentzeiger. Die Captures zeichnen Richtungen/Farben/Revisionen auf. A1 und B1 liefern in diesen vorhandenen Assets identische Lichtfarben/-richtung; daraus wird kein Beweis für unterschiedliche Kartenfarben abgeleitet. Der portable A→B→A-Test verwendet ausdrücklich unterschiedliche Werte und prüft deren vollständige Wiederherstellung.

## 50. Settings Persistence

Der automatisierte native Hauptprozess endet mit Modern, Prozess 2 prüft dessen gespeicherte vollständige Settings und beendet mit Classic, Prozess 3 prüft Classic nach echtem Neustart. Keine Umdeutung eines In-Process-Toggles als Restart. Die zentralen G0-Persistenztests laufen zusätzlich in den Gates.

## 51. Release

Finaler Windows-x64/D3D11-Release-Build bestanden. FAST GATE **73/73**, 53,30 s: [Gate](../../build/g2x/release-accepted-gate.log), [Build](../../build/g2x/release-tint-build.log). Bekannte PDB-/Linkerwarnungen bleiben sichtbar. Frühere Durchgänge und korrigierte Implementierungs-/Testfehler werden nicht als finaler Nachweis verwendet. Kein Stress-/Fuzz-/Langlauf.

## 52. Debug

Finaler Debug-Build und FAST GATE **73/73** bestanden, 128,59 s: [Build](../../build/g2x/debug-tint-build.log), [Gate](../../build/g2x/debug-accepted-gate.log). Bekannte LNK4099-PDB-Warnungen sowie LNK4098/LNK4075 werden nicht als warnungsfreier Build bezeichnet.

## 53. GCC/LP64

GCC 12/Cygwin, LP64: Build bestanden, portables FAST GATE **43/43**, 2,33 s. [Build](../../build/g2x/gcc-build.log), [Gate](../../build/g2x/gcc-gate-verified.log). Alle fünf portablen Lighting-Tests enthalten keine Windows-Rendererabhängigkeit. Ein eingeschränkter Erstlauf konnte die Restart-Datei nicht schreiben; dessen abhängiger Read-Test war NOT RUN. Im etablierten Buildkontext bestanden beide; kein Produktfehler. Die spätere Texturview-Korrektur betrifft ausschließlich Windows-Renderer und GPU-Tests.

## 54. GR2 Regression

Native Reader-, Animation-, Render- und Regressionsgates bleiben Teil der Prüfung. Native Player/Mob/Boss/Mount/Hair/Weapon/Building-Coverage in beiden Styles; Classic-Goldens unverändert. Kein neuer Granny-Pfad, kein CPU-Deformationsfallback und keine Asset-Konvertierung als Voraussetzung für Modern Lighting.

## 55. GLB Regression

Statischer GLB-Market-Stall inklusive Blendkameras und animierter F5-Character inklusive sechs Clips, 20 Actors und Attachment werden weiter geprüft. G1-PBR-Importsemantik und Animation bleiben erhalten; die neue SceneLightquelle liegt hinter der Providergrenze.

## 56. Vegetation Regression

Die bestehenden portablen Asset-/Ownership- und GPU-Paritätstests laufen weiter. Native A1/B1 erzeugen Branch-/Frond-/Leaf-/Billboard-Draws. Modern-Farben kommen aus denselben originalen Texturen; Classic-Auxiliary-Shader bleibt isoliert. Wind/Card-Normalen sind eine Foundation auf der bisherigen Geometrie, kein neuer Leaf-Shading- oder Transmissionsansatz.

## 57. G1 Material Regression

Alle neun G1-Materialtests bleiben im 73er Windows-Gate. Kanaltests prüfen weiter BaseColor, Normal, Roughness, Metallic, AO, Emissive, Alpha-/Depth-/Camera-Bindings und GR2-Sidecar. Die Test-Lichtquelle wurde von DrawState auf die neue SceneLighting-Schnittstelle gesetzt; die semantischen Kanalprüfungen wurden nicht abgeschwächt.

## 58. Runtime Smoke

Automatisierte native Läufe und menschliche Login-Abnahme werden getrennt erfasst. Der Offline-Proof benutzt denselben Release-Client und echte native Assets, ersetzt jedoch keinen Serverlogin. Private Roots, Binary-SHA, Exitcodes, Captures, Settings und Ressourcenaudits stehen in [runtime-summary.json](../../build/g2x/runtime-summary.json).

| Finaler nativer Proof | Prozesse / Captures | Ergebnis |
|---|---|---|
| `accepted-defaults`, kein Sidecar | 3 / 19+2+2 | PASS, Exit 0/0/0, Ressourcen jeweils 0 |
| `accepted-override`, G1-Warrior-Sidecar | 3 / 19+2+2 | PASS, Exit 0/0/0, Ressourcen jeweils 0 |
| `accepted-vegetation`, B1-Baum A/B | 1 / 2 | PASS, Exit 0, Ressourcen 0 |

Alle drei verwenden denselben Release-SHA-256 `B8AF2CE4A716BFEFD8D88D4A5A87B6704591EEFB5157AE8D2AF11528F2AF028D`, ebenso der normale manuelle Testclient. Die sieben automatisierten Prozesse haben leere syserr-Dateien. Gültige Modern-Captures zeigen PBR-, Terrain- und Vegetationsaktivität; der reine Classic-Neustart benötigt keine PBR-Draws.

**Manuelle G2-Abnahme: PASS.** Der Nutzer bestätigte für den neuen G2-Test ausdrücklich: „Ja, alles fehlerfrei geprüft und Client geschlossen“. Die Abfrage umfasste normalen Login, Charakterauswahl, mehrfachen Live-Toggle, Figuren/Mobs, Gebäude, Bäume/Boden, Bewegung/Kampf in beiden Styles, Resize/Minimize/Restore mit Modern sowie Charakterauswahl→Ingame und normales Schließen. Die frühere G0-/G1-Bestätigung wurde dafür nicht verwendet.

`manual-accepted`: **Exitcode 0**, sämtliche erfassten Ressourcen und acht Renderer-Shutdownzeilen 0. Aktivität: 334.178 PBR-Draws, 439.933 Modern-Terrain-Draws, 134.280 Modern-Vegetation-Draws, 64.052 GPU-Frames. CPU-Deformation, GPU-Fallbacks, Skin-/Animation-/Vegetation-Fehler jeweils 0. [Manueller Nachweis](../../build/g2x/manual-acceptance.json), [Ressourcenaudit](../../build/g2x/runtime/manual-accepted/source-resource-audit.log).

Die manuelle syserr-Datei ist **nicht leer**: ein vorhandenes `MarkManager.cpp`-`invalid idx 0` beim Login sowie bestehende Damage-Effekt-Diagnoseausgaben während des Kampfs. Keine neuen Asset-/Material-/Vegetation-/Python-Fehler im Lauf. Diese Baseline-Diagnostik wurde nicht als G2-Reparatur ausgegeben.

## 59. Live Toggle

Der native Proof bedient den bestehenden Grafikdialog und prüft den zentralen Style nach jedem Wechsel; mehrere A/B-Paarungen wechseln Classic→Modern→Classic→Modern. PBR-/Terrain-/Vegetation-Drawzähler belegen die Modern-Ausführung. Die 20-Draw-Fixture prüft zusätzlich konstante Pipeline- und Uploadzahlen über warme Wechsel.

## 60. Resize/Minimize

`Lighting.Render` prüft Backend-Suspend (0×0) und Wiederherstellung mit Modern. Der native Client ruft die bestehende echte Fensterdiagnose in Modern und Classic auf und verlangt alle Rückgabewerte erfolgreich. Die zusätzliche menschliche Bedien-/Darstellungsprüfung mit Modern wurde ausdrücklich bestanden bestätigt; siehe Runtime Smoke.

## 61. Shutdown

Das native Script verlangt Exitcode 0 für alle drei Prozesse je Lauf und prüft anschließend SceneLightingResources/LightBuffers/LightingPipelines, Materialobjekte/Bindings/PSOs, Source-Texturen/-Buffer, GR2-, Vegetation-, Collision- und weitere Ressourcen auf 0. Acht separate Renderer-Shutdownzeilen müssen ebenfalls vollständig 0 sein. Kumulative Aktivitätszähler wie Draws, Uploads und Reads dürfen natürlich positiv bleiben.

Finales Ergebnis: **sieben automatisierte Prozesse plus ein normaler manueller Client, achtmal Exitcode 0**, Ressourcen in jedem dieser Prozesse 0. PBR-Texturen sind in den Source-Texture-Besitzerzählern enthalten; keine separate ungezählte GPU-Texturallokation für sRGB-Views.

## 62. Resource Lifetime

SceneLightingState besitzt nur Werte, keine dynamische CPU-Ressource. Der Backendbuffer wird einmal erzeugt und nach den Renderern zerstört. Modern-PSOs und Textureviews gehören ihren bestehenden Renderer-/Texturebesitzern. Sampler-/Binding-Caches geben Referenzen beim Release frei. Der GPU-Test verlangt nach Shutdown zusätzlich MaterialRuntime, PBR-PSOs, SkinMeshes, BonePalettes und BoneRemaps 0.

Während der Implementierung wurde ein zu kleines Vegetation-Binding-Array korrigiert: drei Gruppen à zwölf Varianten benötigen 36 Einträge. Der ursprüngliche GPU-Absturz ist durch final erfolgreiche Tests und native Läufe abgedeckt; kein stiller Rendererfallback.

## 63. Known Limitations

- LDR begrenzt Highlights; keine physikalischen Lux/Nit, IBL, neue Schatten, SSAO/GTAO, HDR/Bloom oder Sky-/Water-Änderungen.
- Legacy-Diffuse sagt nichts Sicheres über Stoff/Fell/Metall aus; konservativer Default bleibt nötig, Detailqualität der Assets bleibt unverändert.
- Globales Environment-Material-Emissive ist kompatibles Szenenfülllicht. Hemisphere ist eine Näherung, keine indirekte Lichtsimulation.
- Terrain bleibt Legacy-Splat; Vegetation behält vorhandenen Vertex-Tint und Card-/Wind-Geometrie. B5G5R5A1 ohne sRGB-View behält den G1-Dekodierfallback.
- Native JPEG-A/B-Aufnahmen sind visuelle Belege; exakte Classic-Regression wird separat mit BMPs geprüft. A1/B1 haben in diesem Datenstand identische Lichtparameter.
- Die kurzen lokalen Zeitmessungen sind keine umfassende Leistungs- oder Hardwareabnahme. Bekannte Buildwarnungen bleiben dokumentiert.
- Manuelle Login-/Darstellungsabnahme wurde ausdrücklich für G2 bestätigt; vorhandene MarkManager-/Damage-Diagnostik bleibt sichtbar und ist keine behobene G2-Regressionsmeldung.

## 64. Git Diff

[Source-Patch](../../build/g2x/source.patch), [G2-only-UI-Patch](../../build/g2x/client-ui-g2-only.patch), [Zusammenfassung](../../build/g2x/diff-summary.txt). Source-Patch gegen `1835e66`, einschließlich neuer neutraler Coredateien, Rendereranbindung, Tests, kleiner absichtlicher Materialkugelfixture (156.004 Bytes) und dieses Berichts.

UI-Patch vergleicht gegen die vorab gesicherte G1-Menüdatei, da diese im Asset-Checkout bereits vor G2 untracked war. Vorbestehende Redthief-/Root-/Channel-Änderungen bleiben erhalten. Buildverzeichnisse, Logs, Screenshots und Helferscripte bleiben private Belege; keine gestagten Dateien, kein Commit/Push.

## 65. GO/NO-GO

**G2-X: GO.** Zentrale neutrale SceneLighting-Quelle, Map-Konvertierung, World-Space-Directional-/Hemisphere-Lighting, PBR/Normalmaps, Legacy-GR2, Player/Attachments, animierter GLB, Terrain und Vegetation sind implementiert und geprüft. Release **73/73**, Debug **73/73**, GCC/LP64 **43/43**, Classic **12/12 bytegleich**, alle sieben automatisierten nativen Prozesse und die neue manuelle G2-Abnahme sind bestanden. Alle acht Client-Prozesse enden mit Exitcode 0 und erfassten Ressourcen 0. Die dokumentierten Grenzen betreffen bewusst spätere Milestones und bestehende Diagnostik; keine offene G2-Abnahme bleibt.

## 66. Recommendation G3-X

Nach G2 stoppen. Die zentrale, validierte World-Space-Sonne und der gemeinsame Lightbuffer können einem separat beauftragten G3-X als Grundlage dienen. Vor G3 zuerst bestehende Shadowquellen, Receiver und Reichweiten auditieren. In G2 wurden keine neuen Shadows, Cascades oder Filter implementiert; G3 wird nicht begonnen.
