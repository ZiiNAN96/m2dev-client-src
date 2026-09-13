<!-- ZiiNAN: Bounded 5B implementation and measured acceptance, no later migration. -->
# Milestone 5B – Rüstungsmaterialien und gemeinsamer NPC-/Mob-Pfad

> Historischer Milestone-Stand. Aktuell seit [M12](renderer-milestone12.md): Diligent D3D11 ist im ON-Build der Default; Legacy wird explizit mit `--renderer=legacy-d3d9` gewählt. OFF-Builds bleiben Legacy-only. Frühere Testergebnisse und Auswahlbeschreibungen unten gelten für ihren damaligen Stand.

Stand 12.09.2026. Basis `757930e` (5A). **5B-Kernumfang implementiert und für die unten genannten Rüstungen, NPCs und Mobs getestet.** Release ON/OFF, Renderer-Tests, vollständige Suite und normale Ingame-Sichtprüfungen bestanden. Keine uneingeschränkte Freigabe sämtlicher NPC-Zustände: zwei im ersten normalen Lauf protokollierte Material-State-Ablehnungen bleiben als nicht reproduzierte Randfälle offen (Punkt 28). Keine weitere Migration begonnen.

Die [Analyse vor Änderungen](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/docs/renderer-milestone5b-analysis.md) beantwortet separat alle 22 Actor- und 13 Shape-Fragen anhand der Source und Originalassets. Keine Server-/Assetänderungen; Legacy D3D9Ex bleibt Standard. Genau ein Actor-Renderer für die drei Kategorien.

## 1. Player-/NPC-/Mob-Call-Chains

Gemeinsamer Renderweg: `CPythonApplication::RenderGame` → `CPythonCharacterManager::Render` → vorhandene sortierte Alive-/Dead-Listen → `CInstanceBase::Render` → `CGraphicObjectInstance::Render` → `CActorInstance::OnRender` → `ActorDrawScope(PART_MAIN)` → originale Diffuse-/Opacity-/Blend-/ADD-/MODULATE-Pässe → `CGraphicThingInstance` / aktueller LOD → `CGrannyModelInstance::RenderMeshNodeListWithOneTexture` → aktuelle MR-12-Materialpalette → `ApplyRenderState` → **lesender Submit-Callback** → `DiligentActorRenderer::Draw` → unverändert anschließender nativer D3D9-Draw und `RestoreRenderState`.

Die vorherige zweite 5A-Materialschleife entfällt. Es wird nicht zusätzlich über alle Actors gesucht; native Sichtbarkeit und tatsächliche Materialdraws bestimmen die Übergabe. Hauptstellen: [ActorRenderBridge.cpp](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/GameLib/ActorRenderBridge.cpp), [ActorInstanceRender.cpp](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/GameLib/ActorInstanceRender.cpp), [ModelInstanceRender.cpp](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterGrnLib/ModelInstanceRender.cpp).

## 2. Gemeinsame Klassen

`CInstanceBase`, `CActorInstance`, `CGraphicThingInstance`, `CGrannyLODController`, `CGrannyModelInstance`, `CGrannyModel`, `CGrannyMaterialPalette`; neue Übergabedaten bleiben im bestehenden `ActorModelSource` / `ActorInstanceData` und `DiligentActorRenderer`. Kein eigener NPC- oder Mob-Renderer, keine zusätzliche Weltfläche.

## 3. Unterschiede Player / NPC / Mob

Native ActorType-Werte: Player 6, NPC 1, Enemy 0, statisch gegen die echten Enums geprüft. Race/Shape/Material/Motion kommen weiterhin aus den vorhandenen Laufzeitdaten. Die 5B-Freigabe ist ausdrücklich begrenzt auf PC-Races 0–7, NPC-Races unter 20100 und Enemy-Races unter 8000. `IsPoly`, berittene Player, andere Typen und fremde Parts bleiben ausgeschlossen. Nur Race 0 ist als Player praktisch abgenommen; Freigabe ist kein Volltest aller Klassen.

Grund für die oberen Race-Grenzen: Das untersuchte CharacterAdd-Paket überträgt keinen verlässlichen separaten Pet-Typ. NPC-getypte Begleiter und andere hohe Sonderrassen werden deshalb nicht pauschal freigegeben. Diese konservative Grenze ist keine universelle Pet-Erkennung für beliebige Serverkonfigurationen.

## 4. Rüstungs-/Shape-Pipeline

