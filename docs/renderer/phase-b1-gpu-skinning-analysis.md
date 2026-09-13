# Phase B1 – Granny-/CPU-Skinning-Analyse

Stand: 13.09.2026. Ausschließlich Analyse; Phase A unverändert.

## Ergebnis und Evidenzrahmen

**Technisches GO für eine später separat beauftragte, abgesicherte GPU-Skinning-Vorbereitung. Kein GO zum Abschalten des CPU-Pfads. B2 wurde nicht begonnen.**

Der geeignete Übergabepunkt existiert bereits: nach CGrannyModelInstance::UpdateWorldPose, mit der tatsächlich verwendeten WorldPose und den tatsächlichen Mesh-Bindings, vor DeformPNTVertices. Granny muss weiterhin Animation, Blending, Skeleton und Attachment-Posen berechnen. Nur die Vertex-Deformation kann später ersetzt werden.

Die wesentlichen Befunde:

- Der produktive x64/SSE2-Pfad deformiert mit der vorhandenen eigenen Funktion DeformPWNT3432toGrannyPNGBT33332, nicht primär mit GrannyDeformVertices.
- Originale Bind-/Skin-Vertices, vier Byte-Gewichte und vier mesh-lokale Byte-Indizes sind noch zugänglich.
- Alle **3.118 skinnbaren Mesh-Vorkommen** im untersuchten Bestand haben dasselbe PWNT-Format, 40 Bytes. Der heutige Renderoutput ist PNT, 32 Bytes.
- **9.166/9.166 lokale GR2-Dateien** erfolgreich gelesen; **4.435 Model-Vorkommen, 7.393 Mesh-Vorkommen**. Maximal **163 Skeleton-Bones**, maximal **163 Mesh-Bindings**, maximal **96 positiv gewichtete Bones innerhalb einer Triangle-/Materialgruppe**.
- Mesh-Indizes sind nicht automatisch Skeleton-Indizes. Bei 4.830 Mesh-Vorkommen ist das Self-Binding mindestens teilweise nicht identisch; dieser Wert umfasst auch rigide Meshes.
- Rüstungen besitzen nicht zwingend gleiche Skeleton-Anzahlen/-Reihenfolgen. Haar-Bindings auf drei Kriegerkörper ergeben für denselben Haarindex die Ziel-Bones 67, 66 bzw. 68.
- Kein Bone-Inverse-Transpose und keine Normalisierung im aktiven CPU-Deformer. Diese Eigenschaften gehören zur zu erhaltenden Referenz.
- Empfehlung: zunächst unveränderte Byte-Gewichte, statische 40-Byte-Skin-Vertices mit validiertem Binding und **separater 256×float4x4-Constant-Buffer-Vertrag**. Mehrere Materialien dürfen keine erneute Vertex-Deformation oder Palette-Berechnung auslösen.

### Was diese Zahlen abdecken

Quellstand: **72a695884e3708effabc3a3114b7478d6fc26bae**, „feat(renderer): complete D3D9 removal and Diligent migration“. Der Source-Worktree war zu B1-Beginn sauber.

Assetwurzel: [lokaler Originalasset-Bestand][A00]. Die Untersuchung liest physische GR2-Dateien einschließlich LODs, Patchvarianten, Animationsdateien und Weltobjekten. Model-/Mesh-/Vertex-Vorkommen werden pro Model-Binding gezählt, nicht nach identischem Inhalt dedupliziert. **Das ist keine Messung gleichzeitig geladener Assets und keine Auflösung der Pack-/Patchpriorität eines bestimmten Live-Starts.** Der maximale Wert gilt für diesen vollständig gelesenen lokalen Bestand, nicht für zukünftige oder serverseitig nachgelieferte Assets.

Pfadkategorien wie pc, npc und monster sind Inventarkategorien, nicht automatisch Gameplay-Typen. Reittiere liegen ebenfalls unter npc; bekannte Mounts werden zusätzlich anhand der vorhandenen M5D-Fixture ausgewiesen. Ein Holzportal im npc-Verzeichnis ist kein humanoider NPC.

Analysewerkzeuge liegen ausschließlich im ignorierten [B1-Diagnoseverzeichnis][E00], außerhalb des produktiven Buildgraphs. Sie lesen Assets und verlinken die vorhandene Granny-Library; der Poseprüfer kompiliert die bestehende Deform.cpp unverändert als CPU-Referenz. Keine Instrumentierung, kein Shader und keine Produktionsdatei wurden geändert. Die einzige neue versionierbare Datei ist dieser Bericht.

Evidenztypen im Bericht:

- **Codebefund:** aktueller Source und tatsächlich vorhandener SDK-Header.
- **Assetbefund:** neuer read-only Scan mit der eingebundenen Granny-Library.
- **CPU-Experiment:** neuer isolierter numerischer Vergleich, kein GPU-Skinning.
- **Historische Telemetrie:** vorhandene M13C-Protokolle, nicht als neuer Benchmark ausgegeben.
- **Vorschlag:** noch nicht implementiert.

## 1. Actor-Call-Chain

### Besitzstruktur

| Ebene | Aktuelle Klasse / Feld / Verantwortung |
| --- | --- |
| Spielinstanz | CInstanceBase; enthält m_GraphicThingInstance und SHORSE |
| Actor | CActorInstance → IActorInstance → CGraphicThingInstance → CGraphicObjectInstance |
| Modelle pro Part | CGraphicThingInstance::m_modelThingSetVector, m_LODControllerVector |
| LOD/Part | CGrannyLODController::m_que_pkModelInst, m_pCurrentModelInstance |
| Instanz | CGrannyModelInstance: m_pModel, m_pgrnModelInstance, WorldPose, Mesh-Bindings, Materialpalette, ActorInstanceData |
| Shared Asset | CGraphicThing besitzt Granny-Datei, CGrannyModel- und CGrannyMotion-Arrays |
| Granny-Model | CGrannyModel::m_pgrnModel; Mesh-/Material-/Indexdaten |

Quellen: [Actor-Vererbung][S01], [ThingInstance][S02], [ModelInstance-Aufbau][S03].

### Update und Deformation sind zwei unterschiedliche Abschnitte

~~~text
CPythonApplication::UpdateGame
  → CPythonCharacterManager::Update
    → CInstanceBase::Update
      → State/Motion/Movement/Attack-Verarbeitung
    → CPythonCharacterManager::UpdateTransform
      → CInstanceBase::Transform
        → CActorInstance::INSTANCEBASE_Transform
          → Update → CActorInstance::OnUpdate
            → CGraphicThingInstance::OnUpdate
              → UpdateLODLevel
              → UpdateTime
                → CGrannyLODController::UpdateTime
                  → CGrannyModelInstance::Update
                    → GrannySetModelClock
                  → UpdateLocalTime
          → TransformProcess / Transform / Bounds / Attachments

CPythonApplication::RenderGame
  → CPythonCharacterManager::Deform
    → CInstanceBase::Deform [__CanRender]
      → gegebenenfalls zuerst SHORSE::Deform
      → CActorInstance::INSTANCEBASE_Deform [ActorDeformScope]
        → CGraphicObjectInstance::Deform [isShow]
          → CGraphicThingInstance::OnDeform
            → CGrannyLODController::Deform [aktueller LOD]
              → CGrannyModelInstance::Deform
                → UpdateWorldPose
                → UpdateWorldMatrices
                → DeformPNTVertices
                → ActorInstanceData.vertices-Kopie
~~~

Quellen: [Application][S04], [CharacterManager][S05], [InstanceBase][S06], [ActorInstance][S07], [Thing-Update][S08], [LOD-Update][S09], [Deformation][S10]. Insbesondere nicht fälschlich Animation, CPU-Skinning und Materialdraw als einen einzigen Aufruf behandeln.

### Draw-Seite

~~~text
CPythonApplication::RenderGame
  → CPythonCharacterManager::Render
    → CInstanceBase::Render
      → CActorInstance::OnRender [ActorDrawScope]
        → RenderWithOneTexture / BlendRenderWithOneTexture
          → CGraphicThingInstance → aktueller LOD → CGrannyModelInstance
            → RenderMeshNodeListWithOneTexture
              → Material.ApplyRenderState
              → SubmitActorNativeDraw
                → ActorRenderBridge.cpp::Submit
                  → CaptureStaticMapObjectDraw
                  → ggf. CreateGeometry
                  → ggf. UpdateVertices [einmal je revision]
                  → DiligentActorRenderer::Draw
                    → DiligentStaticObjectRenderer::Draw
                      → SetPipelineState
                      → SetVertexBuffers / SetIndexBuffer
                      → CommitShaderResources
                      → IDeviceContext::DrawIndexed
              → Material.RestoreRenderState
~~~

„NativeDraw“ ist hier ein historischer Bezeichner für den CPU-Drawvertrag, **kein wieder vorhandener D3D9-Draw**. Quellen: [Actor-Renderzustände][S11], [ModelInstance-Materialdraw][S12], [Bridge][S13], [Actor-Renderer][S14], [Diligent-Upload/Draw][S15].

## 2. Granny-Call-Chain und Assetladen

CGraphicThing::OnLoad erhält Dateibytes vom Resource-/Packpfad und ruft GrannyReadEntireFileFromMemory → GrannyGetFileInfo → LoadModels / LoadMotions auf. LoadModels erstellt je FileInfo::Models-Eintrag ein CGrannyModel. Dieses übernimmt den geliehenen granny_model-Zeiger, liest Skeleton/MeshBindings, legt CGrannyMesh an, lädt Materialpalette, rigide Vertices und Indizes. [S16][S17]

Pro Mesh:

~~~text
CGrannyMesh::CreateFromGrannyMeshPointer
  → GrannyNewMeshBinding(mesh, modelSkeleton, modelSkeleton)
  → bei !GrannyMeshIsRigid:
      GrannyGetMeshVertexType
      GrannyNewMeshDeformer(inputType, PNT332, PositionNormal, AllowUncopiedTail)
  → LoadMaterials
  → LoadTriGroupNodeList
~~~

Die runtime-spezifische Bindung entsteht nochmals pro CGrannyModelInstance: GrannyInstantiateModel; bei eigenem Skeleton GrannyNewWorldPose; je Mesh GrannyNewMeshBinding(sourceSkeleton, tatsächlich verwendetes destinationSkeleton). Bei verlinkten Haaren wird die Körper-WorldPose benutzt. [S03][S18]

## 3. Animation-Update, Clips, Übergänge und Blending

CRaceData registriert Motion-Dateien; CActorInstance::SetRace registriert deren Motion-Keys in CGraphicThingInstance. CGraphicThing::LoadMotions bindet FileInfo::Animations an CGrannyMotion::BindGrannyAnimation. CGrannyMotion hält einen geliehenen granny_animation-Zeiger, Dauer und Zugriff auf Trackdaten, keinen neuen Animationsdecoder. [S19][S20][S21]

CActorInstance::MotionProcess / __SetMotion → CGraphicThingInstance::SetMotion → CGrannyLODController::SetMotionPointer → CGrannyModelInstance::SetMotionPointer:

- GrannyPlayControlledAnimation erstellt den Control.
- SpeedRatio und LoopCount werden am Control gesetzt.
- Ein alter Control erhält EaseOutCurve von 1 nach 0 und CompleteControlAt.
- Der neue Control erhält, außer beim Erststart, EaseIn von 0 nach 1.
- GrannyFreeControlOnceUnused / FreeCompletedModelControls verwalten abgeschlossene Controls.
- ChangeMotionPointer ist ein anderer bestehender Wechselpfad: 0,3 Sekunden überspringen, EaseIn/Out aus.
- Ein LOD-Wechsel kopiert Animationszeiger, Geschwindigkeit, Loop-Anzahl und RawLocalClock in die neue ModelInstance und erneuert Parent-Verknüpfungen.

Quellen: [ModelInstanceMotion][S22], [Thing-Motion][S23], [Actor-Motion][S24], [LOD-Wechsel][S09].

