# F5-X – Animated GLB Characters

Datum: 2026-09-15. Arbeitsstand im bestehenden Windows-x64/Diligent-D3D11-Client. Kein Commit/Push.

## 1. Ausgangslage

H-X bleibt die Basis. Vor Änderungen bestanden die sechs gezielten E1-X-Prüfungen: Provider, statisches Rendering, transparentes Rendering, eingebettete Materialien, öffentliche Header und Provider-Abhängigkeiten. GR2 bleibt im nativen ZiiNAN-Reader; Granny und SpeedTree bleiben entfernt. Der vorherige GLB-Pfad importierte Skin-/Animationsmetadaten, gab skinned Modelle jedoch nicht zum Rendern frei.

## 2. GlTF Skin Audit

| Bereich | Vor F5-X | F5-X |
|---|---|---|
| skins/joints | Ein Skin, stabile Joint-Liste, strukturelle Prüfung | Produktives RuntimeSkeleton |
| inverseBindMatrices | Dekodiert und koordinatenkonvertiert | Gemeinsame Bone-Palette, affine/invertierbare Matrizen geprüft |
| Skeleton-Root/Hierarchie | Nicht-Joint-Vorfahren in Bindmatrix zusammengefasst | Echte Hierarchieknoten bleiben animierbar |
| JOINTS_0/WEIGHTS_0 | Neutrale Metadaten; beliebige positive Summe normalisiert | Validierte Gewichte und bestehender PWNT-GPU-Stream |
| Channels/Sampler | Namen, Pfade, Dauer, Interpolation als Metadaten | Tatsächliche RuntimeAnimationClips mit TRS-Keys |
| LINEAR/STEP/CUBICSPLINE | Metadaten, kein Playback | LINEAR/STEP produktiv; CUBICSPLINE klare Ablehnung |
| Actor/Motion | Skinned GLB nicht renderbar; Motion wählte immer Slot 0 | Neutrale Clip-Index-Zuordnung in bestehenden MSA-/Actor-APIs |

## 3. Architecture

```text
GR2 → GR2AssetProvider ─┐
                       ├→ RuntimeSkeleton / RuntimeAnimationClip
GLB → GlTFAssetProvider ┘       ↓
                       RuntimeAnimationInstance
                               ↓
               AnimationRuntime::Sample / Blend / Evaluate / BuildPalette
                               ↓
                 CGrannyModelInstance / bestehendes GPU Skinning
                               ↓
                        DiligentActorRenderer
```

Die vorhandene GR2-Playback-/Pose-/Palette-Implementierung wurde in `src/AssetRuntime/RuntimeAnimationInstance.h` gemeinsam nutzbar gemacht. Der GR2-Adapter behält sein Binding, seine Boundary-Clip-Varianten und seine Root-Motion-Regeln. GLB erzeugt dieselben neutralen Laufzeitobjekte beim Laden. Kein glTF-Animator, Mixer, Actor oder Renderer wurde hinzugefügt. Die GPU-Skinning-Implementierung und Shader wurden nicht geändert.

## 4. RuntimeSkeleton Translation

Jeder Joint bekommt Namen, Source-Node-ID, Parent-ID, lokale Bind-TRS und inverse Bindmatrix. Matrix-Nodes werden verlustfrei als Translation plus 3×3 Scale/Shear mit Identitätsrotation in der vorhandenen Runtime dargestellt. Animationen auf Matrix-Nodes werden abgelehnt; animierte Nodes müssen TRS verwenden. `RuntimeSkeleton::Initialize(IndexedForest)` prüft die gemeinsame Runtime-Hierarchie.

## 5. Joint Order

Die ersten N Skeleton-Einträge entsprechen exakt `skin.joints[0..N)`. Danach folgen tatsächliche übrige Nodes in Source-Reihenfolge. JOINTS_0 wird nicht sortiert oder umnummeriert. Auch Parents, die hinter ihren Kindern stehen, werden durch die getrennte Runtime-Evaluationsreihenfolge korrekt verarbeitet.

## 6. Hierarchy

Echte zusätzliche Roots werden als Forest erhalten. Ein deklarierter `skin.skeleton` muss Vorfahr jedes Joints sein. Joint-Nodes müssen zur aktiven Szene gehören. Doppelte Joint-IDs, fehlende Nodes, ungültige Parents, Selbstbezüge, Mehrfachparents und Zyklen werden verworfen. Die vorhandene allgemeine Hierarchietiefe ist 256; F5-X begrenzt die gesamte Character-Palette zusätzlich auf **163 Nodes**, passend zum bestehenden produktiven GPU-Limit. Ein Skin pro Dokument; mehrere Skins werden explizit abgelehnt.

## 7. Inverse Bind Matrices

