# Milestone 4A – erste statische Weltobjekte über Diligent D3D11

> Historischer Milestone-Stand. Aktuell seit [M12](renderer-milestone12.md): Diligent D3D11 ist im ON-Build der Default; Legacy wird explizit mit `--renderer=legacy-d3d9` gewählt. OFF-Builds bleiben Legacy-only. Frühere Testergebnisse und Auswahlbeschreibungen unten gelten für ihren damaligen Stand.

Stand: 11.09.2026. Ausgangspunkt: `d466965` (`feat(renderer): port terrain splatting to Diligent`).

Der erste begrenzte Objektpfad ist implementiert und im echten A1/B1-Maptest sichtbar: nicht animierte, vollständig starre, opake PNT-Gebäude/Props mit einfacher Diffuse-Textur. Legacy D3D9Ex bleibt Standard; Diligent bleibt ausschließlich beim Start opt-in. Keine weitere Objektvariante und kein Milestone 4B wurden begonnen.

Die vor der Implementierung angelegte Analyse `renderer-milestone4a-analysis.md` beantwortet alle 27 Architekturfragen und enthält die Objektkategorien sowie die minimalen Eingriffspunkte. Die dort nachgetragenen Laufzeitbefunde sind unten ausdrücklich beschrieben.

## 1. Ausgewählter Objekttyp

Ein normales `BUILDING`-Mapobjekt als `CGraphicThingInstance`, repräsentiert durch die Steinlaterne:

- Property: `property/a/01/a1_018-stonelight.prb`, CRC `935928503`.
- Modell: `d:/ymir work/zone/a/building/a1_018-stonelight.gr2`.
- A1, Tile `002002`, `Object055`: Quelldaten `(58082.843750, -60984.550781, 19845.179688)`, Rotation `(0,0,15)`, Höhenkorrektur `-5`.
- Effektive Instanzposition im Adapterlog: `(58082.8, -60984.6, 19840.2)`.
- Ein Modell, keine Animationen, ein starres Mesh `Cone20`, ein Material/eine Triangle Group, 1.375 Vertices, 2.169 Indices, 723 Dreiecke.

Andere Gebäude und Props erscheinen nur dann über denselben Pfad, wenn sie denselben eng geprüften Vertrag erfüllen. Es wurde kein zusätzlicher Fels-, Gebäude-, Dungeon- oder Character-Renderer entwickelt.

## 2. Warum dieser Pfad zuerst?

Echtes Mapobjekt, ohne Animation, Skinning, Opacity-Map, Specular-Pass oder SpeedTree. Die Originaldaten sind während des bestehenden Modellladens zugänglich. Die bereits vorhandene Granny-Library wurde zur Untersuchung verwendet; kein eigener GR2-Parser entstand.

## 3. Tatsächliche Lade- und Render-Call-Chain

```text
Map laden
CMapOutdoor::LoadArea
→ CArea::Load / AreaData.txt / Property-CRC
→ CArea::__SetObjectInstance_SetBuilding
→ CResourceManager / CGraphicThing::OnLoad
→ bestehendes GrannyReadEntireFileFromMemory
→ CGraphicThing::LoadModels
→ CGrannyModel::CreateFromGrannyModelPointer
→ vorhandene Mesh-/Material-Konvertierung und D3D9-VB/IB
→ 4A: begrenzte Kopie derselben starren PNT-/Indexdaten

Sichtbarkeit und Zeichnen
CPythonApplication::RenderGame / bestehendes CCullingManager::Process
→ CPythonBackground / CMapOutdoor::OnRender
→ CMapOutdoor::RenderArea
→ CArea::CollectRenderingObject
→ bestehende Sortierung nach Kameradistanz, aufsteigend
→ CGraphicThingInstance::Render / OnRender / RenderWithOneTexture
→ CGrannyLODController, aktuelle Modellinstanz
→ CGrannyModelInstance::RenderMeshNodeListWithOneTexture
→ CGrannyMaterial::ApplyRenderState
→ CStateManager::DrawIndexedPrimitive → D3D9 (unverändert)
→ 4A: SubmitStaticMapObject
→ originale aktuelle LOD/Mesh-Gruppe/Materialpalette/finale Mesh-Matrix
→ IStaticObjectRenderer → DiligentStaticObjectRenderer
→ D3D11 immutable VB/IB + originale Textur → DrawIndexed
```

