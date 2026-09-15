# H-X Gate B – Production Switch und vollständige SpeedTree-Entfernung

Stand: **15.09.2026 · Gate B: GO · danach STOP**. Gate A1/A2 sind manuell freigegeben; auch die finale Gate-B-Sichtprüfung wurde bestätigt. Kein Commit, kein Push, keine nächste Phase.

Basis: `04cb378651418439b15fd21a19765ef46608f6b4`. Release und Debug wurden im neuen `build-hx-clean` erstellt. ZiiNAN ist der einzige Vegetationspfad. Die vollständigen lokalen Messergebnisse stehen in `build-hx-clean/hx-evidence/result.json`; der reproduzierbare [Zero-Audit](phase-hx-zero-audit.json) enthält Source-Kategorien, Link-Projekte und Binär-Hashes.

## 1. Gate A Ausgangslage

Gate A hat Registry, ZVEG, GLB, neutrale Runtime und die Anbindung an den vorhandenen Diligent-Meshrenderer bereitgestellt. Der alte SDK-Pfad war bis zur manuellen Freigabe noch vorhanden. Die ursprüngliche A1-Berichtsfassung bleibt in der genannten Git-Basis nachvollziehbar.

## 2. Gate A1 Ergebnis

118/118 Quellen konvertiert; alle 85 produktiv erreichbaren Typen unterstützt. A1 bestand mit Release 43/43, Debug 43/43 und GCC/LP64 21/21 sowie World-/Multi-Map-Smokes. Das sind historische A1-Ergebnisse, keine neu ausgeführten Gate-B-Tests.

## 3. Gate A2 Ergebnis

A2 wurde nach dem [Visual-Parity-Fix](phase-hx-a2-visual-parity-fix.md) mit „passt“ bestätigt. Ursache der weißen Blätter war die Übertragung des Blattnebels bei Dichtenebel: veraltete allgemeine FogStart/FogEnd-Werte statt der separaten linearen Blattspanne. Kein Verlust des Textur-Alpha-Kanals. Gate B bewahrt die Korrektur `0 .. 2.3 / density` unverändert in der ausführbaren Logik.

## 4. Production Default Switch

Normaler Start ohne Argumente: `Vegetation=ziinan`, `VegetationSelection=default`. Es gibt keine Runtime-Modusvariable und keinen Reference-Zweig mehr. `--vegetation=ziinan` bleibt ein optionaler Kompatibilitätsalias; `--vegetation=reference` und andere Werte sind ungültig.

Produktiver Aufrufweg: alte Map-/Actor-/Eventreferenz → `CreateWorldTree` → Registry → ZVEG/GLB → Vegetation Runtime → vorhandener Diligent-Meshrenderer.

## 5. No-Fallback Policy

Fehlende Registry-Zuordnung, fehlendes ZVEG/GLB oder beschädigte Daten liefern einen klaren Fehler. Der World-Adapter protokolliert ihn und der Client beendet sich bei Vegetationsfehlern mit Code 6. Es existiert kein SDK-Loader als Ersatz. Fehlgeschlagene Asset-Ladevorgänge werden gecacht; pro Frame entstehen keine erneuten Leseversuche. Die Tests prüfen fehlende Zuordnungen, Dateien und ungültige Metadaten ausdrücklich.

## 6. Golden Vegetation References

Vor dem Entfernen der Runtime wurden [production-goldens.json](../../tests/Vegetation/fixtures/production-goldens.json) und [render-goldens.json](../../tests/Vegetation/fixtures/render-goldens.json) gesichert: zusammen **332.826 Bytes**, ohne vollständige Mesh- oder Texturkopien.