FLOAT/MAT4, Accessor-Grenzen, exakte Joint-Anzahl, endliche Werte, affine Form und Invertierbarkeit werden geprüft. Fehlende `inverseBindMatrices` sind nach dem glTF-Vertrag gültig und bedeuten Identitätsmatrizen; dies wird ausdrücklich positiv getestet. Ein vorhandener, ungültiger Accessor wird abgelehnt. Im Character-Fixture ist `inverseBind × bindModel` für alle 19 Joints die Identität.

## 8. Coordinate Conversion

Die bestehende Provider-Konvention bleibt zentral: `p_runtime = 100 × (x, -z, y)`. Row-Vector-Matrizen werden durch `B⁻¹ × M × B` konvertiert. Quaternionen gehen ohne Euler-Umweg nach `(x, -z, y, w)`, Skalierung nach `(x, z, y)`. Beide Räume sind rechtshändig; der Basiswechsel invertiert kein Winding. Statische gespiegelte Mesh-Transforms behalten die vorhandene Winding-Korrektur.

Die Umsetzung folgt den relevanten Skin-/Transform-/Animationsregeln der [Khronos glTF-2.0-Spezifikation](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html). Bei geskinnten Meshes werden Mesh-Node-Transforms nicht zusätzlich auf die bereits durch Joints transformierten Vertices gebacken. Ein absichtlich um `(7,8,9)` verschobener Mesh-Node im Fixture beweist diesen Fall. Root-Rotation, nicht uniforme Root-Skalierung und Translation werden separat geprüft.

## 9. Unit Scale

Meter zu 100 nativen Einheiten ausschließlich im Provider-Basiswechsel. Mesh, Bindpose, inverse Binds und Translation-Keys verwenden dieselbe Konvention. Keine Größenkorrektur im Actor oder Renderer.

## 10. Skin Weights

JOINTS_0: unnormalisierte UBYTE/USHORT-VEC4. WEIGHTS_0: FLOAT oder normalisierte UBYTE/USHORT-VEC4. Vertex-Anzahlen müssen passen. Alle vier Joint-IDs werden gegen die Skin-Joint-Anzahl geprüft, auch Slots mit Gewicht 0. Negative, nicht endliche und Gewichte über 1 werden verworfen.

## 11. Influence Policy

Bis vier Einflüsse. JOINTS_1/WEIGHTS_1 werden ausdrücklich abgelehnt; keine stille Reduktion. Gewichtssummen müssen innerhalb **±0,02** von 1 liegen, erst dann wird deterministisch normalisiert. Nullgewichte und grob ungültige Mengen werden nicht repariert. Der bestehende GPU-Stream verwendet UNORM8-Gewichte: Largest-Remainder-Quantisierung ergibt exakt 255 Byte-Einheiten pro Vertex. Diese dokumentierte Präzisionsgrenze gilt auch für kleine Einflüsse. Maximal 65.535 kombinierte Character-Vertices; größere Assets müssen offline aufgeteilt werden, statt in einen CPU-Fallback zu gelangen.

## 12. Animation Channels

Vorhandene Offline-Exporte mit reiner Node-Animation ohne Skin bleiben gültig: Die tatsächlichen Nodes werden in dieselbe Runtime-Hierarchie übernommen und rigide Meshes an ihre Node-ID gebunden. AssetTool.ImportRoundtrip prüft diesen Fall einschließlich einer deklarierten Haltephase und eines DAE-Skin-Exports. Ein Zwischenstand, der Skin zwingend voraussetzte, wurde durch diesen Regressionstest aufgedeckt und vor der finalen Abnahme korrigiert.

Translation, Rotation und Scale erzeugen echte `AnimationTrack`-Keys auf stabilen Bone-IDs, einschließlich animierter Nicht-Joint-Vorfahren. Mehrere Kanäle desselben Nodes werden in einem Track gesammelt. Doppelte Node/Pfad-Ziele und ungültige Targets werden verworfen. Source-Daten werden einmal dekodiert und inklusive Runtime-Key-Speicher gegen das Importbudget gerechnet.

## 13. Time Representation

Sekunden; FLOAT-Source-Timestamps werden in die vorhandenen Double-Key-Zeiten übernommen. Zeiten müssen endlich, nicht negativ und strikt aufsteigend sein. Dauer ist der letzte maximale Timestamp. Leere Accessors/Channels sind Fehler. Ein einzelner Key bei Sekunde 0 ist ein gültiger, konstanter Zero-Duration-Clip. Keine FPS-Annahme im Import.

## 14. Translation

Keys werden durch dieselbe Meter-/Achsenbasis wie Positionen übersetzt. Die vorhandene Runtime interpoliert und verwendet Bindwerte für fehlende Kanäle. Kein Provider-Sampler.

