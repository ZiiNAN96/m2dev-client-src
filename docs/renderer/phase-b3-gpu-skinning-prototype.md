# Phase B3 – GPU-Skinning-Prototyp des Referenz-Kriegers

Stand: 13.09.2026. Basis: B2, Source-HEAD `d2b1bd2ee6f66f4f66e8b0e37b248e3d5a31e48e`.

## 1. Umfang und Status

Implementiert: eng begrenzter Diligent-D3D11-GPU-Skinning-Testpfad. **CPU bleibt Standard.** Keine B4-Arbeiten, keine globale Actor-Migration, keine Änderung an Granny, Animation-Sampling, CPU-Deformer, Materialien oder Lighting.

Release-/Debug-Parität, sichtbare CPU-/GPU-Kartenläufe und regulärer Mount-Rückfall sind bestanden. Ein bereits bestehender Fehler des isolierten Python-Absteigehelfers wurde beim erweiterten Test gefunden; Details und Abgrenzung in Abschnitt 26.

## 2. Architektur und Änderungspunkte

~~~text
WinMain → StartupOptions (--skinning=cpu | gpu-prototype)
GetAnimatedActorParts → eng begrenzte Referenzfreigabe
CGrannyModelInstance::Deform
  → unverändert UpdateWorldPose / UpdateWorldMatrices / B2-Palette
  → Referenz + Opt-in + erfolgreiche PreparePrototype:
       immutable PWNT + originale Indizes + aktuelle Bone-CB
       → SkinningVS → bestehende VS-Lightingfunktion → unveränderter PS
  → sonst unverändert DeformPNTVertices → dynamischer PNT-Upload
ActorRenderBridge::Submit → unveränderte Material-/World-/Submeshparameter
~~~

Betroffene Produktion: `StartupOptions.h`, `UserInterface.cpp`, `ActorRenderData.h`, `ActorInstance.h`, `ActorRenderBridge.cpp`, `ModelInstanceUpdate.cpp`, `DiligentActorRenderer.h`, `DiligentStaticObjectRenderer.h/.cpp`. Neu: `GpuSkinningPrototype.h`, `GpuSkinningShader.h`. Keine neuen Graphics-APIs außerhalb des vorhandenen Diligent-Renderers.

## 3. Exakte Referenz

- Actor-Typ Player, Race **0**, Shape **0**, nicht beritten.
- Asset: `d:/ymir work/pc/warrior/warrior_novice.gr2`; Pfadvergleich ignoriert ASCII-Großschreibung und Slashrichtung, akzeptiert keine anderen Assets.
- Native Model-/Skeleton-Name **Bip01**, **75 Bones**, vollständige Instanzpalette mit **75 Matrizen**. B2-Skeleton-Identität und tatsächliche Instanz-Bindings werden erhalten.
- Originaldatei SHA-256: `7AC881B0C60CED40C9388D6186BDF02A353A58382D12804345CB65FC3DC1FA74`.
- Nur das vollständige Referenzmodell: drei vorbereitete deformierbare Meshes, 2.207 Vertices, 6.804 Indizes. Andere Races/Shapes, Rüstungen und reduzierte LODs bleiben CPU.
- Der Modus ist kein VID-spezifischer Cheat: weitere Instanzen desselben eng definierten Referenzassets können eigene Paletten besitzen. A → B → A wird ausdrücklich getestet.

## 4. Meshes und Palette

| Mesh | Vertices | Indizes | Mesh-Bindings | Deform-Offset |
|---|---:|---:|---:|---:|
| warrior_body02 | 1.669 | 4.902 | 72 | 0 |
| Object05 | 258 | 900 | 72 | 1.669 |
| face | 280 | 1.002 | 72 | 1.927 |
| Gesamt | 2.207 | 6.804 | Vollpalette: 75 | |

Originale Gruppen, BaseVertex und Indexbereiche bleiben erhalten. Originaltexturen im Vergleich: `warrior_novice_red.dds` und `warrior_face.dds`.

## 5. Startauswahl

~~~text
Metin2_Release.exe                         → CPU
Metin2_Release.exe --skinning=cpu          → CPU
Metin2_Release.exe --skinning=gpu-prototype → nur Referenzkörper GPU
~~~