`CInstanceBase::SetArmor` → Item-Vnum / `__ArmorVnumToShape` / SpecularPower → `CActorInstance::SetShape` → originale `warrior_m.msm` mit ShapeIndex, Model, SourceSkin/TargetSkin → RegisterModelThing / LODs / SetModelInstance → instanzbezogene Materialpalette. Native `SetMaterialData` / MR-12-Updates bleiben maßgeblich. Der Callback liest **nach** Anwendung des aktuellen Materials; keine stale 5A-Palette.

Die Skeletons sind nicht durchgängig identisch (75/76/75/75/74 Bones der untersuchten Hauptmodelle). Native Granny-Bindings übernehmen diese Unterschiede. Kein neuer Bone-/Skeletonvertrag wurde eingeführt.

## 5. Unterstützte Rüstungsmaterialien

Original-Diffuse- und Skin-Override-Texturen, vorhandene Materialfarbe und Beleuchtung, ursprünglicher SphereMap-Specular mit originaler TextureFactor-Stärke, Stage1-Matrix und Sampler. Beispiel: Shape 12 / `warrior_4-1.gr2`, der im 5A-Ingame-Test noch ausgeschlossen war. Keine PBR-, Normalmap- oder neue Lichtpipeline.

## 6. CPU-Skinning-Pfade

`CPythonCharacterManager::Deform` → `CInstanceBase::Deform` / `__CanRender` → `CActorInstance::INSTANCEBASE_Deform` → bestehende Thing-/LOD-Deformation → WorldPose / WorldMatrices → `CGrannyModel::DeformPNTVertices` → vorhandener Mesh-SSE2-/GrannyDeformVertices-Pfad → fertig deformiertes PNT vor dem bisherigen Unlock kopieren. **Kein Algorithmus verändert, keine zweite Deformation, kein GPU-Skinning.**

Notwendige begrenzte Ergänzung für Schmied, Soldat und Boss: rigide Teilmeshes **innerhalb desselben PART_MAIN-GR2** mit vorhandener Granny-PNT-Konvertierung vor dem bestehenden Section-Free sichern. Gemeinsamer VB: deformierter Bereich, danach lokale starre Vertices. Starre Draws benutzen ihre unveränderte native Bone*World-Matrix; deformierte Draws Actor-World. Keine separat registrierten Waffen-/Haar-/Attachment-Parts.

## 7. Vertexformate

Fertiges PNT332, 8 float / 32 Byte: Position, Normal, UV. Kein Vertexcolor-Attribut im untersuchten Actor-GPU-Format; Materialfarbe statt erfundener Vertexfarben. Nur dieses vorhandene Format und Modelle mit deformiertem Anteil werden angenommen. PNT2 / rein rigide Sonderactors bleiben außerhalb dieses Actorvertrags.

## 8. Indexformate

Native `uint16_t` / D3DFMT_INDEX16 unverändert, mesh-lokale Indizes und originale Materialgruppen-Indexoffsets. Rigid-BaseVertex wird um die Länge des deformierten Präfixes ergänzt; kein Umnummerieren oder Ersetzen der nativen Indizes.

## 9. Dynamic-Buffer-Strategie

Vorhandener 5A-Dynamic-VB mit DISCARD, immutable IB. Ein Upload pro neuer abgeschlossener ModelInstance-Pose, nicht pro Materialgruppe. Verschiedene Shape-/LOD-Größen erhalten die zugehörige Geometrie; keine Instancing-/Pooling-/Optimierungsarbeit. Upload-Bytes enthalten auch den lokalen Rigid-Suffix, `skinned_vertices` zählt nur tatsächlich deformierte Vertices.

## 10. Materialvarianten

Lesender [StaticObjectBridge.cpp](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/GameLib/StaticObjectBridge.cpp)-Capture übernimmt die tatsächlichen Material-, Licht-, Fog-, Sampler- und Passzustände. Der bestehende PNT-Renderer reproduziert eng begrenzt:

- Stage0 Texture × Diffuse, ursprüngliche Material-/Lichtfarbe.
- TextureFactor-Alpha bzw. konstantes Fade-Alpha.
- Stage1 ADD / MODULATE mit CURRENT und TFACTOR.
- Native SphereMap: CURRENT.rgb + CURRENT.a × Sphere.rgb, ursprüngliche Reflection-Koordinaten / Matrix / Sampler.

