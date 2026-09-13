# Phase B5-X – GPU-Skinning-Parität und Stabilität

Stand: 13.09.2026. Basis: abgenommenes B4-X einschließlich gemeinsamer Hair-LOD-Binding-Korrektur; Source-HEAD `5187800` plus bereits vorhandener uncommitteter B4-X-Stand.

## Status und feste Grenze

**B5-X vollständig abgeschlossen: GO für B6-X. B6-X wurde nicht begonnen.** CPU bleibt Produktionsdefault, GPU ausschließlich `--skinning=gpu-prototype`. Diligent D3D11 bleibt der vorhandene Renderer. Keine neue Actor-Kategorie, kein neues Skinning-/Materialfeature, keine Performance-Optimierung oder Produktionsumschaltung. Granny-SDK und CPU-Deformer unverändert.

B5-X ergänzt ausschließlich Tests, private Weltfixtures, deren Launcher/Auswertung und diesen Bericht. Keine Produktcodekorrektur erforderlich. Die Release-Binary hat weiterhin denselben SHA-256 wie B4-X: `E2E24EE5BD5294CBA5829FDD9A403F3E0B81FCC74D242ECE9EA7325FD38F2294`. CPU-Deformer `Deform.cpp`: `C73674782ADA9D554237F1522605ABA3BFCBCBFF66FAB14E2FD34545B1FC9DE7`.

## 1. Verbindliche Actor-Testmatrix

`Renderer.SkinningStability.parity` verwendet zwei getrennte native Instanzen mit denselben Originalassets, Animationszeiten, Kameras und Drawparametern. CPU: unveränderter nativer Deformer und dynamischer PNT-Upload. GPU: vorhandenes statisches PWNT, aktueller Remap und Bonebuffer. Es wird keine Referenzlogik in den Produktpfad eingebaut.

| Kategorie | Konkrete Originalassets / Varianten | CPU / GPU | Fallback / Ausnahme |
|---|---|---|---|
| Player | Krieger, Ninja, Sura, Schamane jeweils `pc` und `pc2`: alle acht Race/Gender-Kombinationen | bestanden / bestanden | Kein unerwarteter Fallback |
| Shapes | Je Race novice → 4-1 → lord → marry → novice; drei Zyklen | bestanden / bestanden | Reale Shape-Meshes, keine erfundenen Rüstungen |
| Haare | hair_1_1 → hair_2_1 → hair_1_1 pro Shape; ergänzend komplette B4-X-Hair-LOD-Matrix dreifach | bestanden / bestanden | Rigid hair_15_1 bleibt rigid; skinned hair_17_1 ist ein reales verknüpftes Attachment |
| NPCs | Goods, Blacksmith, Doctor, Sinseon | bestanden / bestanden | Fehlende Animationsdateien nicht erfunden |
| Kleiner Mob | Wolf | bestanden / bestanden | Kein unerwarteter Fallback |
| Humanoide / gemischte Meshes | Orc Soldier, Barbarian Bow | bestanden / bestanden | Rigid/deformable Meshgruppen bleiben getrennt |
| Große Mobs / Bosse | Fire Dragon, Misterious Diseased Bosshost | bestanden / bestanden | Reales Maximum 163 Bones |
| Mounts | Horse Normal, Boar, White Lion, Halloween Horse, Dinosaur 3 | bestanden / bestanden | Bereits vorhandene Coverage |
| Rigid weapon | item/weapon/00010.gr2; private Welt zusätzlich Waffen A/B/none | bestanden / bestanden | Rigid ist kein GPU-Skinning-Fallback |
| Weitere skinned Attachments | Vorhandene verknüpfte Haare einschließlich hair_17_1 | bestanden / bestanden | Keine neue Attachment-Art erfunden |

## 2–6. Vertex-, Normal-, Matrix-, Pose- und Pixelparität

