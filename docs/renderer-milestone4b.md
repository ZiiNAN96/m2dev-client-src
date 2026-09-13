# Milestone 4B – statische Granny-Weltobjekte und Legacy-Materialparität

> Historischer Milestone-Stand. Aktuell seit [M12](renderer-milestone12.md): Diligent D3D11 ist im ON-Build der Default; Legacy wird explizit mit `--renderer=legacy-d3d9` gewählt. OFF-Builds bleiben Legacy-only. Frühere Testergebnisse und Auswahlbeschreibungen unten gelten für ihren damaligen Stand.

Stand: 12.09.2026. Ausgangsbasis: `95eda15`. Implementierung, repräsentative Ingame-/Map-Abnahme und finale vollständige Testsuite erfolgreich abgeschlossen. Die ausdrücklich verbleibenden Abdeckungs- und Kompositionsgrenzen stehen in den Abschnitten 2, 8, 18 und 19.

Legacy D3D9Ex bleibt der Standard. Diligent D3D11 bleibt ausschließlich mit `--renderer=diligent-d3d11` aktiv. Keine Fortsetzung mit Charakteren, SpeedTree oder Animationen.

## 1. Jetzt unterstützte statische Objektklassen

Normale starre Granny-Buildings/Props im belegten Outdoor-Bestand: große und kleine Gebäude, Mauern/Tore, Felsen, Steinlaternen, Dekoration, Trommel und andere Props. Ein-/Mehrmeshmodelle, mehrere referenzierte Materialgruppen, vorhandene starre LODs und Two-Sided-Materialien benutzen weiterhin die originalen Geometrie-/Transformdaten.

Erweiterungen gegenüber 4A:

- Unbenutzte leere Materialslots verwerfen nicht mehr das ganze Objekt; nur tatsächlich gezeichnete Gruppen werden validiert.
- A1R5G5B5-Diffusetexturen werden nativ und mit sämtlichen Originalmips geladen.
- Statische ShadowReceiver werden aus der vorhandenen Liste übergeben, bevor Legacy sie für den Opaque-Pass versteckt.
- Statische PCBlocker übernehmen die originale Kamera-Alpha-Maske, Projektion und Blend-/Depth-Regeln.
- Die Grundbeleuchtung bleibt bei Schatten an/aus passend zum jeweiligen nativen Materialpfad erhalten.

Bestandserhebung A1/B1/C1: 208 referenzierte Building-Dateinamen, 207 lose auflösbar, 228 starre PNT-Meshes, 13 Mehrmeshdateien; zusätzlich 31 LOD-Dateien mit 67 starren Meshes. Alle untersuchten Dateien ohne Animation. Die geprüften A1/B1-Ansichten sowie der normale C1-Ingame-Ausschnitt haben im finalen Adapterlog **keine Ausschlüsse oder Uploadfehler**. Das ist eine repräsentative Abnahme, keine Behauptung einer visuellen Einzelprüfung jedes Objekts auf jeder Karte.

## 2. Noch fehlende / ausgeschlossene Objektklassen

Animierte oder deformierte Granny-Objekte, TYPE_BLEND_PNT-/Opacity-Grundobjekte, Specular/SphereMap, additive Sondermaterialien, PNT2/DungeonBlock und beliebige nicht belegte Vertex-/Stagevarianten bleiben ausgeschlossen. In den untersuchten Basis- und LOD-Dateien gibt es keine Opacity-/TYPE_BLEND_PNT-Materialien; deshalb wurde kein allgemeines Blend-Materialsystem ergänzt.

SpeedTree, Charaktere/NPCs/Mobs, Effekte, Wasser, UI/Text/Minimap und Animationen sind ausdrücklich nicht Bestandteil von 4B. Ein unbekannter Zustand wird diagnostiziert und nicht als unterstützter Pfad behauptet.

## 3. Legacy-Call-Chain

