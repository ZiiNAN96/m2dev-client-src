# D1-X – ZiiNAN Asset Runtime Foundation + Granny Adapter

Stand: 14.09.2026. **D1-X: GO. Architektur, automatisierte und manuelle Abnahme bestanden.** Die Runtime-Grenze und ihre produktiven Consumer sind implementiert. Release 20/20, Debug 20/20 und GCC/LP64 6/6 sind grün. Die frische Release-EXE besteht den strengen Originalasset-/Map-/Lifetime-Smoke mit leerem Fehlerlog. Der Nutzer bestätigt die anschließende manuelle Prüfung der identischen EXE mit „alles perfekt“; deren Prozess endet ebenfalls mit Exitcode 0 und sämtlichen überwachten Ressourcen auf null. Nach D1-X STOP, keine Phase-E-Arbeit.

Granny/GR2 bleiben produktiv. Diligent D3D11, Phase-B-GPU-Skinning, der bestehende CPU-Deformer und die Matrixkonvention bleiben erhalten. Es wurden keine Formate konvertiert, keine zweite Formatimplementierung, kein neuer Skeleton-Solver und keine Phase-E-Arbeit begonnen. Kein Commit/Push ist Teil dieses Auftrags.

## 1. Granny Dependency Audit

Das vor Änderungen erstellte [Inventar](phase-d1x-granny-dependency-before.csv) enthält 921 Fundzeilen in 67 eigenen Dateien. Jede Zeile enthält Datei, Zeilennummer, textuellen Klassen-/Funktionskontext, Granny-Symbol, Zweck, A–J-Kategorie, Migrationshinweis und Originalzeile. Die [Audit-Notizen](phase-d1x-granny-audit-notes.md) erklären Zuständigkeiten und kritische Lebensdauer-/Layoutstellen. Kontextnamen sind Navigationshilfen, kein C++-Parsernachweis.

Erfasst wurden eigene Quellen, Tests, Android-/Build-Konfigurationen und importierte Targetdeklarationen; Buildausgaben und SDK-Inhalte sind ausgeschlossen. SDK-Header und statische Bibliothek werden separat aufgeführt. Das [Nachher-Inventar](phase-d1x-granny-dependency-after.csv) enthält im abschließenden Snapshot 1198 Fundzeilen in 77 Dateien. Mehr Fundzeilen entstehen insbesondere durch den expliziten Adapter und zusätzliche Referenztests; eine Symbolgesamtzahl ist kein Maß für öffentliche SDK-Kopplung.

## 2. Granny Include Baseline

Die [Before-JSON](phase-d1x-granny-includes-before.json) hält Zählregeln, Includezeilen und SHA-256-Dateihashes fest. Vorher existierten vier physische SDK-Includes: `EterGrnLib/StdAfx.h`, `Material.h`, `Deform.h`, `SkinningDataAdapter.h`.

Renderer, Game/Actor und World hatten bereits vorher jeweils **null direkte SDK-Includes**. Ihre Kopplung entstand durch transitive Legacy-Header. Diese bestehenden Nullen werden nicht als neuer Migrationserfolg ausgegeben. Die historische Spalte „Public Headers“ zählt konservativ alle eigenen Header einschließlich PCH und privater Adapterheader; die neue öffentliche AssetRuntime-API wird zusätzlich separat geprüft.

## 3. Asset Runtime Architecture

Die neutrale API liegt kompakt in `src/AssetRuntime/AssetRuntime.h/.cpp`. Private SDK-Implementierung und explizite Übergänge liegen unter `src/AssetRuntime/Granny/`. Die bestehenden `CGraphicThing`-/`CGranny*`-Klassennamen bleiben als Kompatibilitätsfassade erhalten.

```text
Pack-/Disk-Zugriff und CResourceManager
  -> CGraphicThing::OnLoad
  -> AssetRuntime::LoadModel(id, bytes, provider)
  -> AssetDocument / ModelHandle / AnimationHandle
  -> vorhandene Model-/Mesh-/Actor-/World-Fassaden
  -> neutrale Metadaten, CopyVertices/CopyIndices, AnimationInstance, MeshBinding
  -> Granny-Adapter
  -> bestehende Renderer-Daten, GPU-Skinning und Diligent D3D11
```

`CGrannyModel::CreateFromAsset` akzeptiert neutrale Handles ohne verpflichtenden nativen Model-Pointer. Mesh-/Materialkonstruktion und Upload verwenden die neutrale Schnittstelle. Optionale native Pointer sind explizite Referenz-/Diagnose-Interop und keine Voraussetzung für die produktive Konstruktion.

## 4. Ownership / Handles

Ein `AssetDocument` besitzt Dateidaten und stabile Metadaten. `AssetHandle`, `ModelHandle` und `AnimationHandle` halten die benötigte Dokumentlebensdauer. Die Resource-Cache-Semantik bleibt bei `CResourceManager`.

