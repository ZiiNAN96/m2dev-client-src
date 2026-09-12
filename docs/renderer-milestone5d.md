# Milestone 5D — vorhandene Reittiere über Diligent

## Analyse und Architektur vor Produktionsänderungen

Baseline `f8c2245` (5C), sauberer Source-Checkout. Nur Mount/Reiter, keine Pets, Effekte, Schatten, Bäume, Wasser, UI oder neue Grafikfeatures. Legacy bleibt Default; keine neue Granny-/Skinning-/Materialengine.

### Besitz, Race und Lebenszyklus (Analyse 1–9, 16–18)

`UserInterface/InstanceBase.h/.cpp::CInstanceBase::SHORSE` besitzt mit `m_pkActor` eine **eigene CActorInstance**. Sie hat eine eigene RaceData, CGraphicThingInstance/ModelInstance, Skeleton/Pose, Animationskontrollen und Materialpalette. Sie ist kein Waffen-Part und keine zweite CInstanceBase im CharacterManager. Der native Actor-Typ ist zunächst TYPE_PC; deshalb ist eine pauschale Race-/Typfreigabe ungeeignet.

Normale Kette: `PythonNetworkStreamPhaseGameActor.cpp` liest `dwMountVnum` → `NetworkActorManager.cpp::__AppendCharacterManagerActor` kopiert `m_dwMountVnum` in SCreateData → `CInstanceBase::Create` → `MountHorse(race)` → `SHORSE::Create` → `CActorInstance::SetRace/SetShape(0)` → vorhandener ResourceManager/Granny. RaceManager löst die originale `assets/root/npclist.txt` samt Quellnamenalias in MSM und Motlist auf; MSM bestimmt GR2, Texturen, Kollision/Effects. Keine neue Mount-Race-Tabelle im Renderer.

`CActorInstance::MountHorse` speichert auf der Reiterseite den nativen `m_pkHorse`-Pointer; SHORSE besitzt den Mount. `m_isMounting` wird erst nach erfolgreichem SetRace gesetzt. Die Bewegung beginnt am bisherigen Player-Pixelpunkt. SHORSE stellt Bewegung/Angriffsgeschwindigkeit, MODE_GENERAL und native Spawn-Alpha 0 → 1 ein.

Normale Mount-/Dismount-/Mount-Wechsel kommen als Actor-Neuanlage: NetworkActorManager löscht die alte CInstanceBase und erzeugt dieselbe VID mit neuer SCreateData. Beim Auf-/Absteigen wird die bisherige Position übernommen. `CInstanceBase::Destroy` → `DismountHorse` → `SHORSE::Destroy` zerstört Mount, anschließend wird der Player zerstört. Ein isolierter Aufruf des alten Python-Helpers `chr.DismountHorse` zerstört dagegen nur den Mount, ohne den Reiter-Pointer und seine Modi vollständig zurückzusetzen; dieser Helper ist **nicht** der vollständige normale Netzwerk-Lifecycle. Für Tests denselben Delete/Create-Lifecycle verwenden, keinen unabhängigen Mountingmechanismus erfinden.

### Verknüpfung, Matrix und Animationsreihenfolge (Analyse 10–15)

`CInstanceBase::__AttachHorseSaddle` → `mount.AttachModelInstance(PART_MAIN,"saddle",rider,PART_MAIN)` → `CGrannyLODController` → `CGrannyModelInstance::SetParentModelInstance`. Der Index wird nativ über GrannyFindBoneByName ermittelt. `Create`, Armor-Refresh und bestimmte Battle-Zustände binden den Sattel erneut. Keine hartcodierte Sitzhöhe im Renderer.

`CActorInstance::INSTANCEBASE_Transform` transformiert zuerst den Mount und übernimmt dessen aktuelle Pixelposition (einschließlich nativer Y-Konvention); SetRotation/BlendRotation/AdvancingRotation leiten an den Mount weiter. Der Mount und der Reiter behalten ihre jeweiligen nativen Weltmatrizen. Keine zusätzlichen mountabhängigen Render-Scale-/Rotationstabellen im untersuchten Pfad. Die Modell-/Bone-Daten bestimmen die Sitzhöhe und lokale Orientierung.