SphereMap und 4B-Kamera-Alpha nutzen einen gegenseitig ausschließenden zusätzlichen Texturbinding-Vertrag; statische Defaults bleiben erhalten. Native globale CStateManager-Zustände werden dafür nicht verändert.

## 11. Alpha Test

Echte Opacity-Gruppen von NPC Defence und Wolf laufen über den originalen OneTexture-Opacity-Pass: GREATER 0, native Stage0-Diffusetextur. Der Name TYPE_BLEND_PNT wird nicht mit tatsächlich aktiviertem Alpha-Blending verwechselt. Die ausgewählten Rüstungsassets haben keine solchen Gruppen; keine erfundene Alpha-Test-Rüstung als Beleg.

GPU-Abgleich erforderte den Vergleich des gerundeten 8-Bit-Alpha-Ergebnisses statt eines unquantisierten float gegen AlphaRef. Zehn neue Actor-Materialreferenzen werden gegen echtes D3D9 geprüft. Beim zusätzlichen linear gefilterten Sphere-/Factor-Alpha-Fall bleiben 610 einzelne Coverage-Randpixel, **0 Innenflächenfehler und 0 Alpha-Innenfehler**; die Fallprüfung erlaubt nur eine Einpixelgrenze in beiden Bildern und eine begrenzte Gesamtzahl. Ohne die Alpha-Korrektur entstehen breite Ränder und der Test fällt durch. Alle bisherigen 27 statischen und 27 dynamischen Fälle behalten ihre bisherigen Toleranzen.

## 12. Alpha Blend / Reihenfolge

Native SRCALPHA / INVSRCALPHA / ADD und ursprüngliches DepthWrite; globaler Wolf-Fade tatsächlich durchlaufen. Die vorhandenen Alive-/Dead- und Materialpass-Reihenfolgen bleiben bestehen. Keine neue globale Transparenzsortierung. Specular wird bei aktivem AlphaBlend wie im nativen Materialpfad nicht künstlich ergänzt.

## 13. Cull / Depth / Transformation

Native CullMode, ZENABLE, LESSEQUAL, ZWRITE, aktuelle Mesh-World/View/Projection und Normalmatrix. Originaler Actorpfad benutzt meist CULL_NONE. Keine eigene Typ-/Race-Skalierung: der Renderer übernimmt die tatsächlich fertigen nativen Matrizen; die vorhandene Transformfunktion wird nicht um eine neue Interpretation von SetScale erweitert. GPU-Tests prüfen positive/negative Skalierung, Rotationen, Indexoffsets und Terrain-/Objekt-/Actor-Tiefe in beiden Einreichungsreihenfolgen.

## 14. Animationsergebnisse

Originale MSA-/GR2-Motions und native RaceManager-Motionlisten: Idle, Run, Attack, Wolf Hit/Death, Fade, Hide/Restore, Sichtbereich verlassen/wieder betreten sowie Spawn/Despawn. Isolierter Test steuert nur vorhandene Python-Aufrufe; keine Animationsimplementierung. Kamera in vier Richtungen und verschiedene Abstände. Benutzer zum Diligent-Test: „sieht alles gut aus nur die gemischwarenhändlerin ist grün“.

Die grüne Händlerin ist **absichtlicher Testzustand** (`SetAddRenderMode(0, 0.3, 0)`), kein permanent geändertes Asset oder erzwungener Zustand im normalen Spiel. Eigene Hover-/Treffer-Farbzustände sind Bestandteil der angeforderten Materialparität.

## 15. Getestete Player-Shapes

Originale Item-Tabelle im Test ausgelesen, kein erfundener Shape/Specular-Wert:

| Reihenfolge | Shape / Item | Hauptmodell / Besonderheit |
|---|---|---|
| Grundkörper | 0 / 0 | warrior_novice, 2207 Vertices |
| Rüstung A | 3 / 11209 | warrior_nahan, 3025 Vertices, SphereMap |
| Rüstung B | 6 / 11239 | warrior_saja, 3631 Vertices |
| Rüstung C | 9 / 11269 | warrior_cheongrin, 3324 Vertices |
| Vorheriger 5A-Sonderfall | 12 / 11299 | warrior_4-1, 2582 Vertices, 8 Meshes |
| Skin-Override | 4 / 11219 | Nahan-Mesh, warrior_giryung.dds |
| Rückkehr | 0 / 0 | ursprünglicher Grundkörper erneut |