Meshes leihen Dokument-/Metadatenpointer vom eigenen Model-Handle; das Model zerstört seine Meshes vor Freigabe des Handles. Material-Paletten beziehen stabile Quelldeskriptoren aus demselben Dokument. `AnimationInstance` und `MeshBinding` besitzen Adapterzustand per RAII. Aktiv verwendete Clips bleiben während der zugehörigen nativen Controls gültig. Es gibt keinen neuen globalen Asset-Cache.

## 5. ModelAsset

Enthält Name, Meshes, Materialien, optionales Skeleton, Animationsreferenzen und Rigid/Skinned/Mixed-Klassifikation. `LoadAssetMeshes` erzeugt daraus vorhandene Renderknoten und Buffergrößen. Größen werden vor Übergabe an die vorhandenen `int`-Schnittstellen geprüft. Der explizite native Konstruktor bleibt für Referenztests erhalten.

## 6. MeshAsset

Enthält Vertex-/Indexanzahl, Topologie, Indexbreite, Quelllayout/-stride, Renderattribute, Materialbindungen/-gruppen, Bounds, Skinning-Metadaten und Two-sided-Markierung. `CGrannyMesh::CreateFromAsset` baut die vorhandenen Gruppen aus diesen Daten.

Uploads verwenden `AssetDocument::CopyVertices/CopyIndices` in bereits vorhandene Buffer. 16-/32-Bit-Quellindizes sind explizit beschrieben; die bestehende Ausgabe bleibt `TIndex`/UInt16. Nicht darstellbare Werte werden als Fehler propagiert. Die bisherigen Basis-Offsets, einschließlich des bestehenden UV2-Pfads, werden beibehalten.

## 7. MaterialAsset

Beschreibt Texturreferenzen, Diffuse-/Opacity-Stage, Alpha-Test-/Blend-/Depth-Write-Defaults, Culling und bestehende Specular-/Sphere-Map-Metadaten. Pro-Draw-Passzustände bleiben durch die vorhandenen Renderer-Snapshots bestimmbar.

Quelldeskriptoren speichern Originalpfade und getrennte ursprüngliche Matching-Texturen. Letztere erhalten die bisherige `IsIn`-Semantik auch bei Blend-Materialien. `CGrannyMaterial::GetAsset` liefert stabil gecachte, tatsächlich aufgelöste Resource-Pfade. Create/Copy/Texture-/Specular-Overrides aktualisieren diese Felder; Draws bauen keine Texturstrings neu auf. Die bisherigen Copy-Regeln für Culling/Specular bleiben bestehen.

## 8. SkeletonAsset

Enthält Namen, ursprüngliche Bone-Indizes, Parent-Indizes, lokale Bind-Daten und Inverse-Bind-Matrizen. `FindBone` und `ResolveAttachment` arbeiten auf der neutralen Sicht. Es gibt keine Sortierung, Umnummerierung oder eigene Skeleton-Evaluation.

## 9. AnimationAsset

Enthält Clipname, Dauer, Zeitschritt, Track-Group-Metadaten und benötigte Textevents. `CGrannyMotion` hält ein `AnimationHandle`; Metadaten werden neutral gelesen. Der native Bind-/Pointerzugriff bleibt ausdrücklich als Referenz-Interop bestehen. Sampling und Mixing erfolgen weiterhin durch Granny.

## 10. Pose Boundary

`PoseEvaluator`, `AnimationInstance`, `PoseRequest`, `PoseResult` und `PoseView` beschreiben Evaluation und geliehene Matrixausgaben. Die Session kapselt Clock, Motion-Wechsel, Blend-/Loop-/Speed-Steuerung, Root-Motion-Update, Bone-World-Matrizen und Composite-Pose.

Der Consumer fragt die Session ab. Der Granny-Adapter ruft die bisherigen SDK-Operationen auf. Welt-/Composite-Pose, Multiplikationsreihenfolge, Row-Vector-Darstellung und Bind-Konvention bleiben unverändert. Fehlgeschlagene Evaluation macht Renderdaten ungültig, bevor sie erneut eingereicht werden.

## 11. SkinningAsset

Stellt Bone-Namen, stabile Remaps, Bone-Bounds, Influence- und Byte-Layout-Metadaten bereit. `SkinningStreamView` beschreibt bestehende Vertex-/Index-/Remap-Daten ohne Besitzübernahme. Exaktes Quelllayout und zum Rendern benötigte Attribute bleiben getrennt: Die bestehende PNT-Konvertierung bleibt möglich, ohne unbekannte Quelllayouts als GPU-kompatibel auszugeben.

## 12. AttachmentBinding

Neutrale Bindings kennen Weapon, Shield, Hair, Armor, Mount/Rider und sonstige Attachments. Bone-IDs behalten die ursprüngliche Reihenfolge. Die vorhandenen Attachment-/LOD-Fassaden bleiben bestehen; Knochenabfragen und Matrixausgaben verwenden die Runtime-Grenze. Fehlende Bones bleiben explizit ungültig.

## 13. GrannyAssetProvider

Der Provider übernimmt das Memory-Loading und die Dateilebensdauer hinter `AssetRuntime::LoadModel`. Vorhandene Pack-, Disk-, Kompressions-/Verschlüsselungs- und Pfadbehandlung bleiben unverändert. `d:/ymir work/...`, `locale/...` und Pack-Pfade werden weiter verwendet.