- `Numeric` aus B4-X vergleicht sämtliche deformierbaren Vertices des jeweiligen realen Meshes gegen einen GPU-Readback der unveränderten SkinVertex-Funktion. Der isolierte Compute-Probe ist bereits vorhandene Testinfrastruktur, kein neuer produktiver Compute-Skinning-Pfad.
- Positionstoleranz unverändert: `1e-4 + 2e-6 * max(1, abs(CPU-Komponente))`; Normalentoleranz `1e-5`. Keine Normalisierung oder Gewichtskorrektur zur Verschönerung des Ergebnisses.
- Unabhängig gesampelte native CPU-/GPU-Posen behalten die bereits in B4-X begründete Matrixgrenze `1e-5`. Zusätzlich liest B5-X den **tatsächlich beim Produktdraw gebundenen Vertex-Shader-Constantbuffer** zurück: aktuelle native GPU-Palette inklusive Reihenfolge und Nullfüllung bis 256 Slots muss byte-/floatwertgenau passen. Keine Prüfung nur einer separaten Testkopie der Uploaddaten.
- Remaps bleiben die nativen Meshbindings; der Hair-LOD-Test prüft sie exakt gegen jeweils frisch gebundene native Referenzinstanzen. Gleiche Remap-/Pose-Identitäten müssen bei Rückkehr wieder passen.
- Player: Idle/Walk/Run/Attack/Hit/Death, soweit Originaldatei vorhanden. Andere Actors: vorhandene wait/walk/run/attack/attack1/damage/dead bzw. numerische Originalclipnamen 00/20/30/31; ohne Clip ausdrücklich Bind-Pose. Mount-Bewegung zusätzlich in Originalpaket-Weltläufen.
- Normalvergleich umfasst alle Vertices, damit auch Schultern/Hände/Knie/Haare sowie große Mobs und Mounts, nicht nur optisch ausgewählte Punkte.
- Pixelvergleich: identische Kamera, native Zeit, Shape, Material, Welt-/Normalmatrix. Unveränderte B3/B4-Rastergrenze: höchstens 1 % unterschiedliche RGB-Kanäle, höchstens 0,01 % Kanäle mit Differenz > 2. Leere Vergleichsbilder werden abgelehnt.
- `pixels.csv` enthält pro Vergleich mittlere absolute RGB-Kanalabweichung (0–255), Maximum, geänderte Kanäle und Kanäle > 2. CPU-/GPU-JPEGs und 16-fach verstärkte Differenzbilder werden für alle acht Player-Bases/Rüstungen sowie repräsentative NPCs, Mobs, Bosse und Mounts gespeichert. Die Messwerte stammen **vor** der JPEG-Kompression aus dem Roh-Readback.
- Die numerische Materialfixture ist eine diagnostische RGBA-Textur; Original-Texturzuordnung wird zusätzlich durch B3 und die sichtbaren Originalpaket-Läufe geprüft. Kein Anspruch auf numerischen Vergleich jeder Originaltextur.

Release-Pixelvergleich bestanden: **844 Vergleiche**, 623.560.704 RGB-Kanäle, mittlere absolute Kanalabweichung **0,0000154949**, Maximum **169**, insgesamt **2.239** unterschiedliche Kanäle. Nur **109** Kanäle in 34 Vergleichen unterscheiden sich um mehr als 2, maximal sechs solcher Kanäle pro Bild. Das große Einzelmaximum stammt aus einem Haarfall; es ist kein breitflächiger Farb-/Posefehler und wird trotz bestandener unveränderter Rastergrenze nicht verschwiegen. Die Datei `build/phase-b5x/parity-Release/pixels.csv` enthält die Einzelwerte.

Gespeicherte Beispiele unter `build/phase-b5x/parity-Release/`: `pc-warrior-cycle0-shape1-wait-{cpu,gpu,diff-x16}.jpg`, `goods-cycle0-wait-*`, `wolf-cycle0-00-*`, `boss163-cycle0-00-*`, `horse-cycle0-00-*`. GPU-Beispiele dieser Gruppen und das Krieger-Base-Differenzbild wurden visuell gesichtet. Die diagnostischen Farben sind absichtlich nicht die Original-Assettexturen.