`WinMain` setzt den Modus genau einmal vor Gerät/Spielstart. Kein UI-Schalter, kein produktives Hot-Switching. Unbekannte oder widersprüchliche Werte sind Startfehler, kein stiller Fallback. Der gewählte Modus steht einmal in `renderer-startup.log`. Der Renderer bleibt entsprechend Phase A ausschließlich Diligent D3D11; ein D3D9-/OFF-Build existiert seit M13C nicht mehr.

## 6. Statischer GPU-Vertexbuffer

Die Referenzgeometrie wird bei erster erfolgreicher Vorbereitung einmal pro Instanz/LOD als immutable Buffer erstellt: **88.280 Byte**. Keine CPU-deformierten Vertices dienen dem Skinning-VS als Eingang. Bei weiteren Posen wird nur die Knochenpalette aktualisiert. Eine globale Mesh-/Palette-Optimierung oder ein Crowdsystem wurde nicht eingeführt.

## 7. Input-Layout

Stride **40 Byte**, C++-Alignment **4**; identisch zu B2-PWNT, nicht zum bisherigen deformierten PNT mit 32 Byte.

| Attribut | Diligent-Slot | Offset | Typ | HLSL |
|---|---:|---:|---|---|
| Position | ATTRIB0 | 0 | 3 × FLOAT32 | float3 |
| Weights | ATTRIB3 | 12 | 4 × UINT8, nicht normalisiert | uint4 |
| Indices | ATTRIB4 | 16 | 4 × UINT8, nicht normalisiert | uint4 |
| Normal | ATTRIB1 | 20 | 3 × FLOAT32 | float3 |
| UV | ATTRIB2 | 32 | 2 × FLOAT32 | float2 |

Die vier Gewichte sind explizite Originalbytes und werden im Shader mit `1/255` multipliziert. Keine Gewicht-Neunormalisierung und kein implizites viertes Gewicht.

## 8. Indizes und Remapping

B2-Daten werden nicht verändert. Nur die temporäre Uploadkopie übersetzt aktive Mesh-Bone-Indizes mittels des bereits verifizierten `BoneRemap` in Ziel-Skeleton-Indizes. Gewicht-0-Lanes werden in dieser Kopie auf Index 0 gesetzt. Aktive Indizes müssen innerhalb der tatsächlichen Palette liegen.

Der immutable Indexbuffer enthält die bestehenden **6.804 originalen uint16-Indizes**, **13.608 Byte**. Der Test vergleicht ihn mit `ActorModelSource::indices`, einschließlich Mesh-Reihenfolge. Kein Neuaufbau der Topologie, keine pro-Frame-Indexuploads.

## 9. Bone Constant Buffer

Pro Referenzgeometrie eine eigene dynamische Uniform-CB, **16.384 Byte**, `Bones[256]`. Der technische Akzeptanzbereich ist **1–163** tatsächlich vorhandene Bones. 75 ist nur die Referenz-Assetkennung, keine allgemeine Shader-Palettengröße.

Jede Matrix ist 64 Byte, jeder Matrixanfang damit 16-Byte-konform. Pro erfolgreicher Vorbereitung: MAP_WRITE/DISCARD, gesamte CB nullen, aktuelle tatsächliche Matrizen kopieren. Beim Krieger sind **4.800 Byte** Pose-Nutzdaten enthalten; tatsächlich geschrieben und gezählt werden bewusst **16.384 Byte**, nicht nur 4.800. Die Nullfüllung des Restes vermeidet uninitialisierte CB-Daten. Keine Behauptung über den tatsächlichen internen Treiber-/PCIe-Verkehr.

## 10. Matrixkonvention

Unverändert B1/B2: Granny-Composite aus `GrannyGetWorldPoseComposite4x4Array`, row-major, Row-Vektoren. CPU-Matrixbytes werden ohne Transposition kopiert; HLSL deklariert `row_major float4x4` und verwendet `mul(float4(position,1), Bones[index])`.

Die gewichtete Position liegt danach im bisherigen deformierten Modellraum. Der vorhandene World/View/Projection-Pfad wird genau einmal anschließend angewandt. Keine doppelte Actor-/Parent-Transformation.

