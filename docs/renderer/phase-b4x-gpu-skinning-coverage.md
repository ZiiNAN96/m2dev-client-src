# Phase B4-X – GPU-Skinning-Abdeckung

Stand: 13.09.2026. Basis: Source-HEAD `5187800` (B3).

## Status: B4-X vollständig abgeschlossen – GO für B5-X

Die ausdrücklich freigegebene minimale gemeinsame Haar-LOD-Korrektur ist umgesetzt und abgenommen: gezielte CPU-/GPU-Parität, beide sichtbaren Wiederholungsläufe, vollständige Release-Suite **24/24** und Debug-Renderer-Suite **20/20** bestanden. Der normale GPU-Relog einschließlich NPC-/Fensterprüfung und Shutdown ist ebenfalls bestätigt und protokolliert: Exit 0, Skinning-Fehler 0, überwachte Ressourcen 0. Der Nutzer hat am 13.09.2026 die protokollierten realen LOD-Übergänge nach Relog zusammen mit den **224 bestandenen Hair-LOD-Paritätsfällen** ausdrücklich als Ersatz für den nicht separat durchgeführten manuellen Nah/Fern/Nah-LOD-Test akzeptiert. **B4-X ist damit vollständig abgeschlossen; GO für B5-X ist gesetzt.** Kein B5-X begonnen.

CPU bleibt Produktionsdefault. GPU bleibt ausschließlich `--skinning=gpu-prototype`. Kein Production Switch, keine Deformer-/Granny-Ersetzung, kein B5-X.

## Architektur und Änderungen

- `ActorRenderBridge.cpp`: bestehende Actor-Parts statt Race-0-/Shape-0-Freigabe. Player, NPC, Mob, Mount, berittener Player und bereits unterstützte Special-Actors verwenden dieselbe Auswahl. Keine neue Actor-Klassifikation.
- `ActorRenderData.h`: expliziter GPU-geeigneter Deform-Scope und vorhandene Kategorie/Part-Information. CPU-only-Paritäts-Scope bleibt möglich; kein Laufzeit-Menü/Hot-Switch.
- `ModelInstanceUpdate.cpp`: unverändert zuerst Granny-Pose und Weltmatrizen aktualisieren; nur nach erfolgreicher Vorbereitung CPU-Deform/Kopie überspringen. Bei Fehler aktuelle native CPU-Deformation, altes GPU-Handle verwerfen, begrenzte Diagnose.
- `GpuSkinningPrototype.h`: bestehende PWNT-Aufbereitung auch für andere Modelle, gemischte rigid/deform Meshslots und verknüpfte Ziel-Skeletons. 163 aktive Skeleton-Bones, 256 Matrixplätze im bestehenden Shader. B3-Fingerprinter bleibt für den Referenztest.
- `DiligentStaticObjectRenderer.cpp`: schwacher Cache geteilter immutable PWNT-/Indexbuffer anhand tatsächlicher Mesh- und Remap-Identitäten. Ein Posebuffer pro tatsächlichem Palette-Owner; Körper und verknüpfte Haare teilen die Pose, Mount und Reiter nicht.
- Gemischte Modelle: rigide PNT-Vertices in eigenem immutable Buffer, unveränderter PNT-VS und native Bone*World-Matrix. Keine künstlichen Skinning-Gewichte für Waffen/rigide Meshes. Bestehende Materialgruppen, Indexbereiche und Drawanzahl bleiben erhalten.
- `DiligentActorRenderer`: Mount-/Attachment-Ownership auch für GPU-Geometrie erfasst.
- `SkinningData.h`: monotone Palette-Identität verhindert Wiederverwendung eines fremden Posebuffers bei wiederverwendeten Instanzadressen.
- `UserInterface.cpp`: Shutdown-Audit um geteilte statische Skin-Mesh-Ressourcen ergänzt.
- Neuer `SkinningCoverageTest.cpp`, neue private Weltfixture `gpu_skinning_coverage_entry.py`, bestehender Launcher um Coverage-/Normalstart erweitert. B2/B3-Tests bleiben vorhanden; B3 prüft jetzt zusätzlich die freigegebene Armor-Route und expliziten CPU-Rückfall.

Skinning-VS und Pixel-/Materialshader wurden nicht geändert. Native Animation, CPU-Deformer, UI, Welt und Gameplay wurden nicht migriert oder modernisiert.

## Reproduzierter Haar-LOD-Fehler

Die ersten sichtbaren CPU-/GPU-Läufe wechselten alle sechs Kartenphasen und endeten sauber. Beide meldeten aber **7 `DestinationChanged`-Vorbereitungsfehler**. GPU protokollierte zeitweilige CPU-Haar-Draws für:

- Race 3, weibliche Schamanin, `shaman_miyeom.gr2`, Hair-Part 4;
- Race 1, weibliche Ninja, `assassin_novice.gr2`, Hair-Part 4.

Der isolierte Test vor der Korrektur reproduzierte den Ninja-Fall:

| Alte Haarbindung | Vollmodell 79 Bones | LOD 78 Bones |
|---|---|---|
| Index 71, aktiver Einfluss | `Bip01 Head` | `Bip01 Ponytail1` |
| Index 72, aktiver Einfluss | `Bip01 Ponytail1` | `Bip01 Ponytail11` |
| Index 73, aktiver Einfluss | `Bip01 Ponytail11` | `Bip01 Ponytail12` |
| Index 78, unbenutzter Einfluss | `Bip01 HeadNub` | außerhalb der Palette |

`CGrannyLODController::SetCurrentModelInstance` wechselt den Pose-Owner. Die beim `SetLinkedModelPointer` erstellte native Haar-Meshbindung bleibt jedoch bestehen. B2 verwirft deshalb richtigerweise die Sidecar-Freigabe bei inkompatiblem Layout. **Die Schutzprüfung wurde nicht abgeschwächt.** Ein reines Übernehmen der alten Indizes wäre keine korrekte Lösung.

### Freigegebene minimale Korrektur

Gemeinsamer Produktionspfad:

`CActorInstance::SetHair` → bestehendes `SetModelInstance(PART_HAIR, ..., PART_MAIN)` / `HAIR_LINK` → `CGrannyLODController::AddModel` → `SetLinkedModelPointer(..., true)` → `UpdateWorldMatrices` → `__RefreshLinkedLodBinding` → unveränderte CPU-Deformation oder bestehende GPU-Vorbereitung.

- `src/EterGrnLib/LODController.cpp`: nur der bereits vorhandene verknüpfte Skeleton-/Hair-Pfad registriert die Refresh-Teilnahme. Keine Änderung der LOD-Auswahl oder Distanzen.
- `ModelInstance.h` / `ModelInstanceModel.cpp`: initiales Ziel-Skeleton für registrierte Links festhalten und beim Clear freigeben. Direkte, nicht registrierte Links behalten das bisherige Verhalten.
- `ModelInstanceSkinning.cpp`: bei geändertem Bone-Layout neue native `GrannyNewMeshBinding`-Objekte gegen das aktuelle Ziel erzeugen; Bone-Indizes validieren und GPU-Remaps aus genau diesen neuen nativen Bindings extrahieren. Erst wenn alle Meshes erfolgreich vorbereitet sind, Bindings und Sidecars gemeinsam übernehmen. Temporäre/alte Bindings werden kontrolliert freigegeben.
- Gleiches Layout benötigt keine neue native Indextabelle; die vorhandene B2-Vorbereitung aktualisiert die Sidecar-Zielidentität. Kein globaler oder per-Frame-Rebind.
- `ModelInstanceUpdate.cpp`: vor den Mesh-Weltmatrizen und vor CPU-/GPU-Deformation aktualisieren. Bei gescheiterter Neubindung die aktuelle Pose ablehnen, anstatt einen möglicherweise ungültigen nativen Remap als CPU-Fallback zu verwenden.
- Vollständig rigide Haare bleiben rigid: lediglich ihre native Bone-Bindung/Weltmatrix wird aktuell gehalten, kein künstliches Skinning.
- B2-Negativtest für absichtlich inkompatible, nicht registrierte Owner-Wechsel bleibt unverändert und besteht weiterhin. Die Schutzprüfung wurde nicht entfernt oder abgeschwächt.

Eigene Integrationsstellen sind sparsam mit `ZiiNAN` markiert. Granny-SDK, `SkinningDataAdapter.cpp` und `Deform.cpp` unverändert; SHA-256 des CPU-Deformers: `C73674782ADA9D554237F1522605ABA3BFCBCBFF66FAB14E2FD34545B1FC9DE7`. Keine zusätzliche Hair-/LOD-Architekturänderung.

Vorher-Belege: `build/phase-b4x-lod-diagnosis.log`, `build/phase-b4x/gpu-coverage-release/log/syserr.txt`, `actor-renderer.log`. Die damaligen 7 Vorbereitungsfehler sind in beiden aktuellen Weltläufen auf 0 zurückgegangen.

### Gezielte Hair-LOD-Parität nach der Korrektur