Der Player bleibt während dieser Folge derselbe native Actor; Shapewechsel dürfen Model-/Texturressourcen ersetzen. Andere Klassen/Kostüme sind nicht als vollständig getestet zu verstehen.

## 16. Getestete NPCs

Goods 9003 (Händlerin, inklusive ADD-Zustand), Defence 9002 (Rüstungshändler, Alpha-Test-Teilgruppe im Hauptmodell), Bank 9004, Blacksmith 20016 (1180 deformierte + 376 lokale starre Vertices). Unterschiedliche native Idle-Animationen / Texturen und vier Kamerarichtungen; komplette Originalmodelle innerhalb des abgegrenzten Hauptmodellvertrags.

## 17. Getestete Mobs

Stray dog 101 (682 Vertices), Wolf 102 (762, Alpha-Test-Zähne), Bear 110 (621), Bksoldier 301 (1330 deformierte + 79 starre), Orc lord 691 (1553 + 115, großer Boss). Starre Teile sind bereits Bestandteil ihrer Haupt-GR2; daraus folgt keine Freigabe separater Player-Waffen. Hit/Death reproduziert mit Original-Wolfmotions; keine Kampflogikänderung.

## 18. Sichtbares Weltbild und bekannte Sonderfälle

| System | Legacy-Start | Opt-in Diligent-Weltbild |
|---|---|---|
| Terrain / Splatting / Gebäude / Props | unverändert | bestehende migrierte Pfade |
| PC-Hauptkörper und geprüfte Rüstungen | unverändert | gemeinsamer 5B-Pfad |
| geprüfte NPC-/Mob-Hauptmodelle | unverändert | gemeinsamer 5B-Pfad |
| Login / Charakterauswahl | unverändert | weiterhin Legacy |
| separate Waffen / Schilde / Haare / Zubehör | weiterhin nativ | nicht migriert |
| Mounts / Pets / berittener Körper / Sondertypen | weiterhin nativ | ausgeschlossen |
| SpeedTree / Wasser / Partikel / Effekte / Schatten | weiterhin nativ | nicht migriert |
| UI / Text / Minimap / Nameplates / Damage Numbers | weiterhin nativ | nicht zusammengesetzt |

Kein vollständiger Hybrid-Compositor. Vorhandene Legacy-Systeme laufen weiter, werden aber nicht in die Diligent-Weltfläche eingeblendet. Keine Vollabdeckung des gesamten NPC-/Mob-Katalogs, hoher Sonderrassen oder aller LOD-Assets behauptet. Im automatischen Lauf keine ausgeschlossenen oder fehlgeschlagenen ausgewählten Body-/Materialgruppen.

Konkrete konservative Ausschlüsse im normalen Lauf: Jinno-Patrouillen 20340–20349 und Hauptmann 20354/20355 liegen über der vorab festgelegten NPC-Grenze. Das ist eine Scope-/Klassifikationsgrenze, **kein Nachweis eines technisch separaten Renderers**. NPC-getypte frei stehende Ponys 20029/20030 benutzen dagegen den freigegebenen normalen NPC-Hauptmodellpfad; das ist keine Unterstützung des Reit-/Sattel-/Mount-Actorpfads. Serverseitige Pet-Sonderkonfigurationen bleiben nicht pauschal abgenommen.

## 19. Dynamic Upload Bytes / Frame

Isolierter Diligent-Lauf: auf B1 beispielsweise 9 Actors, 12.920 hochgeladene Vertices (12.350 deformiert), **413.440 Byte pro Frame**. Stress mit 36 sichtbaren Figuren: 44.640–46.948 Gesamtvertices, 42.360–44.668 deformiert, **1.428.480–1.502.336 Byte pro Frame**. Unterschiedliche native Shape-/LOD-Größen sind im Messwert enthalten; keine Optimierung.

## 20. Actor-Drawcalls / Ressourcen / Performance

Gemischte B1-Szene: 1 Player + 3 NPCs + 5 Mobs, beispielsweise 19 Draws, 9 residente Geometrien, 13 Texturhandles. Belastung: 4 Player + 12 NPCs + 20 Mobs, 72 Draws, 36–37 residente Geometrien und 52–55 Texturhandles einschließlich eines nativen LOD-/Shapeübergangs. Dies sind Instanz-Handlezähler, **nicht** die Anzahl weltweit einzigartiger Texturdateien.