`CGrannyModelInstance::UpdateWorldPose` liest die vorhandene Parent-Sattelmatrix und übergibt sie an GrannySampleModelAnimationsAccelerated. CPU-skinned Reitervertices enthalten diese Bindung bereits; sie werden mit der nativen Actor-Weltmatrix gezeichnet. Rigid-Parts verwenden unverändert `nativeCompositeBone * actorWorld`. Diligent übernimmt fertige PNT-Daten und `m_meshMatrices[mesh]`; kein zusätzliches Multiplizieren des Sattels, kein Handedness-Wechsel.

`CInstanceBase::Update` → Reiter `MotionProcess`, Mount `HORSE_MotionProcess`; `ActorInstanceMotion.cpp::__SetMotion` setzt denselben Motionindex/Subindex für die unterschiedlichen nativen Motion-Modes mit Blendzeit/Loop/SpeedRatio. Mount hat seine eigene Clock/Pose. Der Player verwendet MODE_HORSE oder vorhandene MODE_HORSE_* je Waffe; Mount MODE_GENERAL. Idle/Walk/Run, Combo-Angriffe, Damage/Dead und Skill2 hängen von Motlist und nativer Level-/CanAttack-/CanUseSkill-Regel ab.

**Gefundene Reihenfolge:** `CInstanceBase::Deform` ruft bisher Reiter vor SHORSE auf, `Render` dagegen Mount vor Reiter. GetBoneMatrixPointer liest nur die vorhandene Pose; es aktualisiert den Parent nicht rekursiv. Für Diligent muss deshalb das tatsächliche Mount/Reiter-Paar einmal pro Durchlauf in der Reihenfolge **Mount deformieren → Reiter samt seinen Parts deformieren** durch die vorhandenen Methoden laufen. Kein zusätzliches Skinning und keine Änderung der Granny-Runtime. Der Legacy-Zweig behält seine Reihenfolge. Aktuelle Frame-Marken sichern gegen stale Posen; der genaue visuelle Effekt wird in den Tests verglichen.

### Materialien, Attachments, Sichtbarkeit (Analyse 19–24)

Der Mount läuft durch dieselben CActorInstance::OnRender-Diffuse-/Opacity-/Blend-Schleifen und Materialpaletten wie 5B. Start-Fade erfordert vorhandenes Alpha-Blend. CActorInstance::SetMaterialColor propagiert die native Farbe vom Player auf den Mount. Keine getrennte Mount-Materialengine; PSO/SRV/Sampler/Alpha/Cull/Depth bleiben der deterministische 5B.1-Bind.

Erste echte Dateiproben mit vorhandenem Granny-Inspektor:

| Race / Mount | Originalmodell unter Runtime `assets` | Meshes / Vertices | Materialaufbau |
| --- | --- | --- | --- |
| 20104 Pferd | NPC/ymir work/npc/horse/horse_normal.gr2 | 3 / 1855 | Körper + zwei Rüstungsteile, 3 diffuse Materialien |
| 20110 Keiler | patch2/ymir work/npc/boar/boar.gr2 | 1 / 2229 | wild_boar.dds |
| 20114 Weißer Löwe | patch2/ymir work/npc/lion_white/lion_white.gr2 | 1 / 4453 | lion_white.dds |
| 20225 Dinosaurier | metin2_patch_pet1/ymir work/npc/dinosaur/dinosaur_3.gr2 | 1 / 2373 | dinosaur_3.dds |
| 20219 Halloween-Pferd | metin2_patch_halloween/ymir work/npc/horse_halloween1/horse_halloween1.gr2 | 3 / 1876 | drei diffuse Materialien/Teile |