## 11. Shader und Normalen

Separater VS-Einstieg `SkinningVS`; `SkinVertex` gewichtet Positionen mit w=1 und Normalen mit w=0. Die Normalen erhalten keine Translation, keine neue Bone-Inverse-Transpose-Operation und keine neue Skinning-Normalisierung. `precise` begrenzt Reassoziation im Gewichtssummenpfad; bitidentische GPU-/CPU-Arithmetik wird nicht unterstellt.

Danach dieselbe vorhandene VS-Funktion einschließlich NormalTransform und bisherigem `normalizeNormals`-Flag. Der Pixelshader bleibt bytegleich im Source. Der finale Vergleich prüft zusätzlich nichtuniforme Actor-Skalierung `(1.3, 0.7, 1.1)`, Z-Rotation 0,37 rad und Translation `(17,-9,4)`.

## 12. Materialien, PSOs und Bindings

Die zwölf bestehenden Cull-/Blend-/DepthWrite-Varianten werden für den Skinning-VS spiegelbildlich erstellt. Unverändert: Pixelshader, Texturen/Sampler, Materialfarbe, Alpha-Test, SRCALPHA/INVSRCALPHA, Fog, Specular-/CameraAlpha-Parameter und vorhandene Beleuchtung.

GPU-Skinning verwendet pro Materialdraw ein kurzlebiges SRB mit explizit gesetzter Instanz-Bone-CB. Es wird nicht im gemeinsamen Textur-SRB-Cache abgelegt. Damit kann eine Textur keine Palette einer anderen Instanz festhalten. Kein zusätzlicher globaler Reset pro Draw. Der A → B → A-Test verlangt für A nach B exakt dieselben RGB-Bytes.

## 13. CPU-Pfad und Auswahl/Kollision

Nur nach erfolgreichem `PreparePrototype` überspringt der Referenzkörper `DeformPNTVertices`, die PNT-Kopie und den dynamischen Vertexupload. Bei CPU oder Rückfall läuft der bisherige Pfad weiter. Ein vorheriges GPU-Geometryhandle wird beim Rückfall verworfen, sodass kein CPU-Upload einen PWNT-Buffer trifft.

`ModelInstanceCollisionDetection.cpp` verwendet weiterhin Bone-OBBs und aktuelle WorldPose; es liest nicht den übersprungenen deformierten Vertexbuffer. Animation, Bounding- und Picking-Pose bleiben vorhanden. Die vorhandene CPU-Deformbuffer-Reservierung wurde nicht zusätzlich entfernt.

## 14. Rückfall und negative Prüfungen

Bevorzugter Rückfall: CPU, mit einmaliger instanzbezogener Diagnose im aktivierten Prototypmodus. Kein per-Frame-Log.

Geprüft: unbekannte Startupwerte, fehlende Palette, falsches Modell, Nonfinite-Matrix, Überschreitung 163, Referenz → Rüstung → Referenz, Freigabe des alten GPU-Handles und CPU-Haar mit derselben aktuellen Körperpalette. B2 prüft unverändert Originalweights, aktive Bone-Indizes, Nullgewichte, Ziel-Bindings und Skeletonwechsel.

## 15. Animationen und identische Posen

Reale Originalclips: `general/wait.gr2`, `walk.gr2`, `run.gr2`, `attack.gr2`. Je Clip Zeitstempel **0 / 0,17 / 0,37 / 0,63 s**. CPU und GPU sind getrennte native Instanzen mit identischen Clips, Clock, Worldmatrix und Kamera. Der Test verlangt identische gesampelte Composite-Matrizen vor dem Rendervergleich.

## 16. Numerische GPU-Parität

Ein ausschließlich im Test verwendeter Compute-Probe führt dieselbe `SkinVertex`-Funktion auf den originalen 40-Byte-PWNT-Bytes aus und liest Positionen/Normalen zurück. Das produktive Input-Layout und der eigentliche Skinning-VS werden zusätzlich durch den Rastervergleich geprüft; der Compute-Test allein wäre dafür kein Ersatz.

**35.312 Vertexvergleiche** je Build (16 × 2.207), alle Positionen und Normalen:

- maximaler Positionsfehler Release/Debug: **0,0000457764**;
- maximaler Normalenfehler Release/Debug: **0,000000178814**;
- Positionsgrenze: `1e-4 + 2e-6 * max(1, abs(CPU-Komponente))`;
- Normalengrenze: `1e-5`;
- NaN/Inf stets Fehler, kein Ausblenden ungünstiger Vertices.

Zusätzlich tatsächlicher GPU-Zugriff auf Bone **162** einer **163er-Palette**, einschließlich Translation und translationsfreier Normale. 164 wird vorher abgelehnt.

## 17. Pixelvergleich und Bilder

640×640 bzw. 800×800, dieselben Originaltexturen und derselbe bestehende Material-/Lichtpfad. JPEG-Artefakte bei 0,37 s für Idle, Walk, Run und Attack, je CPU/GPU; verglichen werden die **unkomprimierten RGB-Readbacks**, nicht verlustbehaftete JPEG-Dateien.

Toleranz vorgegeben im Test: höchstens 1 % abweichende Farbkanäle; höchstens 0,01 % Kanäle mit Differenz >2. Gemessen im finalen nichtuniform skalierten Test: höchstens **41** geänderte Kanäle pro Bild. In zwei Walk-Posen jeweils **3** Kanäle >2 (ein Pixel), maximale Differenz **24/255**; alle anderen Posen maximal **1/255**. Das ist keine bitexakte Rastergarantie. Die zuvor unskalierten 16 Posen lagen alle bei maximal 1/255.

Idle-/Run-/Attack-Bilder wurden zusätzlich visuell angesehen: kein erkennbarer Versatz von Silhouette, Gliedmaßen, Texturen oder Beleuchtung. Sichtbare Clienttests wurden vom Nutzer separat bestätigt.

## 18. Performance: eng begrenzte Messung

Keine Optimierung und kein FPS-/Crowd-Benchmark. Native Animation/Pose-Sampling bleiben auf beiden Seiten unverändert. Referenzbezogene Zähler werden nur einmal beim Shutdown ausgegeben.

| Messung | CPU-Referenzlauf | GPU-Referenzlauf |
|---|---:|---:|
| Erfolgreiche Referenz-Deforms | 6.888 | 6.855 |
| CPU-Skinning gesamt | 101.515 µs | für GPU-Referenz übersprungen |
| Mittel pro Referenz-Deform | 14,74 µs Skinning | 4,80 µs GPU-Vorbereitung |
| GPU-Vorbereitung gesamt | – | 32.905,3 µs |
| dynamische CPU-PNT-Bytes pro Pose | 70.624 | 0 |
| geschriebene Bone-CB-Bytes pro Pose | 0 | 16.384 |
| bereitgestellte PNT-Bytes gesamt | 486.458.112 | 0 |
| Bone-CB-Bytes gesamt | 0 | 112.312.320 |

Die Runs sind separate 151-Sekunden-Läufe mit Benutzerinteraktion und leicht verschiedener Frameanzahl. Zeiten sind Diagnosewerte dieser Maschine, kein allgemeiner Geschwindigkeitsgewinn. Vorbereitung enthält Validierung, CB-Mapping/-Kopie und amortisierte Erst-Erstellung, nicht das gemeinsame Granny-Sampling.

Der Live-Zähler `CPUVertexBytes` zählt die fertig vorbereiteten Referenz-PNT-Bytes an der Deformationsstelle, nicht den gesamten Client-/Treiberverkehr. Den **tatsächlichen** dynamischen Renderer-Upload prüft der separate Rastertest: pro CPU-Pose 70.624 Byte, pro GPU-Pose 0; über 16 CPU-Bilder 1.129.984 Byte. Im GPU-Modus behalten andere Actors, Haare und CPU-Fallback-LODs ihre eigenen dynamischen Uploads; diese sind in den referenzbezogenen Zahlen nicht enthalten.

## 19. GPU-Kosten

Duration-Queries im finalen isolierten Release-Rastertest: CPU-PNT-Drawpfad **190,112 µs / 16** Messungen; GPU-Skinning-Drawpfad **222,080 µs / 24** Messungen. Die zusätzliche Anzahl entsteht durch A → B → A. Alle Queries verfügbar. Erfasst ist der Actor-Drawbereich, **nicht der gesamte Ingame-Frame**; Readbacks, synchronisierte Miniszene und parallele Systemlast verhindern belastbare Gameplay-/Performanceaussagen. Debug-Werte stehen separat im Log und werden nicht als Performancevergleich verwendet.