Der Diligent-Aufruf sitzt ausschließlich hinter dem vorhandenen Draw im sortierten opaken Mapobjekt-Loop. Der experimentelle Aufbau behält die D3D9-Zeichnung weiterhin bei; es gibt kein Hot-Switching und noch keine Einsparung doppelter Renderarbeit.

## 4. Relevante Klassen

`CMapOutdoor`, `CArea`, `CGraphicThing`, `CGraphicThingInstance`, `CGrannyModel`, `CGrannyMesh`, `CGrannyLODController`, `CGrannyModelInstance`, `CGrannyMaterialPalette`, `CGrannyMaterial`, `CCullingManager` sowie neu `IStaticObjectRenderer` und `DiligentStaticObjectRenderer`.

Granny übernimmt weiterhin Parsing, Modell-/Materialaufbau und sämtliche vorhandene CPU-Modelllogik. Änderungen in `EterGrnLib` betreffen ausschließlich die Client-Wrapper: rechtzeitige Datenkopie und Zugriff auf bereits berechnete Daten. Granny-Library, Parser, Skeleton, Pose, Animation und Skinning wurden nicht ersetzt oder geändert.

## 5. Dateien und minimale Eingriffspunkte

| Dateien, relativ zu `m2dev-client-src` | Änderung |
|---|---|
| `src/Renderer/StaticObjectRenderData.h` | Neutraler Daten-/Handle-/Draw-Vertrag, mapgebundener Capture-Scope |
| `src/Renderer/DiligentStaticObjectRenderer.{h,cpp}` | Getrennte Objekt-Pipeline, Upload, Draw und Ressourcenprüfung |
| `src/GameLib/StaticObjectBridge.{h,cpp}` | Ein zentraler Adapter für vorhandene Objekt-/Material-/Renderdaten |
| `src/GameLib/Area.cpp` | Capture-Scope beim bestehenden Building-Load; GPU-Freigabe vor Instanzlöschung |
| `src/GameLib/MapOutdoorRender.cpp` | Ein Submit-Aufruf im vorhandenen opaken Loop; Legacy-Draw bleibt |
| `src/EterGrnLib/Thing.cpp`, `Model.{h,cpp}` | Originaldaten vor bestehendem `GrannyFreeFileSection` bewahren, nur beim Diligent-Mapload |
| `src/EterGrnLib/ModelInstance.h` | Zugriff auf bestehende Instanzmaterialien und finale `m_meshMatrices` |
| `src/Renderer/TerrainTextureData.h`, `TerrainRenderData.h` | Kleines gemeinsames `ITextureUploader` statt terrainexklusiver Upload-Signatur |
| `src/EterLib/TerrainTextureLoader.{h,cpp}` | Derselbe Decoder/Packzugriff, nur allgemeiner Upload-Empfänger |
| `src/Renderer/TerrainPresentation.cpp`, `DiligentD3D11Backend.h`, `CMakeLists.txt` | Objekt-Renderer am bestehenden Gerät/Frame/Depth anbinden, Diagnostik und Build |
| `tests/Renderer/StaticObjectGpuChecks.h`, `TerrainGpuTest.cpp` | Bestehenden GPU-Test um 14 Objektfälle und gemeinsamen Terrain-Depth erweitern |
| `tests/Renderer/static_object_smoke.py` | Isolierter realer A1 → B1 → A1-Test ohne Login/Charakter |
| `docs/renderer-milestone4a-analysis.md`, `renderer-milestone4a.md` | Analyse und Abschlussnachweis |

Keine Änderung an `CStateManager`, Terrain-Geometrie/Splatting-Algorithmus, UI, SpeedTree, Actor-/Effect-Code oder Assets. Kein neuer Scenegraph, Materialmanager, Streaming- oder LOD-Entwurf.

## 6. Vertexformat

Exakt bestehendes PNT: Position `float3`, Normale `float3`, UV0 `float2`, 32 Byte. Keine Neukonvertierung in ein neues Meshformat. Der Snapshot ruft dieselben bestehenden `NEW_LoadVertices`-Routinen mit denselben Offsets auf. Deformable- und PNT2-Modelle werden abgewiesen.