Alle fünf haben gewichtete PNT-Meshes, separate Animationsdateien, keine explizite Opacity-Map in dieser Probe. Native Submeshes/16-Bit-Indizes und CPU-Deformation entsprechen dem bestehenden Actorvertrag. Alpha-Test richtet sich nach der tatsächlich angewandten nativen Materialgruppe, nicht nach Race-Namen. Ein Paketname `pet1` macht einen als Mount eingesetzten Dinosaurier nicht zur Pet-Migration.

Bei den geprüften MSMs sind normale Mount-Rüstungsteile im Grundmodell, nicht separate Waffen-Parts. AttachingData enthält Kollision und beim weißen Löwen `ice_smoke1.mse`: ausdrücklich nicht migrieren. Horse Dust, Motion-/Skill-Effekte, Weapon-Traces und RenderToShadowMap bleiben separate unveränderte Legacy-Pfade. Playerwaffen/Haare verwenden 5C, einschließlich des nativen beidseitigen Fächers beim Reiten und der bereits korrigierten Hand-Slot-Bereinigung.

Visibility: `CInstanceBase::__CanRender` prüft native Reitersichtbarkeit/Frustum sowie Invisibility vor Deform und Render beider Actors. Die neue Rendererfreigabe muss genau innerhalb dieses gemeinsamen Scope bleiben. Keine neuen Cullingregeln, kein GPU-Culling. Keine pauschale Freigabe von NPC-/Pet-Races; NPCs/Mobs mit eigenen Sonder-Mounts werden nicht erfunden.

### Minimale geplante Änderungspunkte

1. `Renderer/ActorRenderData.h`, `GameLib/ActorRenderBridge.{h,cpp}`: kurzlebiger Scope mit exakt dem realen Rider-/Mount-Pointer; Mount/MountedPlayer-Kategorie nur in diesem Scope. Bestehende numerische 5B-Ausschlüsse bleiben unverändert. Mount und Rider vor Draw auf aktuelle, gültige CPU-Pose prüfen.
2. `UserInterface/InstanceBase.cpp`: nur die bestehende Deform-/Render-Integration um den Scope ergänzen; bei aktivem unterstütztem Diligent-Paar vorhandene Deform-Aufrufe in Abhängigkeitsreihenfolge. Kein Eingriff in Netzwerk, Mount-Erzeugung/-Zerstörung, Modi oder Sitzmathematik.
3. `GameLib/ActorInstance.cpp`, `ActorInstanceRender.cpp`: bisherigen Ausschluss gerittener Actors gezielt durch gemeinsame Scope-Auswahl ersetzen.
4. `Renderer/DiligentActorRenderer.{h,cpp}`, `TerrainPresentation.cpp`: derselbe Mesh-/Materialrenderer; begrenzte Mount-/Rider-/Upload-/Ressourcenzähler mit schwachen Ressourcenreferenzen.
5. `tests/Renderer`: Scope-/Category-/Lifetime-Prüfung, nativer Materialvergleich und isolierte Originalasset-Fixture mit Mount-/Dismount-/Wechsel-/Map-/Sichtbarkeitsfolgen. Normale Loginprüfungen mit Originalpaketen und Benutzerbestätigung; keine Authentisierung automatisieren.

## Implementierte Invarianten

`ActorMountPair` enthält nur zwei nicht besitzende Pointer. `MakeAnimatedMountPair` erzeugt ihn ausschließlich bei aktivem Diligent-Weltframe und einem unverformten normalen Player (race 0–7) mit seinem tatsächlichen SHORSE. Der Scope gilt innerhalb der bestehenden gemeinsamen Visibility-Abfrage und wird auch bei verschachtelten Aufrufen wiederhergestellt. Keine dauerhafte Mount-Liste und keine Suche nach vermeintlichen Mount-Races.

`RenderCategory` erlaubt genau diesen Mount trotz seines nativen TYPE_PC/IsPoly-Befunds, nicht fremde Actors mit demselben Race oder Pets. Die gewöhnlichen 5B-Kategorien bleiben unverändert. MountedPlayer zählt weiterhin als Player, der Mount zusätzlich separat; Attachment-Teile werden nicht zu Mounts umklassifiziert.

