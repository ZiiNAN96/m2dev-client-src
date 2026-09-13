# Milestone 5C: bestehende Actor-Attachments

> Historischer Milestone-Stand. Aktuell seit [M12](renderer-milestone12.md): Diligent D3D11 ist im ON-Build der Default; Legacy wird explizit mit `--renderer=legacy-d3d9` gewählt. OFF-Builds bleiben Legacy-only. Frühere Testergebnisse und Auswahlbeschreibungen unten gelten für ihren damaligen Stand.

## Umfang und Entscheidung vor der Umsetzung

Baseline: `8c4ee3a`, M5B einschließlich deterministischer Materialzustände. Legacy D3D9Ex bleibt Standard. Der Nutzer hat für Schilde ausdrücklich **Legacy-Verhalten** gewählt: Ausrüstung ohne sichtbares Schildmodell. Kein neues Schildsystem, keine neuen Netzwerkfelder oder Assets.

Nur vorhandene Waffenmodelle und Haare; keine Mounts, Pets, Effekte, Weapon Glow/Trails, UI, Bäume, neue Materialien oder Animationstechnik. NPCs/Mobs profitieren nur, falls sie tatsächlich dieselben existierenden Teile besitzen. Ihre normale Registrierung reserviert nur PART_MAIN.

## Analyse vor Codeänderungen

### Besitz, Call-Chains, Teile (Fragen 1–6, 24–25)

`UserInterface/InstanceBase.cpp::Create/SetWeapon/SetHair/Refresh` → `GameLib/CActorInstance` → `EterGrnLib/CGraphicThingInstance` → `CGrannyLODController` → `CGrannyModelInstance`.

`GameLib/RaceData.h` definiert MAIN=0, WEAPON=1, HEAD=2, WEAPON_LEFT=3, HAIR=4. HEAD hat hier keinen visuellen Setter. `Packet.h::ECharacterEquipmentPart` enthält Armor/Weapon/Head/Hair, kein Schild. `WEAR_SHIELD` ist nur Ausrüstung; Schildzeilen 13000ff in `assets/locale/locale/common/item_list.txt` enthalten Icon, kein Modell.

Waffe: `ActorInstanceAttach.cpp::AttachWeapon(item)` → native Auswahl rechter/linker Hand → `RegisterModelThing/RegisterLODThing` → `SetModelInstance` → `AttachModelInstance(parent,bone,child)` → LOD-Controller → `SetParentModelInstance`. Linkes Modell ist `CItemData::GetSubModelThing`, bei fehlendem Submodell dasselbe Modell wie rechts. Eigene ModelInstance und Materialpalette je Hand. ModelSource wird über CGraphicThing geteilt, die GPU-Instanzressourcen bleiben pro ModelInstance.

Haare: `ActorInstanceData.cpp::SetHair` → MSM `CRaceData::SHair` → `SetModelInstance(HAIR,HAIR,0,MAIN)` → `SetLinkedModelPointer`. Eigene ModelInstance/Palette, aber über Pointer auf die aktuelle Körperinstanz geteilte Skeleton/Pose. MSM SourceSkin→TargetSkin wird auf die Haarpalette angewandt, nicht auf den Körper. Texturbilder stammen aus dem bestehenden ResourceManager; Diligent-Texturen verbleiben im jeweiligen Instanzcache.

Weitere native AttachingData: EFFECT (ausgeschlossen), COLLISION_DATA (keine Rendergeometrie), OBJECT (hier leere switch-Zweige, kein implementierter Zubehörpfad). Waffen-Trace ist ein separater Effect/Trace-Pfad und bleibt ausgeschlossen. Keine generische Migration beliebiger Effekte oder anderer CGraphicThingInstances.

### Bones, Transformation, Hände, Races (Fragen 7–18)

Die Registrierung in `assets/root/playersettingmodule.py` ist für beide Geschlechter derselben Klasse gleich:

