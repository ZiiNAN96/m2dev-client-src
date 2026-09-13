# Milestone 6 — bestehendes SpeedTree in Diligent D3D11

> Historischer Milestone-Stand. Aktuell seit [M12](renderer-milestone12.md): Diligent D3D11 ist im ON-Build der Default; Legacy wird explizit mit `--renderer=legacy-d3d9` gewählt. OFF-Builds bleiben Legacy-only. Frühere Testergebnisse und Auswahlbeschreibungen unten gelten für ihren damaligen Stand.

Stand: 12.09.2026. **M6 für den geprüften normalen SpeedTree-Baumpfad abgeschlossen und freigegeben.** Release ON/OFF, vollständige Suite, Renderer ON/OFF und beide Ingame-Prüfungen bestanden. Abschließender ON-Build wiederhergestellt und erneut geprüft; Legacy bleibt weiterhin Startstandard. Die unten beschriebenen historischen LOD-/SDK-Grenzen bleiben ausdrücklich bestehen.

Ausgangspunkt: `288f4da` (M5D), sauberer Source-Checkout. Die [Analyse vor Produktionsänderungen](renderer-milestone6-analysis.md) enthält die 35 Architekturfragen, Komponentenmatrix und minimale Eingriffsliste. Kein anderer Renderer-Milestone wurde begonnen. Legacy D3D9Ex bleibt Startstandard; Diligent bleibt `--renderer=diligent-d3d11`.

## Architektur und Änderungen

```text
Originale Map/Area → Forest/MainTree/Instance → unveränderte SpeedTreeRT
                                              ↓ CPU-Geometrie/LeafTable
Originale Sichtbarkeit und native Drawreihenfolge
    ├─ unveränderte D3D9-Draws
    └─ TreeRenderBridge → TreeRenderData → DiligentTreeRenderer
                                         vorhandenes World-RTV/Depth
```

Die Bridge übernimmt synchron die schon angewandten D3D9-Materialstates, nicht einen eigenen globalen Standardzustand. Der Renderer kennt keine SDK-Pointer. Modellgeometrie und Texturen gehören den vorhandenen Modell-/Instanzbesitzern; Instanzen teilen ihre Modellressourcen. Kameraabhängige Billboard-Vertices gehören der einzelnen Instanz. Keine neue Runtime, kein Instancing, keine Batch-, Culling- oder LOD-Neuentwicklung.

Produktionsdateien:

- `src/Renderer/TreeRenderData.h`: API-neutraler CPU-/Drawvertrag, Handles, Welt-/Draw-Scope.
- `src/SpeedTreeLib/TreeRenderBridge.{h,cpp}`: CPU-Aufnahme, Originaltextur-Auflösung, vollständig geprüfter Draw-Snapshot, begrenzte Diagnose.
- `src/Renderer/DiligentTreeRenderer.{h,cpp}`: Vertex-/Index-/Textur-Uploads, PSO/SRV/Sampler, Originalshader-Semantik, Lebensdauerzähler.
- `SpeedTreeWrapper.{h,cpp}`: Capture nach Setup, vorhandene Modellteilung, wenige Hooks unmittelbar an nativen Draws.
- `SpeedTreeForestDirectX.cpp`: Welt-Scope; Shadow-/Minimap-Pässe ausgeschlossen.
- `GameLib/MapOutdoorRender.cpp`: ausschließlich Scope für die vorhandene CameraBlocker-Maskentextur.
- `Renderer/TerrainPresentation.cpp`, `DiligentD3D11Backend.h`, `Renderer/CMakeLists.txt`: Initialisierung, vorhandene Weltoberfläche, Framegrenze, Diagnose und Abbau.

## Die 34 Abschlusspunkte

