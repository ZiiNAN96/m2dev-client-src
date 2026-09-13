# Milestone 4B – Analyse vor Produktionsänderungen

> Historischer Milestone-Stand. Aktuell seit [M12](renderer-milestone12.md): Diligent D3D11 ist im ON-Build der Default; Legacy wird explizit mit `--renderer=legacy-d3d9` gewählt. OFF-Builds bleiben Legacy-only. Frühere Testergebnisse und Auswahlbeschreibungen unten gelten für ihren damaligen Stand.

Stand: 12.09.2026, Source-Baseline `95eda15`. Zunächst ausschließlich lesende Analyse und eigene Hilfen unter `build/milestone4b`; keine Assets geändert. Zwischenzeitlich fremde Resource-Editor-Dateien `src/UserInterface/UserInterface.aps`/`RCa39752` beobachtet; nicht anfassen.

## 1. Belegter Bestand und 4A-Abdeckung

`inventory.ps1` liest die vorhandenen Property-/AreaData-Dateien. `inspect-static.cpp` verwendet ausschließlich die vorhandene Granny-Bibliothek (`GrannyReadEntireFile`, `GrannyGetFileInfo`, Mesh-/Material-APIs), keinen eigenen GR2-Parser. Die Zuordnung loser Assets folgt der Pack-Reihenfolge aus `UserInterface.cpp`; spätere Runtime-Tests müssen die tatsächlich geladenen Packs zusätzlich prüfen. Die Zahlen sind keine behauptete Vollinventur beliebiger Spielkarten.

| Map | Building-Einträge | SpeedTree-Einträge | Effect-Einträge |
|---|---:|---:|---:|
| A1 | 601 | 368 | 6 |
| B1 | 891 | 317 | 5 |
| C1 | 565 | 296 | 10 |

208 unterschiedliche referenzierte Building-Dateinamen, davon 207 in den losen Assets auflösbar: 207 Granny-Modelle, 228 Meshes, alle starr, Position/Normal/UV0 (PNT32), **0 Animationen**. 13 Dateien besitzen mehrere Meshes. Alle Modelle besitzen Skeleton/Bones: Meshzuordnungen verteilen sich auf Skeletons mit 1/2/3/4 Bones (194/14/12/8 Meshes). Das bedeutet hier Bind-/Meshtransformationen, nicht Animation; die bereits vorhandenen finalen `m_meshMatrices` bleiben maßgeblich. Zusätzlich 31 vorhandene, vom Area-Loader verwendbare LOD-Dateien: 67 Meshes, ebenfalls starr, PNT32, 0 Animationen und keine Opacity-Materialien.

Eine Bestandsreferenz in B1, CRC 3481091450, dreimal Tile 002002, zeigt auf `d:/ymir work/zone/공용/hay_02.gr2`. Unter diesem Namen fehlt die lose Datei; ein anders benannter Pfad ist kein autorisierter Ersatz. Nicht als Diligent-Meshfehler ausgeben; Packs/Runtime gesondert prüfen.

