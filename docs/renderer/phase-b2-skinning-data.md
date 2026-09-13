# Phase B2 – Static Skin Data und Bone Palette Infrastructure

Stand: 13.09.2026. Grundlage: [B1-Analyse](<C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/docs/renderer/phase-b1-gpu-skinning-analysis.md>), Source-Basis `72a695884e3708effabc3a3114b7478d6fc26bae`.

## Ergebnis und Scope

Die neue Datenbasis liegt **parallel zum unveränderten CPU-Skinning** vor. Originalvertices, Topologie, Materialgruppen, Source-/Destination-Remaps und aktuelle Composite-Matrizen sind als CPU-Werte zugänglich. Keine neuen Draws, Shader, Pipeline-States oder GPU-Ressourcen; keine Granny-Ersetzung, keine neue Asset-Runtime, kein B3.

Die B1-Matrix-/Index-/Weight-Konventionen wurden übernommen und mit realen Assets erneut gegen den vorhandenen SSE-Deformer und den Granny-SDK-Deformer geprüft. Es gibt keinen Widerspruch zu B1. Der ergänzte Schutz gegen einen inkompatiblen verknüpften LOD-Pose-Besitzer ist eine Absicherung der **neuen Sidecar-Daten**, kein behaupteter oder korrigierter Produktionsfehler.

## 1. Neue Datenstrukturen und Integrationsgrenzen

| Datei | Aufgabe |
| --- | --- |
| [SkinningData.h](<C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/Renderer/SkinningData.h>) | Backend-neutrale CPU-Typen, Validierung, Remap-Cache, Palette, Lifetime-Zähler |
| [SkinningDataAdapter.h](<C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterGrnLib/SkinningDataAdapter.h>) / [Adapter.cpp](<C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterGrnLib/SkinningDataAdapter.cpp>) | Ausschließlich die Grenze von Granny-Daten zu eigenen Werten |
| [Model.cpp](<C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterGrnLib/Model.cpp>) / Model.h | Einmalige statische Erfassung beim Modellaufbau, noch vor Freigabe der GR2-Sections |
| [ModelInstanceSkinning.cpp](<C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterGrnLib/ModelInstanceSkinning.cpp>) | Remaps aus vorhandenen Instanz-Bindings; Pose-Besitz und verknüpfte Skeletons |
| ModelInstanceModel.cpp / ModelInstanceUpdate.cpp / ModelInstance.h | Kleine Create/Clear/Update-Hooks und lesende Getter |
| [SourceResourceAudit.h](<C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/src/EterLib/SourceResourceAudit.h>) | Eine zusätzliche zusammengefasste Shutdown-Zeile |

Produktionskette bleibt:

```text
Granny-Sampling → vorhandener CPU-Deformer → vorhandene PNT-Ausgabe → Diligent-Upload → vorhandener Draw
       │
       └─ B2: Composite-Matrizen als eigene CPU-Werte kopieren

Modellaufbau → B2: gemeinsame Original-Skinvertices / Indices / Materialgruppen
Instanz-Binding → B2: exakten vorhandenen Mesh→Ziel-Skeleton-Remap übernehmen
```

`productionSkinningMode` ist eine unveränderliche Konstante `SkinningMode::CPU`. Der Enum benennt einen späteren GPU-Modus, schaltet aber nichts um. Kein Startargument, UI-Schalter oder Hot-Switch wurde hinzugefügt.

## 2. StaticSkinnedMeshData

Enthält Meshname, Original-Meshslot, Offset im bestehenden deformierten Modell-VB, Mesh-Bone-Anzahl, `vertices`, `meshToSourceSkeleton`, 16-Bit-Indices, ursprüngliche Materialgruppen und Validierungsstatistik. Die Erfassung geschieht einmal in `CGrannyModel::CreateFromGrannyModelPointer`, nach dem bisherigen Modellaufbau und vor den Sectionfreigaben durch `CGraphicThing::LoadModels`.

`SkinningModelData::meshes` behält die Original-Meshslots. Rigid/Empty/Unsupported haben keinen künstlichen Skin-Mesh-Eintrag und einen expliziten Status. Ein gemischtes Modell kann gültige skinned und unveränderte rigid Teile enthalten. Für einen späteren GPU-Draw müssen **Meshstatus, Remap und fertige Palette gemeinsam** gültig sein; ein vorhandenes Modell allein ist keine GPU-Freigabe.

## 3. Vertexlayout

Exakt der in B1 belegte PWNT3432-Inhalt, keine produktive Vertexformatumstellung:

| Feld | Typ | Offset | Bytes |
| --- | --- | ---: | ---: |
| position | float[3] | 0 | 12 |
| weights | uint8_t[4] | 12 | 4 |
| indices | uint8_t[4] | 16 | 4 |
| normal | float[3] | 20 | 12 |
| uv | float[2] | 32 | 8 |

Stride **40 Byte**, natürliche CPU-Ausrichtung **4 Byte**. Größe und Offsets sind durch `static_assert` gegen die eigene Struktur und die tatsächliche Granny-Struktur gesichert. Kein float4-Aufblähen. Die derzeitige produktive CPU-Ausgabe bleibt PNT mit **32 Byte**.