Die bestehenden Aufrufe erzeugen im Diligent-Zweig erst die Mount-Pose, anschließend die Rider-Pose und seine Waffen/Haare. Vor jedem Paar-Draw müssen **beide** Körper sichtbare, gültige Snapshots des aktuellen Frames besitzen. Der Renderer verändert weder Sattelmatrix, Granny-Clock, Motion-Modi noch ursprüngliche Vertex-/Index-/Materialdaten. Die neue Kategorie beeinflusst Uploadzählung und Diagnose, nicht Shader-/Materialkonfiguration. Geometrien und Texturen werden über vorhandene Instanzbesitzer freigegeben; Diagnose hält nur schwache Referenzen.

## Validierung — 12.09.2026

Milestone 5D ist für die geprüften normalen Player-/SHORSE-Paare abgeschlossen. Implementierung, isolierte Regression, normale Ingame-Sichtprüfungen und alle abschließenden Build-/Suite-Prüfungen bestanden. Die unten genannten optionalen Serverpfade und Sonderfälle bleiben ausdrücklich außerhalb dieser Abnahme. Alle Artefakte nachfolgend relativ zu `build/milestone5d`.

### Builds, Suite und GPU-Vergleich

| Prüfung | Ergebnis | Nachweis |
| --- | --- | --- |
| Erster Release ON | Exit 0 | `build-on-initial.log` |
| Erste Renderer-Prüfung ON | 6/6, 10,57 s, Exit 0 | `renderer-initial.log` |
| ON mit finaler Testausgabe | Exit 0 | `build-on-final.log` |
| Vollständige Suite | **10/10**, 475,65 s, Exit 0 | `full-suite.log` |
| Release OFF / Renderer OFF | Exit 0 / **4/4**, 0,56 s, Exit 0 | `build-off.log`, `renderer-off.log` |
| Abschließend Release ON wiederhergestellt | Exit 0 | `build-on-restored.log` |
| Abschließende Renderer-Prüfung ON | **6/6**, 10,51 s, Exit 0 | `renderer-on-restored.log` |
| Strikter Originalasset-Verifier | PASS für beide Backends | `visible-initial/verification.log` |

Build-Konfiguration abschließend `M2_ENABLE_DILIGENT_D3D11=ON`; der Programmstart bleibt gemäß `Renderer/StartupOptions.h` standardmäßig LegacyD3D9. Diligent wird weiterhin ausschließlich mit `--renderer=diligent-d3d11` gewählt. Der abschließende Linker meldet die bekannten fehlenden Python-/zlib-PDBs (LNK4099), keine Buildfehler. Nach den Ingame-Läufen wurde kein Produktionscode mehr verändert.

Der MountGpuAdapter durchläuft dieselben **37 nativen D3D9/D3D11-Material-/Transform-Pixelvergleiche** wie dynamische Actors. Darin: diffuse/specular/native Actorstages, Alpha-Test/Blend, Cull/Depth, Filtering/Mips, Matrixrotation/-skalierung, Resize und Nullgröße/Restore. Der bestehende 5B.1-Test mit **720 Materialreihenfolgen / 12 PSOs** bleibt unverändert aktiv. Neue dreifache Lifetime-Zyklen prüfen getrennte Mount-/Rider-/Attachment-Geometrien, mehrere Materialdraws bei nur einem Mount-Upload, Verbergen, Mount-Freigabe bei weiter bestehendem Reiter und abschließend sämtliche Besitzer auf 0.

Die Policy-Prüfung testet exakte Pair-Pointer, MountedPlayer, fremde Companion-Instanzen, verschachtelte Scopes, leere/ungültige Paare und Scope-Ende. Keine Tests deaktiviert. Historische 5A-Logverifier für damalige Builds bleiben unverändert; deren damalige absichtliche Mount-Ausschlüsse sind kein aktuelles 5D-Erfolgskriterium. Die heutige Paarfreigabe wird ausdrücklich im neuen `verify_mounts.ps1` geprüft.