`CPythonApplication::RenderGame` → `CMapOutdoor::OnRender` → `RenderArea` → originale Area-/Visibility-/Distanzlisten → `CGraphicThingInstance::Render` → `CGrannyLODController::RenderWithOneTexture` → `CGrannyModelInstance::RenderMeshNodeListWithOneTexture` → `CGrannyMaterial::ApplyRenderState` → `CStateManager::DrawIndexedPrimitive` → D3D9Ex.

Sonderlisten: `FAreaRenderShadow` → `RenderShadow` → `Hide`; später `RenderPCBlocker` → `FRenderPCBlocker` → `RenderPCBlocker`. Originale Reihenfolge, Hide/Show, Blendregeln und native Drawcalls bleiben erhalten. Kein grundlegender CStateManager-Umbau.

## 4. Diligent-Call-Chain / minimale Integrationspunkte

Dieselben originalen Maplisten → `BeginStaticMapObjects(shadowActive)` → `SubmitStaticMapObject(thing, Opaque / ShadowReceiver / CameraBlocker)` → aktuelle ModelInstance/Materialpalette/Meshmatrix → vorhandener CPU-Snapshot → `IStaticObjectRenderer::UploadGeometry/UploadTexture/Draw` → `DiligentStaticObjectRenderer` → D3D11.

Zentrale Dateien:

- [MapOutdoorRender.cpp](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/GameLib/MapOutdoorRender.cpp): drei vorhandene Übergabestellen, sieben zusätzliche / eine ersetzte Zeile.
- [StaticObjectBridge.cpp](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/GameLib/StaticObjectBridge.cpp) und [Header](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/GameLib/StaticObjectBridge.h): State-Erfassung, Gruppenauswahl, Übergabe und Diagnose.
- [DiligentStaticObjectRenderer.cpp](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/Renderer/DiligentStaticObjectRenderer.cpp) und [Drawdaten](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/Renderer/StaticObjectRenderData.h): begrenzte D3D9-Materialnachbildung.
- [StaticObjectTextureLoader.cpp](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterLib/StaticObjectTextureLoader.cpp) und [Header](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterLib/StaticObjectTextureLoader.h): objektbezogener Format-Zusatz über vorhandenen DDS-Parser.
- [TerrainTextureData.h](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/Renderer/TerrainTextureData.h): ausschließlich ein angehängter neutraler Formatwert; Terrain-Decoder/-Renderer unverändert.

Bei Legacy ist `staticObjectRenderer == nullptr`; die Übergabe kehrt ohne Änderung nativer Renderstates zurück. Backendwahl und Lifecycle wurden nicht erweitert.

## 5. Granny-Datennutzung

Die vorhandene Granny Runtime lädt weiterhin GR2, Models, Skeletons, Meshes und Materialien. Kein GR2-Parser, Granny-Rewrite, Skinning oder neues Bonesystem. Vorhandene 4A-Snapshots werden genutzt, ohne bereits freigegebene Granny-Rohdaten nachträglich auszulesen.

Statische Modelle können 1–4 Bones besitzen, obwohl sie keine Animation haben. Es gelten die vorhandenen finalen `m_meshMatrices` der ModelInstance; bloße Annahmen wie „statisch = Bone 0“ wären falsch und wurden nicht eingeführt.

## 6. Mesh-/Submesh-Struktur

Originales 32-Byte-PNT-Layout und 16-Bit-Indizes. Pro Mesh bleiben `baseVertex`, `vertexCount` und finale World-Matrix erhalten; pro Dreiecksgruppe `firstIndex = idxBase + TriFirst*3`, `indexCount = triCount*3`, Palettenindex und ursprüngliche Reihenfolge.

Die Materialpalette der aktuellen ModelInstance/LOD ist maßgeblich. Die 198 leeren, unbenutzten Palettenslots der Inventur sind keine Draws. Validierung überspringt nur solche Slots, nicht fehlerhafte referenzierte Gruppen. Alle benötigten Gruppen eines unterstützten Objekts werden vor Einreichung validiert.

## 7. Materialtypen und Grundfarbe

