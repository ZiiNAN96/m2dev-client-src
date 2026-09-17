# H2-X — Vegetation 2.0

Status: **Automatische H2-Gates PASS; H2-X-FINAL im Originalclient bis zur Klärung der Mapwechsel-Evidenz noch NO-GO.** Zwei frische Netzwerk-/Ingame-Läufe im Originalordner endeten mit Exit 0 und sämtlichen erfassten Shutdown-/Fallback-/Diligent-Zählern 0. Der Nutzer bestätigte die manuelle Restprüfung als fehlerfrei. Beide erfassten Läufe enthalten jedoch nur C1 und keinen weiteren Kartenladevorgang; die geforderte A1/B1-Wechselfolge ist deshalb noch nicht als belegt markiert. Kein Commit, kein Push.

**Production-Audit vom 16.09.2026:** Release-EXE und 342/342 Root-Paketeinträge stimmen mit dem freigegebenen H2-Stand überein. Die noch alte D3D9-Debug-EXE im Originalordner wurde nach verifizierter Sicherung gezielt durch den geprüften H2-Debug-Build ersetzt. Beide Original-EXEs bestehen nun den Legacy-Import-/SDK-Marker-Abgleich. [Milestone-Matrix, Originalclient-Proof und offene Abnahmepunkte](../../../m2dev-client/docs/production-milestone-deployment.md). Keine funktionale Sourceänderung, keine neuen Features, keine Golden-Neuerzeugung.

Belege liegen unter `build-h2x/`. Die dortigen Builds, Laufzeitkopien, Logs und Bilder sind lokale, ignorierte Artefakte. Die reproduzierbaren Quellen, Tests und originalen Demonstrationsassets gehören zum Diff.

**Nutzerkorrektur vom 16.09.2026:** Gras wird jetzt für die gesamte Karte beim Laden vorbereitet; die Sichtweite wurde vergrößert. [Änderung und aktuelle Nachweise](h2x-grass-preload.md). Maßgebliche neue Gates, Bilder, Messungen und Dateihashes liegen unter `build-h2x/grass-preload/`. Die früheren G8/H2-Vergleichszeiten in Abschnitten 63–68 dokumentieren den Stand vor dieser Nachkorrektur und sind keine Messung der größeren Grasreichweite.

## 1. Baseline

Vor Änderungen: Source-Branch `codex/g56-hdr-atmosphere`, HEAD `02e0ec7` (G8 final), vorher `8d42cce` (CLEAN-X), Working Tree clean. G8-Bericht enthält das finale Nutzer-GO. Separates Asset-Repository: `codex/g56-hdr-colors`, `6c713237`, clean. Baseline-Release-SHA256: `4c362615c0b3b008a95f574a7e3591ad7059cee95cf96e3e681ea818c0212a8c`.

## 2. Architecture

Registry → ZVEG → GLB → vorhandener GlTFProvider/AssetRuntime → VegetationRuntime → DiligentStaticObjectRenderer/ModernRenderer. Keine zweite Tree-, Bush- oder Grass-Engine. Diligent liefert Instanzbuffer, Instanced Draws, Shader, Material- und Shadow-Passes. Keine neue Dependency.

## 3. Legacy Compatibility

Der reale Corpus enthält 118 konvertierte Typen. `Vegetation.AllLegacyTypes` lädt alle 118 und prüft ihre Nah-/Fernzustände. Der frisch ausgeführte PackAudit und Content-Report finden 85 produktiv referenzierte Typen, 81 platzierte Typen, 12.819 Platzierungen, unsupported=0. Vier weitere Typen sind Actor-/Event-Referenzen. Keine Mapdaten oder Positionen geändert.

## 4. Modern Asset Override

Optionales `modernOverrides`-Objekt in der bestehenden Registry, nur für vorhandene Legacy-Keys. Im privaten Testpaket zeigt allein `d:/ymir work/tree/b1_beech_rt4.spt` auf `vegetation/modern/beech.zveg`. Die ursprüngliche Zuordnung bleibt erhalten. Classic wählt weiterhin die konvertierte Version.

## 5. VegetationAsset 2.0

V2 ergänzt PlantKind, drei LOD-Distanzen, TransitionFraction und Foliage-Transmission. Geometry, Materialbeschreibungen und Texturen verbleiben im GLB/AssetRuntime. V1 bleibt lesbar. Validatoren prüfen endliche Werte, geordnete Distanzen, nicht überlappende Übergänge, Bounds inklusive Wind und vorhandene Meshreferenzen.

## 6. Instance Data