Für einen späteren Diligent-Inputvertrag wären Position/Normal/UV Float32 und Indices/Weights jeweils vier UInt8-Komponenten möglich. Bei Integer-HLSL-Eingängen ist Diligents `IsNormalized` ausdrücklich `False` zu setzen; die bestehende Bytegewichtinterpretation wäre dann `weight * (1/255)`. **B2 erzeugt keinen InputLayout-/Shadervertrag.**

## 4. Bone-Indexformat

Die vier Originalindices bleiben unverändert `uint8_t` und **mesh-lokal**. `meshToSourceSkeleton[local]` beschreibt die Selbstbindung zum ursprünglichen Modell-Skeleton. `BoneRemap::meshToSkeleton[local]` beschreibt dagegen die **konkrete vorhandene Instanzbindung zum Ziel-Skeleton**. Letztere gilt etwa für Haare am Rüstungskörper.

Die vorbereitete Full-Skeleton-Palette hat keine weitere Verdichtung: `paletteIndex == destinationSkeletonIndex`. Eigene Remap-Einträge sind `uint16_t`; der aktuelle Vorbereitungsschutz erlaubt höchstens 256 Bones. Es wird noch kein remapped GPU-VB hergestellt.

## 5. Weightformat und Normalisierung

Alle vier Bytegewichte werden erhalten; kein implizites viertes Gewicht, kein Renormalisieren, kein Clamp. Aktive Lane: Gewicht ungleich null. Null-Lanes werden wie im CPU-Pfad nicht dereferenziert.

In den B2-Stichproben: **64.055 originale skinned Vertices**, alle mit **Bytegewichtsumme exakt 255**. Bei **8.245** ist die Summe der einzeln als Float berechneten `byte*(1.f/255.f)` nicht bitgenau 1. Das ist normale Float-Rundung bei dennoch exakt normierter Bytequelle und kein Anlass zur Datenkorrektur. Nullsummen und falsche Bytegewichtssummen: **0** in diesen realen Stichproben.

NaN ist im UInt8-Gewichtformat nicht darstellbar; die Umrechnung mit dem endlichen Faktor 1/255 erzeugt daraus ebenfalls kein NaN. NaN/Infinity in Position, Normal, UV oder Bone-Matrix werden dagegen ausdrücklich geprüft. Synthetisch fehlerhafte Gewichte 0 und 254 werden abgewiesen und nicht geändert.

## 6. Influence Count

Maximal vier Lanes, getrennte Statistik für 0/1/2/3/4 positive Einflüsse. Die Real-Asset-Tests verlangen tatsächlich vorkommende Vertices aller vier gültigen Kategorien; nicht nur eine fixe Kriegerreferenz. Maximal beobachtet in B2: **4**, entsprechend B1.

Ein Haarmesh mit einem Einfluss wird nicht allein deshalb als rigid klassifiziert. Maßgeblich bleibt der vorhandene Granny-Meshpfad; etliche Hair-Dateien sind trotz eines Einflusses skinned.

## 7. Palette-Struktur

`BonePalette` hält `shared_ptr<const SkeletonLayout>`, `vector<SkinningMatrix>`, `revision` und `ready`. Es gibt keine native Granny-Matrixreferenz und kein Device-Handle. Die Palette enthält alle aktuellen Composite-Matrizen des tatsächlichen Pose-Besitzers, in Skeletonreihenfolge.

Auf ungültige Größe/Matrizen wird `ready=false`; eine alte fertige Palette darf nicht als aktuelle weiterverwendet werden. `GetSkinningPalette()` liefert nur bei gültigem Bindingstatus, fertiger Pose und übereinstimmendem Ziel-Skeleton eine Palette.

## 8. Remap-Struktur und Cache

`BoneRemap` hält die Ziel-Skeletonbeschreibung und die exakte Mesh→Skeleton-Tabelle aus `GrannyGetMeshBindingToBoneIndices` des **bereits bestehenden** Instanz-Bindings. Keine neue Animations-/Skinningberechnung, kein Erraten anhand Race oder BoneCount.

Der schwache Cache befindet sich beim geteilten statischen Mesh. Der Schlüssel besteht praktisch aus konkreter Skeleton-Generation plus vollständigem Mappingvergleich. Identische Actors teilen damit denselben Remap. Abgelaufene Einträge werden bei der nächsten Akquisition entfernt; der Cache hält keine Remaps oder Skeletons künstlich am Leben. Zugriff bleibt auf dem vorhandenen Modell-/Update-Thread; keine neue Threadingarchitektur.

`SkeletonLayout` kopiert Bone-Namen und Parentindices und besitzt eine monotone Identität. Dadurch bleiben keine dangling SDK-Pointer in eigenen Daten, und neu geladene Skeletons werden nicht aufgrund wiederverwendeter Adressen verwechselt.

## 9. Rüstungsremapping und Shapewechsel