Unterstützt ist das belegte starre `TYPE_DIFFUSE_PNT` einschließlich Two-Sided, nativer Diffusetexturen und separatem PCBlocker-Verdeckungszustand. Keine frei erfundene Granny-Materialfarbe und kein Vertex-Color-Attribut: Die PNT-Daten enthalten keine Farbe.

Die Originalumgebung liefert `D3DMATERIAL9`, Ambient/Diffuse/Emissive und Lichtwerte. Stage-0-`SELECTARG1(TEXTURE)` bleibt RGB-unbeleuchtet; `MODULATE` verwendet die ursprüngliche Diffuse-Beleuchtung. Ein aus der Charakterauswahl verbliebenes Punktlicht 1 wird nur in den tatsächlich benötigten beleuchteten ShadowBase-/PCBlocker-Pfaden nachgebildet. Der 4A-Test für unbeleuchtete Weltobjekte mit aktivem Intro-Licht bleibt erhalten.

## 8. Alpha-Test-Modi

`Disabled`, `GREATEREQUAL` und `GREATER`, jeweils mit dem **tatsächlichen** `ALPHAREF` im Bereich 0–255. Keine pauschale 0,5-Schwelle. Referenz-GPU-Fälle prüfen GE/128, G/128, GE/1 und G/0 an originaler nativer D3D9-Alphaausgabe.

Wichtige Testgrenze: In den untersuchten normalen statischen Grundmaterialien ist kein aktiv gesetztes Cutout-Material belegt. Eine Fahne oder ein Zaun wird daher nicht ohne Beleg als Alpha-Test-Objekt ausgegeben. Die Fähigkeit ist nativ GPU-geprüft; eine separate reale Cutout-Prop-Abnahme kann für diesen Bestand nicht behauptet werden. PCBlocker übernimmt auch geerbte Alpha-Test-Zustände.

## 9. Alpha-Blend-Modi

Genau `SRCALPHA / INVSRCALPHA`, Operation `ADD`, keine separate Alpha-Blendfunktion. Der belegte Bedarf ist die originale Gebäude-Kameraverdeckung mit `PCBlockerAlpha.dds`, nicht ein neues Opacity-Materialsystem.

Stage 1 ersetzt Alpha durch die vorhandene Maske, RGB bleibt CURRENT. Stage-0-Alpha darf SELECTARG1 oder MODULATE sein: Ein vorausgehender nativer Tree-PCBlocker kann MODULATE hinterlassen; der folgende Maskenpass ersetzt dieses Alpha ohnehin. Originale Draw-Reihenfolge und aktueller DepthWrite-Zustand bleiben maßgeblich. Tests prüfen sowohl aktiviertes als auch deaktiviertes DepthWrite; nichts wird pauschal auf „Transparent = keine DepthWrites“ umgestellt.

## 10. Cull Modes / Transformation / Depth

NONE, CW, CCW pro Draw; `Two-sided=1` setzt ausschließlich das betroffene Material auf NONE. Kein globales Abschalten des Cullings. World/View/Projection und Normaltransformation aus 4A bleiben bestehen.

Depth-Test LESSEQUAL, originale Write-Regeln. Native GPU-Referenzen prüfen nichtuniforme Skalierung, Spiegelung, Originaloffsets, mehrere Materialien sowie Terrain/Objekt-Überdeckung in beiden Einreichungsreihenfolgen. Reale Bilder bestätigen Silhouette, Lage und Materialgruppen; normale Ingame-Kamera und Verdeckung wurden vom Nutzer bestätigt.

## 11. Texture-/Sampler-Verhalten

Originaler Resource-/Pack-Lookup und DDS-Parser. Bereits unterstützte BC1/BC2/BC3-/32-Bit-Formate bleiben; hinzu kommt genau der belegte A1R5G5B5-Fall als natives `B5G5R5A1_UNORM`, ohne verlustbehaftete neue Konvertierung. `general_obj_drum.dds` hat fünf Originalmips.