CGrannyModelInstance::Update drosselt **GrannySetModelClock** anhand ANIFPS_MIN=30 / ANIFPS_MAX=120 und LOD. CGrannyLODController::UpdateTime ruft Update vor UpdateLocalTime auf. Diese Reihenfolge bleibt Referenz. Die Drosselung ist **keine Garantie, dass CPU-Skinning nur 30–120 Mal/s oder nur bei geänderter Pose stattfindet**: sichtbare ModelInstances werden im Render-Deformabschnitt erneut gesampelt/deformiert.

## 4. Skeleton-Update und sauberer Palette-Hook

Granny stellt im Header dar:

- granny_skeleton: Name, BoneCount, Bones-Array, LODType, ExtendedData.
- granny_bone: Name, ParentIndex, LocalTransform, InverseWorld4x4, LODError, ExtendedData.
- granny_transform: Flags, Position[3], Quaternion Orientation[4], ScaleShear[3][3].
- ParentIndex beschreibt die Hierarchie; Wurzeln haben keinen Parent. Keine selbst erfundene Knochenhierarchie nötig. [S25]

UpdateWorldPose verwendet einen **statischen wiederverwendeten CGrannyLocalPose-Scratch**, der nur wächst. Bei m_ppkSkeletonInst auf eine andere ModelInstance wird das eigene Sampling übersprungen. Sonst:

~~~text
GrannyGetSourceSkeleton
  → sharedLocalPose.Get(BoneCount)
  → Parent ? Parent.GetBoneMatrixPointer(parentBone) : null
  → GrannySampleModelAnimationsAccelerated(instance, BoneCount,
                                          parentOffset, localPose, worldPose)
  → GrannyFreeCompletedModelControls
~~~

Das auskommentierte Paar SampleModelAnimations + BuildWorldPose ist nicht der aktive Pfad. Die Accelerated-Funktion erzeugt die benutzbare Pose einschließlich Composite-Arrays. Der gleiche SharedLocalPose ist nicht automatisch thread-sicher, nur weil GRANNY_THREADED definiert ist. B1 plant keine Parallelisierung. [S10]

**Empfohlener zukünftiger Hook:** innerhalb CGrannyModelInstance::Deform nach UpdateWorldPose und dem bestehenden UpdateWorldMatrices, vor Vertex-Deformation. __GetWorldPosePtr muss dabei die tatsächliche Skeleton-Owner-Instanz auflösen. Aus GrannyGetWorldPoseComposite4x4Array werden Werte kopiert, kein langlebiger roher Zeiger an spätere Renderarbeit gegeben. Pro Poseowner/Frame eine Palette-Revision; Hair-Instanzen referenzieren dieselbe Owner-Revision mit eigener Bindung.

UpdateSkeleton / DeformNoSkin zeigen bereits, dass Posen ohne Vertex-Deformation aktualisiert werden können. Sie sind jedoch **kein fertiger Ersatz für Deform**: ActorInstanceData::ready/capturedFrame/revision und der aktuelle Submit-Vertrag verlangen noch fertige CPU-Vertices.

## 5. CPU-Skinning: vollständiger aktueller Ablauf

CGrannyModelInstance::Deform → LockRange(deformVertexCount) am bestehenden CGraphicVertexBuffer → CGrannyModel::DeformPNTVertices → Schleife über sämtliche skinnbaren Meshes → CGrannyMesh::DeformPNTVertices. [S10][S17][S18]

| Frage | Befund |
| --- | --- |
| Einheit | Je deformierter ModelInstance, darin je skinnbarem Mesh |
| Submesh/Material | Kein separates Skinning pro Triangle-/Materialgruppe |
| Normale Häufigkeit | Ein Durchlauf je sichtbarem Actor-Part/aktuellem LOD im Deformabschnitt eines gerenderten Weltframes |
| Sonderpfad | DeformAll bearbeitet alle LOD-Instanzen; nicht mit dem normalen Current-LOD-Pfad gleichsetzen |
| SSE2 | DeformPWNT3432toGrannyPNGBT33332 → I-Variante mit TransformTable; D-Variante ohne Tabelle vorhanden |
| Fallback | GrannyDeformVertices mit bei Mesh-Erstellung erzeugtem Deformer |
| Position | Vier gewichtete affine Bone-Transformationen |
| Normale | Gleiche gewichtete lineare 3×3-Anteile, keine Translation |
| Tangent/Binormal | Nicht deformiert, im PNT-Actorformat nicht vorhanden |
| UV | Unverändert kopiert |
| Rigide Meshes | Keine Vertex-Deformation, Bone×World als Drawmatrix |
| Cache | Mesh-Deformer, Bindings, SharedLocalPose und Scratch-VB werden wiederverwendet; kein allgemeiner Cache fertiger Posen über Frames |
| Output | Zunächst CPU-PNT im LockRange-Speicher, danach instanzeigener ActorInstanceData-Snapshot |

CPU_HAS_SSE2 wird in CGraphicDevice::Create über CPUID gesetzt. Der verwendete x64-Rechner unterstützt SSE2; die echte Referenz ist daher die vorhandene SIMD-Funktion. Ihr Name enthält PNGBT, **der tatsächlich geschriebene Typ ist jedoch granny_pnt332_vertex**, also ohne Tangent/Binormal. [S26][S27]

Der Deformer reinterpretiert den Quellzeiger, benutzt aber einen fest vorgegebenen **40-Byte-SourceStride und 32-Byte-DestStride**. Der lokale Variablenname/TPNTVertex-Cast in Mesh.cpp beschreibt deshalb nicht korrekt das Quelllayout. Es gibt dort keine vollständige reflektierte Layoutprüfung vor der SSE2-Auswahl. Der neue Assetscan belegt die Annahme für den heutigen skinnbaren Bestand; zukünftige fremde Layouts dürfen nicht blind übernommen werden.

### CPU-Speicher und Lebensdauer

CGraphicVertexBuffer ist nach Phase A ein Renderer::CpuBuffer, kein nativer D3D9-/GPU-Vertexbuffer. Das Pooling in LODController reserviert in 500er-Schritten, hält mehrere Größenklassen vor und teilt Scratch innerhalb der LODs eines Controllers. Verschiedene aktive Controller leihen unterschiedliche Buffer; dies ist kein einziger globaler Vertex-Scratch für alle Actors. [S28][S29]

Nach Deformation kopiert ModelInstanceUpdate den fertigen deformierten Bereich in ActorInstanceData::vertices. In gemischten Models folgen hinten erneut die statischen rigidVertices. Dieser Snapshot überlebt Unlock und bleibt bis zum nächsten Capture oder Clear bestehen; CapturedFrame verhindert veraltete Submission. Rigid-only-Models haben keinen dynamischen Vertexsnapshot. Keine zweite CPU-Skinning-Runde im ActorRenderBridge.

## 6. Vertexformat vor Skinning

Tatsächlich gelesenes **granny_pwnt3432_vertex**, SDK-Pack/Alignment 4, sizeof=40, Stride=40. Gleiches reflektiertes Layout in allen 3.118 skinnbaren Mesh-Vorkommen. [S30][E01]

| Attribut | SDK-Typ | Offset | Bytes | Bedeutung |
| --- | --- | ---: | ---: | --- |
| Position | real32[3] | 0 | 12 | ursprüngliche Skin-/Bindgeometrie |
| BoneWeights | NormalUInt8[4] | 12 | 4 | Bytewerte, Faktor 1/255 |
| BoneIndices | UInt8[4] | 16 | 4 | Indizes in Mesh.BoneBindings |
| Normal | real32[3] | 20 | 12 | ursprüngliche Normale |
| UV0 | real32[2] | 32 | 8 | TextureCoordinates0 |
| UV1 | nicht vorhanden | – | 0 | kein Bestandteil dieses Actorlayouts |
| Tangent/Binormal | nicht vorhanden | – | 0 | keine verdeckten Tangentdaten im gescannten Skinformat |
| VertexColor | nicht vorhanden | – | 0 | Actorfarbe kommt separat aus Material-/Drawstate |

Es gibt im gesamten gescannten Modellbestand zusätzlich:

| Format | Mesh-Vorkommen | Stride |
| --- | ---: | ---: |
| PNT, Position/Normal/UV0 | 3.964 | 32 |
| PNT2, zusätzlich UV1 | 309 | 40 |
| PWNT, obiges Skinformat | 3.118 | 40 |
| leeres Mesh ohne VertexType | 2 | 0 |

PNT2 ist kein PWNT, obwohl beide 40 Bytes haben. Die PNT2-Vorkommen sind rigide im untersuchten Bestand. CGrannyModel::CaptureActorSource akzeptiert ausschließlich Position|Normal|Tex1; Dungeon/PNT2 bleibt ein anderer Pfad. Formatentscheidung niemals allein über Stride.

## 7. Vertexformat nach Skinning

| Struct/Vertrag | Position | Normal | UV0 | Gesamt / Alignment |
| --- | ---: | ---: | ---: | --- |
| granny_pnt332_vertex | 0 / 12 B | 12 / 12 B | 24 / 8 B | 32 B / 4 |
| TPNTVertex / SPNTVertex | 0 / 12 B | 12 / 12 B | 24 / 8 B | 32 B / 4 |
| Renderer::StaticObjectVertex = array<float,8> | Float 0–2 | Float 3–5 | Float 6–7 | 32 B / 4 |

Quellen: [GrpBase][S31], [StaticObjectRenderData][S32], [SDK PNT][S33]. Aktuelle static_asserts sichern den Größenvertrag; die isolierte SDK-Probe bestätigt Größe und Alignment.

Nach Skinning fehlen Bone-Indizes, Gewichte und Bindpositionen im Renderoutput. Sie können **nicht** aus ActorInstanceData::vertices zurückgewonnen werden. Sie bleiben davor in granny_mesh::PrimaryVertexData / GrannyGetMeshVertices verfügbar.

Zum Vergleich TPNT2/GrannyPNT3322: Position@0, Normal@12, UV0@24, UV1@32, 40 Bytes, Alignment 4; nicht zum neuen Skinning-Input umdeuten.

## 8. Bone-Indizes und Abgriff

Der rohe Vertexindex ist uint8 und adressiert **Mesh.BoneBindings**, nicht Skeleton.Bones. Der aktuelle CPU-Pfad macht:

~~~text
meshLocalIndex = vertex.BoneIndices[lane]
skeletonIndex = GrannyGetMeshBindingToBoneIndices(instanceMeshBinding)[meshLocalIndex]
matrix = activeWorldPose.composites[skeletonIndex]
~~~

Die Mappingtabelle ist granny_int32x/int32 pro Eintrag, im Probe-Build 4 Bytes. Der höchste tatsächlich positiv gewichtete lokale Vertexindex ist **161**. Dies ist nicht mit der Bone-Anzahl 163 gleichzusetzen.

Der sauberste Abgriff für statische Originaldaten liegt bei Model-/Mesh-Erstellung, solange die FileSections leben, zum Beispiel neben dem heutigen CaptureActorSource-Aufruf in CGraphicThing::LoadModels. Das aktuelle CaptureActorSource hält nur Indizes und rigide Vertices fest, keine Skin-Sourcevertices.

Der spätere bindungsspezifische Abgriff liegt in __CreateMeshBindingVector: dort ist erst das Ziel-Skeleton bekannt. Für Hair muss deshalb eine Quellasset-Capture von einem späteren Binding unterschieden werden. Keine pointers auf gelöschte FileSections speichern. [S16][S03]

## 9. Bone-Gewichte und reale Influences

Der SDK-Header kennt mehr Formate und GrannyMaximumWeightCount=256 für seine allgemeinen Weight-Array-Helfer. **Das ist nicht das heutige Vertexformat und kein Beleg für 256 Influences im Client.** Der aktive SIMD-Kern verarbeitet exakt vier uint8-Lanes. [S27][S30]

Der vollständige lokale Scan zählt 3.069.950 skinnbare Vertex-Vorkommen:

| positive Gewichte / Vertex | Vertex-Vorkommen |
| ---: | ---: |
| 0 | 0 |
| 1 | 2.016.840 |
| 2 | 814.102 |
| 3 | 177.482 |
| 4 | 61.526 |
| >4 | 0 im gefundenen PWNT-Format |

Alle Byte-Summen sind **255**. Keine positiv gewichteten Indizes außerhalb der jeweiligen Mesh-Bindings; keine ungültigen Self-Binding-Zielindizes. Zero-weight-Lanes wurden nicht als benutzte Bones gezählt. Ihre unbenutzten Indexbytes sind daher nicht durch diese Aussage als gültig garantiert.

| Reales Modell | maximale positive Influences | Bemerkung |
| --- | ---: | --- |
| Krieger novice | 4 | Körpermesh enthält 8 Vierfach-Vertices |
| Krieger warrior_4-1 | 4 | eines der acht Meshes enthält 63 Vierfach-Vertices |
| Doctor | 3 | 1.265 Vertices |
| Wolf | 3 | 762 Vertices |
| horse_normal | 4 | Körpermesh enthält 21 Vierfach-Vertices |
| Krieger hair_1_1 | 1 | trotzdem als deformierbares/granny-skinned Mesh klassifiziert |

Die Funktion multipliziert **jedes der vier gespeicherten Gewichte** mit 1/255. Sie berechnet kein implizites viertes Gewicht, sortiert nicht neu und renormalisiert nicht. Der synthetische CPU-Test mit Summe 140 bestätigt dieses Verhalten. Ein späterer GPU-Pfad darf Daten validieren/ablehnen, aber nicht still „korrigieren“ und damit die Referenz verändern.

## 10. Reale Bone Counts

D = skinnbare Vertices, R = rigide Vertices. „Gruppe benutzt“ zählt unterschiedliche positiv gewichtete Mesh-Binding-Einträge in den Dreiecken der größten Materialgruppe. Keine Palette-Aufteilung implementiert.

| Asset, relativ zu Assetwurzel | Skeleton | Meshes | D / R | größte Mesh-Palette | größte Gruppe benutzt |
| --- | ---: | ---: | ---: | ---: | ---: |
| PC/ymir work/pc/warrior/warrior_novice.gr2 | 75 | 3 | 2.207 / 0 | 72 | 38 |
| PC/ymir work/pc/warrior/warrior_4-1.gr2 | 74 | 8 | 2.582 / 0 | 73 | 26 |
| PC/ymir work/pc/warrior/warrior_nahan.gr2 | 76 | 4 | 3.025 / 0 | 73 | 41 |
| NPC/ymir work/npc/doctor/doctor.gr2 | 57 | 1 | 1.265 / 0 | 55 | 43 |
| NPC/ymir work/npc/blacksmith/blacksmith.gr2 | 48 | 3 | 1.180 / 376 | 43 | 34 |
| NPC/ymir work/npc/sinseon/sinseon.gr2 | 72 | 2 | 2.421 / 0 | 71 | 52 |
| Monster/ymir work/monster/wolf/wolf.gr2 | 40 | 1 | 762 / 0 | 38 | 31 |
| monster2/ymir work/monster2/fire_dragon/fire_dragon.gr2 | 113 | 8 | 5.820 / 0 | 111 | 40 |
| Monster/ymir work/monster/misterious_diseased_bosshost/misterious_diseased_bosshost.gr2 | 163 | 2 | 1.403 / 0 | 163 | 63 |
| NPC/ymir work/npc/horse/horse_normal.gr2 | 54 | 3 | 1.855 / 0 | 51 | 34 |
| patch2/ymir work/npc/boar/boar.gr2 | 39 | 1 | 2.229 / 0 | 37 | 28 |
| patch2/ymir work/npc/lion_white/lion_white.gr2 | 50 | 1 | 4.453 / 0 | 48 | 36 |
| metin2_patch_halloween/ymir work/npc/horse_halloween1/horse_halloween1.gr2 | 53 | 3 | 1.876 / 0 | 51 | 33 |
| metin2_patch_pet1/ymir work/npc/dinosaur/dinosaur_3.gr2 | 70 | 1 | 2.373 / 0 | 52 | 52 |

### Maxima und Gültigkeitsbereich

| Bereich | Maximum Skeleton | Maximum Mesh-Palette | Maximum Gruppe benutzt | Gültigkeit |
| --- | ---: | ---: | ---: | --- |
| gesamter gelesener Bestand | 163 | 163 | 96 | alle 9.166 lokalen GR2-Dateien |
| Player-Modelpfade ohne hair | 102 | 101 | 72 | 785 Model-Vorkommen, auch Kostüme/LODs |
| Hair-Pfade | 102 | 101 | 14 | 326 Model-Vorkommen |
| NPC-Pfade | 149 | 106 | 71 | enthält rigide Portale und Mount-Dateien |
| tatsächlich skinnbare NPC-Pfad-Meshes | 109 | 106 | 71 | Skeleton-Maximum bei pwahuang1 |
| Mob-Pfade | 163 | 163 | 96 | 492 Model-Vorkommen inkl. leerer Modelle |
| fünf bekannte M5D-Mountmodelle | 70 | 52 | 52 | oben: Pferd, Keiler, Löwe, Halloween-Pferd, Dinosaurier |
| reine weapon-Pfade | 1 | 1 | 1 | 147 Model-Vorkommen, keine Garantie für alle künftigen Waffen |

102 Player-Bones beispielsweise im pc2/sura/sura_assassin1-Patchmodell; 163 beim misterious_diseased_bosshost. Die Gruppe mit 96 benutzten Bones liegt in ent_boss2.gr2, dessen Skeleton 149 Bones hat. NPC-Maximum 149 stammt von wooden_door, nicht vom größten skinnbaren Händler.

Die naive Kategorie „Pfad enthält horse/mount/ride“ ergäbe nur 54 Mount-Bones. Sie wäre falsch als allgemeine Mount-Grenze: dinosaur_3 ist ein echter M5D-Mount mit 70 Bones. Der Bericht benutzt deshalb die nachweislich bekannte Mountauswahl. Eine vollständige semantische Race→Mount-Liste wurde nicht erfunden; jedes GR2 in diesem Bestand bleibt durch das globale Maximum 163 abgedeckt.

## 11. Bone-Palette: vorhanden, aber nicht als GPU-Ressource

Heute existieren:

1. Skeleton-weite World-/Composite-Matrixarrays pro tatsächlichem Poseowner.
2. Mesh.BoneBindings mit Bone-Namen.
3. Pro Instanz/Mesh eine Mappingtabelle zum Ziel-Skeleton.
4. Kein eigener GPU-Bone-Buffer.

Ein Mesh benötigt für Vertex-Deformation nur seine referenzierten Bones, aber nicht notwendigerweise alle Einträge seiner deklarierten Palette. Beispiel novice-Körper: 72 Mesh-Bindings, größte Materialgruppe benutzt 38. Das Skeleton mit Parents bleibt für die Animationsauswertung dennoch vollständig nötig.

B1 empfiehlt **keine erneute Bone-Reduktion oder Submesh-Palette-Kompaktierung**. Das wäre zusätzliche Indexumschreibung und ein neuer Paritätsrisikofaktor. Zunächst bestehende Mappings exakt übernehmen.

## 12. Mesh Bindings und Skeleton Remapping

CGrannyModelInstance::__CreateMeshBindingVector wählt das eigene Model-Skeleton oder das Skeleton der verlinkten Instanz und erzeugt für **jedes Mesh** GrannyNewMeshBinding. __DestroyMeshBindingVector gibt alle Bindings beim Clear frei. [S03]

Ein Self-Binding ist nicht zwangsläufig die Identität: schon beim novice-Körper sind 70 von 72 Einträgen anders als ihre lokale Nummer. Das entsteht aus Mesh-Bone-Namen/-Reihenfolge gegenüber Skeleton-Bone-Reihenfolge.

Für den späteren GPU-Pfad gibt es zwei korrekte Varianten:

- Lokale Vertexindices beibehalten, je Binding eine Matrixpalette P[k]=C[toBone[k]] zusammenstellen.
- Einmalig validierte binding-spezifische statische Vertexindices auf Ziel-Skeleton-Indizes umschreiben; dann je Poseowner nur C hochladen.

Empfehlung für den einmal-je-Pose-Upload: zweite Variante, aber mit gemeinsam nutzbarem Asset/Binding-Cache und ausdrücklicher Bindingsignatur, **nicht** einer neuen Vertexkopie pro Actor und Frame. Die erste Variante bleibt die einfachere Referenz zum Testen des Mappings.

Ein anderer Ziel-Skeleton-Owner, LOD-/Shapewechsel oder veränderte Skeleton-Generation muss eine neue Bindung ergeben. Niemals ausschließlich Race, VID, BoneCount oder recycelte Zeigeradresse als Identität verwenden.

## 13. Mesh → Submesh → Material → Draw

Ein granny_model besitzt MeshBindings. Ein CGrannyMesh hält einen Vertexbasisoffset und einen Indexbasisoffset innerhalb des Modells; rigide und deformierte Vertexbasen werden getrennt akkumuliert. GrannyCopyMeshIndices konvertiert in TIndex/uint16. Die Indizes bleiben mesh-lokal. [S17][S18]

granny_tri_material_group enthält MaterialIndex, TriFirst, TriCount. CGrannyMesh::LoadTriGroupNodeList erzeugt:

- idxPos = MeshIndexBase + TriFirst × 3;
- triCount;
- materialPaletteIndex aus der Mesh-Materialbindung;
- verkettete Gruppierung nach diffuse/blend.

Beim Draw gelten firstIndex=idxPos, indexCount=triCount×3, vertexCount=MeshVertexCount, baseVertex=MeshVertexBase; für rigide Teile im gemischten Actorbuffer addiert die Bridge deformVertexCount. Der Materialtyp ändert nicht die Skin-Palette.

**Reales Gegenbeispiel gegen „Submesh = eigenes Skeleton“: Wolf.**

| Gruppe | Material | firstIndex im Mesh | indexCount | verwendeter Vertexbereich | unterschiedliche Vertices | positiv benutzte Bones |
| --- | ---: | ---: | ---: | --- | ---: | ---: |
| 0 | 0 | 0 | 2.826 | 0–749 | 714 | 31 |
| 1 | 1 | 2.826 | 72 | 475–761 | 44 | 2 |

Beide Gruppen teilen dasselbe Mesh mit 762 Vertices, dieselbe 38-Einträge-Bindung und dasselbe 40-Bone-Skeleton. Bereiche können überlappen und sind nicht kompakt. **Keine unterschiedliche Palette pro Submesh erforderlich.** Eine optionale zukünftige Kompaktierung wäre eine neue Darstellung, nicht etwas, das der aktuelle Pfad verlangt.

## 14. Rüstungen / Shapes

CInstanceBase::SetArmor/SetShape bzw. ChangeArmor → CActorInstance::SetShape:

1. CRaceData::FindShape wählt Basis- oder Shape-GR2.
2. RegisterModelThing und vorhandene _lod_01 bis _lod_03 registrieren die Modellfamilie.
3. SetModelInstance(0,0,0) ersetzt den Körpercontrollerinhalt.
4. Neue ModelInstances, WorldPose und MeshBindings entstehen.
5. Skin-Dateinamen sowie Specular-/Materialwerte werden instanzseitig gesetzt.
6. Der umfangreichere ChangeArmor-Pfad kann Race/Actor neu aufbauen und Haare/Waffen erneut setzen.

Quellen: [Shape/Hair][S19], [InstanceBase-Ausrüstung][S34], [Thing-Instanzersatz][S35].

**Skeleton bleibt nicht als Objekt/Indexraum garantiert gleich.** Die tatsächlichen Kriegerbeispiele 75/74/76 Bones belegen das. Ähnliche Bone-Namen oder identische Job/Race sind kein hinreichender Cachekey. Die vorhandenen GR2-Layouts der untersuchten Rüstungen bleiben dagegen PWNT40.