`Renderer.HairLodParity`: **224 Fälle, 75.005 Vertexvergleiche**, Release und Debug bestanden. Acht Player-Races, 32 Haar-/Körperkombinationen: `hair_1_1`, `hair_2_1`, `hair_3_1` für alle acht, `hair_2_2` für die vier Original-Races sowie reale rigide `hair_15_1` und skinned `hair_17_1` für Ninja/Schamanin. Schamanin ausdrücklich mit dem betroffenen `shaman_miyeom`-Körper.

- Pro Kombination Vollmodell → LOD 1 → LOD 2 → LOD 3 → LOD 2 → LOD 1 → Vollmodell, bei fortschreitender nativer Run-Animation.
- Wiederverwendete CPU-Haarvertices sind exakt gleich zu einer jeweils frisch gegen das aktuelle Ziel erzeugten nativen Referenzinstanz. GPU-Remaps entsprechen ebenfalls exakt deren Bindung.
- Rigide und skinned Mesh-Weltmatrizen stimmen exakt mit der frischen nativen Referenz überein. Rückkehr stellt die ursprüngliche Remap-Identität wieder her; alte inkompatible GPU-Geometrie wird freigegeben.
- Maximaler Positionsfehler GPU gegenüber CPU: **0,0000457764**; Normalenfehler **1,78814e-7**, innerhalb der unveränderten B2-/B3-Grenzen.
- Wiederholtes Suspend/Resize/Restore sowie Cull-/Blend-/Alpha-Test-/DepthWrite-Varianten, Drawanzahl und fehlender CPU-PNT-Upload im GPU-Pfad geprüft.
- Nach Despawn/Shutdown: SkinMeshes, BoneRemaps, BonePalettes, PrototypeGeometry, PrototypePalettes und StaticSkinMeshes jeweils **0**; Vorbereitungsfehler **0**. Debug-D3D11-Validierung tatsächlich aktiv, Warnungen/Fehler **0**.

Belege: `build/phase-b4x-hair-lod-release-final.log`, `build/phase-b4x-hair-lod-release-details.log`, `build/phase-b4x-hair-debug-renderer-verified-details.log`.

## Bisherige numerische Ergebnisse

Erste breite Prüfungen vor Ergänzung des gezielten LOD-Tests:

- Release und Debug: je **359 Fälle, 687.993 Vertexvergleiche**.
- Maximaler Positionsfehler **0,000488281**, innerhalb `1e-4 + 2e-6 * max(1, abs(CPU-Komponente))`.
- Maximaler Normalenfehler **2,38419e-7**, Grenze `1e-5`.
- Reales 163-Bone-Modell `misterious_diseased_bosshost.gr2` enthalten, keine künstliche Maximalwertbehauptung.
- Zwölf vorhandene Cull-/Blend-/DepthWrite-Varianten mit Alpha-Test und Materialparametern gespiegelt geprüft. Rastertoleranzen wie B3; gleiche CPU-/GPU-Drawzahlen.
- Die neue Rasterfixture verwendet bewusst eine diagnostische RGBA-Textur für Materialzustände. Originaltexturen werden durch B3 und die sichtbare Originalpaket-Weltfixture geprüft; kein Anspruch, alle Originalmaterialtexturen numerisch verglichen zu haben.
- Native Geometriefixture registriert keine Image-Resource-Factories; deshalb bekannte `NOT SUPPORT FILE`-Meldungen für DDS. Das sind keine ignorierten Diligent-/D3D11-Validierungswarnungen.
- Debug: tatsächlich aktiver D3D11-InfoQueue-Layer, **0 Grafikvalidierungswarnungen/-fehler**.

Die Prüfungen wurden anschließend um den gezielten LOD-Fall, CPU-Fallback/Recovery und vorgerückte Zeitpunkte nach jedem Motionwechsel erweitert. Die obigen Zahlen sind deshalb **historische, nicht finale Ergebnisse des aktuellen Teststands**.

### Testfixture-Korrekturen und abschließende Wiederholung