Diligent im isolierten Einzelmodellabschnitt meist 32–33 angezeigte Render-FPS, gemischt etwa 31, Stress 28–29. Legacy meist 61 Render-FPS. Der native Updatezähler läuft etwa 60/s. Gleichzeitig liefen Bibliothekstests, Vergleichsclient und andere vorhandene Clients: **kein kontrollierter Benchmark**. Je 75 Prozesssamples über 375,8 s: Diligent Private Bytes maximal 445,0 MiB / zuletzt 417,9 MiB, Handles maximal 1127 / zuletzt 1117; Legacy maximal 314,4 MiB / zuletzt 304,1 MiB, Handles maximal 787 / zuletzt 779. Map-/Kamerawechsel laden zusätzlich Weltressourcen. Kein monotoner Actor-GPU-Handleanstieg; kein allgemeiner Beweis langfristiger Leakfreiheit.

Normaler Diligent-Ingame-Lauf über mehrere besuchte Bereiche: 38 Samples, Private Bytes maximal 642,6 MiB / zuletzt 606,6 MiB, Handles maximal 1163 / zuletzt 1155. Mehr geladene Actors/LODs und Weltressourcen als im kleinen Fixture; nach Shutdown alle gemessenen Actor-/Objekt-GPU-Handles null. Der Diagnose-Nachlauf hatte 17 Samples, maximal 557,5 MiB, zuletzt 554,7 MiB; kein Vergleichsbenchmark bei identischer Route.

## 21. Mapwechsel

Automatischer Original-Mapload **A1 → B1 → A1**, mit Player, NPCs und Mobs, vorherigem Destroy und neuer Population. Beide Backends vollständig durchlaufen; `verify_actor_variants.ps1` bestätigt alle 31 Phasen, beide Mapfolgen, Modelle, Materialfälle und Lifetime. Das ist ein echter Client-Map-/Asset-/Ressourcenwechsel, **kein behaupteter serverseitiger Ingame-Warp**.

## 22. Resize

Automatische native/Diligent-GPU- und Presentation-Tests bestanden: wiederholte Größenwechsel, dynamische Posen, Backbuffer-/Depth-Prüfung, 640×480 / 800×600 sowie Presentation-320×240 / 1024×768. Ein frei per Fensterrand vergrößerter normaler Ingame-Client ist nicht separat als bestanden behauptet. Keine neue Fenstergrößenfunktion eingeführt.

## 23. Minimize / Restore

Automatische GPU-Suspend-/Restore-Prüfungen mehrfach bestanden. Benutzer bestätigt die Aufforderung zu dreimaligem Minimize/Restore des isolierten Diligent-Tests PID 58260 positiv (Hinweis nur auf den absichtlich grünen NPC). Isoliert Legacy PID 59900 separat bestätigt mit „Ja, war korrekt“. Normaler Diligent-Test PID 3700 einschließlich dreimaligem Minimize/Restore, Kamera, Laufen/Angreifen sowie Player/NPC/Mob mit „ist gut“ bestätigt.

## 24. Shutdown / Exitcodes

| Testlauf | Legacy | Diligent |
|---|---|---|
| Isolierte 31-Phasen-Originalszene | PID 59900, Exit 0, 375,8 s | PID 58260, Exit 0, 375,8 s |
| Normaler Start / Ingame, Originalpakete | PID 17608, Exit 0, 130,3 s | PID 3700, Exit 0, 190,4 s |
| Rein lesender Diagnose-Nachlauf | – | PID 51112, Exit 0, 85,2 s |
| Isolierte zusätzliche NPC-State-Probe | – | PID 42056, Exit 0, 105,2 s; Sinseon-Motionwahl siehe unten |
| Gezielter Doctor-/Sinseon-Test, originaler Idle | – | PID 18360, Exit 0, 80,2 s |

Keine fremden Prozesse beendet, kein erzwungener Shutdown.

## 25. Offene Ressourcen nach Shutdown

Isoliert und normal Ingame Diligent: `shutdown actor_geometry=0 actor_textures=0`, zusätzlich `object_geometry=0 object_textures=0`. Bereits nach Despawn im isolierten Lauf: keine Draws, Uploads, Geometrien oder Actor-Texturen; nach Respawn erneut normale Darstellung, nach finalem Löschen wieder null. Ursprüngliche Map-/Instanz-Teardown-Reihenfolge bleibt bestehen.

## 26. Build-Ergebnisse