Texture Overrides arbeiten über Materialnamen bzw. ursprüngliche Bildnamen und die instanzseitige CGrannyMaterialPalette. Die Shader-/PSO-/SRV-Logik der heutigen ActorBridge darf durch GPU-Skinning nicht geändert werden. Materialwechsel ohne Geometrie-/Skeletonwechsel erfordert nicht automatisch eine neue Palette oder ein neues Skinmesh.

## 15. Attachments

### Waffen: rigide Bone-Attachments im geprüften Bestand

AttachWeapon registriert das Item-Model/LOD im rechten oder linken Part, SetModelInstance erstellt die Waffeninstanz, AttachModelInstance bindet sie an den von RaceData benannten Bone. SetParentModelInstance löst den Namen mit GrannyFindBoneByName auf. [S36][S37]

~~~text
Körper: GrannyGetWorldPose4x4(handBone)                 [keine inverse Bind]
  → ParentOffset der Waffen-ModelInstance
    → Waffen-WorldPose / Composite
      → UpdateWorldMatrices:
          weaponComposite[weaponBinding[0]] × actorWorld
        → unveränderte rigide Waffenvertices → Draw
~~~

Die erforderliche Attachment-Parentmatrix ist die **aktuelle Joint-/WorldPose-Matrix**, nicht die Skin-Composite-Matrix des Körpers. Letztere enthält die Inverse-Bind und würde die Waffe falsch versetzen.

item/ymir work/item/weapon/00010.gr2: ein Bone, ein rigides Mesh, 558 Vertices, ein Material. Im Pfadbestand wurden 147 solche rigid-only-Waffenmodelle gefunden. Zukunftsassets dennoch nach tatsächlichem Mesh-Vertrag klassifizieren, nicht nur nach Part.

### Haare: kein allgemeines rigid-Attachment

SetHair ruft SetModelInstance(PART_HAIR, PART_HAIR, 0, PART_MAIN) auf. Damit benutzt die Hair-ModelInstance die Körper-WorldPose, hat aber **eigene Source-Meshes und MeshBindings** und wird heute separat CPU-geskinnt. [S19][S03]

hair_1_1: 74 Source-Bones, 73 Mesh-Bindings, 427 Vertices; alle haben nur einen positiven Einfluss. Trotz dieses Inhalts meldet Granny das Mesh als deformierbar. Keine automatische Umstellung auf rigid in B1/B2.

Neu gemessener echter Hair-Transfer:

| Zielkörper | Ziel-Bones | Änderungen gegenüber Hair-Selfmap | lokaler Vertexindex 5 wird Zielindex | Bone-Name |
| --- | ---: | ---: | ---: | --- |
| novice | 75 | 72 | 67 | Bip01 Head |
| warrior_4-1 | 74 | 40 | 66 | Bip01 Head |
| nahan | 76 | 72 | 68 | Bip01 Head |

Alle Zielindices gültig. Gleiche Bone-Anzahl im mittleren Fall verhindert also nicht ein anderes Mapping. [E02]

### Andere Attachments

Rigide Stücke innerhalb eines NPC-Körpers, beispielsweise Blacksmith, behalten ihre getrennte Bone×World-Matrix. Hair kann selbst skinned sein; rein statische Weltgeometrie braucht keine Actor-Skinpalette. Effekte, WeaponTrace, Collision-/Hit-Sphären und Bone-Abfragen brauchen weiterhin CPU-Bone-Posen. Ein visueller Schildpart wurde im bestehenden Projekt bewusst nicht erfunden; B1 ergänzt keinen.

## 16. Mounts und Reiter

CInstanceBase::SHORSE besitzt einen separaten CActorInstance. Er hat eigenes GR2, Skeleton, Animation, ModelInstances, MeshBindings und PNT-Snapshots. Reiter und Mount haben **keine gemeinsame zusammengesetzte Skeleton-Palette**. [S38]

__AttachHorseSaddle bindet Rider.PART_MAIN an Mount.PART_MAIN/"saddle". CActorInstance::INSTANCEBASE_Transform transformiert zunächst den Mount und übernimmt dessen Pixelposition mit der bestehenden Y-Konvention. Das ersetzt nicht die eigenständigen Bone-Posen.

Im normalen Diligent-Deformpfad wird der Mount zuerst gesampelt, dann der Reiter. Der aktuelle Sattel-Joint wird als ParentOffset in das Rider-Sampling gegeben. Der Reiter bleibt anschließend mit seiner bestehenden Worldmatrix gerendert. **Sattel oder Mountworld nicht ein zweites Mal im GPU-Shader multiplizieren.**

Die Bridge fordert für ein Mountpaar aktuelle, sichtbare CapturedFrames beider Körper. Das ist ein Stabilitätsvertrag, kein Mesh-Skinningdetail. Nach GPU-Skinning kann der Mount eine eigene Palette und der Rider eine zweite Palette haben; Hair teilt die Rider-Palette über ein eigenes Mapping, rigide Waffen folgen den bereits parentbeeinflussten Rider-Joints. [S06][S13]

Auf-/Absteigen bzw. Netzwerkswechsel benutzen den bestehenden Actor-Lifecycle. Kein neuer Mountmechanismus. M5D-Fixture und normale NetworkActorManager-Neuanlage bleiben Testvorbilder. [S39]

## 17. NPCs / Mobs / Bosse und Sonderfälle

Alle Granny-basierten Player-, NPC-, Mob- und Bosskörper erreichen denselben CGrannyModelInstance → CGrannyModel → CGrannyMesh-Deformkern. ActorCategory steuert Sichtbarkeit/Diagnose/Zuordnung, **keinen eigenen Skinningalgorithmus**. [S40]

Sonderfälle, die dieser gemeinsame Pfad erhalten muss:

- gemischte deformierte/rigide Meshes, z.B. Blacksmith 1.180+376;
- große Skeletons, z.B. Boss mit 163 Bones;
- mehrere Materials/Alpha-/Specular-/Blendzustände;
- Actor-Alpha, Unsichtbarkeit, Add/Modulate, Polymorph-/Racewechsel;
- aktive LOD-Modelle mit anderem Meshumfang;
- reine rigide Metins/Props und meshlose GR2-Modelle;
- Bäume verwenden SpeedTree, nicht Granny-Skinning;
- animierte/blendende Welt-Things verwenden seit M11 ebenfalls ActorModelSource und denselben Materialrenderer über StaticObjectBridge::SubmitSpecialThing;
- Charakterauswahl und andere Vorschaukontexte benötigen denselben Modelpfad, aber andere Kamera-/Licht-/Framekontexte.

Der neue Scan enthält 0 MorphTargets. Im betrachteten produktiven PNT-Deformpfad werden keine Morph-Targets aufgerufen. Das ist keine Zusage für zukünftige Dateien.

## 18. Aktueller CPU→GPU-Upload

CGrannyModel::CaptureActorSource liefert gemeinsam gehaltene CPU-Quelldaten: Model-Indexarray und rigide lokale Vertices. **GPU-Geometry liegt heute in ActorInstanceData pro ModelInstance**, nicht als globaler Skinmesh-Assetcache. [S17][S40]

Beim ersten gültigen Draw:

- CreateGeometry erzeugt je ModelInstance einen Indexbuffer mit USAGE_IMMUTABLE.
- Rigid-only: Vertexbuffer ebenfalls USAGE_IMMUTABLE, einmal initialisiert.
- Mindestens ein deformiertes Mesh: ein gemeinsamer USAGE_DYNAMIC/CPU_ACCESS_WRITE-Vertexbuffer für D+R Vertices des Models.
- Bei uploadedRevision != revision: UpdateDynamicVertices prüft Finite-Werte, mappt mit MAP_WRITE / MAP_FLAG_DISCARD und kopiert das gesamte Array.
- Alle folgenden Materialgruppen verwenden dieselbe GPU-Geometrie und Revision.

Quellen: [ActorBridge][S13], [ActorRenderer][S14], [Diligent-PNT-Ressourcen][S15].

**Uploads pro Actor** sind daher nicht pauschal 1: Körper, skinned Hair und Mount können je einen Upload benötigen; rigide Waffen normalerweise keinen dynamischen Upload. **Uploads pro Mesh** sind keine getrennten API-Maps; der Modelupload enthält die Bereiche mehrerer Meshes. Die Material-/ObjectConstants werden dagegen im Drawpfad pro Draw gemappt; diese separaten Updates erfasst actor_bytes nicht.

## 19. Bytes/frame, Vertices/frame und Drawcounts

Für sichtbare, neu gecapturete ModelInstances j:

~~~text
CPU-skinned vertices/frame = Summe D_j
dynamic vertex bytes/frame = 32 × Summe (D_j + R_j) für D_j > 0
dynamic vertex uploads/frame = Anzahl hochgeladener deformierbarer ModelInstances
index bytes at creation = 2 × ModelIndexCount je erzeugter Geometry
draws = Summe tatsächlich gerenderter Material-/Trianglegruppen und Durchläufe
~~~

Nicht enthalten: erstmalige Texture-/Indexuploads, ObjectConstants, CPU-Capturekopie, Allocator-/Treiber-Overhead. Statische R-Anteile in gemischten Models werden aktuell **erneut mitkopiert**, obwohl nur ihre Drawmatrix dynamisch ist.

### Reale Einzelmodelldaten

| Modell | CPU-deformiert | dynamisch hochgeladene Vertices | Vertexbytes pro Pose | normale Gruppen/Draws in einem Körperdurchlauf |
| --- | ---: | ---: | ---: | ---: |
| novice | 2.207 | 2.207 | 70.624 | 3 |
| warrior_4-1 | 2.582 | 2.582 | 82.624 | 8 |
| Doctor | 1.265 | 1.265 | 40.480 | 1 |
| Blacksmith | 1.180 | 1.556 | 49.792 | 3 |
| Wolf | 762 | 762 | 24.384 | 2 |
| horse_normal | 1.855 | 1.855 | 59.360 | 3 |
| hair_1_1 | 427 | 427 | 13.664 | 1 |
| rigide Waffe 00010 | 0 | 0 dynamisch | 0 dynamisch; 17.856 einmalig | 1 |

Ein Körperdurchlauf bedeutet: keine zusätzlichen Schatten-/Sonderdurchläufe, alle Gruppen sichtbar, keine weiteren Parts. Nicht jede Gruppe entspricht einem neuen Textureupload oder einer neuen Palette.

### Saubere Hochrechnung für 1/10/50/100 Actors

**Schätzung aus realen novice-Daten, kein neu gemessener Massenszenenbenchmark.** N identische sichtbare Grundkörper, höchster LOD, keine Haare/Waffen/Mounts, eine aktuelle Pose und ein regulärer Drawdurchlauf je Frame.

| Actors | skinnbare Vertices/frame | dynamische Vertexbytes/frame | Vertexuploads/frame | Körperdraws/frame | Nutz-Palettedaten, 75×64 B/Actor |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 2.207 | 70.624 | 1 | 3 | 4.800 |
| 10 | 22.070 | 706.240 | 10 | 30 | 48.000 |
| 50 | 110.350 | 3.531.200 | 50 | 150 | 240.000 |
| 100 | 220.700 | 7.062.400 | 100 | 300 | 480.000 |

Letzte Spalte nur künftiger Datenbedarf, **keine gemessene Ersparnis/Performancezusage**. Bei vollständigem Schreiben einer festen 256-Matrix-Palette wären es 16.384 Bytes je Actor statt 4.800. Bei 100 Actors/60 FPS bedeutet das rechnerisch 423.744.000 B/s heutiger Vertexpayload gegenüber 98.304.000 B/s Fixed-Palettepayload; daraus folgt nicht derselbe Faktor für FPS.

### Vorhandene M13C-Telemetrie als echte historische Baseline

Aus [normalem M13C-Protokoll][E03], neu gelesen, aber nicht erneut ausgeführt:

| Protokollstelle | sichtbare Actors | Vertexuploads | hochgeladene Vertices | davon skinned | Vertexbytes | Draws |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| frühe 1-Actor-Stelle, Zeile 59 | 1 | 2 | 3.009 | nicht für diese Tabellenzeile separat übernommen | 96.288 | 9 |
| Frame 840, Zeile 98 | 9 | 10 | 16.637 | 15.077 | 532.384 | 24 |
| Frame 960, Zeile 111 | 10 | 11 | 18.037 | 16.375 | 577.184 | 26 |

Frame 960: ein Player, neun Mobs, zwei sichtbare Attachments. Das erklärt, warum Uploads und Actoranzahl nicht identisch sind. Keine 50-/100-Actor-Liveaufzeichnung als solche vorhanden/ausgewertet.

Der historische Weltfixture-Snapshot hat 6 Actors, 5 Uploads, 5.527 Uploadvertices, 5.151 skinned, 176.864 Bytes und 99 Draws. [E04] Diesen gemischten Sonderpfad-/Map-Snapshot **nicht** als allgemeine „16,5 Draws pro Actor“-Faustregel verwenden.

actor_index_uploads ist kumulativ; actor_geometry/textures sind Resident-Zähler; actor_bytes/vertices/uploads sind Framewerte. Kategorien/Attachments sind teilweise Teilmengen, keine zusätzlich zu addierenden unabhängigen Gesamtmengen. [S14][S41]

## 20. Performance-Baseline: Messgrenzen

| Größe | Verfügbar? | B1-Aussage |
| --- | --- | --- |
| CPU-Skinningzeit je Actor/Mesh | nein, keine verwertbare aktive granulare Messung | nicht gemessen |
| CPU-Vertexuploadzeit | nein, keine separate aktive Dauer um Map/Copy/Unmap | nicht gemessen |
| gesamter Actor-CPU-Aufwand | keine isolierte belastbare Zeitreihe | nicht gemessen |
| Actor-/Skinning-GPU-Zeit | keine Timestamp-/Disjoint-Reihe im betrachteten Actorpfad | nicht gemessen |
| Payload/Upload-/Drawanzahl | ja | Abschnitt 19, mit Codeherkunft und historischen Framebeispielen |
| absolute FPS-Verbesserung | nein | nicht aus Bytes berechenbar |

Alte __PERFORMANCE_CHECKER__-Blöcke enthalten grobe Updatezeiten, teilweise Referenzen auf inzwischen auskommentierte Timer. Sie sind kein fertiger Skinningprofiler und wurden nicht aktiviert. Keine künstlich präzisen Millisekundenwerte aus dem isolierten Dateiscanner ableiten.

B10 benötigt später separate Timer für Granny-Sampling, CPU-Deformation, Capturekopie, Upload/Submission und GPU-Drawzeit mit Warmup/Disjoint-Prüfung. Die vollständige Phase-A-Testsuite wurde für diese reine Analyse bewusst nicht wiederholt.

## 21. Tatsächlich eingebundene Granny-Version und APIs

[Granny-CMake][S42] importiert ausschließlich extern/library/Granny/granny2_static.lib. BUILDING_GRANNY_STATIC und GRANNY_THREADED werden im aktuellen Root-CMake gesetzt. [S43]

Neuer Lauf gegen diese Library:

- Header GrannyProductVersion: **2.11.8.0**
- GrannyGetVersionString: **2.11.8.0**
- GrannyVersionsMatch: **1**
- Library SHA-256: **C8DE47645CE41CFE9E37D15BB26FB0BC919BA1B648E6BE7746730C37D8671A5E**
- vorhandene Deform.cpp SHA-256: **C73674782ADA9D554237F1522605ABA3BFCBCBFF66FAB14E2FD34545B1FC9DE7**

| API/Struktur | heutiger Einsatz / künftige Eignung |
| --- | --- |
| GrannyReadEntireFileFromMemory / GetFileInfo | Assetadapter; nicht im Shader/Renderer verteilen |
| GrannyInstantiateModel / FreeModelInstance | Animatorinstanz |
| GrannyPlayControlledAnimation, Control*-Funktionen | unverändertes Playback/Blending |
| GrannySetModelClock | unveränderte Zeitführung |
| GrannySampleModelAnimationsAccelerated | aktuelle Pose ohne notwendigen Vertex-Deformcall |
| GrannyGetWorldPose4x4 / Array | Jointpose für Attachments/Kollision |
| GrannyGetWorldPoseComposite4x4 / Array | bereits benutzte Skinmatrizen |
| GrannyNewMeshBinding / GetMeshBindingToBoneIndices | aktueller Quell-/Ziel-Indexvertrag |
| GetMeshBindingFromBoneIndices / FromSkeleton / ToSkeleton | SDK vorhanden, für Diagnose/Validierung geeignet |
| GrannyBuildMeshBinding4x4Array | SDK vorhanden; nicht blind als Ersatz verwenden, Transfer-/Inverse-Bind-Semantik zuerst gegen heutigen gather(C,map)-Vertrag prüfen |
| GrannyBuildIndexedCompositeBuffer / Transposed | SDK vorhanden; nicht nötig für ersten Hook, transponierte 3×4-Variante hat anderes Layout |
| GrannyBuildWorldPose / BuildWorldPoseComposites | SDK vorhanden; aktueller Accelerated-Pfad bleibt zunächst bestehen |
| GrannyGetMeshVertexType / GetMeshVertices | originale statische Skinningdaten |
| GrannyCopyMeshVertices / CopyMeshIndices | vorhandene Format-/Indexkonvertierung, keine neue Runtime erforderlich |
| granny_vertex_data, granny_bone_binding | originale Vertexbeschreibung, Binding-Namen, Boundingdaten |
| GrannyNewMeshDeformer / DeformVertices | heutige CPU-Referenz/Fallback, zunächst erhalten |

Quellen: [SDK Sampling][S44], [SDK Mesh/Binding][S45], [SDK WorldPose][S46]. Kein Granny-Source wurde eingebunden oder aus externer Theorie auf eine unbekannte SDK-Version geschlossen.

Zusätzlich wurden SampleModelAnimationsAccelerated, GetWorldPoseComposite4x4Array, BuildMeshBinding4x4Array, BuildIndexedCompositeBuffer und BuildWorldPoseComposites als Symbole in der tatsächlich importierten statischen Library nachgewiesen. Das bestätigt ihre Verfügbarkeit, nicht automatisch die Parität jeder unbenutzten Transfer-/Packingvariante.

## 22. Matrixkonvention und genaue Formel

### Speicher / Koordinaten

- Math::Matrix: 16 float32, 64 Bytes, gepackte Werte, Zeilenvektoren; Translation in _41/_42/_43 bzw. Floatoffsets 12/13/14. [S47]
- granny_matrix_4x4 wird im aktuellen Code direkt als Math::Matrix/float[16] genutzt.
- Der SSE-Kern lädt die vier Speicherzeilen und berechnet p.x*r0 + p.y*r1 + p.z*r2 + 1*r3. Das belegt **Vektor × Matrix** für diese gespeicherten Matrizen unabhängig von anderslautender generischer SDK-Nomenklatur.
- Diligent-HLSL verwendet ausdrücklich row_major und mul(float4(position,1), World), anschließend View, Projection. [S48]
- Geprüfte GR2-ArtTool-Basis: Right=(1,0,0), Up=(0,0,1), Back=(0,-1,0), UnitsPerMeter=100; RH/Z-up. Die Spiel-Pixelkoordinaten haben ihre bestehende Y-Umkehr. Keine zusätzliche Achsenumkehr im Skinning vorschlagen.
- Binddaten sind gespeicherte InverseWorld4x4 je Skeletonbone und originale Skinvertices. LocalTransform ist die Default-/Resthierarchie; nicht voraussetzen, dass deren Sample in jeder importierten Datei exakt die Bindpose reproduziert.

### Exakte Referenz

Für Mesh m, Vertex v, aktive Destination-Skeleton-Pose:

~~~text
r_i       = MeshBindingToBoneIndices_m[BoneIndices_v[i]]
w_i       = float(BoneWeights_v[i]) * (1.0f / 255.0f)
J_b(t)    = aktive Granny WorldPose4x4 für Bone b
I_b       = gespeicherte InverseWorld4x4 desselben aktiven Skeletons
C_b(t)    = I_b * J_b(t)          [Row-Vector-Konvention]
q_v       = sum_i w_i * ((position_v, 1) * C_{r_i}(t))
p_model   = q_v.xyz
p_clip    = ((p_model, 1) * ActorWorld) * View * Projection
~~~

Wichtig: Der heutige CPU-Kern speichert nur xyz. Der spätere VS bildet daraus **erneut w=1**. Bei einer hypothetischen fehlerhaften Weight-Summe dürfte man nicht einfach ein gewichtet gebliebenes q.w unverändert in ActorWorld weiterreichen. Im aktuellen Bestand ist die Summe 255; der präzise Vertrag bleibt trotzdem relevant.

Bei Hair ist „aktives Skeleton“ der Körperowner, nicht das Source-Hairskeleton. Nicht eigenmächtig sourceInverseBind * destinationJoint als allgemeines Retargeting einführen: die heutige Referenz liest die **Composite-Matrix der Destination-WorldPose** über die vorhandene Mappingtabelle.

J kann bereits ParentOffsets enthalten: bei der Waffe Handbone, beim Rider Sattelbone. „WorldPose“ bedeutet deshalb Skeleton-/Parentraum, nicht automatisch endgültiger Map-Weltraum. ActorWorld wird weiterhin genau einmal im Draw angewandt.

Für rigide Meshes bleibt:

~~~text
p_clip = (position_local,1) *
         (C_{meshBinding[0]} * ActorWorld) * View * Projection
~~~

### Neu numerisch geprüft

9 Asset-/Clipfälle × 3 Zeiten × mit/ohne synthetischen Parentoffset = **54 CPU-Posefälle**, plus Haar-Transfers und synthetische Gewichte/Skalierung. Warrior wait/run, Rüstung attack; NPC/Mob/Boss/Mount zusätzlich Default-Posen, kein behaupteter vollständiger Animationssweep. [E02]

- unabhängige skalare Formel gegen vorhandenen SSE-Kern: maximale Differenz **0** in diesen Fällen;
- I×J gegen tatsächlich geliefertes Granny-Composite: maximal **0,00012207**;
- vertauschte Reihenfolge J×I: Fehler bis **920,469** mit Parentoffset;
- GrannyDeformVertices gegen SSE: Position maximal **0,000488281**, Normal maximal **2,38419e-7**;
- diese Differenzen sind Float-Rundungen im untersuchten Sample, keine GPU-Paritätsmessung.

Default-Sample ohne Clip ist bei Sinseon/Pferd/Dinosaurier nicht identisch zur originalen Skin-Vertexposition (z.B. Pferd bis 292,953 Einheiten). Sowohl SDK- als auch vorhandener SSE-Deformer stimmen miteinander überein. **Kein Beleg für einen neuen Fehler**, sondern ein Grund, Bind-/Restdaten nicht aus einer angenommenen Identität neu zu berechnen. Finale Parität gegen aktuelle Granny-Composites und bestehende CPU-Ausgabe prüfen.

## 23. Normal-Skinning

Exakte CPU-Normale:

~~~text
n_model = sum_i w_i * ((normal_v,0) * C_{r_i}).xyz
n_eye   = (n_model,0) * inverseTranspose(ActorWorld * View)
optional: normalize(n_eye), nur wenn der aktuelle Drawstate dies verlangt
~~~

Für rigide Stücke ist in der letzten Zeile World bereits BoneComposite×ActorWorld. [S27][S13][S48]

Der CPU-Deformer:

- benutzt keine eigene inverse-transponierte Bone-Normalmatrix;
- ignoriert Translation;
- übernimmt Scale/Shear aus dem linearen Composite-Anteil;
- normalisiert weder pro Einfluss noch nach dem Blend.

Der Bridge-/Shaderteil verwendet dagegen bereits eine inverse-transponierte **World×View**-Normalmatrix und die vorhandene normalizeNormals-Option. Der Default-State ist FALSE; die heutige Actor-Skinningfunktion setzt ihn nicht neu. [S49]