## 7. Indexformat

Originale 16-Bit-Indices, Triangle List. Mesh-BaseVertex, Triangle-Group-Start und Anzahl bleiben erhalten. Native GPU-Parität prüft ausdrücklich einen von null verschiedenen BaseVertex und Gruppenstart; ungültige Indices werden abgewiesen.

## 8. Transformation und Culling

Unverändert übernommene finale `m_meshMatrices`, aktuelle View-/Projection-Matrizen, Row-Vector-/Row-Major-Konvention und bereits etablierte D3D9/D3D11-Pixelzentrum-Korrektur. Kein zusätzlicher Y-Flip; Höhenkorrektur kommt vom vorhandenen Area-Loader. Die Building-`Transform`-Methode verwendet Rotation plus Position; eine dort nicht angewendete gespeicherte Scale wird nicht nachträglich erfunden. Eingebettete Skalierung bleibt in der finalen Mesh-Matrix erhalten.

Wichtiger behobener Integrationsfehler: `GetMeshMatrixPointer` liefert eine Bone-/Kollisionsmatrix, nicht die finale Draw-Matrix. Der neue, ausdrücklich benannte Zugriff liefert die tatsächlich im Legacy-Draw verwendete `m_meshMatrices`-Matrix. Ein früherer Versuch mit dem falschen Zugriff zeichnete an falscher Stelle und ist **kein** Abnahmelauf.

Originales SpherePack-/Frustum-Culling, Show/Hide, Portal-/Area-Logik, Kameradistanzsortierung und aktuelle LOD bleiben maßgeblich. Der isolierte Test ruft zusätzlich zum bestehenden Update ausdrücklich `grp.Culling()` wie der normale RenderGame-Pfad auf. Drehung, Zoom und Positionswechsel ergeben unterschiedliche Objekt-Drawzahlen; es wird nicht blind die ganze Map eingereicht. Eine flächendeckende Prüfung jeder Portal-/PC-Blocker-Kombination wurde nicht durchgeführt.

## 9. Material- und bestehender Lichtpfad

Originale Materialpalette der **aktuellen Instanz**, originale Mesh-Gruppen und Diffuse-Zuordnung. Vor dem ersten Draw wird das Objekt hinsichtlich Modell-/Materialunterstützung vollständig geprüft: kein Motion-Thing, kein Blend-Thing, ausschließlich starres PNT, Diffuse-Material, kein Specular, keine zweite/Opacity-Textur.

Laufzeitbefund: `RenderArea` ruft `CArea::RenderDungeon` auch bei leeren Dungeon-Listen auf. Dieser Aufruf hinterlässt RGB `SELECTARG1(TEXTURE)`, Alpha `MODULATE`, Stage1 deaktiviert. Der getestete schattenfreie Außenobjektpfad verwendet deshalb unveränderte Textur-RGB plus vorhandenen Vertex-Fog, obwohl Lighting aktiviert ist. Dies wird exakt übernommen, nicht optisch verbessert.

Für den vorhandenen einfachen `MODULATE`-Zustand wird nur das bestehende Material-Ambient/Emissive plus gerichtetes Diffuse-Licht0 reproduziert; weitere aktive Lichter/Specular/zusätzliche Stages werden nicht approximiert. EXP/EXP2/LINEAR-Vertexfog und Rangefog sind geprüft. Keine moderne Licht- oder Materialtechnik.

## 10. Texturpfad

Bereits aufgelöster Dateiname aus dem originalen Image-/Materialobjekt → bestehender PackManager → bestehender DDS/Image-Decoder → `ITextureUploader` → separate Diligent-Objekttextur. UV0 und vorhandene Mips bleiben erhalten. Steinlaterne: `a1-018-stonelight-01.dds`, BC1, 256×256, 9 Mips.

Sampler übernimmt reale D3D9-Werte: Wrap/Clamp, Point/Linear oder das vorhandene anisotrope Min/Mag mit linearem Mipfilter, inklusive tatsächlichem MAXANISOTROPY. Nicht initialisierte Legacy-Sampler-Cachefelder werden **nur gelesen**, direkt vom Gerät; kein StateManager-Umbau. Getestetes reales Maximum: 1, keine neue Qualitätseinstellung.