## 20. Ressourcenbesitz und Freigabe

VB, IB und eigene Bone-CB liegen im vorhandenen Instanz-Geometryhandle. B2-Remap/Skeleton werden für die Uploadidentität gehalten; Material-SRBs halten fremde Instanzen nicht dauerhaft fest. Shader/PSOs gehören dem Renderer. Die bestehende Clear-/LOD-/Map-Lifetime wird verwendet.

Unitprüfungen: Spawn, mehrere Posen und Materialien, zweite Instanz, A → B → A, Despawn, Shapewechsel, Haarfreigabe, Rendererabbau. Nach jedem Despawn: `PrototypeGeometry=0`, `PrototypePalettes=0`; nach Gesamtshutdown auch alle B2-/Actor-Owner 0.

Je erfolgreichem Weltrun wurden 31 Speicher-/Handleproben genommen. Private-Memory-Spitze / letzte Probe: CPU-Referenz **446,1 / 357,2 MiB**, GPU-Referenz **452,6 / 358,1 MiB**, breite CPU-Welt **427,4 / 356,5 MiB**, reguläre GPU-Übergänge **475,9 / 363,4 MiB**. Die Kurven sinken nach Karten-/Cachewechseln wieder; kein beobachteter monotoner Anstieg. Kein Langzeit-Leakbeweis aus vier kurzen Läufen. Zusätzlich sind die eigenen Owner am Ende jeweils 0.

## 21. Karten-/CPU-Regression

Der sichtbare Referenztest lädt A1 → B1 → A1 → Dungeon → Gildenkarte → A1, vier Originalanimationen, NPC/Mob/Pferdemodell, Welt und Schrift. CPU-Standard ohne Skinningargument: PID **67908**, Exitcode **0**, **151,3 s**, alle eigenen Ressourcen 0. Nutzer: dreimal Minimize/Restore und Darstellung bestätigt.

Zusätzlicher unveränderter breiter Welttest: PID **63604**, Exitcode **0**, **151,5 s**, sechs Phasen, Figuren mit Waffe/Haaren, NPC/Mob/Metin, Welt/Bäume/Wasser/Minimap/Atlas/Schrift; alle gezählten Source-/B2-/Prototyp-Owner 0. Keine erneute Live-Login-/Quest-/Serverprüfung beansprucht.

## 22. Sichtbare GPU-Regression

PID **46528**, `--skinning=gpu-prototype`, **151,5 s**, sechs Kartenphasen, Exitcode **0**, GPUFrames **6.855**, SkinPreparationFailures **0**, SourceTextures/SourceBuffers/SkinMeshes/BoneRemaps/BonePalettes/PrototypeGeometry/PrototypePalettes sämtlich **0** nach Shutdown.

`actor-renderer.log` belegt GPU-Referenzkörper, CPU-NPCs/Mobs sowie CPU-Fallback für reduzierte 897-/786-Vertex-LODs. Die Nahaufnahme wurde zusätzlich geprüft. Nutzer bestätigt Körper/Texturen/Animationen und dreimal Minimize/Restore.

## 23. Resize und Minimize/Restore

Numerischer/Rastertest: für alle 16 Posen Suspend `Resize(0,0)`, kein BeginFrame im suspendierten Zustand, Wiederaufnahme mit 640 oder 800 Pixeln und erneuter Pose-/Pixelprüfung. Bestehende ProductionGpu-/Backendtests prüfen weitere Resize-/Lifecyclepfade. Sichtbare CPU-/GPU-Welttests jeweils dreimal vom Nutzer minimiert/wiederhergestellt und bestätigt.

## 24. Builds und vollständige Tests

