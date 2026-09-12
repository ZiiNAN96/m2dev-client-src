<!-- ZiiNAN: Evidence-backed 5A implementation and acceptance boundaries. -->
# Milestone 5A – animierter Granny-Grundkörper

Stand: 12.09.2026. Basis `7f038ef`. **Milestone 5A für den ausgewählten animierten Krieger-Grundkörper abgeschlossen.** Isolierter Diligent-Test, normale Diligent-Ingame-Abnahme, normale Legacy-Regression und der abschließende Mount-/Map-Gegenlauf sind bestanden. Die unten genannten Grenzen bleiben ausdrücklich bestehen. Keine automatische Fortsetzung mit 5B. Legacy D3D9Ex bleibt Default, Diligent ist nur mit `--renderer=diligent-d3d11` aktiv.

Die [Analyse vor den Änderungen](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/docs/renderer-milestone5a-analysis.md) enthält alle 29 angeforderten Source-Befunde und die minimale Architekturentscheidung. Die folgenden 26 Punkte unterscheiden automatisierte Nachweise, manuelle Rückmeldung und offene Prüfungen.

## 1. Ausgewählter Actor

Originaler männlicher Krieger, Race 0, Shape 0, `d:/ymir work/pc/warrior/warrior_novice.gr2`, 75 Bones. Drei vollständig deformierbare Body-/Face-Meshes, keine zusätzlichen Haare/Waffen. Das Originalmodell gehört zum echten Spiel; der isolierte Test lädt die originalen MSM/MSA/GR2 und originalen Maps ohne Serveranmeldung. Kein Test-Dummy als Ersatz für den Granny-Nachweis.

## 2. Actor-Call-Chain

`CPythonApplication::RenderGame` → `CPythonCharacterManager::Render` → sortierte Alive-/Dead-Liste → `CInstanceBase::Render` → `CGraphicObjectInstance::Render` → `CActorInstance::OnRender`. Der normale native `RenderWithOneTexture` bleibt bestehen. Direkt im selben sichtbaren Diffuse-Pass ergänzt `SubmitAnimatedActorBody` ausschließlich `PART_MAIN`.

## 3. Granny-Call-Chain

`CGraphicThing::OnLoad` → bestehendes Granny-Dateilesen → `LoadModels`/`LoadMotions`; bestehende Race-/LOD-/ModelInstance-Verwaltung. Pro sichtbarer Instanz: `CActorInstance::INSTANCEBASE_Deform` → `CGraphicThingInstance::OnDeform` → aktueller `CGrannyLODController` → `CGrannyModelInstance::Deform` → `UpdateWorldPose`/`UpdateWorldMatrices` → `DeformPNTVertices`. Sampling, Skeleton, Bones, Zeitsteuerung und beide CPU-Deformationsimplementierungen bleiben unverändert.

## 4. CPU-Skinning-Punkt

In [ModelInstanceUpdate.cpp](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterGrnLib/ModelInstanceUpdate.cpp) unmittelbar nach dem ursprünglichen `DeformPNTVertices(pntVertices)` und vor dem ursprünglichen VB-Unlock. Ein enger `ActorDeformScope` bezeichnet genau die aktuelle Hauptkörper-ModelInstance, nicht deren Attachments. Die fertigen Bytes werden in instanzeigenen Speicher kopiert. Revision und Frame-ID verhindern die Verwendung einer alten oder fremden Pose; kein zweites Skinning und kein GPU-Readback.

## 5. Vertexformat / Normals

Originales PNT: Position float3, bereits CPU-deformierte Normale float3, UV0 float2, **32 Byte pro Vertex**. Keine Vertexfarbe, Bone-Indizes oder Gewichte im Rendererformat. Vorhandene Actorfarbe stammt aus dem nativen Material, nicht aus neu erfundenen Vertexattributen.

## 6. Indexformat

Originale mesh-lokale **uint16-Indizes**, zusammen 6.804 Indizes / 13.608 Byte. `CaptureActorSource` kopiert sie über die bestehenden Mesh-/Granny-Funktionen vor der unveränderten Section-Freigabe in `CGraphicThing::LoadModels`. Die vorhandenen Mesh-/TriGroup-Offsets und BaseVertex-Werte bleiben erhalten.

## 7. Dynamic-Buffer-Strategie

