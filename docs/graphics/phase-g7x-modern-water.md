# G7-X — Modern Water

Datum: 2026-09-15. Windows x64 / Diligent D3D11. Abschlussstatus: **G7-X = GO**.

## 1. Start State

Source: `codex/g56-hdr-atmosphere`, HEAD `6f544d4` (`feat(graphics): add HDR and atmospheric rendering`), anfangs clean. Die freigegebenen G-DX/G-DX-C-, C-LIB-X- und G5/6-Berichte wurden als Baseline geprüft.

Assets: `codex/g56-hdr-colors`. Die beiden offenen Menüdateien entsprachen dem archivierten, freigegebenen G5/6-Patch nach Vergleich mit vollständigen Git-Index-Hashes. Ausschließlich dieser Altstand wurde im ausdrücklich erlaubten Checkpoint `40445c57` (`feat(graphics): finalize DiligentFX renderer and HDR atmosphere`) gesichert. Anschließend beide Trees clean. Kein Push, kein G7-Commit.

Die unveränderte Baseline-EXE liegt unter `build/g7x/baseline/Metin2_Release.exe`, SHA256 `bb3cfb6d5a9619df2f93aae51f2ed8474377b831e0fa3d5576357b1c2d535434`.

## 2. Existing Water Audit

`GameLib/MapOutdoorWater.cpp` lädt 30 lokale DDS-Bilder (`special/water/01.dds` bis `30.dds`), wählt sie über Millisekunden und erzeugt kamerabezogene Texturkoordinaten. Die Dateien sind alte Oberflächenbilder, keine geeigneten signierten Normalmaps. Geometrie und Alpha entstehen aus dem bestehenden `water.wtr`, seinen Höhen-Layern und der Terrainhöhe in `AreaTerrain.cpp`. Die native Höhenbewegung ist bereits zeitbasiert und bleibt erhalten.

Classic verwendet weiterhin die bisherigen Water-Dreiecke, Texturen, Vertex-Alpha, deaktiviertes Z-Write und den vorhandenen Effect-Renderer. Modern übernimmt Platzierung, Höhen, World-Transform und Alpha, ersetzt aber die Oberflächenschattierung. Es gibt keine Map-Konvertierung und keine Produktions-Sonderfälle für bestimmte Map-Namen.

## 3. Architecture

`Graphics/WaterConfig.h` enthält neutrale Material- und Qualitätssemantik. `DiligentModernRenderer` besitzt genau einen `DiligentWater`; dieser besitzt seine gemeinsamen GPU-Ressourcen. `WorldRenderBridge` reicht native Water-Geometrie an diesen speziellen Materialpfad weiter und vermeidet dabei redundante Legacy-Textur-Uploads in den World-Cache. Die vorhandenen Map-Texturen bleiben für unmittelbares Classic-Umschalten verfügbar.

Wasser verwendet einen gemeinsamen Normal-/Coverage-Prepass und einen HDR-Composite. Wasser ist kein GR2-PBR-Material. AssetRuntime, Skeletons, Animationen und Vegetation erhalten keine zweite Engine.

## 4. Diligent/DiligentFX Reuse

Verwendet wird der vorhandene FX-Pin `cb380ac52100672b5762f595acfb6609e0ecc248`, Archiv-SHA256 `7117a1d0067ef0c36315900647904116234cf55cadc11682f6f6e70064658236`.

Wiederverwendet: `ScreenSpaceReflection`, `PostFXContext`, dessen numerischer Depth-Copy, Depth-/NDC-Helfer, PBR-Schlick/GGX, Shadow-Filter, Atmosphere-Sky, Texturen, PSOs und Bindings. `PrepareDiligentFX.py` erzeugt das reproduzierbare minimale Subset, entfernt ImGui und ergänzt Bereitschafts-/Diagnosezugriffe sowie einen kleinen expliziten spatial-only History-Bypass. Kein eigener SSR-Raymarcher; keine Änderung am upstream Intersection-Algorithmus.

## 5. Render Order

Opaque World/G-Buffer → vorhandene Shadow-/AO-/Sky-HDR-Komposition → bestehende vorwärts gerenderte transparente Meshes → Water-Normal-/Depth-Prepass → optional FX SSR → HDR-Water-Composite → Weapon Traces, Snow, Map-/Manager-Effekte, Particles, Items/Flying/Blocker → Bloom/Tone Mapping → Damage/Nameplates/UI.

Die Engine rendert ihre transparenten Mesh-Submissions bereits vor Water. Deshalb ist die kopierte Farbe präzise die *vor Water zusammengesetzte Szene*, während Refraction die opake Tiefe benutzt. G7 baut die allgemeine Transparenzsortierung nicht um. Weapon Traces wurden gezielt hinter `FinishWater()` verschoben, damit Wasser sie nicht als Untergrund behandelt.

## 6. HDR Integration