Neun Typen: Baobab, Beech, Monterey Cypress, Pagoda, Sassafras, B3 Beech RT3, B3 Pagoda Winter, Cinnamon Fern und Coconut Palm. Enthalten sind unabhängige SDK-Zählwerte, ausgewählte Positionen/UV/Farben, Bounds, Material-/Texturbezüge, reale Maptransforms und SDK-Windstichproben. Zusätzlich sind akzeptierte neutrale Metadaten samt LOD-/Alpha-Stichproben gesichert. Ihre Herkunft ist im Fixture ausdrücklich getrennt. 54 Referenzbilder aus A2 werden als kleine 16×16-RGB-Kachelsignaturen und Originalbild-Hashes bewahrt.

## 7. Final Dependency Audit

[Auditwerkzeug](../../tests/Vegetation/audit_removal.py): **1.480 vorhandene produktive Textdateien**, **39 transitive produktive Build-/Link-Projekte**, vier Binärdateien. Produktionsfunde: **0**. CMake-Regenerierungsaufgaben werden separat als Build-Utilities aufgeführt, weil sie keine Bibliotheken in den Client linken.

| Kategorie | Einordnung |
|---|---|
| A – Production | 0 Includes, Typen, Aufrufe, SDK-Artefakte und Link-Anbindungen |
| B – Offline Converter | Nur ausdrücklich aktivierte `tools/Vegetation`-Targets mit externem SDK |
| C – Tests/Reference | Negative Audits und historische M6-/A2-Testbeschreibungen |
| D – Docs/Comments | Historische Berichte und Dokumentation der Entfernung |
| E – Dead Code | Alter Runtime-Code entfernt; keine produktive zweite Baumruntime |

Alle verbleibenden Texttreffer stehen mit Datei und Zeile im JSON-Audit. Historische Test-/Dokumentationsnamen sind keine Produktionsabhängigkeiten.

## 8. Old Runtime Removal

`src/SpeedTreeLib` samt Wrapper, Forest, Material-/Wind-/LOD-Verwaltung, alter Grass-Integration und Bridge entfernt. `DiligentTreeRenderer` sowie `TreeRenderData` entfernt. Keine alte Render-Submission, kein SDK-Billboard- oder Windzustand bleibt im Client.

## 9. Include Removal

SDK-Includes aus World, Actor, Area, Map und Client-PCH entfernt. `extern/include/SpeedTreeRT.h` ist nicht mehr im Quellprojekt. Die übrigen gemeinsamen Header in `extern/include` bleiben für unabhängige Bibliotheken verfügbar.

## 10. Type Removal

Produktive `CSpeedTree*`-/SDK-Typen: **0**. World verwendet den neutralen `CWorldTreeInstance`-Vertrag; AssetRuntime und Vegetation enthalten ausschließlich eigene Strukturen. Das gemeinsame Frame-Flag heißt `vegetationWorldFrame` und benötigt keinen alten Tree-Renderer-Vertrag.

## 11. Runtime Call Removal

Alte LoadTree-, Compute-, GetGeometry-, SDK-LOD-, Wind-, Licht- und Fog-Aufrufe sowie Singleton-Zugriffe entfernt. Der automatisierte `Vegetation.NoLegacyDependency`-Test kontrolliert den produktiven Source- und CMake-Baum.

## 12. Linker Cleanup

`SpeedTreeLib`, `SpeedTree`, `speedtree_static.lib` und `speedtree_staticd.lib` sind aus dem Client-Linkpfad entfernt. Der transitive Projekt-Audit prüft auch Includes, Defines, Linkoptionen und zusätzliche Abhängigkeiten in den generierten Projekten. Keine alten Build-Artefaktpfade oder Offline-Importer im Client-Linkpfad.

## 13. CMake Cleanup

Produktive `add_subdirectory`- und Renderer-Source-Einträge entfernt. `ZIINAN_BUILD_LEGACY_SPT_CONVERTER` bleibt standardmäßig **OFF**. Der Client kann ohne SDK konfigurieren und bauen. Golden-, Registry- und GPU-Tests hängen nicht mehr vom Converter ab; der Pack-Audit ist ebenfalls SDK-frei.

## 14. Legacy Converter Separation

