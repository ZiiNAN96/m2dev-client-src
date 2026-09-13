<!-- ZiiNAN: Milestone 5A actor boundary analysis before production edits. -->
# Milestone 5A – Analyse und minimale Integrationsgrenze

> Historischer Milestone-Stand. Aktuell seit [M12](renderer-milestone12.md): Diligent D3D11 ist im ON-Build der Default; Legacy wird explizit mit `--renderer=legacy-d3d9` gewählt. OFF-Builds bleiben Legacy-only. Frühere Testergebnisse und Auswahlbeschreibungen unten gelten für ihren damaligen Stand.

12.09.2026, Ausgangsbasis `7f038ef`, sauberer Source-Worktree. Kein Granny-Rewrite, keine neue Animation, kein GPU-Skinning. Legacy D3D9Ex bleibt Default. Die folgenden Befunde wurden vor den Produktionsänderungen aus dem bestehenden Source und den Originalassets erhoben.

Sourcewurzel: `C:\Users\ZiiNAN\Documents\GitHub\m2dev-client-src\src`. Assetwurzel: `C:\Users\ZiiNAN\Documents\GitHub\m2dev-client\assets`. Pfade in den Tabellen sind relativ zu diesen ausdrücklich angegebenen Wurzeln.

## Actor- und Materialvertrag (29 Analysepunkte)