[DiligentActorRenderer](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/Renderer/DiligentActorRenderer.cpp) verwendet die vorhandene 4B-PNT-/Materialimplementierung in einer separat gezählten Instanz. Ein VB je aktiver ModelInstance: `USAGE_DYNAMIC`, `CPU_ACCESS_WRITE`, Vollupload mit `MAP_WRITE/MAP_FLAG_DISCARD`. IB: `USAGE_IMMUTABLE`, keine CPU-Schreibrechte. Ein Vertexupload je neuer fertiger Revision, nicht pro Materialgruppe. Kein Ringbuffer, Batching oder GPU-Skinning. Native D3D9-Lockflags und Drawcalls unverändert.

## 8. Tatsächlich gemessene Uploads

Im korrigierten Original-Map-Test: **1 sichtbarer Actor, 1 Upload, 2.207 Vertices, 70.624 Byte pro gezeichnetem Frame**. Bei Hide und außerhalb des Sichtbereichs: 0 sichtbare Actors, 0 Uploads, 0 Bytes. Keine FPS-/Performancebehauptung aus diesen Messwerten. Die Diagnose steht in `terrain-renderer.log` als `actors_visible`, `actor_uploads`, `actor_vertices`, `actor_bytes`.

## 9. Drawcalls

**3 Body-Drawcalls pro sichtbarem Frame**, zwei unterschiedliche Diffusetexturen. Im ursprünglichen kompletten Actorlauf (`probe2`) bleibt vor dem ersten Mapwechsel `actor_index_uploads=1`, nach Neuanlage auf B1 2, nach Rückkehr auf A1 3. Der abschließende Gegenlauf (`gate-after`) hat wegen des zusätzlichen Mount-/Absteigefalls vier getrennte Body-Lebenszyklen und genau vier IB-Uploads; der Mount selbst erzeugt keinen. Im normalen korrigierten Ingame-Lauf bleibt der Wert während der gesamten Sitzung 1. Kein Indexupload pro Frame. Der GPU-Test prüft zusätzlich zwei Actors mit drei Materialgruppendraws, ohne Actors oder Uploads mehrfach zu zählen.

## 10. Materialpfad

Aktuelle ModelInstance-Materialpalette, originale TriGroup-Zuordnung, native Actor-Materialfarbe. Die lesende 4B-State-Erfassung übernimmt Diffuse-Modulation, Alpha-Test, Cull, Depth und Fog. Der Actor-Normalpass setzt Cull NONE; es wird nicht versehentlich Weltobjekt-CW erzwungen. Two-Sided bleibt erhalten. Der erste Pfad akzeptiert nur vollständig deformierbare diffuse PNT-Bodymodelle ohne Opacity-/Specular-Sondermaterialien. Zwischenalpha-, Add- und Modulate-Modi werden ausdrücklich protokolliert und nicht angenähert.

## 11. Texturen

Vorhandene Pack-/Resource-Auflösung und `LoadStaticObjectTextureFile`; keine zweite Character-Texturpipeline. Originale Body- und Face-DDS einschließlich UV/Mips. Die Instanz hält genau ihre verwendeten Texturhandles. Keine Normal Maps oder neue Materialassets.

## 12. Transformation / Licht / Depth

Bestehende finale Mesh-Worldmatrix, aktuelle Kamera-View-/Projection und inverse transponierte World*View-Normalmatrix. Keine erneute Bone-Multiplikation und keine zweite Y-Umkehr. Originales Character-Directional-Light sowie das gegebenenfalls aktiv gebliebene Intro-Punktlicht1 werden gelesen, keine Lichtquelle hinzugefügt. Terrain, statische Objekte und Actor benutzen dasselbe D3D11-Depth-Ziel; native Referenztests prüfen Front-/Hintergrund in beiden Draw-Reihenfolgen, nicht nur anhand der Objektfarbe.

## 13. Animationstest

Originale `general/wait.msa`, `run.msa`, `attack.msa` mit den zugehörigen GR2-Animationen. Der isolierte Test ruft die bestehenden Motion-APIs auf: Idle, Run mit Positionsbewegung und wiederholtes `PushOnceMotion` für Attack. A1 und B1 werden durchlaufen, Rotation 0/90/180 Grad. Benutzer bestätigt am isolierten Diligent-Test: „sieht gut aaus“. Die ausdrückliche Rückfrage zum normalen Diligent-Ingame-Lauf nach der Filterkorrektur (PID 58424) – Grundkörper sichtbar, Stehen, Laufen, Angriff und Minimize/Restore korrekt – beantwortet der Benutzer mit **„Ja, alles korrekt“**. Keine neue Animation, kein Ersatzparser, kein Skeleton-Rewrite. Zusätzlich 27 unterschiedliche dynamische PNT-/Normalen-Posen im GPU-Pixelvergleich gegen echte D3D9-Fixed-Function-Draws.