BC1/BC2/BC3 und bereits vorhandene 32-Bit-RGB(A)-Uploadformate sind unterstützt. Reale 16-Bit-Objekttexturen, z.B. `general_obj_drum.dds`, werden mit Diagnose als außerhalb von 4A ausgeschlossen. Es wurde kein zusätzlicher 16-Bit-Konverter eingeführt.

## 11. Alpha Test

Im ausgewählten opaken Pfad deaktiviert. Der Legacy-Default speichert Referenz 1 und Vergleich GREATEREQUAL, aktiviert den Test dort aber nicht. Kein geratenes `discard`, kein neuer Grenzwert. Objekte/Zustände mit aktivem Alpha Test gehören nicht zu diesem 4A-Vertrag. Deshalb kein Alpha-Test-Bildunterschied beim Referenzobjekt; Cutout-Varianten sind nicht als gelöst behauptet.

## 12. Alpha Blending

Deaktiviert. Ein `HaveBlendThing` wird vollständig ausgeschlossen; der bestehende separate Legacy-Blendpfad mit SRCALPHA/INVSRCALPHA bleibt erhalten. Kein neuer Transparenzpass und keine neue Sortierung.

## 13. Cull-/Depth-State

Originales CW/CCW/None; `Two-sided`-Material setzt None. Bei Spiegelung kein automatisches Umdrehen der Indices. Depth aktiviert, Write aktiviert, LESS_EQUAL, derselbe Depth-Buffer wie Terrain.

GPU-Tests vergleichen native D3D9-Geometrieabdeckung über 14 Fälle: in jedem Fall **0 abweichende Abdeckungspixel**. Negative und nichtuniforme Skalierung sowie CW/CCW/None sind enthalten. Gemeinsamer Terrain-/Objekt-Depth wurde für vorne/hinten und beide Draw-Reihenfolgen geprüft: identische Farb-/Depth-Resultate unabhängig von Einreichungsreihenfolge.

## 14. Resource Lifetime

Der CPU-Snapshot entsteht während `LoadModels`, bevor der vorhandene Loader starre Vertex-/Index-Dateisektionen freigibt. Es gibt keinen unsicheren späteren Granny-Raw-Pointer- oder WRITEONLY-D3D9-Readback.

GPU-Caches gehören lebenden Area-Objektinstanzen. Vor der gepoolten Thing-Löschung werden Kontextbindungen freigegeben und der Eintrag entfernt; bei Mapwechsel werden keine Handles an eine neue Instanz vererbt. CPU-Snapshots folgen weiterhin dem vorhandenen verzögerten Modell-Resource-Cache, GPU-Ressourcen nicht.

Echter Diligent-Lauf: `release live_objects=0` nach A1-Freigabe, nach B1-Freigabe und beim abschließenden A1-Shutdown; danach `shutdown object_geometry=0 object_textures=0`. Rückkehr nach A1 erzeugt gültige neue Ressourcen und dasselbe sichtbare Bild. GPU-Test prüft zusätzlich abgelaufene Weak-Pointer und Zähler null. Das sind Engine-Handle-/Lebensdauerprüfungen, keine Behauptung eines externen GPU-Leak-Profilers.

## 15. Diagnostik und typische Drawcall-Zahlen

`terrain-renderer.log`: Anzahl Object-Draws, lebende Geometrien/Texturen neben unveränderten Terrain-Zählern. `static-object-adapter.log`: eingereichte bzw. bewusst ausgeschlossene Objekte, anfängliche States und Freigaben.

| Reale Kamera | Typische Object-Draws pro Frame |
|---|---:|
| A1, Steinlaterne, Distanz 2.500, Winkel 15/0 | 134 |
| A1 gedreht, 2.500, 20/95 | 96 |
| A1 herausgezoomt, 5.000, 35/180 | 49 |
| A1 bewegt, 10.000, 12/270 | 67 |
| B1, 7.000, 15/0 | 63 |
| A1 erneut, ursprüngliche Kamera | 134 |