## 15. Rotation

Endliche, annähernd normalisierte Quaternionen werden geprüft und in der Runtime normalisiert. Der bestehende generische Sampler erhielt eine optionale `SphericalLinear`-Policy für konstante Winkelgeschwindigkeit. Nur importierte LINEAR-Rotationstracks wählen sie. GR2 behält seine bisherige normalisierte lineare Quaternion-Interpolation. STEP bleibt STEP; kürzester Quaternion-Bogen und ein 180°-Interpolationstest sind abgedeckt.

## 16. Scale

Uniforme und nicht uniforme Scale-Keys werden als diagonale Scale/Shear-Tracks übertragen. Die Runtime übernimmt Sampling und Pose-Erzeugung; kein neues Normalen-/Lighting-System.

## 17. Interpolation

LINEAR und STEP werden produktiv unterstützt. CUBICSPLINE wird bereits beim Import mit einer eindeutigen Unsupported-Diagnose abgelehnt. Ein Export muss hierfür explizit LINEAR/STEP erzeugen; keine automatische Umdeutung.

## 18. Clip Naming

Stabile Dokument-Slots sind die Clip-Identifier. Source-Namen bleiben erhalten; fehlende Namen bekommen `animation-N`. Das Fixture verwendet absichtlich `Ruhe`, `Gehen`, `Laufen`, `Schlag`, `Treffer`, `Fallen`.

## 19. Motion Mapping

MSA unterstützt optional `MotionClipIndex` (Default 0 für alle bisherigen Assets). `CRaceMotionData`, `CGraphicThingInstance` und die Dauerabfrage im bestehenden Actor transportieren diesen Index. Ungültige/malformed/negative Indizes werden beim Parsing beziehungsweise beim Zugriff abgewiesen. Im Actor gibt es keine Extension-Abfrage und keine glTF-Typen.

| Motion | Clip-ID | Source-Name | Dauer | Policy |
|---|---:|---|---:|---|
| IDLE | 0 | Ruhe | 2,0 s | Loop |
| WALK | 1 | Gehen | 1,2 s | Loop |
| RUN | 2 | Laufen | 0,7 s | Loop |
| ATTACK | 3 | Schlag | 0,8 s | One-shot |
| DAMAGE | 4 | Treffer | 0,55 s | One-shot |
| DEATH | 5 | Fallen | 1,3 s | One-shot |

## 20. Loop Semantics

Der Loader baut Clips mit neutraler Clamp-Voreinstellung. Motion-Aufrufe/Character-Definition entscheiden über Loops. `motions.json` dokumentiert die Fixture-Zuordnung; der native Smoke verwendet bestehendes BlendLoopMotion, PushOnceMotion und Die. Geschwindigkeiten 0,5/1/2 und One-shot-Enden werden gegen direktes Runtime-Sampling geprüft.

## 21. Root Motion Decision

Das Fixture enthält Pelvis-Bobbing und eine echte Translation beim Fallen. Sie bleiben vollständig in den Pose-Tracks. Der neutrale GLB-Playback-Pfad verändert die Gameplay-/World-Matrix in `UpdateTransform` nicht. GR2s vorhandene Movement-Behandlung bleibt im GR2-Adapter. Kein Server-/Protokollwechsel.

## 22. Bone Palette

Die gemeinsame Runtime erzeugt Model-Matrizen mit `local × parent`, danach `inverseBind × model`. Palette-Slot entspricht der Skeleton-ID. Der GPU-Test liest den tatsächlich gebundenen Bone-Constant-Buffer zurück und vergleicht Reihenfolge, Werte und Null-Tail exakt.

## 23. GPU Skinning

GLB liefert den bestehenden 40-Byte-PWNT-Stream: Position, vier Byte-Gewichte, vier Byte-Indizes, Normalen, UV. Neutrale Mesh-zu-Skeleton-Remaps gehen durch denselben GPU-Vorbereitungspfad wie GR2. Produktions-CPU-Deformation und Fallback müssen 0 bleiben. Keine Änderungen an Skinning-Shader oder Diligent-Implementierung.

## 24. Test Character Source

Fixture-Größe: **39.796 Bytes**. Zweite unabhängige Erzeugung in `build/f5x/reproduced-fixture` stimmt für beide GLBs und alle Motion-Dateien bytegenau überein; Protokoll `build/f5x/fixture-determinism.log`. Character-SHA256: `447ba15c7a7bb22270b51a8743f9c883c81ecfd4302df6208ff03a4f86685759`.

Eigenes prozedurales, artikuliertes humanoides Testasset: Torso, Kopf, Arme, Beine, 19 Joints, zwei Materialgruppen, Checker-Textur und sechs Animationen. Zwei reale zusätzliche Nodes ergeben 21 Runtime-Einträge. Keine Internet-Assets. Generator, kleine GLBs und Definitionen liegen unter `tests/AssetRuntime/`; Kennzeichnung CC0-1.0 für diese eigens erzeugten Fixtures.