Gemeinsamer Asset-Handle, Transform, LOD-Zustand und deterministische Windphase. GPU-Datensatz: 80 Byte (4×4 Transform plus Phase, Transition, AlphaCutoff und Distanzdeckung). Keine Mesh-, Material- oder Texturkopie pro Instanz.

## 7. GPU Instancing

Batching nach Asset, LOD/Mesh, Material/PSO, Übergangsseite und Transform-Winding. Diligent-Input-Slot 3 ist per instance; `DrawIndexedAttribs.NumInstances` wird gesetzt. Auch konvertierte V1-Vegetation nutzt unter Modern die gemeinsamen Draws.

## 8. Instance Buffers

Geteilte dynamische Vertexbuffer mit Größenreserve; Upload nur bei geänderten sichtbaren Daten. Absolute Windzeit erfordert keinen Transformupload. Color- und Shadow-Sichtmengen besitzen getrennte Buffer. Verzögert gezeichnete transparente Camera-Blocker halten kurzlebige eigene Buffer, damit spätere Blocker ihre Daten nicht überschreiben. Der GPU-Test vergleicht Batch und getrennte transparente Instanzen pixelgleich.

## 9. Culling

CPU-Culling verwendet transformierte Renderbounds, sechs D3D-Frustumebenen und Asset-/Qualitätsdistanz. Shadow-Caster nutzen zusätzlich die bestehende Light-Caster-Sichtbarkeit. Gras hat eine gesonderte enge Distanzgrenze. Keine neue Compute-/GPU-Culling-Architektur.

## 10. LOD System

V2 unterstützt vier authored Zustände: High, Medium, Low und Far. Alle liegen im selben GLB. V1 behält seine bisherige Sample-/Alpha-Semantik.

## 11. LOD Distances

Buche: 5.750 / 12.650 / 21.850 Spielzentimeter, Cull 41.400; multipliziert mit der bestehenden Vegetation-Qualitätsdistanz. Gras und Busch besitzen eigene assetlokale Werte. Das 23 m hohe Baum-Demo orientiert sich an der gemessenen Legacy-Höhe von etwa 23,16 m des ausgewählten Typs. Die Galerie dokumentiert die Kameraabstände; künstlerische Feinabstimmung bleibt Gegenstand der finalen Sichtprüfung.

## 12. Crossfade

Komplementärer 4×4-Dither in schmalen 12-%-Distanzbändern; außerhalb jeweils eine Stufe. Color und Shadow verwenden dieselbe Alpha-/Ditherlogik. Gras-Distanzdeckung ist unabhängig vom LOD-Wechsel, damit die eingehende Stufe am Cullrand nicht wieder voll erscheint.

## 13. Impostors

Aufrechte kameraorientierte Far-Card mit acht Ansichten. Auswahl berücksichtigt Kamera und Instanzrotation. Alpha und Grundbeleuchtung bleiben aktiv; UVs bleiben innerhalb der ausgewählten Atlaszelle. Kein voller Branch-Wind im Far-Zustand.

## 14. Impostor Generation

Optionales Offline-Tool `ZiiNANModernVegetationAssets` rasterisiert acht 192-Pixel-Ansichten aus High-LOD-Geometrie und Alpha. Reproduzierbare Seeds; keine Generierung während des Spiels.

## 15. Foliage Material

Vorhandene neutrale GLB-Materialdaten liefern BaseColor, Normal, Roughness, Metallic, Occlusion, Emissive, AlphaMask und Cutoff. Dünne Blätter erhalten dezente Foliage-Parameter aus ZVEG. Kein separater komplexer Subsurface-Renderer.

## 16. Legacy Leaves

Originaltexturen und bisherige Leaf-Card-/Alpha-Daten bleiben erhalten. Keine künstliche PBR-Texturkonvertierung. Modern ergänzt lediglich dezentes Backlighting; Classic bleibt unverändert.

## 17. Two-Sided Lighting

Blätter/Gras/Busch verwenden den vorhandenen Two-Sided-Normalpfad; Branches behalten Cull-Winding. Gespiegelte Transform-Instanzen bilden eigene Winding-Batches. Der reale GPU-Test enthält positive und negative X-Skalierung.

## 18. Leaf Transmission

Günstiger direkter Gegenlichtterm, multipliziert mit BaseColor und TransmissionColor, Standardstärke 0,16. Kein Emissionstrick. GPU-Captures mit gleicher Sonne und Transmission an/aus zeigen eine messbare Bildänderung.

## 19. Sun Integration

