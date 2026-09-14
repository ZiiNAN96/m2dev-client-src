# F3/4-X – Production Switch + Complete Granny Removal

Stand: 2026-09-14, Neustart nach F3-B. **Gate A GO. Gate B GO. F3/4-X abgeschlossen.** Kein Commit/Push.

## Ausgangslage und aktuelle Coverage

Source-Baseline `961401a09297a0f0d7504595681b3bf53eb7c134`, zu Beginn sauber. Der ältere F3/4-Versuch hatte vor F3-A/B 52 Rejects und 27 Produktionsblocker; dessen Baseline-/Klassifikationsdateien bleiben erhalten. Maßgeblich ist jetzt [F3-B](phase-f3b-redthief-animation-repair.md).

Neuer Corpus-Scan: **9.160 / 9.166 parsed, sechs malformed**, 40,108 s. Alle 52 in F3-B klassifizierten Quelldateien besitzen weiterhin denselben SHA256. Damit bleiben die Klassifikation `production_referenced_unsupported=0` und die nicht pauschal aufgelösten 20 Nutzungseinordnungen nachvollziehbar. Keine Finite-Prüfung oder Reader-Semantik verändert. Lokale Evidenz: `build/f34-restart/corpus/`, `f3b-input-verification.json`.

## Gate A: Production Default / No Fallback

`GR2ReaderMode.h`, `AnimationRuntimeMode.h` und `Renderer/StartupOptions.h` verwenden ZiiNAN als Default. Die Startdiagnose protokolliert auch `GR2ReaderSelection=default`. Granny ist während Gate A ausschließlich explizit auswählbarer Entwicklungs-/Referenzpfad. Widersprüchliche Optionen werden abgewiesen; beide Reihenfolgen der ausdrücklichen Granny-Auswahl sind getestet.

Der vorhandene `Providers.cpp`-Dispatch bleibt ein einzelner Provideraufruf. Ein ungültiges `.gr2` liefert einen Fehler mit Diagnose, ohne einen Granny-Read. `GR2RenderTest` verwendet jetzt den normalen Dispatch, keine explizite Native-Provider-Auswahl.

## Golden Reference Preservation

Neue SDK-freie Tests `AssetRuntime.GR2Golden.static` und `.animation`; während Gate A getrenntes, nur ausdrücklich gebautes `GR2GoldenExport` (mit Gate B entfernt). Die Erwartungen stammen vom Granny-Provider und seiner expliziten Granny-Poseauswertung, nicht vom getesteten nativen Reader. Tests können sie nicht neu erzeugen oder überschreiben.

- Static: Warrior, Hair, Weapon, Wolf, Boss, Mount, Building, Prop; Counts, vollständige Bone-Namen/Reihenfolge/Parents, ausgewählte Bind-Transforms, Material-/Skin-Mappings und Vertex-/Index-Hashes.
- Animation: Warrior Idle/Walk/Run/finite Attack, Wolf, Boss, Mount; Dauer, Loop-/Clamp-Vertrag, ausgewählte Welt-/Palettewerte und finale Vertexpositionen/-normalen an sechs Zeitpunkten.
- F1-X-Grenzen unverändert: Welt/Palette 2e-3, Position 5e-3, Normale 5e-5. Bestehende Live-Parität prüft während Gate A zusätzlich lokale Translation/Rotation/Scale und vollständige Vertexsets.
- 3.835 statische und 6.075 Animationswerte/Records, zusammen 103.037 Bytes. Keine GR2-Datei dupliziert. Vollständige Beschreibung: `tests/AssetRuntime/fixtures/gr2-golden-README.md`.

## Player / Actor / Attachments / Static World

`GR2RenderTest`: Warrior, Assassin, Sura, Shaman, jeweils PC/PC2 und Novice/4-1-Rüstung, Haar, Idle/Walk/Run/Attack/Hit/Transitions. Klassenspezifische Waffen (einschließlich beider Assassin-Hände) und Combo_01/02 bestehen im ergänzten Release-/Debug-Nachtest. Wolfman-Assets im vorhandenen Corpus nicht gefunden; keine erfundene Wolfman-Freigabe. Die sieben Sura-Clips mit leeren Model-Records bleiben ausdrücklich geprüft.