Getestet: `warrior_novice → warrior_4-1 → warrior_nahan → warrior_novice`. Skeletongrößen **75 → 74 → 76 → 75**. Jede native `SetMainModelPointer`-Neubindung wählt die passenden eigenen statischen Daten und Remaps. Das alte Instanz-Palettenobjekt wird bei `Clear` freigegeben; das gecachte Asset bleibt nur solange sein Modellbesitzer lebt.

Der integrierte Test ruft die echten `CGrannyModel`-/`CGrannyModelInstance`-Methoden auf und vergleicht danach alle skinned Positionen, Normalen und UVs mit dem **unveränderten nativen CPU-Ausgabepuffer**. Kein zweiter Produktionsdeformer wurde eingebaut.

## 10. Haarremapping

Der in B1 numerisch belegte Fall ist explizit festgeschrieben:

| `warrior/hair/hair_1_1.gr2` | Ursprünglicher Source-Bone | Ziel-Bone `Bip01 Head` | Gegenüber Selbstbindung geänderte Einträge |
| --- | ---: | ---: | ---: |
| Localindex 5 → Novice | 66 | 67 | 72 |
| Localindex 5 → warrior_4-1 | 66 | 66 | 40 |
| Localindex 5 → warrior_nahan | 66 | 68 | 72 |
| Zurück → Novice | 66 | 67 | 72 |

Getestet wird jeweils Hair A → B → A, auch mit allen acht unten aufgeführten Player-Varianten. Haare besitzen eigene Bindings, verwenden aber **dieselbe aktuelle Palette wie der Körper**; keine Matrixkopie pro Haarinstanz. Gleichzeitige Haarinstanzen am selben Skeleton teilen auch ihre Remaptabelle.

Bei einem wechselnden verknüpften LOD-Pose-Besitzer wird die neue Zielgeneration erfasst, sofern Bone-Namen/-Reihenfolge und Parentstruktur äquivalent sind. Ein absichtlich inkompatibler Wechsel erzeugt `DestinationChanged`, keine stillschweigende Änderung des nativen CPU-Mappings. Zurückwechseln zum passenden Besitzer ist ebenfalls getestet. Eine solche Ablehnung betrifft nur die B2-Datenbasis; der Test deformiert den absichtlich unpassend gebundenen CPU-Fall nicht.

## 11. Player-Races und Geschlechter

Aktuelle reale Novice-Daten, jeweils größte Mesh-Bone-Anzahl des Modells. Palette ist Full-Skeleton, nicht die lokale Meshpalette:

| Race / Datenpfad | Skeleton-Bones | Mesh-Bones max. | Palette | Einflüsse max. | Nichtidentisches Remap |
| --- | ---: | ---: | ---: | ---: | --- |
| Krieger männlich, pc/warrior | 75 | 72 | 75 | 4 | ja |
| Ninja weiblich, pc/assassin | 79 | 75 | 79 | 4 | ja |
| Sura männlich, pc/sura | 90 | 88 | 90 | 3 | ja |
| Schamanin, pc/shaman | 94 | 90 | 94 | 4 | ja |
| Kriegerin, pc2/warrior | 84 | 82 | 84 | 4 | ja |
| Ninja männlich, pc2/assassin | 90 | 89 | 90 | 4 | ja |
| Sura weiblich, pc2/sura | 102 | 100 | 102 | 4 | ja |
| Schamane, pc2/shaman | 100 | 97 | 100 | 4 | ja |

Zusätzlich je zwei reale Haarvarianten, insgesamt 16 Hair-Assets. Hair-Skeletons sind nicht pauschal gleich dem jeweiligen Novice-Skeleton, etwa pc/shaman Hair A 87 Bones gegenüber Körper 94. Eigene Sourcewerte und konkrete Zielremaps bleiben getrennt.

## 12. NPCs

| NPC | Skeleton | Mesh-Bones max. | Einflüsse max. | Besonderheit |
| --- | ---: | ---: | ---: | --- |
| Doctor | 57 | 55 | 3 | gemeinsamer Datenpfad |
| Sinseon | 72 | 71 | 1 | skinned trotz eines Einflusses |
| Blacksmith | 48 | 43 | 4 | 1.180 deformable Vertices und zwei zusätzliche rigid Meshes |

Alle behalten ihre bisherigen Material-/Textur-/Blendpfade. B2 fasst keinerlei Materialzustand an.

## 13. Mobs und Bosse

| Asset | Skeleton | Mesh-Bones max. | Einflüsse max. | skinned Vertices |
| --- | ---: | ---: | ---: | ---: |
| Wolf | 40 | 38 | 3 | 762 |
| Orc soldier, humanoid | 68 | 64 | 3 | 1.450 |
| Fire dragon | 113 | 111 | 4 | 5.820 |
| Misterious diseased bosshost | 163 | 163 | 4 | 1.403 |
| Ent boss2 | 149 | 146 | 3 | 3.926 |

Damit sind insbesondere der B1-Maximalfall von **163 Bones** und große nichtidentische Remaps abgedeckt. Beim Orc bleibt der zusätzliche rigid Teil außerhalb des Skinningformats.

## 14. Mounts