| Numerische B5-X-Prüfung | Fälle / Vertexvergleiche | Max. Position | Max. Normale | Max. native Posedifferenz |
|---|---:|---:|---:|---:|
| Release breite Parität | 844 / 1.427.815 | 0,000488281 | 2,38419e-7 | 7,62939e-6 |
| Release wiederholte Hair-LOD-Parität | 672 / 225.015 | 0,0000457764 | 1,78814e-7 | 0 |
| Debug breite Parität | 844 / 1.427.815 | 0,000488281 | 2,38419e-7 | 0 |
| Debug wiederholte Hair-LOD-Parität | 672 / 225.015 | 0,0000457764 | 1,78814e-7 | 0 |

Je Konfiguration zusätzlich **1.070 tatsächliche Bonebuffer-Readbacks**, jeweils exakt zur aktuellen Palette einschließlich Null-Tail, sowie **264** im breiten Test gezählte Actor-/Shape-Lifecycles. Im Stresstest werden die Crowd-/Hair-Lifecycles separat geprüft, nicht in diesen 264 mitgezählt. Release-Beleg: `build/phase-b5x-new-release-details.log`; Debug: `build/phase-b5x-debug-renderer-details.log`.

Debug-Pixelvergleich ebenfalls bestanden: 844 Vergleiche, dieselbe Kanalzahl, Mittelwert **0,0000154869**, Maximum **169**, 2.234 geänderte Kanäle und 109 Kanäle mit Differenz > 2. Die geringe Differenz zum Release-Aggregat betrifft getrennte native Rundungen; die unveränderten Einzelbildgrenzen werden in beiden Konfigurationen eingehalten. Debug-D3D11-InfoQueue tatsächlich aktiv, Warnungen/Fehler **0**.

## 7–16. Wechsel-, Material-, Fallback- und Invalid-Data-Prüfungen

| Prüfung | Umfang / Beweis |
|---|---|
| Player Shapes | Acht Races × drei Base/Armor-A/B/C/Base-Zyklen; laufende Run-/Attack-Posen, erste Runde zusätzlich weitere verfügbare Clips |
| Hair LOD | Drei vollständige Wiederholungen der 224 B4-X-Fälle: acht Races, 32 Haar-/Körperkombinationen, full → LOD1 → LOD2 → LOD3 → LOD2 → LOD1 → full |
| NPC / Mob / Boss / Mount | 15 repräsentative Assets einschließlich rigid weapon; je acht Spawn-/Despawn-Zyklen, zwei unabhängige Instanzen und verfügbare Animationen |
| Stale SRB / Palette | Draw A → B mit anderer Pose → A; beide A-Bilder müssen exakt übereinstimmen. Tatsächlich gebundene Bonepalette zusätzlich zurückgelesen |
| Material | Vorhandene zwölf Cull-/Blend-/Alpha-Test-/DepthWrite-Varianten und Faktorparameter gegen CPU; echte Armor-Overrides in Originalpaket-Weltfixture |
| Hair / Attachment | A → B → A; verknüpfte Haare teilen genau einen aktuellen Body-Posebuffer; starre Waffen bleiben PNT mit nativer Weltmatrix |
| Fallback | Fünfmal absichtlich fehlender Sidecar-Remap im Stresstest; zusätzlich acht Zyklen mit exakt verglichenen aktuellen CPU-Vertices/Normals, identischem CPU-Fallback-Bild und identischem frischem GPU-Bild nach Recovery |
| Ungültige Daten | OOB aktiver Boneindex, OOB Remap, fehlender Remap, Palette mit 164 Bones, NaN-Matrix, Nullgewichte, Gewichtsumme 254, fehlende Meshdaten; keine Veröffentlichung eines GPU-Handles |
| Gültige Grenze | B3 prüft tatsächliches GPU-Lesen von Bone 162 in einer 163-Bone-Palette; B2 prüft weitere native Extraktions-/Topologie-/Gewichtsfehler |