Metadaten und erforderliche Upload-Snapshots entstehen vor den bisherigen Section-Frees. `ReleaseUploadData` gibt dieselben entbehrlichen Sektionen frei; spätere Uploadabfragen liefern `UploadDataReleased`. Die weiterhin benötigten deformierbaren Quelldaten bleiben für den bestehenden CPU-Fallback verfügbar.

## 14. Granny Model Adapter

Der private Adapter übersetzt Dateimodelle in neutrale Modell-/Mesh-/Material-/Skeleton-Daten und stellt vorhandene Vertex-/Indexkonvertierung bereit. `GrannyInterop.h` benennt die verbleibenden nativen Referenzeinstiege ausdrücklich. Ein nicht von Granny stammendes Handle darf beim optionalen nativen Zugriff `nullptr` liefern, ohne dass neutrale Model-/Meshkonstruktion daran scheitert.

## 15. Granny Skeleton/Animation Adapter

Granny bleibt Eigentümer der tatsächlichen Skeleton-Evaluation und Control-Logik. `GrannyAnimationInstance` kapselt diesen Zustand; `GrannyMeshBinding` kapselt die ursprüngliche Source-/Destination-Bindung. RAII ersetzt freiliegende Consumer-Lebensdauerpflichten. Die SDK-Control-Sequenzen wurden übertragen, kein neuer Mixer oder Solver eingebaut.

## 16. Static World Integration

Statische GR2-Objekte verwenden dieselbe Model-/Mesh-Grenze. `StaticObjectBridge` konsumiert neutrale Modellidentität und Materialpfade, während bestehende Geometrieuploads, Materialzustände und Draws erhalten bleiben. Buildings/Props, spezielle animierte oder transparente Things und Camera-Blocker bleiben in ihren vorhandenen Renderpfaden. `DungeonBlock` behält seine Kompatibilitätsunterklasse.

## 17. Actor Integration

Player, NPC, Mob, Boss, Mount, Body/Armor, Hair und Weapon-Attachments laufen über die bestehenden Resource-/Thing-/LOD-/Model-Instanzen. Deren produktive Datenquelle ist das Runtime-Handle; Sessions, Bone-Lookup, Bindings und Metadaten verwenden die neutralen Verträge. Game-/Actor-Aufrufsignaturen mussten dafür nicht grundsätzlich geändert werden.

## 18. GPU Skinning Integration

`SkinningDataAdapter::Extract(ModelHandle)` überträgt neutrale Metadaten und bestehende gewichtete Vertexdaten in die vorhandenen Phase-B-Strukturen. Remaps kommen aus `MeshBinding::BoneIndices`; Palettewerte aus der Composite-Pose. Die bestehende GPU-Vorbereitung und Diligent-Einreichung bleiben bestehen. GPU-Default, null CPU-Deformationen und null unerwartete Fallbacks sind im abschließenden Fast Gate und Originalasset-Runtime-Lauf bestätigt.

## 19. CPU Fallback Status

Der unveränderte Quelltext `EterGrnLib/Deform.cpp` wird jetzt im Target `AssetRuntimeGranny` gebaut. Die SSE-Funktion und der alternative native `GrannyDeformVertices`-Aufruf bleiben erhalten. Consumer übergeben neutrales Binding, Zielspan und Composite-Pose. Fehler werden propagiert; ein fehlgeschlagenes Runtime-Binding springt nicht heimlich auf einen zweiten alten Pfad zurück. CPU-Zähler bleiben erhalten.

## 20. Hair LOD

Near–Far–Near, gleiche Bone-Layouts, echte Remap-Wechsel und inkompatible Ziele bleiben unterschiedliche Fälle. Ein aktualisiertes Binding wird vollständig aufgebaut und validiert, bevor es das bisherige ersetzt. CPU- und GPU-Remaps stammen aus derselben Adapterbindung. **HairLodQuick: PASS, 14 repräsentative Fälle je Konfiguration**, Release 6,26 s / Debug 6,56 s. Zusätzlich bestanden die Instance-Remap-/CPU-Paritätsfälle und drei Near–Far–Near-Sequenzen mit Haar-/Rüstungswechseln im echten App-Lauf.

## 21. Resource Cache

`CResourceManager` bleibt für Pfadidentität, Duplikatvermeidung und bestehendes Nachladen/Freigeben zuständig. `CGraphicThing` integriert das Dokument in seine vorhandene Lebensdauer. Modellmetadaten und Materialübersetzungen werden nicht durch einen zusätzlichen globalen Cache dupliziert. Reale Load-/Unload-/Relog-/Map-Transition-Nachweise stehen unter Punkt 36.

## 22. Failure Semantics

Ungültige Eingaben/Handles, beschädigte Metadaten, fehlende Skeleton-/Attachment-Daten, unbekannte Uploadlayouts, zu kleine Zielbuffer, ungültige Indexbreiten, Provider-Mismatch und freigegebene Uploaddaten sind explizite Fehler. Bestehende fehlende Texturen bleiben im Resource-/Materialpfad behandelbar. Fehlende Pose-/Binding-/Deformationsdaten verhindern die Einreichung veralteter Actor-Daten.