| Klasse | Rechte Hand | Linke Hand |
| --- | --- | --- |
| Warrior | equip_right_hand | keine registrierte Waffenbindung |
| Assassin | equip_right | equip_left |
| Sura | equip_right | keine registrierte Waffenbindung |
| Shaman | equip_right | equip_left |

Bone-Indizes sind **nicht konstant**: `ModelInstanceModel.cpp::SetParentModelInstance` ermittelt sie mit dem vorhandenen `GetBoneIndexByName`/`GrannyFindBoneByName` im aktuellen Parent-Skeleton. LOD-Wechsel binden in `GrannyLODController::RefreshAttachedModelInstance` Parent und Children erneut über die Bone-Namen. Keine neuen Bone-Indizes oder Race-Transformtabellen.

Native Reihenfolge: `INSTANCEBASE_Transform` → `INSTANCEBASE_Deform` → `CGraphicThingInstance::OnDeform`, Teile in Slotreihenfolge (Körper vor Händen und Haaren) → `CGrannyModelInstance::Deform` → `UpdateWorldPose` → `UpdateWorldMatrices`. Waffenpose übernimmt die existierende Parent-Bone-Matrix in `GrannySampleModelAnimationsAccelerated`. Haare überspringen eine eigene Poseberechnung und verwenden das Körper-Skeleton mit nativer Mesh-Bone-Zuordnung.

Die tatsächliche D3DX-Zeilenvektorreihenfolge lautet für starre Meshes:

`meshWorld = nativeCompositeBoneMatrix[mappedMeshBone] * actorWorld`

Der vorhandene Composite-Pose-Puffer enthält bereits Parent-Bindung und Modelltransformation. Skinned Meshes erhalten `meshWorld = actorWorld`, ihre CPU-PNT-Vertices sind bereits verformt. Diligent übernimmt **die fertige `m_meshMatrices[mesh]`**, nicht nochmals Bone/Local/Actor multiplizieren. Kein Handedness-Wechsel, keine zusätzliche Spiegelung oder Transposition. Bestehende Normalen-Inversetransposition bleibt im gemeinsamen 5B-Pfad.

Schwert/Zweihand/Glocke/Fächer: rechts; Zweihand wird durch Körperanimation, nicht zweite Waffeninstanz geführt. Dolche: beide Hände. Bogen: links, rechts keine neue Bogeninstanz. Fächer links zusätzlich nur beim Reiten (ausgeschlossen). Keine zusätzliche Waffentyp-Rotation oder Skalierung in AttachWeapon: vorhandenes Modell, Bone, Actor-Transform und Animation bestimmen diese. Race/Gender unterscheiden sich durch Modelle, Skeletons und Motiondaten. Haare haben keine neue hartcodierte Head-Bone-Bindung.

### Materialanalyse und reale Assetprobe (Fragen 19–23)

Mit vorhandenem Granny-Inspektor gelesen: `build/milestone5c/asset-inventory.jsonl`, zunächst 7 Waffenmodelle (10,20,1000,2000,3000,4000,5000) und je hair_1_1/hair_2_1 für alle 8 Races/Geschlechter. Die Typzuordnung wurde anhand der originalen Item-Tabelle berichtigt: **4000 ist Amija/Dolch, 5000 ist Glocke, 7000 ist Fächer**. 7000 ist zusätzlich in `fan-inventory.jsonl` erfasst. Der abschließende Test prüft diese Subtypes vor dem Start über die originalen Client-Itemdaten; die früheren Testläufe zählen nicht als vollständiger Sechs-Waffentypen-Nachweis.

Alle 8 geprüften Waffenmodelle: starres PNT-Mesh, 1 Bone, keine eigene Animation. Glocke 5000 ist Two-Sided; Fächer 7000 hat 568 Vertices und keine Two-Sided-Metadaten. Alle 16 Haare: skinned PNT, keine eigene Animation, 74–101 Skeleton-Bones. Warrior-M/Sura-M hair_2_1 haben explizite Opacity-Texturen, mehrere Haare Two-Sided. Shaderparameter nicht aus dem Dateinamen oder aus dieser Probe ableiten: maßgebend bleibt der tatsächliche native Drawzustand.