Der synthetische Nonuniform-Scale-Test ergibt Normalenlänge 2,28817 statt 1 und stimmt exakt mit dem skalaren Vertrag überein. „Physikalisch korrektere“ Bone-Normalmatrizen oder permanente Normalisierung wären eine visuelle Änderung, keine neutrale Migration. Spätere Beleuchtungsverbesserungen ausdrücklich außerhalb Phase B.

## 24. Empfohlenes minimales GPU-Vertexformat

**Vorschlag, kein produktiv hinzugefügtes Format:** das vorhandene 40-Byte-PWNT-Layout beibehalten.

| Feld | Speicher | Offset | zukünftige IA-Interpretation |
| --- | --- | ---: | --- |
| position | float32×3 | 0 | float3 |
| weights | uint8×4 | 12 | uint4, nicht normalisiert; explizites 1/255 gemäß Referenz |
| indices | uint8×4 | 16 | uint4, nicht normalisiert, binding-validierte Zielindices |
| normal | float32×3 | 20 | float3 |
| uv0 | float32×2 | 32 | float2 |

Stride 40, Alignment 4. Keine float4-Gewichte/uint32×4 im Vertexbuffer nötig; der Shader kann die Byteattribute in breitere Register laden. Keine Tangents, Farben, UV1 oder künstlichen Materialattribute hinzufügen.

Im vorhandenen Diligent-InputLayout ist IsNormalized standardmäßig TRUE; für **beide Byteattribute explizit FALSE** wählen, sonst werden Bone-Indizes versehentlich zu [0,1]-Werten. [S50] Ein UNORM-Weightformat wäre alternativ möglich, aber zunächst den CPU-Faktor exakt beibehalten und Rundungsparität prüfen.

Vier Influences reichen nach aktuellem vollständigem Scan. Loader muss Format, Anzahl, Indexbereiche, BoneCount und Finite-Werte validieren. Zero-weight-Lanes mit sicherem Index 0 ablegen oder nachweislich ohne ungültigen Matrixzugriff behandeln. Nicht aus der heutigen Datenlage eine unbegrenzte 4-Influence-Garantie für künftige Assets ableiten.

Bindungsspezifische Indexumschreibung betrifft nur eine neue statische GPU-Capturekopie; Granny-Originalvertices und der CPU-Referenzpfad bleiben unangetastet.

## 25. Bone-Buffer-Empfehlung

Ein StructuredBuffer ist selbst ein über SRV sichtbarer Buffer; „Structured Buffer“ und „Shader Resource Buffer“ sind keine vollständig disjunkten Techniken.

| Strategie | Bewertung für diesen Bestand |
| --- | --- |
| separater Constant Buffer | einfachster erster Vertrag; 256 vollständige Matrizen = 16 KiB, deckt gemessene 163 ab; pro Owner/Pose aktualisieren, über alle Draws binden |
| StructuredBuffer/SRV | gut für spätere gepackte Paletten vieler Actors und variable Größen; Offsetverwaltung, Update-/Lifetime-/SRV-Vertrag aufwendiger |
| typed Buffer/SRV oder Matrixtextur | möglich; zusätzliche Adressierung und Formatwahl, für höchstens 163 Bones aktuell kein sachlicher Vorteil als Einstieg |

D3D11-FL11 garantiert 4.096 Constant-Buffer-Elemente à float4, also 64 KiB Shader-sichtbaren Bereich; ein vollständiges 4×4 verbraucht vier Elemente. Eine separate 256er-Palette liegt deutlich darunter. Constant-Buffer-Größen müssen 16-Byte-konform sein. [Microsoft Ressourcenlimits](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-resources-limits), [Bufferbeschreibung](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_buffer_desc)

**Klare Empfehlung für B2:** Backend-neutraler Palettevertrag mit float32[16] pro Bone, anfängliche geprüfte Obergrenze 256; Diligent-D3D11 zunächst über separaten dynamischen Constant Buffer. ActorWorld/Materialkonstanten getrennt lassen. Keine frühe 3×4-Kompression/Transposition.

- 75 Bones: 4.800 Bytes Nutzinhalt.
- 163 Bones: 10.432 Bytes Nutzinhalt.
- feste 256er-Kapazität: 16.384 Bytes.
- Bei 100 Actors sind 100 solche Ressourcen 1.638.400 Bytes logische Kapazität, **ohne** mögliche Treiber-Renaming-/Framekopien.
- Für den einfachsten deterministischen Anfang die feste Palette vollständig definiert schreiben; unbenutzte Einträge z.B. Identität. Eine spätere nur-benutzte-Bones-/Ringbuffer-Optimierung gehört in B10.
- Überläufe nicht abschneiden: in B2/B3 kontrolliert beim CPU-Pfad bleiben und begrenzt diagnostizieren.
- Für einen späteren StructuredBuffer nicht ungeprüft die Constant-Buffer-Mapstrategie übertragen. Native D3D11-Misc-/CPUAccess-Einschränkungen beachten; passenden Diligent-Uploadpfad separat testen. [Microsoft Misc Flags](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ne-d3d11-d3d11_resource_misc_flag)

Ein zukünftiger Vulkan-Backendadapter kann dieselben logischen Paletten über Uniform-/Storagebuffer transportieren; Device-Limits und Alignment dort erst dann prüfen. **Keine Vulkan-Unterstützung oder Portabilitätsfreigabe in B1.** Ein Textur-Palettepfad ist für den Einstieg nicht empfohlen.

## 26. Resource Lifetime

### Aktuell

~~~text
Spawn / Race / Shape
  → ResourceManager-CGraphicThing-Referenz
  → GrannyFile + Models + Skeleton + Original-Skinvertices
  → CGrannyLODController / ModelInstance
  → GrannyInstance + eigene oder verlinkte WorldPose
  → MeshBinding-Vector + Materialpalette + CPU-Scratch
  → Rendercapture + instanzeigene GPU-Geometry/Texturen
  → Despawn / Shape-/LOD-Lifecycle
    → ModelInstance::Clear
      → Renderbindings freigeben, ActorInstanceData zurücksetzen
      → CPU-Buffer, Bindings, Meshmatrizen, GrannyInstance, WorldPose
    → bei letzter Assetreferenz: Model/Motion/Filename-Ressourcen freigeben
~~~

CGraphicThing::OnClear löscht Motions/Models vor GrannyFreeFile. LoadModels gibt bereits einzelne rigide Vertex-/Index-/Texturesections frei; deformierbare Sourcevertices bleiben für heutiges Skinning erhalten. [S16][S03]

### Später vorgeschlagene Besitzer

| Ebene | Ressourcen |
| --- | --- |
| Asset / Source-Mesh | unveränderliche Skinvertices, ursprüngliche lokale Boneindices/-weights, UV, Indizes, Materialzuordnung, Skeletondefinition |
| Asset+Binding-Signatur | gemeinsam nutzbare statische GPU-Vertexdaten mit validiertem Remap; ggf. Indexbuffer/Drawranges |
| Actor / tatsächlicher Poseowner | GrannyAnimator, Playback, aktive Bindingreferenzen, Palettebuffer/-revision |
| Actor-Part | Material-/Textur-Overrides; eigene Bindung; Hair kann Paletteowner des Körpers referenzieren |
| Frame | stabile Pose-/Paletteversion und Drawreferenzen; keine geliehenen Granny-Pointer über nachfolgendes Sampling hinweg |
| Despawn/Shapechange | references lösen; GPU-resident Ressourcen erst gemäß Backend-/Frame-Lifetime freigeben |

Keine Materialpalette oder Texture/SRV durch Geometrysharing versehentlich actorübergreifend teilen. Despawn mitten in queued Frames und gepoolte ModelInstance-Adressen brauchen Generation/Ownership, nicht Pointergleichheit allein.

CPU-Jointmatrizen bleiben für Gameplay, Collisions, Attachments und Rootmotion nötig, selbst wenn sämtliche skinnbaren Vertices irgendwann GPU-seitig entstehen.

## 27. Static-/Dynamic-Datentrennung

| Daten | Heute | Künftiger Vertrag |
| --- | --- | --- |
| originale Skinposition/Normal/UV | Granny-File, CPU liest sie bei jedem Deform | unveränderlicher Skin-Vertexbuffer |
| Byteweights / lokale Indizes | Granny-Mesh | unveränderlich; bindungsspezifischer GPU-Remap getrennt |
| Triangle-Indizes / Gruppen | Modelsource + instanzeigener GPU-Indexbuffer | asset-/bindinggeteilt, kein Frameupload |
| Materialdefinition/Texture-Asset | Asset/Palette | Assetdaten teilbar |
| Materialfarbe/Alpha/Override | Instanz/Draw | dynamischer Drawvertrag, nicht global statisch machen |
| Bone-Composites | CPU-WorldPose | pro Poseowner aktualisierte Palette |
| Joint-WorldPose | CPU | bleibt CPU-seitig für vorhandene Nutzer |
| ActorWorld, View, Projection | Drawkonstanten | weiterhin Drawkonstanten |
| Animatorstate / Controls | Granny-Instanz | weiterhin Granny-Instanz |
| fertig deformierte PNT | CPU-Scratch + Snapshot + Dynamic VB | bis zur Parität Referenz/Fallback; erst B11 gezielt entfernen |

„Materials sind statisch“ wäre als allgemeine Behauptung falsch: statische Definition und veränderliche Instanzwerte auseinanderhalten.

## 28. Vorgeschlagene Zielarchitektur

~~~text
GR2 / bestehender ResourceManager
  └─ GrannyAssetAdapter
       ├─ Model/Skeleton/Clips                         [weiter Granny]
       └─ SkinMeshSource                              [unveränderlich]
            ├─ PWNT40-Source
            ├─ uint16-Indizes + Drawranges
            └─ Materialzuordnung
                  │
       source + destination skeleton + mapping signature
                  ↓
          StaticSkinnedMeshBinding                    [geteilt]
            └─ unveränderliche GPU-Geometrie

Actor / LOD / Part
  └─ bestehender GrannyAnimator
       ├─ WorldPose/Joints → Attachments/Kollision/Mount
       └─ CompositePalette + revision                 [Poseowner]
             ↓
          Diligent Bone Buffer
             ↓
          späterer Skinning-VS
             ↓
          bisheriges PNT-/Material-/Licht-/Alpha-Verhalten
             ↓
          Diligent DrawIndexed
~~~

Körper, NPC, Mob und Mount benutzen denselben Vertrag. Hair referenziert Skeletonowner plus eigenes Binding. Rigid-only-Waffen und rigide Teilmeshes laufen weiter durch den bestehenden rigiden Materialpfad; nicht künstlich vier Gewichte hinzufügen.

Bis vollständige Parität erreicht ist: CPU bleibt Standard/Referenz. Ein Fehler in Format, Binding oder Palette darf nicht die fertige Phase A ersetzen oder zum stillen Nichtzeichnen führen.

## 29. Vorbereitung ZiiNAN Asset Runtime – nur Grenzen

| spätere API-Grenze | heutige Stellen / einzukapselnde Granny-Nutzung |
| --- | --- |
| IModel | CGraphicThing/CGrannyModel; FileInfo.Models, Model.MeshBindings, Gruppen/Materiale |
| ISkeleton | granny_skeleton/bone; Namen, Parentindices, gespeicherte InverseBind, BoneLookup |
| IAnimationClip | CGrannyMotion; FileInfo.Animations, Duration/Tracks |
| IAnimator | CGrannyModelInstance Motion/Update; InstantiateModel, Controls, Clock, AcceleratedSample, WorldPose |
| ISkinnedMesh | CGrannyMesh; VertexType/Vertices, Weights/Indices, Drawranges und Bindingvertrag |
| SkinBinding / PoseView | explizites Ergebnis von NewMeshBinding + tatsächlichem Poseowner, Lifetime/Revision |