### Originalasset-Testfolge

`tests/Renderer/mounts_smoke.py`: **33 Phasen × 8 s**, jeweils acht Reiter (alle vier Klassen/beide Geschlechter), fünf echte Mount-Modelle, originale Race-/MSM-/Granny-/Motion-/Item-Daten. Alle fünf Models besitzen CPU-skinned PNT-Daten; die Mount-Rüstungsstücke sind Teil ihrer Grundmodelle, nicht neu erfundene Attachments.

- A1 → B1 → A1 zu Fuß; später A1 → B1 → A1 beritten.
- Pferd Idle/Walk/Run/Attack; danach Keiler, weißer Löwe, Dinosaurier, Halloween-Pferd und zurück zum Pferd.
- Native Create/Delete-Neuanlage am gleichen VID bei Mountwechsel und mehrfach Fuß → Mount → Fuß → Mount; kein unvollständiger isolierter DismountHorse-Helper.
- Reiter mit Schwert/Zweihand/Dolchen/Bogen/Fächer/Glocke, inklusive beider nativer Fächer-Handslots beim Reiten. Direkter Wechsel auf Bogen bzw. Einhandwaffe behält keine alte Handgeometrie.
- Hair A/B und Rüstung 0/3; Native ChangeArmor bindet nach dem Test-Racewechsel den Sattel erneut. Bestehende Python-Test-CreateData gibt zunächst Warrior vor; anschließend werden die übrigen Races über vorhandene Setter/ChangeArmor eingerichtet. Keine neue Python-/Netzwerk-API.
- Kamera aus mehreren Richtungen, Hide/Show, außer Sichtbereich/zurück, vollständiger Despawn/Respawn und endgültiges Löschen.

Verifiziert: acht sichtbare Mounts zu acht Reitern; Pferde mit drei Materialgruppen ergeben **24 Mount-Draws bei nur 8 Mount-Uploads**. Ein-Mesh-Mounts ergeben 8 Draws/8 Uploads. Berittenes Set A ergibt **12 Waffen + 8 Haare**, Set B **8 Waffen + 8 Haare**. Nach Despawn und Shutdown sind alle erfassten Modell-/Texturressourcen 0. Kein `excluded:` oder `ERROR:` im finalen isolierten Actorprotokoll; beide isolierten syserr-Dateien sind leer.

Die fünf Originalmodelle hatten in der Probe keine explizite Opacity-Map. Native Spawn-Alpha/Blend ist tatsächlich im Mountprotokoll erfasst. Alpha-Test ist im gemeinsamen Mount-GPU-Vergleich geprüft; kein zusätzlicher originaler Alpha-Test-Mount wird ohne Nachweis behauptet. Der weiße Löwe besitzt den separaten `ice_smoke1.mse`-Effekt, der ausdrücklich nicht Teil des Diligent-Bildes wird.

### Ingame- und Fensterabnahme

| Lauf | PID | Dauer | Exitcode | Sichtprüfung |
| --- | --- | --- | --- | --- |
| Isoliert Diligent | 58200 | 266,0 s | 0 | Nutzer „passt“, einschließlich dreimal Minimize/Restore |
| Isoliert Legacy | 16704 | 266,0 s | 0 | Nutzer „stimmt aalles“, einschließlich dreimal Minimize/Restore |
| Normaler Legacy-Start ohne Renderer-Argument | 57760 | 33,6 s | 0 | Nutzer „paasst“ |
| Normaler Diligent-Start mit Originalpaketen | 52872 | 53,6 s | 0 | Nutzer „top“, einschließlich Minimize/Restore |

Die normale Legacy-Abfrage umfasste Auf-/Absteigen, Bewegung, Reiter-/Mount-Animation, Waffen/Haare/Rüstung und die übrige native Darstellung. Die normale Diligent-Abfrage umfasste Modell/Texturen/Sitzposition/Bewegung, Waffen/Haare/Rüstung, Auf-/Absteigen und dreimal Minimize/Restore; Angriff war jeweils „wenn möglich“. Keine optionale tatsächliche Angriffsausführung aus dieser allgemeinen Bestätigung ableiten; die isolierte Original-Motion-Folge deckt Mounted Attack eindeutig ab.