| Objekttyp / Variante | 4A | 4B nötig / Beleg |
|---|---|---|
| Starres einfaches PNT-Gebäude/Prop | Ja, schattenfreier Grundpfad | Bestehende Daten/Transforms behalten |
| Großes/kleines Gebäude, Mauer, Felsen, Dekoration | Dieselbe Building-Klasse | Keine neue Objektklasse; konkrete Materiallücken schließen |
| Multi-Mesh | Grundsätzlich ja | 13 echte Dateien, z.B. `b1-middledam-07`, `b1-008-house5`; alle vorhandenen Meshmatrizen/Offsets prüfen |
| Multi-Material | Ja, sofern ganze Palette gültig | Fehler: unbenutzte leere Slots führen zur Ablehnung des ganzen Gebäudes |
| Leere unbenutzte Materialslots | Nein | 198 leere Slots ohne Dreiecksgruppen; nur tatsächlich referenzierte Materialien prüfen |
| A1R5G5B5-Diffuse | Nein | `general_obj_drum.dds`, fünf originale Mips; Fahne und Trommel verwenden sie |
| Two-Sided | Bereits pro Material | `obj-0001.gr2`, `obj-0002.gr2`, ExtendedData `Two-sided=1`; Rückseiten vergleichen |
| Alpha-Test-Grundobjekt | Kein aktiver Fall in den untersuchten Grundpfaden belegt | Kein Banner/Zaun ohne Beleg zum Cutout umdeuten; echte State-/Ref-Weitergabe separat testen |
| Opacity-/TYPE_BLEND_PNT-Grundmaterial | In 4A ausgeschlossen | 0 in den 207 Basis-/31 LOD-Dateien; kein frei erfundener Opacity- oder Blendmodus |
| Kamera-Verdeckungsobjekt / PCBlocker | Nicht übergeben | Reale statische Gebäude benutzen hier SRCALPHA/INVSRCALPHA und die vorhandene `PCBlockerAlpha.dds` |
| Statisches Objekt bei Schatten an | Unvollständig/fehlt | Schattenreceiver wird separat gezeichnet und aus Opaque-Liste verborgen; außerdem bleiben zusätzliche States zurück |
| SpeedTree | Nein | Eigene `PROPERTY_TYPE_TREE`-/Forest-Pipeline, ausdrücklich ausgeschlossen |
| Animiertes Gebäude, Character/NPC/Mob, Effect | Nein | Andere Pfade, ausdrücklich ausgeschlossen |
| DungeonBlock/PNT2 | Nein | Separater `CDungeonBlock`-Zweitexturpfad; keine solchen AreaData-Einträge in diesen drei Maps |

Konkrete 4A-Nachweise: A1-Gasthaus, Steinlaterne, Zäune und Props wurden zuvor visuell verglichen; im letzten C1-Ingame-Lauf 333 erste erfolgreiche Objektmeldungen, 7 Material- und 8 Format-Ausschlüsse. Beispielsweise Bank, Stonedoor, Bellhouse, 6tower: unbenutzte leere Materialslots im Granny-Bestand erklären die pauschale Material-Ablehnung. Kein verwendeter Slot in der untersuchten Grundmenge hat eine fehlende Diffuse-Referenz. Die Liste ist keine Behauptung, jedes einzelne Objekt bereits neu visuell abgenommen zu haben.

## 2. Legacy-Call-Chains und Daten

Laden: `CMapOutdoor::LoadArea` → `CArea::Load`/AreaData/Property-CRC → `CArea::__SetObjectInstance_SetBuilding` → `CResourceManager` → `CGraphicThing::OnLoad` → originale Granny Runtime → `CGrannyModel::LoadMeshs`/Materialpalette/VB/IB → vorhandener mapgebundener CPU-Snapshot vor `GrannyFreeFileSection` → Thing/LOD/ModelInstance.

Opaque: `CPythonApplication::RenderGame` → Background/Map → `CMapOutdoor::OnRender` → `RenderArea` → `CollectRenderingObject` → originale Distanzsortierung aufsteigend → Thing `Render`/`OnRender` → `CGrannyLODController::RenderWithOneTexture` → `CGrannyModelInstance::RenderMeshNodeListWithOneTexture` → `CGrannyMaterial::ApplyRenderState` → `CStateManager::DrawIndexedPrimitive` → D3D9.

Blend: `CollectBlendRenderingObject` filtert `HaveBlendThing` für das **ganze Objekt** → `RenderBlendArea` → gleiche aufsteigende Distanzsortierung → `BlendRenderWithOneTexture` zeichnet nur TYPE_BLEND_PNT-Gruppen. Keine neue Sortierung oder automatische Ergänzung von in diesem Legacy-Pfad ausgelassenen Gruppen.

Schattenreceiver: `RenderArea` → `FAreaRenderShadow` → `Thing::RenderShadow` → LOD `RenderWithOneTexture`, dann `Hide`, später `Show`. Ohne zusätzlichen mapgebundenen Übergabepunkt fehlen diese Objekte in Diligent.