Release ON erfolgreich (`build-on-probe.log`, anschließend Diagnose-Build). Release **OFF erfolgreich** (`build-off.log`), abschließendes **ON erfolgreich** (`build-final-on.log`). Abschließender CMake-Zustand ist ON; das ändert nicht den Legacy-Standard beim Programmstart. Vorhandene Buildwarnungen (u. a. fehlende Python-/zlib-PDBs) sind keine neu eingeführten Rendererfehler. Ein Diagnose-Buildaufruf scheiterte zunächst am falsch getrennten Windows-Skriptpfad, vor dem Compilerstart; korrigierter Aufruf erfolgreich. Normale Runtime-EXE nicht ersetzt; Tests benutzen private Kopien des fertigen Builds.

## 27. Vollständige Tests und Vergleich

Renderer ON **6/6 bestanden**. Neu: portable `Renderer.ActorPolicy` mit Kategorien-/Pet-/Mount-/Attachment-Ausschluss und Scope-Wiederherstellung. GPU: bisherige 27 statische + nun 37 dynamische Actor-Referenzfälle, echte D3D9-Fixed-Function-Referenz, multi-group-/owner-/NaN-/Längen-/Lifetime-Negativtests, gemeinsame Depth, Resize und Suspend/Restore. Neue Actor-Materialfälle: mittlere RGB-Abweichung **0,042–0,154 auf der 0–255-Skala**, keine Alpha-Innenfehler. Einzelne gefilterte Coverage-Kanten siehe Punkt 11. Kein Test deaktiviert.

Vollständige ON-Suite: **10/10 bestanden**, 472,92 s, einschließlich fullbench / fuzzer / zstreamtest / playTests. Danach wurde nur lesende Diagnose für abgewiesene Actor-Zustände ergänzt; mit diesem Endstand **Renderer OFF 4/4** (0,57 s) und **final ON 6/6** (2,75 s) bestanden. Die vollständigen Bibliothekstests wurden nach der reinen Logging-Ergänzung nicht nochmals wiederholt. Originalasset-Abnahme verwendet [actor_variants_smoke.py](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/tests/Renderer/actor_variants_smoke.py) und strikte nachträgliche Prüfung [verify_actor_variants.ps1](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/tests/Renderer/verify_actor_variants.ps1); diese Prüfung ist bestanden (`verify-probe.log`).

Sichtprüfungen erfolgten anhand beider Original-Asset-Testfenster und durch den Benutzer. Ein unverändertes [Diligent-Bild der gemischten B1-Szene](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build/milestone5b/probe/diligent-mixed-b1.png) ist abgelegt. Kein zeit-/posegleicher Screenshotvergleich je Einzelmodell erstellt; diesen optionalen Bildvergleich nicht als erledigt ausgeben. Numerischer Material-/Geometrievergleich beruht auf den separaten nativen GPU-Readbacks, die echte Granny-Animation auf dem Originalasset-Lauf.

## 28. Legacy-Ingame-Regression

Normaler Start ohne Renderer-Argument, Originalpakete. Benutzer bestätigt PID 17608 mit „sieht gut aus“ zur angefragten Prüfung von Rüstung, NPCs/Mobs, Animationen, Terrain/Gebäuden, Bäumen, Effekten und UI. Anschließend normal geschlossen, Exit 0. Ein Wechsel mehrerer Rüstungen wurde angeboten, aber nicht separat einzeln bestätigt; die vollständige Rüstungsfolge ist im isolierten Vergleich reproduzierbar. Fehlerlog nur das schon in 4B/5A dokumentierte `invalid idx 0`; keine gefundene neue Assertion-/Device-/Granny-Meldung.

Normale Diligent-Abnahme: Benutzer „ist gut“, anschließend Schließen mit „erledigt“ bestätigt. Tatsächlicher Player `warrior_4-1.gr2` mit **warrior_4-2.dds-Skin-Override**, zunächst nativer Fade, danach Stage3-SphereMap. Zahlreiche zusätzliche echte NPC-/Mob-Instanzen und LODs im Log. `syserr.txt` nur das bekannte `invalid idx 0`.