Im normalen Diligent-Protokoll sind eine Mount-Körper-Lebenszeit, der zugehörige MountedPlayer und Fuß-/Mount-Frames erfasst. Wiederholte Mount-/Dismount-Zyklen sind damit **automatisiert** nachgewiesen; die genaue Zahl manueller Ingame-Wechsel ist nicht unabhängig belegt. Der normale Mount ist der originale weiße Löwe (20114), zuvor in 5B/5C noch ausgeschlossen. Die Nutzerbestätigung belegt die visuelle Sitz-/Animations-/Attachment-Prüfung, keine instrumentierte Subframe-Latenzmessung.

Mehrfaches Backend-Resize ist im Mount-Materialvergleich enthalten (640×480/800×600 und Nullgröße/Restore), zusätzlich läuft der bestehende Präsentationstest mit mehreren Größen. Ein frei per Fensterkante geändertes normales Clientfenster wurde nicht separat nachgewiesen. Reale Minimize/Restore-Zyklen beider isolierter Backends und des normalen Diligent-Clients wurden bestätigt.

### Ressourcen und Stabilität

Finaler isolierter Diligent- und normaler Diligent-Shutdown:

```text
shutdown actor_geometry=0 actor_textures=0
shutdown attachment_geometry=0 attachment_textures=0
shutdown mount_geometry=0 mount_textures=0
shutdown object_geometry=0 object_textures=0
```

Die Mount-Werte sind Teilmengen der allgemeinen Actor-Ressourcen; sie nicht zu einer vermeintlich zusätzlichen Gesamtzahl addieren. `mount_uploads` zählt dynamische Uploads fertiger nativer Posen, keine neue Skinningberechnung. Sichtbare Actors/Parts und Materialdraws sind unterschiedliche Zähler. `actor_index_uploads` ist kumulativ und steigt erwartungsgemäß bei Neuanlage/Equipmentwechsel; es ist kein Resident-Zähler.

Messung im Fünfsekundenraster (`*-resources.csv`):

| Lauf | Private Memory Maximum | Letzte Probe vor Exit | Handles Maximum / letzte Probe |
| --- | --- | --- | --- |
| Isoliert Legacy | 474,1 MB | 463,6 MB | 787 / 779 |
| Isoliert Diligent | 635,9 MB | 567,2 MB | 1125 / 1118 |
| Normal Legacy | 443,3 MB | 443,3 MB | 823 / 823 |
| Normal Diligent | 558,2 MB | 555,4 MB | 1164 / 1163 |

Kein beobachteter Crash, keine Assertion und kein protokollierter Device-/Mount-Renderfehler in diesen Läufen. Während Map-/Modellwechseln steigt Prozessverbrauch vorübergehend; Diligent sinkt nach Freigabe wieder. Alle erfassten Grafikbesitzer werden freigegeben. Dies ist **kein Langzeit-Leaknachweis** für den gesamten Client oder Treiber; Prozesscaches sind nicht mit den Nullwerten der GPU-Besitzer gleichzusetzen.

Ein erster zusätzlicher Hintergrundlauf (PID 62556, 265,9 s, Exit 0) enthielt eine Test-Datenwarnung `Cannot find item by 11909`, weil die Item-Probe über die für Shape 3 nötigen Daten hinauslief. Die Fixture beendet diese Suche jetzt unmittelbar beim gefundenen Shape. Der finale sichtbare Lauf hat leere syserr-Dateien. Kein Produktionsfix dafür. Die normalen Starts melden nur das bekannte `invalid idx 0` aus dem unveränderten MarkManager.

### Grenzen und verbleibende Systeme