## 25. Blender Pipeline if used

Blender/bpy war über PATH beziehungsweise die Standardinstallation nicht verfügbar. Das Fixture wird reproduzierbar mit Python-Standardbibliothek direkt als GLB erzeugt: `python3 tests/AssetRuntime/generate_character_fixture.py`. Ein tatsächlicher Blender-Export wurde in diesem Lauf **nicht ausgeführt**. Der implementierte Importvertrag unterstützt entsprechende Blender-Exporte mit einem Skin, eingebetteten Texturen, bis vier Einflüssen und LINEAR/STEP-TRS-Clips. Das ist eine Formatfähigkeit, kein behaupteter Blender-Bediennachweis.

## 26. Materials

Bestehende MaterialAsset-Semantik einschließlich Base-Color-Faktor, Textur, Alpha und Culling. Das Character-Fixture ist opaque und einseitig. Das cyanfarbene Hand-Prop ist ein eigenes starres GLB. Keine PBR-/Remaster-Erweiterung.

## 27. Textures

Eine eingebettete 8×8-PNG wird von beiden Character-Materialien geteilt. Bestehender Image-/Texture-Cache übernimmt Decode und Upload. Der 20-Instanzen-Test verlangt genau eine Character-Textur. Keine Texturladung im Frame-Loop. Die E1-X-Material-/Texture-Regression bleibt enthalten.

## 28. Multi-Primitive

Zwei Mesh-Primitives desselben Skins ergeben zwei Draws pro Actor. Im Mehrinstanzen-Frame werden 40 Draws nachgewiesen. Rigid-/Skinned-/Mixed-Klassifikation bleibt neutral; rigide Nodes eines animierten Dokuments erhalten ihre tatsächliche Node-ID als Bindung.

## 29. Actor Integration

Das private native Test-Root registriert Development-Race 65000 und lädt normale MSM/MSA-Dateien. `CInstanceBase → CActorInstance → CGraphicThingInstance → CGrannyModelInstance → DiligentActorRenderer` führt die Szene aus. Der Name CGrannyModelInstance bezeichnet den erhaltenen Consumer, keine Granny-Abhängigkeit. Keine produktiven Race-Dateien oder Player-/Mob-Bestände wurden migriert.

## 30. Attachment Proof

`ResolveAttachment(skeleton, BoneId{12}, Weapon)` bindet das eigene starre Hand-Prop an RightHand. Die normale `SetParentModelInstance`-Anbindung übernimmt die animierte Matrix. Test: Prop-Root-Matrix entspricht der RightHand-Model-Matrix; nativer Screenshot zeigt das Prop während Attack. Der bisherige Name-Lookup bleibt deterministisch beim ersten Source-Eintrag; doppelte Display-Namen sind erlaubt und per expliziter ID unterscheidbar.

## 31. Blending

Idle → Walk → Run → Idle nutzt die gemeinsam aus dem Produktionspfad übernommene Blend-/Crossfade-Logik. Der Test vergleicht die Palette unmittelbar vor und am Beginn jedes Übergangs und prüft endliche Posen nach dem Übergang. Kein GLB-Mixer.

## 32. Attack Transition

Idle → Attack → Idle wird sowohl im Runtime-Test als auch im nativen Actor-Smoke ausgeführt. Der ausgehende One-shot wird beim Blenden geklemmt; keine erneute Loop-Auswertung des beendeten GLB-Clips.

## 33. Damage/Death

Damage und Death verwenden One-shot-Playback. Death erreicht und hält die letzte Pose. Native `chr.Die()` prüft die bestehende Actor-Endsemantik. Kein neues Hit-/Skill-/Eventsystem; die Fixtures definieren keine Hitframes.

## 34. Unsupported Features

Mehrere Skins pro Dokument, über 163 gesamte Character-Nodes, über 65.535 kombinierte Vertices, zusätzliche Influence-Sets, CUBICSPLINE, Morph-Targets/-Channels und nicht unterstützte Required Extensions werden abgelehnt. Kein Cross-Document-Retargeting für GLB. Statische E1-X- und Vegetationsunterstützung bleibt bestehen. Externe Animation-only-GLBs ohne Skin sind nicht Ziel dieses Milestones.

## 35. Ownership

Provider-Strukturen werden nach dem Import freigegeben. Mesh-/Material-Metadaten, Skeleton und Clips sind eigenständig besessen. Model-/AnimationHandles halten das Dokument am Leben. ReleaseUploadData gibt temporäre Vertex-/Indexdaten nach bestehendem Uploadvertrag frei; Animationen und Attachments bleiben nutzbar.