Foliage liest dieselben Modern-Frame-SunDirection/SunColor-Konstanten aus `SceneLighting.sun` wie die Welt. Keine Vegetationssonne, kein gesonderter Lichtzyklus.

## 20. Leaf Shadows

Shadow- und Color-Pass teilen Vertex-Wind, Material-AlphaCutoff und LOD-Dither. Keine pauschal rechteckigen Deckschatten für Leaf-Cards.

## 21. Bark

Normales Material im bestehenden Modern-PBR-Pfad. Das Demo besitzt raue originale Rindentextur und Roughness. Legacy-Bark behält seine Texturen und Materialabbildung.

## 22. Wind Architecture

GPU-Vertexdeformation mit absoluter Zeit, gemeinsamer Weltrichtung/-stärke und assetlokalen Branch-/Leaf-Antworten. Keine CPU-Vertexdeformation; CPU erzeugt nur Instanz-/Sichtdaten.

## 23. Global Wind

Die bestehende Environment-Windstärke wird über `SetNativeVegetationWind` geteilt. Gemeinsame horizontale Standardrichtung; RenderContext unterstützt eine Weltrichtung. Noch kein neuer mapseitiger Richtungseditor oder Environment-Dateiformat.

## 24. Per-Instance Variation

Windphase aus normalisierten Transformpositionen und stabiler Instance-ID. Gleiches Asset und gleiche Position liefern bei gleicher Zeit reproduzierbare Bewegung.

## 25. Branch Bending

Langsames Sinussignal mit quadratischem Höhengewicht. Wurzel bleibt fest, unterer Stamm bewegt sich wenig. Windbounds enthalten die konservative maximale horizontale Auslenkung.

## 26. Leaf Flutter

Kleines schnelleres Signal zusätzlich zur Branch-Bewegung. Qualitätsstufen reduzieren Leaf-Detail. Keine zufällige Änderung pro Frame.

## 27. Time Based Motion

Keine akkumulierten Frameschritte: Shader verwendet absolute Sekunden. Portable Prüfung vergleicht 60-/120-Schritte am identischen Zeitpunkt, Phase und Wurzelverankerung. Native Diagnostik kann ausschließlich für Beweisaufnahmen eine feste Vegetationszeit setzen.

## 28. Wind Shadows

Identischer Vertexshader für Color und Shadow; gemeinsame Zeit und Phase. Far-Impostors lassen Branch-Wind weg. Kein zweites CPU-Windmodell im produktiven Renderer.

## 29. Grass Architecture

Optionales `vegetation/modern/grass.zveg`, dieselbe Runtime/Material-/Instancing-Pipeline. Nach der Nutzerkorrektur wird die komplette Grasverteilung bereits in `CMapOutdoor::Load` vorbereitet und bis zum Kartenwechsel gehalten. Auch Terrain außerhalb der gestreamten 3×3-Nachbarschaft wird einmal mit den vorhandenen CPU-Datenladern ausgewertet, ohne dessen Terrain-Geometrie oder Splat-Texturen zu laden. Fehlendes optionales Gras erzeugt keinen Ersatzrenderer.

## 30. Grass Masks

Reale Metin2-Daten sind kategorische `tile.raw`-Layerindizes, keine frei erfundenen Graustufenmasken. Vier benachbarte Layer werden bilinear als Dichte interpretiert, sofern die zugehörige Texture-Set-Datei eine Grass-Textur benennt. Blockierte, unter Wasser liegende und steile Stellen werden ausgeschlossen.

## 31. Grass Placement

Deterministische Kandidaten aus Mapname, 400-cm-Zelle und Kandidatenindex, 24 Kandidaten pro Zelle. Position, Terrainhöhe, Rotation, Maßstab, Windphase und Dichterang werden beim Kartenladen in kompakten 32-Byte-Datensätzen gespeichert. Der Renderpfad liest ausschließlich vorhandene Zellen; kein Terrain-Sampling, keine Kandidatenerzeugung und kein Verwerfen beim Vorbeilaufen. Keine Änderungen an bestehenden Terrainmasken.

## 32. Grass Density

Low / Medium / High / Ultra wählen verschachtelte Kandidatenmengen mit 0,18 / 0,35 / 0,65 / 1,00 Dichte. Ein höheres Preset würfelt vorhandene Positionen nicht neu.

## 33. Grass Culling