- Echter serverseitiger Teleport zu Fuß/beritten und Relog im selben Prozess: **ungeprüft**. Offline-Mapwechsel und getrennte Starts ersetzen diesen Nachweis nicht. Keine Infrastruktur dafür ergänzt.
- Freigegeben ist das reale SHORSE-Paar normaler Player, nicht jedes NPC-Objekt mit einer Mount-Race. Ein freistehendes gerufenes Pferd kann serverseitig ein separater NPC sein; es wird nicht anhand der Race künstlich zum gerittenen Mount erklärt. Ein eigenständiger NPC-/Mob-Reiterfall wurde nicht belegt und nicht neu entwickelt. Polymorph-Reiter/Pets bleiben ausgeschlossen.
- Standalone `chr.DismountHorse` bleibt ein unvollständiger historischer Testhelper; der produktive Delete/Create-Lifecycle wurde unverändert übernommen. Keine neue Legacy-Korrektur außerhalb von 5D.
- Bestehende 5B-Ausschlüsse bleiben: Wachen 20340ff (auch im normalen Lauf protokolliert), Metins außerhalb der Mobgrenze, rein starre NPC-Körper außerhalb des bisherigen Body-Snapshotvertrags. Keine generelle 5B-Gesamtabnahme aus der Mountmigration ableiten.
- Keine Migration von Pet-Systemen, Ice Smoke/Mount-Effekten, Horse Dust, Weapon Glow, Skills/Particles, Schatten, SpeedTree, Wasser oder Ingame-UI/Minimap/Nameplates/Damage Numbers. Native Hintergrundlogik kann weiterhin laufen, ohne im Diligent-Weltbild sichtbar zu sein.
- Die Legacy-Deform-Reihenfolge bleibt unverändert. Nur das opt-in Diligent-Paar erhält die passende Parent-vor-Child-Auswertungsreihenfolge; keine neue Pose oder neue Skeleton-Runtime. Nicht jede private Mount-/LOD-/Materialkombination ist getestet.

### Abschlussabgleich der 28 Berichtspunkte

1. **Call-Chain:** Netzwerk-MountVnum → Actor-Neuanlage → SHORSE → native Animation/Deform → vorhandener Actorrenderer; oben konkret aufgeschlüsselt.
2. **Klassen:** CInstanceBase/SHORSE, CActorInstance, CGraphicThingInstance, CGrannyLODController, CGrannyModelInstance, CRaceManager; kein DiligentMountRenderer.
3. **Mount-Race:** originales npclist/MSM/Motlist/GR2, nicht im Renderer dupliziert.
4. **Player/Mount-Link:** SHORSE besitzt Mount, Reiter m_pkHorse plus ModelInstance-Parent am Sattel.
5. **Riding Transform:** fertige Parent-gebundene native Pose und Mesh-Weltmatrix; keine Offsettabelle.
6. **Mount-Animation:** eigenes Skeleton/Clock, native MODE_GENERAL-Motions.
7. **Player-Sync:** bestehender gemeinsamer Motionindex/Blend/Speed, für Diligent Parent-vor-Reiter-Deform und Frame-Validierung.
8. **Renderer:** DiligentActorRenderer und derselbe getestete PNT-/Materialpfad.
9. **Materialien:** originale diffuse/spezielle Actor-Materialstages, pro Draw deterministisch.
10. **Alpha:** native Spawn-Blend belegt; Alpha-Test im Mount-Pixelvergleich, keine unbelegte Original-Opacity-Variante.
11. **Attachments:** Waffen/Haare/Rüstung beim Reiten, native linke/rechte Handregeln und Haar-Skeleton aus 5C.
12. **Mounting:** reale SHORSE-Erzeugung im CInstanceBase-Create.
13. **Dismounting:** originaler vollständiger Destroy/Create-Pfad; Nullressourcen nachgewiesen.
14. **Mount-Wechsel:** A → weitere vorhandene Models → A, jeweils native Actor-Neuanlage am VID.
15. **Mount-Typen:** Pferd, Keiler, weißer Löwe, Dinosaurier, Halloween-Pferd.
16. **Visibility:** vorhandene gemeinsame CanRender-Prüfung plus gültige Paar-Snapshots, keine neue Cullingengine.
17. **Spawn/Despawn:** wiederholt, Hide/Show und Sichtbereich verlassen/zurück.
18. **Mapwechsel:** A1 → B1 → A1 auf beiden Backends zu Fuß und beritten.
19. **Teleport/Relog:** echter Serverpfad ungeprüft.
20. **Resize:** mehrfach auf Backend-/Präsentationsebene; kein behaupteter Fensterkanten-Test.
21. **Minimize/Restore:** mehrfach automatisiert und vom Nutzer bestätigt.
22. **Shutdown:** finale isolierte und normale Läufe Exit 0.
23. **Ressourcen:** Actor-, Attachment- und Mount-Geometrien/Texturen nach Shutdown 0.
24. **Builds:** Release ON/OFF sowie wiederhergestellter ON-Build jeweils Exit 0; Legacy bleibt Startdefault.
25. **Testsuite:** vollständig 10/10; abschließend Renderer ON 6/6 und OFF 4/4, jeweils Exit 0; kein Test deaktiviert.
26. **Legacy:** normale Ingame- und isolierte Regression bestätigt; nativer Pfad unverändert.
27. **Sonderfälle:** freistehende NPC-Mounts, historischer Dismounthelper, Effekte, Kategorie-/Testgrenzen ausdrücklich dokumentiert.
28. **Empfehlung:** als nächsten abgegrenzten Prüfschritt vor breiter Gesamtabnahme echte Teleport-/Relog-Fälle nachholen. Nächste Migration erst nach neuer Beauftragung planen, hier keine beginnen.