| Nr. | Frage | Tatsächlicher Pfad / Befund |
|---|---|---|
| 1 | Sichtbarer Charakter | `UserInterface/InstanceBase.{h,cpp}`: `CInstanceBase`, enthält `m_GraphicThingInstance` vom Typ `CActorInstance`. |
| 2 | Gemeinsamer Kern | `GameLib/ActorInstance.h`: `CActorInstance : IActorInstance`; Spieler/NPC/Enemy werden über denselben Actor-Kern mit verschiedenen EType-Werten verwaltet. Gemeinsamer Kern allein beweist keine gleiche Material-/Attachment-Abdeckung. |
| 3 | Race/Models | `CRaceManager`, `CRaceData`, `CGraphicThingInstance`, `CGrannyLODController`, `CGrannyModelInstance`. Hauptkörper `CRaceData::PART_MAIN == 0`; Hair/Weapon sind eigene Parts. |
| 4 | GR2-Laden | `ActorInstanceData.cpp::SetShape` → `CRaceData::GetBaseModelThing`/ResourceManager → `CGraphicThing::OnLoad` → `GrannyReadEntireFileFromMemory`/`GrannyGetFileInfo` → `LoadModels`/`LoadMotions`. |
| 5 | Skeleton/Animation | `ModelInstanceUpdate.cpp::Update` setzt Granny-Clock (ANIFPS-Min/Max 30/120, LOD-/FPS-Drosselung); `UpdateWorldPose` benutzt `GrannySampleModelAnimationsAccelerated`, native Local-/World-Pose und bestehende Parent-/Skeletonverknüpfung. |
| 6 | CPU-Deformation | `CGrannyModelInstance::Deform` → `DeformPNTVertices` → `CGrannyModel::DeformPNTVertices` → `CGrannyMesh::DeformPNTVertices`. SSE2: vorhandenes `DeformPWNT3432toGrannyPNGBT33332`; sonst `GrannyDeformVertices`. Beide bleiben unverändert. |
| 7 | Fertiges Format | `TPNTVertex` / `granny_pnt332_vertex`: Position float3, Normal float3, UV0 float2, 32 Byte. Raw-Asset enthält zusätzlich BoneWeights/BoneIndices; diese sind nach CPU-Skinning keine GPU-Attribute mehr. |
| 8 | Updatezeitpunkt | In `CPythonApplication::RenderGame` nach Perspektive/Culling und vor dem Shadow-/Welt-/Character-Rendern: `m_kChrMgr.Deform()`. |
| 9 | Pro Frame? | `CInstanceBase::Deform` läuft im RenderGame-Pfad nur nach `__CanRender`; dann `INSTANCEBASE_Deform` → `Deform`. Sichtbare Instanz wird deformiert; Granny-Clockfortschritt kann unabhängig davon gedrosselt sein. Nicht pauschal „jede Instanz jede Simulationstaktung“. |
| 10 | Native VBs | `ModelInstanceModel.cpp::__GetDeformableVertexBufferRef`: geteiltes LOD-VB, sonst lokales VB; rigide Meshes nutzen das Model-PNT-VB separat. |
| 11 | Dynamisch? | LOD-Pool `__AllocDeformVertexBuffer`: D3DUSAGE_DYNAMIC, D3DPOOL_DEFAULT, PNT-FVF, Kapazität in 500er-Schritten. `GrpVertexBuffer::LockRange` sperrt ab Offset 0 mit Größe count*stride; aktueller Lockflag für DYNAMIC ist 0, nicht implizit DISCARD. |
| 12 | Indizes | `CGrannyModel::LoadIndices` erzeugt D3DFMT_INDEX16; `CGrannyMesh::LoadIndices` benutzt GrannyCopyMeshIndices und Originaloffset. Indizes bleiben statisch. `CGraphicThing::LoadModels` gibt danach unter anderem GrannyStandardDeformableIndexSection frei: Snapshot MUSS davor erfolgen, kein späterer Raw-/GPU-Readback. |
| 13 | Submeshes | `CGrannyModel::TMeshNode`, `CGrannyMesh::TTriGroupNode`; getrennte Meshlisten TYPE_DEFORM/TYPE_RIGID und DIFFUSE/BLEND. |
| 14 | Materialauswahl | `RenderMeshNodeListWithOneTexture` wählt `m_kMtrlPal.GetMaterialRef(group->mtrlIndex)` aus der aktuellen ModelInstance. |
| 15 | Texturbindung | `CGrannyMaterial::ApplyRenderState`, Diffuse setzt Stage0; vorhandene Resource-/Pack-Auflösung und Shape-Skin-Ersetzungen. |
| 16 | Mehrere Materialien | Pro Mesh alle originalen TriGroup-Nodes, getrennte Palettenindizes, keine neue Zusammenführung. Kriegergrundkörper: drei Gruppen, zwei unterschiedliche Texturen. |
| 17 | Worldmatrix | `UpdateWorldMatrices`: deformiertes Mesh erhält Actor-World direkt; rigides Mesh Bone-Matrix*Actor-World. Kein erneutes Multiplizieren der Skinning-Bones in Diligent. |
| 18 | Position/Rotation/Scale | `INSTANCEBASE_Transform` → vorhandener TransformProcess/Transform; `CGraphicObjectInstance::Transform` enthält Rotation/Position/Scale. Gegebene finale Meshmatrix verwenden; Y-Konvention nicht erneut umdrehen. |
| 19 | View/Projection | Aktuelle StateManager-Transforms aus Kamera; RenderGame setzt Perspektive 30°, Aspect, Near 100, Background-FarClip. Kein neues Kameramodell. |
| 20 | Hauptstates | `CActorInstance::OnRender`: globale Material.Diffuse aus `m_dwMtrlColor`, CULL_NONE; NORMAL → BeginDiffuseRender/OneTexture → BeginOpacityRender/BlendOneTexture. Manager deaktiviert Stage1 vor Actorliste. |
| 21 | Alpha-Test | Diffuse-Pass übernimmt aktuellen Zustand. Opacity-Pass setzt ALPHATEST TRUE, ALPHAREF 0, ALPHAFUNC GREATER, stellt danach zurück. Keine frei gewählte Cutout-Schwelle. |
| 22 | Alpha-Blend | Normal-Diffuse setzt ALPHABLEND FALSE. RENDER_MODE_BLEND bei alpha==1 nimmt normalen Pfad; Zwischenwerte benutzen SRCALPHA/INVSRCALPHA und Stage0 SELECTARG2(TFACTOR). ADD/MODULATE haben eigene Stage1-Effekte. Letztere Sonderpfade sind nicht Teil des ersten Grundkörpers. |
| 23 | Cull | Actor setzt temporär NONE, Material kann ebenfalls Two-sided setzen. Nicht den Weltobjekt-Default CW auf Charaktere übertragen. |
| 24 | Depth | Bestehender ZENABLE/ZFUNC/ZWRITE-Zustand; Actor-Normalpass führt keinen eigenen ZWRITE-Override ein. Native LESSEQUAL übernehmen. |
| 25 | Vertexfarben | Keine im fertigen PNT-Format. Actorfarbe ist D3DMATERIAL9.Diffuse, keine neue Vertexfarbe. |
| 26 | Lighting | Background.BeginEnvironment aktiviert native Beleuchtung/Material/Fog; anschließend SetCharacterDirLight. Vorhandenes Licht0, Ambient/Material und ggf. verbliebenes Intro-Punktlicht1; keine neue Lichtquelle. Deformierte Normalen werden mit originaler World/View-Normalmatrix behandelt. |
| 27 | Loops | Actor → Thing-Partliste → aktueller LOD → ModelInstance → Meshliste → TriGroup → Material → DrawIndexedPrimitive. Nur PART_MAIN wird zusätzlich übergeben, keine Waffen-/Hair-Partschleife in Diligent. |
| 28 | Visibility | `CCullingManager::Process`, `CInstanceBase::__CanRender`: ViewFrustum + AFFECT_INVISIBILITY; `CGraphicObjectInstance::Render/Deform` zusätzlich `isShow()`. |
| 29 | Renderauswahl | CharacterManager sortiert Alive-/Dead-Instanzen nach Kameradistanz und ruft CInstanceBase::Render. Zusätzliche Übergabe bleibt innerhalb dieses bereits selektierten Actorpfads, nicht als unabhängiger „alle Actors“-Pass. |