`LegacySPTSDK` ist nur im optionalen Tool-Verzeichnis definiert. Der Entwickler muss `ZIINAN_LEGACY_SPT_SDK_ROOT` ausdrücklich auf ein externes SDK setzen. Die drei früher produktiv erreichbaren Header-/Library-Dateien wurden mit Hashkontrolle außerhalb des Checkouts gesichert; kein lokaler SDK-Pfad wird in Source-Dateien festgeschrieben.

Separater Nachweis in `build-hx-converter`: Windows, `M2_BUILD_WINDOWS_CLIENT=OFF`, `M2_BUILD_RENDERER_TESTS=OFF`, `M2_BUILD_ASSET_TOOL=ON`, Converter ON. Keine Client-/Diligent-Abhängigkeit nötig. Beide Tool-Targets erfolgreich gebaut; ein Baobab erzeugt byteidentische GLB-/ZVEG-Dateien. Fehlende/beschädigte Eingaben werden sauber mit Code 1 verworfen.

## 15. Clean Configure

`build-hx-clean` existierte vor diesem Lauf nicht. Visual Studio 2022, x64, Windows D3D11, Converter OFF. SDK-Header und -Libraries wurden vor Configure aus den produktiven Suchpfaden entfernt. Kein alter Cache, OBJ oder Build-LIB übernommen. Bereits vorhandene, gepinnte Quellarchive von Diligent/Assimp/meshoptimizer wurden wiederverwendet; ihre Buildprodukte wurden neu erstellt. Unabhängige bereits mitgelieferte Fremdbibliotheken bleiben Teil des bestehenden Client-Builds.

## 16. Clean Release

Frischer vollständiger Release-Build: **Exitcode 0**. EXE: `build-hx-clean/bin/Release/Metin2_Release.exe`.

SHA256: `942058d512ddf1db3f3d0643a7e67f57c50e8d70ef294b8335fa41c4a5c0a307`.

## 17. Clean Debug

Frischer vollständiger Debug-Build: **Exitcode 0**. EXE: `build-hx-clean/bin/Debug/Metin2_Debug.exe`.

SHA256: `c9f3064fce37ecca8e1242fd04be14d10dc77c482eed8d25082d89968f8149b3`.

Keine Warnungsfreiheit behauptet: vorhandene C4005/C4313/C4477/C4834/D9025 sowie LNK4099; Debug zusätzlich LNK4075/LNK4098. Details stehen in den beiden Buildlogs. Keine neuen Vegetations-Compile-/Linkfehler.

## 18. Binary Audit

Release-/Debug-Client und die beiden frisch gebauten Diligent-D3D11-DLLs: **AMD64**, keine SpeedTree-/Granny-SDK-Marker oder -Imports, keine D3D8/D3D9/D3DX-Imports. Static-Link-Freiheit wird gemeinsam durch Source-Audit, entfernte SDK-Artefakte und transitive Link-Projekte belegt; eine PE-Importliste allein würde dazu nicht genügen. Bestehende DirectDraw-/Input-/Video-Abhängigkeiten wurden nicht verändert.

## 19. Header Leak Test

Vegetation Boundary, AssetRuntime PublicHeaders und Platform PortableHeaders sind in Release, Debug und GCC grün. Zusätzlich erfasst der vollständige Source-Audit alle produktiven Header, auch Renderer/Core. Keine SDK-Typen oder Includes; die gemeinsame Runtime bleibt ohne Windows-SDK-Glue baubar.

## 20. Golden Tests

SDK-freie CPU-Goldens für neun Typen bestanden in allen drei Builds. Geprüft: Branch-/Frond-/Leaf-Zählwerte, LODs, ausgewählte Positionen/UV/Farben, Bounds, Texturen/Materialzustände, Metadaten, reale Platzierungen, Wind, Asset-Sharing und vollständige Freigabe.