Scene-Copy und Water-Composite verwenden RGBA16F vor Bloom und Tone Mapping. Kein Clamp auf 1. Der explizite GPU-Test liest die tatsächliche FP16-Textur: opakes Maximum **0,842773**, Wassermaximum **19,5938**, alle Werte endlich. Der diagnostische Readback ist im normalen Client deaktiviert.

## 7. Shared Sun

Water erhält dieselbe validierte `SceneLighting` wie World, Schatten und Atmosphäre. Richtung, Farbe und Intensität werden direkt übernommen; keine zusätzliche Wasser-Sonne und keine Bildschirmposition als Highlight-Quelle.

## 8. Sun Specular

Diligent `ApplyDirectionalLightGGX` arbeitet mit World-Normale, View-Vektor und gemeinsamem Sun-Vektor. Diffuse Water-Reflektanz ist null, F0 entspricht Wasser. Die GPU-Sonnenvarianten und nativen Morning/Noon/Evening-Bilder ändern Lage und Form des Reflexes gemeinsam mit dem Sonnenstand.

## 9. Sun Glint

Das Glitzern entsteht aus den bewegten Normalen im gerichteten GGX-Licht. Es gibt keinen separaten Sprite-/Screen-Glanz. Die Prüfbilder zeigen lokal helle Streifen/Punkte; die gesamte Wasserfläche bleibt durchzeichnet. Bewegung wird im GPU-Test zwischen 10 und 12 Sekunden nachgewiesen.

## 10. Water Normals

Eine deterministische, periodische 128×128-RGBA8-Normaltextur mit acht Mips wird einmal erzeugt. Quelle sind analytische Ableitungen von vier periodischen Höhenanteilen; Frequenzen werden in kleinen Mips reduziert. Keine Downloads oder unbekannten Assets. Zwei Samples teilen dieselbe Textur und nutzen unterschiedliche Skalen, Richtungen und Geschwindigkeiten. Low verwendet ein Sample.

## 11. Time-based Movement

Production nutzt `steady_clock`, keine Frame-Zähler. Die UV-Verschiebung wird pro Layer modulo einer Periode berechnet. Der portable Test integriert zehn Sekunden bei 60 und 120 FPS und bestätigt gleiche Offsets. Nicht endliche Zeitwerte liefern sichere Null-Offsets. Die feste Zeit/der feste FX-Seed sind ausschließlich explizite Diagnoseeinstellungen.

## 12. Fresnel

Schlick mit Wasser-IOR 1,333: `F0 = ((1,333−1)/(1,333+1))² = 0,02037318784`. Die GPU nutzt den Diligent-Helfer, der neutrale Test prüft Endpunkte und monoton stärkere Reflexion bei flachem Blick. Steile Sicht bewahrt den sichtbaren Untergrund, flache Sicht zeigt mehr Himmel/SSR.

## 13. Roughness

Zentraler Materialdefault **0,16**, validierter Bereich 0,07–0,5. Normalstärke 0,14. Roughness steuert GGX und die an FX übergebene Water-Maske. Werte sind in `WaterMaterial` zusammengefasst; keine Map-spezifische Abstimmung.

## 14. Sky Reflection

Der World-Reflexionsvektor tastet die bestehende G5/6-Atmosphere-Sky-Textur ab. Die geringe Horizon-Beimischung verwendet dieselben Environment-Farben wie die bestehende Sky-Komposition. Kein zweites Himmelsmodell, keine Welt-Fog-Strecke. Bei fehlenden SSR-Treffern bleibt diese Spiegelung erhalten.

## 15. DiligentFX SSR

FX erhält vor Water kopierte HDR-Farbe, kombinierte opake/Water-Normals, Roughness und kombinierte Tiefe. Die opake Tiefe bleibt separat für Refraction und spätere Transparenz bestehen.

Ein gefundener Integrationsfehler wurde behoben: FXs D3D-CopyDepthToColor-Schnellpfad setzt Float-Depth voraus; natives D24 führte zu ausbleibenden Treffern. Die bestehende `PostFXContext::CopyTextureDepth`-Operation konvertiert jetzt D24 numerisch nach R32F vor der FX-Tiefenhierarchie. Radiance-/Confidence-Bilder und messbare SSR-On/Off-Differenzen belegen echte Treffer. Temporäre Ray-Debug-Experimente sind aus dem Produktionsalgorithmus entfernt.

## 16. SSR Quality

| Qualität | Normals | Refraction/Absorption | SSR |
|---|---:|---|---|
| Low | 1 Sample | aus | aus |
| Medium | 2 Samples | an | aus |
| High | 2 Samples | an | halbe Auflösung, maximal 64 Traversal-Intersections |
| Ultra | 2 Samples | an | volle Auflösung, maximal 128 Traversal-Intersections |

High/Ultra benutzen dieselbe Kerntechnik. Der GPU-Test verlangt einen tatsächlichen Bildunterschied; Ultra ist kein bloßer UI-Wert.

## 17. SSR Limitations

Offscreen-Objekte und verdeckte Flächen sind nicht verfügbar. An Bildschirmrändern und bei fehlenden Treffern sinkt die Confidence. Dünne, alpha-maskierte und bewegte Details können räumlich unruhig spiegeln. Es gibt weder Planar-Reflection noch Raytracing als zusätzliche Technik.