`CGrannyMaterialPalette`/`CGrannyMaterial::ApplyRenderState` → `RenderMeshNodeListWithOneTexture` → vorhandener Renderer-Callback → originaler D3D9-Draw → Restore. Diffuse/Opacity verwenden native Gruppen und Stage-0-Textur. Weapon-Material setzt die bestehende Specular-Konfiguration mit SphereMapIndex=1; diese Funktion existiert bereits im 5B-Pfad und ist kein Weapon Glow. Actor Fade/Blend, Alpha-Test, Two-Sided/Cull, Depth, Materialfarbe, Texturfilter/Mips und Overrides werden unverändert pro Materialdraw erfasst. Keine neue additive/emissive Engine; Effekte/Trails bleiben Legacy und werden nicht in Diligent zusammengesetzt.

### Ladezeit und minimale Architektur

`CResourceManager::GetResourcePointer` erstellt/holt nur einen Resource-Pointer; AddReference über `RegisterModelThing/RegisterLODThing` löst `CResource::OnConstruct/Load` aus. `CGraphicThing::LoadModels` gibt anschließend originale Granny-Rigid-Vertex-/Index-Sektionen frei. Deshalb erforderliche starre Attachment-Daten **beim originalen Laden** erhalten, nicht später aus freigegebenen Sektionen lesen oder einen neuen Loader bauen.

Geplante Eingriffe:

1. `ItemData.cpp`, `ActorInstanceData.cpp`, `EterGrnLib/Thing.h/.cpp`, `Model.h/.cpp`: semantisch registrierte Item-/Haarmodelle vor dem Laden markieren; nur dort zusätzlich rein starre Actor-PNT-Daten erhalten. Kein globales Erfassen aller statischen Weltmodelle. Originale Granny-Sektionen werden unverändert freigegeben.
2. `Renderer/ActorRenderData.h`, `GameLib/ActorRenderBridge.h/.cpp`, `ActorInstance.cpp`: eng begrenzte aktuelle ModelInstance-Menge MAIN/WEAPON/WEAPON_LEFT/HAIR statt ausschließlich MAIN. Keine neue Draw-Reihenfolge; Callback innerhalb der originalen Materialschleife verwenden.
3. `EterGrnLib/ModelInstanceUpdate.cpp`: vorhandene fertige CPU-Daten für Haare erfassen; bei starren Waffen nur bestätigen, dass die nativen Weltmatrizen im aktuellen Frame aktualisiert wurden. Kein zusätzliches Skinning.
4. `DiligentActorRenderer.h/.cpp`: bestehende immutable Mesh-Uploads für rein starre Attachments, bestehende dynamische Uploads für skinned Teile; derselbe 5B-Materialrenderer. Ressourcen/Draws nach Part zusätzlich zählen; kein globaler Reset nach jedem Draw.
5. `TerrainPresentation.cpp`: begrenzte Attachment-Zähler und Shutdown-Ressourcennachweis; Tests und isolierte Originalasset-Fixtures ergänzen.

Stale Posen und fremde Instanzen bleiben ausgeschlossen. Originale Instanzfreigabe beseitigt GPU-Bindings und Besitzreferenzen bei Swap/Despawn/Shutdown. Keine neue Skeleton-Clock, keine spätere zweite Attachment-Updatephase (kein zusätzlicher Frame Versatz).

## Zusätzlich ausdrücklich freigegebene Legacy-Korrektur