GPU-Goldens pro Windows-Konfiguration: **225 Frames**, **81 Vergleiche gegen 54 unabhängige Bildsignaturen**, fünf Kamera-/Umgebungsfälle und near → mid → far → mid → near. Maximaler mittlerer RGB-Kachelfehler **0,0103892/255**, maximale einzelne Kachel-/Kanalabweichung **0,996522/255**. Grenzwerte: Mittel <0,25 und Maximum <3. Release und Debug liefern dieselben Vergleichswerte. Das ist eine kompakte Regression der gespeicherten Referenzansichten; es ersetzt keinen neuen SDK-Livevergleich.

## 21. Registry/Resolver

Valid Mapping, fehlender Key, normalisierte Groß-/Kleinschreibung und Slashes, doppelte Keys, Pfad-Traversal, malformed ZVEG, fehlendes/defektes GLB, atomare Registry-Fehler und gecachte Ladefehler geprüft. Der Golden-Reader akzeptiert ausschließlich `.zveg` und `.glb`; ein SPT-Leseversuch würde den Test abbrechen. Die instrumentierten echten Maps verwenden den normalen Pack-/Loose-File-Resolver und Original-Texturpakete.

## 22. Map Transform

Reale gespeicherte Platzierungen werden gegen die neutralen Bounds und Instanztransforms geprüft. Bestehende Tree-Semantik bleibt Translation einschließlich HeightBias; separate Euler-Angaben aus AreaData wurden schon vom alten Tree-Erzeugungspfad nicht angewendet. Größe bleibt im konvertierten Asset, Instanzscale ist unverändert 1. Allgemeine Rotation-/Scale-Bounds sind zusätzlich im Runtime-Vertrag getestet. Es wurde keine neue Interpretation alter Mapwinkel eingeführt. Manuelle Position-/Größen-/Ausrichtungsprüfung: bestätigt.

## 23. LOD

Near → medium → far → medium → near besteht in CPU- und GPU-Tests. Die zurückkehrenden Bilder sind deterministisch identisch; alle Zustände besitzen gültige Geometrie. ZVEG liefert diskrete Meshzuordnung und Alpha-Werte. World-Smoke: **192 beobachtete LOD-Wechsel**. Der bereits in Gate A akzeptierte native LOD-Pfad bleibt aktiv.

## 24. Branch/Frond/Leaf

Im World-Smoke: **42.326 Branch-, 38.086 Frond-, 63.735 Leaf- und 24.414 Billboard-Draws**. Materialien, Texturreferenzen, Culling und Sichtbarkeit sind durch Golden-Tests und die finale Sichtprüfung abgedeckt. Höchstens fünf Teil-Draws pro getesteter Instanz; kein neuer Batch-/Meshkopierpfad.

## 25. Alpha

A2-Logik erhalten. GLB-Materiale sind MASK, Basis-Cutoff 84/255; konkrete Draw-Cutoffs kommen aus den gespeicherten LOD-Werten. Alpha-Test `Greater`, DepthWrite an, normale Vegetation ohne Blend; Zweige cullen wie zuvor, Fronds/Blätter/Billboards doppelseitig. Kamera-Verdeckung verwendet weiterhin den vorhandenen Alpha-Masken-/Blendpfad. Produktionscache und direkter Texturdecoder stimmen in den GPU-Tests in Format, Maßen und sämtlichen Mip-Bytes überein. Keine weißen/schwarzen Quads in der bestätigten finalen Sichtprüfung.

## 26. Billboard

Gesicherte vier Vertices/sechs Indices, Textur-/Alpha-/Cull-Vertrag und Fernaktivierung geprüft. Far-Bildsignaturen, Wiedereintritt in mittlere/nahe LODs und manuelle Prüfung bestanden. Keine neue Impostor-Technik oder zusätzliche SDK-Abhängigkeit.

## 27. Wind

Deterministische Instanzphase, neutrale Profilwerte und übergebene Amplituden/Frequenz geprüft. GPU: t=0 → t=1,25 → t=0 verändert das Blattbild und kehrt exakt zurück. Im akzeptierten Profil sind Branch-/Frond-Amplituden 0; dafür wird unveränderte Nullantwort geprüft. Blattbewegung bleibt aktiv. Keine nachträglich hinzugefügte Branch-/Frond-Windphysik.