Native Bone-Matrizen sind vor dem ersten Frame gültig und bleiben beim Motionwechsel gültig. Native Hair-Near/Far/Near-, Weapon-, Wolf-, Boss-, NPC-/Horse- und statische Building-GPU-Pfade sind geprüft. Der originale Client-Smoke enthält außerdem echten Mount/Rider, Hair/Weapon sowie protokollierte Static- und Camera-Blocker-Submissions. Kein zusätzlicher sichtbarer Shield-GR2-Pfad wurde behauptet.

Die Collision-Freigabe steht unverändert nach dem World-Teardown in `CPythonApplication::Destroy`. Der normale Character-Select-Pfad wurde separat vom Benutzer bestätigt.

## Release / Debug / GCC Fast Gate

| Gate | Ergebnis |
|---|---|
| Release-Build | PASS, inkrementell |
| Release-Auswahl | 30/31 zunächst PASS; alter Hair-Referenztest abgestürzt; nach expliziter Referenzauswahl gezielter Nachtest 1/1 PASS (6,36 s) |
| Debug-Build | PASS, inkrementell |
| Debug-Auswahl | 31/31 PASS, 171,49 s |
| GCC 12.4 / LP64 | 18/18 PASS, 10,36 s; SDK-freie Common-Tests inklusive beider Goldens |
| Erweiterte Waffen-/Combo-Coverage | Release PASS 6,24 s; Debug PASS 85,18 s |

Auswahl: Reader Safety/Independence/Compatibility/Redthief/Parity, Goldens, AssetRuntime-/AnimationRuntime-Verträge, Warmup/StallAudit, HairLodQuick, D3D11-Smoke, Platform inklusive WindowInput und GLB-Provider/Render. Kein Fuzzer, Stresslauf oder große Renderer-Suite. Der GCC-Gate ist keine Android-/Device-Abnahme.

Der historische Hair-Test baut direkt rohe SDK-Modelle ohne AssetHandle und benötigte eine ausdrückliche Granny-Animationsauswahl. Das korrigiert seinen Referenzvertrag; produktive Consumer verwenden AssetHandles. Native Hair-LOD bleibt gesondert in GR2Render und im Client-Smoke abgesichert.

Der erste Debug-Aufruf nach dem Hair-Fix selektierte wegen eines Zeilenumbruchs im Regex keine Tests. Er zählt nicht als PASS; der oben genannte echte 31er Lauf verwendet einen bereinigten Filter und `--no-tests=error`.

Builds innerhalb der Sandbox scheiterten an SDK-Erkennung bzw. Cygwin-Schreibrechten; autorisierte Builds außerhalb der Sandbox bestanden. Bekannte Vendor-Warnungen LNK4099/LNK4075/LNK4098 bleiben vorhanden. Auch die frischen Gate-B-Builds sind erfolgreich, aber nicht warnungsfrei.

## Manual Runtime / Character Select / Multi-Map

**Default-Client manuell bestätigt: „perfekt“.** Angefordert und bestätigt: Login → 3D-Character Select → Ingame, Welt/Player direkt nach Loading ohne sichtbaren Freeze, Idle/Gehen/Laufen/Angriff, Wildhund/NPC, Haar/Waffe/Mount soweit verfügbar, Resize, Minimize/Restore, X. PID 61736, Exitcode 0. Startoptionen enthalten nur Diagnostik/Audit, keinen Reader-/Animationsschalter. Laufkopie: `build/f2x/runtime/f34-default-login/`.

Automatischer Original-Pack-Smoke: **A1 → B1 → A1 PASS**, 63,1 s, PID 72584, 3.528 gerenderte Frames. Je Map Near/Far/Near und Screenshots; Mount/Rider, NPC, Wildhund, Boss, Haar/Waffe und Static-/Camera-Blocker-Submissions nachgewiesen. B1-Kamera zeigt den durchsichtigen Camera Blocker vor den Actors; A1-Rückkehr zeigt den korrekt dargestellten Warrior mit Waffe und NPC. Kein zusätzlicher Dungeon-Lauf. Laufkopie: `build/f2x/runtime/f34-default-multimap/`.