| Nr. | Thema | Ergebnis / belastbare Grenze |
| --- | --- | --- |
| 1 | Runtime | Vorhandener Header `extern/include/SpeedTreeRT.h`: Release 1.6.0, 19.12.2003. Originale statische `speedtree_static.lib` / Debug-Variante. Keine zusätzliche interne Binärversion behauptet; keine neue SDK-Bibliothek. |
| 2 | Call-Chain | `CArea::__SetObjectInstance_SetTree → TObjectInstance::SetTree → Forest.CreateInstance/GetMainTree → Wrapper.LoadTree → SDK.LoadTree/Compute → SetupBuffers`; Render: `RenderGame → MapOutdoor.OnRender → RenderTree → Forest.Render → Advance → RenderBranches/Fronds/Leaves/Billboards → TreeRenderBridge.Draw`. |
| 3 | Klassen | `CSpeedTreeRT`, `CSpeedTreeWrapper`, `CSpeedTreeForest`, `CSpeedTreeForestDirectX`, `CArea`, `CMapOutdoor`; neu ausschließlich Bridge/Renderdaten/TreeRenderer. |
| 4 | Assets | Originale AreaData/Property-CRC, `.prt`, `.spt`, `.dds`; SPT durch SDK, DDS durch vorhandenen Pack-/StaticObjectTextureLoader. Keine eigenen SPT-Parser oder neue Assets. |
| 5 | Branches | Originale CPU-Positionen, D3DCOLOR, UV0/UV1, native Stripindizes/Offsets/Längen; Welttranslation. Statische Beleuchtung verwendet Farben, keine neue Normalenbeleuchtung. |
| 6 | Leaves | Native Centers, UVs, Cluster/Corner und LOD-Scalar; sechs Vertices pro Blatt. Aktuelle bereits hochgeladene D3D9-Konstanten übernehmen; ursprünglichen VS1.1-Ausdruck und Clip-Z-Nebel reproduzieren. |
| 7 | Fronds | Tatsächlich vorhanden: alle 15 initial geprüften A1/B1-SPTs besitzen Fronds. Derselbe minimale Indexed-Pfad, Composite-/SelfShadow-Texturen, Cull None. Nicht ausgelassen. |
| 8 | Billboards | Vorhandene vertikale SDK-Quads/UVs in sechs TriangleList-Vertices übersetzt, eigener dynamischer Instanz-VB. GPU-Parität der Quad-Ausgabe geprüft. Normale Welt aktiviert sie wegen bestehender LOD-Festsetzung nicht; kein echter Welt-LOD-Übergang behauptet. Horizontalvariante bleibt ausgeschaltet. |
| 9 | Vertices | Legacy Branch/Frond 32 Bytes; Leaf 40 Bytes. Neuer neutraler Vertex 40 Bytes: Position3, D3DCOLOR, UV2, ShadowUV2, LeafIndex/Scalar2. D3DCOLOR-BGRA korrekt nach Shader-RGBA umgesetzt. |
| 10 | Indizes | Originale uint16-Strips für Branch/Frond; native Substrip-Offsets. Leaves/Billboards nicht indiziert. Kein Mesh-Merging. |
| 11 | Materialien | Originalfarben × Originaltextur; optionale Stage1-SelfShadow-RGB- bzw. CameraBlocker-Alpha-Modulation. Keine Materialfeatures hinzugefügt. |
| 12 | Alpha-Test | Native `D3DCMP_GREATER`, tatsächlicher `DWORD(SDK AlphaRef)` pro Draw. Normal meist 84, keine fest erfundene Schwelle. D3D9-kompatible 8-Bit-Alpha-Auswertung; 84/127/255 und Maskenfälle im GPU-Test. |
| 13 | Alpha-Blend | Tatsächlich angewandtes Enable, SRCALPHA/INVSRCALPHA, ADD, gemeinsamer Alpha-Blend wie Legacy; pro PSO definiert. Kamera-Verdeckung benutzt den existierenden Pfad. |
| 14 | Cull | Branches CW; Fronds/Leaves/Boards None. Pro PSO explizit, nicht aus dem vorherigen Draw geerbt. |
| 15 | Depth | Z-Test, Z-Write, Vergleich pro Draw; gemeinsame existierende Depth-Fläche mit Terrain/Objekten/Actors. Zwölf zusätzliche Tree/Terrain-Fälle prüfen Reihenfolge und aktivierte/deaktivierte Depth-Regeln. |
| 16 | LOD | **Legacy ruft bereits `ComputeLodLevel(); SetLodLevel(1.0f)` auf.** Das bleibt unverändert. Kein neuer High-LOD-Zwang im Diligent-Code und kein Aktivieren anderer LODs. Near/Mid/Far geprüft; echte entfernungsabhängige Welt-LOD-Umschaltung ist kein Ergebnis von M6. |
| 17 | Wind | Aktive Konfiguration: NO_WIND für Ast/Frond/Leaf-Deformation, aber LeafRocking TRUE. Native LeafTable verändert sich messbar bei fester Kamera über die Zeit. Dieselbe Tabelle/Scalar übernimmt der neue Leafshader; keine neue Windmatrix-/Physiksimulation. |
| 18 | Texturen | Bestehender DDS-Decoder inklusive vorhandener Mips, Formate und Packpfade; Tree-GPU-Handles getrennt gezählt. Beide SRVs/Sampler pro Draw vollständig gebunden; native Wrap/Clamp, Point/Linear/Mip/unterstütztes Anisotropic. Keine universelle neue Texture Engine. |
| 19 | Visibility | Vorhandene BoundingSphere/Frustum-/Hide/Show-Entscheidung bleibt führend. Nur tatsächlich ausgeführte native World-Draws werden gespiegelt; keine blinde Ausgabe aller Map-Bäume. PART_TREE aus/an ebenfalls getestet. |
| 20 | Weltkomposition | Terrain/Splatting, Gebäude/Props, Player/Rüstung, NPCs/Mobs, Waffen/Haare und Mounts mit Bäumen sichtbar. User bestätigt normalen Ingame-Client. Effekte, Wasser, Ingame-UI/Text/Minimap bleiben nicht migriert. |
| 21 | Baumtypen | Beech, Pagoda, MontereyCypress, Sassafras; zusätzlicher Baobab im normalen Ingame-Lauf. 14 unterschiedliche SPTs im isolierten sichtbaren Lauf; 15 im normalen Ingame-Drawlog. SDK-Probe zusätzlich für B1-Baobab2 und normalen Baobab. |
| 22 | Maps | Isolierte Originalmap-Läufe A1/B1, Stadt und baumreichere Bereiche, keine neuen Tree-Platzierungen. Asset-Inventar: A1 368 Platzierungen/14 SPTs, B1 317/15. Normales Ingame separat durch Benutzer getestet. |
| 23 | Draw Calls | Maximale sichtbare Stichprobe: 854 Branch + 1468 Frond + 53 Leaf + 0 Billboard = **2375 Tree-Draws**. 636943 summierte Vertex-Referenzen pro Draw, 49634 Index-Referenzen. Keine Zahl eindeutiger transformierter GPU-Vertices und kein Performanceversprechen. |
| 24 | Sichtbare Instanzen | Dieselbe Stichprobe **53** eindeutige Instanzen mit mindestens einem Tree-Draw. 42 Geometrie-Handles / 56 Textur-Handles. Kein Anspruch, dass alle Draws am Ende sichtbare Pixel erzeugen. |
| 25 | Mapwechsel | Beide isolierten finalen Clients absolvieren alle 18 Phasen und A1 → B1 → A1 über echte Background.LoadMap/Destroy-Pfade. Keine automatisierte Serverteleport-Abnahme behauptet. |
| 26 | Resize | GPU-Parität wechselt wiederholt 640×480/800×600, suspendiert 0×0 und stellt wieder her; zusätzliche gemeinsame Tiefenprüfung 160×120. Vorhandene Presentation-/Backend-Resizetests bleiben aktiv. |
| 27 | Minimize/Restore | User bestätigt dreimal pro isoliertem Backend und im normalen Diligent-Ingame-Client. Legacy-Testfenster zusätzlich nach Wiederherstellung visuell geprüft. |
| 28 | Shutdown | Finale isolierte Prozesse: Diligent 60808 / Legacy 62068 jeweils 180.9 s, Exit 0. Normal: Legacy 63008, 72 s; Diligent 63344, 58.4 s; beide Exit 0. Keine Testprozesse zwangsbeendet. |
| 29 | Ressourcen | Beide finalen Diligent-Läufe: `tree_geometry=0 tree_textures=0`; ebenso Actor/Attachment/Mount/Object 0. SRBs werden beim Framewechsel freigegeben und PSOs/Sampler/Shader per RAII vor Backend.Shutdown zerstört. Zähler beziehen sich auf unsere Handles, nicht auf eine erfundene interne SDK-Allocator-Abfrage. |
| 30 | Builds | Release ON und OFF erfolgreich; abschließender ON-Wiederherstellungsbuild ebenfalls Exit 0 und erneut Renderer 7/7. Nur bekannte fehlende PDB-/Legacy-Warnungen, keine Buildfehler in diesen finalen Builds. |
| 31 | Suite | Komplette Suite **11/11**, 472.06 s einschließlich Fuzzer. Renderer ON **7/7**, OFF **5/5**, jeweils einschließlich neuem TreePolicy-Test. Kein vorhandener Test deaktiviert. |
| 32 | Legacy | Ohne Rendererargument, Originalpakete. User bestätigt Bäume, Nähe/Ferne, Figuren/Mount, Gebäude/Terrain, Effekte/UI; Exit 0. Legacy-Draws, Runtime, CPU-Skinning und Materialzustände nicht verändert. |
| 33 | Sonderfälle | Feste native Welt-LOD, deaktivierte horizontale Boards, ausschließlich aktive STATIC_LIGHTING/NO_WIND/GPU_LEAF_PLACEMENT-Konfiguration, historische Kamera-Leaf-Sampling-Semantik. Keine Freigabe anderer SDK-Builddefines oder beliebiger unbekannter Sonderassets. |
| 34 | Nächster Milestone | Erst nach separat erteiltem Auftrag eine begrenzte Analyse des bestehenden Effektpfads erwägen. Nicht Teil dieser Arbeit; kein Effekt-/Wasser-/UI-Schritt begonnen. |