## 14. Spieler-Unterstützung

Der unterstützte Vertrag ist der nicht berittene, nicht verwandelte PC-Hauptkörper im normalen bzw. voll deckenden Blend-Pass mit dem beschriebenen Material-/Meshvertrag. Repräsentativ geprüft: Krieger Race 0 / Shape 0. Andere passende PC-Körper können denselben Pfad benutzen; das ist **keine Abnahme aller Klassen, Rüstungen und Kostüme**. CPU-Übergabe, Renderaufruf und Bridge berücksichtigen nach dem Ingame-Befund zusätzlich die vorhandene `IsPoly()`-Klassifikation; `IsPC()` allein genügt nicht.

## 15. NPC-Unterstützung

Nicht aktiviert. Gemeinsamer Actor-Kern ist vorhanden, doch ein eigener NPC-Material-/Modelbestand wurde nicht für 5A abgenommen. NPCs laufen weiterhin im nativen Pfad und werden nicht in das Diligent-Weltbild zusammengesetzt.

## 16. Mob-Unterstützung

Nicht aktiviert; dieselbe bewusste Grenze wie bei NPCs. Keine Monster-, Polymorph-, Mount- oder Pet-Erweiterung.

## 17. Verbleibende Sonderpfade / tatsächliches Weltbild

| System | Legacy-Default | Diligent-Weltbild nach 5A |
|---|---|---|
| Terrain / Splatting | unveränderter Pfad | bestehend migriert |
| Unterstützte Buildings / Props | unveränderter Pfad | bestehender 4B-Pfad |
| Abgenommener PC-Grundkörper | unveränderter Granny-/D3D9-Pfad | neue CPU-PNT-Übergabe |
| Separate Haare / Waffen / sonstige Attachments | weiterhin nativ | nicht migriert |
| NPCs / Mobs / Mounts / Pets | weiterhin nativ | nicht migriert |
| SpeedTree / Effekte / Wasser / dynamische Schatten | weiterhin nativ | nicht migriert |
| UI / Text / Nameplates / Minimap | weiterhin nativ | nicht zusammengesetzt |

Specular, Opacity, gemischte rigide/deformierte Bodymodelle und partielle Fade-/Add-/Modulate-Darstellung bleiben ausgeschlossen. Auch Auswahl-/Hoverzustände können einen solchen Sonderrender-Modus benutzen; ein dann absichtlich ausgelassener Body wird diagnostisch gemeldet. Keine vollständige Hybrid-Komposition: Login und Charakterauswahl bleiben Legacy, die experimentelle Diligent-Weltfläche zeigt nur ihre unterstützten Systeme. Der erste Weltframe dient als bisherige Terrain-Grenze; die Actorfreigabe folgt der letzten abgeschlossenen Weltpräsentation.

## 18. Mapwechsel / Modell- und LOD-Grenzen

Automatisch **A1 → B1 → A1** mit echten vorhandenen Maploads, Löschen und Neuerzeugen der Actorinstanz: bestanden. Das ist kein behaupteter serverseitiger Warp. Fern-/Nahkamera wurde geprüft; beim ausgewählten zentrierten Grundkörper wurde **kein Wechsel auf ein anderes LOD-Modell beobachtet**. Die native LOD-Zentrums-Ausnahme und vorhandene Modellregistrierung bleiben unverändert. Ein echter Wechsel auf eine andere Rüstung/Modelvariante ist nicht als abgenommen zu werten.

## 19. Resize

Automatischer GPU-/Presentation-Test mit wiederholten Größenwechseln (640×480, 800×600, 320×240, 1024×768); dynamische Posen, Indexwiederverwendung und gemeinsames Depth-Verhalten bleiben korrekt. Ein frei per Fensterrand vergrößerter realer Ingame-Client wurde nicht separat als bestanden behauptet.

## 20. Minimize / Restore

GPU-Suspend/Restore mehrfach bestanden. Benutzer bestätigt die Aufforderung zum dreimaligen Minimize/Restore im Diligent-Actor-Test (PID 14564) einschließlich Körper/Texturen/Animationen positiv. Auch zweimaliges Minimize/Restore im normalen Diligent-Ingame-Lauf nach der Filterkorrektur (PID 58424) ist mit „Ja, alles korrekt“ bestätigt.