## 36. Clip Sharing

Ein Dokument besitzt ein RuntimeSkeleton und sechs immutable RuntimeAnimationClips. Alle Actors referenzieren dieselben Definitionen. Individuell bleiben Clock, aktuelle Controls, Blendzustand, Pose und Palette. Keine Actor-abhängigen Clip-Decodes oder Retarget-Kopien.

## 37. Multi-Instance

20 native Actors desselben ModelAssets. Die bestehenden GPU-Wrapper/Pose-Buffer sind pro Actor; der unveränderliche Vertex-/Indexbuffer-Satz und die Character-Textur werden geteilt. Der Test prüft den **gemeinsamen Meshbuffer-Zähler**, nicht fälschlich nur die Anzahl der instanzbezogenen Geometry-Wrapper.

## 38. Hotpath

Warm: Sample, Blend, Evaluate, BuildPalette, Palette-Upload, Draw. Import/Floats/Node-Graph/Skeleton-/Clip-Konstruktion liegen ausschließlich im Load-Pfad. Motion-Wechsel wählen Clip-Slots; kein glTF-Parsing oder String-Binding im Frame-Loop. 2.400 Posen mit 20 Instanzen lassen die Anzahl der Skeleton-/Clip-Definitionen unverändert.

## 39. Error Tests

Gezielt geprüft: fehlende Skin-Zuordnung, ungültige/fehlende/duplizierte Joints, inverse-Bind-Anzahl und NaN, gültiges Fehlen des optionalen Inverse-Accessors, Null-/falsche Gewichtssummen, nicht endliche Transforms, Zyklen, ungültige Animationsaccessors/Targets, unsortierte/doppelte/negative Zeiten, leere Accessors, CUBICSPLINE, zusätzliche Influences, Morph-Animation und unbekannte Required Extension. E1-X prüft ergänzend Container-/Accessor-Grenzen, sparse/normalisierte Daten, Topologien, Transforms und Materialien.

## 40. Header Leak Test

AssetRuntime.PublicHeaders, AssetRuntime.ProviderDependencies und AnimationRuntime.PublicHeaders prüfen die bestehenden Grenzen. Neue Actor-/Motion-Header enthalten keine cgltf-/fastgltf-Typen. Der Loader bleibt unter `src/AssetRuntime/GlTF`.

## 41. GR2 Regression

**PASS:** Finaler kurzer nativer Player-/Mob-/Mount-/Hair-/Weapon-Smoke `build/f2x/runtime/f5x-regression-final`: sechs Actors, eine A1-Phase, **3.555 Frames / 62,9 s / Exitcode 0**. Idle/Walk/Run/Attack wurden mit 843/906/904/902 Frames erfasst. Originale Hair-/Weapon-Draws und Mount/Rider sind im Actor-Log belegt; Nah-/Fernbilder wurden gelesen. Derselbe finale Release-SHA256 wie `native-07`, ausschließlich Production Defaults.

25.454 GPU-Actor-Frames, 859 native GR2-Reads, 23 native Animation-Decodes; CPU-Deformation/Fallbacks, AnimationRuntimeFailures und sämtliche erfassten Ressourcen **0**. `syserr.txt` leer. Log: `build/f5x/gr2-regression-final.log`. Bestehende Golden-/Compatibility-/Warmup-/Skinning-Prüfungen sind in beiden Windows-Gates sowie im verfügbaren portablen Umfang grün. Keine automatische Provider-Ausweichroute.

## 42. Vegetation Regression

Der native Character-Smoke zeichnet die bestehende A1-Vegetation. Contracts, Goldens, RenderGoldens und NoLegacyDependency gehören zum Gate. SpeedTree bleibt 0. Keine Vegetationsmigration/-Optimierung.

Finaler GR2-Regressionslauf: **194 erstellte Instanzen, 165.816 Draws, 39 LOD-Wechsel**, VegetationFailures und alle Vegetationsressourcen beim Shutdown **0**. Der Runner verwendet die bereits aus H-X vorhandenen kompilierten Vegetationsassets unter `build/hx/compiled/vegetation`; keine neue SDK-Konvertierung.

## 43. Release

Vollständiger Build in `build-hx-clean`, Visual Studio 2022 x64, bestehender Diligent-D3D11-Stack. Finale Nachweise: `build/f5x/release-final.log`, `release-tests.log`. Bekannte LNK4099-/LTCG-/Fremdbibliotheksdiagnosen bleiben sichtbar; kein Anspruch auf warnungsfreien Build.

