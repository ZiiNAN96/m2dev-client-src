# Milestone 5B – NPC-Materialzustände stabilisieren

> Historischer Milestone-Stand. Aktuell seit [M12](renderer-milestone12.md): Diligent D3D11 ist im ON-Build der Default; Legacy wird explizit mit `--renderer=legacy-d3d9` gewählt. OFF-Builds bleiben Legacy-only. Frühere Testergebnisse und Auswahlbeschreibungen unten gelten für ihren damaligen Stand.

Stand: 12.09.2026. Ausschließlich die zwei offenen Material-State-Fälle aus 5B; kein 5C.

## 1. Ursache und Abgrenzung

Ein konkreter Fehler der Zustandsaufnahme ist deterministisch reproduziert und minimal korrigiert:

`CActorInstance::BeginBlendRender()` setzt für den nativen Fade `ALPHAOP=SELECTARG2` und `ALPHAARG2=TFACTOR`. `ALPHAARG1` wird dabei absichtlich nicht benötigt und auch nicht gesetzt. Die Actor-Übergabe in `StateReader::Capture()` verlangte trotzdem `ALPHAARG1=TEXTURE`. Ein anderer gültiger, aber inaktiver Wert führte zu `excluded: actor render state` und zum Überspringen des Diligent-Draws. Die native D3D9-Darstellung war davon nicht betroffen.

Vorhandene Schreiber anderer Werte sind unter anderem `CMapOutdoor::RenderWater()` (`DIFFUSE`) und `CEffectInstance::OnRender()` (`TFACTOR`). Das belegt eine mögliche Herkunft geerbter Werte, nicht die exakte Reihenfolge der historischen beiden Frames. Im normalen Weltpfad wird Wasser nach den Charakteren gezeichnet; es ist deshalb nicht pauschal der unmittelbar vorherige Draw desselben Frames. Kein Wasser-, Effekt- oder Legacy-Code wurde geändert.

Die ursprünglichen Meldungen betrafen Doctor (Race 20018) und Sinseon (Race 20095). Damals fehlte ein vollständiger State-Dump. Die hier nachgewiesene Ursache passt zum sporadischen Abweisen beim Fade, kann aber nicht rückwirkend beiden historischen Meldungen zweifelsfrei zugeordnet werden. Kein Nachweis für einen überlaufenden Diligent-PSO/SRV-Cache. Die frühere grüne Händlerdarstellung der isolierten 5B-Szene war ein absichtlich gesetzter ADD-Modus, kein Beleg für Materialübertragung.

## 2. Reproduktion und konkrete Änderung

Der native GPU-Vergleich setzt im vorhandenen Fade-Fall das unbenutzte Alpha-Argument 1 auf `CURRENT`. **Vor dem Fix scheitert der Test** an `5B native actor material capture` (`build/milestone5b-state/repro-test.log`, CTest Exit 8). Nach dem Fix wird exakt dieser native Zustand gezeichnet und mit der echten D3D9-Fixed-Function-Ausgabe verglichen. Ein weiterer Referenzfall verwendet `DIFFUSE` bei gleichzeitigem Alpha-Test.

Einzige geänderte Produktionsdatei gegenüber dem übernommenen 5B-Stand: `src/GameLib/StaticObjectBridge.cpp`.

- Bei Actor-Capture beginnt der CPU-Draw-Snapshot mit `StaticObjectDraw{}`. Deaktivierte Alpha-Test-, Fog-, Point-Light- und optionale Texturfelder können nicht aus einem wiederverwendeten Snapshot stammen. Der bisherige Actor-Aufrufer verwendete bereits einen frischen Snapshot; dies ist Absicherung des Capture-Vertrags, nicht eine behauptete zweite historische Ursache.
- `ALPHAARG1` wird nur geprüft, wenn der unterstützte Alpha-Modus es tatsächlich verwendet. Die Ausnahme ist der bereits implementierte Actor-Fade `SELECTARG2/TFACTOR`; kein neues Materialfeature.
- Aktive, nicht unterstützte Argumente werden weiterhin abgewiesen. Kein stiller Fallback auf den vorherigen Draw.
- Keine Änderungen an Diligent-PSOs, Shadern, SRBs, Drawcalls, CStateManager, Granny, CPU-Skinning, Terrain, statischen Objekten oder UI. Kein globaler Geräte-Reset. Die Initialisierung betrifft nur den lokalen CPU-Snapshot.

Sparsame Markierung: `// ZiiNAN: Ensure deterministic actor material state`.

## 3. Call-Chain und State-Audit