## 18. SSR/Sky Composite

`lerp(sky, ssr.rgb, saturate(ssr.a))`; anschließend steuert Wasser-Fresnel den Gesamtreflexionsanteil. Null-Confidence ergibt Himmel. Schwarze Bereiche in ausdrücklich beschrifteten SSR-Diagnosebildern bedeuten fehlende Treffer und sind keine schwarzen Wasserflächen im Produktionsbild.

## 19. SSR History

SSR verwendet aktuelle SceneColor und räumliche Rekonstruktion. Beide temporalen Stabilitätsfaktoren stehen auf null. Der kleine FX-Adapter liest in diesem Modus gar keine frühere Radiance/Variance; dadurch können auch NaNs aus unbrauchbarer History nicht über ein mathematisches `lerp(...,0)` einfließen.

Map-/Character-Wechsel und Teleport benötigen daher keine inhaltliche SSR-History. Resize und Style-Wechsel geben die FX-Ressourcen frei; die aktuelle Szene wird erneut kopiert. Entfernte Testobjekte hinterlassen keine dauerhafte Spur.

## 20. TAA Decision

Kein Production-TAA. Die vorhandenen Motion-/Previous-State-Daten reichen für eine allgemeine temporale Lösung nicht aus, und FX SSR funktioniert räumlich. Die verbleibende räumliche Unruhe ist dokumentiert.

## 21. Refraction

Medium bis Ultra verschieben SceneColor anhand der Water-Normale um maximal den zentralen Default von drei Pixeln, mit zusätzlicher Shore-Abschwächung. Low bewahrt den unverschobenen Untergrund. Keine zweite Szene wird gerendert.

## 22. Depth-aware Refraction

Der versetzte Kandidat muss hinter der Wasseroberfläche liegen und innerhalb der sichtbaren Wasser-Coverage bleiben. Andernfalls wird die unversetzte Farbe genommen. Der GPU-Proof und die nativen Ufer-/Actor-Bilder zeigen keine über trockene Silhouetten gezogenen Farbbänder. Allgemeine sortierte Alpha-Transparenz bleibt eine Grenze des vorhandenen Depth-Buffers.

## 23. Water Depth / Absorption

Aus rekonstruierter World-Position von Wasser und Untergrund wird die vertikale Wassersäule ermittelt, auf 2500 cm begrenzt. Beer-Lambert-Transmittanz mit RGB-Koeffizienten `(0,0018; 0,00075; 0,00045)` pro cm mischt dezent zur zentralen Tiefenfarbe. Das ist eine einfache vertikale Tiefenapproximation, keine Unterwassersimulation. Die geneigte GPU-Bodenfläche und das native Flachufer zeigen flache/tiefe Bereiche.

## 24. No World Fog

Außerhalb der Water-Coverage verwirft der Composite das Fragment. Im GPU-Test bleiben die oberen 80 Bildzeilen mit Himmel/trockenen Objekten bytegleich zur Szene ohne Wasser. Kein distanzabhängiger grauer World-Fog wurde eingeführt; das G5/6-Farbdesign bleibt erhalten.

## 25. Shore Handling

Ein `smoothstep` über standardmäßig 35 cm Wassertiefe multipliziert die vorhandene native Vertex-Coverage. Wasser bleibt am flachen Ufer transparent; keine Foam-Simulation. Das alte Terrain-Water-Raster begrenzt weiterhin die Geometrieauflösung.

## 26. Water Shadows

Water nutzt die vorhandenen Cascades, Bias-Daten und Diligent-Shadow-Filter. Nur der direkte Sonnen-Specular wird mit dem Shadow-Faktor multipliziert; Himmel/SSR werden nicht pauschal verdunkelt. Native Gebäude am Wasser liefern vorhandene Schatten. Keine neue Shadow-Infrastruktur und kein gesonderter dynamischer Brücken-Test.

## 27. Existing Map Compatibility

Originale B1- und A1-Daten laden ohne Migration. Die Fixture wechselt B1 → A1 → B1 und erstellt Original-Actors neu. Alle Änderungen an Kameras/Actor-Positionen liegen ausschließlich in der isolierten Test-Fixture. Production enthält keine B1/A1-Hardcodes. Materialdefaults gelten für vorhandene Maps.

## 28. GraphicsSettings

`WaterQuality` erhält Ultra als Wert 3; vorhandene Werte 0–2 bleiben stabil. Laden, Validieren, Speichern und Runtime-Auflösung unterstützen alle vier Stufen. Das vorhandene Grafikmenü erhält ausschließlich die angeforderte Wasserzeile mit Niedrig/Mittel/Hoch/Ultra und unmittelbarem Apply.

## 29. Presets

Modern Low/Medium/High/Ultra lösen auf die gleichnamige Wasserqualität auf. Classic hält sein bisheriges effektives Water-Verhalten und 70-ms-Texturintervall unabhängig vom gespeicherten Modern-Wert. Die Preset- und Persistenzfälle werden unter MSVC und GCC geprüft.