Legitime Rückfälle des vorhandenen Produktcodes: fehlende/nicht freigegebene Skin-Daten oder Remaps, nicht unterstütztes Vertexlayout, fehlende/ungültige oder zu große Palette, fehlgeschlagene GPU-Vorbereitung/Allokation. Diese führen nur bei intakter nativer Pose zum bestehenden CPU-Deformer; GPU-Geometry wird verworfen. Die beabsichtigte Testkorruption ist **kein** unerwarteter Welt-Fallback. Für Allokations-/Geräteausfall wird kein künstlich injizierter Erfolg behauptet. Eine gescheiterte native Hair-Neubindung verwirft die Pose vor CPU/GPU-Deformation statt mit alten Indizes weiterzulaufen. Rigid parts und expliziter CPU-Testmodus sind keine Fallbacks.

Der zusätzliche `Renderer.SkinningFallbackParity` ist in Release und Debug bestanden: acht absichtlich ausgelöste Fallbacks, je exakt eine native CPU-Deformation; Vertex-/Normaldifferenz 0, Fallback-Pixeldifferenz 0, Recovery-Pixeldifferenz 0, Owner nach jedem Zyklus 0. Debug: D3D11-Validierung aktiv, Warnungen 0. Belege: `build/phase-b5x-fallback-release.log`, `build/phase-b5x-fallback-debug.log`. Damit ist nicht nur ein Bereitschaftsflag, sondern die tatsächliche Fallback-Darstellung geprüft.

## 17–21. Actor-, Karten-, Relog- und Mount-Stress

- Massen-Actors: 1/10/25/50/100 originale Wolf-Instanzen, fünf vollständige Wiederholungen; CPU-/GPU-Drawzahl gleich, ein geteilter statischer Meshbuffersatz und jeweils eigene Posebuffer. Keine FPS-/Performancebewertung.
- Private CPU- und GPU-Weltfixture: jeweils zwölf Phasen zu 80 Sekunden, insgesamt 16 Minuten. Kartenfolge **A1 → B1 → A1 → Dungeon → Gildenkarte → A1**, zweimal vollständig. Originalpakete, keine Servermutation.
- Sichtbare Gruppen mit 16/25/50/100 Actors; wiederholte native Instanzersetzung, Shape-/Hair-/Weaponwechsel alle vier Sekunden und Mount-/Dismount-ähnliche Delete/Create-Wechsel alle 20 Sekunden. Kein unsicherer direkter Python-Dismount-Helfer.
- Kamera nähert/entfernt sich; in Fernschritten versetztes Kamerazentrum vermeidet die 500-Einheiten-Ausnahme des zentrierten Players. Die direkte numerische Hair-LOD-Prüfung ist unabhängig davon.
- Echte normale Relogs erfolgen separat mit Original-Login und nutzerbedientem Spielmenü. Ein Clientneustart oder bloßer Login wird nicht als Relog gezählt.

| Normaler Lauf | Beleg | Ergebnis |
|---|---|---|
| GPU 01, PID 35988 | `gpu-normal-relog-01` | Nutzer bestätigt Darstellung/Fensterzustände; protokolliert VID 0 → 22345 → 0 → 22346, also ein echter Relog; Exit 0, 53,7 s, Skinningfehler/Fallbacks 0, alle überwachten Owner 0 |
| CPU 01, PID 6868 | `cpu-normal-relog-01` | Nutzer bestätigt Darstellung/Fensterzustände; Log belegt zunächst nur Login, deshalb keine belegte Relog-Wiederholung daraus abgeleitet; Exit 0, 92 s, Skinningfehler 0, alle überwachten Owner 0 |
| GPU 02, PID 61232 | `gpu-normal-relog-02` | Vier echte Relogs: 0 → 22357 → 0 → 22360 → 0 → 22361 → 0 → 22364 → 0 → 22366; Darstellung und Fensterprüfung bestätigt. Exit 0, 230,8 s, GPUFrames 148.945, CPUReferenceFrames/CPUVertexBytes 0, Skinningfehler 0, alle überwachten Owner 0 |
| CPU 02, PID 65220 | `cpu-normal-relog-02` | Ein echter Relog: 0 → 22367 → 0 → 22368; zusätzliche novice/4-1/lord/nahan-Rüstungs- und reduzierte/volle LOD-Wechsel. Nutzer bestätigt Darstellung/Fensterzustände; Exit 0, 92,6 s, GPUFrames 0, Skinningfehler 0, alle überwachten Owner 0 |