- Der erste erweiterte Lauf forderte irrtümlich `pc2/.../hair_2_2.gr2`, obwohl diese Datei dort nicht existiert. Die feste Assetliste wurde auf vorhandene Dateien korrigiert; fehlende Pflichtassets werden weiterhin nicht still übersprungen. `hair_2_2` bleibt für die vorhandenen Original-Races enthalten. Belege: `build/phase-b4x-hair-lod-release.log`, `build/phase-b4x-hair-debug-renderer.log`.
- Danach waren im Debug-Rendererlauf 19/20 Tests erfolgreich, einschließlich HairLodParity. Nur der breite Coverage-Test scheiterte an einer überstrengen exakten Gleichheit zweier unabhängig gesampelter nativer Doctor-Posen: Bone 1, Translationskomponenten 12/14, maximal **7,62939e-6** Abweichung. Separat reproduziert, ohne Hair-/LOD-Pfad (`build/phase-b4x-pose-diagnostic.log`).
- Ausschließlich diese Testannahme wurde auf endliche Pose-Komponenten innerhalb **1e-5** korrigiert; der maximal gemessene native Poseunterschied wird protokolliert. Die direkten Vertex-/Normalen-/Rastervergleiche behalten unverändert ihre B2-/B3-Grenzen. Keine SDK-, Deformer- oder NPC-Produktänderung.
- Eingrenzung `--actors-only`: **39 Fälle, 74.733 Vertexvergleiche**, Debug-D3D11-Validierung 0, alle Ressourcen 0, bestanden (`build/phase-b4x-pose-check.log`).
- Sämtliche Debug-Renderer-Tests mit dem finalen Teststand: **20/20 bestanden**, 355,13 s, Exit 0. Darin breite Coverage **583 Fälle / 762.998 Vertexvergleiche**, zusätzlich HairLodParity **224 / 75.005**. Breite maximale Position **0,000488281**, Normale **2,38419e-7**, native Poseabweichung **7,62939e-6**, reales Maximum **163 Bones**. Im reinen Hair-LOD-Vergleich ist die native Poseabweichung **0**. D3D11-Debug-Validierung aktiv, Warnungen/Fehler 0; überwachte Ressourcen 0. Belege: `build/phase-b4x-hair-debug-renderer-verified.log` und `build/phase-b4x-hair-debug-renderer-verified-details.log`.
- Vollständige Release-Testsuite: **24/24 bestanden**, 1.059,03 s, Exit 0, einschließlich aller bisherigen Tests und des zusätzlichen HairLodParity-Tests. Breite Coverage ebenfalls **583 Fälle / 762.998 Vertexvergleiche**, zusätzlich HairLodParity **224 / 75.005**; gleiche maximale Positions-/Normalenfehler wie Debug, native Poseabweichung hier **0**, alle überwachten Ressourcen 0. Belege: `build/phase-b4x-hair-full-release-final.log` und `build/phase-b4x-hair-full-release-final-details.log`.

Beide vollständigen Produktbuilds erfolgreich: `build/phase-b4x-hair-release-final-build.log`, `build/phase-b4x-hair-debug-final-build.log`. Der test-only Posevergleich wurde anschließend für beide Konfigurationen neu gebaut; der geprüfte Produktcode blieb unverändert. Release-Binary und beide privaten Welt-Testkopien haben denselben SHA-256: `E2E24EE5BD5294CBA5829FDD9A403F3E0B81FCC74D242ECE9EA7325FD38F2294`.

## Gemessene Uploads und Sharing

Reale Wolf-Instanzen, je Frame/native Pose; keine Performance-/FPS-Bewertung:

| Actors | CPU-PNT-Upload Byte | GPU-PNT-Upload Byte | Bone-CB geschrieben Byte | Geteilte Meshbuffersätze | Posebuffer | CPU/GPU Draws |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 24.384 | 0 | 16.384 | 1 | 1 | 2 / 2 |
| 10 | 243.840 | 0 | 163.840 | 1 | 10 | 20 / 20 |
| 25 | 609.600 | 0 | 409.600 | 1 | 25 | 50 / 50 |
| 50 | 1.219.200 | 0 | 819.200 | 1 | 50 | 100 / 100 |
| 100 | 2.438.400 | 0 | 1.638.400 | 1 | 100 | 200 / 200 |

Bone-CB wie B3: 16.384 Byte, vollständig nullgefüllt und aktuelle Matrizen hineinkopiert. Gezählt werden tatsächlich geschriebene Bufferbytes, nicht nur Pose-Nutzdaten und nicht behaupteter PCIe-Verkehr. World-/Materialkonstanten sind unverändert separate Drawparameter. GPU-SRB hält explizit den richtigen Instanz-Posebuffer; kein Bone-Binding im gemeinsamen Texturcache.

## Sichtbare Läufe und Exitcodes