Radien nach der Nutzerkorrektur: 5.000 / 7.500 / 10.000 / 13.000 cm. Zusätzlicher Frustumtest; letzte 40 % blenden durch Dither aus. Ab etwa 35 Metern übernimmt der vorhandene sparsame Impostor. Die ursprüngliche kurze Sichtweite von 12–34 Metern ließ Gras sichtbar in unmittelbarer Nähe auftauchen. Die ganze Karte ist vorbereitet; gezeichnet wird weiterhin nur der relevante sichtbare Ausschnitt.

## 34. Grass Wind

Dieselbe Weltrichtung und Zeit; Root stabil, Spitze stärker gewichtet. Der GPU-Test fordert einen sichtbaren Unterschied bei Zeit 0 und 1,3 Sekunden.

## 35. Grass Lighting

Gemeinsame Sonne, Schattenempfang und dezente qualitätsabhängige Transmission. Kein eigenes Graslicht. Originale Grasfarbe kommt aus dem Demo-Material.

## 36. Bushes

Originaler 2,1-m-Demo-Busch mit drei Geometrie-LODs und Impostor. GPU-Galerie prüft Wind, Dichte, Culling und geteilte Ressourcen. Keine willkürliche Ersetzung historischer Bäume durch Büsche in Maps.

## 37. Flowers

Optionaler Blumencontent nicht hinzugefügt. Der PlantKind-/Assetpfad benötigt keine separate Engine für spätere kleine Pflanzen.

## 38. Test Assets

Eine Buche, eine Grasart und ein Busch; vollständig originale prozedurale Geometrie/Texturen, keine Internet- oder Unity-Assets. Quellen: `tools/Vegetation/ModernAssets.cpp`; Runtime-Ausgaben und Herkunft: `content/vegetation/`. Kein Austausch der 118 Legacy-Typen.

## 39. GLB Pipeline

Das Offline-Tool verwendet vorhandene AssetTool-Szenen, GLB-Writer und anschließend den produktiven GlTFProvider zur Validierung. Renderer-private Diligent-Typen bleiben außerhalb neutraler Metadaten.

## 40. meshoptimizer

Vorhandener Offline-Einsatz für Vertex-/Indexcache und Fetch-Optimierung. Keine neue Runtime-Importer- oder meshoptimizer-Abhängigkeit.

## 41. LOD Generation

Kontrolliert authored drei Dichten/Geometriekomplexitäten. Alpha-Leaf-Cards werden nicht blind durch generische Dreiecksreduktion zerstört. Die Far-Stufe wird aus High-LOD rasterisiert.

## 42. Bounds

ZVEG prüft Geometrieeinschluss und konservativen Windrand. DrawBatch transformiert alle acht Bounds-Ecken. Auch die vorgelagerte native Culling-Sphere umfasst in Modern die vereinigten Legacy-/Override-Windbounds; beim Stilwechsel wird sie aktualisiert. Legacy-Kollisionen und Positionen bleiben erhalten.

## 43. Graphics Settings

Bestehendes `vegetation`-Qualitätsfeld und `vegetationDistanceScale`, keine neue UI. Native Runtime liest aktuelle GraphicsRuntimeConfig beim Rendern.

## 44. Presets

| Preset | Tree distance | Grass radius cm | Density | Leaf wind detail | Transmission multiplier |
| --- | ---: | ---: | ---: | ---: | ---: |
| Low | 0,65 | 5.000 | 0,18 | 0,35 | 0 |
| Medium | 0,85 | 7.500 | 0,35 | 0,65 | 0,5 |
| High | 1,00 | 10.000 | 0,65 | 1,00 | 1,0 |
| Ultra | 1,25 | 13.000 | 1,00 | 1,00 | 1,0 |

## 45. Live Apply

Private native Serie wechselt vier Gras-Presets ohne Mapneuladung und Classic → Modern auf derselben A1-Map. Sichtmengen und Buffer werden bei Bedarf aktualisiert; Classic leert Modern-Instanzbuffer.

## 46. A1

Feste Übersicht sowie tatsächliche unveränderte `b1_beech_rt4`-Platzierung bei x=12328, y=7030 mit Nah-/Mittel-/Fernkamera. Evidenz: `runtime-native-final/views.json` und Galerie.

## 47. B1

Originale B1-Kamera (68900,54000), vier Grasqualitäten und Performancevergleich. Keine Mapbearbeitung.

## 48. Dense Map

Trent-Wald bei (15000,15000) sowie kontrollierte Instancing-Szenen mit 600 Bäumen/Büschen beziehungsweise 1.000 Grasinstanzen. Kontrollierte Szenen sind kein Nachweis einer produktiven Massenbepflanzung.

## 49. Camera Movement