| Zähler | Default Multi-Map | Manueller Default |
|---|---:|---:|
| NativeGR2Reads | 933 | 735 |
| IndependentPoseSamples | 30.742 | 5.540 |
| GPUFrames | 25.414 | 6.444 |
| GrannyFileReads | 0 | 0 |
| ReferencePoseSamples / ImportPoseSamples | 0 / 0 | 0 / 0 |
| AllCPUDeformationCalls / Vertices | 0 / 0 | 0 / 0 |
| GPUFallbacks / AnimationRuntimeFailures | 0 / 0 | 0 / 0 |
| Exitcode | 0 | 0 |
| erfasste Shutdown-Ressourcen | 0 | 0 |

Ressourcen umfassen SourceTextures/Buffers, AssetDocuments, ReaderDocuments, RuntimeSkeletons/Clips, AnimationInstances, MeshBindings, SkinMeshes, Remaps/Palettes und GPU-Prototype-/Static-Skin-Objekte. Kein gesonderter Collision-Pool-Zähler im bisherigen Audit. Manueller syserr: bekannte MarkManager-Baseline `invalid idx 0`, kein Reader-/Animationsfehler.

## Prewarm / Performance

Die F2-P-Logik Local Player → relevante Clips → initiale Pose/GPU-Readiness → World Reveal ist unverändert. Manueller Lauf: 90 Prewarm-Anforderungen, 0 Failures/Limits/Bypasses; maximale Game-Pose laut Gesamtaudit 0,3059 ms, maximale Einzelpose 0,1948 ms. Der Benutzer bestätigt keinen sichtbaren Freeze.

Der schwellenbasierte manuelle Audit enthält allerdings 13 neue Clip-Aufbereitungen in acht erfassten Gameplay-Displayzeilen (maximal 99,3 ms Frame). Er trennt neue Umgebungsactors/erstmalige Inhalte nicht zuverlässig vom geprüften warmen Satz und ist **kein pauschaler Null-Decode-Nachweis**. Deshalb wird zusätzlich die vorhandene F2-P-Fixture mit vollständigen Cold-/Warm-Phasen ausgeführt. Der Fixture-Nachtest besteht: 80,417 s, 4.673 Gameplay-Frames, in allen sechs Cold-/Warm-Phasen null GR2-Reads, Animationsdecodes und Clip-/Track-Bindings. Keine Gameplay-Frames über 50 ms, Warm-Movement maximal 18,776 ms, Warm-Combat maximal 18,920 ms. Exitcode und Shutdown-Ressourcen 0. Die normale Framezeit enthält die bestehende Warte-/Present-Taktung; keine neue FPS-Optimierung. Belege: `build/f2p/runtime/f34-default-prewarm-fixed/`, `build/f34-restart/prewarm-analysis.log`.

Dabei wurde eine bestehende Python-Binding-Inkonsistenz sichtbar: `chrmgrPrewarmVisibleActors` verwendet `PyArg_ParseTuple` für optionale Argumente, war aber als `METH_NOARGS` registriert. Der direkte Python-Aufruf endete mit 0xc0000005. Einzige Korrektur: `METH_VARARGS`; kein Umbau/Optimierung von Prewarm, Reader oder Animation. Release/Debug neu gebaut; gezielter Fixture-Nachtest bestanden. Der manuell bestätigte Client stammt vor dieser isolierten Binding-Korrektur; seine Hashes werden getrennt bewahrt.

`GR2WarmupTest` prüft bereits wieder keine neuen Clip-Decodes und keine Release-C++-Heapallokationen im warmen Sample/Blend/Palette-/Motionwechsel-Hotpath. Debug trennt die bekannten STL-Proxy-Allokationen wie F2-P. Keine GPU-Ausführungszeit oder allgemeine FPS-Garantie behauptet.

## Gate-A Decision

**GO.** Warmup-Smoke und ergänzte Waffen-/Combo-Coverage sind ebenfalls bestanden; Gate B ist freigegeben. Alle bisherigen Default-Produktionsläufe enthalten null Granny-Reads/Pose-/Importsamples. Kein produktiver Granny-Fallback hinzugefügt.

## Gate B / Dependency Audit / Removal