## 21. Shutdown / Exitcodes

| Lauf | Legacy | Diligent |
|---|---|---|
| Erster diagnostischer Lauf, noch ohne Actorfreigabe | PID 54240, Exit 0 | PID 54104, Exit 0 |
| Korrigierter kompletter Actor-/Maplauf | PID 41948, Exit 0 | PID 14564, Exit 0 |
| Normaler Start mit Originalpaketen | PID 59948, Exit 0 | Windows-Start durch Benutzer abgebrochen, kein Client-Exitcode |
| Ingame mit Rüstung / Löwen-Reittier, vor Filterkorrektur | – | PID 62208, Exit 0, 175,4 s; fehlender Rüstungskörper / fehlerhafte Mountübernahme |
| Gezielte isolierte Mount-Reproduktion mit altem Build | – | PID 46520, Exit 0, 55,1 s; erwarteten Filterfehler nachgewiesen |
| Normale Ingame-Nachprüfung nach Filterkorrektur | PID 48748, Exit 0, 185,4 s | PID 58424, Exit 0, 165,5 s |
| Abschließender isolierter Mount-/Actor-/Map-Gegenlauf | PID 48432, Exit 0 | PID 59008, Exit 0 |

Der abgebrochene Starter lieferte Fehlercode 1; das ist **kein Diligent-Clientcrash und kein bestandener Ingame-Test**. Kein Testprozess wurde zwangsweise beendet.

## 22. Resource Lifetime / Stabilität

Der ursprüngliche vollständige Original-Map-Test (`probe2`) lief rund **288 Sekunden**. Sichtbar → Hide → Restore → außerhalb Sichtbereich → Rückkehr → Löschen → neue Map/Instanz → Rückkehr → finale Löschung. Nach Löschung `actor_geometry=0`, `actor_textures=0`; beim Shutdown zusätzlich `object_geometry=0`, `object_textures=0`. Beide isolierten Fehlerlogs leer. Keine dort beobachtete Assertion, Device-Fehlermeldung oder Crash.

58 Speicher-/Handle-Samples pro Backend: Legacy Private Bytes maximal 300,0 MiB, zuletzt 291,3 MiB; Diligent maximal 409,0 MiB, zuletzt 396,6 MiB. Diligent im ersten stabilen Mapabschnitt etwa 376,7–376,8 MiB; spätere Map-/Kamerabereiche laden zusätzliche Weltressourcen. Kein fortlaufender Actor-GPU-Ressourcenanstieg; die Handles gehen bei Actor-Löschung auf null. **Kein Beweis für allgemeine Leakfreiheit oder stundenlange Stabilität** aus diesem begrenzten Test.

Abschließender erweiterter Lauf (`gate-after`): **18 Zustände à 18 Sekunden, rund 324 Sekunden** je Client, mit echter nativer Mount-Erzeugung und anschließender Neuerzeugung des Grundkörpers. Beide Clients normal beendet, beide Fehlerlogs leer. Diligent: 65 Prozesssamples bis 320,6 Sekunden, Private Bytes maximal 411,9 MiB, zuletzt 395,9 MiB; Prozesshandles maximal 1.127, zuletzt 1.119. Während der Mount-Phase keine Actor-VB-/Texturressourcen, danach normaler Body erneut sichtbar. Nach finalem Löschen Actorzähler null, beim Shutdown zusätzlich Weltobjektzähler null. Die abschließende Logprüfung bestätigt alle vier Body-Lebenszyklen und A1 → B1 → A1. Für diesen Gegenlauf gibt es keine Legacy-Speichersamples, weil der Legacy-Prozess während der verzögerten Windows-Startfreigabe des Diligent-Prozesses bereits endete; sein Exitcode und seine vollständigen Lifecycle-Logs sind dennoch erfasst. Die frühere Legacy-Messreihe bleibt separat ausgewiesen.

## 23. Builds

Release mit Diligent: erfolgreich. Release mit `M2_ENABLE_DILIGENT_D3D11=OFF`: erfolgreich. Abschließender erneuter ON-Build: erfolgreich, alle fünf Renderer-Tests danach nochmals bestanden. Vorhandene Python-/zlib-PDB-Linkwarnungen und bereits bestehende Compilerwarnungen bleiben dokumentiert; keine neue Dependency-Version. Die normale Runtime-EXE wurde nicht ersetzt; Testclients verwenden isolierte Kopien.