PCBlocker: `RenderArea` versteckt die vorhandene PCBlocker-Liste. Später `RenderGame` → `BeginEnvironment` → `RenderPCBlocker` → `FRenderPCBlocker` → `Show` → starrer OneTexture-Draw mit bestehender Kamera-Alpha-Maske. Diese Transparenz ist kein Character-/UI-Renderer.

Gruppen: `CGrannyMesh::LoadTriGroupNodeList`: `idxPos = m_idxBasePos + TriFirst*3`, `triCount`, Palette `mtrlIndex`; verkettete Listen werden vorangestellt. Mesh `baseVertex` und Gruppe `firstIndex` getrennt erhalten. Materialpalette der aktuellen ModelInstance, nicht beliebige Model-Grundpalette. World bleibt die finale `GetStaticObjectWorldMatrix(iMesh)` aus 4A; View/Projection aus aktuellem StateManager. Kein Skinning, kein eigenes Bonesystem.

## 3. Tatsächlicher Legacy-Materialvertrag

| Aspekt | Konkreter Originalpfad / States |
|---|---|
| Erzeugung/Auswahl | `CGrannyMaterialPalette::RegisterMaterial`, `CreateFromGrannyMaterialPointer`; Gruppe verweist auf Palettenindex. Nicht verwendete Einträge sind keine Draws. |
| Texture Lookup | `GrannyGetMaterialTextureByType(DiffuseColor/Opacity)`, Blend-namige Mehrfachmaps separat; `__GetImagePointer`/ResourceManager einschließlich bestehendem relativen Modellpfad. |
| Bindings | `__ApplyDiffuseRenderState` setzt **nur Stage 0** und optional Cull. Opacity-Referenz klassifiziert TYPE_BLEND_PNT; sie wird im OneTexture-Draw nicht automatisch als zweite Textur gebunden. |
| Materialfarbe | Globale `D3DMATERIAL9` aus `CMapManager::BeginEnvironment`; keine Anwendung einer neu erfundenen Granny-Diffusefarbe. MATERIAL-Quellen für diffuse/ambient/emissive; `COLORVERTEX=FALSE`. |
| Vertex Color | PNT32 hat keine Farbkomponente; im untersuchten Bestand kein entsprechender Feldtyp. Nicht neu hinzufügen. |
| Grundfarbe | `SetBlendOperation`: TEXTURE*CURRENT. Anschließendes `CArea::RenderDungeon` setzt selbst bei leerer Dungeonliste Stage0 `SELECTARG1(TEXTURE)` und deaktiviert Stage1. 4A-Fix (zusätzliches Intro-Licht bei TextureOnly ignorieren, Alpha erhalten) bewahren. |
| Beleuchtung | Bei MODULATE vorhandene Material-/Lichtwerte, kein PBR. Umgebung ersetzt Licht0; Intro `SetOmniLight` lässt Punktlicht1 aktiv. Für den tatsächlich beleuchteten PCBlocker-Pfad relevant, nicht einfach global abschalten. |
| Alpha Test | Default `ALPHATESTENABLE=FALSE`, `ALPHAREF=1`, `ALPHAFUNC=GREATEREQUAL`. Terrain setzt zeitweise GREATER/0 und stellt zurück. Kein Cutout-Threshold ohne ursprünglichen State. |
| Alpha Blend | `RenderBlendArea`: TRUE, SRCALPHA/INVSRCALPHA, ADD, **ZWRITE=TRUE**; kein vermeintlich besseres ZWRITE-FALSE. PCBlocker nutzt dieselben geerbten Blendfaktoren und bestehende Reihenfolge. |
| Alpha Operation | Grundpfad TEXTURE*CURRENT/Diffuse; Blend/PCBlocker Stage0 SELECTARG1(Texture); PCBlocker Stage1 Alpha SELECTARG1(Texture), RGB CURRENT. |
| Cull | Default CW; `Two-sided=1` temporär NONE pro Material; danach Originalzustand zurück. Meshname `2x` ist kein ausgewerteter Cull-Schalter im OneTexture-Draw. CCW ebenfalls schon im 4A-Drawschema. |
| Depth | ZENABLE TRUE, LESSEQUAL; RenderArea erzwingt Writes; BlendArea ebenfalls TRUE; PCBlocker aktuellen Zustand übernehmen. |
| Address/Filter/Mips | Stage0 WRAP/CLAMP, POINT/LINEAR/ANISOTROPIC Min/Mag, Mip NONE/POINT/LINEAR; MAXANISOTROPY native Geräteeinstellung. Native Defaults für MipBias/MaxMip lesen, Cache nicht umbauen. Originale DDS-Mips erhalten. |
| Texture Transform | Grund-UV0 ohne Transform. PCBlocker Stage1 TCI_CAMERASPACEPOSITION, COUNT2, `m_matBuildingTransparent`, CLAMP/CLAMP; vorhandene BC2-Maske `d:/ymir work/special/PCBlockerAlpha.dds`. |
| Schatten-Stage | Legacy bindet dynamische Schattentextur an Stage1, MODULATE, CameraSpacePosition/COUNT2/Border. Kein neues Schattensystem in 4B: Diligent zeichnet den erfassten schattenfreien Grundmaterialpfad unabhängig von diesem Schalter; **keine Übertragung/Neuerzeugung der D3D9-Schattentextur**. Schattenparität selbst ist kein Erfolgskriterium dieser Erweiterung. |
| Fog | Bestehendes vertex EXP/EXP2/LINEAR, RANGEFOGENABLE, FogColor/Start/End/Density; keine atmosphärische Neuentwicklung. |
| Additiv/Specular | Material-Specular hat eigenen SphereMap-Stagepfad, kein aktiver Bedarf in dieser statischen Inventur belegt. Kein allgemeines Additiv-/Materialsystem hinzufügen. |