Native langsame und schnelle Kamerafahrt (35 bzw. 1.200 cm/s), feste Aufnahmen nach sechs Sekunden. GPU-Test prüft Szene hinter der Kamera. Endgültige Bewertung von LOD-Auffälligkeit bleibt die Sichtabnahme.

## 50. Sun Tests

Native Morgen/Mittag/Abend-Einstellungen und kontrolliertes Gegenlicht-/Mittagspaar. Gleiche SceneLighting-Sonne; Transmission-on/off zusätzlich objektiv verglichen.

## 51. Water Integration

Native Uferansicht bei (71500,55000). Gras schließt vorhandene Wassermasken und überdeckte Terrainhöhen aus. Water-/SSR-Implementierung unverändert; deren GPU-Gate bleibt Teil des Fast-Gates.

## 52. HDR Integration

Foliage schreibt in den vorhandenen HDR-/Deferred-/Forward-Pfad. Tone Mapping, Bloom und Atmosphäre werden weiter von G8 gesteuert. Keine separaten vegetationseigenen Tonemapper.

## 53. Color Preservation

Legacytexturen unverändert, demonstrative Blattfarben kontrolliert grün und Transmission albedobasiert. Galerie enthält G8/H2 bei gleicher B1-Kamera. Keine globale Entsättigung oder Terrain-Texturänderung.

## 54. No Fog

Kein allgemeiner World Fog ergänzt. Distanzen/Impostors werden nicht durch grauen Nebel verdeckt. Bestehender Classic-Leaf-Fog-Code bleibt unangetastet.

## 55. Classic

H2 wird nur für Modern aktiviert. Classic nutzt V1-Assets und ursprüngliche Draws. Harte Byteprüfung erfolgt durch `Renderer.ClassicGoldens`, nicht durch JPG-Sichtvergleich.

## 56. Modern Fallback

Nicht vorhandene oder ungültige optionale ZVEG-/GLB-Overrides fallen auf den konvertierten Legacy-Eintrag zurück. Fehler werden gecacht. Optionale Material-/Texturvorbereitung lässt ebenfalls den Legacy-Baum bestehen. Kein SDK-Fallback.

## 57. Asset Sharing

Runtimecache nach kompiliertem Pfad, Renderressourcen pro Asset. 600 Instanzen erzeugen keine 600 Geometrieuploads. GPU-Test prüft unveränderten Geometriezähler und begrenzte Batchzahl; identische Wiederholung hat null Instanzuploads.

## 58. Resource Lifetime

Getrennte Color-/Shadowbuffer, reference-counted Render-/Textur-/Materialdaten, beim Laden vorbereitete maplokale Grasdaten. GPU-Test fordert alle Asset-/Instance-/Geometry-/Texture-/Bufferzähler nach Freigabe null. Native Shutdown-Audit prüft dieselben Klassen im echten Client; die Nachprüfung verlangt zusätzlich null gespeicherte Grasplatzierungen nach Map-Destroy.

## 59. Map Change

Native Serie A1 → B1 → Trent → A1; weitere Qualitäts-, Sonnen- und Kameraansichten danach. Keine manuellen Zwischenabnahmen.

## 60. Relog

**NOT RUN: Netzwerk-Relog.** Der bestehende native Offline-Harness lädt echte Maps und Original-Actors, führt aber keinen authentifizierten Login-/Character-Select-/Relog-Zyklus aus. Map-Lifecycle-Evidenz wird nicht als Netzwerk-Relog ausgegeben.

## 61. Resize

High und Ultra rufen die bestehende native Window-Probe für Resize, Minimize und Restore auf. Ergebnis und Rückgabewerte stehen in `runtime-native-final/g8-visual.log`.

## 62. Diligent Diagnostics

**PASS: Diligent ERROR=0 / FATAL=0** im finalen Debug-Gate und in allen vier nativen Serien. CPU-Deformation=0, GPU-Fallback=0; Assets, Instances, RenderAssets, Geometry, InstanceBuffers, InstanceBytes, SourceTextures, SourceBuffers, Skeletons und Clips nach Shutdown jeweils 0. Native Syserr-Dateien sind leer. `evidence/final-result.json` verknüpft diese Nachweise mit den tatsächlich gestarteten Binärhashes.

## 63. G8 Regression

Vollständige Fast-Liste enthält HDR/Atmosphäre, Material, DiligentFX, Shadows/SSAO, Water/SSR, Static/Skinned GLB/GR2, Effects und Classic. Abschließender Zero-Audit: **PASS**, 1.505 Produktionsdateien, 43 transitive Linkprojekte, vier PE-Binaries; Produktionshits, SDK-Artefakte und Linkhits jeweils 0, keine verbotenen SDK-/D3D8/9-Imports oder SDK-Marker. `evidence/zero-audit.json` enthält die vollständige Prüfung.