Fehlgeschlagene Instanzerzeugung wird bis `LODController::AddModel` und `ThingInstance::SetModelInstance` propagiert. Fehlende Bone-/Composite-Matrizen liefern keinen Erfolg; WeaponTrace und der Effekt-Anheftungspfad prüfen dieses Ergebnis vor der Dereferenzierung.

Ein echter Laufzeitbefund erforderte die Unterscheidung `NoMatchingTracks`: Motion-Broadcasts eines Warriors erreichen auch die starre Waffe. Für Originalwaffe `00010.gr2` und Warrior `wait`/`run` liefern sowohl `GrannyFindTrackGroupForModel` als auch der ursprüngliche Control-Aufruf keinen passenden Track/Control. Die Runtime meldet diesen bisherigen normalen No-op ausdrücklich, ohne ihn als Evaluation-Fehler zu protokollieren. Nur ein nativer Null-Control **zusammen mit nachgewiesen fehlender Track-Gruppe** erhält diesen Status; echte passende-Clip-Fehler bleiben `EvaluationFailed`. Start, Change und Copy werden gegen die direkte SDK-Referenz geprüft; ein erfolgloser Copy beendet die Quellanimation nicht.

Native Referenzpfade werden ausschließlich über ihre vorhandenen expliziten Konstruktoren erreicht. Sie sind kein automatischer Rücksprung nach einem Runtime-Fehler.

## 23. Renderer Boundary

Die eigentlichen Renderer-APIs bleiben frei von SDK-Typen. Actor-/World-Brücken konsumieren neutrale Modell-/Mesh-/Materialdaten und die vorhandenen Phase-B-Ausgabeformen. Diligent, GPU-Bufferklassen und Shader werden dadurch nicht neu entwickelt. Die alten `CGranny*`-Namen in der Kompatibilitätsfassade bedeuten nicht mehr, dass ein nativer Pointer zur produktiven Konstruktion zwingend vorhanden sein muss.

## 24. Public Header Leak Test

`AssetRuntime.PublicHeaders` prüft die öffentliche Runtime-Grenze auf SDK-Namen/-Includes und Plattform-/Renderertypen. Der private Unterordner `Granny/` ist ausdrücklich aus dieser öffentlichen API ausgenommen. Zusätzlich kompiliert `AssetRuntime.Contracts` die neutrale API eigenständig. **Beide Tests PASS in Release, Debug und GCC/LP64.**

## 25. Granny Includes Before/After

Zählungen nach denselben Definitionen; Details in [After-JSON](phase-d1x-granny-includes-after.json).

| Physischer Include-Messwert | Vorher | Nachher |
|---|---:|---:|
| Direkte SDK-Includes insgesamt | 4 | 1 |
| Direkte SDK-Includes in allen eigenen Headern, inklusive privater Header | 4 | 1 |
| Direkte SDK-Includes in Renderer | 0 | 0 |
| Direkte SDK-Includes in Game/Actor | 0 | 0 |
| Direkte SDK-Includes in World/Static | 0 | 0 |
| Direkte SDK-Includes in öffentlicher AssetRuntime-API | nicht vorhanden | 0 |
| Cross-Module-Includes `EterGrnLib/...` insgesamt | 24 | 27 |
| Diese Legacy-Includes in Headern | 11 | 11 |
| Diese Legacy-Includes in Game/Actor | 7 | 7 |
| Diese Legacy-Includes in World/Static | 5 | 5 |

Die physischen SDK-Includes sind um **75 %** reduziert. Die verbleibende Stelle ist der private Einstieg `AssetRuntime/Granny/Native.h`. 13 Includes dieses privaten Einstiegs sind separat erfasst; Zentralisierung bedeutet ausdrücklich nicht null transitive SDK-Abhängigkeiten innerhalb des Adapters. Von den drei zusätzlichen `EterGrnLib/...`-Includes stammen zwei aus dem neuen Consumer-Test und einer aus `GrannyAssetProvider.cpp` für den unveränderten `Deform.h`-Vertrag. Die Legacy-Fassaden wurden nicht durch Umbenennen aus der Messung entfernt.

## 26. Adapter Parity Tests

`AssetRuntime.GrannyParity` vergleicht direkte SDK-Referenzdaten echter Assets mit der Runtime: Geometriezahlen, Materialien/Texturen, Bounds, Bone-Namen/-Parents/-Binddaten, Remaps, Animationen und Uploadwerte. Hinzu kommen 16-/32-Bit-Grenzen ohne partielle fehlerhafte Ausgabe, Byte-Parität der Vertices, sieben Pose-Samplezeiten, Blend-/Change-/Copy-/Root-Motion-Parität und Clip-Lebensdauer nach Cache-Unload. **PASS: Release 0,12 s, Debug 0,16 s.**