Der erste direkte Originalasset-Wechsel Dolche → Bogen reproduzierte einen schon vorhandenen Fehler: `CActorInstance::AttachWeapon` legte die neue linke Waffe an, ließ den bisherigen rechten Dolch aber bestehen. Im ersten Diligent-Test waren deshalb 12 statt 10 Handmodelle aktiv (die damalige Testauswahl enthielt zusätzlich Amija-Dolche an den Schamanen). Mit identischer alter Testauswahl und der Korrektur sind es 10; mit der abschließend korrigierten Sechs-Typen-Auswahl werden 8 erwartet. Der Legacy-Vergleich verwendet denselben nativen Instanzzustand. Bogen → Einhandwaffe kann entsprechend den alten linken Slot behalten. Kein PSO-/SRV-/Matrixleck.

Der Nutzer hat die minimale Hand-Slot-Bereinigung **für beide Backends** ausdrücklich freigegeben. Änderung ausschließlich in `ActorInstanceAttach.cpp::AttachWeapon`: die bestehenden `__IsRightHandWeapon`/`__IsLeftHandWeapon`-Entscheidungen einmal ermitteln; unbenutzte Hand über vorhandenes `RegisterModelThing(NULL)`/`SetModelInstance` leeren. `CGrannyLODController::Clear` löst die alte Parent-Verbindung und gibt die Instanz samt Grafikressourcen frei. Keine neue Handregel, keine Mount-Migration. Abgesehen von dieser freigegebenen Fehlerkorrektur bleibt der native Render-/Animationspfad unverändert.

Testfolge erweitert um direkte Dolche → Bogen → Einhandschwert → keine Waffe sowie Bogen-Spawn ohne Vorgänger. Haar-Variante B nutzt die echten MSM-IDs 1001/2001/3001/4001 je Klasse (beide Geschlechter), nicht eine gemeinsame Test-ID.

## Validierung / Abschluss — 12.09.2026

**5C ist für den vereinbarten vorhandenen Waffen-/Haarpfad abgeschlossen.** Die normale Legacy- und Diligent-Ingame-Sichtprüfung wurde vom Nutzer bestätigt. Schilde sind ausdrücklich **N/A als visuelles Modell**, nicht migriert: unveränderte native Ausrüstung gemäß Nutzerentscheidung. Dies ist keine uneingeschränkte Freigabe sämtlicher ursprünglicher 5B-Actor-Kategorien oder des gesamten Diligent-Clients.

### Builds und automatische Tests

Alle Nachweise liegen unter `build/milestone5c/`; keine Tests wurden deaktiviert.

| Prüfung | Ergebnis | Nachweis |
| --- | --- | --- |
| Release Diligent ON nach Hand-Slot-Korrektur | Exit 0 | `build-on-hand-fix.log` |
| Vollständige Suite nach Korrektur | **10/10**, 492,27 s, Exit 0 | `full-suite-hand-fix.log` |
| Release Diligent OFF | Exit 0 | `build-off.log` |
| Renderer im OFF-Build | **4/4**, 0,57 s, Exit 0 | `renderer-off.log` |
| Release ON abschließend wiederhergestellt | Exit 0 | `build-on-restored.log` |
| Renderer im wiederhergestellten ON-Build | **6/6**, 9,94 s, Exit 0 | `renderer-on-restored.log` |
| Originalasset-Fixture Legacy | 26/26 Phasen, Exit 0 | `verified-items/legacy-runtime/actor-attachments-test.log` |
| Originalasset-Fixture Diligent | 26/26 Phasen, Exit 0 | `verified-items/diligent-runtime/actor-attachments-test.log` |
| Strikter Fixture-/Ressourcen-Verifier | PASS für beide Backends | `verified-items/verification.log` |

Die vollständige Suite umfasst die vorhandenen Kompressions-/Script-Tests und alle sechs Renderer-Tests. Der echte D3D9/D3D11-Pixelvergleich läuft zusätzlich mit dem neuen **RigidAttachmentGpuAdapter durch 37 Material-/Transformfälle**. Die bestehenden dynamischen Actor-/Terrain-/Objekttests und die **720 5B.1-Materialreihenfolgen** bleiben aktiv. Die Attachment-Lifetime-Prüfung wiederholt Erzeugung, mehrfachen Draw, Verbergen, Freigabe und Neuerzeugung dreimal. Sie prüft ausdrücklich: starre Waffen verursachen keinen dynamischen Vertex-Upload und kein zusätzliches Skinning; nur skinned Haare verwenden den vorhandenen Uploadpfad.