Stage 0: UV0, WRAP/CLAMP, POINT/LINEAR sowie native anisotrope Kombination mit tatsächlichem MaxAnisotropy; Mips NONE/POINT/LINEAR. Stage 1 für PCBlocker: originale BC2-Maske, CAMERASPACEPOSITION/COUNT2, `m_matBuildingTransparent`, CLAMP. Keine generierten Ersatztexturen, neuen Materialien oder Schattenmaps. Uninitialisierte Cachefelder werden lesend vom nativen Gerät erfasst, nicht durch CStateManager-Umbau „repariert“.

## 12. Resource Lifetime / Stabilität

Objekt-/Model-/Texturressourcen gehören weiterhin zur lebenden Area/Thing-Lifetime. Beim Unload werden GPU-Bindings freigegeben und Objektcaches entfernt. Die Kameramaske wird im Bindungs-Cache über eine schwache Identität referenziert; kein Shared-Pointer-Selbstzyklus. Backendtests prüfen auslaufende Weak-Pointer und null offene Geometrie-/Texturzähler.

Finaler gepaarter Echt-Map-Lauf, 54 Messungen je Backend, ca. 270 Sekunden:

| Backend | Private Speicher nach 15 s | Maximum | Letzte Messung | Handles nach 15 s → Ende |
|---|---:|---:|---:|---:|
| Legacy | 284,3 MiB | 302,5 MiB | 291,6 MiB | 784 → 778 |
| Diligent | 374,0 MiB | 413,2 MiB | 375,0 MiB | 1119 → 1116 |

Kein stetiger Anstieg über die Mapwechsel in diesem Lauf. Mapabhängige Peaks sind sichtbar; dies ist kein Langzeit-Leakbeweis. Beim Wechsel A1→B1, B1→A1 und finalem Unload steht im Adapterlog jeweils `release live_objects=0`; der Shutdown meldet `object_geometry=0 object_textures=0`.

Kein beobachteter Renderer-Crash, keine Assertion, kein Device-/Upload-/Lifetime-Fehler in den erfolgreichen finalen Läufen. Ressourcenverbrauch ist wegen parallel weiterlaufendem D3D9 und zusätzlichen Snapshots/Caches höher; keine Performanceoptimierung vorgenommen.

## 13. Schatten an / aus

Automatisierte Original-Map-Tests wechseln ShadowLevel 0 und 3. Gebäude bleiben in beiden Zuständen vorhanden. Zusätzlich bestätigt der normale Diligent-Ingame-Lauf den echten ShadowReceiver `c1-022-10m-bridge.gr2`.

Der Schalter verändert im nativen Materialpfad auch die Grundbeleuchtung: Diese wird jetzt korrekt übernommen. Dynamische D3D9-Schattentexturen werden **nicht** mit dem Diligent-Weltbild zusammengesetzt und nicht neu erzeugt. Vollständige Schattenparität ist kein 4B-Ergebnis. Im Trommel-Bildpaar ist deshalb ein Bodenschattenunterschied außerhalb des eigentlichen Trommelmaterials erwartbar.

## 14. Build-Ergebnisse

Release x64, Visual Studio 2022 / MSVC 14.44, Windows SDK 10.0.26100.0:

| Variante | Ergebnis | Nachweis |
|---|---|---|
| `M2_ENABLE_DILIGENT_D3D11=OFF` | erfolgreich, Exit 0 | `build-final-off.log` |
| OFF Renderer-Tests | 3/3, 0,56 s | `tests-final-off.log` |
| `M2_ENABLE_DILIGENT_D3D11=ON` | erfolgreich, Exit 0 | `build-final-on.log` |
| ON Renderer-Tests | 5/5, 2,13 s | `tests-final-on.log` |

Keine Tests deaktiviert. OFF enthält erwartungsgemäß keine Diligent-spezifischen Testtargets. Vorhandene Debug-PDB-/Bibliothekswarnungen bleiben; keine neuen Buildfehler. Anfangs konnte die eingeschränkte Buildumgebung installierte SDK-/ATL-Verzeichnisse nicht lesen; der Build mit regulärem SDK-Zugriff war erfolgreich, ohne Checks abzuschalten.