### Git-Diff und Übergabe

9 Produktionsdateien: `GameLib/ActorInstance.cpp`, `ActorInstanceRender.cpp`, `ActorRenderBridge.{h,cpp}`, `Renderer/ActorRenderData.h`, `DiligentActorRenderer.{h,cpp}`, `TerrainPresentation.cpp`, `UserInterface/InstanceBase.cpp`. Letztere enthält Actor-Lifecycle, keine UI-Migration. Keine Änderung an Granny-/CPU-Skinning-Implementierung, StateManager, Shadern, Netzwerk oder nativen Mount-Regeln.

5 bestehende Renderer-Testdateien ergänzt; neue Dateien: dieser Bericht und `tests/Renderer/{mounts_entry.py,mounts_smoke.py,setup_mounts.ps1,run_mounts.ps1,verify_mounts.ps1}`. Das Setup nutzt vorhandene 5A-Bootstrap-/Testconfig-Artefakte unter `build/milestone5a`; keine neuen Downloads oder Änderung der Originalpakete. Normale Starts sind private EXE-Kopien mit Originalpaketen. Ingame-getestete Produktions-EXE SHA-256: `639581D08098961139FAA9494D75EF3084A7BE8DB4C7EE51FE6A1202DC395F68` (isolierter und normaler Lauf identisch). Nach OFF/ON erneut verknüpfter Build unter `build/bin/Release`: `4CDD307E7AB9039EC7704F42FF27B7A12033564367BADAEF059E753339D4C39F`; gleicher Produktionssource, abschließend Renderer 6/6, kein weiterer manueller Loginlauf mit dieser erneut verknüpften Datei.

Wesentliche neue Stellen sparsam mit `ZiiNAN` gekennzeichnet. Diff der 14 bereits versionierten Dateien: 198 hinzugefügte / 26 entfernte Zeilen; zusätzlich 6 neue Dateien. `git diff --check` ohne Whitespacefehler. Original-Runtime-EXE/Pakete nicht überschrieben, Serverdateien nicht angefasst, vorhandene `m2dev-client/config/channel.inf`-Änderung erhalten. Keine Commits/Pushes. Milestone 5D im oben abgegrenzten Umfang abgeschlossen; hier gestoppt, keine weitere Migration begonnen.