**Offener Randfall:** Gegen Laufende wurden bei Doctor 20018 und Sinseon 20095 zwei Materialzustände abgewiesen; beide Modelle waren zuvor im selben Lauf mit Fade und normalem Material nachweislich gezeichnet worden. Die bisherige Meldung protokollierte weder exakten State noch Dauer. Deshalb wurde ausschließlich eine begrenzte, lesende State-/Sampler-/Light-Diagnose ergänzt. Der normale Nachlauf PID 51112 mit erneuter Kamera-/Minimize-/Restore-Prüfung (Benutzer „erledigt“) zeigt keine Material-State-Ablehnung und endet mit Exit 0 / Ressourcen null. Er enthält jedoch andere NPCs am gespeicherten Spawnort; **keine gezielte Wiederholung derselben Doctor-/Sinseon-Situation**. Die Ursache und mögliche kurze Sichtauswirkung sind damit nicht geklärt. Keine behauptete Behebung, kein Nachweis dauerhafter Modellunsichtbarkeit. Für eine uneingeschränkte Freigabe dieser beiden Übergangsfälle ist eine gezielte Reproduktion mit den nun vorhandenen Zustandsdaten erforderlich.

Zusätzlich gezielte isolierte Prüfung beider konkreten Modelle (`npc-edge-native`): 13 Phasen mit Doctor / Sinseon, Wechsel vom und zum SphereMap-Rüstungskörper, native ADD-Hervorhebung, Fade und Rückkehr zu normalem Material. Sinseon wird mit `season1/npc/sinseon/sinseon.msm` und dem tatsächlich in `motlist.txt` eingetragenen `stand00.msa` geladen. Keine ausgeschlossenen Materialzustände, Fehlerlog leer, final keine Actor-/Objekt-GPU-Ressourcen, Exit 0. [Geprüftes Bild mit originalem Idle](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build/milestone5b/npc-edge-native/diligent-sinseon-native-idle.png). Der vorherige private Probeaufbau `npc-edge` hatte fälschlich das ebenfalls vorhandene `wait.msa` ausgewählt; seine Sinseon-Pose ist **kein gültiger Native-Idle-Abgleich**. Nur den korrigierten Folgeaufbau dafür heranziehen. Auch dieser gezielte Test reproduziert den ursprünglichen Ingame-Übergang nicht: modellbezogene Normal-/ADD-/Fade-Unterstützung nachgewiesen, genaue ursprüngliche State-Ursache weiterhin offen.

## 29. Empfehlung für 5C / STOP

Vor einer uneingeschränkten Folgefreigabe zuerst die zwei offenen NPC-State-Randfälle aus Punkt 28 gezielt reproduzieren. Danach nur nach gesondertem Auftrag einen klaren nächsten Part-Vertrag auswählen, beispielsweise separat angelegte Player-Waffe **oder** Haare, einschließlich Bone-Attachment-/Material-/Lifetime-Analyse. Keine Implementierung dafür begonnen. **Hier STOP; kein automatischer Übergang zu Waffen, Haaren, Mounts, Effekten, SpeedTree oder UI.**

## Git-Diff und Nachweise

Produktion: enger nativer Materialdraw-Callback, gemeinsame Kategorien, Hauptmodell-Rigid-Suffix, Original-Factor-/SphereMap-Zustände sowie differenzierte Messzähler. CStateManager, Granny-Runtime, Skinningalgorithmus, Animation, Original-Drawcalls, UI, Assets und Server bleiben unverändert. Eigene wesentliche Integrationsstellen sparsam mit exakt `ZiiNAN` markiert. Kein Commit, Push, Reset, Stash oder Clean. Vorhandene Runtime-Änderung `config/channel.inf` nicht angerührt.

Git-Diff: 18 bestehende Dateien mit 296 hinzugefügten und 101 entfernten Zeilen; zusätzlich acht neue Dateien (zwei Dokumente, sechs Test-/Prüfdateien). `git diff --check` ohne Fehler. Änderungen beschränken sich auf die oben beschriebenen Übergabe-/Material-/Diagnosestellen und Tests. Zeilenendewarnungen zur bestehenden Windows-Git-Konfiguration sind keine Whitespace-Prüffehler.

Nachweise: [build/milestone5b](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build/milestone5b), insbesondere `actor-inventory.jsonl`, `catalog-inventory.jsonl`, Build-/CTest-Logs, `probe` mit beiden isolierten Laufzeitlogs/Exitcodes/Ressourcen-CSVs, `normal-gate` und `normal-diagnostic` mit normalen Ingame-Läufen und Originalpaketen sowie `npc-edge-native` mit der gezielten Folgeprüfung. Alle von dieser 5B-Prüfung gestarteten Clients sind regulär beendet; vorhandene fremde Clients bleiben unangetastet.
