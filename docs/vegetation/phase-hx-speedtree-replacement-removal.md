# H-X – Vegetation Runtime / Gate A1

Autonomer Lauf vom 14. September 2026. Kein Commit, kein Push, keine manuelle Bestätigung erfunden.

**H-X Gate A1: GO**  
**H-X Gate A2: GO – erneute Sichtprüfung am 15.09.2026 mit „passt“ bestätigt**  
**Gate B: NOT STARTED**

Der normale Clientstart bleibt auf SpeedTree Reference. Die neue Vegetation wird ausschließlich mit --vegetation=ziinan ausgewählt. Kein automatischer Rückfall auf SpeedTree.

Nachtrag 15.09.2026: Die manuelle A2-Prüfung meldete massive Blattartefakte. Ursache und begrenzte Korrektur, neue Vorher-/Referenz-/Nachherbilder sowie aktuelle Nachweise stehen in [H-X A2 Visual parity fix](phase-hx-a2-visual-parity-fix.md). Die erneute Sichtprüfung wurde anschließend vom Benutzer mit „passt“ bestätigt: A2 GO. Die folgenden A1-Ergebnisse bleiben als historische Nachweise erhalten. Gate B ist weiterhin nicht gestartet.

## 1. Ausgangslage

Basis ist der aktuelle Granny-freie F34-Stand: nativer GR2-Reader, eigene AnimationRuntime, produktives GPU-Skinning und Diligent D3D11. Source/CMake liegen in m2dev-client-src, Originaldaten in ../m2dev-client/assets und ../m2dev-client/pack. Der Source-Checkout war zu Beginn sauber. SDK und Referenzpfad bleiben bis zur manuellen Freigabe vorhanden.

## 2. SPT Corpus

**118 rohe und 118 effektiv gepackte SPTs**, alle SDK-lesbar. Alle enthalten die Kennung __IdvSpt_02_; daraus wird keine unbelegte SDK-Produktversion abgeleitet. Pfade, Dateigrößen, SHA256 und Features stehen in [phase-hx-feature-matrix.csv](phase-hx-feature-matrix.csv). Vollständige lokale Referenzdaten: build/hx/audit/corpus.json.

## 3. Production References

**82 durch Map-/Property-/Actor-Konfigurationen verwendete Typen, 12.819 Platzierungen in 60 Map-Verzeichnissen.** Dazu kommen drei hardcodierte Weihnachtsbaumvarianten in PythonBackground.cpp: **85 produktiv erreichbare Typen insgesamt**. 85 Tree-Properties wurden gelesen; Eventbäume besitzen keine festen Mapplatzierungen.

Die effektive Packreihenfolge entspricht dem Client: zuletzt registrierter Pfad gewinnt. Alle 118 SPTs stimmen bytegenau zwischen ausgewähltem Pack und Quelle überein. Zwei koreanisch benannte AreaData-Sicherungskopien sind über den schmalen Audit-Dateipfad nicht identisch zuordenbar. Sie heißen nicht areadata.txt und gehören nicht zu den geladenen Mapdaten; sie werden nicht als Platzierungen gezählt.

## 4. SpeedTree API Baseline

CArea/Actor/Event → CSpeedTreeForest::CreateInstance → CSpeedTreeWrapper::LoadTree/MakeInstance → GetGeometry/GetLeafBillboardTable → TreeRenderBridge → DiligentTreeRenderer. Genutzt werden Bounds, Collision-Daten, Texturbezüge, statische Farben, dynamische Normalen, diskrete LODs, Kameratabellen und Blattbewegung.

**Der bisherige Wrapper berechnet LOD, erzwingt danach aber SetLodLevel(1.0f).** Der normale Reference-Client bleibt deshalb auf Nah-LOD. H-X aktiviert ausschließlich im expliziten neuen Testpfad die tatsächlich vorhandenen SDK-LODs und Fernbillboards.

## 5. Feature Matrix

| Feature | Bestand |
|---|---:|
| Branch-Geometrie | 118 Typen |
| Frond-Geometrie | 94 Typen |
| Leaf-Geometrie | 92 Typen |
| ein Fernbillboard | 118 Typen |
| Branch-LOD-Anzahlen | 115 × 6; 3 × 2 |
| Frond-LOD-Anzahlen | 102 × 4; 16 × 6 |
| Leaf-LOD-Anzahlen | 108 × 4; 7 × 3; 3 × 2 |
| SDK-Collision-Objekte | 242 |
| fehlende benötigte Texturen | 0 |

Leere Geometrie in einer vorhandenen SDK-LOD bleibt als nicht zeichnender Zustand erhalten. Deklarierte LOD-Anzahl und vorhandene Dreiecke werden getrennt erfasst.