- Produktiver Release-Client: erfolgreich.
- Produktiver Debug-Client: erfolgreich.
- Gesamtsuite Release: **22/22**, **874,30 s**. Alle vorherigen 19 Tests + beide B2-Tests + neuer B3-Test; kein Test entfernt.
- Finale Renderer-/Skinning-Suite nach letzter Upload-Assertion: Release **18/18**, **16,10 s**; Debug **18/18**, **16,87 s**.
- B3-Native-/GPU-Test separat Release/Debug: Exitcode **0**, gleiche numerische Fehlermaxima.
- `git diff --check`: erfolgreich.

Vendor-Shelltests wurden unverändert mit Git-`usr/bin` im ausschließlich prozesslokalen PATH ausgeführt. Kein Cygwin-/Vendorcode-Fix. Kein D3D9-/OFF-Build neu eingeführt.

## 25. Validation und Warnungen

Der Debug-Test verlangt eine tatsächlich vorhandene `ID3D11InfoQueue`, prüft Native-Device-Status und scheitert bei D3D11-Warnungen/Fehlern. Ein zusätzlicher Diligent-Callback lässt Warnungen/Fehler ebenfalls fehlschlagen. Ergebnis: aktive Debug-Validation, **0 Warnungen/Fehler**. Buffergrößen, Layout, Palettenindizes, CB-Alignment und Nonfinite-Matrizen werden geprüft.

Die native Geometrie-Testfixture registriert wie der B2-Instanztest absichtlich keine Image-Resource-Factories. Deshalb meldet sie `NOT SUPPORT FILE` für die nativen Bildanforderungen; die Rasterfixture lädt und prüft dieselben Original-DDS separat. Die absichtlich abgelehnte Armor-Testvorbereitung erzeugt genau die erwartete CPU-Fallback-Diagnose. Das sind keine ignorierten Grafik-Validationfehler.

Buildmeldungen bleiben die bestehenden Hinweise: fehlende Vendor-PDBs (SpeedTree/Python/zlib-ng), LTCG/INCREMENTAL-/CRT-Verknüpfung sowie bestehende C4311/C4834-Stellen außerhalb B3. Keine neuen B3-Compilerwarnungen. Diese Bestandsmeldungen wurden nicht durch sachfremde Linkeränderungen unterdrückt.

## 26. Gefundener Testhelfer-/Bestandsfehler

Der erste erweiterte Test verwendete direkt `chr.MountHorse()` / `chr.DismountHorse()`. Nach dem Absteigen: GPU PID **64736**, Exit **-1073741819 (0xC0000005)** bei 146,4 s. Derselbe Ablauf mit **CPU-Default**: PID **69344**, identischer Exit **-1073741819**, 146,0 s. Kein GPU-Skinning erforderlich, um den Fehler auszulösen.

Source-Ursache: `CInstanceBase::DismountHorse()` zerstört lediglich `m_kHorse`; der lebende `m_GraphicThingInstance` behält den von `MountHorse()` gesetzten `m_pkHorse`. Im vollständigen Instanz-Destroy wird anschließend auch der Actor zerstört, nicht aber beim isolierten Python-Helfer. Der reguläre `CNetworkActorManager::__AppendCharacterManagerActor` löscht/erstellt die Instanz beim Mountstatuswechsel. Die direkten Update-Aufrufe sind dort bereits auskommentiert.

**Kein produktiver Mount-Code wurde geändert.** Die finale Fixture bildet den vorhandenen Netzwerk-Instanzwechsel am selben VID ab, wie bereits die 5D-Fixture. Die beiden fehlgeschlagenen Versuche bleiben dokumentiert, werden nicht als erfolgreiche Shutdown-/Lifetimeprüfungen gezählt und nicht stillschweigend aus der Evidenz gelöscht. Eine allgemeine Reparatur des alten Python-Helfers wäre ein separater Auftrag.

## 27. Weitere Startgrenzen und reguläre Abnahme

Ein erster CPU-Fixture-Versuch konnte die gerade noch gelinkte EXE nicht kopieren; es wurde kein Client gestartet. Wiederholung nach beendetem Build erfolgreich.

Der zusätzliche Debug-Weltprozess wurde von Windows vor Start abgebrochen, ohne PID/Exitcode eines Clients. **Kein bestandener Debug-Ingame-Welttest wird behauptet.** Der produktive Debug-Build, alle 18 Debug-Tests und echte Debug-GPU-Rücklese-/Resize-/Shutdownprüfungen sind dagegen durchgeführt.