`AssetRuntime.ConsumerProvider` benutzt einen unabhängigen In-Memory-Provider ohne natives Granny-Modell. Er durchläuft die realen Model-/Mesh-/Material-/Motion-/Instance-Consumer, erzeugt statische Geometrie und rendert rigide sowie GPU-geskinnte Geometrie über Diligent. Pixel-Readback ist nicht schwarz; Bone-Ausgabe folgt der Provideranimation, CPU-Deformation/Fallback bleiben null und nach Clear sind Dokument-/Session-/Binding-/Renderer-Owner null. **PASS: Release 0,59 s, Debug 0,69 s.** Dieser Test belegt den Providerwechsel am produktiven Consumer, nicht nur an Metadaten.

## 27. Original Asset Tests

Alle acht repräsentativen Originalfixtures bestehen in Release und Debug. Bestehende kurze Skinning-/Hair-/Instance-Tests ergänzen die Adapter-Parität. Der Material-Instance-Test prüft neutrale Materialkonstruktion, aufgelöste Pfade, Override-/Copy-Isolation, Originalpfad-Matching und Entfernen von Overrides. Keine 583-Fälle-Wiederholung, Langzeit-Fuzzer oder automatisierte 15–30-Minuten-Testsitzung wurde gestartet.

| Rolle | Originalfixture unter `m2dev-client/assets` |
|---|---|
| Player | `PC/ymir work/pc/warrior/warrior_novice.gr2` |
| NPC | `NPC/ymir work/npc/goods/goods.gr2` |
| Mob | `Monster/ymir work/monster/wolf/wolf.gr2` |
| Boss | `monster2/ymir work/monster2/fire_dragon/fire_dragon.gr2` |
| Mount | `NPC/ymir work/npc/horse/horse_normal.gr2` |
| Hair | `PC/ymir work/pc/warrior/hair/hair_1_1.gr2` |
| Weapon | `item/ymir work/item/weapon/00010.gr2` |
| Static building | `Zone/ymir work/zone/n/obj/snow.m/snow-004-house2.gr2` |

Die Live-Fixture verwendet zusätzlich die tatsächlich konfigurierten Races 101 (Stray Dog), 691 (Orc Lord), 20101 (Pony) und Mount 20104. Die Parity-Fixtures und Live-Race-Zuordnungen werden bewusst getrennt angegeben.

## 28. Release Build

**Windows x64 Release: PASS, Exitcode 0.** Der vollständige abschließende Build mit Visual Studio 2022 x64 dauerte 99,12 s (inkrementeller vorhandener Buildbaum). Log: `build-c3x/d1x-release-build.log`.

Die bei der ersten Integration korrigierten Vorwärtsdeklarationen entsprechen den tatsächlichen sechs opaken `granny::...`-Typaliasen. Bekannte unabhängige Warnungen bleiben: LNK4099 für fehlende Python-/zlib-PDBs, LNK4075 bei bestehenden Linkoptionen sowie vorhandene C4834-Hinweise in `MarkManager.cpp` bei dessen Neubau. CMake meldet außerdem bestehende Third-Party-Deprecations. Der Build wird ausdrücklich nicht als warning-free bezeichnet.

## 29. Debug Build

**Windows x64 Debug: PASS, Exitcode 0, 73,39 s** im abschließenden vollständigen inkrementellen Build. Log: `build-c3x/d1x-debug-build.log`. Zusätzlich zu den oben genannten PDB-/Linkoption-Hinweisen bleibt das bereits in C1-X/C3-X dokumentierte LNK4098 bestehen: Debug-CRT trifft auf die vorhandene Release-Python-Static-Library. Kein D1-X-Compilerfehler bleibt offen.

Der erste Debug-Testlauf fand einen Cleanup-Fehler der neuen Material-Testfixture: Der vorhandene ResourceManager verlangt `DestroyDeletingList()` vor `Destroy()`. Der Test folgt jetzt diesem regulären Lebenszyklus; die Assertion wurde nicht deaktiviert. Das ursprüngliche Fehlerlog bleibt unter `build-c3x/d1x-debug-fastgate-first.log` erhalten. Abschließende Testwerte stehen bei der Fast-Gate-Abnahme.

## 30. LP64/GCC Status

**GCC 12.4 / LP64: PASS, 6/6 Tests in 9,06 s** am abschließenden öffentlichen API-Stand, einschließlich `NoMatchingTracks`. Lauf über den bestehenden Cygwin-/Common-Buildbaum; parallel lief der Windows-Neubau. Die neutrale Runtime wird als eigenes portables Target gebaut. Geprüft: PortableHeaders, PortableAbi, AndroidLifecycle, SourceBoundaries, AssetRuntime.Contracts und PublicHeaders. Logs: `build-c3x/d1x-gcc-build.log`, `build-c3x/d1x-gcc-tests.log`. Dies ist der vorhandene C3-X-Portabilitätsnachweis, kein kompletter Linux-Client-Build. Android-Level B–E wurde nicht begonnen.

## 31. Runtime Smoke