## 64. Performance Baseline

**PASS als begrenzte Vegetations-Sanity.** Frischer G8-Lauf gegen H2 mit Legacycontent und H2 mit Demonstrator/Gras; gleiche B1-Kamera, vier Presets, je 180 warme Frames, keine Ausreißer entfernt. Quelle: [vollständiger Vergleich](../../build-h2x/performance/comparison.md), Rohdaten und JSON daneben.

| High, 1024×768 | CPU Ø / P95 ms | GPU Frame Ø / P95 ms | Dedicated GPU memory median MiB |
| --- | --- | --- | ---: |
| G8 committed | 1,623 / 1,737 | 0,757 / 1,285 | 252,30 |
| H2 Legacycontent | 1,618 / 1,945 | 0,747 / 1,521 | 252,70 |
| H2 Modern-Assets + Gras | 1,608 / 1,937 | 0,722 / 1,205 | 255,86 |

Ultra CPU-Median steigt von 1,600 auf 1,946 ms; GPU-Median 0,814 auf 0,795 ms. Das ist kein nachgewiesener FPS-Gewinn. Low enthält einzelne CPU-Ausreißer: G8 maximal 12,895 ms, H2 Legacy 60,106 ms, H2 Modern 32,345 ms. Bei den beiden großen H2-Ausreißern beträgt die gemessene World-Phase nur 1,177 bzw. 1,281 ms; die Ursache außerhalb dieser Phase ist hier nicht isoliert. Alle Werte bleiben in der Statistik. Kein allgemeines Hitch-/60-/120-FPS-GO.

Die drei Läufe waren sequenziell, ohne parallelen Build/GPU-Test. Ein bereits vorhandener fremder Metin2-Prozess blieb unverändert aktiv; deshalb keine vollständig isolierte Benchmarkbehauptung. Hardwareinventar: Ryzen 7 9800X3D, RTX 5070 Ti und integrierte Radeon; das Inventar allein belegt keine Diligent-Adapterwahl.

## 65. Instancing Performance

**PASS:** 600/600 sichtbare Bäume → vier Draws, 1.137.600 Dreiecke, vier initiale Uploads; 1.000/1.000 Grasinstanzen → zwei Draws, 32.000 Dreiecke, zwei initiale Uploads. 600 Büsche → vier Draws. Alle Szenen teilen Geometrie/Material/Textur; Wiederholung derselben Einzelinstanz: null neue Instanzuploads. Gespiegelte Transformationen sind enthalten. Raw CSV: `msvc/h2-evidence/Release/performance.csv`.

## 66. Culling Performance

**PASS:** Hinter die Kamera gerichtete Szene verwirft 600 Bäume, 1.000 Grasinstanzen bzw. 600 Büsche vollständig, jeweils null Draws/Dreiecke. CPU-Kosten dieser Sichtprüfung: 0,054 / 0,087 / 0,060 ms. Keine neue GPU-driven-Architektur erforderlich; gemessen wird CPU-Gruppierung/Submission, kein isolierter Compute-Pass.

## 67. GPU Performance

Diligent-Duration-Query um die reale Vegetation-Color-Submission: 600 Bäume **0,380 ms**, 1.000 Grasinstanzen **0,065 ms**, 600 Büsche **0,270 ms**. Die zugehörigen CPU-Zeiten sind 0,845 / 0,433 / 0,274 ms. Diese Messungen schließen Shadow, Composite, Present und Readback aus. Native Frame-GPU-Zeiten stehen separat im Vergleich. Kaltkompilierung ist in den Rohdaten sichtbar; keine isolierten Shadow-GPU-Zeiten behauptet.

## 68. Memory

GPU-Instanzdaten: 80 Byte; dichte Baum-/Buschprobe reserviert 96.000 Byte, Grasprobe 80.000 Byte. Originaler Demo-Content nach der Gras-Korrektur: zwölf Dateien, 4.896.301 Byte auf Disk, keine Aussage über identischen VRAM-Verbrauch. Kartengrasspeicher: 32 Byte pro vorbereiteter Platzierung zuzüglich Zellindex/Container; B1 578.476 Platzierungen / 17,65 MiB Nutzdaten, A1 625.189 / 19,08 MiB. Es werden keine individuellen Runtime-Objekte pro gespeichertem Büschel gehalten.