Texturinventur verwendeter Basis-Materialien: 128 BC1-Dateien, 1 BC2-Datei, 1 A1R5G5B5-Datei. B5G5R5A1 kann im vorhandenen Diligent-D3D11-Backend nativ hochgeladen werden; keine verlustbehaftete neue Materialkonvertierung erforderlich. Support tatsächlich auf dem Gerät testen.

## 4. Geplante minimale Änderungspunkte

1. `src/GameLib/StaticObjectBridge.{h,cpp}`: nur referenzierte Gruppen validieren; schattenfreien Grundzustand mapgebunden erfassen; ShadowReceiver und PCBlocker getrennt und ohne doppelte Einreichung bedienen. Native Legacy-States ausschließlich lesen. Diagnose und Freigabe bei Area-Lifetime behalten.
2. `src/GameLib/MapOutdoorRender.cpp`: wenige zusätzliche Übergaben in den **bestehenden** Objekt-/ShadowReceiver-/PCBlocker-Listen, Grundzustand vor Shadow-Setup erfassen. Keine neue Sichtbarkeits-/Sortierarchitektur und kein zusätzlicher unabhängiger Szenengraph.
3. `src/EterLib/StaticObjectTextureLoader.{h,cpp}` (neu), `src/Renderer/TerrainTextureData.h`: statische Texturen über vorhandenen DDS-Parser/Decoder; zusätzlich genau das belegte B5G5R5A1-Format mit allen Mips. Terrain-Verhalten unverändert.
4. `src/Renderer/StaticObjectRenderData.h`, `DiligentStaticObjectRenderer.cpp`: begrenzte Drawparameter für originale Alpha-States, SRCALPHA/INVSRCALPHA, DepthWrite, PCBlocker-Maske/Transform/Sampler und notwendige Legacy-Lichtwerte; vorhandene PNT-/Transformpipeline behalten. Kein allgemeines Materialsystem.
5. `tests/Renderer`: native D3D9-GPU-Parität für diese Ergänzungen, ursprünglicher 4A-Regressionstest, CPU-Mip-/Formatgrenzen, echte Maps/Objekte; separate isolierte Testhilfe mit A1 → B1 → A1, Schatten 0/3 und vergleichbaren Kameraposen. Keine Authentifizierungsautomation.
6. Dokumentation/Nachweise und ON-/OFF-Release-Builds, vollständige Suite, Legacy-Kontrolle, Bilder von mindestens drei Objekten, mehrfach Resize/Minimize/Restore, Exitcodes und Resource-Lifetime. Normaler Client hat weiterhin keinen frei ziehbaren Resize-Rand; Backendtests sind von echter Fenstergrößenänderung zu unterscheiden.