**Automatisierter App-Smoke PASS:** `build-d1x/runtime/d1x-fast-02`, Exitcode 0 nach 62,2 s, 3441 gerenderte App-Frames, A1 → B1 → A1, fünf Actors je Szene, Mount/Rider in B1, Haar-/Rüstungs-/Waffenpfad und drei Near–Far–Near-Sequenzen. Sechs erfolgreiche Nah-/Fern-Screenshots, konkrete Actor-/Static-/Blocker-Submissions, GPU-Default, leeres `log/syserr.txt` und sämtliche Shutdown-Zähler sind Pflichtassertionen des Harness.

Release-SHA256: `8A3783592DF06FA0AF6EF38E7BAB0C00CEFF6B1CDFF6A446D98A1355B78C5CF0`. Build-EXE, Smoke-Kopie und manuelle Kopie sind identisch. Das Harness kopiert Originalskripte aus `m2dev-client/assets/root`, ersetzt ausschließlich seinen eigenen Einstieg und erzeugt ein privates `root.pck`; Originalpacks, Originalkonfiguration und bestehende Runtime-Logs werden nicht bearbeitet.

**Manuelle Abnahme PASS:** Der Nutzer hat den angefragten Ablauf Login → Character Select → Ingame einschließlich Sichtprüfung von Player/NPC/Mobs, Weapon/Hair/Armor, verfügbaren Mounts, Buildings/Props, Trees/Effects/UI sowie Fensterbedienung und X-Schließen mit „alles perfekt“ bestätigt. Geprüft wurde die identische EXE unter `build-c3x/runtime/d1x-manual-01` mit `--renderer-diagnostics`. Die Sicht-/Bedienbestätigung stammt vom Nutzer; Prozessende und Logwerte wurden anschließend unabhängig ausgelesen.

Manueller Lauf: PID 64928, Exitcode 0, `GPUFrames=323521`, `AllCPUDeformationCalls=0`, `AllCPUDeformationVertices=0`, `GPUFallbacks=0`, `SkinPreparationFailures=0`; alle überwachten Ressourcen null. Die protokollierte Prozessdauer von 2819 s enthält die Wartezeit auf den Nutzer und ist keine gemessene aktive Testdauer oder zusätzlich angesetzte automatische Langzeitsitzung. `syserr.txt` enthält 142 vorhandene Damage-Effect-Diagnosezeilen sowie einmal `invalid idx 0` aus der unveränderten Gildenmarkenverwaltung (`MarkManager.cpp`). Die geprüften AssetRuntime-/D3D-/Renderer-/Lifecyclefehler-Muster haben null Treffer. Das manuelle Log wird daher nicht als leer bezeichnet; der Gildenmarkenhinweis wird getrennt von D1-X dokumentiert.

## 32. Static World Smoke

**Technischer Originalwelt-Smoke PASS.** Logs bestätigen starre Gebäude (u. a. `b1-middledam-05.gr2`), Props (`general_obj_ferryboat02.gr2`) und Camera-Blocker (`general_obj_suspension bridge01.gr2`, `b_general_obj_01_1.gr2`). Nah-/Fernbilder wurden geöffnet und geprüft: Actors, Waffe/Haar/Mount, Prop, Gebäude/Mauer, Bäume und Wasser sind sichtbar; keine offensichtliche zerfallene Geometrie. Der Nutzer bestätigt auch den angefragten visuellen Spieltest als unauffällig. Eine exakte Vorher-/Nachher-Bildparität oder separat protokollierte Camera-Blocker-Bediensequenz wird daraus nicht behauptet.

## 33. GPU Skinning Smoke

**PASS.** Startup: `Skinning=gpu`, `SkinningSelection=default`, ohne Skinning-Override gestartet. Strenger App-Smoke: `GPUFrames=17080`, `AllCPUDeformationCalls=0`, `AllCPUDeformationVertices=0`, `GPUFallbacks=0`, `SkinPreparationFailures=0`. Submissions belegen Player, NPC, Mob, Boss, Pony, Weapon, Hair sowie Mount/Rider. Der kurze CPU/GPU-Benchmark besteht in beiden Konfigurationen und prüft dieselben Draw-Counts, null CPU-Deformation/Vertexuploads im GPU-Modus und freigegebene Ressourcen. Die ausdrücklich ausgeführten CPU-Referenzteile behalten ihren ursprünglichen Deformer.

## 34. Resize/Minimize

**Manueller App-Test PASS:** Die angefragte Bedienprüfung mit Resize und Minimize/Restore ist durch „alles perfekt“ bestätigt. Zusätzlich besteht der vorhandene automatisierte `Renderer.DiligentD3D11`-Test für Resize/Backbuffer/Minimize/Restore in Release und Debug. Der manuelle Prozess endete anschließend regulär und ohne protokollierten Runtime-/Rendererfehler.

## 35. Shutdown

**Automatisches App-Ende und manuelles X-Shutdown PASS:** Beide Prozesse endeten mit Exitcode 0. In beiden abschließenden Audits sind `SourceTextures`, `SourceBuffers`, `SkinMeshes`, `BoneRemaps`, `BonePalettes`, `PrototypeGeometry`, `PrototypePalettes`, `StaticSkinMeshes`, `AssetDocuments`, `AnimationInstances` und `MeshBindings` sämtlich null. Der manuelle Prozess ist beendet; `exit.txt` und `renderer-startup.log` bestätigen Exitcode 0. Der statische SDK-Bestand wird nicht als Runtime-Leak gezählt.