A1-Referenz: 43 Terrain-Patches / 190 Splat-Draws. Anfangs 123 Objektgeometrien / 134 Texturen im Cache; nach der Kamerafahrt zeitweise 212/234; nach Rückkehr 120/128. Cachegröße ist kein Sichtbarkeitszähler und nicht mit Drawzahl gleichzusetzen. Keine Performance-/FPS-Verbesserung behauptet, keine neue Optimierung.

## 16. Builds

Release-Build mit `M2_ENABLE_DILIGENT_D3D11=OFF`: erfolgreich, Exit 0; alle 3 dann verfügbaren Renderer-Tests bestanden. Release-Build mit `ON`: erfolgreich, Exit 0. Zum Schluss wieder **ON** konfiguriert. Keine Tests manuell deaktiviert; Diligent-spezifische Tests sind beim OFF-Build wie bisher bedingt nicht vorhanden.

MSVC 14.44 / Windows SDK, lokaler bereits vorhandener Diligent-Checkout. Bestehende Bibliotheks-/PDB-Linkwarnungen bleiben; keine Buildfehler. Reale Maptests benutzten isolierte Kopien des korrekten ON-Builds vor dem OFF/ON-Matrix-Rebuild, normale Starttests und finale GPU-Suite den danach neu gelinkten ON-Build; derselbe Implementierungsstand, nicht byte-identische EXEs.

Nachweise: `milestone4a-configure-{off,on}.log`, `milestone4a-build-{off,on}.log`, `milestone4a-tests-off.log`.

SHA256 des zuletzt gelinkten ON-Clients und beider normalen Starttest-Kopien: `B3D612A2BC4E40FB889C2987A8CCF86B30730E974FAAF38BEF6777A0D2B60174`. Die beiden vorherigen real-map-Testkopien teilen sich `C4836ADA0E8AE2BDBBFC2DCB5D8FC4BCBC7E0BF3B9E2F8B7E39186107E05BE28`.

## 17. Testsuite und Renderer-Tests

Vollständige vorhandene Suite: **9/9 bestanden, 0 Fehler, Exit 0**, Gesamtdauer 451,70 Sekunden. Enthalten sind `fullbench`, `fuzzer`, `zstreamtest`, `playTests` und alle fünf Renderer-Tests. Kein Test deaktiviert oder übersprungen. Der Fuzzer lief 451,63 Sekunden und wurde regulär beendet, nicht abgebrochen.

Die fünf Renderer-Tests des ON-Builds sind bestanden: StartupOptions, TerrainTextureCpu, DiligentD3D11, TerrainGpuParity, LegacyD3D9. Bestehende Geometrie-/Textur-/Splat-Tests bleiben aktiv und bestehen weiterhin. Der erweiterte GPU-Test besteht zusätzlich separat mit Exit 0.

14 neue Objekt-Vergleichsfälle gegen natives D3D9 Fixed Function: Transform, Gruppen-/Base-Offsets, Spiegelung, nichtuniforme Skalierung, Materiallicht, Textur-only-Operation, Fog, Cull, Filter/Wrap/Clamp/Mips, Resize 640×480 → 800×600, Suspend 0×0 und Restore. Mittlere RGB-Abweichung 0,000 bis 0,240/255; überall 0 abweichende Abdeckungspixel. Originaler Textur-only-Fall: exakt 0 RGB-Abweichung. Zusätzlicher gemeinsamer Terrain-Depth-Test und ungültiger Index-/Ressourcenfreigabetest bestanden.

Testfixture-Korrektur: Nach D3D9Ex-Reset können native States erhalten bleiben, während der alte Cache zurückgesetzt wird. Der GPU-Referenztest setzt deshalb Fog explizit über einen Gegenwert und dann den gewünschten Wert. Kein Produktionsfix am StateManager, kein gelockerter Toleranzwert.

Nachweise: `milestone4a-all-tests.log`, `milestone4a-final-gpu.log`.

Reproduktion im Source-Repository: `cmake -S . -B build -DM2_ENABLE_DILIGENT_D3D11=ON`, dann `cmake --build build --config Release --parallel 4`. Für die vollständige Suite wurde Git Bash mit `/usr/bin` am PATH-Anfang verwendet: `ctest --test-dir build -C Release --parallel 3 --timeout 900 --output-on-failure`. OFF-Build identisch mit `OFF`; danach `ctest --test-dir build -C Release -R '^Renderer\.' --output-on-failure`. Die statischen Objektprüfungen sind Bestandteil von `Renderer.TerrainGpuParity`, keine lediglich manuell behaupteten Checks.