Keine dieser Interfaces wurde angelegt. Die ersten GPU-Skinning-Schritte brauchen keine komplette Runtime-Neuimplementierung. Besonders sinnvoll: Granny-Zeiger an der GameLib/EterGrn-Grenze auf klar besessene CPU-Werte/Handles abbilden, statt den Diligent-Renderer Granny-Objekte lesen zu lassen.

Rootmotion über UpdateTransform/GrannyUpdateModelMatrix, Bone-OBBs, Collision- und Attachment-Abfragen gehören weiterhin zur Animator-/Skeletonseite, nicht zum Skinning-Shader.

## 30. Risiken und Absicherungen

| Risiko | konkrete Ursache | vorgesehene Absicherung |
| --- | --- | --- |
| Bone-Limit | künftige Datei >256 oder fremde Packvariante | Asset-/Bindingvalidierung; keine Trunkierung; CPU-Fallback bis Erweiterung geprüft |
| falsches Vertexlayout | PNT2 und PWNT beide 40 B; leere Meshes | Namen/Typ/ArrayWidth/Offsets prüfen, leere Geometrie überspringen |
| falscher Boneindex | Mesh-local ≠ Skeletonindex | exakte ToBone-Tabelle; Tests mit nichtidentischem Mapping |
| Weight Precision | UNORM-Konvertierung/implizites Gewicht/Normalisierung | originale Bytes, expliziter Referenzfaktor, vier Lanes |
| Zero-weight-Lane | unbenutzter Index trotzdem im Shader geladen | kanonischer sicherer Index oder nachgewiesener Bounds-Schutz |
| Normalen | inverse Bone-Normalmatrix wäre neue Beleuchtung | bestehenden linearen CPU-Vertrag und World/View-Normalmatrix erhalten |
| Nonuniform scale | ungeprüfte Normalisierung/Shearverlust | synthetische und reale CPU/GPU-Geometrie-/Lichtparität |
| InverseBind | Default-Sample nicht überall Identität | gespeicherte InverseWorld/aktuelle Composites verwenden |
| Matrixtransposition | Row-/Column-Verwechslung, 3×4-Packing | 4×4/64B zunächst beibehalten, nicht symmetrische Testmatrizen |
| Parentraum | Hand/Sattel zweimal multipliziert | genaue Joint-vs-Composite-Verträge, Parent vor Child |
| Rüstungswechsel | gleiche Race, anderes Skeleton/Mapping | Bindinggeneration aktualisieren, 75/74/76-Bone-Tests |
| Hair | geteilte WorldPose, eigene Map | Owner-/Bindingtrennung; reale Head-Index-67/66/68-Prüfung |
| Mount | zwei animierte Körper, Parent-Sampling-Reihenfolge | getrennte Paletten, aktueller Mount vor Rider, Paar-CapturedFrame |
| große Bosse | maximale Palette/weit entfernte Koordinaten | 163-Bone-Boss + 96-Bone-Gruppe + FireDragon-Test |
| LOD/Blending | Controls/Clocks und Zielowner wechseln | vorhandene CopyMotion/Ease-Verträge erhalten, kein Posecache nach Race |
| Materialstate | falsche PSO/SRV-Nutzung beim neuen VS | bisherige Pixel-/Blend-/Depth-/Samplerpfade unberührt, 5B-Isolationstest erweitern |
| starre Mischteile | CPU-PNT bisher D+R in einem Upload | separate Drawmatrizen/Ranges erhalten; nicht doppelt skinnen |
| Ressourcenteilung | pro Actor wieder gesamte Geometrie kopiert | Asset-/Bindingsignaturcache; Instanzmateriale getrennt |
| Lifetime | Posepointer nach erneutem Sample, gepoolte Adressen | besessene Snapshots/Generationen, Frame-Fencing/Referenzen |
| Threads | SharedLocalPose ist statisch | B2 bleibt synchron; Threading nicht nebenbei beginnen |
| Sonderkontexte | Auswahl/Vorschau/WorldThing fehlt im Test | dieselben Modelverträge außerhalb normaler Ingame-Actors prüfen |
| Assetpriorität | physischer Scan ≠ gewählte Packvariante | in B2 Herkunft/Hash des tatsächlich gebundenen Assets erfassen |
| falscher Performance-Schluss | weniger Bytes ≠ gleicher FPS-Gewinn | erst B10 mit getrennten CPU-/GPU-Messungen beurteilen |

## 31. Testplan für B2/B3 und spätere Freigaben

### B2: Infrastruktur ohne Shader-/Bildwechsel

- Originale statische Skinvertices neben dem unveränderten CPU-Pfad capturen.
- sizeof/alignof/offsetof plus reflektierte SDK-Layouts vergleichen.
- Alle aktuellen 3.118 Skinmeshes, leere Models, PNT2 und rigide Meshes als Eingabe-/Abgrenzungsfälle.
- Asset-/Binding-/Palettegenerationen, zero-weight indices, Overflow, ungültige Mappings testen.
- Granny-Composites und MeshRemaps byte-/numerisch mit CPU-Referenz vergleichen.
- Mehrere gleiche Assets teilen Geometrie, aber besitzen verschiedene Posen/Materialwerte.
- Material-State-Isolation aus M5B behalten, keine neuen Log-Spams.
- Release + Debug, relevante Renderer-/Resource-/Actor-Tests; bei produktiver Instrumentierung gemäß Auftrag auch entsprechende Builds.
- Auf allen sichtbaren Szenen weiter ausschließlich bisherigen CPU-Draw verwenden.

### B3: erster strikt opt-in Vergleichspfad

| Bereich | zwingende Fälle |
| --- | --- |
| erster Referenzactor | Grundkrieger, feste Pose, Idle/Walk/Run/Attack, gleiche Kamera |
| Zeit/Blending | wiederholtes Looping, Ease-In/Out, Motionwechsel, Hitstop/Paralyse, LODwechsel |
| Rüstungen | novice, warrior_4-1, nahan; Wechsel mit aktivem Hair/Weapon |
| NPC | Doctor, Sinseon, Blacksmith; Alpha-/Materialwechsel |
| Mob/Boss | Wolf mit zwei Gruppen, 163-Bone-Boss, FireDragon, ent_boss2 |
| Mount | Fuß→Pferd→anderer Mount→Fuß, Sattelposition, bewegter Rider, Mountattack |
| Attachments | Schwert, beide Dolche, Bogen, Bell/Fan, wechselnde Haare; kein neuer Schildpfad |
| Sichtbarkeit | Kamera weg/zurück, Despawn/Respawn, mehrere Gruppen, versteckte Actors |
| Lifecycle | A1→B1→A1, Relog, Charakterauswahl→Ingame, Shape-/Race-Neuanlage |
| Fenster | Resize mehrfach, 3× Minimize/Restore, mehrere Starts |
| Ende | Shutdown Exitcode 0; Palette-/Skinmesh-/Binding-/bestehende Actor-GPU-Owner nach Shutdown 0 |

B3 aktiviert zunächst nur den zugelassenen Referenzfall. Die Tabelle ist zugleich die später schrittweise abzuarbeitende Gesamtmatrix, **kein Auftrag, alle Kategorien sofort in B3 umzubauen**. Bestehende Renderer-Tests wie ActorGpuChecks, ActorStateIsolationChecks und die M5D/M11-Fixtures erweitern, nicht umgehen. [S51][S39]

## 32. CPU/GPU-Paritätsstrategie

1. Exakt dieselben Asset-/Binding-/Materialgenerationen, Animationstime, Granny-Composites, ActorWorld, View, Projection und Renderstates verwenden.
2. CPU-Referenz unmittelbar aus dem vorhandenen SSE-Kern sichern; nicht als Referenz einen zweiten unabhängig veränderten Animator laufen lassen.
3. Geometrie-Parität vor Pixel-Parität: später im isolierten GPU-Test den tatsächlichen Skinning-Output per Stream-Output oder geeignetem Test-Readback auslesen. Kein neuer produktiver Readback/Compute-Skinningpfad für B1.
4. Positionen und **nicht vorab normalisierte** Normalen pro Vertex vergleichen; UV muss unverändert bleiben. Rigid-Mischteile und BaseVertex-/Indexranges separat prüfen.
5. Erste numerische Toleranz nur als Teststartwert: Positionskomponente 1e-4 + 2e-6×max(1, Betrag der Referenz), Normalkomponente 1e-5. Maximal-/RMS-Fehler und Ausreißer ausgeben; nach GPU-Messung begründen, nicht blind lockern.
6. Nonsymmetrische Matrices, ungleichmäßige Skalen, Parentoffset, mehrere Einflüsse und nichtidentische Remaps verhindern „zufällig passende“ Identitätstests.
7. Danach identische Kamera/Resolution/Framepose mit CPU- und GPU-Draw; Silhouette, Texturkoordinaten, Licht/Specular, Alpha-Test/Blend, Depth und Attachments vergleichen.
8. Pixelvergleich braucht kontrollierte Zeit, Effekte/Wetter/RNG bzw. maskierte Fremdanimationen, gleiche Gamma-/MSAA-/Clearbedingungen. Nicht jeden Partikelunterschied als Skinningfehler zählen.
9. Korrekte Skinningformel darf trotzdem über FMA/Operationen andere Rundungen erzeugen; die jetzigen SDK-vs-SSE-Abweichungen zeigen den Maßstab, sind aber keine pauschale GPU-Toleranzfreigabe.
10. Erst nach Geometrie-, Bild-, Material-, Lifecycle- und Ingame-Parität per Kategorie umschalten.

**In B1 wurde keine GPU-Parität behauptet oder getestet**, weil noch kein GPU-Skinning existiert.

## 33. Konkrete B2/B3-Architektur und empfohlene Reihenfolge

### Minimale spätere Änderungspunkte

| Stelle | mögliche spätere isolierte Ergänzung |
| --- | --- |
| CGraphicThing::LoadModels / CGrannyModel::CaptureActorSource | zusätzliche immutable Skin-Sourcecapture vor Sectionfreigabe |
| CGrannyMesh | reflektierte Layout-/Weightvalidierung, Originalmesh-ID/Drawranges |
| CGrannyModelInstance::__CreateMeshBindingVector | eigener CPU-Wertevertrag für Bindinggeneration/Remap |
| CGrannyModelInstance::Deform | Palettecapture nach Sampling, CPU-Deform zunächst weiterhin ausführen |
| ActorRenderData | separater SkinSource-/Pose-/Bindingvertrag, bisheriger PNT-Vertrag bleibt gültig |
| ActorRenderBridge::Submit | später opt-in Auswahl nach validiertem Skinbinding; bisherige Materialcapture beibehalten |
| DiligentActorRenderer | später eigene Skin-Geometry/Palettebesitzer; rigid/PNT beibehalten |
| DiligentStaticObjectRenderer | nicht pauschal den statischen VS ersetzen; gemeinsamer Materialteil darf in B3 gezielt wiederverwendet werden |
| Clear/LOD/Shape/Mount-Lifecycle | referenzgezielte Freigabe/Generation, kein globaler Reset pro Draw |

B2 soll statische Daten und Bonepaletten neben dem unveränderten CPU-Rendering aufbauen. Für die empfohlene einmal-je-Owner-Palette werden GPU-Vertexindices einmal je Asset+Binding auf das Ziel-Skeleton abgebildet. B3 fügt einen separaten Skinning-VS/PSO-Vertrag hinzu, der die bestehenden PNT-Ausgaben und Material-/Lichtparameter reproduziert. **Keine der beschriebenen Ergänzungen wurde in B1 implementiert.**

### Roadmap mit Stop-Gates