## 6. Converter Architecture

Optionales Flag ZIINAN_BUILD_LEGACY_SPT_CONVERTER=ON, standardmäßig OFF. Das Windows-Tool benötigt M2_BUILD_ASSET_TOOL=ON und verwendet die vorhandene AssetToolCore-Scene und deren GLB-Writer. Kein neuer allgemeiner Importer, Mesh-Writer, Texturdecoder oder Renderer.

Targets: ZiiNANSPTProbe, LegacySPTExtraction, ZiiNANVegetationTool, VegetationPackAudit und der ausschließlich diagnostische VegetationRenderParity. Keines davon wird vom VegetationRuntime-Target gelinkt.

## 7. Neutral Vegetation Source

VegetationSource besitzt AssetTool::Scene, neutrale Metadaten, Legacy-Key und Diagnosezähler. Der öffentliche Header enthält keine SDK-Typen. SDK-Zeiger werden innerhalb des Offline-Extractors gelesen und sofort in eigene Daten kopiert.

## 8. Geometry Extraction

Branch-/Frond-Strips werden mit wechselnder Winding-Reihenfolge in Dreiecke umgesetzt. Wiederholte Indizes eines degenerierten Strip-Dreiecks erzeugen kein Dreieck. Leaves erhalten vier Vertices/sechs Indizes je Karte; jede wirkliche LOD wird getrennt extrahiert.

Die SDK-LeafMap-Koordinaten enthalten ihre LOD-Größe bereits. Eine zunächst doppelte Skalierung wurde behoben und durch unabhängige Positions-/Bildprüfungen abgesichert. Statische Farben müssen vollständig kopiert werden, bevor eine zweite SDK-Abfrage dynamische Normalen anfordert: das geschlossene SDK teilt intern Beleuchtungszustand und Speicher.

## 9. GLB Geometry

Gewöhnliche Meshes/Materials über den vorhandenen GlTFProvider. Export: Z-up/Zentimeter → Y-up/Meter mit (x,z,-y) × 0,01. Der vorhandene Provider wandelt einmalig zurück mit (x,-z,y) × 100. Ausgewählte Positionen werden gegen Originalwerte geprüft.

Optionale gemeinsame Vertexkanäle: COLOR_0, TEXCOORD_1, _ZIINAN_PIVOT, _ZIINAN_FLEXIBILITY, _ZIINAN_CARD_PITCH_COS und _ZIINAN_CARD_PITCH_SIN. Die letzten beiden beschreiben die Kameraantwort im kanonischen Blattkoordinatensystem; keine SDK-Speicherabbilder.

## 10. ZVEG Metadata

Versioniertes JSON: GLB-Pfad, Shadow-Textur, Referenz-/Renderbounds, LOD-Distanzen, Parts, Alpha-Zustände, Windprofil und Collision-Daten. Grenzen: 2 MiB, 16 Verschachtelungsebenen, 256 Parts, 2–2049 LOD-Samples, 256 Collision-Objekte und endliche begrenzte Zahlen. Ungültige Pfade, doppelte Schlüssel, falsche Slot-Typen, inkonsistente Meshindices und Bounds werden abgewiesen. Parsing veröffentlicht erst ein vollständig gültiges Ergebnis.

## 11. Registry Generation

vegetation/registry.json enthält 118 normalisierte Legacy-Keys und ZVEG-Ziele. Die ursprüngliche Verzeichnisstruktur unter vegetation/ymir work/... verhindert Kollisionen gleicher Basenames. Fehlerhafte Batches liefern Exitcode 1 und veröffentlichen keine neue Registry.

Ausgaben erst nach erfolgreichem Batch und Strukturprüfung übernehmen. Der Converter ist kein Hot-Reload-/Transaktionssystem: ein erneuter fehlgeschlagener Batch kann vorhandene Ausgabedateien bereits teilweise ersetzt haben. Deshalb während einer Konvertierung keine laufende Deploymentkopie als Ausgabe verwenden. Die hier bereitgestellte Testkopie entstand erst nach vollständig erfolgreichem Batch und wurde per Hash geprüft.

## 12. Resolver

Alte .spt-Referenz → Registry → ZVEG → GLB → vorhandener GlTFProvider. Der neue Runtime-Pfad öffnet/parst keine SPT-Datei. Slashes und Groß-/Kleinschreibung werden normalisiert. Kompilierte Pfade sind auf vegetation/ begrenzt. Erfolge und Ladefehler werden gecacht; unbekannte Keys lösen keine Datei-I/O aus.

## 13. VegetationAsset