**48/48 PASS in 27,12 s**, Build Exitcode 0. Die anschließende Erweiterung ausschließlich des GPU-Testharness um echte native Fensteraktionen wurde gezielt erneut gebaut und geprüft: **1/1 PASS, 5,45 s**, `window-release-build.log`, `window-release-tests.log`, `window-release-details.log`. Der produktive Release-Client blieb dabei unverändert.

## 44. Debug

Vollständiger Debug-Build im selben Multi-Config-Build. Finale Nachweise: `build/f5x/debug-final.log`, `debug-tests.log`. Vorhandene LNK4075-/LNK4099-Warnungen sind keine neuen GLB-Produktfehler.

**48/48 PASS in 110,13 s**, Build Exitcode 0. Bestehender GR2Render-Test: 82,02 s. Anschließend derselbe erweiterte native Fenster-/GLB-GPU-Test gezielt erneut gebaut: **1/1 PASS, 5,55 s**, `window-debug-build.log`, `window-debug-tests.log`, `window-debug-details.log`.

## 45. GCC/LP64

Cygwin GCC 12/x86_64, portabler Core, Client/Renderer OFF: **24/24 PASS in 4,35 s** nach erfolgreichem vollständigem Core-Build. Logs: `build/f5x/gcc-final-build.log`, `gcc-final-tests.log`. Bekannte GCC-12-Optimiererwarnungen zu Vector-Initializer-Lists werden nicht als beseitigt dargestellt.

## 46. Unit Tests

GlTFCharacter ergänzt die vorhandenen Provider-/Runtime-Tests. Prüft Skeleton, Order/Parents, Bind-Palette, TRS-/Quaternion-Konversion, Forest, animierte Vorfahren, Clips/Dauer, Loop-Mapping, alle drei Geschwindigkeiten, Übergänge, Attachment-IDs, Eigentum und Fehlerfälle. Keine Fuzzer oder große Asset-Corpus-Läufe.

## 47. GPU Validation

GlTFCharacterRender verwendet bestehende Consumer, DiligentActorRenderer und den vorhandenen SkinningGpuReadback-Harness. Alle Vertices beider Primitives werden für sechs unterschiedliche Clip-Posen gegen Double-Referenzmathematik geprüft: Positionstoleranz 0,002 native Einheiten, Normalentoleranz 0,0001. GPU-Palette wird zusätzlich exakt zurückgelesen. Referenzdeformation existiert nur im Test.

## 48. Visual Result

Native GPU-Screenshots für sechs Clips und das Attachment wurden tatsächlich gelesen. Geprüft: aufrechte Orientierung, plausible native Größe, Textur/Materialgruppen, bewegte Gliedmaßen, gefallene Endpose, keine Bone-Explosion, kein invertiertes Mesh. Faceplate und Füße markieren die +Z-Source-/−Y-Runtime-Vorderseite. Finale native Client-Screenshots werden in der Abschlussmatrix verlinkt. Keine persönliche Benutzerabnahme behauptet.

Auch die finalen echten Client-Ansichten aller sechs Bewegungen und der Mehrinstanzenszene aus `native-07` wurden gelesen. Direkte Belege: [Walk](../../build/f5x/runtime/native-07/f5x-native-1-0915_114534.jpg), [Attack](../../build/f5x/runtime/native-07/f5x-native-4-0915_114539.jpg), [Death](../../build/f5x/runtime/native-07/f5x-native-7-0915_114544.jpg), [20 Actors](../../build/f5x/runtime/native-07/f5x-native-8-0915_114545.jpg), [RightHand-Prop](../../build-hx-clean/tests/AssetRuntime/f5x-hand-attachment.bmp). Bilder bleiben lokale, ignorierte Testartefakte.

## 49. Multi-Instance Visual Result

`f5x-20-actors.bmp` zeigt gleichzeitig 20 Characters mit verschiedenen Clip-Posen einschließlich Death. Der echte Client-Smoke erzeugt ebenfalls 20 Actors. Pose-Speicher und Playback-Controls sind getrennt, Modell-/Clip-/Texture-Definitionen gemeinsam.

## 50. Resize/Minimize

**PASS in Release und Debug:** Der native GPU-Harness ändert sein eigenes Win32-Fenster auf 720×480, minimiert es (IsIconic=true), restauriert es (IsIconic=false), setzt Fenster/Backend auf 960×640 und zeichnet danach erneut den animierten Character. [Post-Restore-Bild](../../build-hx-clean/tests/AssetRuntime/f5x-window-restored.bmp) wurde gelesen. `window-*-details.log` protokolliert `NativeWindow resize=720x480 minimized=1 restored=1 final=960x640 postRestoreAnimatedDraw=PASS`.