## Tatsächliche Call-Chains

`CPythonApplication::RenderGame` → `CPythonCharacterManager::Render` → `__RenderSortedAliveActorList`/`__RenderSortedDeadActorList` → `CInstanceBase::Render` → `CGraphicObjectInstance::Render` → `CActorInstance::OnRender` → `CGraphicThingInstance::RenderWithOneTexture` → `CGrannyLODController::RenderWithOneTexture` → `CGrannyModelInstance::RenderWithOneTexture` → `RenderMeshNodeListWithOneTexture` → `CGrannyMaterial::ApplyRenderState` → `CStateManager::DrawIndexedPrimitive`.

`CPythonApplication::RenderGame` → `CPythonCharacterManager::Deform` → `CInstanceBase::Deform` → `CActorInstance::INSTANCEBASE_Deform` → `CGraphicObjectInstance::Deform` → `CGraphicThingInstance::OnDeform` → aktueller LOD → `CGrannyModelInstance::Deform` → `UpdateWorldPose`/`UpdateWorldMatrices` → native CPU-Deformation → fertig geschriebener gesperrter PNT-Buffer → Unlock.

**Übergabepunkt:** unmittelbar nach `DeformPNTVertices(pntVertices)` und vor `rkDeformableVertexBuffer.Unlock()`. Nur lesen/kopieren; kein zusätzlicher Granny-Sampling-/Deform-Aufruf, keine GPU-Rücklese und keine Annahme, dass das gepoolte VB später noch zu derselben LOD/Instanz gehört. Snapshot gehört zur ModelInstance und wird bei deren Clear/Reuse freigegeben.

## Ausgewählter erster Actor

Männlicher Krieger, Race 0, Shape 0, `PC/ymir work/pc/warrior/warrior_novice.gr2` laut originalem `root/msm/warrior_m.msm`. Granny-Inventur über den vorhandenen SDK-Inspector (kein eigener GR2-Parser): 75 Bones, drei deformierte Meshes mit 1669/258/280 Vertices und 4902/900/1002 Indizes. Zwei Diffusetexturen (`warrior_novice_red.dds`, `warrior_face.dds`), kein Opacity-Grundmaterial; Face ist Two-Sided. Gesamt 2207 Vertices, 6804 uint16-Indizes, drei Drawgruppen. Erwarteter voller PNT-Upload: **70.624 Byte**, einmaliger Indexinhalt 13.608 Byte. Erst Laufzeitmessung zählt als tatsächlich beobachteter Upload.

`general/wait.gr2`, `run.gr2`, `attack.gr2` enthalten jeweils eine originale Animation. Separates Haar ist ein anderer Part und wird nicht vorgezogen. Vorhandene erste LOD ebenfalls 75 Bones, drei deformierte Meshes; keine neue LOD-Auswahl.

## Geplante minimale Änderungen