Der vor dem Removal gesicherte, nach Dateipfad und Zeilen kategorisierte Audit steht in `phase-f34-dependency-before.json` (production, reference test, docs, dead code, third-party artifact). Kommentare mit toten SDK-Samplingaufrufen sind gesondert ausgewiesen. Historische Verbraucherklassen `CGrannyModel`, `CGrannyMesh`, `CGrannyModelInstance` und `CGrannyLODController` behalten ihre Namen; sie enthalten nur noch neutrale AssetHandles, AnimationInstances, MeshBindings und GPU-Daten.

Entfernt: kompletter `src/AssetRuntime/Granny`-Provider einschließlich AnimationAdapter/Interop/NativeTypes/LegacyVertexTypes, SDK-Header `granny.h`/SPU-Header, `granny2_static.lib` und Importtarget, SDK-Deformer/Util, rohe SDK-Model-/Mesh-/Material-/Motion-Einstiege sowie die SDK-Fallbackzweige der Consumer. Gemeinsame Deform-Buffer-Hilfsfunktionen heißen neutral. GR2 bleibt unverändert unterstützt.

Der normale `.gr2`-Dispatch ruft ausschließlich `GetGR2AssetProvider()` auf. Beide Startoptionen akzeptieren nur noch `ziinan`; `granny` und `auto` werden als ungültig abgewiesen. Die ehemaligen SDK-Zähler bleiben als stets null erwartete Auditfelder erhalten, enthalten aber keinen SDK-Aufruf. `.glb` bleibt beim GlTF-Provider.

Alle Runtime-SDK-Referenztests und der einmalige Golden-Exporter sind entfernt. Die acht statischen und sieben Clip-Goldens sind unverändert erhalten. `GR2RenderTest` deckt den produktiven Consumer ab; `Renderer.HairLodQuick` führt jetzt dessen native Near/Far/Near-Sequenz aus. Die alten SDK-Vollvertex-, Benchmark- und Stressharnesses werden nicht als weiterhin verfügbare Tests ausgegeben. Eine vollständige Live-SDK-Parität wird nach Removal nicht behauptet.

`GetCollisionInstanceCapacity()` summiert die reservierten Plätze der fünf bestehenden Collision-Pools; `CollisionResources` wird nach dem vollständigen Shutdown protokolliert. Pool-Verhalten und Freigabereihenfolge sind unverändert; es wurde nur ein lesender Nachweis ergänzt.

### Clean Build / finale Abnahme

`build-f34-clean` wurde erst nach dem physischen Löschen des SDK neu konfiguriert. Configure PASS. Ausschließlich bereits heruntergeladene Quellpakete werden wiederverwendet: DiligentCore `b036337d68be2353c9950a85929acf796b9a6d50`, Assimp 6.0.2 und meshoptimizer 0.25. Alle zugehörigen Bibliotheken entstehen neu unter `build-f34-clean`; kein alter CMakeCache und keine alten Build-Libs werden verwendet. Die verbleibenden vorgebauten externen Bibliotheken gehören zu anderen, unveränderten Abhängigkeiten.

| Geforderter Gate-B-Nachweis | Ergebnis |
|---|---|
| Provider-/Adapter-/SDK-/CMake-/Linker-Removal | PASS |
| Fresh Configure / Release / Debug | PASS |
| Release Fast Gate | **39/39 PASS, 46,94 s** |
| Debug Fast Gate | **39/39 PASS, 104,93 s** |
| GCC 12.4 / LP64 | **19/19 PASS, 10,22 s** |
| Golden Static / Animation | PASS in allen drei Gates; Original-Hashes unverändert |
| Native Player-/Actor-/GPU-Coverage | PASS; GR2Render Release 24,81 s, Debug 82,44 s |
| Native HairLodQuick | PASS; Release 0,98 s, Debug 1,40 s |
| GLB Provider / Blend-/Static-Render / Wide Indices / Embedded Materials | PASS |
| Offline Tool Unit / FBX-OBJ-DAE Roundtrip / CLI / Runtime Isolation / Diligent Roundtrip | 6/6 je Release und Debug PASS |
| Finaler Prewarm-Lauf | PASS, 80,288 s |
| Finaler A1 → B1 → A1-Lauf | PASS, 63,4 s |
| Finaler Default-Client / Character Select / Fenster / X | Benutzer bestätigt „perfekt“, Exitcode 0 |