Die Rückmeldungen allein werden nicht mit einer bestimmten Anzahl von Wiederholungen gleichgesetzt: Der GPU-Stress belegt vier Relogs in einer Sitzung, die CPU-Regression einen. Alle normalen Läufe verwenden den männlichen Krieger / Race 0; die übrigen sieben Races werden numerisch und in den Weltfixtures geprüft. Nicht protokollierte Mount-/Mob-Begegnungen dieser kurzen Starts werden durch diese Logs nicht belegt; dafür existieren getrennte numerische und Weltfixture-Tests.

## 22–26. Fenster, Langzeitlauf, Validierung und Resource Lifetime

- Nutzer bestätigt im GPU-Langzeitfenster PID 32224 und CPU-Vergleich PID 47608 korrekte Darstellung bei Resize und 3× Minimize/Restore. Beide Tests haben ihr automatisches Ende erfolgreich erreicht.
- 16-Minuten-Weltläufe kombinieren Kamerabewegung, reale Animationen, Map-/Actor-/Mount-/Materialwechsel. Interaktive Spiel-/UI- und echte Relog-Prüfungen sind separate normale Clientläufe; die Fixture wird nicht als 16-minütiges manuelles Gameplay ausgegeben.
- Vorhandene Actor-/Attachment-/Mount-Framezähler und Prozessspeicher/Handles werden erfasst. Numerische Tests prüfen Bonebuffer-/StaticMesh-Owner nach Despawn, Hair-/Shape-Zyklen und Fallback-Recovery. Normale Clients liefern die vorhandenen Shutdown-Audits. Nicht vorhandene separate native Granny-Binding-Zähler werden nicht erfunden.
- Debug-Suiten müssen eine tatsächlich verfügbare D3D11-InfoQueue und **0** Warnungen/Fehler nachweisen; Release ohne Debuglayer ist nicht gleichbedeutend mit dieser Validierung.
- Erwartete finale Owner: Actor-/Attachment-/Mount-Geometrie und Texturen, SkinMeshes, BoneRemaps, BonePalettes, PrototypeGeometry, PrototypePalettes, StaticSkinMeshes jeweils 0. `BoneRemaps` enthält Hair-Sidecars.
- Speicherkurven sind wegen absichtlichem Asset-/Map-/Actorwechsel und Caches keine isolierte Leakmessung. Vergleichbare Zyklen und genaue Owner-Checks werden getrennt ausgewertet; keine absolute Leakfreiheit aus kurzem Arbeitsspeichervergleich behauptet.

| Abgeschlossener Weltlauf | GPU opt-in | CPU Default |
|---|---:|---:|
| PID / Dauer | 32224 / 962,5 s | 47608 / 962,2 s |
| Vollständige Kartenphasen | 12 | 12 |
| Exitcode | 0 | 0 |
| Max. sichtbare Actors einschließlich Mounts | 108 | 108 |
| Max. NPCs / Mobs / Mounts | 48 / 44 / 8 | 48 / 44 / 8 |
| Max. Actor-/Attachment-Geometrie | 128 / 20 | 128 / 20 |
| GPUFrames | 3.462.343 | 0 |
| CPUReferenceFrames im GPU-Scope | 0 | 3.468.713 |
| Unerwartete Fallback-/Binding-/Remap-/Palette-Fehler | 0 | 0 |
| Sämtliche überwachten Shutdown-Owner | 0 | 0 |
| Prozesssamples | 193 | 192 |
| Private MB: Maximum / letzter Sample | 685,3 / 607,2 | 683,4 / 624,3 |
| Handles: Maximum / letzter Sample | 934 / 925 | 935 / 926 |

Die 108 sichtbaren Actors sind maximal 100 angelegte Figuren plus acht Reittiere. `GPUFrames`/`CPUReferenceFrames` zählen Actor-Deformationen, nicht Bildschirmbilder oder FPS. Beide `syserr.txt` leer; Actor-ERROR 0. Sämtliche Welt-/UI-/Text-/Effekt-/Baum-Shutdownzähler ebenfalls 0. Belege unter `build/phase-b5x/{gpu,cpu}-stability-release-01/`: `stability-audit.json`, `resources.csv`, Phasenlog, Actor-/Rendererlogs und `exit.txt`. Jeder Lauf hat zwei vollständige Kartenzyklen, 48 vollständige Populationserzeugungen, **2.292 Actor-Erzeugungen** und **240 Zustandswechsel** durchlaufen.