## 28. Asset Sharing

Eine gecachte Asset-/RenderAsset-Struktur wird von vielen Instanzen genutzt. Der Vertragstest erzeugt 1.000 Instanzen mit einem Geometrieupload und einem Texturzugriff; nach dem Laden entstehen keine weiteren Dateizugriffe. Der World-Adapter teilt vorbereitete Ressourcen und Bildreferenzen. Shutdown-Prüfungen bestehen.

## 29. GPU Instancing Status

Kein GPU-Instancing aktiviert. Vorhandene Mesh-/Texturressourcen werden geteilt, die Draws bleiben pro Instanz. `VegetationInstanceBuffers=0` beschreibt das Fehlen solcher Buffer; es ist kein Nachweis für neu implementiertes Instancing. Keine Erweiterung in Gate B.

## 30. Performance Sanity

Keine Registry-/ZVEG-/GLB-Parser oder Dateileser im Framepfad. Instanzen aktualisieren Distanz-LOD und liefern Drawzustände; Mesh-/Texturvorbereitung findet beim Laden statt. 4.656 World-Frames in 83,2 Sekunden; 168.561 Teil-Draws über den gesamten Kartenlauf. Die Messung ist ein kurzer Funktions-/Ressourcencheck, kein FPS-Benchmark. Kein Fuzzer, keine Stresssuite, kein Test über zehn Minuten.

## 31. GR2 Regression

Nativer GR2-Reader und ZiiNAN-Animation bleiben Default, GPU-Skinning bleibt Default. Golden-/Safety-/Compatibility-/Warmup-/Render- und HairLOD-Prüfungen bestanden. Player, Mob, NPC, Hair, Weapon und Mount im kurzen World-Smoke; **CPU-Deformation 0, GPU-Fallback 0, GrannyFileReads 0**. Bekannte frühere Raw-Corpus-Grenzen werden durch Gate B nicht als behoben behauptet.

## 32. GLB Regression

E1-X Provider-, Material-, statische/animierte Diligent-Render- und WideIndex-Tests grün. Vegetation nutzt denselben bestehenden GLB-Provider; dessen normalen Pfad hat Gate B nicht geändert.

## 33. Offline Tool Regression

E2-X AssetTool Unit, ImportRoundtrip, CLI, RuntimeIsolation, ConvertRenderFixture und DiligentRoundtrip in Release/Debug bestanden. Vorhandene FBX/OBJ/DAE-Konvertierungsfälle bleiben grün. Assimp/meshoptimizer sind weiterhin ausschließlich im Offline-Tool, nicht im Client-Linkpfad.

## 34. Release Tests

**46/46 PASS**: Haupt-Fast-Gate 44/44 in **47,61 s**, anschließend zwei gezielte Renderer-Removal-Prüfungen 2/2 in **10,58 s**. Logs: `build-hx-clean/tests-Release.log` und `tests-Release-renderer.log`. Keine Wiederholung langer Suiten.

## 35. Debug Tests

**46/46 PASS**, **108,40 s**. Gleicher Umfang einschließlich Renderer.ProductionGpu und Renderer.NoLegacyArchitecture. Log: `build-hx-clean/tests-Debug.log`. Der längste einzelne Test ist GR2Render mit 80,28 s.

## 36. GCC/LP64

Neuer `build-hx-common`, GCC 12/Cygwin x86_64, LP64, Windows-Client/Renderer und Converter OFF: Build erfolgreich; **23/23 PASS in 16,32 s**. Runtime, Registry, Goldens, Header-/Source-Grenzen sowie GR2/GLB/Animation abgedeckt. Ein vorhandener GCC-Optimiererhinweis im GLB-Code bleibt dokumentiert; keine Vegetationsfehler.

## 37. Final Runtime Smoke