## 30. Live Apply

Die native Serie schaltet alle vier Qualitäten ohne Prozessneustart. Beim Abschalten von SSR wird dessen Owner freigegeben; beim Einschalten wird er vorbereitet. High/Ultra ändern die echte FX-Ressourcenkonfiguration. Der GPU-Test führt zusätzlich zweimal Classic → Modern aus und verlangt zwischen den Durchläufen `liveWaterRenderers=0`.

## 31. Transparency

Production-Water komponiert transmissive SceneColor in HDR und lässt die ursprüngliche opake Tiefe für spätere Effekte bestehen. Die privaten Water-Depth-/Normal-Ziele werden nicht als globale World-Tiefe weitergereicht. Vegetation-Alpha bleibt im vorhandenen Opaque/Mask-Pfad. UI liegt nach Tone Mapping. Die Engine-weite Reihenfolge transparenter Meshes bleibt die in Abschnitt 5 genannte Einschränkung.

## 32. Terrain Intersection

Top-down, flache Sicht und nähere Uferkamera wurden geprüft. Keine invertierten Normalen, flächigen schwarzen/weißen Wasserfehler oder offensichtlichen künstlichen Uferkanten in den geprüften Ansichten. Sehr flaches Wasser lässt bewusst viel Terrainfarbe durch.

## 33. Actor Intersection

Original-Player, NPC, Mob, Boss, Mount und Reiter sind in der nativen Fixture vorhanden. Die Zusatzkamera positioniert den Player am flachen Wasser. Der farbige GPU-Actor-Probe ragt gezielt durch die Wasserhöhe und wird verschoben/entfernt. Depth-Rejection und Refraction bleiben plausibel; kein vollständiger Unterwasser-Gameplay-Test.

## 34. Vegetation Reflection

Native ZiiNAN-Vegetation bleibt sichtbar; die SSR-Confidence am Ufer zeigt Treffer, wo die aktuelle Bildschirmgeometrie erreichbar ist. Im nativen Composite sind dunkle Vegetationsanteile abhängig von Uferposition/Fresnel subtil, kein sauberer Vollbaum-Spiegel. Der separat beschriftete grüne GPU-Probe isoliert die Reflexion und belegt den Material-/Normal-/Depth-Pfad. Diesen künstlichen Probe nicht mit einem Originalbaum verwechseln.

## 35. Building Reflection

Feste native SSR-Off/High/Ultra-Nahansichten 18–20 zeigen das Gebäude auf Pfosten am Originalufer; Ansicht 22 zeigt zugehörige Treffermasken unter dem Gebäude. Zusätzlich isoliert der rote GPU-Probe den SSR-On/Off-Effekt ohne bewegte Vegetation/Map-Höhenphase. Die Galerie trennt Produktionsbild und Diagnosemaske.

## 36. Actor Reflection

Native Idle-/Walk-Ansichten und die Confidence unter dem Player ergänzen den gezielten blauen GPU-Probe. Nach dessen Verschiebung folgt die Reflexion der aktuellen Position; nach Entfernen aller Probes entspricht die Folgeaufnahme wieder der leeren Szene. Feste Modern-Bilder erlauben höchstens eine 8-Bit-Rundungsstufe in maximal 0,1 % der Kanäle. Das ist keine Classic-Toleranz; Classic bleibt strikt bytegleich.

## 37. Sun Consistency

Native Ansichten 4/5/6 setzen Morning-/Noon-/Evening-Sun über den vorhandenen Diagnosezugriff auf `SceneLighting`. G5/6-Sun-/HDR-Tests prüfen weiterhin sichtbare Sonne und Beleuchtung. Water-GPU-Bilder zeigen die wandernde Reaktion. Keine einzelne native Kamera enthält zugleich jede Sun-Disk-, Shadow- und Water-Konstellation; die gemeinsame Datenquelle ist zusätzlich im Code nachvollzogen.

## 38. HDR/Bloom

Native Ansichten 7/8 sowie der GPU-Bloom-Proof vergleichen Bloom aus/ein. Die HDR-Wasserhighlights bleiben lokal; keine vollständig weiße Wasserfläche. Der tatsächliche Float-Peak von 19,5938 beweist die HDR-Energie unabhängig vom tonemapped Screenshot.

## 39. Color Preservation

Die nativen Bilder bewahren Terrain-, Gebäude-, Armor- und Vegetationsfarben. Refraction erhält SceneColor statt eines grauen Overlays. Der bitgenaue trockene GPU-Bildbereich und die weiter grünen G5/6-Tests ergänzen die Sichtprüfung.

## 40. Performance

Messung: gespeicherte G5/6-EXE gegen finale G7-Release-EXE, getrennte Prozesse ohne gleichzeitige Builds/GPU-Tests. Gleiche Original-B1-Szene, 1024×768, Kamera `(5000,55,40)`, sechs Actors, Shadows=High (4), AO=Low (1), Vegetation=High (2), Sichtweite 25600, ModernSky an, Bloom aus. Pro Stufe 303 warme Samples ab Sekunde 3. Die feste Kamera enthält ein flaches Ufer, keinen maximalen Vollbild-Ozean.