**Nicht als PASS gewertet:** Externe Fensterbedienung des vollständigen Clients. `native-04/05` fanden das versteckte Fenster nicht; nach korrigierter Null-Parameterübergabe verweigerte Windows in `native-06` die externe Fenstersteuerung beziehungsweise den Prozesszugriff. Die fehlgeschlagenen Runner wurden nicht zur Abnahme verwendet. Die tatsächlichen Fensteraktionen werden deshalb innerhalb des eigenen nativen Testprozesses nachgewiesen. Persönliche Benutzerbedienung wurde nicht durchgeführt.

## 51. Shutdown

Jeder erfolgreiche native Lauf muss Exitcode 0, AssetDocuments/AnimationInstances/MeshBindings/RuntimeSkeletons/RuntimeAnimationClips 0 und CPU-Deformation/Fallbacks 0 liefern. Das GPU-Harness prüft außerdem Mesh-/Remap-/Palette-/Geometry-/Texture-Freigabe. Ein früher Sandbox-Startfehler 0xc0000142 ist als nicht gestarteter Lauf erfasst.

## 52. Resource Lifetime

Finaler vollständiger nativer Character-Lauf **`native-07`**: **9 Stufen, 9 Screenshots, 954 Frames in 17,49 s, Exitcode 0**, 6.712 GPU-Actor-Frames und 8.059 unabhängige Pose-Samples. 194 Vegetationsinstanzen, 49.080 Vegetations-Draws und 28 LOD-Wechsel. AssetDocuments, AnimationInstances, MeshBindings, RuntimeSkeletons, RuntimeAnimationClips, SourceTextures/SourceBuffers, SkinMeshes/BoneRemaps/BonePalettes, PrototypeGeometry/PrototypePalettes und sämtliche Vegetationsressourcen jeweils **0**. AllCPUDeformationCalls/Vertices, GPUFallbacks, SkinPreparationFailures, AnimationRuntimeFailures, VegetationFailures jeweils **0**. `log/syserr.txt` ist leer.

Laufverzeichnis: `build/f5x/runtime/native-07`; Zusammenfassung `build/f5x/native-07.log`. Starter verwendet ausschließlich `--renderer-diagnostics`; Diligent D3D11, GPU Skinning, ZiiNAN GR2 Reader, ZiiNAN Animation und Vegetation sind laut `renderer-startup.log` alle Production Defaults. Der kopierte finale Release-Client ist SHA256-identisch zum Build: `7886f10c48d05b4094281a491439e5f4febd5fa7a7bc31856ed24f69cce0b107`. Frühere native Zwischenläufe ersetzen diesen Nachweis nicht.

## 53. Performance Sanity

GCC Release-Core: 2.400 Posen für 20 Actors in **2,8946 ms** im lokalen Testharness; Windows Release **6,7095 ms**. Das sind kurze Plausibilitätsmessungen, keine Client-FPS-Benchmarks. Ein gemeinsamer GPU-Meshbuffer-Satz; keine 20 Mesh-Decodes oder Texturladungen. Kein Langzeit-/Stresstest.

## 54. Known Limitations

Die oben genannten Exportgrenzen sind bewusst explizit. Kein tatsächlich ausgeführter Blender/bpy-Export, keine Retarget-/Morph-/CUBICSPLINE-/Gameplay-Event-Unterstützung und keine visuelle Remaster-Qualität. Bekannte Buildwarnungen bleiben. Anfangs blockierten Sandbox-Rechte Windows-SDK-Ermittlung, Cygwin-Objektzugriff und den Clientstart; die erforderlichen Builds/Läufe wurden mit installiertem Toolchain-Zugriff ausgeführt. Zwischenfehler des Testharness sind keine bestandenen Gates.

## 55. Git Diff

Produktiv geändert: GlTFProvider, gemeinsam genutzte RuntimeAnimationInstance, optionale sphärische Quaternion-Interpolation und neutrale MotionClipIndex-/Attachment-ID-Anbindung. Tests/Fixture/Runner/Bericht ergänzen den Nachweis. Renderer-/GPU-Skinning-Code, Shader, GR2-Formatreader und produktive Assets bleiben ohne F5-X-Änderung. Vorhandene lokale Änderungen im separaten Runtime-Checkout wurden nicht übernommen. Keine Build-/Log-/Screenshot-/PDB-Dateien stagen. Kein Commit/Push.

Abschluss: **16 geänderte vorhandene Dateien, 19 neue Dateien** einschließlich 39,8-KB-Character, 1,8-KB-Prop und kleiner Definitionen. `git diff --check` PASS; Index leer, nichts gestagt. Alle Testausgaben liegen in ignorierten Build-Verzeichnissen.