Ein immutable Asset hält Metadaten und AssetRuntime::AssetHandle. Der Renderadapter hält gemeinsame Geometrie-/Texturhandles. Der Loader verlangt ein starres renderbares Modell, korrekte Parts und alle Zusatzkanäle; Renderbounds müssen die tatsächliche GLB-Geometrie umfassen.

## 14. Vegetation Runtime

src/Vegetation enthält Registry, Parser, Assetcache, Instance, LOD-/Bounds-/Windlogik und Adapter zum vorhandenen IStaticObjectRenderer. Öffentliche Header enthalten keine SDK-, Windows-, D3D11-, Loader- oder Assimp-Typen. WorldTree verbindet diese Schicht mit bestehenden Fabriken. Vegetation.Boundary und GCC prüfen die Trennung.

## 15. Asset Sharing

Instanz: Transform, LOD-Zustand, deterministische Phase und geteiltes Assethandle. Geometrie wird pro Asset/LOD hochgeladen. CResourceManager/CGraphicImage und dessen Uploadcache liefern die Texturen.

Der neutrale Test erstellt 1.000 Instanzen mit genau einem Meshupload, einem Texturlookup und zwei initialen Datei-Reads. Zeichnen verursacht keine weiteren Reads. Keine Mesh- oder GPU-Texturkopie pro Baum.

## 16. GPU Instancing Decision

Gemeinsame immutable Geometrie und kleine Instanzzustände sind vorhanden; gebündelte Instancing-Draws noch nicht. Parität von Strips, Karten, Alpha, Verdeckung und LOD hatte Vorrang vor einem zusätzlichen Batching-Umbau. Die Asset-/Instanzgrenze erlaubt späteres Batching. Maximal fünf aktive Parts je Instanz.

## 17. Frustum Culling

CWorldTreeInstance registriert konservative Bounding Spheres im bestehenden World-Culling. Nur isShow()-Instanzen werden eingereicht. Der portable Kern hat zusätzlich einen getesteten AABB-/Plane-Test. Konservative Bounds können zusätzliche Offscreen-Submissions zulassen, sollen sichtbare Karten aber nicht vorzeitig abschneiden.

## 18. Distance Culling

Die bisherigen höhenbezogenen Faktoren 2 und 9 bilden Near/Far. ZVEG enthält eine explizite Culldistanz, standardmäßig das Doppelte von Far. Diese Grenze lässt sich pro kompiliertem Asset anpassen; keine einheitliche globale Abschneidedistanz für alle Baumgrößen. Eine neue Benutzeroberfläche für Qualitätsparameter wurde nicht eingeführt.

## 19. LOD

257 offline erfasste Zustände über SDK-Level 0–1 enthalten Branch, Frond, bis zu zwei Leaf-LODs und Billboard mit eigenen Alpha-Grenzen. Jede Instanz wählt ihren Zustand aus der Kameradistanz. Kein gemeinsamer globaler Asset-LOD-Zustand.

## 20. LOD Transitions

SDK-Zustände, Leaf-Überlappungen und Alpha-Grenzen bleiben erhalten. Interpolation nur zwischen Samples derselben Meshkombination. Diskrete Schwellen haben 1/256-Auflösung des LOD-Bereichs. Kein neuer Dither-/Remaster-Crossfade. Sichtbare Pops sind Bestandteil von A2.

## 21. Branches

Originalgeometrie, Strip-Winding, UV0/UV1, Farben, Normalen, Alpha-Test und Clockwise-Culling. Gebackene Vertexfarben und vorhandene Self-Shadow-Textur werden im normalen Meshrenderer verwendet. Branch-Windamplitude bleibt gemäß tatsächlicher No-Wind-Baseline null.

## 22. Fronds

Composite-Atlas, Alpha-Test, beidseitige Darstellung und vorhandene Self-Shadow-UV1. Keine Behandlung als opake Branches. Farn und Kokospalme erweitern den Bildvergleich um ausgeprägte Frondformen.

## 23. Leaves

Pivot, Kartenform, UV, Farbe und echte LOD-Größe bleiben erhalten. Die SDK-Kameraantwort variiert je Blattgruppe: zwei kleine immutable Vektoren pro Vertex beschreiben ihre Cosinus-/Sinus-Reaktion. Alle Leaf-LODs werden bei vier zusätzlichen positiven/negativen Kameraneigungen überprüft.

Vor erzwungenen LOD-Abfragen muss die SDK-Kameratabelle regulär aktualisiert werden; andernfalls liefert sie bei höheren LODs veraltete Werte. Dieser während der Umsetzung gefundene Fehler wurde korrigiert.