## 36. Resource Lifetime

**PASS für Load/Unload, A1 → B1 → A1 und Shutdown im selben App-Prozess sowie Shutdown nach dem manuellen Ingame-Test.** Dokument-/Session-/MeshBinding-Zähler enden in beiden Läufen bei 0/0/0; alle Renderer-Owner ebenfalls null. Contract-/Consumer-Tests prüfen gehaltene Handles nach Root-Unload, Clear und vollständige Freigabe. Parity prüft zusätzlich, dass aktive/fadende Clip-Controls ihre Ressourcen behalten und nach Ablauf freigeben. Der optionale echte Relog wurde nicht gesondert bestätigt und wird nicht als eigener bestandener Nachweis ausgegeben.

## 37. Performance Sanity

Quellpfadprüfung: große Uploaddaten werden beim Laden in bereits vorhandene Buffer kopiert; Skeleton-/Material-Metadaten entstehen einmal pro Asset. Meshes leihen Daten vom Model-Handle. Materialpfade werden bei Create/Copy/Overrides aktualisiert. Sessions und Bindings entstehen bei Instance-/Binding-Aufbau, nicht pro Draw. Der unabhängige Consumer-Test zählt Provider-Vertexkopien und bestätigt, dass nach dem Laden keine weiteren Frame-Kopien erfolgen. GPU-Laufzeitzähler und der kurze vorhandene Benchmark bestätigen null CPU-Deformation/Vertexuploads im GPU-Pfad. Kein großer Benchmark und keine pauschale FPS-Steigerung werden behauptet.

## 38. Remaining Direct Granny Dependencies

| Bereich | Bewusst verbleibende Ausnahme |
|---|---|
| `AssetRuntime/Granny/Native.h` | Einziger physischer SDK-Include |
| `NativeTypes.h`, `LegacyVertexTypes.h`, `GrannyInterop.h` | Private SDK-Typaliase/Vertexdeklaration und ausdrücklich benannte Referenz-Interop |
| `GrannyAssetProvider.cpp` | Produktives Laden, Metadatenübersetzung, Sessions/Controls, Pose, Mesh-Bindings und Aufruf des vorhandenen CPU-Deformers |
| `EterGrnLib/Model.*` | Expliziter `CreateFromGrannyModelPointer`/`LoadMeshs`-Referenzpfad; optionaler nativer Pointer für alte Referenzaufrufe |
| `EterGrnLib/Mesh.*` | Expliziter nativer Meshkonstruktor und native Referenzuploads; produktiv gebundene Meshes verwenden Runtime-Copy und neutrale Bindings |
| `EterGrnLib/Material.*` | Expliziter nativer Materialkonstruktor/-vergleich für Referenzen; produktive Konstruktion und Originaltextur-Matching sind neutral |
| `EterGrnLib/Motion.*` | Native Referenzbindung/-pointer; produktive Metadaten und Playback-Übergabe verwenden Handles |
| `EterGrnLib/ModelInstanceModel/CollisionDetection/Skinning.*` | Klar getrennte Legacy-Referenzabfragen für Instanzen ohne Runtime-Handle; produktive Session-/Bounds-/Binding-Abfragen sind neutral |
| `EterGrnLib/SkinningDataAdapter.*` | Direkte native Extraction-Überladung für unabhängige Referenztests; produktive Überladung verwendet `ModelHandle` |
| `EterGrnLib/Deform.*`, `Util.cpp` | Unveränderter CPU-SSE-Quelltext und verbleibender nativer Rigid/Deform-Helfer |
| Tests und Builddeklarationen | Explizite SDK-Referenztests, importiertes Granny-Target und Adapterlinkage |

Die vollständigen Zeilen stehen im Nachher-CSV. Game/Actor/World/Renderer haben weiterhin keine direkten SDK-Calls/-Typen; Wrappernamen und projektinterne `GrannyCreateSharedDeformBuffer`-/`GrannyDestroySharedDeformBuffer`-Funktionen sind hiervon getrennt. Der SDK-freie App-Startup-Aufruf `ConfigureGrannyDiagnostics` erhält den zuvor deaktivierten SDK-Logcallback.

## 39. Git Diff

Der Umfang umfasst Runtime/Adapter, die gezielte EterGrnLib-Fassade, Actor-/Static-Brücken, Buildzuordnung, kurze Tests und Auditdokumente. `Deform.cpp` selbst bleibt unverändert. `AssetRuntimeGranny` besitzt die SDK-Linkabhängigkeit; `EterGrnLib` bezieht die Runtime-/Adaptertargets. Die vorhandene globale Projektstruktur wurde nicht vollständig neu verkabelt.