| Lauf | PID | Ergebnis |
|---|---:|---|
| Erste CPU-Fixture | 61216 | `-1073741819`, 4,3 s, Fixture-Startfehler |
| CPU-Startdiagnose | 52036 | `-1073741819`, 3,9 s, gleicher Fixture-Fehler |
| Korrigierte CPU-Fixture | 67660 | Client Exit 0, 212,6 s, sechs Kartenphasen, Nutzer bestätigt Darstellung und 3× Minimize/Restore |
| GPU-Fixture | 68092 | Client Exit 0, 213,4 s, sechs Kartenphasen, Nutzer bestätigt Darstellung und 3× Minimize/Restore |
| CPU nach Hair-LOD-Fix | 23348 | Exit 0, 212,2 s, sechs Kartenphasen; Nutzer bestätigt Haare bei Nah/Fern, Resize und 3× Minimize/Restore |
| GPU nach Hair-LOD-Fix | 64388 | Exit 0, 212,8 s, sechs Kartenphasen; Nutzer bestätigt Haare/Figuren/Mounts bei Nah/Fern, Resize und 3× Minimize/Restore |

Startfehler der neuen Fixture: Spieler-`chr.ChangeShape` wurde auch auf NPCs angewendet. Nur die Testfixture wurde korrigiert: NPCs wieder über `SetArmor` wie im vorhandenen 5B-Test. Kein nativer NPC-Code geändert.

Kartenfolge: A1 → B1 → A1 → Dungeon → Gildenkarte → A1. Acht Player-Races, vier NPCs, vier Mobs einschließlich Boss; Auf-/Absteigen über den vorhandenen Netzwerk-analogen Delete/Create-Instanzwechsel, nicht den bekannten fehlerhaften direkten Python-Absteigehelfer. Original-Haare, Waffen und Rüstungswechsel aktiv.

Die älteren Läufe 67660/68092 gaben Ressourcen frei, ihre Launcher meldeten aber korrekt einen Fehler wegen der jeweils 7 Haar-Binding-Ausnahmen. Das war keine Abnahme.

Die aktuellen Läufe **23348/64388** bestehen auch die Launcher-Prüfungen: SourceTextures/SourceBuffers/SkinMeshes/BoneRemaps/BonePalettes/PrototypeGeometry/PrototypePalettes/StaticSkinMeshes nach Shutdown **0**, `SkinPreparationFailures=0`, beide `syserr.txt` leer. CPU-Lauf: `GPUFrames=0`; GPU-Lauf: `GPUFrames=366016`, `CPUReferenceFrames=0`, `CPUVertexBytes=0`. Keine Testclients bleiben offen.

Belege: `build/phase-b4x/cpu-hair-lod-final/` und `build/phase-b4x/gpu-hair-lod-final/`, jeweils `exit.txt`, `source-resource-audit.log`, `gpu-skinning-coverage-test.log`, `resources.csv`. Speicher-/Handle-Samples wurden über beide Kartenläufe erfasst; die kurze Testdauer ersetzt keine Langzeit-Leakmessung.

## Finale normale GPU-Relog-Abnahme (abgeschlossen, 13.09.2026)

Ausschließlich vorhandene Release-Binary, Originalpakete und normale Login-/Charakterauswahl-UI, gestartet über den bestehenden Launcher mit `-Coverage -Normal -Visible -Skinning gpu-prototype`. `-Coverage` bestimmt hier das private Belegverzeichnis, **nicht** eine andere Spielfixture: `-Normal` verwendet die Originalpakete. Beide Startlogs bestätigen `Skinning=gpu-prototype`; CPU bleibt außerhalb dieser Starts Default. Keine Code-, SDK-, Deformer-, Coverage- oder Defaultänderung für diese Abnahme.

### Ablauf und Belege

- Erster normaler Lauf **PID 37476**, `build/phase-b4x/gpu-normal-relog-final/`: Darstellung vom Nutzer bestätigt, aber bereits vor dem angeleiteten Relog beendet. **Exit 0**, 97,3 s; GPUFrames 48.905, CPUReferenceFrames 0, SkinPreparationFailures 0, sämtliche überwachten Shutdown-Ressourcen 0. Dieser Lauf zählt nicht als Relog-Nachweis.
- Zweiter normaler Lauf **PID 60936**, `build/phase-b4x/gpu-normal-relog-repeat/`: Testcharakter **`[SA]Admin`**, männlicher Krieger / Race 0. Körper/Rüstung `warrior_4-1.gr2`, Haar mit 427 deformierbaren Vertices und Textur `warrior_hair_01.dds`, starre Waffe zunächst mit 916 Vertices.
- **Login → Charakterauswahl → Ingame → Charakterauswahl → erneut Ingame** vom Nutzer bestätigt. Das Actorlog belegt die Reihenfolge unabhängig: Vorschau VID 0 (Zeilen 1/10), erste Weltinstanz VID 21100 (12/21/23), erneute Vorschau VID 0 (272/281), neue Weltinstanz VID 21101 (283/292/294).
- Nach Relog Körper und Haar erneut als `GPU-skinned actor part`, Waffe als `rigid attachment` eingereicht. Rüstung, Haare, Waffe und Animationen nach Wiedereintritt vom Nutzer als korrekt bestätigt. Weitere Rüstungswechsel `warrior_novice` ↔ `warrior_4-1` und ein starrer Waffenwechsel auf 625 Vertices wurden im laufenden Test protokolliert; hierfür wurde nichts implementiert oder automatisch ausgelöst.
- Mobs werden nach dem Relog weiter dargestellt, ohne GPU-/CPU-Fallback-Anomalie. Anschließend wurden auch mehrere NPCs im GPU-Pfad protokolliert, unter anderem Blacksmith / Race 20016, Doctor / Race 20018, Sinseon / Race 20095, Defence / Race 9002 sowie Oldster / Race 9012. NPC-Besuch und abschließende Darstellung vom Nutzer mit „hat alles gepasst“ bestätigt. Der optionale Mount-Test wurde nicht beobachtet und wird nicht als bestanden eingetragen.