## Gefundene Paritätsstelle und deterministische Bindings

Der Original-Leafshader schreibt nur `oT0`, nicht `oT1`. Ein an Stage1 gesetztes Fixed-Function-Camera-TCI erzeugt für diesen programmierbaren Shader keine neuen Blatt-UVs. Der native Vergleich auf diesem D3D9Ex-Treiber liefert dafür UV (0,0). Die erste Diligent-Fassung hatte Camera-UVs berechnet und bestand deshalb den verschärften Maskentest nicht. Sie verwendet jetzt den nachgewiesenen nativen Blattzustand. Vier Maskenfälle (Alpha am Ursprung 0/128/255/192, wechselnde Kamera, Point/Linear, Blend/DepthWrite) sichern ihn ab. **Keine Korrektur des Legacy-Shaders.**

Jeder Materialdraw setzt PSO, beide SRVs, beide Sampler, Blend/Cull/Depth-Konfiguration und sämtliche Transform-/Leaf-/Alpha-/Fogparameter. Sampler und PSOs dürfen gecacht sein, ihr vollständiger Schlüssel wird aus dem jeweiligen Draw gebildet. Kein globaler Reset nach jedem Draw. Die Bridge lehnt nicht analysierte aktive Zustände ausdrücklich ab und protokolliert sie begrenzt, statt sie still zu ersetzen. Finale Runs: keine Tree-ERROR-Zeile.