Logs unter `build/f34-restart/`: `clean-configure.log`, `clean-release-build.log`, `clean-release-finalize.log`, `clean-debug-build.log`, `final-release-tests.log`, `final-debug-tests.log`, `final-gcc-build.log`, `final-gcc-tests.log`. Der kleine Release-Abschlussbuild übernimmt die Collision-Diagnose und neutrale Consumer-Bereinigung. Die abschließenden Besitz-/Material-Kommentarkorrekturen ändern keine C++-Tokens oder Laufzeitlogik; dafür wurde kein weiterer Build gestartet. Release-GPU-Tests liefen parallel zum Debug-Build, daher sind ihre Zeiten keine Performance-Benchmarkwerte. Prewarm lief nach Ende aller Builds/GPU-Tests.

Es wurde kein Fuzzer, kein Stresslauf und keine große Renderer-Gesamtsuite ausgeführt. Kein einzelner finaler Test benötigte mehr als 83 Sekunden. Die nicht verfügbare Wolfman-/Shield-/zusätzliche Dungeon-Abnahme bleibt wie oben abgegrenzt. Bestehende Vendor-Linkwarnungen LNK4099/LNK4075/LNK4098 bleiben erhalten.

### Source-/Linker-/Binary-Zero-Audit

`phase-f34-zero-audit.json` enthält den finalen Audit: **2.678 Produktions-/Builddateien**, null SDK-Includes, `granny_*`-Typen, SDK-Aufrufe, SDK-Artefakte und generierte Linkprojekt-Treffer. Alle sieben ausdrücklich geforderten SDK-Typen sind entfernt. SDK-Verzeichnisse sind physisch entfernt. Der zusätzliche CMake-Test `AssetRuntime.NoGrannyDependency` verhindert die Rückkehr dieser Abhängigkeiten.

Die effektiven Linkinputs unter `build-f34-clean` referenzieren keine alten `build-c2x`-/`build-c3x`-/`build/e2x`-Bibliotheken. Diligent-Quellrevision wurde separat bestätigt. Das aktuelle `asset-tool-runtime-isolation.txt` bestätigt weiterhin: kein Assimp/meshoptimizer/AssetTool im Runtime-Linkgraph.

**Fünf finale EXEs geprüft:** Client Release/Debug, Release PackMaker und Offline AssetTool Release/Debug. Alle AMD64; keine Granny-, D3D8-, D3D9- oder D3DX-Imports. Es entstehen keine Granny-Deploymentdateien. Auch die ASCII-/UTF16-Kennungen `granny2`, `GrannyReadEntireFile`, `GrannyGetFileInfo`, `GrannySampleModelAnimations` und `GrannyPlayControlledAnimation` fehlen in diesen Binärdateien. Die erhaltenen `CGranny*`-Verbrauchernamen und Nullzähler sind keine SDK-Abhängigkeiten.

Finaler Release-SHA256: `A80C05736F21690F9215A8A84B1557BEC1B3D0CBA5ABAF096A5FEDB75B8F3683`.
Finaler Debug-SHA256: `993F7B3B09C5D01FC9CDD2643ACF222B96FE6125F035010215444A6D25AD55E8`.
Alle drei finalen Release-Laufkopien stimmen mit diesem Release-Hash überein.

### Finale Runtime / Shutdown / Grenze der Beobachtung

Normaler Default-Client: `build/f2x/runtime/f34-final-login/`, PID 52624. Start nur mit Diagnostik-/Stall-Audit, ohne Reader-/Animationsauswahl. Login → 3D-Character Select → Ingame, Welt direkt nach Loading, Bewegung/Kampf/Attachments/NPC, Fenster-Resize, Minimize/Restore und X vom Benutzer mit **„perfekt“** bestätigt. Mount soweit manuell verfügbar; Mount/Rider zusätzlich im automatischen Original-Pack-Lauf nachgewiesen.