## 24. Billboard

Alle 118 Typen verwenden ein Fernbillboard. Der Extractor prüft 16 Azimutrichtungen und drei Elevationen auf zweite/horizontale Billboards und wechselnde Atlas-UVs. Nicht erfasste Varianten würden ausdrücklich abgewiesen. Das exportierte Billboard bleibt aufrecht und folgt dem Kameraazimut. Kein neues Impostor-System.

## 25. Textures

**Fehlende benötigte Texturen: 0.** DDS-Bezüge entsprechen der bisherigen Kombination von SPT-Verzeichnis und SDK-Dateiname. GLB erlaubt kontrollierte d:/ymir work/...dds-Packpfade; der Provider führt selbst keine externe Datei-I/O aus. Keine Original-SPTs oder DDS-Dumps in Testfixtures kopiert.

## 26. Materials

Branch/Frond/Leaf/Billboard mit Alpha-Mask und bisherigen Cull-Unterschieden; wechselnde Alpha-Schwellen stammen aus ZVEG. Kameraverdeckung multipliziert bestehende Texturalpha mit Maskenalpha. Leaves behalten die konstante zweite UV und den alten Clip-Z-basierten Nebel; übrige Parts nutzen regulären World-Nebel. GPU-Vergleiche decken diese Varianten ab.

## 27. Wind Audit

WRAPPER_USE_NO_WIND ist aktiv: Branch-/Frond-CPU-/GPU-Winddeformation ist aus. Leaf-Rocking bleibt aktiviert; alle 118 SDK-Tabellen ändern sich zwischen t=0 und t=1,25. EnvironmentData::fWindStrength wird weitergereicht. Die bisherige Integration liefert keine eigene deterministische platzierungsabhängige Instanzphase.

## 28. Wind Profile

Neutraler Typ mit Richtung, Stärke, Branch-/Frond-/Leaf-Amplitude und Frequenz. Konvertierte Baseline: Branch/Frond=0, Leaf=0,018 rad, Frequenz=1,3. Die neue kleine Bewegung ist eine eigene deterministische Grundfunktion. Eine exakte zeitliche Reproduktion der geschlossenen SDK-Windfunktion wird nicht behauptet. Die Bildparität verwendet abgeschaltete eigene Bewegung.

## 29. Instance Wind

FNV-basierte Phase aus Position und optionaler stabiler ID, keine Zufallswerte pro Frame. Zeit und Umgebungsstärke steuern die Blattbewegung. Tests prüfen verschiedene Phasen, geänderte GPU-Pixel bei t=1,25 und identische Pixel bei wiederholtem t=0. Keine Windvolumes oder Wettersimulation.

## 30. Bounds

Original-SDK-Bounds bleiben als Vergleichsdaten erhalten. Separate Renderbounds umfassen alle LOD-Geometrien, Kameradrehung, Blatt-Pitch-Reaktion und kleinen Windspielraum. Instanzbounds transformieren alle acht Ecken.

Sechs SPTs liefern je 72 ungültige Zusatznormalen über LOD-Kopien: b2_japanesemaple_rt_fall, b2_japanesemaple_rt_fall2, n1_tulip_rt_winter_01 und zone/b/tree/c/riverbirch_rt_01/02/03. Die allgemeine Reparatur nutzt inzidente Flächennormalen. Hier existiert keine nichtdegenerierte inzidente Fläche oberhalb der numerischen Toleranz; diese Vertices erhalten endliche kanonische Normalen. Sichtbare Geometrie/Farben bleiben erhalten; keine Asset-Sonderfälle.

## 31. Map Transform

Position einschließlich HeightBias bleibt unverändert. Der echte bisherige Tree-Zweig reicht keine Map-Rotation/Scale weiter, sondern setzt nur x/y/z. H-X bewahrt dieses Verhalten und aktiviert keine bislang ignorierten Mapwinkel. Die neutrale Instanzmatrix und Bounds unterstützen Rotation/Skalierung und sind separat getestet. Unbearbeitete ausgewählte Mapzeilen stehen in den Goldens.

## 32. Reference Trees

Buche, Pagode, Monterey-Zypresse, Baobab, Sassafras, Zimtfarn und Kokospalme. Damit sind geforderte Typen, häufige produktive Vegetation, Fronds und Leaf-/Billboarddarstellung abgedeckt. Die drei Weihnachtsbaumvarianten sind vollständig konvertiert und strukturell geprüft.

## 33. Structural Parity