**VRAM gemessen:** Windows `GPU Process Memory` / `DedicatedUsage`, `SharedUsage`, `TotalCommitted`, den vom Testlauncher protokollierten PIDs zugeordnet und über Adapter summiert. Drei Samples pro Preset im Fenster 2–4,5 Sekunden nach erster Beobachtung, vor Screenshot/Shutdown. High Dedicated median/peak: G8 252,30 MiB, H2 Legacy 252,70 MiB, H2 Modern 255,86 MiB; Ultra 300,70 / 301,47 / 304,96 MiB. Das sind Prozesszähler für den ganzen Client, keine isolierte Residency einzelner Pflanzen und keine physische Grafikkartenkapazität. Rohdaten: `evidence/vram-samples.csv`.

## 69. Content Priority

Frisch aus effektiven Packdaten, keine geschätzten Markt-/Nutzungszahlen. Priorisiert nach A1/B1/Trent-Platzierungen, dann global. Top (Platzierungen gesamt / davon auf A1+B1+Trent): `b1_beech_rt4` 567/79, `b3_beech_rt3` 505/78, `b1_pagodatree_rt` 668/77, `b1_beech_rt2` 522/72, `b1_pagodatree_rt2` 641/71. Vollständige Liste unter `build-h2x/content/content-priority.md` und `.json`.

## 70. Grass Priority

Reale Texture-Set-/Tile-Auswertung: Grass01 108 Tiledateien/1.089.100 Samples, Grass02 98/634.125. Davon 582.843 beziehungsweise 344.156 in A1/B1/Trent. Tile-Ränder sind in den Rohdaten enthalten; Samples sind keine Quadratmeter oder Grasinstanzzahl. Ein Demo-Gras beweist die Technik; spätere Artvarianten nach diesen Daten priorisieren.

## 71. Release

**PASS 72/72** nach der Gras-Korrektur, 70,95 Sekunden; vollständiger Build erfolgreich. JUnit: `grass-preload/Release.xml`, Log: `grass-preload/Release-tests.log`. Release-SHA256: `959d03d5ff6e560a2043e300688811852ff56784d14236aea541a0ab003964cd`.

Der erste Lauf bestand 71/72; nur die alte Erwartung von 18 vorbereiteten Shaderpaaren war überholt. Jetzt werden gezielt 24 Varianten vorbereitet (18 bestehende plus sechs Vegetationsvarianten) und geprüft. Danach wurden die korrigierten nativen Bounds und finalen Demonstrationsassets vollständig neu validiert.

## 72. Debug

**PASS 72/72** nach der Gras-Korrektur, 166,57 Sekunden; vollständiger Build erfolgreich. JUnit: `grass-preload/Debug.xml`, Log: `grass-preload/Debug-tests.log`. Debug-SHA256: `7e969342228a6428bfa2cdb65299d3429a813c1ee565b3c4dd34ed28731e0cff`. Installierte Visual-Studio-x64-Entwicklungsumgebung; bekannte Vendor-PDB/LTCG/Library-Warnungen bleiben, kein warning-free Claim.

## 73. GCC/LP64

**PASS 43/43** nach der Gras-Korrektur, Cygwin GCC 12.4 / LP64, 5,66 Sekunden; natives Windows-Rendererziel ausgeschaltet. Metadaten, LOD, Wind, kompakte Platzierung, Qualität, Validierung und GLB-Assets werden headless geprüft. Log: `grass-preload/portable-tests.log`, JUnit `grass-preload/portable.xml`. Vorläufe scheiterten an gemischten Dateirechten alter generierter Testausgaben; diese wurden mit Hashbeleg gesichert, anschließend lief das komplette Gate mit frischen Ausgaben erfolgreich. Keine Testanforderung abgeschwächt.

## 74. Classic Goldens

**PASS: 12/12 bytegleich in Release und Debug**, bestätigt durch den jeweiligen `Renderer.ClassicGoldens`-JUnit-Output. Keine Golden-Referenz geändert. Native JPGs sind zusätzliche Sichtbelege und ersetzen diese Byteprüfung nicht.

## 75. Visual Gallery

[Aktuelle Gras-Galerie](../../build-h2x/grass-preload/gallery.html) nach der Nutzerkorrektur. Die ursprüngliche [H2-Galerie](../../build-h2x/gallery/index.html) mit **74 festen Bildern** bleibt als Ausgangsbeleg erhalten: 22 native H2-Ansichten, 40 kontrollierte reale GPU-Aufnahmen und zwölf G8/H2-Vergleichsaufnahmen. Enthalten: Legacy-Fallback, Tree High/Medium/Low/Impostor, Übergangsschritte, Wind, Gegenlicht an/aus, Mittag, Gras, Busch, A1/B1/Trent, vier Presets und Ufer.

