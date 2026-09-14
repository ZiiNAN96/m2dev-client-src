# E1-X – Native GLB Asset Provider + Modern Mesh Pipeline

Stand: 14.09.2026. **E1-X GO: automatische Abnahme PASS, positive manuelle Gesamtrückmeldung und regulärer Shutdown bestätigt.**
Nur E1-X. Kein Commit/Push, E2-X, Phase F, Granny-Ersatz, PBR oder weitere Android-Portierung.

## 1. D1-X Baseline

Sauberer Source-Checkout auf `8756115 refactor(assets): introduce ZiiNAN asset runtime` vor Änderungen.
Vorhandene Client-Änderungen (`config/channel.inf` und Laufzeitlogs) werden nicht verändert.
Vorab Fast Gate: Release 20/20 (9,77 s), Debug 20/20 (11,08 s), bestehendes GCC/LP64 6/6 (0,20 s).
Dies sind Tests der vorhandenen D1-X-Binaries; E1-X-Neubau und Ergebnisse stehen separat unten.

## 2. Loader Evaluation

| Kriterium | cgltf 1.15 | fastgltf 0.9 |
|---|---|---|
| C++ Integration | C99 Single Header, direkt in C++ nutzbar | typisierte C++17-API |
| GLB / glTF / Buffer / Accessor | unterstützt | unterstützt |
| Sparse / normalized / stride | Metadaten und teilweise sparse-fähige Lesehilfen; E1-X verwendet eigene geprüfte Konvertierung | typisierte Accessor-Hilfen einschließlich sparse |
| Nodes / Skins / Animation | strukturelle Daten | strukturelle Daten |
| Materialien / Extensions | Metadaten, erforderliche Extensions separat prüfen | Metadaten, aktivierbare Extensions |
| Dependencies / Lizenz | keine externen; eingebettetes jsmn; MIT | simdjson; fastgltf MIT, simdjson Apache-2.0 |
| Windows / GCC / Android | C99 ohne Plattform-API; Windows/GCC-Nachweise unten, Android hier nicht gebaut | MSVC/GCC/Clang; eigene Android-Asset-Hilfen |
| E1-X-Abwägung | minimale Integration, explizite private Validierung | bessere C++-Hilfen, zusätzliche Dependency ohne benötigten E1-Nutzen |