**118/118 PASS.** Unabhängiger SDK-Probeprozess und separater Python-GLB-Leser vergleichen Mesh-/Vertex-/Indexzahlen, Strip-Dreiecke, alle LODs, Bounds, Textur-/Materialbezüge, ausgewählte Positionen/UVs/Farben, Leaf-Größen und Billboardaktivierung. Der Prüfer nutzt weder SDK noch Runtime-Provider.

Lokale Ergebnisse: build/hx/structural-parity.json und .csv. [phase-hx-reference-goldens.json](phase-hx-reference-goldens.json) enthält kleine Stichproben für sieben Typen, Mapzeilen und deterministische Winddaten; keine vollständigen Meshes oder Texturen.

## 34. Automated Visual Parity

105 Paarvergleiche: sieben Typen × drei Ansichten × nah/mittel/fern/mittel/nah im selben versteckten D3D11-Framebuffer. Zusätzlich Nebel, Kamera-Alpha, Wind, Resize und Nullgrößen-Restore. Vorab festgelegte Grenzen: IoU >0,90; mittlerer RGB-Fehler <8/255; deutlich veränderte Pixel <10 %. Keine nachträgliche Lockerung.

Release: **Minimum-IoU 0,999933; maximaler mittlerer RGB-Fehler 0,006590/255; maximal veränderte Pixel 0,025771 %.** Debug liefert dieselben 105 Messwerte innerhalb der angegebenen Grenzen. TSV und 20 bewusst behaltene 384×384-Nah-/Fern-Referenzbilder der fünf geforderten Baumtypen: build/hx/visual-Release. Redundante Debug- und Zwischenstands-Bilder wurden entfernt; die Debug-TSV bleibt erhalten. Automatische Vorbereitung, keine manuelle Ingame-Abnahme.

## 35. Corpus Conversion

**total=118; converted=118; failed=0.** Ausgabe build/hx/compiled/vegetation: 118 GLBs + 118 ZVEGs + Registry, rund 66,3 MiB. Jeder Export wurde zusätzlich durch den echten Runtime-Loader validiert. Originalquellen/Packs unverändert.

## 36. Production Coverage

**production referenced=85; production referenced unsupported=0.** 82 feste Map-/Property-/Actor-Typen plus drei Event-Typen. Alle 12.819 Platzierungen lösen auf unterstützte Quellen auf. Die zunächst genannten 82 Typen waren der Kartenbestand; der finale Wert schließt hardcodierte Eventreferenzen ein.

## 37. Failure Tests

| Fehlerfall | Nachweis |
|---|---|
| fehlende/beschädigte SPT | Offline-Exit 1, kein Prozessabsturz, keine neue Registry |
| unbekannter Legacy-Key | Fehler ohne Datei-I/O |
| fehlende ZVEG/GLB | klarer gecachter Ladefehler |
| beschädigte GLB/ungültige ZVEG | abgewiesen |
| fehlende Textur | Prepare-Fehler und vollständige Geometriefreigabe |
| falsche LOD-/Part-/Billboard-Zuordnung | abgewiesen |
| falsche Bounds/Traversal/doppelte Schlüssel | abgewiesen |
| unbekannte Billboard-/Kameravarianten | ausdrücklicher Extractorfehler |

Kein neuer Runtime-Fehler ruft SpeedTree auf. Closed-SDK-Ausnahmen enden ausschließlich im separaten Offline-Prozess mit Fehlerexit. Keine Fuzzkampagne.

## 38. Release Build

**Vollständiger Release-Build und Abschlussbuild PASS. 43/43 Fast-Gate-Tests PASS, 58,99 s.** Bisheriger 39er-Gate plus vier H-X-Tests. VS2022/x64, CMake build-f34-clean, vorhandene Abhängigkeitsquellen wiederverwendet. Logs: build/hx/release-build.log, release-finalize.log, release-tests.log. Bestehende PDB-/Vendor-Linkwarnungen bleiben; nicht warnungsfrei.

## 39. Debug Build

**Vollständiger Debug-Build PASS. 43/43 Fast-Gate-Tests PASS, 109,93 s.** Logs: build/hx/debug-build.log und debug-tests.log. Beide Client-EXEs sind AMD64 (0x8664). Debug-SHA256: f1de81a7c952be4a7f195ead47a16d6944a567bc8f995b58cff3f6948706d45a.

## 40. GCC/LP64

**21/21 PASS, 25,57 s** im bestehenden Cygwin/GCC-Common-Pfad, einschließlich Vegetation.Contracts/Boundary. SDK-Converter Windows-only. Kein Android-/Vulkan-Gerätenachweis. Logs: build/hx/gcc-build.log und gcc-tests.log.

## 41. Unit Tests