## 18. Legacy-Regression, reale Maptests und normale Starts

| Prüfung | Ergebnis |
|---|---|
| Legacy real-map, A1 → B1 → A1, Kamera drehen/zoomen/bewegen | Sichtbar korrekt; automatisches Map-/Window-Cleanup, Exit 0 |
| Diligent real-map, A1 → B1 → A1 | Gebäude, Steinlaterne, Props sichtbar; Cleanup, Exit 0 |
| Legacy minimieren/wiederherstellen | Vom Nutzer bestätigt: „richtig“ |
| Diligent minimieren/wiederherstellen | Vom Nutzer bestätigt: „ja paaasst“ |
| Normale EXE ohne Renderer-Argument | Legacy-UI/Serverauswahl sichtbar, Nutzer beendet, Exit 0 |
| Normale EXE mit `--renderer=diligent-d3d11` | Bestehende Start-/Login-UI sichtbar, ohne Login beendet, Exit 0 |
| Alle vier finalen Runtime-Fehlerlogs | `syserr.txt` jeweils 0 Byte |
| Diligent finaler Shutdown | Object-Geometrien und Texturen 0 |

Automatische Testfenster schließen nach sechs Kameraposen selbstständig. Die regulären Starttests waren getrennte Prozesse mit eigenen Config-/Logordnern; andere laufende Spielclients blieben unangetastet. Reale Map-Packs wurden gelesen, nicht verändert. Kein Deployment der experimentellen EXE in den normalen Clientordner.

Abgrenzung: Keine erneute interaktive Vollspiel-Abnahme mit eingeloggtem Charakter, Kampf, NPCs/Mobs, Effekten und SpeedTree. Diese Produktionspfade sind im Diff unverändert; daraus folgt keine pauschale Behauptung, jede denkbare Legacy-Spielsituation sei visuell erneut getestet. Ein solcher Gesamtclient-Regressionslauf bleibt vor einer breiteren Freigabe sinnvoll.

## 19. Bildvergleich

Unveränderte Fensteraufnahmen, 1.025×800 einschließlich Titelleiste, Client 1.024×768. Identische A1-Kamera: Ziel `(58082.84375, -60984.550781, 20195.1796875)`, Distanz 2.500, Pitch 15, Rotation 0, FOV 30, Near 100, Far 25.600. Diligent-Aufnahme am ersten A1-Aufenthalt, Legacy-Aufnahme nach Rückkehr mit derselben Pose.

Dateien im Nachweispaket: `a1-pose0-legacy-verified.png`, `a1-pose0-diligent.png`. Keine Retusche, keine neue Grafik zum Verdecken von Unterschieden.

| Bildausschnitt, Fensterkoordinaten `[x0,y0,x1,y1)` | Mittlere absolute RGB-Abweichung /255 | Exakt gleiche RGB-Pixel |
|---|---:|---:|
| Steinlaterne `[218,56,264,230)` | 0,02349 | 97,76 % |
| Großes Gebäude `[587,32,978,232)` | 0,11364 | 94,74 % |
| Vordere Props `[790,438,1020,592)` | 0,37614 | 73,56 % |
| Vorderes Terrain mit Zaun `[1,300,760,799)` | 0,31396 | 78,45 % |

Dies sind fest angegebene Rechtecke **einschließlich lokalem Hintergrund**, keine geschönten Objektmasken. Die Werte sind nicht als Vollbild-/Alle-Objekte-Parität zu lesen. `compare_real_objects.py` im Nachweispaket berechnet sie nur lesend aus den Originalbildern.

Geometrie/Transform: kein sichtbarer Versatz oder verzerrtes Mesh beim Referenzobjekt; native synthetische Abdeckungsprüfung zusätzlich exakt. Texturen/UV: gleiche Details, kleine Filter-/Rundungsabweichungen. Alpha-Test: auf beiden Seiten aus. Depth: optisch gleiche Überdeckung im Objektbereich, zusätzlich separater quantitativer Front-/Hintergrundtest.

## 20. Verbleibende Unterschiede und Grenzen