Finales Buildartefakt: `C:\Users\ZiiNAN\Documents\GitHub\m2dev-client-src\build\bin\Release\Metin2_Release.exe`, SHA256 `A294B966E106C3739E91FBF5DCD559CAEC70E74979F718AC6AF10B61E20BDCF8`.

Die normalen Testclients verwendeten eigene EXE-Kopien, Originalpakete und getrennte Testkonfigurationen. Die letzte Diligent-Ingame-/Bildtest-Kopie vor dem OFF→ON-Neubuild hat SHA256 `B99B371BBC38E8782ED52D6A243B7337B47B6A91C453B82713422E8D609CCEDA`; gleicher Produktionsquellstand, separat gelinktes Buildartefakt. Die Haupt-Runtime-EXE wurde im Rahmen dieses Abschlusses nicht ersetzt.

## 15. Testsuite / native GPU-Parität

Die vollständige Suite bestand zunächst 9/9 Tests in 512,09 s (`full-suite.log`). Nach der letzten Präzisierung der Schatten-Grundfarbe wurden sämtliche 27 statischen GPU-Fälle und die fünf Renderer-Tests erneut erfolgreich ausgeführt. **Auch die vollständige Suite auf dem finalen ON-Build ist erfolgreich: 9/9, Exit 0, 456,39 s** (`full-suite-final.log`, Fuzztest 456,37 s). Keine Tests ausgelassen oder deaktiviert.

`Renderer.TerrainGpuParity` enthält bestehende Terrain-Geometrie, Textur-/Splatting-/Fog-/Lifetime-Tests und jetzt 27 statische Objektposen. Die neue Alpha-Referenz liest eine tatsächliche A8R8G8B8-D3D9-Testfläche; das unbenutzte X-Byte eines X8-Fensterpuffers zählt nicht als Alpha.

Statische Referenzfälle: ursprüngliche 4A-Posen 0–13, native 16-Bit-Mips 14/15, Original-AlphaRef-Vergleiche 16–19, Blend/DepthWrite 20/21, Kameramaske 22, Punktlicht 23, geerbtes MODULATE-Alpha 24 und ShadowBase/Intro-Licht 25/26. Mittlerer direkter RGB-Fehler maximal ca. 0,241 Kanalwerte von 255. Keine Alpha-Innenfehler; klar begrenzte Ein-Pixel-Texel-/Rastergrenzen werden separat gezählt (im Punktlichtfall 35 Kantenpixel und zwei Alpha-Grenzpixel), nicht verschwiegen.

Resize wird mehrfach mit echten Backendgrößen 800×600, 320×240, 1024×768, 640×480 sowie 0×0-Suspend/Resume geprüft. Statische Objektfälle wiederholen Suspend/Restore in den Posen 6/19/23. Der normale Client hat keinen frei ziehbaren Resize-Rand; es wird kein manueller Drag-Resize behauptet.

## 16. Legacy- und Diligent-Ingame-Regression / Exitcodes

| Lauf | PID | Ergebnis |
|---|---:|---|
| Isolierter Legacy-Map-/Minimize-Test | 7920 | Nutzer „passt“, automatischer Shutdown 0, 275,6 s |
| Korrigierter Diligent-13-Posen-Maplauf | 3120 | A1→B1→A1, Schatten 0/3, Shutdown 0, 325,8 s |
| Normaler Legacy-Start ohne Rendererargument | 28028 | Nutzer „passt“, Shutdown 0, 95,2 s |
| Normaler Diligent-Start mit Rendererargument | 60940 | Nutzer „sieht gut aus“, Shutdown 0, 80,2 s |
| Finaler gepaarter Legacy-Maplauf | 53884 | A1→B1→A1, automatischer Shutdown 0 |
| Finaler gepaarter Diligent-Maplauf | 16380 | A1→B1→A1, automatischer Shutdown 0, Objektressourcen 0 |
| Abschließende Legacy-Bank-/Prop-Aufnahmen | 35484 | automatischer Shutdown 0 |
| Abschließende Diligent-Bank-/Prop-Aufnahmen | 62440 | automatischer Shutdown 0, Objektressourcen 0 |