Registry, atomarer Parser, Bounds, Transformationen, Plane-Culling, LOD/Alpha, Phasen, 1.000 geteilte Instanzen, fehlende Dateien/Texturen, beschädigte Daten, Cached-Failure-Semantik und Freigabe. StartupOptions prüft Reference-Standard, explizites ZiiNAN, Konflikte und ungültige Werte.

## 42. GR2 Regression

Native statische/Animations-Goldens, Reader-Safety/Compatibility/Warmup, Produktions-Consumer, GR2Render, AnimationRuntime und HairLodQuick sind im begrenzten Gate. NoGrannyDependency besteht. GPU-Skinning bleibt Standard; World-Smoke weist CPU-Deformation=0 und GPUFallbacks=0 nach.

## 43. GLB Regression

GlTFProvider, eingebettete Materialien, Blend-/Static-Render, breite Indizes und bestehende FBX/OBJ/DAE-Offline-Roundtrips im Gate. Normale Assets ohne Zusatzkanäle behalten ihren Renderpfad. AssetTool.RuntimeIsolation prüft die Trennung der Offline-Abhängigkeiten.

## 44. World Smoke

**Endgültiger Lauf PASS: A1 → B1 → Trent → A1, 83 s, Exitcode 0.** 706 erzeugte Vegetationsinstanzen, 192 LOD-Zustandswechsel, 171.037 Part-Draws: Branch 43.007, Frond 38.705, Leaf 64.845, Billboard 24.480.

Original-Pack-Harness prüft zusätzlich Gebäude, Props, Camera Blocker, Player/NPC/Mobs/Boss/Mount und GPU-Animation. Belege: build/f2x/runtime/hx-native-final und build/hx/world-native-final.log. Separater Standard-Reference-Kontrolllauf A1 → B1 → A1: **PASS, 63 s, Exitcode 0**, unveränderte Standardauswahl, saubere Ressourcen und 11.431 erfasste Referenzeinstiege. Beleg: build/hx/world-reference-final.log. Die syserr-Dateien beider finalen Läufe sind leer; Renderer-Failure-Logs enthalten keine Fehler.

## 45. Multi-Map Automation

Isolierte eigene Test-Root-Pack, unveränderte Originalpacks per Hardlink, separate kompilierte Vegetationsdaten. Karten laden/entladen und A1-Rückkehr bestanden. Auf A1 zusätzlich ausgeführte Event-Grade-Aufrufe werden korrekt durch die bestehende Snow-Map-Bedingung verworfen; sie sind ausdrücklich **kein** Nachweis gerenderter Weihnachtsbäume. Diese drei Typen sind über Corpus-/Strukturprüfung abgedeckt.

## 46. LOD Distance Automation

Je Karte fünf tatsächliche Kamerastufen: 1000 → 3500 → 6500 → 3500 → 1000. Zusätzlich 105 gezielte SDK-/ZVEG-LOD-Paare. Reference im normalen Client behält den Near-Override; Fernparität wird deshalb im Offline-Referenzharness nachgewiesen.

## 47. Performance Sanity

Keine Mesh-/Texturkopie je Instanz und kein Parsing/Dateizugriff pro Frame. Im kontrollierten Vergleich **1–3 neue Draws** gegenüber bis zu **45 Referenz-Draws**. Jede Probe verlangt neue Drawzahl ≤ Referenzzahl. Kein FPS-/GPU-Zeit-Benchmark; Release-Gate und Debug-Build liefen teilweise gleichzeitig.

## 48. Resource Lifetime

Endgültiger World-Exit 0: VegetationAssets/Instances/RenderAssets/Geometry=0; InstanceBuffers=0; SourceTextures/SourceBuffers=0; AssetDocuments/AnimationInstances/MeshBindings=0; CollisionResources=0; GR2ReaderResources=0; RuntimeSkeletons/AnimationClips=0. VegetationFailures=0, registrierte Reference-Einstiege=0, CPU-Deformation=0, GPUFallbacks=0. GPU-Referenztest prüft zusätzlich die Geometrie-/Texturzähler beider Renderer auf null.

InstanceBuffers=0 bezeichnet hier ausdrücklich das Fehlen eines separaten Instancing-Buffers, nicht einen bereits implementierten Instancing-Pfad.

## 49. Gate A1 Decision

**GO.** Corpus-/Produktionsabdeckung, Offline-Konvertierung, portable Runtime, Struktur- und GPU-Parität, Wind, Fehlersemantik, vollständige Release-/Debug-Builds, beide 43er-Gates, GCC/LP64, finaler World-Smoke und Ressourcenfreigabe sind bestanden. Die isolierte Testkopie ist fertig und mit dem finalen Release-Build identisch. Manuelle Ingame-Sichtprüfung bleibt ausschließlich Gate A2; Gate B bleibt unangetastet.