## 27–29. CPU-Regression, Builds und Tests

| Prüfung | Stand |
|---|---|
| Vollständiger Release-Build | Bestanden, `build/phase-b5x-release-full-build.log` |
| Vollständiger Debug-Build | Bestanden, `build/phase-b5x-debug-build.log` |
| Neue Release-B5-X-Tests | Breite Parität (427,82 s) und Stress (306,56 s) bestanden; zusätzlicher Fallback-Bildtest ebenfalls bestanden |
| Komplette bestehende Release-Suite | **Alle 24 erfolgreich ausgeführt:** erster Gesamtlauf 23/24, anschließend unveränderter Shelltest mit korrekter Shellumgebung 1/1 |
| Vollständiger registrierter Release-Testbestand einschließlich B5-X | **27/27 erfolgreich ausgeführt:** 24 Bestandstests wie oben, neue Parität/Stress 2/2 und registrierter Fallback-Bildtest 1/1; kein behaupteter einzelner 27/27-Gesamtlauf |
| Debug-Renderer-Suite mit B2/B3/B4-X/B5-X | **23/23 bestanden:** zusammenhängender Lauf 22/22 (1.146,67 s) plus anschließend registrierter Fallback-Bildtest 1/1 (0,77 s), jeweils Exit 0 |
| CPU-/GPU-Langzeitwelten | Beide bestanden, je zwei Kartenzyklen / mehr als 16 Minuten, Exit 0 |
| Normale CPU-/GPU-Relogs | GPU vier in einer Sitzung, CPU einer; Exit 0 und Owner 0 in allen vier normalen Starts |

Release-Gesamtlauf: `build/phase-b5x-existing-release-tests.log`, **23/24**, 1.046,89 s, CTest-Exit **8**. Alle Renderer-/B2-/B3-/B4-X-Tests darin bestanden. Nur der unveränderte Vendor-`playTests` scheiterte, weil die zuerst gefundene Cygwin-Shell einen `/cygdrive/c/...`-Pfad an das native Windows-`zstd.exe` weitergab. Die betroffene Golden-Testdatei ist vorhanden. Wiederholung desselben unveränderten CTest-Falls mit `C:/Program Files/Git/usr/bin` ausschließlich im **Testprozess-PATH** (bereits in `docs/renderer/README.md` dokumentiert): **1/1**, 23,93 s, Exit **0**; `build/phase-b5x-playtests-git-shell.log`. Keine globale Umgebungsänderung, kein Vendor-Patch, kein ausgelassener oder verkürzter registrierter Test. Die vom unveränderten Upstream-Skript standardmäßig ausgeschlossenen Large-Data-Tests wurden nicht zusätzlich aktiviert.

Neue Tests: `build/phase-b5x-new-release-details.log`, `build/phase-b5x-fallback-release-ctest.log`, `build/phase-b5x-debug-renderer-details.log`, `build/phase-b5x-fallback-debug-ctest.log`. Der Debug-Hauptlauf wurde vor Registrierung des zusätzlichen Fallback-Tests gestartet; dieser wurde anschließend separat registriert, gebaut und erfolgreich ausgeführt. Deshalb ausdrücklich **22+1**, nicht ein erfundener einzelner 23/23-Lauf.

## 30. Bekannte Grenzen und Restprobleme