CPU ist Process-Zeit ohne Present/Framelimiter; GPU ist der vorhandene asynchrone BeginFrame→EndFrame-Duration-Query. Wall-Frame bleibt etwa 16,5 ms durch den nativen Takt. Ergebnisse sind eine kurze lokale Sanity-Messung, kein Hardware-unabhängiger Leistungsnachweis. Details: [performance.json](../../build/g7x/performance.json) und [Performance Sanity](../../build/g7x/performance.md).

## 41. Quality Performance

| Stufe | G5/6 CPU Median/p95 ms | G7 CPU Median/p95 ms | G5/6 GPU Median/p95 ms | G7 GPU Median/p95 ms |
|---|---:|---:|---:|---:|
| Low | 0,934 / 1,378 | 0,900 / 1,388 | 0,325 / 0,584 | 0,382 / 0,950 |
| Medium | 0,960 / 1,186 | 0,942 / 1,195 | 0,326 / 0,881 | 0,384 / 0,957 |
| High | 0,960 / 1,336 | 0,961 / 1,266 | 0,326 / 0,388 | 0,475 / 1,192 |
| Ultra | 0,987 / 1,423 | 0,962 / 1,251 | 0,328 / 0,947 | 0,471 / 1,150 |

High/Ultra kosten hier etwa 0,09 ms GPU zusätzlich zu Medium. Ihr kleiner gegenseitiger Zeitunterschied liegt innerhalb der Messstreuung; Ultra erhält trotzdem real mehr Samples/Auflösung. Bei höheren Auflösungen/mehr Wasser kann SSR stärker kosten und ist über Low/Medium abschaltbar.

Die 18-Ansichten-Serie meldet 111,518 ms Water-CPU-Submission über 8560 Water-Frames (~0,013 ms/Frame) und 1900,96 ms SSR-Submission über 6294 SSR-Frames (~0,302 ms inklusive Vorbereitung). Refraction ist Teil des Composite und nicht separat gemessen. Copies vor dem Prepass sind nicht im Water-CPU-Zähler enthalten. Es gibt keine isolierten GPU-Water-/SSR-Timer. Die 36 CSV-Draws sind Actor-Draws, kein globaler Draw-Count; globale Teilzähler stehen im Rendererlog.

## 42. First Use

Gemeinsame Water-Textur/PSOs/Targets entstehen bei Modern-Prepare. `FinishWater()` führt den SSR-Pfad bei Initialisierung/Loading auch ohne sichtbares Wasser einmal aus. Qualitäten mit neuem SSR und Resize dürfen neue Targets/PSOs vorbereiten; gewöhnliche Folgeframes nicht.

Die vergleichbare Startup-Sequenz erreicht maximal 1538,624 ms Process-Zeit bei G5/6 und 1737,812 ms bei G7 (+199,189 ms, etwa 13 %). Das ist Start inklusive Map/Modern-Initialisierung, nicht die isolierte historische ~0,96-s-Style-Reinitialisierung. Neue SSR-Vorbereitung lag in der Serie ungefähr bei 172–181 ms. Live-Qualitätswechsel/Resize können diesen kurzen Vorbereitungsstall zeigen. Kein Anspruch auf hitch-freie Initialisierung; kein zusätzlicher späterer First-Water-Resource-Aufbau im vorgewärmten Modus.

## 43. Resource Sharing

Ein Water-Owner, eine mipmapped Normalmap, drei Water-PSOs, ein dynamischer Vertexstream für 32766 Vertices und gemeinsame Targets pro Modern-Renderer. Dreiecke werden bei Bedarf in begrenzten Chunks über denselben Stream übertragen. Keine identischen Ressourcen pro Wasserfläche.

Eigene flächenabhängige Ziele: HDR-Copy 8 B/Pixel, D24/S8 4, numerisches R32-Depth 4, Water-Normals 8, kombinierte Scene-Normals 8, Roughness 1 = **33 B/Pixel**. Der gemeldete Zähler einschließlich Normalmips und Stream ist bei 1024×768 **26.826.020 Bytes (~25,58 MiB)**. FX-interne SSR/PostFX-Ziele, PSO-/Treiberkosten und kleine Konstanten sind darin nicht vollständig enthalten; kein vollständiges VRAM-Budget behauptet.

## 44. Resize

GPU-Test: 640×480 → 320×240 → Suspend → 640×480 mit SSR. Native Fensterprüfung in Ansicht 11 ergänzt tatsächliche Resize-/OriginalSize-Ereignisse. Window-Release löst SRB-Eingaben und SSR-/Scene-/Depth-Ziele; neue Größen erzeugen passende Views.

## 45. Minimize/Restore