Primärquellen: [cgltf](https://github.com/jkuhlmann/cgltf),
[fastgltf Überblick](https://fastgltf.readthedocs.io/v0.9.x/overview.html),
[fastgltf Accessor tools](https://fastgltf.readthedocs.io/v0.9.x/tools.html).
Die Parser liefern Bildverweise und Bildbytes; PNG/JPEG decodiert weiterhin die bestehende Client-Library.
Bei cgltf unterstützen `accessor_read_float`, `accessor_read_uint` und `accessor_unpack_indices`
keine sparse-Daten; `accessor_unpack_floats` unterstützt sie. Der Provider benutzt für normale und
sparse Daten eigene geprüfte `memcpy`-Reads, insbesondere ohne UInt32-Indices über Float zu führen.
[Gepinnte API/Implementierung](https://raw.githubusercontent.com/jkuhlmann/cgltf/v1.15/cgltf.h),
[fastgltf Lizenzhinweise](https://github.com/spnda/fastgltf#license).

## 3. Gewählter Loader

Genau ein Runtime-Loader: cgltf v1.15, Commit `360db1a95480fe102ae9c69b27c5d101167ff5ba`, MIT.
Vendored Quelltext und Lizenz unter `vendor/cgltf`; keine Internetabhängigkeit beim Build/Test.
`CgltfImplementation.cpp` ist die einzige Implementierungseinheit. Numerische Parser-Hooks verwenden
`std::from_chars`; die private JSON-Vorprüfung fängt falsche Typen/Enums ab, die cgltf sonst teilweise
auf Defaults abbildet. cgltf bleibt der einzige glTF-Strukturparser; Assimp ist nicht integriert.
[Release](https://github.com/jkuhlmann/cgltf/releases/tag/v1.15),
[geprüfter Commit](https://github.com/jkuhlmann/cgltf/commit/360db1a95480fe102ae9c69b27c5d101167ff5ba).

## 4. GlTFProvider Architecture

```text
ResourceManager / CGraphicThing
       -> AssetRuntime::LoadModel(id, bytes)
          |-- GrannyAssetProvider -> GR2
          `-- GlTFAssetProvider   -> GLB
                 -> ModelAsset / MeshAsset / MaterialAsset
                 -> CGrannyModel / CGrannyMesh als neutrale Consumer-Fassaden
                 -> StaticObjectSource oder vorhandener ActorModelSource-Spezialpass
                 -> DiligentStaticObjectRenderer / DiligentActorRenderer
```

Loader-Typen bleiben privat unter `src/AssetRuntime/GlTF`. Öffentliche Ergänzungen beschreiben
Bildbytes, Materialzustände, importierte Skin-/Animationsmetadaten und Renderbarkeit ohne Formattypen.

## 5. Provider Selection

`AssetRuntime/Providers.cpp` wählt anhand der Endung exakt einen Provider. `gr2` bleibt Granny,
`glb` verwendet cgltf. Groß-/Kleinschreibung wird normalisiert. Ein Fehler löst keinen zweiten Provider aus.
Die Resource-Registrierung verwendet `ModelExtensions()`; Game/World/Renderer prüfen keine GLB-Endung.
E1-X aktiviert nur GLB. `.gltf` mit externen Ressourcen ist ausdrücklich nicht Teil dieses Blocks.

## 6. GLB Container

Version-2-Container mit genau JSON gefolgt von BIN, einem eingebetteten Buffer und `asset.version=2.0`.
Zusätzliche Chunks, externe Buffer und Data-URIs werden abgelehnt. Magic, tatsächliche Gesamtlänge,
vierfach ausgerichtete Chunk-Längen und BIN-Padding bis drei Bytes werden geprüft. Keine Extraktionsdateien.
Vor cgltf folgen JSON-Syntax-/Typ-/Enumprüfung und Grenzen für Tiefe/Tokenzahl. Doppelte Keys,
gebrochene Strings und nichtintegrale beziehungsweise negative Index-/Größenfelder werden zurückgewiesen.

## 7. Buffers/Accessors

BufferViews, `byteOffset`, `byteStride`, Typ, count, Komponentenausrichtung und Range werden vor Reads geprüft.
Ein gesetzter BufferView-Stride muss 4–252 Bytes und durch vier teilbar sein; Attributdaten benötigen
vierfache Elementausrichtung. `normalized` ist für Float32/UInt32 ungültig. Fehlender Basis-BufferView
wird mit Nullwerten initialisiert; sparse überschreibt ausgewählte Werte.

Sparse-Indizes müssen unsigned, strikt steigend, eindeutig und innerhalb von count sein. Sparse-Views
dürfen keinen Stride haben; sparse Werte bleiben dicht gepackt, auch bei interleaved Basisdaten.
Dies gilt auch für sparse Dreiecksindices. Alle Reads verwenden `memcpy`; E1-X setzt little-endian voraus.
Die eigenen Bereichsprüfungen ersetzen hier `cgltf_validate` und die ungeprüften Accessor-Hilfen.

## 8. Vertex Attributes

POSITION/VEC3 ist Pflicht. NORMAL/VEC3 und TEXCOORD_0/VEC2 ergeben zusammen das vorhandene 32-Byte-PNT-Layout.
POSITION/NORMAL/TANGENT verwenden ohne Quantisierung Float32; UV0 zusätzlich normalized UInt8/UInt16.
`KHR_mesh_quantization` erlaubt die passenden 8-/16-Bit-Varianten: Positionsdaten signed/unsigned,
Normalen/Tangenten nur normalized signed, UV auch quantisierte signed/unsigned Komponenten.
UInt32-POSITION bleibt ungültig. Attributanzahlen müssen mit POSITION übereinstimmen.

Fehlende Normalen werden aus Dreiecken akkumuliert/normalisiert; unbenutzte beziehungsweise entartete
Vertices erhalten als definierte Rückfallnormale +Z. Texturierte Primitives benötigen UV0.
Bei untexturierten Primitives ohne Quell-UV werden (0,0)-Werte ausgegeben; das konvertierte PNT
kennzeichnet UV0 entsprechend als vorhandenen Ausgabekanal für den normalen Consumer.
TANGENT/VEC4 wird als normalisierte, orthogonalisierte Metadaten mit korrigiertem w-Vorzeichen erhalten;
der Renderer benutzt keinen Tangentenkanal. COLOR_0 wird ausdrücklich abgelehnt. Weitere UV-Sets und
Custom-Attribute werden nicht gerendert; BaseColor-Texturen dürfen ausschließlich TEXCOORD_0 verwenden.
JOINTS_0/WEIGHTS_0 sind getrennte Importmetadaten, siehe 25/26.

## 9. Index Formats

UInt8/UInt16/UInt32-SCALAR werden als Indices gelesen, einschließlich sparse; signed/normalized oder
strided Indexdaten werden abgelehnt. Ohne Indexaccessor werden sequenzielle Indices erzeugt.
Der größte gelesene Index entscheidet über UInt16/UInt32 in MeshAsset, nicht allein der Quelltyp.
Die neutrale Mesh-Indexbreite entscheidet anschließend über den Upload.
Große rigide Meshes verwenden UInt32 bis zum Diligent-Draw; bestehende GR2-/GPU-Skin-Streams bleiben UInt16.
Beide Renderer-Sichten validieren lokale Indexbereiche und BaseVertex ohne Abschneiden.

## 10. Primitive Modes

TRIANGLES ist der E1-X-Produktionspfad. Andere Modi werden ausdrücklich abgelehnt.
Morph Targets werden nicht still als korrekte statische Geometrie behandelt.

## 11. Coordinate Conversion

glTF ist rechtshändig, +Y oben, +Z Asset-Vorderseite. Der Client verwendet rechtshändige
Renderkoordinaten mit +Z oben; die Actor-Vorderseite bei Rotation 0 liegt auf -Y.
Zentrale Abbildung: `p_client = 100 * (p_gltf.x, -p_gltf.z, p_gltf.y)`.
Die Achsenrotation hat Determinante +1. glTFs column-major-Matrixbytes entsprechen beim Kopieren in
das vorhandene row-major-Layout der mathematischen Transposition. Der Provider verwendet Zeilenvektoren:

```text
C                  = Skalierung 100 und Rotation (x,y,z) -> (x,-z,y)
world_row          = local_row * parent_world_row
p_baked            = p_gltf_row * world_row * C
M_native           = inverse(C) * M_gltf_row * C
```

Der letzte Ausdruck gilt für lokale Joint-/Inverse-Bind-Metadaten. Actor-/Map-World folgt erst beim
normalen Draw. Es gibt keine zweite Achsenkonversion in Attachments oder Renderer.

Codebelege: `src/EterLib/Camera.cpp:344,537` (RH, Up Z), `src/GameLib/ActorInstanceRender.cpp:105–115`
und `ActorInstanceBattle.cpp:945–947` (Vorderseite -Y), `src/Renderer/DiligentStaticObjectRenderer.cpp`
(row_major und `mul(position, matrix)`). Die vorhandene GR2-ArtTool-Prüfung in
`docs/renderer/phase-b1-gpu-skinning-analysis.md:598–603` belegt Right +X / Up +Z / Back -Y und 100 Einheiten/m.
Das ArtTool-Feld Back wird nicht mit der durch Actor-Code bestimmten Vorderseite gleichgesetzt.
[glTF Koordinaten und Einheiten](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#coordinate-system-and-units).

## 12. Unit Scale

100 historische Welt-Einheiten pro Meter als zentrale Providerkonstante. Das eigene Fixture
ist 2,8 m breit und etwa 2,4 m hoch. GR2-Daten und Weltpositionen werden nicht skaliert.

## 13. Winding/Culling

Positive Node-Determinante erhält die glTF-Dreiecksreihenfolge; negative Determinante tauscht
Indices 1/2 je Dreieck genau einmal beim Backen. Der vorhandene CullCw-/Diligent-Vertrag bleibt:
Backface-Culling und `FrontCounterClockwise=true`. Singuläre Mesh-Transforms werden abgelehnt.
Double-sided ist eine Materialeigenschaft, keine globale Korrektur des Renderers.
[glTF Instantiation/Winding](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#instantiation).

## 14. Normals/Tangents

Normalen verwenden `normalize(n * inverse(transpose(linear(world_row*C))))`.
Tangenten verwenden die lineare Transformation, anschließend Gram-Schmidt gegen die konvertierte Normale
und Normalisierung; ihr w muss +/-1 sein und wird bei Spiegelung negiert. Fehlende Tangenten werden nicht
erzeugt. Nichtuniforme Skalierung ist damit berücksichtigt; PBR/Normalmapping bleibt ausgeschlossen.

## 15. UV

UV0 wird ohne V-Flip übernommen. glTF-Bildursprung und der vorhandene PNG/JPEG-Decoder
passen zum bestehenden UV-Sampling. Das asymmetrische rot/grün/blau/gelbe Fixture macht
Orientierungsfehler sichtbar; kein globaler Texture-Flip.
`KHR_texture_transform` wird für die BaseColor-UV0 einmal eingebrannt: Scale, Rotation, Offset;
ein texCoord-Override auf ein anderes Set wird abgelehnt. Tangenten sind derzeit reine Metadaten,
keine Zusage einer späteren Normalmap-Auswertung mit transformierten UVs.
[glTF Bild-/UV-Ursprung](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#images).

## 16. Scene/Node Graph

Default Scene; falls sie fehlt, erste Scene, und ohne Scenes alle Root-Nodes. Die aktive Auswahl muss
mindestens ein Mesh-Primitive enthalten. Mehrere Nodes und wiederverwendete Meshes werden unterstützt:
jedes ausgewählte Node-/Primitive-Paar wird einmal in ein eigenes gebackenes MeshAsset übersetzt.
Dies ist kein neues GPU-Instancing. Zyklen, Mehrfachparents, doppelte Children/Scene-Nodes und
Scene-Roots mit Parent werden abgewehrt; Hierarchietiefe maximal 256.

## 17. Node Transforms

Matrix oder TRS, niemals beide zugleich; Quaternionen müssen normiert, Transformwerte endlich und
Matrizen affin sein. Lokales TRS stammt aus cgltf; die Parent-Auswertung ist eigene iterative,
zyklusgesicherte Logik. World-Transformationen werden einmal in Vertices/Normals gebacken.
Keine Node-Evaluation pro Draw, keine dynamische Node-Animation.

## 18. Rigid Mesh Pipeline

`CGraphicThing -> ModelAsset -> CGrannyModel::CreateFromAsset -> CGrannyMesh ->`
vorhandene Buffer und Static-/Actor-Sources. Die historischen Klassennamen sind Kompatibilitätsfassaden.
Kein cgltf-Renderpfad und keine Loader-Typen in den Draw-APIs.

## 19. Static World Integration

Bestehender StaticObjectLoadScope und StaticObjectSource. Der gezielte native Runtime-Test
verwendet die normale Resource-Registrierung, Instanz, Materialien und DiligentStaticObjectRenderer.
Produktive Mapdaten werden nicht verändert. Blend-/Spezialobjekte behalten ihren vorhandenen Pass.

## 20. Material Translation

BaseColorTexture und BaseColorFactor verwenden die bestehende Diffuse-/Modulate-Semantik.
Neutrales `explicitRenderState` erlaubt authored Materialzustände, während GR2 seine bisherigen
Passdefaults behält. Untexturierte Materialien erhalten ein gecachtes weißes Pixel.
BaseColorFactor muss endlich und je Komponente in [0,1] liegen. Die Übersetzung verwendet nur
BaseColor aus metallic-roughness-Materialien; Metallic/Roughness, weitere PBR-Texturen und
Material-Extensions aktivieren keine zusätzliche Beleuchtung. Ein fehlendes Material erhält einen
OPAQUE-Default. Materialkopien und per-instance Overrides bleiben an der bestehenden Palette.

## 21. Alpha Modes

OPAQUE ignoriert Textur-/Faktoralpha; MASK verwendet Alpha-Test mit Cutoff;
BLEND verwendet SRCALPHA/INVSRCALPHA und deaktivierte Depth Writes.
Endlicher, nichtnegativer Cutoff bis 1 wird in den 8-Bit-AlphaReference-Vertrag übertragen (`ceil(cutoff*255)`);
dies ist eine dokumentierte Legacy-Approximation. Cutoff > 1 verwirft alle Fragmente über
den bestehenden GREATER/255-Zustand. Keine Shaderänderung. Native Pixelchecks sind für OPAQUE trotz
Texturalpha 0, MASK um 0,5 inklusive Faktoralpha und BLEND gegen erwartete RGB-Werte eingerichtet;
Cutoff > 1 wird zusätzlich am neutralen Zustand geprüft.

## 22. Double Sided

Material-Culling None für doubleSided. Neutrale Materialkopien behalten authored Culling;
die bisherige GR2-Copy-Semantik bleibt erhalten.
Das bedeutet geometrische Sichtbarkeit beider Seiten. Es wird keine PBR-konforme Umkehr der
Rückseitennormale im Shader eingeführt; die vorhandene Legacy-Beleuchtung bleibt maßgeblich.

## 23. Textures

PNG/JPEG über den vorhandenen Decoder. Ressourceneinträge besitzen kodierte Bilddaten
und dekodierte Bildressourcen; GPU-Uploads werden am Bild gecacht.
Nur eingebettete BufferView-Bilder mit passendem MIME-Type; alle URI-/Data-URI-Bilder bleiben unsupported.
PNG-Signatur/Chunkgrenzen/CRC und JPEG-Marker/Dimensionen werden vorab geprüft. Erst der vorhandene
stb-Decoder beweist, dass die komprimierten Pixeldaten tatsächlich dekodierbar sind.

Alle deklarierten Sampler müssen in den E1-X-Ausschnitt passen: MAG absent/LINEAR,
MIN absent/LINEAR/LINEAR_MIPMAP_LINEAR, WrapS/WrapT REPEAT. Gültige NEAREST-, CLAMP_TO_EDGE-,
MIRRORED_REPEAT- und andere Mip-Varianten werden explizit als unsupported zurückgewiesen;
ungültige Enumzahlen als fehlerhaft. Der bestehende lineare Sampler bleibt aktiv. Eingebettete Bilder
werden mit einer Mip-Stufe angelegt; bei fehlender Mip-Kette bleibt die lineare Basisebene die
Darstellung. Kein neuer Sampler-/Mipgenerator und keine Behauptung vollständiger glTF-Samplerparität.

## 24. Embedded Images

GLB-Bilder aus BufferViews werden als besitzende neutrale Bilddaten übertragen. Kein geliehener
Zeiger auf cgltf-Daten. Fehlgeschlagener Decode verhindert die Material-/Modellkonstruktion.
Die Bildkennung kombiniert Asset-ID, Image-Index und Inhalts-Hash. `GetEncodedImagePointer` benutzt
den bestehenden ResourceManager; Materialregistrierung liefert bei Decodefehlern `InvalidMaterial`,
und Mesh-/Modellaufbau bricht ab. Nach Parserfreigabe bleiben Bildbytes und Ressourcen gültig.

## 25. Skeleton Import

Höchstens ein deklariertes Skin pro GLB. Joint-IDs behalten die Reihenfolge aus `skin.joints`;
Namen, Quell-Node-ID, nächster Joint-Parent und expliziter Skeleton-Quellroot werden gespeichert.
Nicht-Joint-Nodes zwischen zwei Joints werden in die lokale Bindmatrix eingerechnet.
Der maßgebliche vollständige Transform steht in `localBindMatrix` mit `hasLocalBindMatrix=true`;
das ältere TRS-Feld erhält nur die daraus abgeleitete Translation, keine neue Decomposition.
InverseBindMatrices sind Float32/MAT4 mit exakt einem Eintrag je Joint; ohne Accessor gilt Identity.
Alle Matrizen durchlaufen dieselbe Basiskonjugation wie Abschnitt 11.

Sobald irgendein Skin deklariert ist, wird das gesamte importierte ModelAsset konservativ
`renderable=false`, auch bei zusätzlicher statischer Geometrie oder unbenutztem Skin. Metadaten sind
über AssetRuntime verfügbar; der normale `CGrannyModel`-Aufbau und ein AnimationInstance werden
abgelehnt. Es gibt in E1-X weder Skin-Bindpose-Visualisierung noch produktive glTF-Skinning-/Attachment-Parität.
Rein rigide GLBs erhalten dagegen genau einen synthetischen Identity-Root mit normalem statischem
Binding; individuelle Scene-Nodes werden nicht als benannte Attachment-Bones angeboten.

## 26. Skin Data

Nur ein VEC4-Set JOINTS_0/WEIGHTS_0: Joints unnormalized UInt8/UInt16; Gewichte Float32 oder
normalized UInt8/UInt16. Beide Accessoren benötigen die POSITION-Anzahl und eine Node-Skin-Zuordnung.
Auch Nullgewicht-Jointindices müssen im Skinbereich liegen. Werte müssen endlich, Gewichte in [0,1],
die Summe > 1e-8 sein; anschließend wird je Vertex normalisiert. Weitere Influence-Sets werden abgelehnt.
Die neutralen `jointIndices`/`jointWeights` besitzen die Daten. Sie sind kein gepackter produktiver
GPU-Skin-Stream; dessen bisherige Byte-Gewichte, Remapping und UInt16-Indices bleiben unverändert.

## 27. Animation Metadata

Clipname, Dauer, Channel-/Keyframe-Anzahl, Ziel-Node/-Name, TRS-Pfad und LINEAR/STEP/CUBICSPLINE.
Float32-Zeitwerte müssen nichtnegativ und strikt steigend sein; Output-Form und Anzahl müssen zum
Pfad passen (CUBICSPLINE: drei Outputs je Key). Rotationskeys werden auf normierte Quaternionen
geprüft; doppelte Node-/Pfad-Channels und Morph-Channels werden abgelehnt.
Die Dauer ist der größte Sampler-Endzeitpunkt. Nur Metadaten werden gespeichert, keine ausführbaren
Sampler-/Keyarrays. `metadataOnly` hält diese Clips aus `CGraphicThing::GetMotionCount`/Granny-Motions
heraus. Rigide Modelle mit Clips zeigen ihre beim Laden gebackene Ausgangsgeometrie; kein Mixer/Sampling.

## 28. Extensions

| Extension / Fall | E1-X-Verhalten |
|---|---|
| KHR_texture_transform | unterstützt auf BaseColor/TEXCOORD_0, in UVs gebacken |
| KHR_mesh_quantization | unterstützt für die in Abschnitt 8 genannten Attributtypen |
| EXT_meshopt_compression | komprimierte BufferView wird ausdrücklich abgelehnt |
| KHR_draco_mesh_compression | komprimiertes Primitive wird ausdrücklich abgelehnt |
| EXT_mesh_gpu_instancing | Node wird ausdrücklich abgelehnt; keine still fehlenden Instanzen |
| KHR_materials_* / KHR_materials_unlit | required: Fehler; optional: bestehende Core-BaseColor-/Legacy-Darstellung |
| Andere required Extensions | Fehler mit Extensionnamen |
| Andere optionale Extensions | keine zugesicherte Semantik; Core-Fallback, soweit vorhandene Core-Daten ausreichen |

Die Required-Allowlist besteht genau aus Texture-Transform und Mesh-Quantization. Extensions werden
beim Load geprüft, aber kein allgemeines Extensioninventar im öffentlichen ModelAsset gespeichert.
Primitive-Morph-Targets, Node-Morphgewichte und Animations-Weight-Channels werden unabhängig von
Extensions zurückgewiesen. Mesh-Weight-Defaults werden nicht ausgewertet.

## 29. meshoptimizer Decision

Für E2-X zurückgestellt. MIT, C/C++-API und modulare Quellen passen grundsätzlich,
aber unkomprimierte kleine GLBs benötigen in E1-X keinen zusätzlichen Optimierungs-/Decoderpfad.
Vertex-Cache-, Overdraw-/Vertex-Fetch-Optimierung, Simplification und meshopt-Decode sind für einen
gesondert messbaren Import-/Offline-Block geeignet. Der Audit begründet die Nichtintegration;
es gibt keinen Performancevergleich oder heimlich zugeschalteten Optimierungsschritt.
[Primärquelle](https://github.com/zeux/meshoptimizer).

## 30. Draco Decision

Kein Draco-Decoder. Required Draco wird explizit als unsupported zurückgewiesen.

## 31. Bounds

Bounds entstehen aus den tatsächlich transformierten Vertices, einschließlich Node-Scale.
Der Rendernachweis wechselt Kameraseite und Fenstergröße und prüft sichtbare Pixel.
Rigide Meshes liefern diese Bounds zusätzlich als Bounds des synthetischen Root-Bindings,
damit die vorhandene ModelInstance-Bounds-/Culling-Abfrage keine Granny-Strukturen voraussetzt.

## 32. Ownership/Lifetime

AssetDocument besitzt Metadaten und Uploaddaten. ModelHandle hält das Dokument.
EncodedImage besitzt Bildbytes; Resource-/Materialreferenzen halten die Bilder.
Nach `ReleaseUploadData` bleiben Metadaten und bereits erstellte Consumer-Sources gültig.
Die Provider-Kopie verwirft die temporären cgltf-Strukturen noch während des Loads. Nach Freigabe
der Mesh-Uploaddaten liefern CopyVertices/CopyIndices `UploadDataReleased`. Statische Sessions
besitzen ihren ModelHandle weiter; sie erzeugen ausschließlich Identity-/Attachment-Pose und
melden Bewegungs-/Deformationsoperationen als unsupported.

## 33. Cache

ResourceManager bleibt maßgeblich. Zweimal derselbe GLB-Pfad ergibt dasselbe CGraphicThing
und Dokument. Kein zweiter globaler Provider-Cache; keine Textur pro Draw.
Ein direkter wiederholter Aufruf des Low-Level-Providers parst erneut. Der Cachevertrag gilt für
den produktiven ResourceManager-Pfad, nicht für einen neu erfundenen globalen Parsercache.

## 34. Failure Semantics

Leere Eingaben werden an der neutralen Fassade als `InvalidInput` abgelehnt. Fehlerhafte GLB-Struktur,
JSON-Typen, Chunks, Accessors, Indices, POSITION, Bilder oder Node-Hierarchien liefern `InvalidAsset`
mit Diagnose; nicht freigeschaltete Primitive-Modi, Kompressionen oder Required-Extensions
`UnsupportedLayout`. Die genaue Klassifikation einzelner Containerbeschränkungen bleibt die des
Providers: etwa mehrere/externe Buffer liefern `InvalidAsset`, obwohl die Diagnose die Begrenzung nennt.
Allokationsfehler werden abgefangen. Der normale Thing-Aufbau meldet die Diagnose und bricht ab;
fehlschlagender Bilddecode verhindert die Material-/Modellkonstruktion. Kein Granny-Fallback,
kein erfolgreiches leeres Modell und kein Ersatz kaputter Assets durch ein Dummy-Mesh.

## 35. Robustness/Security

Checked offset/size/count/stride, endliche Floatwerte, Enumprüfung und Hierarchieschutz.
Geprüfte Accessor-Lesezugriffe statt ungeprüfter Pointer-Arithmetik. Die implementierten Grenzen:

| Bereich | Grenze |
|---|---|
| GLB / JSON-Chunk | 128 MiB / 8 MiB |
| JSON | Tiefe 64, höchstens 1.000.000 geprüfte Werte/Tokens |
| cgltf-Allokationen | 256 MiB kumulativ je Parse |
| Konvertierungsbudget | separat 256 MiB kumulativ je Dokument |
| Accessor-Elemente / Primitive-Indices | jeweils höchstens 4.194.304 |
| Sparse-Validierung | insgesamt höchstens 4.194.304 sparse Einträge |
| Top-Level-Objektarrays / ausgegebene Scene-Primitives | jeweils höchstens 32.768 |
| Node-Hierarchie | Tiefe 256 |
| Ein kodiertes Bild | 32 MiB |
| Bilddimensionen | je Achse höchstens 8192; Fläche höchstens 16.777.216 Pixel |
| Skin | höchstens ein deklariertes Skin, 1–32.768 Joints |
| Konvertierte Position | endlich, Betrag jeder Komponente kleiner als 1e10 Client-Einheiten |

Das Konvertierungsbudget zählt kodierte Bilder, vorausberechnete RGBA-Bildgrößen, temporäre
Floatarrays, Meshstreams und große Tangenten-/Skinarrays. Beide Budgets sind kumulativ, keine
Behauptung eines globalen 256-MiB-Prozesslimits: Eingabebytes, sonstige Metadaten, Consumer-Kopien
und GPU-Ressourcen kommen hinzu. JSON-Zahlfelder für Indices/Größen benötigen ein nichtnegatives
UInt32-Dezimalliteral; auch mathematisch integrale Exponenten-/Bruchschreibweisen werden abgelehnt.
Semantische Keys und Enumwerte mit Escape-Schreibweise werden konservativ abgelehnt;
`extras` und unbekannte optionale Extensioninhalte werden nur syntaktisch geprüft.
E1-X ist ein begrenztes Fast Gate; kein behaupteter vollständiger Fuzz-/Security-Nachweis.

## 36. Test Fixtures

`generate_gltf_fixtures.py` erzeugt mit Python-Standardbibliothek zwei eigene Assets mit jeweils 7184 Bytes:
`fixtures/market_stall.glb` und die Variante `market_stall_static.glb`: neun Mesh-Nodes/Boxinstanzen,
Interleaved PNT, asymmetrische PNG-Textur, nonuniform Scale, OPAQUE/MASK und doubleSided.
Die erste Variante enthält zusätzlich BLEND; die zweite hält den normalen StaticObject-Pfad ein.
Kleine synthetische Providerfixtures prüfen gesondert Datenlayout- und Fehlerfälle. Keine
heruntergeladenen Testassets. Die Fixture-Bytes werden eingecheckt; Python ist keine Runtime-Dependency.

## 37. Real GLB Render Proof

`AssetRuntime.GlTFRender` und `AssetRuntime.GlTFStaticRender`: echtes GLB durch ResourceManager/Thing/Model/Mesh/Material,
GPU-Buffer, Bilddecoder und bestehenden Diligent-Renderer. Statische Variante über StaticObjectSource;
BLEND-Variante über den bestehenden Actor-/SpecialObject-Sourcepfad. Je drei native Bildreadbacks,
Cache-/Instanz-/Bounds-/Ressourcenchecks. **PASS in beiden finalen Windows-Konfigurationen.**
Die Belegbilder liegen nach dem Test unter
`build-c3x/windows/tests/AssetRuntime/e1x-market-stall-{static,blend}-camera-{0,1,2}.bmp`.
Das wiederverwendete Testziel nutzt die normalen statischen beziehungsweise speziellen Drawpfade
gemäß `HaveBlend`; eine neue GLB-Sonderklasse im Renderer existiert nicht.
Die zuletzt gespeicherten Bilder stammen vom finalen Debug-Lauf; auch Release bestand die
GPU-Readbacks. Zusätzlich wird ein untexturiertes GLB mit generierten Normalen/Null-UVs durch
den echten `CGrannyModel`-Consumer aufgebaut und anschließend freigegeben.

## 38. Public Header Leak Test

`AssetRuntime.PublicHeaders` prüft öffentliche Runtime-Header auf Granny, cgltf/fastgltf,
Windows/D3D/Diligent. Private Providerverzeichnisse sind ausgenommen; der portable Build
kompiliert dieselbe neutrale API ohne Granny- und Renderer-Bibliotheken. **PASS Release/Debug/GCC.**

## 39. Provider Dependency Leak Audit

`AssetRuntime.ProviderDependencies` durchsucht eigene `.h`-/`.cpp`-Dateien und verbietet
cgltf/fastgltf-Includes außerhalb `AssetRuntime/GlTF`. Erwartung: null Treffer in GameLib,
World-/Actor-Consumern, EterLib und Renderer. Der Test ist auch im GCC-Common-Gate registriert.
**PASS Release/Debug/GCC: null verbotene Includes.** Der Include-Audit ist kein allgemeiner Datenflussbeweis.

## 40. GR2 Regression

**PASS.** Vorhandenes GrannyParity-/Originalasset-Fast-Gate in Release und Debug sowie kurzer
nativer Originalwelt-Smoke. Player/NPC/Mob/Boss/Mount/Hair/Weapon/Buildings und Props bleiben auf
Granny. Die automatische Sequenz durchlief drei Originalwelt-/Actorphasen mit 3466 Frames;
die manuelle Ingame-Sichtprüfung wurde anschließend positiv bestätigt (Abschnitt 48).

## 41. GPU Skinning Regression

GPU bleibt Default; CPU-Referenz/Fallback bleibt erhalten. Keine Shader-/Skinning-Math-Änderung.
**PASS:** frischer Smoke mit `GPUFrames=17050`, `AllCPUDeformationCalls=0`,
`AllCPUDeformationVertices=0`, `GPUFallbacks=0`, `SkinPreparationFailures=0`.
Der bestehende kurze SkinningBenchmark besteht in Release und Debug; seine CPU-Referenzteile
bleiben unverändert. Kein neues glTF-Skinning wird aus diesem GR2-Nachweis abgeleitet.

## 42. Hair LOD

**PASS:** bestehender HairLodQuick, Release 6,37 s / Debug 7,16 s. Keine große Phase-B-Suite.

## 43. Release Build

**PASS:** vollständiger inkrementeller Windows-x64-Neubau, BuildExit 0, 58,64 s.
Fast Gate **26/26**, 13,11 s. Belege: `build-c3x/e1x-release-acceptance-build.log`
und `build-c3x/e1x-release-acceptance-tests.log`.
Der Build enthält 236 bestehende LNK4099-Meldungen fehlender Third-Party-PDBs sowie den
bekannten /LTCG-Link-Neustarthinweis. Er wird nicht als warning-free bezeichnet.

SHA256 des Release-Binarys, der isolierten Smoke-Kopie und der manuellen Kopie:
`DD64E397AF59E7785E0F16C07B8A060157A5EDB16AF5165F333BE10F51DE872F`.

Reproduktion im Source-Checkout mit vorhandener CMake-Konfiguration, sequenziell ausführen:

```powershell
cmake --build build-c3x/windows --config Release --parallel 4
cmake --build build-c3x/windows --config Debug --parallel 4
```

## 44. Debug Build

**PASS:** vollständiger inkrementeller Windows-x64-Debug-Neubau, BuildExit 0, 87,52 s.
Fast Gate **26/26**, 16,21 s. Belege: `build-c3x/e1x-debug-acceptance-build.log`
und `build-c3x/e1x-debug-acceptance-tests.log`.
Bestehende Meldungen bleiben sichtbar: dreimal C4834 in MarkManager, zweimal LNK4075,
einmal LNK4098 (Debug-/Release-CRT-Mix) und 284 LNK4099 zu Third-Party-PDBs.
Diese Hinweise sind vom Provider-/Renderer-Testergebnis getrennt.

## 45. GCC/LP64

**PASS:** vorhandener Cygwin/GCC-Common-Neubau einschließlich echtem cgltf-Provider und
Provider-Tests; **8/8**, 0,33 s. Belege: `build-c3x/e1x-gcc-acceptance-build.log`
und `build-c3x/e1x-gcc-acceptance-tests.log`.
Kein kompletter Linux-Client-Build und keine Android-Level-B–E-Arbeit.
Mit dem neuen Provider-Dependency-Test umfasst dieser konfigurierte Ausschnitt acht Tests:

```powershell
cmake --build build-c3x/cygwin-common --parallel 4
ctest --test-dir build-c3x/cygwin-common --output-on-failure
```

## 46. Unit Tests

**PASS:** Provider-, Material-/Bild-, UInt32-Renderer- und bestehende Regressionstests im
finalen Release-/Debug-Fast-Gate jeweils 26/26; portabler GCC-Ausschnitt 8/8.

`GlTFProviderTest` enthält kompakte Fälle für Container/JSON, Typen/Strides/Alignment,
sparse/normalized Daten und unsigned Indexbreiten, Transformation/Spiegelung, Scenegraph,
Material-/Bildmetadaten, Sampler, Extensions, Skin-/Clipmetadaten und Uploadfreigabe.
`WideIndexRenderTest` beweist separat den realen UInt32-Consumer bis zum D3D-Draw mit Index 65536,
nichtnull FirstIndex/BaseVertex und UInt16-Parität. Hinzu kommen fehlerhafte Range-/Formatfälle,
rigider Actor-Pfad ohne Deformationsuploads und erwartete Materialpixel aus den bestehenden Shadern.
Dieser breite Drawtest verwendet neutrale synthetische Geometrie; die beiden echten Marktstand-GLBs
beweisen den vollständigen Dateiladepfad mit kleinen Meshes. Gemeinsam decken sie beide Grenzen ab.
`EmbeddedMaterialTest` prüft Bildbesitz, Decoderfehler, Cache und Materialzustände.

```powershell
$fastGate = '^AssetRuntime\.|^Platform\.|^Renderer\.(StartupOptions|ResourceSource|DiligentD3D11|SkinningInstance|NoLegacyArchitecture|HairLodQuick|SkinningBenchmark)$'
ctest --test-dir build-c3x/windows -C Release --output-on-failure -R $fastGate
ctest --test-dir build-c3x/windows -C Debug --output-on-failure -R $fastGate
```

Eine neue manuelle Laufzeit-/Sichtprüfung wird daraus nicht abgeleitet. Einzelne alte
Integrationsläufe ersetzen nicht den abschließenden Neubau nach dem finalen Providerstand.

## 47. Runtime Smoke

**PASS:** frische Release-Kopie in `build-d1x/runtime/e1x-acceptance-01`, PID 67912,
Exitcode 0 nach 62,6 s, drei Phasen und 3466 Frames. Originalpacks wurden nur gelesen.
Der Harness bestätigt die SHA256-Identität zum fertigen Release-Binary aus Abschnitt 43.
`syserr.txt` ist leer (0 Bytes). Belege: `build-c3x/e1x-runtime-smoke.log`,
`build-d1x/runtime/e1x-acceptance-01/artifact.txt`, `exit.txt`, Runtime-Logs und Screenshots.
Login -> Charakterauswahl -> Ingame wurde zusätzlich zur manuellen Abnahme angefragt;
deren positive Gesamtrückmeldung und eigener Shutdown-Nachweis stehen in Abschnitt 48–50.

Der bestehende begrenzte Harness wird mit frischem Namen wiederverwendet:

```powershell
./tests/AssetRuntime/run_runtime_smoke.ps1 -Name e1x-final -BuildDirectory ./build-c3x/windows
```

Er verwendet das fertig gebaute Release-Binary mit SHA256-Abgleich, private Root-Packdaten,
nur lesend verwendete Originalpacks und einen eigenen Prozess. Sein bestehender Ablagepfad lautet
weiterhin `build-d1x/runtime/e1x-final`; er überschreibt keine vorhandenen Belegordner.
Die automatische Sequenz umfasst drei Originalwelt-/Actorphasen mit Near/Far/Near, Logs,
Screenshots und regulärem Exit innerhalb des 150-Sekunden-Limits. Eine Wiederholung braucht
einen anderen frischen Namen. Das ersetzt weder Login noch eine Nutzerbestätigung der Fensterbedienung.

## 48. Visual Sanity

Native GLB-Readbacks und geprüfte Marktstand-Screenshots zeigen aufrechte Geometrie, Metermaßstab,
Textur und transparente Elemente plausibel; beide finalen Konfigurationen bestehen die Pixelchecks.
Die getrennten Materialpixeltests prüfen OPAQUE trotz Alpha 0, MASK beidseits des Cutoffs,
Faktoralpha, BLEND-Farbwerte und reversed winding/doubleSided mit bestehenden Shadern.

**Manuelle Abnahme positiv:** Kopie `build-c3x/runtime/e1x-manual-01`, PID 42456, mit genau dem
Release-SHA256 aus Abschnitt 43. Die Rückmeldung des Nutzers zur angefragten Checkliste lautet
„sieht sehr gut aus“. Dies wird im Gesprächskontext als positive Gesamtrückmeldung zur manuellen
Abnahme gewertet; einzelne Bedienaktionen wurden nicht separat kommentiert oder beobachtet.
Die Runtime-Logs belegen aktives Ingame-Rendering einschließlich Actors, Hair/Weapon, Welt und Effekten.
Der Prozess ist beendet; der eigene Client-Log bestätigt ExitCode 0 und der Shutdown-Audit null Ressourcen.

Der manuelle `log/syserr.txt` ist **nicht leer**: 6632 Bytes, 85 Zeilen. Davon sind 84 bereits
vorhandene Damage-Diagnosen aus den normalen Erfolgspfaden von
`src/UserInterface/InstanceBaseEffect.cpp:69–214` und einmal `invalid idx 0` aus
`src/UserInterface/MarkManager.cpp:304–310` (Gildenmarkenbild noch nicht im Index).
Dieselben Meldungsklassen sind bereits in `build-c3x/runtime/d1x-manual-01/log/syserr.txt`
und im D1-X-Bericht dokumentiert; beide Codepfade sind unverändert. Kein neuer Fehlertyp
oder E1-X-Renderer-/Providerfehler im manuellen Log. Der automatische E1-X-Smoke hat weiterhin 0 Fehlerlogbytes.

## 49. Resize/Minimize

**Automatischer Test PASS:** Backbuffer-Resize und Suspend/Restore im GLB-Renderer-Test in
Release und Debug. Die Nutzer-Gesamtrückmeldung zur angefragten manuellen Checkliste ist positiv;
Resize, Minimize/Restore und X wurden nicht einzeln protokolliert. Keine Resize-/Rendererfehler
im manuellen Lauf. Die automatische Backend-Prüfung und diese Gesamtrückmeldung bleiben getrennte Belege.

## 50. Shutdown

**Automatischer Test PASS:** Exitcode 0 des frischen Smokes und Shutdown-Audit mit null
überwachten verbleibenden Ressourcen. GLB-Renderer-Tests prüfen Dokumente, Sessions,
Bindings und GPU-Handles nach regulärem Clear/Destroy.

**Manueller Client ebenfalls regulär beendet:** `build-c3x/runtime/e1x-manual-01/renderer-startup.log`
enthält `ExitCode=0`. Das ist der vom Client direkt vor `return result` protokollierte Wert
(`src/UserInterface/UserInterface.cpp:366–367`). Der externe Prozessbeobachter konnte den Wert
nicht übernehmen; das leere Feld in `exit.txt` ist kein Exitcode-Nachweis und bleibt unverändert.
Der frische `source-resource-audit.log` und die Shutdown-Zeilen in `terrain-renderer.log`
belegen null überwachte Asset-/GPU-/UI-/World-/Actor-/Effect-Ressourcen.

## 51. Resource Lifetime

**PASS:** Live-Zähler plus schwache Besitzer-/Cache-Nachweise in Provider-/Consumer-/Materialtests;
GLB-Sessions, Bindings, Dokumente und GPU-Handles werden freigegeben.
Im frischen Originalwelt-Smoke stehen SourceTextures, SourceBuffers, SkinMeshes, BoneRemaps,
BonePalettes, AssetDocuments, AnimationInstances, MeshBindings, PrototypeGeometry,
PrototypePalettes und StaticSkinMeshes im Shutdown-Audit jeweils auf 0.
Auch der anschließende manuelle Lauf endet mit diesen Zählern auf 0 sowie
`AllCPUDeformationCalls=0`, `AllCPUDeformationVertices=0`, `GPUFallbacks=0`,
`SkinPreparationFailures=0` bei `GPUFrames=1093799` und `PaletteUpdates=1090303`.
Das belegt die überwachten Ressourcen und ist keine pauschale Leakfreiheit aus bloßem Prozessende.

## 52. Performance Sanity

Parsing, Mesh-Konversion und Scene-Auswertung beim Laden. Bilddecode im Ressourcenaufbau.
GPU-Upload beim ersten Gebrauch; unveränderte Live-Geometrie-/Texturzahlen über mehrere Drawframes.
Keine großen Benchmarks oder FPS-Versprechen.

## 53. Remaining Limitations

Der unterstützte Produktionsausschnitt sind little-endian GLB-2.0-Dateien mit einem BIN-Buffer,
TRIANGLES, statisch gebackenem Scenegraph, PNT-Geometrie und Core-BaseColor-/Alpha-/Cull-Zuständen.
PNG/JPEG müssen eingebettet sein, Sampler linear/repeat. Größere breite rigide Indexstreams sind
innerhalb der dokumentierten Speicher-/Elementgrenzen unterstützt. `.gltf`, externe/Data-URIs,
weitere Bildformate, Vertexfarben, andere Topologien, Morphs, Draco/meshopt und GPU-Instancing fehlen.

Kein PBR, Normalmapping, neue Sampler-/Mipkettenverwaltung, Sortieralgorithmus oder Rendering-Backend.
Das bestehende AlphaReference quantisiert MASK-Cutoffs; doubleSided schaltet Culling ab, ohne neue
Rückseitennormalen-Beleuchtung. Optionale PBR-Extensions erhalten lediglich Core-/Legacy-Darstellung.
Nur Texture-Transform und Mesh-Quantization sind als required freigeschaltet; Abschnitt 8/23/28
definiert den genauen Ausschnitt. Die strikte JSON-/Containerannahme ist bewusst enger als alle
möglichen validen glTF-Dokumente.

Jedes Dokument mit Skin bleibt vollständig metadatenbasiert und nicht renderbar, höchstens ein Skin.
Keine produktive Skin-Bindpose, glTF-Skinning, Animation-Sampler, Animation-Mixer, Retargeting oder
Granny-Motion-Parität. TRS-/Inverse-Bind-/Channel-Metadaten sind für den späteren Block vorhanden;
statische Node-Animation und individuelle Scene-Node-Attachments werden nicht ausgeführt.

## 54. E2-X Roadmap

Nur dokumentiert: Assimp als Offline-Konverter FBX/OBJ/DAE -> GLB; meshoptimizer,
LOD-Erzeugung, optionale Texturverarbeitung, Batch-Konversion und Validierungs-CLI.
Ein möglicher GR2-Zwischenpfad erfordert einen eigenen Auftrag. Keine Implementierung dieser Punkte.

## 55. Git Diff

`git diff --check` **PASS**. Abschließender Source-/Client-Statusabgleich durchgeführt:
26 geänderte bestehende und 20 neue Dateien im Source-Checkout. Nur Source, CMake,
kurze Tests, die zwei jeweils 7184 Byte kleinen GLB-Fixtures, Dokumentation und
gepinntes cgltf einschließlich Lizenz gehören zur Änderung. Builds, Belegbilder,
Runtime-Kopien und Logs bleiben in ignorierten Buildverzeichnissen.
Der Client-Checkout hat weiterhin ausschließlich die vorgefundenen Änderungen an
`config/channel.inf` und acht unversionierte Renderer-/Auditlogs. Kein Commit/Push.

## 56. GO/NO-GO

**Automatische technische Abnahme PASS:** Release 26/26, Debug 26/26, GCC 8/8,
echter GLB-Rendernachweis, GR2-/GPU-Smoke, Ressourcenaudit und leerer frischer Fehlerlog.

**E1-X GO.** Zur technischen Abnahme kommen die positive Nutzer-Gesamtrückmeldung
„sieht sehr gut aus“, der reguläre manuelle Client-Shutdown mit ExitCode 0 und dessen
frisches Ressourcenaudit mit null verbleibenden überwachten Ressourcen. Die Grenzen der
manuellen Rückmeldung und die bekannten Diagnosemeldungen stehen ausdrücklich in Abschnitt 48–50.
Nach E1-X STOP; nächste Phasen werden nicht begonnen.