Bekannte Linkerwarnungen über fehlende PDB-Dateien statisch eingebundener Bibliotheken bleiben vorhanden (u. a. LNK4099); beide Builds sind erfolgreich. Der abschließend bereitliegende Build ist **ON**, jedoch bleibt die Startauswahl ohne Argument **Legacy**.

### Originalasset-Fixture und manuelle Sichtprüfung

`tests/Renderer/actor_attachments_smoke.py` lädt die originalen Race-/Bone-/Motion-Registrierungen und verifiziert die Waffen-Subtypes vor dem Test. Alle acht Races/Geschlechter werden parallel dargestellt. Die 26 Phasen decken Folgendes ab:

- Schwert, Zweihand, Dolche, Bogen, Fächer und Glocke; direkte Wechsel keine Waffe → A → B → C → keine Waffe, danach erneut anlegen.
- Hair A → klassenabhängiges Hair B → A; Geschlechtswechsel am bestehenden VID, anschließend zurück.
- Idle, Walk, Run, Attack, Hit, Death und Skill-Animation ohne Migration des Effekts; Kamera aus wechselnden Richtungen.
- Actor-Fade einschließlich Haar-/Waffenmaterial, außer Sicht bewegen/zurückholen, Hide/Show, Despawn/Respawn und endgültiges Löschen.
- Originale Map-Ladungen **A1 → B1 → A1** samt Actor-Neuaufbau, Texturen und Ressourcenfreigabe. Kein Serverzugriff durch diese Fixture.

Die finale Sechs-Waffentypen-Fixture enthält keine Actor-/Attachment-Ausschlüsse oder Material-/Uploadfehler. Erwartete Hand-/Haarzahlen sind protokolliert: Set A **10 Waffen + 8 Haare**, Set B **8 Waffen + 8 Haare**, ohne Waffen **0 + 8**, nach Despawn **0 + 0**. Alpha-Test der originalen Warrior-Haare sowie Alpha-Blend beim Fade beider Attachmentarten sind im Materialprotokoll belegt. Das beseitigte doppelte Handmodell ist damit deterministisch nachgetestet.

| Lauf | PID | Dauer | Exitcode | Sichtprüfung |
| --- | --- | --- | --- | --- |
| Finale Originalasset-Fixture Legacy | 63448 | 265,6 s | 0 | Automatische Phasen/Verifier vollständig |
| Finale Originalasset-Fixture Diligent | 52084 | 265,6 s | 0 | Automatische Phasen/Verifier vollständig |
| Normaler Legacy-Start ohne Renderer-Argument | 58200 | 70,2 s | 0 | Nutzer: „sieht gut aus“ |
| Normaler Diligent-Start mit Originalpaketen | 60492 | 125,3 s | 0 | Nutzer: „alles korrekt“ |

Legacy-Ingame-Abfrage umfasste Waffen/Haare/Rüstung, Charakter/NPCs/Mobs, Animationen, Terrain/Gebäude/Bäume, Effekte und UI. Der optionale manuelle vollständige Waffenwechsel wurde nicht separat bestätigt; dessen verbindlicher Nachweis ist die automatisierte Fixture.

Diligent-Ingame-Abfrage umfasste Waffe/Haare bei Stehen, Laufen, Angriff, Kamera aus mehreren Winkeln und dreimal Minimize/Restore. Der Nutzer bestätigte alles als korrekt. Der normale Lauf protokolliert einen Spieler mit **einem Waffen- und einem Haarteil**, bis zu 13 NPCs und bis zu 77 Mobs (jeweilige Maxima, nicht als gleichzeitige Gruppe zu verstehen).