1. `Renderer/ActorRenderData.h` neu: neutraler CPU-Snapshot-/Instanzvertrag, Rendererinterface und enger Body-Deform-Scope. Keine Skeletondaten im Renderer.
2. `EterGrnLib/Model.{h,cpp}` + `Thing.cpp`: optionaler unveränderlicher Indexsnapshot für passende vollständig deformierte PNT-Modelle, vor bestehender Section-Freigabe. Rigide/Deform-Mischmodelle und fehlende Formate zunächst explizit ausgeschlossen.
3. `EterGrnLib/ModelInstance.{h,cpp}`/`ModelInstanceModel.cpp`/`ModelInstanceUpdate.cpp`: fertige PNT-Daten nur für ausgewählten Actor-Hauptkörper kopieren; Instanz-/GPU-Daten bei Clear freigeben. Granny-, SSE2- und Fallback-Berechnung unangetastet.
4. `GameLib/ActorInstance.cpp`/`ActorInstanceRender.cpp` + `ActorRenderBridge.{h,cpp}` neu: nur nicht berittener Spieler-Hauptkörper im normalen sichtbaren Pass. NPC/Mob benötigen keinen neuen Kern, werden aber nicht ohne eigene erfolgreiche Abnahme als unterstützt aktiviert. Sonderrender-Modi, Attachments und Material-Sonderfälle bleiben dokumentiert draußen.
5. `Renderer/DiligentStaticObjectRenderer.{h,cpp}`: kleine zusätzliche dynamische Geometrieerzeugung/-aktualisierung; bestehender statischer Pfad bleibt Standard. `DiligentActorRenderer.{h,cpp}` neu verwendet dieselbe PNT-/Material-/Texturimplementierung in einer separat gezählten Instanz.
6. `GameLib/StaticObjectBridge.{h,cpp}`: vorhandene lesende State-Erfassung für Actor-Licht1 freigeben, ohne den bisherigen Static-Default/ShadowBase-Vertrag zu ändern.
7. `Renderer/TerrainPresentation.cpp`/`CMakeLists.txt`: Actorrenderer am vorhandenen D3D11-Gerät/Depthbuffer, Framezähler, Fehler-/Shutdownprüfung. Kein neues Fenster, kein neuer Backendtyp, keine Kompositions-/UI-Erweiterung.
8. `tests/Renderer`: native dynamische GPU-Parität, Original-Granny-Animationsdaten, Real-Map-Actorhilfe, Lebensdauer/Größenwechsel; ON/OFF und vollständige Suite. Markierung der wesentlichen Ergänzungen mit exakt `ZiiNAN`.

## Bufferstrategie und Testgrenzen

Diligent-Version bleibt gepinnt (`b036337d`, 2.5.6). Lokale SDK-Verträge `GraphicsEngine/interface/Buffer.h`, `GraphicsTypes.h` und bestehender MapHelper-Pfad geprüft: dynamisches VB mit `USAGE_DYNAMIC`, `CPU_ACCESS_WRITE`, Vollaktualisierung per MAP_WRITE/MAP_FLAG_DISCARD; Indexbuffer unveränderlich. Kein Ringbuffer, Batching, Instancing oder GPU-Skinning. Nur ein Upload pro neuer fertig deformierter Revision, nicht pro Materialgruppe. Originale D3D9-Buffer-/Lockstrategie bleibt erhalten.

Erforderliche Nachweise: Idle/Run/Attack ohne neue Animation, mehrere Kameraposen/Model-/LOD-Wechsel, Native/Diligent-Geometrie und gemeinsam genutzter Welt-Depthbuffer, vorhandene Map A1→B1→A1, Entfernen/Neuerzeugen von Actors, mehrfach Resize/Suspend/Restore, normaler Legacy-Ingame-Test, normaler Diligent-Actor-Test, Exit 0 und keine Actor-GPU-Handles nach Shutdown. Ein geskripteter Maplade-Test ist kein behaupteter serverseitiger Warp. Testaufbau getrennt von Originalpaketen und von anderen laufenden Clients.

Nach erfolgreicher Abnahme des ersten vollständigen animierten Grundkörperpfads: **STOP nach 5A**. Keine automatische Erweiterung auf Waffen, Hair-Attachments, Mounts/Pets, Skill-Effekte, SpeedTree oder UI.