## 50. Gate A2 Status

**GO.** Nach dem dokumentierten Blatt-Nebel-Fix bestätigt der Benutzer die erneute Sichtprüfung am 15.09.2026 mit „passt“. Die Abnahme bezieht sich auf den A2-Visual-Parity-Fix; nicht einzeln gemeldete Bedienungsprüfungen werden nicht nachträglich als separat bestanden ausgegeben. Gate B bleibt NOT STARTED.

## 51. Historische A1-Testkopie und vorbereitete Prüfliste

Fertige Kopie: build/hx/test-client. **start-ziinan.cmd** wählt den neuen Pfad ausdrücklich; **start-reference.cmd** die Referenz. Kein Build nötig. LIESMICH.txt liegt daneben.

1. Login → Character Select → Ingame.
2. A1/B1/Wald: Bäume vorhanden; Position, Größe, Ausrichtung; Branches/Fronds/Leaves; Texturen/Blattalpha; keine schwarzen/weißen Quads.
3. Kamera nah → mittel → fern → mittel → nah: LOD, Billboard, Wind, Pops, fehlende Typen, Nebel und Kameraverdeckung.
4. Resize → Minimize/Restore → X.

Zum damaligen A1-Abschluss war A2 offen. Die inzwischen bestätigte korrigierte Testkopie liegt unter build/hx/a2-test-client; siehe A2-Fixbericht. Historischer A1-Release-SHA256: e864f95c77c89992ede03d282e69c2105fff24f5bd27fbeb12579d977bbfb1c6. Testkopie und finales Binary stimmen überein; alle 237 kopierten Vegetationsdateien wurden gegen die Ausgabe gehasht. Originalpacks sind Hardlinks, die Konfiguration ist eine eigene Kopie. Die Starter wurden vorbereitet, der interaktive Loginclient wurde heute nicht gestartet.

## 52. Gate B Dependency Removal Plan

Erst nach positivem A2, separat:

- Standard auf native Vegetation umstellen; Reference-Auswahl entfernen.
- CSpeedTreeForest/ForestRenderer/Wrapper, TreeRenderBridge, TreeVertexData und nicht mehr benötigte SDK-Sample-Hilfen aus src/SpeedTreeLib entfernen.
- Referenzzweige, Forest-Member und Includes in WorldTree, Area, Actor, MapOutdoor, MapManager, MapOutdoorLoad/Update/RenderHTP und UserInterface-StdAfx bereinigen.
- ITreeRenderer/DiligentTreeRenderer und Backend-/Bootstrap-Anbindung nach vollständigem Callsite-Audit entfernen; neutrales WorldTree-Interface erhalten.
- src/SpeedTreeLib/CMakeLists.txt, src/CMakeLists.txt, src/UserInterface/CMakeLists.txt, extern/CMakeLists.txt und extern/library/SpeedTree/CMakeLists.txt trennen.
- Produktionslinks auf speedtree_static.lib/speedtree_staticd.lib, SDK-Header und spezifische Wrapper-Defines entfernen.
- Bestehenden SpeedTreeAssetProbe und H-X-Referenzrenderer ausschließlich optional offline behalten oder nach Sicherung der Goldens aus der normalen Testmenge nehmen.
- USE_LOD nicht pauschal löschen: es betrifft weitere bestehende Pfade.

**Keine endgültige Removal-Aktion wurde heute ausgeführt.**

## 53. Clean Build Plan

Nach Gate B frisches build-hx-clean konfigurieren, ohne erreichbare Produktions-SDK-Header/-Bibliotheken, Converter OFF. Release/Debug neu bauen, begrenzten Gate und World-Smoke wiederholen. Source, öffentliche Header, Targetgraph, erzeugte Linkprojekte und Binärimporte auditieren: Production SpeedTree Includes/Types/Calls/Links/SPT-Parsing jeweils null. Kein alter Cache als Ersatznachweis.

## 54. Converter Retention Plan

SPT bleibt Quellformat. Optionaler Windows-Converter mit separat verfügbar gemachtem SDK. Dessen Pfad wird in Gate B ausschließlich an das Offline-Target gebunden. Produktion muss auch ohne erreichbares SDK konfigurieren/bauen. Neutrale Goldens, Registry-/ZVEG-/Runtime-Tests bleiben dauerhaft nutzbar.

## 55. Known Limitations