Granny, dessen Mesh-/Pose-/Skinning-Implementierung, Terrain-Draws, Charaktere/NPCs/Mobs, SpeedTree, Effekte und UI müssen für den belegten Bestand nicht umgebaut werden. Falls Runtime andere benötigte Varianten zeigt: erst belegen, dann eng begrenzt ergänzen; fehlende Assets oder nicht beauftragte Systeme nicht improvisieren.

## 5. Durch Laufzeittests belegte Präzisierungen

Die Abschnitte oben halten die Analyse und Planung vor der Implementierung fest. Die abschließende Umsetzung berücksichtigt zusätzlich diese beobachteten Originalzustände:

- Ein vorausgehender nativer SpeedTree-PCBlocker kann Stage-0-Alpha auf `MODULATE` belassen. Die statische Stage-1-Kameramaske ersetzt dieses Alpha anschließend vollständig. Beide vorhandenen Stage-0-Varianten werden daher akzeptiert; der SpeedTree-Code bleibt unverändert. Der zunächst reproduzierte Ausschluss wurde damit beseitigt.
- Das originale Shadow-Setup ändert nicht nur die Schattentextur: Es schaltet die Grundfarbe auf `TEXTURE * DIFFUSE` und Stage-0-Alpha auf `DISABLE`. Die finale Übergabe bewahrt diese Grundbeleuchtung einschließlich Materialalpha und des noch aktiven nativen Punktlichts 1. Sie übernimmt weiterhin **keine Schattentextur**. Ein reiner unbeleuchteter Grundpass wäre bei aktiviertem Schalter sichtbar heller gewesen; der direkte Bank-Vergleich und native D3D9-GPU-Fälle 25/26 prüfen die Korrektur.
- Die originalen Punktlichtanteile folgen der vorhandenen D3D9-Diffuse-/Ambient-Berechnung und Entfernung/Range/Attenuation, ohne neue Lichtlogik im Spiel. Referenz: [Ambient](https://learn.microsoft.com/en-us/windows/win32/direct3d9/ambient-lighting), [Diffuse](https://learn.microsoft.com/en-us/windows/win32/direct3d9/diffuse-lighting), [Attenuation](https://learn.microsoft.com/en-us/windows/win32/direct3d9/attenuation-and-spotlight-factor).
- `obj-0001`/`obj-0002` sind Two-Sided-Dekorationsprops, keine behaupteten Alpha-Test-Fahnen. Der echte A1R5G5B5-Anwendungsfall ist separat an `general_obj_drum` und dessen fünf Originalmips belegt.
- Im normalen Ingame-Lauf wurde der zusätzliche ShadowReceiver tatsächlich benutzt: `c1-022-10m-bridge.gr2`. Die finalen A1/B1-Testläufe und dieser C1-Ingame-Ausschnitt enthalten keine Adapter-Ausschlüsse oder Uploadfehler. Das ist keine visuelle Vollabnahme sämtlicher Objekte sämtlicher Maps.
- Die nativen Pixeltests lesen Alpha aus einer echten A8R8G8B8-Test-Renderfläche. Das unbenutzte X-Byte des D3D9-X8R8G8B8-Fensterpuffers ist kein gültiger Alpha-Referenzwert. Die Anpassung betrifft nur die Testreferenz.

Ergebnisse und ausdrücklich verbleibende Grenzen: `renderer-milestone4b.md`.