| Mount | Skeleton | Mesh-Bones max. | Einflüsse max. |
| --- | ---: | ---: | ---: |
| Horse normal | 54 | 51 | 4 |
| Boar | 39 | 37 | 3 |
| Lion white | 50 | 48 | 4 |
| Horse Halloween 1 | 53 | 51 | 4 |
| Dinosaur 3 | 70 | 52 | 4 |

Alle durchlaufen denselben Extraktions-/Referenzpfad. Der native Instanztest hält Pferd und Reiterkörper gleichzeitig und prüft unterschiedliche Paletten. Keine Änderung an Saddle, Riding, Parentoffset oder Auf-/Absteigen.

## 15. Attachments

A) Rigid: bestehender Bone-/World-transformierter Pfad, kein `StaticSkinnedMeshData`. Reale Waffe `item/ymir work/item/weapon/00010.gr2`: ein Bone, ein rigid Mesh, keine Skinvertices/Palette notwendig.

B) Skinned: originale Weights/Indices plus eigener Mesh-Remap und vorhandener Pose-Besitzer, z.B. Haare. Weder Waffen noch Haare werden umklassifiziert, nur weil eine bestimmte Anzahl Einflüsse vorkommt.

## 16. Resource Ownership

| Ressource | Besitzer / Teilen | Freigabe |
| --- | --- | --- |
| SkinningModelData / StaticSkinnedMeshData | CGrannyModel; gleiche Modellinstanzen lesen dieselben Daten | Modell-Destroy/letzte shared Referenz |
| SkeletonLayout | unveränderliche Werte, starke Referenzen von Modell/Remap/Palette | letzte Referenz; keine SDK-Pointer |
| BoneRemap | Instanzreferenzen, geteilt bei identischem Asset/Zielskeleton/Mapping | Instanz-Clear/letzte Referenz; Cache nur weak |
| BonePalette | tatsächlicher Pose-Besitzer | Instanz-Clear/letzte Referenz |
| Hair-Palette | lesender Verweis auf aktuellen Körperbesitzer | keine eigene dauerhafte Hair-Palette |

`Clear` entfernt Remaps und Palette vor nativer Binding-/WorldPose-Freigabe und vor Pool-Wiederverwendung. Der native Wechseltest prüft dies einschließlich abgelaufener `weak_ptr`-Referenzen. Die Modellquelle selbst darf noch leben, solange das Asset im Cache benutzt wird.

## 17. Static/Dynamic Split

Statisch pro Asset/Mesh: Originalpositionen, Normalen, UVs, Byteindices/-weights, Indexbufferwerte, ursprüngliche Materialgruppen und Source-Skeleton-Mapping. Statisch pro **Asset + konkreter Bindingvariante**: Destination-Remap, geteilt statt pro Actor dupliziert.

Dynamisch pro Pose-Besitzer: Composite-Matrizen und Revision. Der Actor-Worldtransform und Animationszustand bleiben ausschließlich in ihren bisherigen Klassen; B2 erzeugt keine unnötigen Kopien davon. Materialoverrides bleiben in der bisherigen Instanz-Materialpalette. Kein statischer Skinvertex wird pro Frame oder pro Actor kopiert.

## 18. Matrixkonvention

Unverändert aus B1: Row-Vektoren, Translation in Matrixzeile 4, Composite = `InverseWorld4x4 * JointWorldPose`. Das Composite aus der **tatsächlich gesampelten WorldPose** wird kopiert; kein eigener Restpose-/InverseBind-Nachbau.

Position: Summe `weight/255 * (position,1) * composite[remap[localIndex]]`.

Normal: Summe `weight/255 * (normal,0) * composite[remap[localIndex]]`. Keine zusätzliche Normalisierung und kein boneweises Inverse-Transpose. Der existierende spätere World-/View-Normalpfad bleibt unberührt.

Parentoffset, z.B. Reiter/Mount, ist bereits in der vorhandenen Pose enthalten. ActorWorld kommt im unveränderten Renderpfad danach; kein doppeltes Anwenden. Die Tests verwenden zusätzlich einen nichtuniform skalierten und verschobenen Parentoffset, damit Identitätsmatrizen keine Reihenfolgefehler verdecken.

## 19. CPU-Bone-Matrixstruktur

`SkinningMatrix = std::array<float,16>`, 64 Byte; natürliche CPU-Ausrichtung 4 Byte. Der Adapter kopiert über `memcpy` aus dem SDK-Array in eigene Werte, prüft die endlichen Komponenten und publiziert erst danach eine fertige Palette. Keine aliasenden `reinterpret_cast`-Zugriffe auf ein fremdes `std::array`-Objekt.

Erfassung nach jedem bereits vorhandenen `GrannySampleModelAnimationsAccelerated` beim Pose-Besitzer. Kein zusätzliches Sampling. Hair kann auf diese Palette zugreifen, ohne nochmals zu sampeln oder die Matrixliste zu kopieren. Eine spätere GPU-Uploadgrenze muss Matrixmajorität ausdrücklich definieren; **B2 transponiert nichts**.

## 20. GPU-Bufferempfehlung für einen späteren Auftrag