- Nicht unterstützte Varianten werden diagnostiziert und nicht teilweise als unterstütztes Objekt ausgegeben: Animation, Deform/PNT2, Blend/Opacity, Specular, aktive Alpha-Tests, zusätzliche Texturstages/Lichter, Schattenreceiver-Varianten und ausgeschlossene Texturformate.
- Der Echt-Mapvergleich verwendet ausdrücklich `ShadowLevel(0)`. Schattenreceiver-Parität ist nicht Bestandteil dieser Abnahme.
- Im oberen Bildbereich bleibt ein sichtbarer Unterschied außerhalb des ausgewerteten Objektbereichs: Legacy zeigt dort ferneres Terrain/Hintergrund, Diligent seine blaue Clear-Fläche. Keine Vollbildparität behauptet; der Terrain-/Hintergrund-Sonderfall wurde nicht als 4A-Nebenumbau angefasst.
- Der bestehende experimentelle Diligent-Child-Surface-Aufbau aus den vorigen Milestones ist keine fertige Legacy/Diligent-Komposition. Nicht migrierte Legacy-Draws laufen unverändert weiter, werden bei sichtbarer Diligent-Fläche aber nicht automatisch darüber komponiert. Insbesondere keine Behauptung, UI/Characters/SpeedTree seien bereits im Diligent-Weltbild integriert.
- Ein Modell, das vor dem mapgebundenen Capture schon außerhalb dieses Pfads gecacht wurde, erhält keinen nachträglichen unsicheren Raw-Readback; ohne geeigneten Snapshot wird es ausgeschlossen.
- CPU-Snapshots/Objekt-GPU-Caches verursachen Zusatzspeicher; D3D9-Draws bleiben parallel. Kein Batching/Instancing/Streaming-/LOD-Redesign.
- Diagnose-/Integrationsläufe mit nicht erfüllten States, nicht unterstützter 16-Bit-Textur oder falscher Bone-Matrix zählen nicht als erfolgreiche Abnahmen. Die veröffentlichten Bilder/Logs stammen aus den korrigierten, erfolgreichen Läufen.

## 21. Weiterhin ausschließlich Legacy

Animierte Gebäude und nicht unterstützte Material-/Vertexvarianten, Dungeon-Blöcke, SpeedTree, Spieler/NPCs/Mobs, Mounts/Attachments, Effekte/Partikel/Flying Objects, Wasser, UI/Text/Minimap. Collision/Attribute/Sound bleiben bei ihrer bestehenden Logik. Keine Granny-, Shader-/Material-Modernisierung über die ausdrücklich nachgebildeten Legacy-Funktionen hinaus.

## 22. Empfehlung für einen separat zu beauftragenden Milestone 4B

Genau eine häufige, im Ausschlusslog belegte starre Objektvariante auswählen und zuerst ihren vollständigen Legacy-Vertrag analysieren – z.B. ein reales Objekt mit bisher ausgeschlossener 16-Bit-Diffuse-Textur. Anschließend nur diesen Pfad ergänzen, dieselben Bild-/Lifetime-/ON/OFF-Tests wiederholen. Schatten, Cutout und Blend nicht gleichzeitig hinzufügen. Vor breiter Spielbarkeit außerdem die bestehende Hybrid-Komposition gesondert planen. **Nicht umgesetzt; separater Auftrag erforderlich.**

## Git-Diff und Übergabe

14 bestehende Dateien mit kleinen Integrationsänderungen, 9 neue Dateien einschließlich zweier Dokumente. Bestehender Code: 67 hinzugefügte / 7 entfernte Zeilen; der Hauptteil der neuen Implementierung und Tests liegt in getrennten Dateien. `git diff --check` ohne Fehler. Keine Commit-/Push-/Reset-/Stash-/Clean-Aktion.

Client-Source ist absichtlich uncommitted. Im Runtime-Repository bleibt die bereits zuvor geänderte `config/channel.inf` unverändert bestehen. Server-Repositories und reale Map-/Objekt-Assets wurden nicht geändert. Isolierte Build-/Testartefakte liegen unter `build/milestone4a` und gehören nicht zum Quellcode-Diff.

**STOP nach 4A.** Keine automatische Fortsetzung mit 4B oder weiteren Objekt-/Character-/Animationspfaden.