Die vorherigen isolierten Sichtläufe `regression-final` mit der bereits korrigierten Produktionsdatei wurden ebenfalls für dreimal Minimize/Restore bestätigt: Diligent PID 35336 „sieht gut aus“, Legacy PID 308 „passt“. Dort war nur die Testauswahl der Glocke/des Fächers noch unvollständig. `regression-final`, `verified-items` und `normal-final` verwenden dieselbe getestete ON-EXE mit SHA-256 `BF817F5D472F7191F935FC04C6777951E5DB3E34AFE2A3A06D01A9DE4B846D91`. Nach den Sichtprüfungen keine weitere Produktionscodeänderung; abschließend ON neu gebaut und Renderer-Tests wiederholt.

### Resize, Shutdown und Ressourcen

Mehrfaches **Backend-Resize** und Nullgröße/Restore sind im nativen GPU-Vergleich einschließlich starrer Attachments enthalten: 640×480 → 800×600, wiederholte Resize-Aufrufe und drei Minimize/Restore-Zyklen innerhalb der 37 Fälle. Der bestehende Präsentationstest prüft zusätzlich 640×480, 800×600, 320×240, 1024×768 und Restore. Der Benutzer bestätigte reales Minimize/Restore beider Testfenster sowie des normalen Diligent-Clients. Ein freies Ziehen der normalen Fensterkante wurde nicht separat nachgewiesen.

Sowohl der finale isolierte Diligent-Lauf als auch der normale Diligent-Ingame-Lauf enden mit:

```text
shutdown actor_geometry=0 actor_textures=0
shutdown attachment_geometry=0 attachment_textures=0
shutdown object_geometry=0 object_textures=0
```

Die neuen Attachment-Zähler halten nur schwache Referenzen, verlängern also keine Lebensdauer. Sichtbar gezählte Teile und Material-Drawcalls sind unterschiedliche Größen. `shield_draws=0` dokumentiert den fehlenden visuellen Legacy-Pfad. Die Nullwerte belegen die erfassten Modell-/Texturressourcen, nicht die Abwesenheit sämtlicher Treiber- oder allgemeiner Prozesscaches.

Messung im Fünfsekundenraster (`*-resources.csv`):

| Lauf | Private Memory Maximum | Letzte Probe vor Exit | Handles Maximum / letzte Probe |
| --- | --- | --- | --- |
| Isoliert Legacy | 456,3 MB | 423,2 MB | 787 / 779 |
| Isoliert Diligent | 589,6 MB | 532,7 MB | 1127 / 1115 |
| Normal Legacy | 442,6 MB | 438,0 MB | 822 / 818 |
| Normal Diligent | 609,7 MB | 584,6 MB | 1164 / 1164 |

Map-/Asset-Ladevorgänge erhöhen zeitweise den Verbrauch; nach Löschen fällt er wieder. In den begrenzten Läufen kein stetig wachsender Attachment-Bestand, kein beobachteter Crash, keine Assertion und kein protokollierter Renderer-/Device-Fehler. Das ist **kein Langzeit-Leaknachweis** für den gesamten Client. Ingame-Prozessverbrauch und GPU-Ressourcenzähler nicht gleichsetzen.

### Bekannte Grenzen und nicht bestandene/nicht ausgeführte Punkte