`build/hx/final-test-client` enthält die fertige Release-EXE und **237 hashgeprüfte compiled-Dateien**. Der normale Starter verwendet **keine Argumente**. Login → Charakterauswahl → Ingame, A1 nach der Brücke, Blätter/Wedel/Alpha/Wind und nah → fern → nah wurden vom Benutzer bestätigt: **„Ja, alles passt; mit X geschlossen“**. Kein zweiter Client-Build oder Reference-Schalter für diese Abnahme.

## 38. Multi-Map

Gleiche Release-EXE: **A1 → B1 → Trent/Wald → A1**, 4.656 Frames, 706 erzeugte Vegetationsinstanzen, Originalmaps und Actor-Animationen. Nah-/Fern-Screenshots je Phase vorhanden. `build/f2x/runtime/hx-b-default-multimap`: **Exitcode 0**, **83,2 s**. Argument war nur `--renderer-diagnostics`; alle produktiven Reader-/Vegetationsmodi kamen aus dem Default.

## 39. Resize/Minimize

Einmal Größe ändern und minimieren/wiederherstellen im finalen sichtbaren Client vom Benutzer bestätigt. Zusätzlich bestehen die D3D11-Backend- und Vegetations-GPU-Tests mit 0×0-Suspend, Restore und Resize. Keine neue Fenster-/Rendererarchitektur.

## 40. Shutdown

Manueller X-Shutdown: **Exitcode 0 nach 52,6 s**, `VegetationAssets=0`, `VegetationInstances=0`, Geometrie=0 im normalen Vegetationslog; 65.690 vorherige Vegetations-Draws. Ohne Diagnoseargument wird das umfangreiche `source-resource-audit.log` absichtlich nicht erstellt.

Vollständiger Ressourcenbeleg stammt deshalb aus dem instrumentierten Kartenlauf **derselben unveränderten EXE**: Assets/Instanzen/RenderAssets/Geometrie/InstanceBuffers=0; SourceTextures/SourceBuffers=0; MeshBindings/AssetDocuments/Animation-/GR2-/Collision-Ressourcen=0; VegetationFailures=0. Das wird getrennt von der manuellen Sichtbestätigung ausgewiesen.

## 41. Source Zero Audit

Produktive SpeedTree Includes/Typen/Aufrufe/SDK-Artefakte/CMake-Link-Anbindungen: jeweils **0**. `Vegetation.NoLegacyDependency` in Release, Debug und GCC bestanden. `AssetRuntime.NoGrannyDependency` ebenfalls grün. Historische neutrale `CGranny*`-Consumerbezeichnungen bleiben gemäß F34 erlaubt und sind keine Granny-SDK-Typen.

## 42. Runtime Zero Audit

Der Client enthält weder SDK-Loader noch SDK-Linkinput oder SDK-DLL-Import. `Runtime::Load` löst Legacy-Keys ausschließlich über validierte compiled-Pfade auf. Golden-I/O-Verträge verbieten SPT-Lesen, echte Maps laufen mit Default-ZiiNAN. Keine OS-Modulliste als Beleg behauptet: die Nachweise sind Source, Linkgraph, Binärprüfung und tatsächlich ausgeführte Pfade; der entfernte Reference-Zähler wurde nicht als künstliche konstante Null weitergeführt.

## 43. Legacy Map Compatibility

Alte `.spt`-Strings bleiben unveränderte Registry-Keys. Map-/Property-/Actor-/Eventdateien benötigen keine Massenmigration. Für den neuen Client müssen Registry und compiled ZVEG/GLB mitgeliefert werden; die fertig vorbereitete Test-/Releasekopie enthält sie bereits. Fehlende Deployment-Dateien führen zu den dokumentierten Fehlern, nicht zur alten Runtime.

## 44. Production Coverage

Erneut SDK-frei gegen gesicherte unabhängige Referenzdaten geprüft: **118/118**, Registryeinträge **118**, produktiv erreichbare Typen **85**, **production referenced unsupported = 0**. 12.819 Mapplatzierungen; 82 datenreferenzierte Typen plus drei Event-/Weihnachtsbaumtypen. Quelle: `build-hx-clean/hx-evidence/coverage.json` und bestehende Feature-Matrix. Die 118 compiled-Dateipaare wurden nicht neu verändert.