| Phase | Umfang | Gate vor nächster Phase |
| --- | --- | --- |
| B2 | statische Skin-Source, Binding-/Palettevertrag, CPU-Parallelcapture | Daten/Owner/Layouttests; unverändertes Bild |
| B3 | erster opt-in Skinning-VS für feste Kriegerreferenz | numerische Positions-/Normalparität und einfacher Pixelvergleich |
| B4 | Krieger Idle/Walk/Run/Attack, LOD/Blend/Visibility | stabile Referenz inkl. Material-/Fenster-/Lifecycletests |
| B5 | Player-/Rüstungsvarianten | Shape-/Skeletonremapping, mehrere Klassen/Kostüme |
| B6 | NPC/Mobs/Bosse | gemischte rigide Teile, Alpha, 163-Bone-/96-Gruppenbone-Fälle |
| B7 | Mounts/Rider | unabhängige Paletten, Parentpose, Auf-/Absteigen |
| B8 | Hair, Waffen-/rigide Attachmentgrenzen, Vorschau/WorldThing-Sonderfälle | Bindings/Owner und alle Phase-A-Kontexte vollständig |
| B9 | umfassende Geometrie-/Bild-/Ingame-Parität | alle Kategorie-/Lifecycle-Gates bestanden |
| B10 | getrennte CPU/GPU-Benchmarks, 1/10/50/100 Actors | gemessener Nutzen ohne Paritätsverlust |
| B11 | CPU-Vertexdeformation aus freigegebenen Produktionspfaden entfernen | vollständige Parität/Lifetime-Abnahme; CPU-Skeleton/Granny bleiben |

Abweichung von einer rein linearen Roadmap: **Parität beginnt bereits B2/B3**, nicht erst B9. Die Hair-/Attachment-Mappingfähigkeit muss im Datenvertrag B2 vorhanden sein, auch wenn Hair erst später GPU-seitig aktiviert wird. Kein frühes Abschalten der Referenz.

## 34. GO / NO-GO und Abschluss

### GO – als technische Empfehlung, nicht als bereits erteilte Implementierungsfreigabe

Die vorhandene Granny-Runtime kann Animation/Skeleton inklusive Bone-Composites ohne Vertex-Deformation liefern. Originaldaten und Bindinginformationen sind zugänglich. Vier Byteeinflüsse und die gemessenen maximal 163 Bones passen in einen einfachen ersten GPU-Vertrag. Es braucht weder Granny-Rewrite noch neue Materialien.

Eine spätere **begrenzte B2-Infrastrukturaufgabe** ist daher sinnvoll, wenn CPU-Rendering Default/Referenz bleibt und die im Bericht beschriebenen Asset-, Binding-, Parent- und Lifetime-Grenzen eingehalten werden.

### NO-GO

- CPU-Skinning jetzt produktiv entfernen oder GPU-Skinning allgemein als „fertig/paritätssicher“ bezeichnen.
- Skeletonindex = Meshindex setzen oder Remaps nur anhand Race/BoneCount cachen.
- Rüstungen/Haare/Mounts auf ein hypothetisch einheitliches Skeleton reduzieren.
- Alle Bone-Normalen neu normalisieren oder InverseBind aus angenommener Restidentität rekonstruieren.
- D3D11-Byteersparnis als gemessenen FPS-Gewinn ausgeben.
- 163 als ewige Asset-/Servergrenze behandeln.
- B2, Shader, Granny-Ersatz, PBR, Vulkan, DX12 oder andere Renderer-Modernisierung in diesem Auftrag beginnen.

### Tatsächliche B1-Prüfungen

| Prüfung | Ergebnis |
| --- | --- |
| Read-only Scan Release, vollständiger Bestand | 9.166 Dateien, 0 Leseausnahmen, Exitcode 0 |
| Standalone Diagnose-Build Release | erfolgreich |
| Standalone Diagnose-Build Debug | erfolgreich |
| CPU-Pose-/Formelvergleich Release | 54 Posefälle + 3 Hair-Bindings + synthetischer Gewicht-/Scale-Fall, Exitcode 0 |
| gleicher CPU-Vergleich Debug | erfolgreich, Exitcode 0 |
| leeres Multi-Model-GR2 im Debug-Assetprüfer | erfolgreich, Exitcode 0 |
| produktive Builds / Testsuite | nicht erneut ausgeführt; keine Produktionsänderung |
| neue Live-/Login-/Fensterregression | nicht erforderlich/nicht ausgeführt; kein Spielclient gestartet |
| neue GPU-Ressourcen | keine erzeugt; kein GPU-Skinningtest |
| Phase-A-Sourceänderung | keine |

Der erste Stand des **separaten Prüfers** beendete sich beim Iterieren eines fehlenden VertexType in zwei leeren Meshes innerhalb redthief2_soldier2_lod_01.gr2. Die Diagnose wurde um den Empty-Mesh-Fall ergänzt und der gesamte Scan danach ohne Skip erneut durchgeführt. **Kein Clientfix und keine beschädigte Assetdatei behauptet.** Das leere Model 0 darf zugleich nicht stellvertretend für das nichtleere Model 1 derselben Datei ausgewertet werden.

Die CPU-Poseprüfungen sind keine neu gemessene Ingame-Regressionsfreigabe. Bestehende Phase-A-Abnahmen werden nicht als in B1 wiederholte Tests ausgegeben.

Git-Änderung dieses Auftrags: nur dieser neue Analysebericht. Die temporären CPU-Analyseprogramme, CSVs und Ausgaben liegen unter build/phase-b1 und sind nicht Teil des Produktionsbuilds. Keine Änderungen an Servern, Runtime-Packages, Granny-Library, Deform.cpp, Renderer oder Shadern; kein Commit/Push.

**B1 abgeschlossen. STOP – B2 nicht begonnen.**

## Anhang A – Reproduzierbare lokale Evidenz

Alle folgenden Verweise sind reale lokale Dateien/Verzeichnisse. Die großen CSVs bleiben bewusst im ignorierten Buildbereich; die für Entscheidungen nötigen Ergebnisse sind in diesem Bericht enthalten.

| Datei | Inhalt |
| --- | --- |
| [survey-final/metadata.txt][E01] | Runtime/Header-Version, Strides/Alignment, vollständige Scananzahl |
| [survey-final/models.csv][E05] | Model/Skeleton/Mesh-/Vertexzahlen, maximale Paletten-/Gruppenwerte |
| [survey-final/meshes.csv][E06] | jedes Meshlayout, Influence-Histogramm, Weight-Summen, Mappingprüfung |
| [survey-final/groups.csv][E07] | Materialgruppen, Index-/Vertexbereiche, positiv benutzte Bones |
| [survey-final/files.csv][E08] | jede GR2-Datei, Models/Animationen/Skeletons, Leseergebnis |
| [pose-analysis.txt][E02] | Release: konkrete Matrixfehler, SSE/SDK-Vergleich, Hair-Zielindices |
| [pose-analysis-debug.txt][E09] | gleicher CPU-Test in Debug |
| [Probe.cpp][E10] / [Pose.cpp][E11] / [Diagnose-CMake][E12] | ausschließlich lokale CPU-Analysewerkzeuge |
| [M13C normal][E03] / [M13C Welt][E04] | historische Upload-/Drawcounter, keine B1-Neumessung |

Reproduktion ohne Clientstart, vom Source-Root aus; diese Befehle bauen ausschließlich die Diagnoseprojekte:

~~~powershell
& 'C:/Program Files/CMake/bin/cmake.exe' -S build/phase-b1/probe -B build/phase-b1/probe-build -A x64
& 'C:/Program Files/CMake/bin/cmake.exe' --build build/phase-b1/probe-build --config Release
& ./build/phase-b1/probe-build/Release/B1ReadOnlyProbe.exe 'C:/Users/ZiiNAN/Documents/GitHub/m2dev-client/assets' './build/phase-b1/survey-final'
& ./build/phase-b1/probe-build/Release/B1PoseProbe.exe 'C:/Users/ZiiNAN/Documents/GitHub/m2dev-client/assets' './build/phase-b1/pose-analysis.txt'
~~~

Der finale Scan benutzt **keine Skip-Liste**. Alte Zwischenstände survey/single* sind nicht die Entscheidungsgrundlage.

## Anhang B – Source-Einstiegspunkte

[S01]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/GameLib/ActorInstanceInterface.h:5>
[S02]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterGrnLib/ThingInstance.h:10>
[S03]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterGrnLib/ModelInstanceModel.cpp:28>
[S04]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/UserInterface/PythonApplication.cpp:174>
[S05]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/UserInterface/PythonCharacterManager.cpp:151>
[S06]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/UserInterface/InstanceBase.cpp:1953>
[S07]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/GameLib/ActorInstance.cpp:14>
[S08]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterGrnLib/ThingInstance.cpp:706>
[S09]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterGrnLib/LODController.cpp:450>
[S10]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterGrnLib/ModelInstanceUpdate.cpp:58>
[S11]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/GameLib/ActorInstanceRender.cpp:33>
[S12]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterGrnLib/ModelInstanceRender.cpp:172>
[S13]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/GameLib/ActorRenderBridge.cpp:67>
[S14]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/Renderer/DiligentActorRenderer.cpp:6>
[S15]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/Renderer/DiligentStaticObjectRenderer.cpp:207>
[S16]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterGrnLib/Thing.cpp:157>
[S17]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterGrnLib/Model.cpp:36>
[S18]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterGrnLib/Mesh.cpp:46>
[S19]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/GameLib/ActorInstanceData.cpp:56>
[S20]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/GameLib/RaceData.cpp:315>
[S21]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterGrnLib/Motion.cpp:29>
[S22]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterGrnLib/ModelInstanceMotion.cpp:47>
[S23]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterGrnLib/ThingInstance.cpp:457>
[S24]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/GameLib/ActorInstanceMotion.cpp:640>
[S25]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/extern/include/granny.h:2266>
[S26]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterLib/GrpDevice.cpp:36>
[S27]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterGrnLib/Deform.cpp:8>
[S28]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterLib/GrpVertexBuffer.cpp:24>
[S29]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterGrnLib/LODController.cpp:33>
[S30]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/extern/include/granny.h:3137>
[S31]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterLib/GrpBase.h:79>
[S32]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/Renderer/StaticObjectRenderData.h:7>
[S33]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/extern/include/granny.h:2851>
[S34]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/UserInterface/InstanceBase.cpp:2668>
[S35]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterGrnLib/ThingInstance.cpp:347>
[S36]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/GameLib/ActorInstanceAttach.cpp:138>
[S37]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterGrnLib/ModelInstance.cpp:45>
[S38]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/UserInterface/InstanceBase.cpp:231>
[S39]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/tests/Renderer/mounts_smoke.py:1>
[S40]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/Renderer/ActorRenderData.h:1>
[S41]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/Renderer/TerrainPresentation.cpp:220>
[S42]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/extern/library/Granny/CMakeLists.txt:1>
[S43]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/CMakeLists.txt:59>
[S44]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/extern/include/granny.h:3296>
[S45]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/extern/include/granny.h:4795>
[S46]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/extern/include/granny.h:5686>
[S47]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/Math/Math.h:1>
[S48]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/Renderer/DiligentStaticObjectRenderer.cpp:58>
[S49]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterLib/DrawState.cpp:173>
[S50]: <C:/Users/ZiiNAN/Documents/Codex/2026-09-11/referenced-chatgpt-conversation-this-is-an/work/DiligentCore/Graphics/GraphicsEngine/interface/InputLayout.h:88>
[S51]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/tests/Renderer/ActorGpuChecks.h:1>
[A00]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client/assets>
[E00]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build/phase-b1>
[E01]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build/phase-b1/survey-final/metadata.txt>
[E02]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build/phase-b1/pose-analysis.txt>
[E03]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build/milestone13c/normal-release-final/default-runtime/terrain-renderer.log:59>
[E04]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build/milestone13c/world-release-final/default-runtime/terrain-renderer.log:7>
[E05]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build/phase-b1/survey-final/models.csv>
[E06]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build/phase-b1/survey-final/meshes.csv>
[E07]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build/phase-b1/survey-final/groups.csv>
[E08]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build/phase-b1/survey-final/files.csv>
[E09]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build/phase-b1/pose-analysis-debug.txt>
[E10]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build/phase-b1/probe/Probe.cpp>
[E11]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build/phase-b1/probe/Pose.cpp>
[E12]: <C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build/phase-b1/probe/CMakeLists.txt>