Nativer Actor-Hauptmodellpfad → `CGrannyModelInstance` in `ModelInstanceRender.cpp` → aktuelle Shape-/Race-/MR12-Materialpalette → `ApplyRenderState()` → synchroner `ActorDrawTarget`-Callback → `ActorRenderBridge.cpp::Submit()` → `CaptureStaticMapObjectDraw(..., actorLighting=true)` → `DiligentActorRenderer::Draw()` → gemeinsamer `DiligentStaticObjectRenderer::Draw()` → native `RestoreRenderState()`.

| Komponente | Vertrag pro Materialdraw / Analyse |
|---|---|
| Material-/Pipeline-Cache | Kein einmaliger Bind je Actor. PSO wird für jeden Draw gesetzt. Schlüssel: Cull + Blend + DepthWrite, insgesamt 12 Varianten. |
| Blend | Im PSO fest: SRC_ALPHA / INV_SRC_ALPHA; ADD für Farbe und Alpha. Enable aus dem Snapshot. Andere aktive native Blend-Konfigurationen werden abgewiesen. |
| Alpha-Test | Modus und 8-Bit-Referenz vollständig im frisch geschriebenen Constant Buffer; deaktivierter Test erhält den Default. Fade verwendet ausschließlich Factor-Alpha. |
| Depth / Cull | Pro Draw gewählter PSO; Depth-Test aktiviert, LESS_EQUAL, DepthWrite aus dem Snapshot, definierte Winding-/Cull-Variante. Stencil bleibt der feste deaktivierte Descriptor-Default. |
| Texture / SRV | Aktuelles Material aus der Instanzpalette nach nativer Override-Anwendung. SRB enthält Diffuse und Auxiliary-SRV. Kein Rückgriff auf die vorige Actor-Textur bei fehlendem Upload. |
| Sampler / SRB-Cache | Haupt- und Auxiliary-Sampler inklusive Wrap/Clamp, Filter, Mips und Anisotropie im Schlüssel. Änderungen invalidieren alle 12 SRBs dieser Textur. Auxiliary-Texturwechsel ist ebenfalls Teil der Invalidierung. |
| Inaktive Auxiliary-Stufe | Gültige aktuelle Diffuse-SRV als Bind-Fallback, aber shaderseitig deaktiviert. Kein fremdes SphereMap-/CameraAlpha-Binding. |
| Material-/Vertexfarbe | Aktuelle native Material-/Licht-/Factor-Werte in jedem Constant-Buffer-Update. PNT hat keine Vertexfarbe; aktives COLORVERTEX bleibt außerhalb des unterstützten Vertrags. |
| Transform / Fog / Licht | Matrizen, Normale, Ambient/Diffuse, Fog und vorhandenes Point-Light werden vollständig übertragen; optionale deaktivierte Felder starten neutral. |
| Submesh / Kategorie | Jeder Submeshdraw bindet separat, unabhängig von VID/Kategorie. VB/IB werden ausdrücklich gesetzt, Ressourcen gebunden und der vollständige Constant Buffer mit DISCARD geschrieben. |

Die Diligent-Descriptor-Konstruktoren definieren die unveränderlichen Felder; diese stammen nicht aus einem vorherigen Draw. Es wurde kein globaler Reset als Ersatz für die bestehende vollständige Materialbindung ergänzt.

## 4. Neue deterministische Tests

`tests/Renderer/ActorStateIsolationChecks.h`, eingebunden in `TerrainGpuTest.cpp`:

- Reihenfolge: opaque Player/SphereMap → Alpha-Test-NPC → opaque Mob/ADD oder MODULATE → blended NPC → opaque NPC.
- Alle 120 Permutationen, sechs Konfigurationen: **720 Reihenfolgen**, alle **12 PSOs**.
- Ganze Farb- und Tiefenpuffer bitgenau gegen die jeweilige Referenz verglichen; jeder Actor muss sichtbar sein. DepthWrite wird zusätzlich je Actor kontrolliert.
- Geteilte Haupttextur bei verschiedenen Samplern, unterschiedliche Override-/Auxiliary-SRVs, Alpha-Test, Blend, Materialfarbe, Fog, Point-Light, alle Cull-Modi.
- Dasselbe Ergebnis bei fünf Submeshes/Materialien einer einzigen Actor-ID statt fünf Actor-IDs.
- Sechs Resize/Suspend/Restore-Zyklen; fehlende Textur wird ohne Draw abgewiesen, keine Übernahme der vorigen SRV; Ressourcen am Ende null.
- Die Flächen überlappen nicht. Physikalisch notwendige Reihenfolgeabhängigkeit überlappender transparenter Flächen wird nicht fälschlich als State-Leak gewertet. Bestehende Terrain-/Objekt-/Actor-Depth-Paritätstests bleiben aktiv.

`ActorMaterialGpuCases.h` ergänzt die native Referenz um inaktive ARG1-Werte TEXTURE/DIFFUSE/CURRENT/TFACTOR, einen absichtlich verunreinigten wiederverwendeten Snapshot und einen Negativtest für ein tatsächlich aktives, nicht unterstütztes Argument. Keine bestehenden Referenzfälle entfernt.