## 24. Testsuite

Nach der abschließenden Filterkorrektur: Renderer ON **5/5 bestanden**, einschließlich der neuen dynamischen GPU-, Framegrenzen- und Lifetime-Prüfungen. Renderer OFF **3/3 bestanden**. Keine Tests deaktiviert. Erneute vollständige ON-Suite: **9/9 bestanden**, Gesamtdauer 541,77 Sekunden; Fuzz-Test 541,69 Sekunden. Auch der abschließende ON-Rendererlauf nach dem OFF-/ON-Neubau ist nochmals grün (5/5, 2,74 Sekunden). Die vorhandenen Bibliothekstests und Renderer-Tests wurden nicht ausgeblendet.

Die 27 dynamischen PNT-Paritätsposen ergeben mittlere RGB-Abweichungen zwischen 0,000085 und 0,378 auf der 0–255-Skala, **0 Silhouetten-/Coverage-Differenzen und 0 Alpha-Innenflächenfehler**. Bei zwei Point-Sampling-Fällen lagen 4 bzw. 6 akzeptierte einpixelige Alpha-Texelgrenzen vor. Das ist synthetische GPU-Parität mit veränderten fertigen Vertices/Normals; den separaten tatsächlichen Granny-Animationsnachweis liefert der Original-Map-Test.

Im ersten realen Actorlauf wurde eine falsche neue Freigabe erkannt: `Terrain.ResetFrame` setzte `HasTerrain` zurück, bevor es für Actors gelesen wurde. Minimal korrigiert durch Übernahme der letzten abgeschlossenen Terrain-Grenze **vor** dem Reset. Ein Test prüft nun diese konkrete Bedingung sowie das Sperren der Actorübergabe außerhalb des Frames. Kein Terrain-Rendering oder Granny-Skinning dafür geändert.

**Ingame-Nachbefund und enge Korrektur:** Der Spieler trug `warrior_4-1.gr2`. Dieser Body wurde mit `excluded: actor specular/opacity material` ausgeschlossen und ist kein erfolgreicher Grundkörpertest. Außerdem wurde Race 20114 (`lion_white.gr2`) irrtümlich als PC-Körper übernommen: Der native Horse-Actor hat standardmäßig `TYPE_PC`, und dessen eigener Horse-Pointer ist leer. Die vorherige Prüfung `IsPC() && !m_pkHorse` war deshalb unzureichend. Die vorhandene native Funktion `IsPoly()` erkennt PC-getypte Nicht-Spielerrassen bereits; sie wird nun in allen neuen 5A-Übergabestellen zusätzlich berücksichtigt. Native Typen, Mount-/Granny-Logik und Materialien wurden dafür nicht verändert.

Die isolierte Reproduktion `gate-before` weist mit dem alten Build tatsächlich 4.453 Löwen-Vertices / 142.496 Upload-Bytes nach; `verify_actor_gate.ps1 -ExpectOldMountLeak` bestätigt diesen erwarteten Fehler. Die erweiterte Testszene verwendet die echte native Mount-Erzeugung, nicht einen erfundenen GPU-Dummy. Danach wird der normale Krieger neu erzeugt. Der korrigierte Gegenlauf `gate-after` ist bestanden: `verify_actor_gate.ps1` bestätigt Mount-Ausschluss, vier Grundkörper-Lebenszyklen, wiederverwendete Indizes, A1 → B1 → A1, keine verbliebenen Actorressourcen und Exit 0. ON-/OFF-/finaler ON-Build, die jeweiligen Renderer-Tests und die vollständige Suite nach der Filterkorrektur sind erfolgreich abgeschlossen.

Im normalen korrigierten Diligent-Ingame-Lauf wurde tatsächlich `warrior_novice.gr2` übergeben: 2.207 Vertices, 70.624 Byte, drei Draws und insgesamt ein IB-Upload. Die kurze anfängliche Ausschlussmeldung für den nativen nicht deckenden Einblendmodus ist erwartungsgemäß. Anschließend kontinuierliche Body-Submission bis zum sauberen Shutdown mit Actor- und Weltobjekt-GPU-Zählern auf null. Das native Fehlerlog enthält nur das bereits vorher vorhandene `invalid idx 0`; keine beobachtete neue Assertion oder Device-Fehlermeldung.