Legacy-Ingame: Nutzer prüfte Gebäude/Props, Charaktere/NPCs/Mobs, Bäume, Effekte, UI, Terrain und Schatten aus/an. Keine gemeldete Regression. Mehrfaches Minimize/Restore der isolierten Legacy-/Diligent-Mapclients wurde separat bestätigt. Im letzten normalen Diligent-Test wurden Gebäude aus mehreren Winkeln, Mauern/Treppen, Tiefe und Kamera-Verdeckung sowie zweimaliges Minimize/Restore bestätigt.

Die normalen beiden Ingame-Logs enthalten jeweils einmal `invalid idx 0`; Herkunft ist die vorhandene Gildenmarkenprüfung in `C:\Users\ZiiNAN\Documents\GitHub\m2dev-client-src\src\UserInterface\MarkManager.cpp` (Zeilen 282/309), kein Rendererfehler. Keine Änderung daran.

Vorläufe mit dem inzwischen korrigierten PCBlocker-Stateausschluss zählen nicht als finale Paritätsabnahme. Auch ein zusätzlicher Bildtest-Versuch mit versehentlich falsch benanntem Testpaket (`Failed to load root.pck`, noch vor dem Weltstart) ist nur Testaufbaufehler: separat archiviert, Pfad korrigiert, danach obige erfolgreiche Läufe. Ein Prozess-Exitcode allein wäre kein ausreichender Weltstartbeweis.

## 17. Bildvergleich

Vier repräsentative Objekte, je identische geskriptete Kamerapose und gleicher Schattenschalter. Originale, unveränderte Fensteraufnahmen mit dem Computer-Use-Skill; keine generierten oder retuschierten Testbilder. Siehe mitgelieferte Vergleichsgalerie `renderer-milestone4b-bildvergleich.html` und Originalbilder in `map-captures`.

| Paar | Prüfung | ROI, mittlerer RGB-Fehler / 255 |
|---|---|---:|
| A1 Bank, Schatten 3 | große Silhouette, Dach/Wände/Treppen, Materialien, Grundbeleuchtung | 5,347 |
| B1 Mehrmeshgebäude, Schatten 0 | Mesh-/Materialgruppen, Kamera-Alpha, Depth, Transform | 2,851 |
| B1 Two-Sided-Prop, Schatten 0 | Dekoration `obj-0001`, Form/Textur/Cull | 4,629 |
| A1 Trommel, Schatten 3 | native A1R5G5B5-Textur, Originalmips, Aufstellung | 2,942 |

Messgrenze: feste Rechtecke einschließlich lokalem Hintergrund, JPEG-Aufnahmen, keine Objektmasken und kein Vollbild-Paritätsbeweis. Die Fensteraufnahmen unterscheiden sich um ein äußeres Randpixel; die zusätzlich ausgewiesene Registrierung ist maximal eine ganzzahlige Verschiebung um ±1 Pixel, **keine Skalierung/Verzerrung/Filterung**. Unregistrierte Fehler, Rechtecke und Versatz stehen vollständig in `image-comparison.json`; `compare_final_objects.py` arbeitet nur lesend. Die verlustfreien nativen GPU-Vergleiche aus Abschnitt 15 sind der präzisere numerische Materialnachweis.

Visuell: keine falschen Materialgruppen, fehlenden Meshteile oder doppelten Objekte in den verglichenen Ansichten. Nebenansichten und Gegenrichtung des Two-Sided-Props sind Bestandteil des 13-Posen-Laufs. Bodenschatten, native Bäume im PCBlocker-Pfad und andere nicht migrierte Weltbestandteile dürfen Unterschiede außerhalb des geprüften Objekts erzeugen.

## 18. Mapwechsel-Ergebnis

**A1 → B1 → A1 durchgeführt**, mehrfach in beiden Backends, mit originalen Pack-Assets und vorhandenen Background/Map-Ladefunktionen. Finaler Lauf jeweils ca. 4,5 Minuten. Log enthält dreimal `map loaded` in genau dieser Folge und anschließend `normal map/window shutdown`.