`actor_state_entry.py` wiederholt Doctor/Sinseon mit den originalen MSM/GR2/Textures und Sinseons originalem `stand00.msa`: Fade, Normal, ADD, Rüstung → NPC, gemischte Gruppen, Sichtbereich verlassen/zurückkehren, Hide/Respawn, A1 → B1 → A1, abschließendes Löschen. 43 Phasen. Der allgemeine 5B-Szenentest erhält lediglich eine konfigurierbare Liste der gemischten Testraces; sein Standard bleibt unverändert.

## 5. Regressionsergebnisse

| Prüfung | Ergebnis |
|---|---|
| Release Diligent ON | Erfolgreich; `release-on.log`. |
| Vollständige Testsuite nach dem Fix | **10/10**, 477,10 s; inklusive fullbench, fuzzer, zstreamtest und playTests. |
| Renderer ON | **6/6**, 9,57 s. |
| Release Diligent OFF | Erfolgreich; Renderer **4/4**, 0,56 s. |
| Abschließendes Release ON | Erfolgreich; Renderer **6/6**, 9,39 s; GPU-Parität darin 8,48 s. |
| Isolierte Originalszene Legacy | **31/31 Phasen**, A1 → B1 → A1, Rüstungswechsel, mehrere NPCs/Mobs, 36 Actors, Hide/Despawn/Respawn; PID 33032, Exit **0**, 375,9 s. |
| Isolierte Originalszene Diligent | Dieselbe vollständige Folge; PID 51824, Exit **0**, 375,9 s. |
| Gezielte NPC-State-Szene Diligent | **43/43 Phasen**, drei Mapladungen A1 → B1 → A1; PID 62144, Exit **0**, 260,7 s. |
| Normaler Legacy-Ingame-Start | Ohne Renderer-Argument, Originalpakete. Benutzer bestätigt „paasst“ zur Prüfung von Rüstung/Charakter/NPCs/Mobs/Animation, Terrain/Gebäuden/Bäumen/Effekten/UI; PID 44612, Exit **0**, 75,2 s. |
| Normaler Diligent-Ingame-Start | Benutzer bestätigt „passt alles“ einschließlich Kamera und dreimal Minimize/Restore; PID 37408, Exit **0**, 80,2 s. Doctor **20018** und Sinseon **20095** im Laufzeitlog tatsächlich jeweils mit Fade **und** normalem Material vorhanden, keine Capture-Ablehnung. |
| Diligent-Neustart mit abschließendem ON-Build | Zweiter normaler Login, NPC-/Player- und Kamera-Prüfung vom Benutzer erneut mit „passt alles“ bestätigt. PID 60732, Exit **0**, 110,2 s; keine State-Ablehnung, Actor-/Objekt-Ressourcen nach Shutdown **0**. |

Die drei vollständigen isolierten Läufe haben **leere Fehlerlogs**, keine abgewiesenen Materialzustände und normale Teardown-Marker. `verify_actor_variants.ps1` sowie das neue `verify_actor_state.ps1` prüfen die vollständigen Phasen, Originalmaterialien, beide Ziel-NPCs, Sichtbarkeitsfolgen, Mapsequenzen, Exitcodes und Ressourcenwerte strikt; beide bestanden. Diligent nach Despawn und Shutdown: **actor_geometry=0, actor_textures=0**, zusätzlich **object_geometry=0, object_textures=0** nach Shutdown. Im normalen Legacy-/Diligent-Ingame-Log steht nur das bereits vor diesem Auftrag vorhandene `invalid idx 0`; keine neue Assertion-/Device-/Granny-Fehlermeldung gefunden.

Die zwei ersten automatischen Startversuche (PID 39096 / 48888, Exit 0 nach 5 / 10 s) wurden laut Benutzer versehentlich geschlossen. Sie zählen ausdrücklich **nicht** als bestandene Szenentests und wurden vollständig wiederholt.

Zwischen Gesamtsuite und abschließendem ON-Test wurde nur die synthetische Override-Testtextur noch farblich deutlicher vom Ausgangsbild getrennt. Damit würde auch ein falsches Auxiliary-SRV ein sichtbar anderes Ergebnis liefern. Die Produktionskorrektur blieb unverändert. Sämtliche Renderer-Tests liefen mit diesem finalen Teststand erneut; die unveränderten Bibliotheks-Langtests wurden danach nicht ein zweites Mal wiederholt. Details inklusive `Actor state isolation: 720 permutations ... PASS` in `renderer-final-on-detail.log`.

### Stabilität und Messgrenzen

Private Memory (MB, Fünfsekunden-Stichproben):