Kein Instancing-Batching, PBR, neue Assets, Gras oder Android/Vulkan. Kleine eigene Windfunktion statt exakter zeitlicher SDK-Reproduktion. LOD-Schwellen mit 1/256-Auflösung; der optionale neue Pfad aktiviert LOD/Billboard gegenüber dem bisherigen Near-Override. Mehrfach-/Horizontalbillboards und unrepräsentierbare Kamerareaktionen werden abgewiesen, kommen aber im Corpus nicht vor.

**SDK bleibt im Gate-A-Clientbinary gelinkt**, damit Reference verfügbar bleibt. Die neue Runtime verwendet es nicht. Der Zähler erfasst geprüfte Integrations-Einstiege und ist zusammen mit Source-/Targetaudit zu lesen; er ist kein systemweiter Profiler geschlossener Bibliotheksinitialisierungen. Bestehende LNK4099/LNK4098, DumpProto-Formatwarnungen, MarkManager-nodiscard-Warnungen und Vendor-Makrowarnungen bleiben erhalten. Diese Dateien wurden durch H-X nicht geändert; ein warnungsfreier Build wird nicht behauptet.

## 56. Git Diff

Neue Runtime, WorldTree-Adapter, optionaler Converter, unabhängige Corpus-/Bild-/Fehlertests und Bericht. Vorhandene AssetRuntime-/GLB-/Meshrenderer-Strukturen erhielten optionale gemeinsame Vertexkanäle. Kein Ersatz/Zurücksetzen von GR2, Animation oder GPU-Skinning. Keine Originalassets geändert. 24 bestehende Dateien geändert, 26 neue Source-/Test-/Berichtsdateien. Git diff --check bestanden; Index leer. Kein Commit/Push; nichts gestaged. GLB/ZVEG, Logs, PDB, Captures und Testclient liegen in ignorierten Build-Verzeichnissen.

## 57. GO/NO-GO Status

H-X Gate A1: GO  
H-X Gate A2: GO – Benutzerbestätigung „passt“ vom 15.09.2026  
Gate B: NOT STARTED  
Test client: READY

## 58. Nächster Schritt

A2 ist bestätigt. Gate B bleibt ein separater, noch nicht gestarteter Arbeitsschritt und benötigt einen eigenen Auftrag. Referenzpfad und Captures bleiben verfügbar; kein automatischer Produktionswechsel.

### Reproduzierbare lokale Befehle

Aus m2dev-client-src; den vorhandenen Python-Interpreter der lokalen Umgebung verwenden.

~~~powershell
cmake -S . -B build-f34-clean -DZIINAN_BUILD_LEGACY_SPT_CONVERTER=ON -DM2_BUILD_ASSET_TOOL=ON
cmake --build build-f34-clean --config Release --parallel 6
cmake --build build-f34-clean --config Debug --parallel 6
build-f34-clean/tools/Vegetation/Release/ZiiNANVegetationTool.exe batch ../m2dev-client/assets build/hx/compiled
python3 tools/Vegetation/audit_corpus.py --client ../m2dev-client --source . --probe build-f34-clean/tools/Vegetation/Release/ZiiNANSPTProbe.exe --output build/hx/audit --pack-audit build/hx/audit/packed.json
python3 tools/Vegetation/verify_corpus.py --audit build/hx/audit/corpus.json --compiled build/hx/compiled --output build/hx/structural-parity.json
ctest --test-dir build-f34-clean -C Release -R '^(Platform\.|AssetRuntime\.|AnimationRuntime\.|AssetTool\.|Vegetation\.|Renderer\.(StartupOptions|DiligentD3D11|HairLodQuick)$)' --output-on-failure
ctest --test-dir build-f34-clean -C Debug -R '^(Platform\.|AssetRuntime\.|AnimationRuntime\.|AssetTool\.|Vegetation\.|Renderer\.(StartupOptions|DiligentD3D11|HairLodQuick)$)' --output-on-failure
cmake --build build-c3x/cygwin-common --parallel 6
ctest --test-dir build-c3x/cygwin-common --output-on-failure
tests/AssetRuntime/run_gr2_smoke.ps1 -Name hx-native-repeat -BuildDirectory build-f34-clean -ProductionDefault -MultiMap -Vegetation ziinan -VegetationAssets build/hx/compiled -VegetationForest
tools/Vegetation/prepare_test_client.ps1 -OutputDirectory build/hx/test-client-repeat
~~~

Frischen Packaudit zuerst mit audit_corpus.py ohne --pack-audit vorbereiten, dann VegetationPackAudit.exe mit Clientverzeichnis, pack-order.txt und Ziel packed.json ausführen; danach Referenz-/Strukturprüfung. Der gesamte Gate enthält keinen Vendor-Fuzzer, keinen Stresslauf und keine vollständige Renderer-Gesamtsuite.