### Hair-LOD: beobachtete Übergänge und manuelle Grenze

Nach dem Relog ist für VID 21101 zunächst ein reduziertes Rüstungsmodell mit **761 Vertices**, anschließend das volle Modell mit **2.582 Vertices** protokolliert (Actorlog 283 und 349). Weitere reduzierte/volle Übergänge beim Rüstungswechsel sind ebenfalls protokolliert (z. B. 639/650 und 665/676), während das Haar im GPU-Pfad bleibt. Keine Hair-Binding-/Remap-Fehlermeldung dabei.

Der Nutzer hat normale Nah-/Fernansichten **am eigenen Charakter** als korrekt bestätigt. Das ist keine unabhängige Bestätigung der verlangten gezielten Nah → Fern-LOD → Nah-Folge: `CGraphicThingInstance::UpdateLODLevel` misst auch den Abstand zum Kamerazentrum, und `CGrannyLODController::UpdateLODLevel` hält Actors innerhalb 500 Einheiten auf dem Vollmodell. Normales Zoomen des zentrierten Players löst diese LOD-Folge daher nicht aus.

Die vorhandene freie Admin-Kamera über Rollen/Nummernblock konnte der Nutzer nicht bedienen. Der zunächst vermutete Test an einem anderen Spieler wurde auf Nachfrage ausdrücklich als Test am eigenen Charakter präzisiert. **Die separate manuelle Nah/Fern/Nah-LOD-Folge wurde nicht durchgeführt** und wird nicht als manuell bestanden ausgewiesen.

**Akzeptierter Ersatznachweis (13.09.2026):** Auf die ausdrückliche Frage, ob die protokollierten echten LOD-Übergänge nach Relog plus die 224 bestandenen Hair-LOD-Tests diese separate manuelle Folge ersetzen dürfen, hat der Nutzer mit „ja“ zugestimmt. Das Hair-LOD-Abnahmekriterium ist damit durch diesen genehmigten Ersatznachweis erfüllt; die oben dokumentierte Grenze der manuellen Prüfung bleibt bestehen.

### Abschließende Diagnostik und Lifecycle

Der zweite Testclient **PID 60936** wurde über X beendet. **Exitcode 0**, Laufzeit **736,3 s** (12 min 16,3 s), Launcher **PASS**. Der Nutzer bestätigt nach Relog den NPC-Besuch, einmal Resize, einmal Minimize/Restore sowie anschließend Player/Haar/Waffe/Animation als korrekt. Kein Testclient bleibt offen.

| Abschließende Prüfung | Ergebnis |
|---|---:|
| GPU-Skinning-Fallback-Diagnosen | 0 |
| CPUReferenceFrames / CPUVertexBytes im GPU-Test | 0 / 0 |
| Hair-Binding-Refreshfehler | 0 |
| InvalidRemap-Diagnosen | 0 |
| DestinationChanged-/Stale-Binding-Diagnosen | 0 |
| Bone-Palette-Vorbereitungsfehler | 0 |
| SkinPreparationFailures gesamt | 0 |
| Actor-ERROR-Meldungen | 0 |
| GPUFrames / PaletteUpdates | 11.335.622 / 11.297.886 |
| Actor-Geometrie / Actor-Texturen nach Shutdown | 0 / 0 |
| Attachment-Geometrie / Attachment-Texturen nach Shutdown | 0 / 0 |
| Mount-Geometrie / Mount-Texturen nach Shutdown | 0 / 0 |
| SkinMeshes / BoneRemaps / BonePalettes nach Shutdown | 0 / 0 / 0 |
| PrototypeGeometry / PrototypePalettes / StaticSkinMeshes nach Shutdown | 0 / 0 / 0 |
| SourceTextures / SourceBuffers nach Shutdown | 0 / 0 |