## 45. Converter Retention

Empfehlung **A: optionales Developer-/Migration-Tool behalten**. Kein Standard-Clienttarget benötigt es. Reproduktion: Converter ON, `M2_BUILD_ASSET_TOOL=ON`, expliziter externer SDK-Root mit `include/SpeedTreeRT.h` sowie Release-/Debug-Libraries unter `lib/`. Für den unabhängigen Tool-Build kann der Windows-Client OFF bleiben. Release-Toolbuild, Fehlerfälle und eine identische Konvertierung sind lokal belegt; kein neuer vollständiger 118er-Export erforderlich.

## 46. Remaining Limitations

Keine neue Vegetationsdarstellung, kein GPU-Instancing, kein PBR/Remaster/erweiterter Wind. Windows D3D11 ist validiert; Vulkan-/Android-Ausführung wurde in diesem Gate nicht erweitert oder als getestet ausgegeben. Die kleinen Bildsignaturen prüfen ausgewählte Referenzen, keine pixelgenaue Voll-Corpus-Abnahme. Bekannte Link-/Compilerwarnungen sowie `MarkManager invalid idx 0` und bestehende Damage-Diagnosen im normalen Spiel-Log bleiben; kein Vegetationsfehler. Vollständige Resource-Counter sind beim normalen Start weiterhin diagnosegesteuert.

## 47. Git Diff

Änderungen: alter SDK-/Runtime-/Renderer-Code und produktive Build-Anbindungen entfernt; Default-/World-Anbindung bereinigt; SDK-freie Goldens, Audits und Tests hinzugefügt; Converter isoliert; Start-/Smokehelfer und Bericht aktualisiert. Rund 5.300 alte Codezeilen und etwa 15,7 MB SDK-Libraries entfernt. Die drei SDK-Dateien sind außerhalb des Checkouts für das optionale Tool gesichert.

Kein Stage, Commit oder Push. Buildordner, EXEs/PDBs, Logs, extrahierte Daten, lokale SDK-Pfade und neu erzeugte Converterausgaben bleiben ungestaged bzw. außerhalb der versionierten Änderungen. Nur kleine Referenzfixtures und der Auditbericht sind als neue Quelldateien vorgesehen. Der Startcheckout war sauber.

## 48. GO/NO-GO

**H-X Gate B: GO.** Einziger produktiver Pfad ZiiNAN, kein SDK-Fallback, frische Release-/Debug-Builds ohne SDK, Source/Link/Binary-Zero-Audits, 46/46 + 46/46 + 23/23 Tests, 118/118 Coverage und 0 unsupported. Alpha-/Fog-/LOD-/Wind-/GR2-/GLB-Regressionsprüfungen sowie manuelle finale Abnahme bestanden; Shutdown 0, instrumentierte Ressourcen 0.

## 49. Architecture After H-X

```text
GR2 -> ZiiNAN GR2 Reader ----\
GLB -------------------------> ZiiNAN AssetRuntime -> Diligent -> D3D11
Legacy .spt key -> Registry --/          ^
                         ZVEG + GLB -> ZiiNAN Vegetation Runtime

Optionaler Offline-Converter + externes SDK -> GLB / ZVEG / Registry
Produktiver Client: Granny SDK = 0, SpeedTree SDK = 0
```

Der bestehende Vulkan-Architekturpfad bleibt außerhalb dieser Windows-Abnahme; kein Android-/Vulkan-Fortschritt wird daraus abgeleitet.

## 50. Recommendation Next Phase

**Nach Gate B STOP.** Erst einen neuen ausdrücklich freigegebenen Auftrag abwarten. Vegetation 2.0, F5-X, Visual Remaster/PBR, Android und 60/120-FPS-Menü wurden nicht begonnen.