Native `TestGraphicsWindow()` bestätigt Minimize, Restore, Resize und OriginalSize. Zero-Size hält den Backend-Frame an; nach Restore sind World und Water wieder sichtbar. Die Serie beendet sich mit Exitcode 0 und freigegebenen Ressourcen. Fensterprüfung und erneute Aufnahme sind autonome Harness-Belege, keine persönliche Nutzerabnahme.

## 46. Map Change

Die 18-Ansichten-Serie lädt B1 mehrfach, A1 und B1 erneut. Original-Actors werden zerstört/neu erzeugt. Die kopierte Szene und Water-Maske stammen aus dem aktuellen Frame; es bleiben keine Asset-/Skeleton-/Vegetation-Owner am Exit erhalten. Der räumliche SSR-Modus trägt keine alte Map-Radiance weiter.

## 47. Relog

**Realer Server-Login → Character Select → Relog: NOT RUN.** Der verfügbare isolierte World-Harness startet ohne Netzwerk-Login und erlaubt keine belastbare Aussage über diesen vollständigen Ablauf. Er deckt Map-/Character-Neuaufbau ab; der Settings-Harness zusätzlich einen zweiten Clientprozess und Persistenz. Die ausdrücklich erlaubte Harness-Grenze bleibt sichtbar und wird nicht als ausgeführter Relog ausgegeben.

## 48. Diligent Diagnostics

Native finale Serie und Zusatzserie: `DiligentErrors=0`, `DiligentFatals=0`; keine Water-/SSR-Fallbacks und keine Warn-/Binding-Diagnosedatei. `renderer-failure.log` enthält nur den Aktivierungshinweis. Die GPU-Tests prüfen ERROR/FATAL über den Callback. Finaler Debug-Gate: **66/66 PASS**, 203,10 s.

## 49. GPU Skinning Regression

Native Original-Actor-Serie: `AllCPUDeformationCalls=0`, `AllCPUDeformationVertices=0`, `GPUFallbacks=0`, `SkinPreparationFailures=0`. Der vorhandene Production-GPU-Test bleibt im FAST Gate. G7 verändert weder Paletten noch Skinning-Auswahl.

## 50. GR2 Regression

Finale native Serie: 948 native GR2-Lesevorgänge, `GrannyFileReads=0`, 301072 unabhängige Pose-Samples, `AnimationRuntimeFailures=0`. Player/Mob/Boss/NPC/Mount/Reiter und Original-Armor/Hair/Weapon sind in den Bildern. Die bestehenden GR2-Reader-/Animation-/Renderprüfungen sind Teil der beiden Gates.

## 51. GLB Regression

Die bestehenden neutralen GLB-Provider-, Character- und Classic/Modern-GPU-Tests bleiben im Gate. Nativer F5-X-Smoke mit 20 animierten GLB-Actors, neun Stufen und HDR/Atmosphere: **PASS**, neun Aufnahmen, 863 Frames, Exitcode 0; Diligent ERROR/FATAL, CPU Deformation, GPU Fallbacks und Shutdown-Owner null. [Native GLB-Log](../../build/g7x/native-glb-final.log). Kein Eingriff in Importer, Loader-Typen oder RuntimeSkeleton/RuntimeAnimationClip.

## 52. Vegetation Regression

Finale native Serie: 553825 Vegetation-Draws, 343 LOD-Wechsel; Branches/Fronds/Leaves/Billboards verwendet. `VegetationFailures=0`, alle Asset-/Instance-/Geometry-Zähler am Exit null. Vorhandene GPU-Goldens und NoLegacyDependency-Test bleiben im Gate. Keine SpeedTree-Rückkehr, keine neuen Vegetationsassets, kein H2.

## 53. G5/6 Regression

HDR/Atmosphere-Konfiguration, HDR-GPU-Proof, Material-/Farbmapping, Legacy Armor Aura, authored Shimmer, UI-Separation, Shadows/AO und Lifecycle-Prüfungen bleiben im unverändert übernommenen Gate. Native Bloom-/Sun-/Farbbilder ergänzen sie. Das Menü-Smoke-Ergebnis ist **PASS**: 14 Schritte im ersten Prozess und zwei Schritte nach Neustart, beide Exitcode 0; Fensterereignisse bestätigt, Fehlerlogs leer, Ressourcen null. [Native Settings-Log](../../build/g7x/native-settings-final.log). Keine globale Visual-Neuabstimmung.

## 54. Classic Goldens

Release **12/12 SHA256-identisch** zum freigegebenen G5/6-Stand: drei Market-Stall-Kameras, 20 GLB-Actors, sechs GLB-Clips, Hand-Attachment und Window-Restore. Debug: CLASSIC_**66/66 PASS**, 203,10 s. Die Referenzdateien stammen aus den tatsächlichen AssetRuntime-Testausgaben; keine veralteten Kopien anderer Testverzeichnisse.

Belege: [Release-Hashes](../../build/g7x/classic-Release.json), [Debug-Hashes](../../build/g7x/classic-Debug.json). Keine Pixel-Toleranz für diese zwölf Goldens.

## 55. Visual Gallery