Die native A1-Nahaufnahme zeigt auch die bestehende Camera-Blocker-Transparenz. Die frei stehenden GPU-Aufnahmen zeigen das unverdeckte Asset. Die extreme native Fernkamera und die schnelle Fahrt sind Lifecycle-/Culling-Sanity; sie ersetzen keine umfassende künstlerische Abnahme jeder Map.

## 76. User Approval

**Nachtrag 16.09.2026:** Auf ausdrücklichen Nutzerauftrag ist derselbe geprüfte Release-Stand einschließlich Vegetationspaket und Gras-Ladekorrektur jetzt auch in den normalen Spielordner übernommen. [Dokumentation der Übernahme](../../../m2dev-client/docs/h2-vegetation-deployment.md). Gezielter nativer Pakettest: drei Ansichten PASS; keine erneute Vollsuite. Die folgende Startnotiz beschreibt den zuvor geöffneten privaten Testclient.

**Aktualisierter normaler Testclient geöffnet**, Modern / High, ohne Diagnose-Argumente, originales `prototype.py` unverändert. Prozess 3508, sichtbares Hauptfenster `METIN2`, Release-Hash entspricht dem nach der Gras-Korrektur getesteten Artefakt. Startbeleg: `grass-preload/manual-launch.json`.

**Nutzerabnahme ausstehend:** Bäume, Gras, natürlicher Wind, Sonnenreaktion, LOD/Impostors, Metin2-Look und Gesamtbild. Dies ist die einzige angefragte Sichtabnahme, wie in Auftragspunkt 84 gefordert. Automatische Prüfungen ersetzen dieses GO nicht.

## 77. Git Diff

Alle Änderungen uncommitted; kein Push. Kein Terrain-/Environment-Contentpass, UI, World Editor, Android oder allgemeines Performance-Finish. Aktuelle Abschlussartefakte einschließlich Gras-Korrektur: `grass-preload/git-diff.patch`, `grass-preload/git-status.txt`, `grass-preload/result.json` und Content-Manifest. Generierte Builds und Beweisbilder bleiben ignoriert; originales Demo-Content wird mitgeliefert.

## 78. Known Limitations

- Netzwerk-Relog nicht automatisiert.
- Kleine originale Demonstrationsassets, keine vollständige Artbibliothek; Bush nur kontrolliert gezeigt, kein beliebiger produktiver Registry-Ersatz.
- Atlas mit acht Azimuten, keine volumetrische oder mehrschichtige Impostor-Middleware. DDS-Demos verwenden zunächst nur die Basis-Mipstufe.
- Grass-Maske wird aus bestehenden Grass-Layern abgeleitet; besondere Biome brauchen später passende Art/Zuordnung. Kein Map-Editor in H2.
- CPU-Sichtgruppierung pro Frame; keine GPU-driven-Architektur. Bounded Sanity statt P-X-Profiling.
- Finale Wind-/LOD-/Look-Bewertung ausstehend. Die B1-Low-Aufnahme zeigt bereits in G8 dieselbe dunkle Legacy-Fernsilhouette; dieser unersetzte Typ erhält durch die drei Demo-Assets keine neue Art.
- Einzelne Frame-Ausreißer bleiben im Performancevergleich; kein allgemeines FPS-/Hitch-GO. Prozess-VRAM ist gemessen, individuelle Asset-Residency nicht.

## 79. GO/NO-GO

**Automatische Technik-/Kompatibilitäts-Gates: PASS. Finales H2-GO: NO-GO bis zur ausdrücklich geforderten Nutzer-Sichtabnahme.** 118/118 konvertierte und 85/85 produktive Typen bleiben unterstützt; neue Technik, Fallback, geteilte Ressourcen, vollständige Gates, Diagnostik, native Lifecycle-Serie und begrenzte Performance-/VRAM-Messung sind belegt. Netzwerk-Relog bleibt separat NOT RUN. Keine Folgephase gestartet.

## 80. Recommendation next phase

Nach technischem und visuellem GO: einzelne besonders häufige Vegetationstypen und zwei bis fünf Grasvarianten gezielt künstlerisch modernisieren. Diese Folgearbeit ist nicht gestartet. Nach H2 STOP; kein Commit/Push und keine nächste Phase ohne neuen Auftrag.