Die frühere Ressourcen-/Mapwechsel-Testlücke ist damit für die **automatisierte echte Maplade-/Renderpipeline** geschlossen. Ein serverseitiger Reise-/Warp-Ablauf mit eingeloggtem Charakter wurde damit nicht automatisiert und nicht als geprüft ausgegeben. Kein Serverumbau und keine neue Teststeuerungs-API.

## 19. Sichtbare Systeme und bekannte Einschränkungen

| System | Normaler Legacy-Client | Sichtbares Diligent-Weltbild |
|---|---|---|
| Terrain/Splatting | bestehend | migriert |
| Unterstützte starre Granny-Buildings/Props | bestehend | migriert, einschließlich Kameraverdeckung |
| Spieler/NPCs/Mobs/Animationen | bestehend | nicht zusammengesetzt |
| SpeedTree/Bäume | bestehend | nicht zusammengesetzt |
| Effekte/Partikel | bestehend | nicht zusammengesetzt |
| UI/Text/Minimap | bestehend | nicht zusammengesetzt |
| Wasser und dynamische Schattentexturen | bestehend | nicht migriert |

Die bestehenden Legacy-Drawcalls laufen weiter. Die experimentelle Diligent-Weltfläche ist noch keine vollständige Hybrid-Komposition; ihre Unsichtbarkeit ist kein stillschweigend gelöster 4B-Fehler. Login/Charakterauswahl verbleiben im bestehenden Legacy-Pfad. Nicht migrierte Systeme wurden nicht umgebaut.

Weitere Grenzen: keine Vollinventur aller Karten, kein belegtes reales Alpha-Test-Grundprop im ausgewählten Bestand, keine Opacity-/Specular-/PNT2-Erweiterung, kein nachträglicher Rohdaten-Readback bereits außerhalb des Map-Capture-Pfads geladener Modelle. Eine B1-Property verweist auf eine unter dem exakten Namen nicht lose vorhandene `hay_02.gr2`-Datei; das beweist allein keinen Packfehler und wurde nicht durch einen willkürlichen Assetersatz kaschiert. Langzeitverhalten über Stunden und Spiel-/Server-Warp sind nicht abgenommen.

## 20. Empfehlung für den nächsten separat zu beauftragenden Milestone

Zuerst die weiterhin offene Weltbild-Komposition und die Priorität **genau eines** nächsten Systems abstimmen; nicht gleichzeitig SpeedTree, Charaktere, UI und Animationen migrieren. Falls Charaktere als nächstes gewünscht sind, zunächst ihren bestehenden Granny-/Skinning-/Attachment-Vertrag analysieren und einen isolierten Minimalpfad definieren. Das ist nur eine Empfehlung, **keine begonnene Umsetzung**.

## Git-Diff / Übergabe

Neun bestehende aufgabenbezogene Dateien: 356 zusätzliche / 61 entfernte Zeilen. Dazu fünf neue Dateien: zwei objektbezogene TextureLoader-Dateien, ein isolierter Real-Map-Test und Analyse/Abschlussbericht. Die Änderungsgruppen sind Map-Übergabe, statische State-/Materialnachbildung, ein Texturformat und gezielte Tests; kein Renderer-Lifecycle-/Terrain-/Granny-/Actor-/SpeedTree-/UI-/Serverumbau.

`git diff --check` ohne Fehler. Die gleichzeitig beobachteten fremden Resource-Editor-Änderungen `UserInterface.aps` / `RCa39752` wurden nicht angefasst und sind nicht Bestandteil dieser Zahlen. Build-/Testpakete und Bilder liegen ignoriert unter `C:\Users\ZiiNAN\Documents\GitHub\m2dev-client-src\build\milestone4b`; Originalassets wurden für die Tests nicht bearbeitet. Keine Commit-, Push-, Reset-, Stash- oder Clean-Aktion.

**STOP nach Milestone 4B.** Keine weitere Migration.