[Lokale Galerie](../../build/g7x/visual-gallery.html), [SHA256-Manifest](../../build/g7x/visual-manifest.json). 18 native Hauptansichten plus fünf nähere Uferansichten, G5/6-Vergleich und deterministische GPU-Probes. Die Galerie enthält SSR aus/ein, Sonne, Morning/Noon/Evening, Gebäude/Player/Vegetation, Shore/Refraction, flach/tief, Bloom aus/ein und alle Qualitäten. Diagnosebilder sind separat beschriftet.

Originale native JPEG-Aufnahmen bleiben unverändert. GPU-PPM-Bilder werden verlustfrei nach PNG übertragen. Keine generierten Ersatzbilder, kein Retuschieren und keine externen Assets.

## 56. Automated Visual Review

**Automated/agent visual review completed; personal user visual review deferred.**

Eigene Prüfung der nativen Kontaktbögen, Ufer-Nahbilder und GPU-Proofs: keine offensichtlichen schwarzen/weißen Produktions-Wasserflächen, invertierten Normalen, dauerhaften Geisterbilder, groben Ufer-/Silhouettenfehler oder UI-PostFX-Verfärbungen. SSR bleibt räumlich körnig und auf sichtbare Informationen begrenzt. Gebäude-/Actor-/Vegetationsanteile werden im nativen Bild durch Fresnel bewusst subtil; die isolierten Probes liefern den eindeutigen technischen Nachweis. Persönliche Nutzerabnahme ist für diesen autorisierten Nachtlauf keine GO-Voraussetzung.

## 57. Release

Vollständiger finaler Release-Build PASS, danach **66/66 PASS**, 116,05 s. Gate = bestehende 64 G5/6-Prüfungen plus `Graphics.WaterConfig` und `Graphics.ModernWaterGpu`; exakte Liste in `tests/Graphics/g7-fast-tests.txt`. Keine Vendor-Fuzzer, kein Stresslauf. [Build](../../build/g7x/release-build-final.log), [Tests](../../build/g7x/release-tests-final.log).

Finale EXE-SHA256: `22315abffc67b84d69a7fa3344f7c36fed83f0fa0daebf2c0bae5c4fa8ffffa0`. Native finale Serien verwenden genau diese EXE.

## 58. Debug

Vollständiger finaler Debug-Build PASS; FAST Gate **66/66 PASS**, 203,10 s. [Build](../../build/g7x/debug-build-final.log), [Tests](../../build/g7x/debug-tests-final.log). EXE-SHA256: `0401caf04f8aeccd0aef9aa155c1e889dfa2790b02c9d886e29485fa72ce4fce`.

Die vorhandenen Linkerwarnungen, insbesondere LNK4099 für fehlende Drittanbieter-PDBs sowie bekannte LNK4098/LNK4075-Konstellationen, werden nicht als G7-freier Warnungsstand dargestellt. Der zunächst eingeschränkte SDK-Zugriff wurde durch den vorhandenen autorisierten Visual-Studio-Buildkontext gelöst; kein Produktfehler.

## 59. GCC/LP64

**35/35 PASS**, 6,20 s, Cygwin GCC/LP64 im bestehenden `build-hx-common`. Das ist der 34er portable Baseline-Gate plus WaterConfig. Keine GPU-SSR-Simulation im rendererlosen Harness. [Build](../../build/g7x/gcc-build.log), [Tests](../../build/g7x/gcc-tests.log). Nach diesem Gate änderten sich nur Windows-Renderer-/Diagnose-/Fixture-Dateien.

## 60. Error Handling

| Fall | Verhalten / Beleg |
|---|---|
| Fehlende externe Normalmap | Keine externe Abhängigkeit; Normalmap entsteht deterministisch bei Prepare. GPU-Allokationsfehler werden vom bestehenden Renderer-Fehlerpfad gemeldet, nicht durch einen Download ersetzt. |
| Fehlende SceneColor/Depth/Sky | `Begin` beginnt keinen Water-Pass; kein Null-Dereferenzieren. Source-Audit, kein erzwungener GPU-Fault-Test. |
| Ungültige Roughness/Absorption | Renderer-neutral validiert, NaN/Inf/negative/extreme Werte im Test. |
| Ungültige Sun-Daten | Gemeinsames `ValidateSceneLighting`; ungültige direkte Sonne deaktiviert. Portabler Test. |
| SSR-Eingänge fehlen / PSOs nicht bereit | Confidence-0-/Sky-Fallback; keine Annahme gültiger Radiance. Source-Audit; normale finalen Läufe haben null Fallbacks. |
| Zero-size Target | Window-Ressourcen freigeben, Backend suspendiert; GPU- und native Lifecycle-Prüfung. |

Keine künstliche Out-of-VRAM-/Device-Loss-Injektion. Daraus wird keine vollständige Recovery-Garantie für einen verlorenen GPU-Kontext abgeleitet.

## 61. Resource Lifetime

