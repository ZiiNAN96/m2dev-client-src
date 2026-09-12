<!-- ZiiNAN: 5B source and original-asset analysis before production changes. -->
# Milestone 5B – Analyse vor Codeänderungen

12.09.2026. Saubere Sourcebasis `757930e` (abgenommenes 5A). Source: `C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src`; Originalassets: `C:/Users/ZiiNAN/Documents/GitHub/m2dev-client/assets`. Nachfolgende Sourcepfade relativ zu dieser Sourcewurzel. Keine Änderungen an Originalassets oder Servern.

## 1. Actor-Abdeckung: 22 angeforderte Befunde

| Nr. | Befund mit konkreter Quelle |
|---|---|
| 1 | Kein hardcodierter Krieger im 5A-Renderer: `ActorInstance.cpp::INSTANCEBASE_Deform`, `ActorInstanceRender.cpp::OnRender` und `ActorRenderBridge.cpp::Body` begrenzen auf `IsPC() && !IsPoly()` und nicht beritten. Der Testbestand ist Race 0 / Shape 0. |
| 2 | `Renderer/ActorRenderData.h`, `DiligentActorRenderer`, PNT-Upload, uint16-Indizes und ModelInstance-Snapshot sind race-neutral. |
| 3 | `GameLib/ActorInstance.h::EType`: ENEMY=0, NPC=1, PC=6; außerdem STONE/WARP/DOOR/BUILDING/POLY/HORSE/GOTO/OBJECT. `ActorInstance.cpp::IsPC/IsNPC/IsEnemy` prüfen diese Typen. |
| 4 | `UserInterface/CInstanceBase` besitzt `GameLib/CActorInstance`; gleiche Kernklassen für Player, NPC, Mob und Boss. |
| 5 | Ja: alle drei Kategorien benutzen `CActorInstance`; kein zweiter NPC-/Mob-Renderer gefunden. |
| 6 | Gleiche `CGraphicThingInstance` → LODController → `CGrannyModelInstance`, `DeformPNTVertices`, `RenderMeshNodeListWithOneTexture`, Materialpalette und TriGroup-Loops. |
| 7 | Fertige deformierte Daten: PNT332, 32 Byte; kein Color-/Bone-Attribut am GPU-Eingang. Eingebettete rigide Meshes werden durch die schon vorhandene Granny-PNT-Konvertierung geladen und separat mit Bone*World transformiert. |
| 8 | Gemeinsame native uint16-Indizes, mesh-lokal mit Mesh-BaseVertex und globalem Indexoffset. Deformierte und rigide native VBs besitzen getrennte Basisoffsets. |
| 9 | Ja: Krieger 3–8 Meshes, Schmied 3, Hauptmann 2, Bksoldier 2, Ork-Lord 2; unten genaue Inventur. |
| 10 | Rüstungs-Specular stammt aus Item-/Shape-Laufzeitdaten, nicht pauschal aus GR2. `Material.cpp::__ApplySpecularRenderState` aktiviert Stage1-SphereMap und TextureFactor. |
| 11 | SDK-Inventur: `defence`, `hotel_grandfa`, `wolf`, `tiger`, `mountain_dog_god`, weitere unten. Native Materialklasse BLEND führt im normalen Actorlauf zu `BeginOpacityRender`: AlphaTest GREATER 0. |
| 12 | Der Name TYPE_BLEND_PNT beweist kein aktiviertes Alpha-Blending. `BeginOpacityRender` schaltet nur den Test; native BlendEnable wird geerbt. Zusätzlich führt `RENDER_MODE_BLEND` für Fade/Despawn SRCALPHA/INVSRCALPHA und konstantes TextureFactor-Alpha ein. |
| 13 | Beispielsweise Krieger-Novice Face, Nahan (3 Materialien), 4-1 (8), NPC goods (1). Actor-Normalpfad setzt ohnehin temporär CULL_NONE; Material kann ebenfalls NONE setzen. |
| 14 | Specular: Stage0 Texture*Diffuse RGB, TextureAlpha*FactorAlpha; Stage1 CURRENT.rgb + CURRENT.a*SphereTexture.rgb. ADD/MODULATE-Actor-Modi benutzen Stage1 CURRENT +/* TFACTOR. Opacity-Pass OneTexture benutzt nur die native Stage0-Diffusetextur, nicht eine zusätzlich erfundene Opacity-Stufe. |
| 15 | Material.Emissive ist Teil der bestehenden Lichtauswertung. Actor-ADD/MODULATE sind vorhandene Farb-/Hover-/Trefferzustände, keine Partikeleffekte; sie müssen am tatsächlich aktiven nativen Draw gelesen werden. Kein neuer Emissive-/Effektrenderer. |
| 16 | Actor CULL_NONE, native ZENABLE/LESSEQUAL/ZWRITE, Alpha-/Blendzustand pro Pass. Keine typabhängige neue Depth-/Cull-Regel. |
| 17 | `ModelInstanceUpdate.cpp::UpdateWorldMatrices`: deformierte Meshes Actor-World; rigide Meshes vorhandene Bone-Matrix*Actor-World. Native Kamera-View/Projection bleiben gleich. |
| 18 | In untersuchten RaceData-Ladern keine separate Mob-Scale-Tabelle gefunden. Größen unterscheiden sich bereits in den Originalmodellen. `CGraphicObjectInstance::Transform` übernimmt Rotation und Position; der vorhandene SetScale-Speicher wird hier nicht neu in den Transform hineininterpretiert. Renderer übernimmt ausschließlich die tatsächlich fertige native Meshmatrix. |
| 19 | NetworkActorManager/InstanceBase: Race, ActorType, Armor; `SetRace` → RaceManager, `SetArmor` → Item Shape, `SetShape` → MSM/Skins und ModelInstance. |
| 20 | PCs registrieren mehrere Parts; `SetHair`, `AttachWeapon` usw. sind eigenständige Attachments. Mount-Rider verknüpft Parent/Saddle. Diese Part-/Attachmentpfade werden nicht durchlaufen. |
| 21 | Goods, Bank, Defence sowie Hund/Bär/Wolf/Fuchs verwenden vollständige Hauptmodelle. Schmied/Bksoldier/Ork-Lord haben rigide Bestandteile direkt im selben GR2, keine dafür separat angelegten Weapon-Parts; deren vollständiger Hauptmodellpfad ist eine begrenzte notwendige Ergänzung. |
| 22 | `CInstanceBase::SHORSE::Create` erzeugt einen PC-getypten separaten Actor; 5A-`IsPoly`-Schutz bleibt. Pet-Flag existiert serverintern, wird im untersuchten CharacterAdd-Paket nicht separat übertragen (nur Typ/Race). Deshalb keine pauschale Freigabe jeder NPC-getypten Begleiterrasse. |

## 2. Gemeinsame Call-Chains

Player/NPC/Mob: `CPythonApplication::RenderGame` → `CPythonCharacterManager::Render` → vorhandene distanzsortierte Alive-/Dead-Listen → `CInstanceBase::Render` → `CGraphicObjectInstance::Render` → `CActorInstance::OnRender` → `CGraphicThingInstance::RenderWithOneTexture` / `BlendRenderWithOneTexture` → aktueller LOD → `CGrannyModelInstance::RenderMeshNodeListWithOneTexture` → Materialpalette / `ApplyRenderState` → nativer D3D9-Draw → `RestoreRenderState`.

Deformation: derselbe Manager → `CInstanceBase::Deform` nach `__CanRender` → `CActorInstance::INSTANCEBASE_Deform` → `CGraphicThingInstance::OnDeform` → aktuelle ModelInstance → unveränderte WorldPose/WorldMatrices → `CGrannyModel::DeformPNTVertices` → Mesh-SSE2 oder `GrannyDeformVertices` → fertiges PNT im nativen gelockten VB → 5A-Snapshot → Unlock. Keine eigene Animation, kein zweites Skinning.

## 3. Rüstung und Shape: 13 Befunde

| Nr. | Tatsächlicher Pfad |
|---|---|
| 1 | `InstanceBase.cpp::SetArmor` übersetzt ItemVnum über `__ArmorVnumToShape`; ItemData liefert SpecularPower. |
| 2 | `root/msm/warrior_m.msm` enthält ShapeIndex, Model, SourceSkin/TargetSkin; PC-Race wird über PlayerSetting/RaceManager geladen. |
| 3 | `ActorInstanceData.cpp::SetShape`: RegisterModelThing(0), optionale `_lod_XX.gr2`, SetModelInstance(0,0,0). |
| 4 | Skeleton nicht blind als identisch behandeln: Novice 75, Nahan 76, Saja 75, Cheongrin 75, 4-1 74 Bones. Native Granny-Bindings und ModelInstance-Neuanlage bewältigen dies; kein neuer Bone-Indexvertrag. |
| 5 | Mesh/Material und gegebenenfalls Skeletonstruktur ändern sich; Renderer bekommt nur die fertig deformierte Pose plus finale Matrix. |
| 6 | SourceSkin-Matching → `SetMaterialImagePointer` bzw. `SetMaterialData` → instanzbezogene Materialpalette. |
| 7 | Beispielsweise Shape 0/1 gleiches Novice-Modell, rote/blaue Zieltextur; Shape 3/4 gleiches Nahan-Modell, Nahan/Giryung-Textur. |
| 8 | Native Specular-SphereMap sowie Alpha-/Blend-/ADD-/MODULATE-Actorstates; kein moderner Materialgraph. |
| 9 | `CActorInstance::OnRender` setzt Material.Diffuse aus `m_dwMtrlColor`; Farbmodi zusätzlich TextureFactor. PNT besitzt keine Vertexfarbe. |
| 10 | ItemData SpecularPower und SphereMapIndex; Material-Specular wird bei AlphaBlend TRUE nativ übersprungen. MR-12 aktualisiert Specular vor jedem Materialdraw, daher muss die Bridge nach Apply lesen. |
| 11 | Fünf ausgewählte Rüstungskörper haben keine GR2-Opacity-Materialien. Keine erfundene Alpha-Test-Rüstung als Beleg. Alpha-Test wird zusätzlich mit echten NPC-/Mob-Materialgruppen geprüft. |
| 12 | Keine transparenten Teilgruppen in den ausgewählten Rüstungsassets gefunden; globaler nativer Fade-/Blendmodus ist trotzdem relevant. |
| 13 | PNT, Stage0-Modulation, vorhandenes Licht, Factor-/SphereMap-Stufe, sampler/mips, AlphaTest/AlphaBlend, Cull/Depth und aktuelle Matrizen; keine neue Lichtpipeline. |

## 4. Nachgewiesene Assetmatrix

SDK-Inspector aus 4B wiederverwendet (GrannyReadEntireFile/GetFileInfo/GrannyMeshIsRigid), kein eigener GR2-Parser. `build/milestone5b/actor-inventory.jsonl` enthält 14 gezielte Modelle; `catalog-inventory.jsonl` 134 vorhandene NPC-/Monster-Hauptdateien, keine Vollabnahme des gesamten Assetbestands.

| Hauptmodell / Race | Bones | Deform/rigid Meshes | Vertices gesamt | CPU-Skinning | Material / 5B-Bedarf |
|---|---:|---:|---:|---|---|
| Warrior novice / 0 Shape 0 | 75 | 3/0 | 2207 | gemeinsam | 5A vorhanden |
| Warrior nahan / Shape 3,4 | 76 | 4/0 | 3025 | gemeinsam | Shape/Override/Specular |
| Warrior saja / Shape 6 | 75 | 5/0 | 3631 | gemeinsam | Specular |
| Warrior cheongrin / Shape 9 | 75 | 5/0 | 3324 | gemeinsam | Specular |
| Warrior 4-1 / Shape 12 | 74 | 8/0 | 2582 | gemeinsam | tatsächlicher 5A-Rüstungs-Sonderfall |
| Goods / 9003 | 38 | 1/0 | 1110 | gemeinsam | NPC-Auswahl, Two-Sided |
| Bank / 9004 | 38 | 1/0 | 1086 | gemeinsam | NPC-Auswahl |
| Blacksmith / 20016 | 48 | 1/2 | 1556 | gemeinsam + native rigid matrices | Hauptmodell-Mischgeometrie |
| Guard leader | 60 | 1/1 | 2011 | gleicher Kern | eingebettetes starres Teilmesh |
| Stray dog / 101 | 38 | 1/0 | 682 | gemeinsam | Mob-Auswahl |
| Bear / 110 | 36 | 1/0 | 621 | gemeinsam | Mob-Auswahl |
| Bksoldier / 301 | 62 | 1/1 | 1409 | gemeinsam + rigid | humanoider Mob |
| Orc lord / 691 | 74 | 1/1 | 1668 | gemeinsam + rigid | Boss, kein eigener Renderer |
| Fox ninetail | 44 | 1/0 | 1312 | gemeinsam | größeres Hauptmodell |

Nachgewiesene Opacity-Materialien im Inventar: NPC defence/hotel_grandfa; Mobs wolf, tiger, wild_boar_god, mountain_dog_god, misterious_diseased_dog, skeleton_wizard, sugu_general, yellow_tigerman. Diese benutzen in den gefundenen Gruppen denselben Dateinamen für Diffuse/Opacity; der native OneTexture-Draw liest Stage0. Warp ist ebenfalls im Inventar, bleibt als anderer Actor-Typ ausgeschlossen. Keine Abnahme allein aus Assetmetadaten ableiten.

## 5. Minimale geplante Änderungen und Grenzen

1. `ActorRenderBridge.{h,cpp}`, `ActorInstance.cpp`, `ActorInstanceRender.cpp`: gemeinsamer Hauptkörper-Scope für Player/NPC/Mob; kein neuer Managerscan. Begleiter und fremde Parts bleiben gesperrt. Da das Clientpaket keinen verlässlichen Pet-Typ liefert, zunächst ausdrücklich begrenzte Klassifikation: reguläre PCs, NPC-Races unter 20100 und Enemy-Races unter 8000. Höhere/sonstige Kategorien werden dokumentiert, nicht als vollständig unterstützt behauptet.
2. `Renderer/ActorRenderData.h`, `EterGrnLib/Model.cpp`, `ModelInstanceUpdate.cpp`: Snapshot um im Hauptmodell enthaltene rigide PNT-Daten erweitern, vor dem bestehenden Section-Free mit vorhandener Granny-Konvertierung kopieren; deformierte Bytes bleiben unverändert. Ein gemeinsames Dynamic-VB enthält Deformbereich plus starre lokale Vertices; starre Draws behalten native Bone*World. Keine separaten Attachment-Parts.
3. Ein enger, nur im aktiven Actor-Scope aufgerufener lesender Callback in `EterGrnLib/ModelInstanceRender.cpp`, unmittelbar am nativen Materialdraw nach `ApplyRenderState`. Dadurch bleiben Reihenfolge, MR-12, Shape-Overrides, Opacity-/Fade-Pässe und native Zustände die Referenz. Vorherige zweite 5A-Materialschleife entfernen, keine doppelten Diligent-Draws.
4. `GameLib/StaticObjectBridge.cpp` lesenden gemeinsamen State-Capture um streng geprüfte Actor-Modi ergänzen; statische Defaults unangetastet. `Material.h` ggf. rein lesender Zugriff auf die schon geladene SphereMap-Image-Ressource.
5. `Renderer/StaticObjectRenderData.h`, `DiligentStaticObjectRenderer.cpp`: vorhandene PNT-/Licht-/Texturimplementierung um originale TextureFactor-/SphereMap-/ADD-/MODULATE-Operationen erweitern. Native Stage1-Sampling/Matrix, keine PBR-/Shader-Modernisierung. Bestehende 4B-/5A-Pixeltests müssen weiterhin bestehen.
6. `DiligentActorRenderer.{h,cpp}`, `TerrainPresentation.cpp`: sichtbare Player/NPC/Mob getrennt zählen; Upload-/Draw-/Texture-/Mesh-Lifetime weiter messen. Keine Optimierung.
7. `tests/Renderer`: zusätzliche echte D3D9-Pixelreferenzen für Materialien, gemischte Geometrie und Zähler; isolierter Originalasset-Test für mehrere Shapes/NPCs/Mobs, Fade, Spawn/Despawn, A1/B1/A1, mehrere Kamera-/Windowzustände. ON/OFF, volle Testsuite, normale Ingame-Abnahmen, Exitcodes und Ressourcenprüfung.

Offizielle Semantik für die begrenzte Reproduktion: [D3DTEXTUREOP](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dtextureop), [automatische Texturkoordinaten](https://learn.microsoft.com/en-us/windows/win32/direct3d9/automatically-generated-texture-coordinates). Numerische und visuelle Parität müssen die tatsächlichen nativen Draws nachweisen.

Nach erfolgreicher 5B-Abnahme STOP. Keine separate Waffen-/Haar-/Mount-/Pet-, UI-, Effekt-, SpeedTree- oder Schattenmigration.