| Lauf | Spitze | Letzte Stichprobe vor Exit |
|---|---:|---:|
| Legacy, isolierte Originalszene | 313,9 | 303,1 |
| Diligent, isolierte Originalszene | 444,1 | 418,4 |
| Diligent, Doctor/Sinseon | 465,6 | 419,0 |
| Legacy, normal Ingame | 442,2 | 439,6 |
| Diligent, normal Ingame | 554,0 | 553,5 |

Kein ungebremstes Wachstum in den isolierten Wiederholungsfolgen; der Verbrauch fällt nach Spitzen wieder. Im normalen Lauf wurden zusätzliche Mapbereiche/Assets geladen. Diese kurzen Läufe und die null offenen Actor-Grafikressourcen sind **kein Langzeitbeweis für die Leakfreiheit des gesamten Clients**.

Resize ist über die echten Renderer-APIs einschließlich sechs Suspend/Restore-Zyklen im neuen Test sowie die bestehenden nativen Paritätstests geprüft. Mehrfaches reales Minimize/Restore im normalen Diligent-Ingame-Fenster ist vom Benutzer bestätigt. Eine zusätzliche automatisierte Minimierung des isolierten Fensters ließ sich wegen wechselnden Vordergrundfensters nicht zuverlässig ausführen; sie wird nicht als bestanden gezählt. Der Computer-Use-Skill diente zur gezielten Sichtkontrolle eigener Testfenster, nicht zum Eingriff in fremde Clients oder Anmeldungen.

Echte serverseitige Teleports A1 → B1 → A1 sowie Logout → Charakterauswahl → erneuter Login **innerhalb desselben Diligent-Prozesses** wurden nicht separat nachgewiesen. Die nachgewiesenen Mapwechsel sind vollständige isolierte Original-Mapladungen, kein Server-Warp. Die nicht migrierte Ingame-UI wurde hierfür weder erweitert noch ersetzt. Der optional angefragte Legacy-Rüstungswechsel/Relog wurde nicht einzeln vom Benutzer bestätigt; Rüstungswechsel sind im automatisierten Vergleich vollständig abgedeckt.

## 6. Freigabe und Grenzen

Der reproduzierte Capture-Fehler ist behoben. Für die unterstützten 5B-Materialdraws ist eine vollständige Zustandsbindung nachgewiesen, ohne globalen Reset und ohne Übernahme des vorherigen Actor-Bindings. Die historische Ursache ist von der reproduzierten Fehlerklasse ausdrücklich zu unterscheiden.

Eine **uneingeschränkte 5B-Gesamtabnahme wird wegen der genannten Live-Testlücken nicht behauptet**. Die technische Materialstabilisierung ist innerhalb des getesteten 5B-Vertrags erfolgreich; die verbleibenden Lücken werden nicht durch eine neue Migration umgangen. Waffen, Schilde, Haare, Mounts, SpeedTree, Effekte und Ingame-UI bleiben im Diligent-Weltbild außerhalb des bisherigen 5B-Umfangs; der vollständige Legacy-Pfad bleibt Standard.

## 7. Diff und Nachweise

Gegenüber dem übernommenen, noch uncommitteten 5B-Stand: **eine Produktionsdatei**, vier hinzugefügte und eine ersetzte/entfernte Zeile in `StaticObjectBridge.cpp`; dazu die beschriebenen Tests und dieses Dokument. Die übrigen bereits vorhandenen 5B-Änderungen im Git-Diff stammen nicht aus dieser Stabilisierung. Vergleichskopien der berührungsnahen Dateien liegen unter `build/milestone5b-state/baseline`. Diligent-Renderer, Actor-Bridge, Render-Datenstrukturen und Granny-Dateien wurden in diesem Auftrag nicht geändert.

Build-/Testbelege: `build/milestone5b-state`. Laufzeitbelege: `build/milestone5b/state-full`, `state-npcs`, `state-normal`, `state-restart`; jeweils eigene Pakete beziehungsweise unveränderte Originalpakete, private EXE-Kopie, Logs, Exitnachweise und Ressourcen-CSV. Abschließender Build: Diligent-Option **ON**, Legacy bleibt Startup-Default; EXE SHA256 `B0197408DCDFFDFE3D44EA4D8DE17F3C1FA2B79F351B268F8739815B3A834DB6`. Der Wiederholungstest PID 60732 benutzte genau diese abschließende EXE.

Normale Runtime-EXE nicht ersetzt; bestehende Änderung `m2dev-client/config/channel.inf` unberührt. `git diff --check` ohne Fehler, nur bekannte CRLF-Hinweise. Alle eigenen Testclients regulär beendet. Kein Commit, Push, Reset, Stash oder Clean.

**STOP nach Abschluss dieses Auftrags. Kein 5C begonnen.**