`BoneRemaps` umfasst auch die Hair-Sidecars; `PrototypePalettes` zählt die GPU-Pose-/Bonebuffer-Owner. Native Granny-Meshbindings besitzen keinen zusätzlichen separaten Runtime-Zähler; ihre gemeinsame Freigabe bleibt Bestandteil des unveränderten Clear-/Destruktorpfads. Auch die übrigen vorhandenen UI-/Text-/Welt-/Effekt-Shutdownzähler sind 0.

Belege im Verzeichnis `build/phase-b4x/gpu-normal-relog-repeat/`: `renderer-startup.log`, `actor-renderer.log`, `terrain-renderer.log`, `log/syserr.txt`, `source-resource-audit.log`, `resources.csv`, `exit.txt`. Produktbinary und Testkopie stimmen weiterhin mit dem oben dokumentierten SHA-256 überein. Kein neuer Build erforderlich, da für diese Abnahme kein Produkt- oder Testcode geändert wurde.

Diese Diagnose verwendet bestehende Guards, begrenzte Fehlermeldungen und Ressourcenzähler. Es wurden keine neuen per-Draw-Counter oder Bone-Matrix-Dumps eingebaut; getrennte InvalidRemap-/Stale-/Palette-Ergebnisse sind Auswertungen der vorhandenen Fehlerwege, keine erfundenen eigenständigen Zähler.

Zwei vorhandene Meldungen `invalid idx 0` stammen aus `CGuildMarkManager` (Gildenmarken-Verwaltung), nicht aus dem Skinning-Remap. Vorhandene `ProcessDamage`-Traceausgaben werden ebenfalls nicht als Skinning-Fehler gezählt. Keine Änderung außerhalb B4-X vorgenommen.

Die anfängliche unklare Rückmeldung zur NPC-/Fenster-/Shutdown-Anfrage ist durch die spätere Bestätigung „hat alles gepasst“ und die vollständigen Prozess-/NPC-/Shutdown-Belege geklärt. Mit der anschließenden ausdrücklichen Zustimmung „ja“ zum oben beschriebenen Hair-LOD-Ersatznachweis ist auch die letzte Abnahmeklärung abgeschlossen. **B4-X vollständig abgenommen; GO für B5-X.** Dieser GO-Vermerk dokumentiert ausschließlich die Freigabe der nächsten Phase: B5-X wurde nicht begonnen.

## Abdeckungsmatrix / abgeschlossene Abnahme (33 Punkte)