24 native GPU-Vergleiche: mittlerer RGB-Fehler maximal rund 0.071 von 255, keine inneren Coveragefehler, keine Alpha-Fehler oberhalb der 2/255-Prüftoleranz, keine DepthWrite-Fehler. Wenige Randpixel entsprechen Raster-/Alpha-Grenzen. Leaf-Transform/Scalar/Fog, Billboard-Geometrie, Kamera-Maske, Texture-Stages und wechselnde Zustände sind enthalten. Dazu zwölf exakte Tree/Terrain-Zentrumpixel-/Drawreihenfolgefälle. Die bereits bestehenden Terrain-/Objekt-/Actor-/Attachment-/Mounttests bleiben unverändert aktiv.

Die ersten Fehlversuche sind nicht als erfolgreiche Tests gezählt: ein Fehler in der neuen NPC-Testfixture (PC-Create/ChangeArmor statt vorhandener NPC-Register/SetArmor-Aufrufe), fehlende native State-Initialisierung nach ResetEx im neuen GPU-Oracle und ein falsch aufgebauter Test-Terrainpatch wurden ausschließlich im Testaufbau korrigiert. Nur der oben erklärte Blatt-Maskenunterschied erforderte eine Korrektur des neuen Produktionsshaders.

## Messung und Grenzen