- **Serverseitiger Teleport und Relog im selben Prozess: ungeprüft.** Die Offline-Mapfolge und getrennte Clientstarts ersetzen diese Nachweise nicht. Dafür keine Server-/UI-/Netzwerkinfrastruktur ergänzt.
- **Schilde:** kein visueller Legacy-Attachmentpfad. Bone-/Spiegelungs-/Sichtbarkeitstest N/A; kein sichtbares Modell und kein neues Feature erfunden.
- **Bestehende 5B-Kategoriegrenzen unverändert:** Player race < 8, NPC type 1/race < 20100, Mob type 0/race < 8000; gerittene Actors weiterhin ausgeschlossen. Im normalen Lauf wurden deshalb unter anderem Wachen 20340ff, Metin 8003 und ein Mount 20114 ausgelassen. Rein starre NPC-Körper wie Silber/Gold 20051/20052 liegen weiterhin außerhalb des bestehenden Body-PNT-Snapshotvertrags. Die gezielte Freigabe starrer **Attachments** erweitert nicht heimlich diese Körperkategorien.
- **Nicht migriert:** Mounts, Pets, sichtbare generische Accessory-OBJECT-Pfade (hier nicht implementiert), Effekte/Weapon Glow/Trails, Schatten, SpeedTree, Wasser, Ingame-UI/Nameplates/Damage Numbers. Native Logik kann weiterhin laufen; das bedeutet nicht, dass diese Systeme im Diligent-Weltbild zusammengesetzt werden.
- **Originaldaten-/Legacy-Meldungen:** Die isolierten Tests melden ausschließlich die auf beiden Backends fehlende Datei `sound/pc2/assassin/bow/attack1.wav`. Normale Starts melden `invalid idx 0` aus dem bestehenden MarkManager. Legacy zusätzlich einmal `Cannot find item by 1280` aus dem Item-Lookup; dieser einzelne Daten-/UI-Aufruf wurde nicht behoben oder als Renderer-Fehler umgedeutet. Diligent protokolliert vorhandene `AddDamageEffect`/`ProcessDamage`-Diagnostik, einschließlich des Spielereignisses `CRITICAL`; keine Renderer-Assertion. Diese Stellen wurden nicht verändert.
- Keine erschöpfende Prüfung aller privaten/neuen Item-Assets, jeder Rüstungs-/Haar-Kombination oder aller LOD-Übergänge. Die Integration übernimmt die nativen Bindungen und Materialzustände; sie ersetzt keine Inhaltsvalidierung.

### Abschlussabgleich der 26 Berichtspunkte

1. **Call-Chain:** InstanceBase → ActorInstance → GraphicThingInstance → LODController → ModelInstance; native Materialschleife → ActorRenderBridge → DiligentActorRenderer.
2. **Klassen/Dateien:** oben analysiert und unten im Diff gruppiert; keine neue Attachment-Engine.
3. **Bones:** native Namen/Granny-Indizes, Parent-/Linked-Model-Bindung unverändert.
4. **Transformation:** fertige native Mesh-Weltmatrix übernommen; tatsächliche Reihenfolge oben dokumentiert.
5. **Waffen:** alle sechs vorhandenen Typen mit originalen Subtypes getestet.
6. **Schilde:** Ausrüstung ohne visuellen Pfad; Nutzerentscheidung umgesetzt.
7. **Haare:** eigenes skinned Teil mit Körper-Skeleton; native Materialpalette/Overrides übernommen.
8. **Race/Gender:** alle acht vorhandenen Varianten und Wechsel getestet; keine neue Race-Tabelle im Renderer.
9. **Material:** unveränderter deterministischer 5B.1-Material-Bind, keine neue Pipeline-Engine.
10. **Alpha-Test:** originale Opacity-Haare plus GPU-Vergleich bestanden.
11. **Alpha-Blend:** Actor-Fade auf Waffen/Haaren plus GPU-Vergleich bestanden.
12. **Cull/Depth:** native Werte einschließlich Two-Sided, Spiegelung/Skalierung und Verdeckung im Pixelvergleich; kein globaler Reset.
13. **Synchronität:** Körper vor Attachments, aktueller Frame verpflichtend; native Matrizen/CPU-Pose, Sichtprüfung ohne wahrgenommenes Nachziehen. Kein separater Frame-Lag-Messapparat.
14. **Equipment-Wechsel:** direkte Wechsel und fehlende Waffen geprüft; freigegebene Hand-Slot-Bereinigung verhindert alte zweite Waffen.
15. **Spawn/Despawn:** Fixture und dreifache GPU-Lifetime-Zyklen bestanden.
16. **Mapwechsel:** native Offline-Ladung A1 → B1 → A1 auf beiden Backends bestanden.
17. **Relog/Teleport:** echter Serverpfad ausdrücklich ungeprüft.
18. **Resize:** mehrfach automatisiert auf Backend-/Präsentationsebene bestanden.
19. **Minimize/Restore:** automatisiert und mehrfach vom Nutzer bestätigt.
20. **Shutdown:** finale isolierte und normale Starts jeweils Exit 0.
21. **Ressourcen:** erfasste Attachment-/Actor-Geometrien und Texturen nach Shutdown 0.
22. **Builds:** Release ON/OFF erfolgreich; endgültiger Build wieder ON mit Legacy-Default.
23. **Testsuite:** vollständig 10/10, Renderer ON 6/6 und OFF 4/4; kein Test deaktiviert.
24. **Legacy-Regression:** normale Ingame-Sichtprüfung bestätigt; nur ausdrücklich erlaubte Hand-Slot-Fehlerkorrektur verändert natives Verhalten.
25. **Sonderfälle:** Schilde, Mount-Handregeln, starre ausgeschlossene NPC-Körper, nicht migrierte Effekte und Testgrenzen oben dokumentiert.
26. **Empfehlung:** vor einer breiten Gesamtabnahme die optionalen echten Relog-/Teleportfälle separat nachholen. Einen nächsten Migrationsmilestone erst nach neuer ausdrücklicher Beauftragung planen; hier keinen beginnen.