**Empfehlung: separater dynamischer Uniform/Constant Buffer für den ersten B3-Prototyp**, noch nicht implementiert. Die bestehende Diligent-Integration nutzt bereits `USAGE_DYNAMIC`, `BIND_UNIFORM_BUFFER`, CPU-Write und `MAP_FLAG_DISCARD` für andere Konstanten; kein Grund für einen neuen Buffertyp nur für diese Größenordnung.

- 163 × 64 = **10.432 Byte = 10,1875 KiB**; die grobe Formulierung „10,4 KiB“ vermischt hier dezimale KB mit KiB.
- 256 × 64 = **16.384 Byte = 16 KiB**, ausreichend Reserve oberhalb der beobachteten 163 Bones. Die 256 sind eine explizite Vorbereitungsgrenze, keine dauerhafte GR2-Grenze.
- D3D11: 4.096 Konstantenelemente à 16 Byte, also **64 KiB** pro shaderadressierbarem CB-Bereich. ByteWidth muss ein Vielfaches von 16 sein. Der lokal verwendete Diligent-Commit `b036337d` richtet Uniformbuffer in `BufferD3D11Impl.cpp` bereits auf 16 Byte aus. [Microsoft Bufferbeschreibung](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_buffer_desc), [Microsoft Functional Specification](https://microsoft.github.io/DirectX-Specs/d3d/archive/D3D11_3_FunctionalSpec.htm).
- Vulkan-Portabilität nur bewertet: Vulkan-Core garantiert mindestens 16.384 Byte `maxUniformBufferRange`; konkrete Limits und `minUniformBufferOffsetAlignment` müssten beim späteren Backend abgefragt werden. Eine reine 256-Matrix-Palette passt in diese Basisgrenze; Actor-/Framekonstanten daher separat planen. **Kein Vulkan-Code oder -Build ergänzt.** [Khronos Limits](https://docs.vulkan.org/spec/latest/chapters/limits.html), [Descriptor-Alignment](https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html).
- Update später pro tatsächlich gesampeltem Pose-Besitzer/Revision, nicht je Materialdraw; Body/Hair teilen die gleiche Pose. Mount bleibt separat. B2 legt noch keinen GPU-Buffer an.
- 100 Actors × 16 KiB wären ca. 1,56 MiB pro vollständiger Palettenrunde, bei 60 Runden/s ca. 93,75 MiB/s reine Nutzdaten. Das ist **eine Rechnung, kein Benchmark**. Treiber-/Mapkosten und GPU-Lifetime müssen B3/spätere Performancephasen messen; Pooling/Ringbuffer nicht vorschnell vorwegnehmen.

## 21. Reference-Skinning-Test

[SkinningDataTest.cpp](<C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/tests/Renderer/SkinningDataTest.cpp>) lädt echte GR2-Dateien aus `M2_SKINNING_ASSET_ROOT`, erzeugt die Produktions-B2-Strukturen und testet alle vorhandenen skinned Vertices. Fehlende Real-Assets führen zum Fehler, nicht zu einem stillen Skip.

41 unterschiedliche GR2-Modelldateien / 43 ausgewertete Modelleinträge inklusive leerer Multi-Model-Einträge; außerdem originale Wait/Run/Attack-Dateien der Player. Insgesamt **390 Pose-/Binding-/Parentfälle** und **453.354 Vertexvergleiche**. Pro Fall Zeit 0 / 0,37 / 0,83 und Parent ohne/mit nichtuniformer Skalierung/Translation. Bei Nicht-Player-Stichproben wird die gesampelte vorhandene Restpose geprüft, nicht behauptet, jede Animation getestet zu haben.

Die unabhängige skalare Testformel liest nur neue B2-Vertices, Remap und Palette. Referenz A ist die unveränderte Funktion `DeformPWNT3432toGrannyPNGBT33332` aus `Deform.cpp`; Referenz B der SDK-Deformer. **Nur im Test** werden beide zusätzlich aufgerufen.

[SkinningInstanceTest.cpp](<C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/tests/Renderer/SkinningInstanceTest.cpp>) ergänzt echte Modell-/Instanz-Lifecycles, bestehendes CPU-Deform-VB, zwölf kombinierte Shape/Hair-Phasen, Clone-Sharing, Mounttrennung und LOD-Besitzerwechsel. Dort sind Imagefactories absichtlich nicht registriert: es ist ein CPU-Geometrie-/Ownershiptest, kein Texturtest. Die entsprechend erwarteten Texture-Factory-Diagnosen sind keine Produktionsregression; Texturen prüft der separate Welttest mit Originalpaketen.

## 22. Positionsparität

Maximaler absoluter Komponentenfehler B2-Skalarformel gegenüber bestehendem SSE-Deformer: **0**. Gegenüber Granny-SDK: **0,000488281**, innerhalb der unten definierten positionsabhängigen Toleranz. Keine Anforderung erzwungener Bitidentität für eine spätere GPU-Implementierung.

## 23. Normalparität

Maximaler absoluter Komponentenfehler gegenüber SSE: **0**. Gegenüber Granny-SDK: **3,57628e-7**. UVs und originale Sourceattribute werden zusätzlich bytegenau verglichen. Alle Vertices schließen ersten/mittleren/letzten Vertex sowie vorhandene 1-/2-/3-/4-Einflussfälle ein.

## 24. Numerische Toleranz

Pro Positionskomponente: `abs(actual-reference) <= 1e-4 + 2e-6 * max(1, abs(reference))`.

Pro Normalkomponente: absolute Toleranz **1e-5**. Alle Vergleichswerte müssen endlich sein. Dies lässt kleine unterschiedliche Float-Operationsreihenfolgen zu, ohne relevante Geometrie-/Normalfehler zu verdecken. Die realen gemessenen Maxima werden zusätzlich ausgegeben; ein passendes EPS ersetzt keine Fehlerstatistik.

## 25. Asset-Sonderfälle

- Zwei leere Ein-Bone-Modelle innerhalb `redthief2_soldier2_lod_01.gr2`: Empty, kein Skin-Datenobjekt und kein Zugriff auf fehlenden VertexType. Das nichtleere mittlere Modell derselben Datei wird separat geprüft: 60 Bones, 2.488 skinned Vertices plus zwei rigid Meshes.
- Gemischte rigid/skinned Modelle, z.B. Blacksmith und Orc, behalten getrennte Meshslots und den bestehenden deform-VB-Offset.
- Ein Einfluss bedeutet nicht zwingend rigid, u.a. Sinseon und viele Haare.
- Abweichende Source-/Ziel-Skeletons werden nicht vereinheitlicht; insbesondere Armor/Hair bleibt bindingabhängig.
- B1s vollständige Bestandszahlen 9.166 Dateien/3.069.950 skinned Vertexvorkommen sind **B1-Evidenz**, kein erneut behaupteter B2-Vollscan. B2 ergänzt reale repräsentative Validierung, nicht nochmals den gesamten Scan.

## 26. Invalid-Data-Diagnostics

Eigene explizite Statuswerte für UnsupportedLayout, InvalidSkeleton, PaletteTooLarge, MissingData, NonFiniteAttribute, ZeroWeights, WeightSum, InvalidBoneIndex, InvalidTopology, InvalidRemap, DestinationChanged und InvalidMatrix. Rigid/Empty sind reguläre Kategorien, keine Fehler.

Negative/out-of-range Zielremaps, falsche Tabellenlänge, fehlende Pose, invalidierte Matrizen, fehlerhafte Gewichte/aktive Indices und Kapazitätsüberschreitung sind Tests. Zusätzlich lokale Kopien realer GR2-Strukturen mit fehlendem Vertexlayout/-speicher, fehlender Topologie und negativer Materialrange. Die Dateien und ursprünglichen geladenen Werte werden dabei nicht verändert.

Ungültige Vertexindices in **gewichtslosen** Lanes werden gezählt und nicht dereferenziert, entsprechend CPU-Semantik. Es gibt keine stillen Datenkorrekturen. Fehlerhafte B2-Daten werden nicht als bereit publiziert, die vorhandene CPU-Produktion bleibt davon unabhängig. Dies ist kein vollständiger Sicherheitsparser für beliebig beschädigte GR2-Dateien vor dem bestehenden Granny-Ladevorgang.

Produktionsdiagnosen sind auf insgesamt 16 Einzelmeldungen begrenzt; Instanzprobleme werden zusätzlich pro Instanz-Lifecycle nur einmal gemeldet. Eine zusammengefasste Shutdown-Zeile enthält lebende SkinMeshes/Remaps/Paletten, PaletteUpdates und Fehleranzahl. Keine Per-Frame-/Per-Vertex-Logflut.

Der negative native LOD-Test erwartet genau **eine** `DestinationChanged`-Diagnose. Der reguläre Welttest erwartet dagegen **0** SkinPreparationFailures.

## 27. Release-Build

Produktiver Client und sämtliche Release-Testziele erfolgreich gebaut, Exitcode 0. Finaler Produktionsbuild: `build/phase-b2-release-final.log`; ergänzter Real-Asset-Test: `build/phase-b2-data-build-final.log`.

Vorhandene Linkerwarnungen der vorgebauten Python-/zlib-Libraries zu fehlenden PDB-Dateien bleiben bestehen. Keine neue B2-Compilerwarnung oder ignorierter B2-Buildfehler. `Deform.cpp` unverändert, SHA-256 `C73674782ADA9D554237F1522605ABA3BFCBCBFF66FAB14E2FD34545B1FC9DE7`.

## 28. Debug-Build

Produktiver Debug-Client und alle Debug-Testziele erfolgreich gebaut, Exitcode 0: `build/phase-b2-debug-final.log`. Bestehende Warnungen zu vorgebauten Libraries/LTCG/CRT sind dokumentiert, nicht mit unangeforderten Buildsystemänderungen unterdrückt.

## 29. Testsuite

Finaler Renderer-Satz inkl. beider B2-Tests: **Release 17/17**, **Debug 17/17**, jeweils Exitcode 0. Enthält die gesamten bestehenden Renderer-/GPU-/Policy-/Source-/Text-/UI-/NoLegacy-Prüfungen sowie `Renderer.SkinningData` und `Renderer.SkinningInstance`.

Die vollständige bestehende Suite wurde ebenfalls ausgeführt. Erster Gesamtlauf **19/20**, 768,44 s: fullbench 145,27 s, fuzzer 501,72 s, zstreamtest 111,20 s und alle Renderer-/B2-Datentests bestanden. Nur der unveränderte Vendor-`playTests` scheiterte zunächst an Windows `NUL: Permission denied`; außerhalb der beschränkten Dateiumgebung zeigte sich anschließend die bereits im Projekt dokumentierte Cygwin/native-Windows-Pfadinkompatibilität (`/cygdrive/c/...`).

Derselbe unveränderte CTest-Fall mit **Git-for-Windows `usr/bin` vor Cygwin ausschließlich im Testprozess-PATH**: **1/1 bestanden, 24,02 s, Exitcode 0**. Keine globale Umgebungsänderung, kein Vendor-Patch und kein gekürzter Test. Damit wurden **alle 19 bestehenden Tests und beide neuen B2-Tests erfolgreich ausgeführt**. Kein falsches „erster Gesamtlauf 21/21“: Der Gesamtlauf wurde vor Registrierung des Instanztests gestartet; der finale Satz aus 17 Renderer-/B2-Tests wurde danach separat vollständig in Release und Debug geprüft.

Reproduktion vom Source-Root:

```powershell
& 'C:/Program Files/CMake/bin/cmake.exe' --build build --config Release --parallel 6
& 'C:/Program Files/CMake/bin/cmake.exe' --build build --config Debug --parallel 6
& 'C:/Program Files/CMake/bin/cmake.exe' -E env "PATH=C:\Program Files\Git\usr\bin;$env:PATH" 'C:/Program Files/CMake/bin/ctest.exe' --test-dir build -C Release --output-on-failure
& 'C:/Program Files/CMake/bin/ctest.exe' --test-dir build -C Debug -R '^Renderer\.' --output-on-failure
```

Der Assetpfad ist als `M2_SKINNING_ASSET_ROOT` konfigurierbar und zeigt hier auf `C:/Users/ZiiNAN/Documents/GitHub/m2dev-client/assets`. Keine Netzwerk-/Serverabhängigkeit der B2-Datentests.

## 30. Phase-A-/Client-Regression

| Prüfung | Ergebnis |
| --- | --- |
| Finaler Release-Standardstart/Bootstrap, Resize, Shutdown | PASS, PID 54828, Exit 0 |
| Finaler Debug-Standardstart/Bootstrap, Resize, Shutdown | PASS, PID 65388, Exit 0 |
| Automatischer vollständiger Release-Welttest | PASS, PID 67392, sechs Phasen, 151,2 s, Exit 0 |
| B2-Zähler nach diesem Welttest | SkinMeshes 0, BoneRemaps 0, BonePalettes 0, PaletteUpdates 39.045, SkinPreparationFailures 0 |
| Bisherige GPU-/CPU-Source-Ressourcen nach Welttest | alle protokollierten Klassen 0, einschließlich Actor-/Attachment-/Mount-Grafikressourcen und SourceTextures/SourceBuffers |
| Sichtbarer Wiederholungstest | PASS, PID 56760, sechs Phasen, 151,2 s, Exit 0; dreimal Minimize/Restore und korrekte Darstellung vom Nutzer bestätigt |
| B2-Zähler nach sichtbarem Lauf | SkinMeshes 0, BoneRemaps 0, BonePalettes 0, PaletteUpdates 44.555, SkinPreparationFailures 0; bisherige Grafik-/Source-Owner ebenfalls 0 |

Die Welttests verwenden die vorhandene unveränderte `special_world_entry.py`-Fixture in einer privaten Runtime: A1 → B1 → A1 → Dungeon → Gildenkarte → A1, wechselnde Kamera, Terrain, Wasser, Bäume, Gebäudepfad, sechs Actor-Kategorien/-Modelle, Waffen/Haar, Minimap/Atlas und Schrift. Der erste Lauf wurde versehentlich mit verborgenem Fenster gestartet, lieferte dennoch automatische Screenshots/Render- und Lifetime-Evidenz. Für die manuelle Fensterprüfung wurde deshalb ausdrücklich ein **zweiter sichtbarer Lauf** gestartet, nicht die unsichtbare Prüfung als bestätigt ausgegeben.

Die aktuelle A1-Aufnahme zeigt Figuren/NPC/Mob/Metin/Pferd einschließlich Körper und Attachments zusammen mit Wasser, Terrain, UI-Schrift und Minimap. Die Daten-/CPU-Ausgabeparität plus unangetasteter Drawpfad und diese vorhandene Phase-A-Regressionsfixture sind für B2 die relevante Bildabsicherung. **Kein streng zeitsynchroner CPU/GPU-Pixelvergleich und kein neuer Live-Server-Login behauptet.** GPU-Skinning existiert noch nicht; ein zusätzlicher Login-/Kampftest würde hier keine Skinningpfad-Umschaltung prüfen.

Vorhandene GPU-Tests prüfen wiederholtes Resize, minimierte Flächen und Restore. Die manuelle Weltprüfung bestätigt dreimal Minimize/Restore. Keine Server-/Account-/Originalpaketänderung, keine fremden Clients beendet.

Je Welttest 31 Ressourcenproben: private Bytes im ersten Lauf maximal 444,4 MiB, letzte Probe 358,8 MiB; im sichtbaren Lauf maximal 420,5 MiB, letzte Probe 355,4 MiB. Wechselnde Karten erzeugen Cache-/Allokationsspitzen, keinen beobachteten monotonen Anstieg. Das ist eine begrenzte zweifache 151-Sekunden-Regression, kein Langzeit-Leak-/Performancebenchmark. Entscheidend zusätzlich: alle gezählten eigenen CPU-/GPU-Owner nach Shutdown 0, keine gemeldeten B2-Vorbereitungsfehler.

## 31. Git-Diff und Grenzen

Sieben bestehende Dateien mit insgesamt **54 ergänzten Zeilen**, keine gelöschten Bestandszeilen. Sechs neue Code-/Testdateien: SkinningData.h, SkinningDataAdapter.h/.cpp, ModelInstanceSkinning.cpp, SkinningDataTest.cpp, SkinningInstanceTest.cpp. Zusätzlich dieser Bericht. Der bereits vorhandene untracked B1-Bericht wurde nicht geändert.

Unverändert: `Deform.cpp`, `Mesh.cpp`, Granny-SDK/Library, ActorRenderBridge, DiligentActorRenderer, sämtliche Shader/PSOs/Draws, Materials, Terrain, Effekte, UI, Mount-/Attachmentlogik. Source-Release bleibt Diligent D3D11-only. Es gibt in dieser Phase keinen Legacy-/OFF-Build mehr.

Temporäre Logs/Fixture-Runtimes und der Starthelfer liegen im ignorierten `build/phase-b2`; die privaten Exekopien ändern nicht den normalen Client. Bereits bestehendes Runtime-`config/channel.inf` blieb unangetastet. Kein Commit, Push, Reset, Restore, Clean oder Stash.

## 32. GO / NO-GO für B3 und STOP

**GO für einen separat beauftragten, eng begrenzten B3-Prototyp. B2 ist abgeschlossen.** Reale Originaldaten, explizite Source-/Destination-Mappings, kleine vollständige Paletten, Positions-/Normalparität, Release/Debug, die bestehenden Tests und Freigabe sind nachgewiesen.

Kein GO zur generellen Produktionsumschaltung. B3 muss einen gesonderten GPU-Input-/Matrixlayoutvertrag, echte GPU-Positions-/Normalparität und Draw-/Lifetimeprüfungen erst implementieren und bestehen. Unsupported-/überkapazitäre Daten oder inkompatible Bindings brauchen weiterhin den bestehenden CPU-Pfad. Sampling/Granny/Parent- und Materialsemantik dürfen nicht nebenbei verändert werden.

**B2 enthält keinen GPU-Skinning-Shader und keinen produktiven GPU-Skinningpfad. STOP; B3 wurde nicht begonnen.**

## Abschlussprotokoll / lokale Evidenz

Die folgenden Logs gehören zu diesem Lauf, nicht zu früheren Phase-A-Abnahmen:

- `build/phase-b2-data-release-final.log`: alle Real-Asset-Zeilen, exakte B1-Hairassertions, Fehlermaxima, Ressourcen 0.
- `build/phase-b2-data-debug-final.log`: dieselben Positions-/Normalmaxima und Ressourcen 0 im Debug-Test.
- `build/phase-b2-instance-release-final.log`: zwölf native Shape/Hair-Phasen; ein absichtlich abgelehnter LOD-Fall; Ressourcen 0.
- `build/phase-b2-ctest-release-renderer.log` und `build/phase-b2-ctest-debug-renderer.log`: je 17/17.
- `build/phase-b2-ctest-full-release.log`: Details des ersten Gesamtlaufs einschließlich fehlgeschlagenem Shelltest; `build/phase-b2-ctest-playTests-retry.log`: Cygwin-Pfadfehler; `build/phase-b2-ctest-playTests-git-shell.log`: erfolgreicher unveränderter Wiederholungstest.
- `build/phase-b2/startup-release-final` und `startup-debug-final`: Start-/Resize-/Shutdownlogs und Exitcodes.
- `build/phase-b2/world-release-final`: sechs Screenshots, Phasenlog, Ressourcenverlauf, Shader-/Rendererlogs, Exit-/Ownershipprotokoll.
- `build/phase-b2/world-visible-final`: separater sichtbarer Abnahmelauf; Nutzerbestätigung dreimal Minimize/Restore am 13.09.2026.

Alle von diesem Auftrag gestarteten Testclients sind beendet. Keine verbleibende Testprozess-/Login-Freigabe nötig. Die Renderer-/Deformer-Produktionspfade bleiben unverändert CPU-skinned.