Finaler regulärer Mount-/Shape-/Haar-Übergangslauf: PID **26092**, **151,4 s**, alle sechs Kartenphasen und fünf zusätzliche Übergänge, Exitcode **0**. Das Protokoll zeigt Referenz-GPU → Shape-CPU → Referenz-GPU → Reiter-/Mount-CPU → Referenz-GPU; rigide Waffe bleibt im vorhandenen Attachmentpfad. SkinPreparationFailures **0**, alle eigenen Source-/Skinning-/Prototyp-Ressourcen **0**. Der separate native Test bestätigt CPU-Haar an der GPU-Körperpalette. Die Fixture aktiviert den vorhandenen Haarparameter; fehlende unregistrierte Haarvarianten werden daraus nicht als zusätzliche GPU-Haarfreigabe abgeleitet.

## 28. Git-Diff und unveränderte Grenzen

Elf vorhandene Code-/Testdateien gezielt erweitert (**172 Ergänzungen, 17 ersetzte/entfernte Zeilen**); sechs neue Code-/Test-/Fixture-Dateien (**676 Zeilen**) plus dieser Bericht. Keine Bearbeitung von Granny/SDK, `Deform.cpp`, Bone-Sampling-/Blendinglogik, Netzwerk, Mount-Implementierung, Terrain, UI-Rendering, Effekten oder Materialien. `Deform.cpp`-SHA-256 weiterhin `C73674782ADA9D554237F1522605ABA3BFCBCBFF66FAB14E2FD34545B1FC9DE7`.

Keine Repository-/Assetbereinigung, kein Commit/Push/Reset/Restore/Stash. Private EXE-/Paketkopien und Logs ausschließlich unter `build/phase-b3`; originale Clientpakete unverändert. Die vorhandene Runtime-Änderung `config/channel.inf` gehört nicht zu B3.

## 29. Reproduzierbarkeit und Evidenz

Neue Tests: `tests/Renderer/SkinningPrototypeTest.cpp`, `SkinningGpuReadback.h`, Startup-Erweiterungen; Fixture `gpu_skinning_entry.py`, Starter `run_skinning_prototype.ps1`. Echte Assetdateien sind Pflicht; fehlende Dateien sind Fehler, kein Skip.

~~~powershell
cmake --build build --config Release
cmake --build build --config Debug
ctest --test-dir build -C Release -R '^Renderer\.' --output-on-failure
ctest --test-dir build -C Debug -R '^Renderer\.' --output-on-failure
./tests/Renderer/run_skinning_prototype.ps1 -Name new-cpu-run -Skinning cpu -Visible
./tests/Renderer/run_skinning_prototype.ps1 -Name new-gpu-run -Skinning gpu-prototype -Visible
~~~

Lokale Logs: `build/phase-b3-full-release.log`, `phase-b3-debug-renderer-final.log`, `phase-b3-prototype-release-final.log`, `phase-b3-prototype-debug-final.log`, Release-/Debug-Buildlogs mit Präfix `phase-b3`. Vergleichsbilder: `build/phase-b3/parity-Release` und `parity-Debug`. Pro Weltrun: `exit.txt`, `resources.csv`, `source-resource-audit.log`, Actor-/Terrain-/Weltlogs, Screenshots und Phasenlog. Frühere Milestone-Abnahmen werden nicht als aktuelle B3-Beweise gezählt.

Alle in B3 gestarteten Testclients sind beendet. Der installierte normale Client wurde nicht durch eine private Test-EXE überschrieben.

## 30. GO / NO-GO und STOP

**B3 GO für den beschriebenen Referenz-Prototyp.** CPU bleibt Default. Numerische und sichtbare Referenzparität, originaler CPU-Rückfall, vollständige Tests, regulärer Mountwechsel und Ressourcenfreigabe sind nachgewiesen. Der alte isolierte Python-Absteigefehler bleibt ausdrücklich außerhalb dieser Freigabe.

**Technische Grundlage für einen separat beauftragten B4-Schritt: GO**, keine pauschale Freigabe beliebiger Assets oder eines GPU-Produktionsdefaults. Kein B4 implementiert. **STOP nach B3.**