Multi-Map: `build/f2x/runtime/f34-final-multimap/`, PID 45772, 3.526 Frames, A1 → B1 → A1 vollständig. Pro Map Near/Far/Near, Idle/Walk/Run/Attack und Near-/Far-Screenshots. B1 zeigt den Camera Blocker vor den Actors; die A1-Rückkehr zeigt den korrekt dargestellten Warrior mit Waffe und NPC. Native Building-/Prop-/Camera-Blocker-Submissions und alle geforderten vorhandenen Actorgruppen sind protokolliert. Kein zusätzlicher Dungeon-Lauf.

| Zähler | Finaler Multi-Map | Finaler manueller Client | Finaler Prewarm |
|---|---:|---:|---:|
| NativeGR2Reads | 933 | 1.043 | 365 |
| IndependentPoseSamples | 30.739 | 238.430 | 29.282 |
| GPUFrames | 25.411 | 234.590 | 27.721 |
| GrannyFileReads / ReferencePoseSamples / ImportPoseSamples | 0 / 0 / 0 | 0 / 0 / 0 | 0 / 0 / 0 |
| AllCPUDeformationCalls / Vertices / GPUFallbacks | 0 / 0 / 0 | 0 / 0 / 0 | 0 / 0 / 0 |
| AnimationRuntimeFailures / SkinPreparationFailures | 0 / 0 | 0 / 0 | 0 / 0 |
| Reader-/Asset-/Animation-/Binding-/Skin-/GPU-Ressourcen | 0 | 0 | 0 |
| CollisionResources (alle reservierten Poolplätze) | 0 | 0 | 0 |
| Exitcode | 0 | 0 | 0 |

Manueller syserr enthält die bekannte MarkManager-Meldung `invalid idx 0` und bestehende ProcessDamage-Diagnostik; keine neuen AssetRuntime-/Reader-/Animationsfehler. Shutdown zählt ReaderDocuments, AssetDocuments, AnimationInstances, MeshBindings, Skeletons/Clips, SkinMeshes, Remaps/Palettes, SourceTextures/Buffers, GPU-Geometrie/Paletten/StaticSkinMeshes sowie Collision-Pools. Freigabe erfolgt weiterhin nach dem World-Teardown.

**Beobachtungsgrenze:** Die zusätzlich versuchte externe Modulauflistung wurde von Windows verweigert (.NET/Tasklist ohne Modulliste, Toolhelp Fehler 5). Eine vollständige OS-Modulliste oder DLL-Suchspur wird daher **nicht als bestanden behauptet**. Der Runtime-Zero-Nachweis stützt sich auf den physisch SDK-freien Clean Build, den direkten nativen Dispatch ohne Referenzzweig, Source-/Link-/Import-/Binärprüfungen und Nullzähler der erfolgreichen realen Läufe. Es wurde keine Prozessschutz-Einstellung geändert.

### Finale F2-P-Regression / Performance

`build/f2p/runtime/f34-final-prewarm/`: vollständige, undropped Aufzeichnung von 4.673 Gameplay-Frames in sechs Cold-/Warm-Phasen. **In jeder Phase null GR2-Read/Decompress/Container/Parse, Animation-Decode, Clip-/Track-Binding, Skeleton/Mesh/Material-Aufbereitung und Fingerprinting.** Alle Imports gehören zum Loading; Ladezeit 2,872 s. 68 Prewarm-Anforderungen, null Fehler/Limits/Bypasses. Kein Gameplay-Frame über 50 ms.

| Phase | Max. Frame inkl. Taktung | Max. Pose |
|---|---:|---:|
| Cold Idle | 22,279 ms | 0,362 ms |
| Cold Movement | 18,488 ms | 0,276 ms |
| Cold Combat | 18,644 ms | 0,350 ms |
| Warm Idle | 18,940 ms | 0,142 ms |
| Warm Movement | 18,800 ms | 0,215 ms |
| Warm Combat | 18,629 ms | 0,143 ms |

Warm-Palette maximal 0,057 ms; Pose/Palette klar unter 8,33 ms. Der bestehende Release-Warmup-Test bestätigt null C++-Heapallokationen im geprüften Sample/Blend/Palette/Motionwechsel-Hotpath; Debug berücksichtigt weiterhin die bekannten STL-Proxies. Keine Performanceoptimierung und kein 60/120-FPS-Versprechen; Framezeit enthält die vorhandene Taktung/Present-Wartezeit.