## 25. Legacy-Ingame-Regression

Der isolierte Legacy-Original-Maptest zeigt denselben original animierten Grundkörper und läuft ohne Fehler bis Exit 0. Der normale Client PID 59948 wurde ohne Renderer-Argument gestartet und sauber beendet. Nach der Filterkorrektur bestätigt der Benutzer die normale Legacy-Ingame-Prüfung (PID 48748) ausdrücklich positiv: „ieht jetzt hevoragend aus“. Angefragt waren Charakter/NPC/Mob, Animationen, Terrain/Gebäude/Bäume, Effekte und UI sowie die Vorbereitung ohne Rüstung/Körperkostüm/Reittier. Der Prozess endete mit Exit 0. Das native Fehlerlog enthält vorhandene `ProcessDamage`-/`AddDamageEffect`-Diagnoseausgaben, keine gefundenen Assertion-/Device-/Granny-/Traceback-Fehlermeldungen.

Im ersten normalen isolierten Verzeichnis fehlte die BGM-Verknüpfung, was native Audio-Decoder-Meldungen auslöste. Nur die Testverknüpfung wurde ergänzt; keine Änderung am Audiocode. `invalid idx 0` war bereits im normalen 4B-Vergleichslog vorhanden und wird nicht als neuer Rendererfehler ausgegeben.

## 26. Empfehlung für separat zu beauftragendes 5B

Nur bei gesondertem Auftrag den nächsten klar abgegrenzten Actorvertrag auswählen: etwa separate Hair-/Weapon-Parts **oder** eine definierte NPC-/Mob-Gruppe. Der im Ingame-Test ausgeschlossene Rüstungskörper ist ein dokumentierter Material-Sonderfall, kein Anlass, diesen Umfang ungefragt vorzuziehen. Nicht gleichzeitig Equipment, Material-Sondermodi, GPU-Skinning, Effekte und UI beginnen. Keine solche Implementierung wurde begonnen.

## Git-Diff / Dateien / Nachweise

16 bestehende Source-/Testdateien mit 146 zusätzlichen und 12 entfernten Zeilen; hinzu kommen 12 neue Dateien: fünf Actor-Daten-/Renderer-/Bridge-Dateien, fünf Testdateien und diese zwei Dokumente. Hauptänderungen:

- `EterGrnLib`: unveränderlicher Indexsnapshot, fertiger CPU-PNT-Snapshot, instanzeigene Freigabe.
- `GameLib`: enger Hauptkörper-Scope, Submit im bestehenden sichtbaren Pass, Wiederverwendung lesender Material-/Lichterfassung.
- `Renderer`: dynamische Erweiterung des vorhandenen Meshpfads, dünner Actorowner, Frame-/Ressourcendiagnose.
- `tests/Renderer`: 27 dynamische D3D9-Paritätsposen, Owner-/Upload-/Lifetime-Negativtests, tatsächlicher Granny-Actor auf originalen Maps, isolierter Prozessbeobachter.

Wesentliche neue Integrationsbereiche mit exakt `ZiiNAN` markiert. `git diff --check` ohne Fehler. Keine Änderungen an Granny-Runtime, SSE2/Fallback-Skinning, CStateManager, Originalassets, Servern, Equipmentlogik, UI oder Shaderquellen. Keine Commit-/Push-/Reset-/Stash-/Clean-Aktion. Die im Runtime-Repository vorhandene Änderung `config/channel.inf` bleibt unangetastet.

Nachweise liegen unter [build/milestone5a](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build/milestone5a): Build-/Testlogs, Actorinventur, `probe2` mit originalen Testlogs/Exitcodes/CSV, `normal` mit den ersten regulären Startdaten, `normal-gate` mit den korrigierten normalen Ingame-Läufen sowie `gate-before`/`gate-after` für die gezielte Mount-Regression. Abschließende Logs: `build-gate-on.log`, `build-gate-off.log`, `build-gate-final-on.log`, `tests-gate-off.log`, `tests-gate-final-on.log`, `full-suite-gate.log`. Einzelnes unverändertes Diligent-Bild: [Krieger auf B1](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build/milestone5a/evidence/diligent-b1-idle.png). Das Bild allein ist **kein synchroner A/B-Pixelnachweis**; der belastbare numerische Vergleich ist der native GPU-Test.

**STOP nach Milestone 5A. Kein 5B.**