- Die vorhandene Meldung `invalid idx 0` in den ersten normalen Starts stammt aus der Gildenmarkenverwaltung, nicht aus einem Skinning-Remap. Sie wird separat gezählt, nicht als Skinningfehler verschleiert oder hier außerhalb des Scopes korrigiert.
- Im zweiten normalen GPU-Lauf außerdem `Cannot find item by 1238` aus `PythonItemModule.cpp:48` (Itemauswahl, nicht Skinning). Keine Änderung am Item-/Gameplaycode. Nicht als Grafikvalidierungswarnung oder Bone-Remap-Fehler eingeordnet.
- Nicht vorhandene Clips bleiben als solche dokumentiert; Bind-Pose ist kein ausgeführter Attack-/Death-Clip.
- In den isolierten nativen Paritätsfixtures erscheinen bekannte `ResourceManager ... NOT SUPPORT FILE ...dds`-Diagnosen, weil dort keine Original-Image-Resource-Factories registriert werden und stattdessen die explizite diagnostische Textur gebunden wird. Originaltexturen werden separat in B3 und den normalen/Weltläufen geprüft. Diese ResourceManager-Diagnosen sind keine unterdrückten Diligent-/D3D11-Validierungswarnungen.
- Keine neue Kategorie, keine weiteren Attachments, keine SDK-/CPU-Deformer-Ersetzung. B4-X-Testdateien und alle bisherigen Suiten bleiben erhalten.
- Kein verbleibender B5-X-Skinning-Regressionsfehler gefunden. Die Shellumgebung des Vendor-Tests erforderte den oben dokumentierten prozesslokalen Wiederholungsaufruf; kein Produktfix.

## 31. Empfehlung für B6-X / Git-Diff

**GO für B6-X.** CPU-/GPU-Parität, tatsächliche Bonebuffer-Werte, Material-/Pose-/Hair-LOD-Verhalten, Lifecycle-/Map-/Relog-/Mount-Stress, defensive Fehlerfälle und Shutdown sind im beschriebenen Umfang abgenommen. Alle registrierten Pflichtprüfungen wurden erfolgreich ausgeführt; die bekannten Test-/Beobachtungsgrenzen bleiben oben ausdrücklich festgehalten. **B5-X ist vollständig abgeschlossen.**

Hier wird gestoppt: **B6-X nicht begonnen**, kein Defaultwechsel, keine Performance-Optimierung, keine CPU-/Granny-Ersetzung und keine Asset Runtime. Alle für B5-X gestarteten Spielclients sind beendet.

B5-X-Diff gegenüber dem vorherigen B4-X-Arbeitsstand:

- Neu: `tests/Renderer/SkinningStabilityTest.cpp` – wiederverwendete B4-X-Referenz, neue Stress-/Pixel-/Invalid-Data-Prüfungen.
- Neu: `tests/Renderer/SkinningFallbackParityTest.cpp` – acht deterministische CPU-Fallback-/GPU-Recovery-Bildvergleiche.
- Ergänzt: `tests/Renderer/SkinningGpuReadback.h` – test-only Readback des tatsächlich gebundenen Bonebuffers.
- Ergänzt: `tests/Renderer/CMakeLists.txt` – drei zusätzliche B5-X-Tests, kein bestehender Test entfernt.
- Neu: `tests/Renderer/gpu_skinning_stability_entry.py` – private Originalpaket-Weltfixture mit zwölf Phasen.
- Ergänzt: `tests/Renderer/run_skinning_prototype.ps1` – separater `-Stability`-Modus, vorhandene B3/B4-Modi erhalten.
- Neu: `tests/Renderer/audit_skinning_stability.ps1` – wiederholbare Auswertung vorhandener Diagnosen, Ressourcen und Instanzübergänge.
- Neu: dieser Bericht. Eigene Integrationsstellen mit `ZiiNAN` markiert. Keine Commits/Pushes/Resets, installierte Runtime und Originalpakete unverändert.

`git diff --check` und die separate Whitespace-/Konfliktprüfung der neuen Dateien bestanden. Beide PowerShell-Skripte wurden zusätzlich mit dem Parser geprüft (0 Syntaxfehler); die Python-Weltfixture wurde in beiden vollständigen Live-Läufen ausgeführt. Bestehende LF-/CRLF-Normalisierungshinweise sind keine Whitespacefehler. Der Gesamtdiff gegen HEAD enthält außerdem weiterhin die vorherigen B4-X-Änderungen; diese gehören nicht zum neuen B5-X-Produktumfang.