**`git status` und `git diff --check`: geprüft, sauberer Whitespace-Diff.** 39 vorhandene Dateien geändert (908 Einfügungen / 754 Löschungen), dazu 23 neue Dateien: 9 Runtime-/Adapterdateien, 7 Test-/Harnessdateien und 7 Audit-/Berichtsdateien. Insgesamt 62 Quell-/Dokumentpfade; die normale Diffstat zählt unversionierte neue Dateien noch nicht mit. `src/Renderer/` und der Inhalt von `Deform.cpp` sind unverändert. Build-/Test-/Runtime-Evidenz liegt ausschließlich in ignorierten Buildverzeichnissen.

Das Runtime-Repository hatte bereits vor dem Auftrag `config/channel.inf` und vorhandene Diagnostikdateien als lokale Änderungen; diese wurden nicht bearbeitet. Kein Commit, Push, Reset, Rebase, Restore, Clean oder Stash wurde ausgeführt.

## 40. GO/NO-GO for Phase E

**D1-X: GO.** Architekturziel, automatisierte Abnahme, manuelle Login-/Ingame-/Fensterprüfung und anschließende Shutdown-/Lifetime-Prüfung sind bestanden. **Phase E: NICHT FREIGEGEBEN und nicht begonnen.** Der Auftrag endet an der D1-X-Grenze; es erfolgt kein Commit/Push.

Architekturfrage: **JA, ein zweiter Provider kann implementiert werden, ohne Game/Actor/Renderer erneut grundsätzlich umzubauen.** Er implementiert `AssetProvider`/`AssetDocument`, `AnimationInstance` und `MeshBinding`. Model-/Mesh-/Materialkonstruktion verlangt keinen Granny-Pointer. Die vorhandenen Aufrufer konsumieren dieselben neutralen Metadaten, Uploads, Sessions, Remaps und Poseausgaben. Die Providerwahl am Ladeeingang bleibt eine schmale Resource-Integration. `AssetRuntime.ConsumerProvider` belegt diesen Anspruch am echten statischen und GPU-geskinnten Consumer mit Render-Readback in Release und Debug.

Ein Testprovider im Speicher ist ausschließlich ein Vertragstest, kein glTF-/GLB-/Ozz-/Assimp-Loader. Nach D1-X wird gestoppt, auch wenn die Abnahme anschließend GO ergibt.

### Abschließende Fast-Gate-Evidenz

| Prüfung | Ergebnis | Dauer | Evidenz |
|---|---|---:|---|
| Windows x64 Release, vollständiger inkrementeller Build | PASS | 99,12 s | `build-c3x/d1x-release-build.log` |
| Windows x64 Debug, vollständiger inkrementeller Build | PASS | 73,39 s | `build-c3x/d1x-debug-build.log` |
| Release Fast Gate | 20/20 PASS | 9,91 s | `build-c3x/d1x-release-fastgate.log` |
| Debug Fast Gate | 20/20 PASS | 11,56 s | `build-c3x/d1x-debug-fastgate.log` |
| GCC/LP64 Fast Gate | 6/6 PASS | 9,06 s | `build-c3x/d1x-gcc-tests.log` |
| Strenger Originalasset-App-Smoke | PASS, Exit 0, leeres Fehlerlog | 62,2 s | `build-d1x/runtime/d1x-fast-02/` |
| Login / Ingame / Fensterbedienung / X-Ende | Nutzerbestätigung PASS; Exit 0, Ressourcen 0 | aktive Testdauer nicht separat gemessen | `build-c3x/runtime/d1x-manual-01/` |

Die beiden Windows-Gates enthalten je neun Platform-Tests, sieben vorhandene Renderer-Tests und vier neue Runtime-Tests. Die summierten finalen Test-/Smoke-Laufzeiten betragen rund 93 Sekunden, ohne Builds. Wiederholungen während der Integration erfolgten nur nach Codeänderungen oder konkreten Fehlerhinweisen; keine Langzeitsuite wurde gestartet.

Reproduktion vom Source-Repository aus mit dem vorhandenen Windows-x64-Buildbaum und lokal verfügbaren Originalassets:

```powershell
cmake --build build-c3x/windows --config Release --parallel 8
cmake --build build-c3x/windows --config Debug --parallel 8
$fastGate = '^AssetRuntime\.|^Platform\.|^Renderer\.(StartupOptions|ResourceSource|DiligentD3D11|SkinningInstance|NoLegacyArchitecture|HairLodQuick|SkinningBenchmark)$'
ctest --test-dir build-c3x/windows -C Release --output-on-failure -R $fastGate
ctest --test-dir build-c3x/windows -C Debug --output-on-failure -R $fastGate
cmake --build build-c3x/cygwin-common --parallel 4
ctest --test-dir build-c3x/cygwin-common --output-on-failure
./tests/AssetRuntime/run_runtime_smoke.ps1 -Name d1x-next-fresh-run -BuildDirectory ./build-c3x/windows
```

Jeder Smoke benötigt einen bisher unbenutzten Namen. Das Harness überschreibt keine vorhandene Evidenz. Es startet ausschließlich seine eigene Clientkopie, prüft die EXE-Identität und begrenzt einen fehlerhaft hängenden automatischen Lauf auf 150 Sekunden. Die manuelle Kopie hat keinen automatischen Abbruch.