Privater Speicher im finalen dreiminütigen Kartenlauf ab Sekunde 10: Legacy 315.9–350.0 MiB, zuletzt 321.4 MiB / 779 Handles; Diligent 406.8–523.2 MiB, zuletzt 476.3 MiB / 1117 Handles. Nach dem Höchststand fällt der Speicher wieder; kein monotoner Anstieg innerhalb dieses begrenzten Laufs. Unterschiedliche Map-/Asset-Caches und zwei gleichzeitig vorhandene Renderpfade sind enthalten. Das beweist keine unbegrenzte Leakfreiheit. Tree-Grafikbesitzer werden nach Shutdown dagegen explizit auf 0 geprüft.

Kameraentfernungen der Fixture 4200–19000, rotierende Kamera, Stadt-/dichtere Bereiche, native Culling-Wiedereinblendung, Tree-Visibility aus/an. Beide Backends verwenden dieselben Kameraphasen/Originalplatzierungen. Sichtprüfung an beiden Fenstern und Nutzerbestätigungen; kein zeitgleich pixelidentisches Ganzmap-Bild bei identischem SDK-Zeitpunkt behauptet. Die exakten Material-/Geometrievergleiche erfolgen im nativen GPU-Test.

Normalstarts loggen in beiden Backends den bekannten `MarkManager`-Hinweis `invalid idx 0`. Kein Tree-Fehler; außerhalb des M6-Scopes unverändert gelassen. Keine Crash-/Assertion-/Device-Fehler in den finalen Runs.

## Reproduzierbarkeit und Belege

- `tests/Renderer/SpeedTreeAssetProbe.cpp`: dieselbe native SDK-Bibliothek; reale SPT-Pfade als Argumente. Initiale 15-Asset-Ausgabe: `build/milestone6/tree-assets.log`.
- `tests/Renderer/TreeGpuChecks.h`: in bestehendem `Renderer.TerrainGpuParity`; optionale BMP-Belege über `M2_TERRAIN_TEST_CAPTURE_DIR`. Bestehende Fälle nicht abgeschwächt.
- `tests/Renderer/TreePolicyTest.cpp`: World-/Shadow-/Minimap-Grenze, native Modellteilung, frischer Drawzustand; ON und OFF.
- `setup_trees.ps1 -Name <frischer-name>` / `run_trees.ps1 -TestRoot <pfad> -Backend legacy|diligent`; `-Normal` bei Setup verwendet Originalroot-Paket statt Fixture. Benötigt die vorhandenen lokalen Runtime-Pakete und den privaten M5A-Testbootstrap. Testkopien, keine Änderung von Originalpaketen oder Original-EXE.
- `verify_trees.ps1 -TestRoot <pfad> [-Normal]`: Exitcodes, alle 18 Fixturephasen, Mapfolge, Komponenten-Draws, Treefehler und Ressourcenabbau; Sicht-/Loginbestätigung separat.
- Finale Laufbelege: `build/milestone6/visible-final` und `normal-final`, jeweils Exitdateien, Ressourcen-CSV, `trees-test.log`, `tree-renderer.log`, `terrain-renderer.log`.
- `build/milestone6/renderer-on-final.log`: 7/7; `full-suite.log`: 11/11; `renderer-off.log`: 5/5; `renderer-on-restored.log`: abschließend erneut 7/7 (13.43 s). Builds: `build-on-leaf-mask.log`, `build-off.log`, `build-on-final.log`, jeweils Exit 0.

## Git-/Scope-Bilanz

Neuer Tree-Vertrag, Bridge und Diligent-Renderer; kleine integrative Hooks und M6-spezifische Tests/Dokumente. 9 bestehende Dateien mit 59 hinzugefügten/1 entfernter Zeile, zusätzlich 5 neue Produktionsdateien, 8 Test-/Hilfsdateien und 2 Dokumente. Keine Änderung an Granny, CPU-Skinning, Actors/Attachments/Mount-Produktionscode, Terraingeometrie, LOD-Algorithmus, Effekten, Wasser, UI oder Assets. `git diff --check` erfolgreich. Keine Commits oder Pushes. Ursprünglich vorhandene Runtime-Änderung `config/channel.inf` unverändert erhalten. Eigene wesentliche Integrationsstellen sparsam mit `ZiiNAN` markiert.

**STOP: Testabschluss erfolgreich. Keine weitere Renderer-Migration begonnen.**