| Punkt | Stand |
|---|---|
| 1 Actor-Kategorien | Bestehender gemeinsamer Pfad erweitert; Haar-LOD-Ausnahmen korrigiert, aktuelle Weltläufe ohne Vorbereitungsfehler |
| 2 Player-Races | Alle acht vorhandenen normalen Races im ersten numerischen und sichtbaren Test |
| 3 Geschlechter | Krieger/Ninja/Sura/Schamane jeweils beide; Source hat `MAIN_RACE_MAX_NUM=8` |
| 4 Shapes/Rüstungen | Alle acht: novice → 4-1 → lord → marry → novice numerisch; live Original-Item-Shapes 0/3/6/9/0 |
| 5 Hair Cases | Acht Races, 32 Haar-/Körperkombinationen, 224 gezielte LOD-Fälle; aktuelle CPU-/GPU-Sichtprüfung bestanden |
| 6 NPCs | Goods, Blacksmith, Doctor, Sinseon numerisch, mehrere NPCs live |
| 7 Mobs | Wolf, Orc Soldier, gemischter Barbarian Bow numerisch; mehrere live |
| 8 Bosse | Fire Dragon und realer 163-Bone-Boss numerisch, Orc Lord live |
| 9 Mounts | Horse, Boar, Lion, Halloween, Dinosaur numerisch; mehrere Typen live |
| 10 Skinned Attachments | Gemeinsamer Hair-Pfad; zusätzliche reale rigide hair_15_1 und skinned hair_17_1 bei Ninja/Schamanin geprüft |
| 11 Skeleton-Bones | Pro Fall im Coverage-Log; reales Maximum 163 erreicht |
| 12 Palette Sizes | Pro Fall im Coverage-Log; 256 Matrixplätze, akzeptiert bis 163 |
| 13 Remaps | Native und GPU-Bindung gemeinsam aktualisiert; LOD hin/zurück exakt gegen frische native Bindung geprüft |
| 14 CPU Fallback | Fehlender Sidecar-Remap → aktuelle CPU-Pose → GPU-Recovery bestanden; NaN/UnsupportedLayout weiter abgelehnt |
| 15 Static Sharing | 1/10/25/50/100 gleiche Mobs: genau ein Meshbuffersatz |
| 16 Bone Strategy | Eine CB pro Palette-Owner/Revision; Linked Hair teilt Body-CB, Mount separat |
| 17 Position-Parität | Gezielte 75.005 Hair-LOD-Vergleiche je Build; breite Release-/Debug-Coverage je 762.998 Vergleiche innerhalb unveränderter Toleranz |
| 18 Normal-Parität | Hair-LOD maximal 1,78814e-7; breite Release-/Debug-Prüfung je 2,38419e-7, Grenze 1e-5 |
| 19 Pose-Parität | Originalclips incl. Idle/Run/Attack/Hit/Death, soweit vorhanden; Zeitfortschritt im Test zuletzt präzisiert |
| 20 Materialien | 12 Pipelinevarianten, Alpha/Color/Depth geprüft; Materialshader unverändert |
| 21 Draw Calls | In numerischen Tests pro Kategorie gleich; Crowd 2/20/50/100/200 auf beiden Pfaden |
| 22 CPU Vertex Upload | Tabelle oben, tatsächlicher Renderer-Zähler |
| 23 GPU Bone Upload | Tabelle oben, tatsächliche CB-Schreibbytes; keine vollen CPU-PNT-Uploads |
| 24 Mapwechsel | Beide aktuellen sichtbaren Läufe sechs Phasen einschließlich A1 → B1 → A1, ohne Hair-Binding-Fehler |
| 25 Relog | Normaler GPU-Relog mit [SA]Admin / VID 21100 → 21101 bestätigt und protokolliert; NPCs/Mobs und Player/Attachments danach korrekt |
| 26 Resize | Gezielter numerischer Test, beide sichtbaren CPU-/GPU-Läufe sowie normaler GPU-Client nach Relog bestanden |
| 27 Minimize/Restore | Frühere sichtbare Läufe je dreimal bestätigt; normaler GPU-Client nach Relog einmal bestätigt |
| 28 Shutdown | Beide normalen GPU-Clients zusätzlich mit Exit 0 beendet; zweiter Lauf 736,3 s, Launcher PASS |
| 29 Resource Lifetime | Überwachte Owner nach gezielten Tests, Weltläufen und normalem GPU-Relog-Shutdown 0 |
| 30 Release/Debug | Beide Produktbuilds nach Hair-LOD-Korrektur erfolgreich; SDK-/Deformer-Code unverändert |
| 31 Testsuite | Finale Debug-Renderer-Suite 20/20 und komplette Release-Suite 24/24 bestanden, beide Exit 0 |
| 32 Sonderfälle | Reale LOD-Übergänge nach Relog plus 224 bestandene Hair-LOD-Paritätsfälle vom Nutzer ausdrücklich als Ersatznachweis akzeptiert; separate manuelle Nah/Fern/Nah-Folge und optionaler Mount-Test im finalen Relog-Lauf nicht durchgeführt |
| 33 B5-X | **GO gesetzt, B4-X vollständig abgeschlossen.** B5-X nicht begonnen; CPU bleibt Produktionsdefault, kein Production Switch |

## Git und Scope

Uncommittete Änderungen ausschließlich im Sourceprojekt, keine Commits/Pushes/Resets. Installierte Runtime-EXE nicht ersetzt, Originalpakete nicht verändert. Eigene Integration mit `ZiiNAN` markiert. Bestehende B2/B3-Tests nicht entfernt. Neue Testdateien und dieser Bericht sind noch untracked.

Der Gesamtdiff gegen B3 enthält weiterhin die zuvor implementierte B4-X-Erweiterung. Die freigegebene Haar-LOD-Korrektur beschränkt den zusätzlichen Produktcode auf die oben genannten fünf Dateien in `EterGrnLib`; hinzu kommen gezielte Tests, die Korrektur der privaten Testfixture und dieser Bericht. `git diff --check` bestanden; nur bestehende LF-/CRLF-Normalisierungshinweise, keine Whitespacefehler.

Die hier freigegebene native Haar-LOD-Binding-Aktualisierung ist umgesetzt. Darüber hinaus keine produktive Hair-/LOD-Änderung. Der finale Abnahmeschritt aktualisiert ausschließlich diesen Bericht; keine Code-/Teständerung, keine Performance-Optimierung und keine Änderung des Produktionsdefaults. **B4-X vollständig abgeschlossen, GO für B5-X; kein B5-X begonnen.**