RAII besitzt Water-/FX-Ressourcen. Dynamische Scene/Depth/Sky/Shadow-Bindings werden nach dem Pass gelöst; Resize/Style/Shutdown geben Targets und Owner frei. Die GPU-Prüfung verlangt konstante Water-Resource-Creation-Zähler über gewöhnliche Folgeframes und null Owner nach zwei Style-Roundtrips. Der FX-Source-Audit bestätigt zusätzlich den frühen Return aus `PrepareResources` bei unveränderter Größe und Feature-Konfiguration; die eigenen Zähler umfassen FX-interne Allokationen nicht.

Native Hauptserie: 8560 Water-Frames, 59118 Water-Geometriedraws, 6294 SSR-Frames, 0 SSR-Fallbacks, 39 Water-Ressourcenerzeugungen einschließlich Resize/Restore, `WaterRenderers=0`, `ModernRenderers=0` am Exit. Zusatzserie: 2259 Water-Frames, 1940 SSR-Frames, 15 Erzeugungen, ebenfalls null Fallbacks/Owner. Die Zähler beweisen normalen Owner-/Source-Lifetime-Abbau; ein externes vollständiges Treiber-Leak-Capture wurde nicht durchgeführt.

## 62. Zero Legacy Audit

Release-/Debug-PE-Dependencies enthalten D3D11/DXGI, keine D3D9-, Granny- oder SpeedTree-DLL. Die bestehenden Source-/Boundary-/NoLegacy-Tests sind grün; native Startup-Logs wählen ZiiNAN GR2/Animation/Vegetation und GPU Skinning. `GrannyFileReads=0`. [Release-Dependencies](../../build/g7x/dependencies-Release.log), [Debug-Dependencies](../../build/g7x/dependencies-Debug.log).

Die bereits vorhandene `DDRAW.dll`-Abhängigkeit besteht weiterhin; dies ist keine Behauptung, sämtliche historischen Windows-/DirectDraw-Nebendependenzen entfernt zu haben. G7 führt keine zusätzliche Legacy-Engine ein.

## 63. Git Diff

G7 liegt ausschließlich als ungestagter Diff plus neue Source-/Test-/Berichtsdateien vor. Assets: nur `assets/root/uigraphicssettings.py` gegenüber Checkpoint `40445c57`. Source: Water-Renderer, FX-Subset, Settings, Integrationsstellen, Diagnose/Tests und dieser Bericht. Keine Serverdatei wurde durch G7 bearbeitet. Der separate Asset-Server-Tree hat einen fremden, unveränderten `share/locale/english/quest/qc.exe`-Diff (Dateizeit 2026-09-03 18:19:16); dieser liegt außerhalb des G7-Checkpoints und der G7-Patches. `m2dev-server-src` ist clean. Keine Build-/Log-/PDB-/Screenshotdateien gestagt.

[Git-Audit](../../build/g7x/git-audit.json), [Source-Patch einschließlich neuer Dateien](../../build/g7x/source-g7.patch), [Asset-Patch](../../build/g7x/assets-g7.patch). Finale Whitespace-/Index-Prüfung: **PASS**, beide Client-Indizes leer. Kein G7-Commit, kein Push.

## 64. Known Limitations

- SSR ist räumlich, ohne TAA/temporale Glättung; sichtbare kleine Unruhe und fehlende Offscreen-/Occlusion-Daten sind Grenzen der gewählten Technik.
- Vertikale Tiefenabsorption und vorhandenes Water-Raster; keine Unterwasseransicht, Foam, Caustics oder Ocean-Physik.
- Allgemeine transparente Meshes liefern weiterhin nicht alle Tiefeninformationen; G7 ändert ihre globale Sortierung nicht.
- Neue SSR-Qualität/Resize/Modern-Initialisierung kann vorbereitungsbedingt kurz stocken. Keine Performance-Finish-Phase.
- CPU-Passzeiten und eigene Target-Bytes sind verfügbar; isolierte GPU-Passzeiten und vollständiger FX-/Treiber-VRAM-Verbrauch nicht.
- Realer Netzwerk-Login/Relog, Device-Loss/Out-of-VRAM sowie vollständige Coverage aller Maps wurden nicht ausgeführt.
- Native Reflexionen können an flachen Ufern subtil sein; farbige technische Probes werden ausdrücklich separat ausgewiesen.

## 65. GO/NO-GO

G7-X = GO. Die finale Entscheidung beruht auf funktionierendem Modern-Water/FX-SSR, HDR-/Sun-/Fresnel-/Refraction-Belegen, nativer Eigenprüfung, den drei grünen Gates, strikt unverändertem Classic und sauberen Diagnose-/Shutdown-Zählern. Die dokumentierten Harness-/Messgrenzen werden nicht als ausgeführte Prüfungen gezählt.

## 66. Recommendation G8-X

G7 hier beenden. Eine spätere separat beauftragte G8-Abstimmung kann das Wasser im gesamten visuellen Kontext beurteilen; G8 wurde nicht begonnen. Ebenso kein H2, Texture-Impact-Pass, allgemeines UI-Projekt oder Performance-Finish. Kein G7-Commit und kein Push.