### Git-Diff und Übergabe

21 bereits versionierte Dateien geändert: 16 Produktionsdateien und 5 Renderer-Testdateien; dazu diese neue Dokumentation und 4 neue Fixture-/Setup-/Verifikationsdateien. Der versionierte Diff enthält 259 hinzugefügte und 55 entfernte Zeilen; neue Dateien sind darin nicht enthalten.

- `src/EterGrnLib/{Thing,Model}.{h,cpp}` und `ModelInstanceUpdate.cpp`: eng markierte Attachment-Snapshots und Übernahme bereits berechneter nativer Daten.
- `src/GameLib/ItemData.cpp`, `ActorInstanceData.cpp`: echte Item-/Haarressourcen vor dem Laden markieren.
- `src/GameLib/ActorInstance.cpp`, `ActorInstanceRender.cpp`, `ActorRenderBridge.{h,cpp}` sowie `src/Renderer/ActorRenderData.h`: Scope vom Körper auf die vier erlaubten nativen Teile erweitern; Instanz-/Frame-Zuordnung bleibt strikt.
- `src/GameLib/ActorInstanceAttach.cpp`: ausschließlich genehmigte unbenutzte Hand-Slots bereinigen.
- `src/Renderer/DiligentActorRenderer.{h,cpp}`, `TerrainPresentation.cpp`: vorhandener starrer/dynamischer Uploadpfad und begrenzte Attachment-Diagnostik.
- `tests/Renderer/{ActorGpuAdapter.h,ActorGpuChecks.h,ActorPolicyTest.cpp,StaticObjectGpuChecks.h,TerrainGpuTest.cpp}`: Policy-, Material-/Pixelvergleich und Ressourcenabdeckung ergänzt.
- Neue `tests/Renderer/actor_attachments_{entry,smoke}.py`, `setup_actor_attachments.ps1`, `verify_actor_attachments.ps1`: isolierte Originalasset-Tests. Das Setup verwendet vorhandene 5A-Testbootstrap-/Konfigurationsartefakte unter `build/milestone5a`; es ist kein eigenständig herunterladender Clean-Checkout-Bootstrap.

Wesentliche neue Integrationsstellen sparsam mit `ZiiNAN` markiert. `git diff --check` ohne Whitespacefehler. Keine Originalpakete oder Original-Runtime-EXE überschrieben, keine Serverdateien verändert. Die bereits vorhandene Änderung an `m2dev-client/config/channel.inf` bleibt unangetastet. Keine Commits/Pushes und keine weitere Migration.