Der freie manuelle Ingame-Lauf ist keine vollständige Warmset-Aufzeichnung: 22 neue Clip-Aufbereitungen erscheinen in 13 über der Audit-Schwelle gespeicherten Gameplay-Displayzeilen, maximaler Game-Frame 187,08 ms. Neue Umgebungsactors/Inhalte bleiben von der kontrollierten Warmset-Garantie getrennt. Max. Game-Pose 2,0534 ms, Einzelpose 0,2223 ms; Benutzer bestätigt keinen sichtbaren Loading-Freeze. Ein pauschales Null-Decode-Versprechen für sämtliche erstmals auftauchenden Inhalte wird nicht abgeleitet.

## Verbleibende sechs GR2-Rejects

| Rohdatei unter assets | Grund / Produktionsauflösung |
|---|---|
| `metin2_patch_dragon_rock_mobs/ymir work/monster2/redthief_general/front_damage.gr2` | Nonfinite float; Nutzung ungeklärt, in geprüften Motlists nicht referenziert; keine willkürliche Reparatur/Freigabe |
| `season3_eu/ymir work/zone/dungeon/haven_dungeon/skipia_boss.gr2` | Deklarierte Größe falsch; gültige native-lesbare Zone-Pack-Version überlagert die Rohkopie |
| `season3_eu/ymir work/zone/dungeon/haven_dungeon/skipia_passc.gr2` | Deklarierte Größe falsch; gültige native-lesbare Zone-Pack-Version überlagert die Rohkopie |
| `season3_eu/ymir work/zone/dungeon/haven_dungeon/skipia_passl.gr2` | Deklarierte Größe falsch; gültige native-lesbare Zone-Pack-Version überlagert die Rohkopie |
| `season3_eu/ymir work/zone/dungeon/haven_dungeon/skipia_passp.gr2` | Deklarierte Größe falsch; gültige native-lesbare Zone-Pack-Version überlagert die Rohkopie |
| `season3_eu/ymir work/zone/dungeon/haven_dungeon/skipia_passt.gr2` | Deklarierte Größe falsch; gültige native-lesbare Zone-Pack-Version überlagert die Rohkopie |

Keine Behauptung von 100-%-GR2-Coverage. Detaillierte F3-B-Klassifikation bleibt in `phase-f3b-compatibility-results.json`.

## Git-Diff / GO-NO-GO / Fortsetzung

Inhaltlicher Diff: 79 vorhandene Dateien geändert/entfernt, darunter 29 entfernte SDK-/Referenzdateien; acht neue Dateien (Golden-Test/-Hilfsheader, zwei Referenzdatendateien plus README, SDK-Abhängigkeitstest und zwei Audit-JSONs). Dazu gehören die native Default-/Dispatch-Auswahl, neutrale Consumer, CMake-Bereinigung, Python-Argumentregistrierung und Collision-Diagnose. Die einmalige Golden-Erzeugung wurde vor Removal lokal gesichert und ist kein neues Buildtarget mehr.

`git diff --check` PASS, Index unverändert, HEAD weiterhin `961401a09297a0f0d7504595681b3bf53eb7c134`. Keine Builds, Logs, PDBs, generierten Libraries oder temporären Dumps im inhaltlichen Diff. Alle Laufkopien und Buildbelege liegen in ignorierten Buildordnern. Die vorbestehenden Änderungen im Client-Repository (Redthief-Asset, `channel.inf`, Logs) bleiben unverändert. Kein Stage, Commit oder Push.

**Gate A GO. Gate B GO. Gesamt-F3/4-X GO** für die hier geprüften Produktionspfade und das verfügbare Corpus. Die sechs bekannten Rohdatei-Rejects bleiben transparent; keine 100-%-Coverage behauptet. Die zusätzliche externe OS-Modulauflistung bleibt ausdrücklich nicht verfügbar, siehe Prüfgrenze oben.

**STOP nach F3/4-X.** Empfehlung: diesen Stand abnehmen; eine Folgephase erst nach einem neuen Auftrag. Kein F5-X, SpeedTree-Umbau, PBR/Visual Remaster, Android-Fortschritt oder FPS-Menü begonnen.