| Bereich | Dateien |
|---|---|
| Gemeinsame Animation | `src/AssetRuntime/RuntimeAnimationInstance.h`, GR2-Adapter, `src/AnimationRuntime/AnimationRuntime.*` |
| GLB/Neutrale API | `src/AssetRuntime/GlTF/GlTFAssetProvider.cpp`, `AssetRuntime.*`, AssetRuntime-CMake |
| Motion-Slots | `src/GameLib/RaceMotionData.*`, `ActorInstanceData.cpp`, `ActorInstanceMotion.cpp`, `src/EterGrnLib/ThingInstance.*` |
| Beweise | GlTFCharacter-/GlTFCharacterRender-Tests, Fixture-Generator/GLBs/MSA/MSM, nativer Test-Root/Runner, E1-X-/E2-X-Testanpassungen, dieser Bericht |

## 56. GO/NO-GO

**F5-X: GO innerhalb des dokumentierten GLB-Importvertrags.** Echte Skeleton-/TRS-Clips gehen durch dieselbe ZiiNAN Animation Runtime und denselben produktiven GPU-Skinning-/Diligent-Pfad wie GR2. Kein zweiter Animator, keine Granny-/SpeedTree-Abhängigkeit, keine Bestandsmigration.

| Nachweis | Abschluss |
|---|---|
| E1-X Baseline vor Änderungen | 6/6 PASS |
| GCC/LP64 | 24/24 PASS |
| Release komplett / FAST GATE | Build 0, 48/48 PASS; erweiterter GPU-Fenstertest zusätzlich 1/1 PASS |
| Debug komplett / FAST GATE | Build 0, 48/48 PASS; erweiterter GPU-Fenstertest zusätzlich 1/1 PASS |
| Nativer finaler GLB-Client-Smoke | 20 Actors, 6 Clips, 954 Frames, 17,49 s, Exit 0 |
| Palette/GPU-Parität/Attachment/Sharing | PASS, 40 Draws/20 Actors, 1 gemeinsamer Meshbuffer-Satz, 1 Character-Textur |
| Native Fensteraktionen im GPU-Harness | Resize, Minimize/Restore, animierter Draw danach: PASS Release/Debug |
| Kurzer finaler GR2-/Vegetations-Smoke | 3.555 Frames, 62,9 s, Exit 0, PASS |
| Shutdown / CPU-Deformation / Fallbacks | Alle erfassten Ressourcen 0 / 0 / 0 |
| Git-Diff | PASS, nichts gestagt, kein Commit/Push |
| Blender/bpy-Export / persönliche Benutzerabnahme | NOT RUN; eigenes deterministisches GLB-Fixture und geprüfte Screenshots |
| Externe Fenstersteuerung des vollständigen Clients | Nicht bestanden; Windows-Zugriff verweigert, durch direkten nativen Fenster-Test ergänzt |

Kurze Reproduktion im vorhandenen Windows-Build: `cmake --build build-hx-clean --config Release --parallel 6` beziehungsweise `Debug`, danach:

```powershell
ctest --test-dir build-hx-clean -C Release -j 2 -R '^(AssetRuntime|AnimationRuntime|Vegetation|AssetTool|Platform)\.|^Renderer\.(StartupOptions|DiligentD3D11|ProductionGpu|NoLegacyArchitecture|HairLodQuick)$' --output-on-failure
# Gleicher Gate-Umfang mit -C Debug; portabler Core:
ctest --test-dir build-hx-common -C Release --output-on-failure
& tests/AssetRuntime/run_character_smoke.ps1 -Name fresh-f5x -BuildDirectory build-hx-clean
& tests/AssetRuntime/run_gr2_smoke.ps1 -Name fresh-f5x-regression -BuildDirectory build-hx-clean -ProductionDefault
```

Beide Runner verlangen einen neuen Namen und erzeugen ein privates Test-Root; die native Runtime-/Pack-/Locale-Basis und die kompilierten H-X-Vegetationsassets müssen vorhanden sein. Kein Login-/Server-Test. Keine komplette Renderer-Gesamtsuite, Fuzzer, Asset-Batch-Migration oder Langzeitsession ausgeführt.

## 57. Recommendation Visual Remaster

Ein späterer eigener Milestone kann auf diesem Character-Importvertrag aufbauen. F5-X selbst endet nach Tests, Runtime-/Visual-Proof und GO/NO-GO. Keine Player-/Mob-Migration, kein PBR/Lighting-Remaster, Android, World Editor oder FPS-Menü in diesem Auftrag.

Die Architektur erlaubt, einen neuen Character in Blender zu erstellen, unter den dokumentierten Grenzen als GLB zu exportieren und per MSM/MSA-Clip-Mapping direkt im Client zu verwenden, ohne GR2 oder Granny. Die tatsächliche Format-/Runtime-Kette ist mit dem eigenen GLB nachgewiesen; ein konkreter Blender-Export bleibt separat zu prüfen. **STOP nach F5-X.**
