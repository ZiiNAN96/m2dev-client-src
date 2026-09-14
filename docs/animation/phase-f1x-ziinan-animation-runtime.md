# F1-X – ZiiNAN Animation Runtime Foundation

Stand: 14.09.2026. Produktionsstandard bleibt Granny; die eigene Runtime ist
explizites Opt-in. Technische und manuelle Abnahme werden getrennt belegt.
F2-X ist nicht Bestandteil dieses Auftrags.

**Zwischenstand nach der Content-Retention-Korrektur:** Release besteht
37/37 Tests in 93,74 s und Debug 37/37 in 137,60 s. Der abschließende GCC-Gate
mit Stall-Instrumentierung besteht 11/11 in 0,39 s.
Die aktuellen nativen Smokes Granny04 und ZiiNAN05 bestehen jeweils in
61,4 s mit Exitcode 0 und allen erfassten Ownern/Retentionsbytes 0.
Manual03 ist trotz sauberem Shutdown wegen bestätigter Bildstillstände
**nicht bestanden**. Manual04 wurde sauber beendet; der Benutzer bestätigt
deutlich reduzierte Hitches, aber verbleibende kurze Bildstillstände.
Die abgeschlossene [Stallprüfung](phase-f1x-stall-audit.md) trennt Import und
laufende Poseauswertung. **F1-X: GO mit akzeptierter First-Use-/Warmup-Limitierung.**
Die Hitches sind **nicht behoben**. Weitere Cache-, Preload- oder persistente
Cacheoptimierungen erfolgen nicht. Technisches GO für den nächsten F2-X-Schritt;
F2-X wurde nicht begonnen. Granny bleibt Produktionsstandard.

## 1. Granny Animation Baseline

Der vollständige Ist-Audit vor Änderungen steht in
[phase-f1x-granny-audit.md](phase-f1x-granny-audit.md), einschließlich
Datei/Funktion, SDK-Aufgabe, F1-X-Grenze und verbleibender Verantwortung.
Die relevante Kette war bereits durch D1-X gekapselt:
Actor -> AssetRuntime::AnimationInstance -> Granny-Provider -> Pose -> GPU.

## 2. Direct Dependency Audit

Vorher: 1 physisches SDK-Include, 9 Pose-Aufrufe, 1 Sampling-Aufruf,
42 Control-Aufrufe, 18 Local-/World-Pose-Typvorkommen, 54 animationsbezogene
SDK-Typvorkommen. Direkte SDK-Animation in den 27 Actor-Dateien bereits **0**.
Definition und reproduzierbare Regex stehen im Baseline-Audit.

## 3. Matrix/Pose Convention

Phase-B bleibt verbindlich: Row-Major-Werte, Row-Vektoren, Translation an
Index 12..14; Local * Parent -> Model. Composite = InverseBind * Model.
Originalkoordinaten, Handedness, inverse Bind und GPU-Upload werden nicht
umgestellt. Normals behalten die vorhandene gewichtete w=0-Multiplikation.

## 4. Animation Runtime Architecture

`src/AnimationRuntime` ist eine eigene C++20-Bibliothek ohne Provider-,
Renderer- oder Plattformabhängigkeiten. Ihr schmaler Header beschreibt
Skeleton, Clip, Tracks, LocalTransform, Pose, Sampling, Evaluation, Blending,
Palette und PlaybackClock. Die Herkunft wird ausschließlich im privaten
`AssetRuntime/Granny/GrannyAnimationAdapter` behandelt.

## 5. RuntimeSkeleton

Eigene Bone-Namen, Parentindices, lokale Bindtransforms, inverse Bindmatrizen
und vorbereitete Auswertungsreihenfolge. Keine geborgten SDK-Pointer.
Initialisierung prüft und normalisiert die Daten vor Verwendung.

## 6. Bone Order

Bone-ID ist der unveränderte Arrayindex. Keine alphabetische oder topologische
Umsortierung der Bones. Die separate Auswertungsreihenfolge verändert weder
Mesh-Remaps noch Attachment-IDs.

## 7. Hierarchy Validation

1..65.536 Bones, eindeutige nichtleere Namen, gültige Parentindices, genau eine
verbundene Wurzel, keine Selbstreferenz, keine Zyklen, maximale Tiefe 256.
Parent-after-child wird unterstützt. Fehler liefern eine Diagnose; die
Initialisierung veröffentlicht keinen teilweise aufgebauten neuen Zustand.

## 8. LocalTransform

Translation float[3], Quaternion xyzw float[4], vollständiges Scale/Shear
float[9]. Letzteres bewahrt den vorhandenen affinen Datenvertrag einschließlich
non-uniform Scale; eine pauschale Scale=1-Annahme wäre falsch. Endliche Werte
werden geprüft, Quaternionen normalisiert. Kein Euler-Zwischenschritt.

## 9. RuntimeAnimationClip

Name, Dauer in Sekunden, Loop-Metadatum, gebundene Bone-Tracks und eigene Keys.
Skeleton-Binding wird einmal vorbereitet. Mehrere Clips teilen ein Skeleton;
der Sampler benötigt weder Bone-Namen noch Quelldokumente.

## 10. Track Storage

Zusammenhängende Key-Vektoren pro Kanal. Import und Binding sind Lade-/
Motionwechselarbeit. Pose- und Matrixpuffer werden vor Sampling vorbereitet;
keine temporären Trackkopien oder pro Bone angelegten Heapobjekte im Frame.

## 11. Translation Sampling

LINEAR-Interpolation zwischen zeitlich benachbarten eigenen Keys; STEP für
diskrete Kanäle. Leere Kanäle verwenden den lokalen Skeleton-Default.

## 12. Rotation Sampling

Shortest-path nlerp mit Quaternion-Normalisierung. Diese bewusst kleine Basis
passt zu eng gebackenen Keys; native quadratische Rotationssplines werden
nicht irrtümlich als lineare Original-Keyframes interpretiert.

## 13. Scale Sampling

Elementweise lineare Interpolation des vollständigen 3x3-Scale/Shear-Anteils.
Uniforme, non-uniforme und vorhandene Scheranteile bleiben darstellbar.

## 14. Interpolation Modes

Inventar von 13 Originalclips: Format 1 `DaK32fC32f`; 857 Identity-,
859 konstante Positions-, 167 konstante Rotations-, 220 konstante Scale/Shear-,
109 lineare Scale/Shear-, 86 quadratische Positions- und 537 quadratische
Quaternionkurven. Keine keyframed-SDK-Kurven in dieser Stichprobe.
Beleg: `build/f1x/curve-inventory.txt`. Nicht die gesamte Assetbibliothek.

## 15. AnimationPose

Ein lokaler Transform pro Bone. Der Aufrufer reserviert die Pose über Prepare;
keine Bind-Pose-Lücken oder uninitialisierten Bones bei fehlenden Kanälen.

## 16. Pose Hierarchy Evaluation

Eigene Quaternion-/Scale-/Translation-Matrix, anschließend Local * Parent
in vorbereiteter Reihenfolge. Ein optionaler Attachment-Parent wird an der
Wurzel angewandt. Nichtendliche Eingaben/Ergebnisse werden zurückgewiesen.

## 17. Bind Pose

Ohne aktive Animation werden lokale Skeleton-Defaults ausgewertet. Ein
Originalasset muss dabei nicht in jeder Composite-Matrix exakt Identity
ergeben; maßgeblich ist sein tatsächlicher Bind-/Initialvertrag.

## 18. Bone Palette Builder

Eigene Multiplikation der gespeicherten Inverse-Bind-Matrix mit der aktuellen
Modellmatrix, in unveränderter Skeletonreihenfolge. Ausgabe verwendet den
vorhandenen Phase-B-Palettenvertrag. Keine Shader-/Weight-/Bonebufferänderung.

## 19. Granny Animation Adapter

Privater Import bestehender Model-/AnimationHandles. Komprimierte Kurven und
SDK-Bind-/Root-Semantik werden einmalig in eigene lokale Keys überführt.
Das ist eine Laufzeit-Importkonvertierung im Speicher, kein GR2->GLB-Konverter
und kein neues Offline-Dateiformat. Die Frameauswertung arbeitet unabhängig
auf diesen Keys. Import-Sampling und Reference-Frame-Sampling werden getrennt
gezählt; die Importqualität wird durch Originalasset-Parität geprüft.

Der Adapter prüft Source-Skeleton/Trackgruppe, Kurvenformat und Grad, bevor
er private SDK-Controls anlegt. Unterstützt sind die inventarisierten
Identity-/Constant-/DaK32fC32f-Kurven bis Grad 2. Andere Kurven, Vector-/Morph-
Gruppen, nichtendliche Daten und ungültige Bindings werden sichtbar abgewiesen.
Vier gebackene Randvarianten erhalten die SDK-Semantik am Anfang/Ende eines
endlichen oder wiederholten Controls. Float-repräsentierbare Importzeiten
vermeiden Präzisionsverlust durch `duration + t`; kanalweise Key-Reduktion
bewahrt enge dokumentierte Fehlerbudgets. Die Reduktion arbeitet iterativ.
Nach dem ersten manuellen Lagbefund tastet der Adapter jeden Bone mit dem
vorhandenen SDK-Bereichssampler separat ab. Konstant/langsam bewegte Bones
werden dadurch nicht mit der feinsten Kurve des gesamten Skeletons verfeinert.
14.580 Vergleiche von Einzelbone- gegen Vollpose-Sampling an originalen Armor-,
LOD-, Wolf-, Boss- und Horse-Daten ergaben exakt gleiche lokale TRS-Werte.
Die Import-/Paritätstoleranzen wurden nicht gelockert.

Grenzen: 1.024 native Bones, 600 s Clipdauer, je Randvariante höchstens
131.072 * Bonezahl gesampelte Bone-Transforms und zusätzlich 2.097.152
SDK-Einzelboneaufrufe, 32.768 gespeicherte Samples pro Bone/Randvariante,
256 MiB adaptive Posepuffer sowie 2 Millionen gespeicherte Kanalkeys über
alle vier Varianten. Dies sind Schutzgrenzen des privaten Adapters, keine
Formatannahmen der Core-API. Der Importzähler zählt jetzt Einzelboneaufrufe;
er ist nicht direkt mit früheren Vollpose-Aufrufzahlen vergleichbar.

## 20. Ownership

Runtime-Strukturen besitzen Namen, Hierarchie, Binddaten und Keys selbst.
SourceHandles sichern private Import-/Control-Lebensdauern; keine Zeiger in
temporäre Importpuffer. Das Modelldokument besitzt das neutrale Skeleton.
Aktive Instanzen halten ihre Clips und die für Controls nötigen SourceHandles.
Lebensdauerzähler erfassen Skeletons, Clips und experimentelle Instanzen
zusätzlich zu den bestehenden Asset-/GPU-Zählern.

Die neue private Prozess-Retention besitzt ausschließlich unveränderliche
neutrale Imports und kopierte Schlüssel. Der Schlüssel enthält SHA256 der
Originalbytes beider Dateien (Modell und Animation), Modell-/Animationsindex
und Skeleton-Binding-ID. Gleiche Bytes können deshalb nach vollständigem
Quelldokumentabbau und unter anderer Asset-ID wiederverwendet werden;
andere Bytes unter derselben Asset-ID ergeben einen anderen Schlüssel.
Die Fingerprints entstehen einmal beim Laden vor dem SDK-Decoding in der
privaten Providerfunktion mit Windows CNG; `bcrypt` ist nur PRIVATE an
`AssetRuntimeGranny` gelinkt. Die neutrale Runtime erhält keine neue
Plattformabhängigkeit. Der Granny-Default behält seine bisherige
Frameauswertung; das Fingerprinting ergänzt Arbeit beim Dokumentladen.
Primärquelle: [Microsoft BCryptHashData](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcrypthashdata).

Starke LRU-Retention ist auf **128 MiB reservierte Key-Vektorkapazität je
Modelldigest/Modellindex und 512 MiB pro Prozess** begrenzt. Zusätzlich gelten
1.024 starke Einträge je Modell, 4.096 starke Einträge insgesamt und
8.192 Einträge im schwachen Live-Index. Geteilte Importobjekte zählen ihre
Key-Allokation nur einmal im Prozess und einmal je betroffenem Modellbudget.
Aktive Besitzer können verdrängte Clips weiter halten; ihre Pins liegen
außerhalb der Retentionsgrenzen. Diese Grenzen beschreiben weder den gesamten
Animationsspeicher noch den Prozessspeicher.

Der zusätzliche schwache Inhaltsindex findet weiterhin aktive Imports auch
nach LRU-Verdrängung und entfernt abgelaufene Runtime-Verweise. Die globalen
Strukturen besitzen weder SourceDocuments noch Skeletons oder SDK-Objekte.
Neutrale Clips können dagegen bewusst länger als ihre Quelldokumente leben.
`ClearImportCache()` gibt die starke Retention und den schwachen Index explizit
nach `Main` und vor dem Owner-Audit frei; verbleibende aktive Pins würden dabei
nicht ungültig. Tests prüfen Source-Owner bereits vor Clear und alle neutralen
Owner sowie RetainedImportKeyBytes danach auf null. Es gibt keinen Diskcache,
keine Persistenz über einen Prozessneustart und kein neues Assetformat.
Fehlgeschlagene Imports geben ihr natives Control sofort frei.

## 21. Reference Mode

`--animation-runtime=granny` verwendet den bisherigen Referenzpfad.
Granny bleibt auch ohne Argument der Produktionsstandard.

## 22. ZiiNAN Runtime Mode

`--animation-runtime=ziinan` wählt die eigene Poseauswertung am realen
AnimationInstance-Seam. Ungültige/widersprüchliche CLI-Werte sind Fehler.
Ein Fehler darf weder still auf die Reference ausweichen noch eine alte
fertige Pose als neu veröffentlichen.

## 23. Parity Harness

**Content-Retention in Release PASS:** 37/37 Tests in 93,74 s,
`AnimationRuntime.GrannyParity` 25,57 s, `AnimationRuntime.ActorPath` 2,35 s.
Die neue Cache-Regression ist darin bestanden; Beleg
`build/f1x/release-content-last-test.log` (`CACHE source-unload/content reload/... PASS`).
Das neue Debug-Gate besteht 37/37 in 137,60 s: GrannyParity 63,79 s,
ActorPath 9,83 s (`build/f1x/debug-content-last-test.log`).
Zuvor bestanden Release 37/37 in 97,13 s
und Debug 37/37 in 129,47 s mit Live-Index, aber ohne Content-Retention;
ihre Paritätsfehler stimmten überein. Keine dieser Prüfungen ersetzt die
manuelle Laufzeitabnahme.

Unabhängig geladene Originaldateien liefern dieselben Skeletons, Clips und
Zeiten an SDK-Referenz und eigene Runtime. Die World-/Composite-Referenz kommt
aus `GrannySampleModelAnimationsAccelerated`; vollständige lokale
Referenztransforms werden separat mit `GrannySampleModelAnimations` gelesen,
weil der beschleunigte Pfad nicht alle lokalen Scratchwerte schreiben muss.
Die eigene Seite verwendet ausschließlich importierte Keys, Sample, Evaluate
und BuildPalette. Reference- und Importzähler bleiben während ihrer
Frameauswertung unverändert.

18 Modell-/Clip-Paare ergeben **198** lokale/World-/Palette-Prüfungen, zusätzlich
fünf Hair-Fälle mit geteilter Body-Pose. ActorPath führt echte Provider-/
ModelInstance-Consumer bis zum produktiven GPU-Draw. Ein absichtlich nicht
unterstützter In-Memory-Kurvengrad prüft sichtbare Fehlerdiagnose, Freigabe des
neu angelegten Controls, leere Fehlerpose, ausbleibenden Fallback und Recovery;
die Originaldatei bleibt unverändert. Die neue Cache-Regression prüft Wiederverwendung nach Ende aller
Instanzen und SourceDocuments, identische Bytes unter anderer Asset-ID,
geänderte gültige Bytes unter derselben ID, LRU-Grenzen und expliziten Abbau.
Sie erhält auch die Regression für noch aktive, aus dem LRU verdrängte Clips:
identisches Runtimeobjekt, keine neuen SDK-Importaufrufe, keine zusätzliche
starke Belegung. Source-Owner sind vor Clear null; alle neutralen, Provider-,
Paletten- und Retentionsbesitzer sind danach null. Details zur getrennten
Prüfung echter Keykapazitäten und deklarierter Budgetfixtures in Abschnitt 43.
Belege: `build/f1x/release-fast-tests.log`, `build/f1x/debug-fast-tests.log` und
`build-c3x/windows/phase-f1x/parity-Release.csv`.

## 24. Sample Times

Pro Modell-/Clip-Paar: 0, 10, 25, 50, 75, 90 und 100 Prozent der Dauer sowie
`duration-epsilon`, `duration+epsilon` und `2*duration+epsilon`, mit
`epsilon=min(0.0001 s, duration*0.001)`. Hinzu kommt eine elfte Pose bei 25 Prozent
mit nichtuniform skaliertem und verschobenem Attachment-Parent. Damit werden
erstes/interiores Loop-Umfeld, exakter Übergang und Zeiten direkt davor/danach
geprüft. Die Werte sind Sekunden ohne implizite FPS-Abhängigkeit; es ist eine
begrenzte Stichprobe und keine exhaustive Prüfung sämtlicher Zwischenzeiten.

## 25. Error Tolerances

Enge, getrennte Grenzen für die Import-/Samplingapproximation; keine
Float-Bitgleichheit. Translation und Position bleiben in Original-Assetunits.
Der Quaternionfehler ist der vorzeichenunabhängige Winkel aus einem in double
normalisierten Skalarprodukt. Matrixfehler sind absolute Komponentenfehler.

| Größe | Unveränderte F1-X-Grenze | Maximum aktuelles Release |
|---|---:|---:|
| Lokale Translation | 2e-4 | 4.57764e-5 |
| Rotation, Radiant | 2e-5 | 5.20313e-6 |
| Scale/Shear-Komponente | 2e-5 | 1.72853e-6 |
| Modellmatrix | 2e-3 | 8.39233e-4 |
| Finale Composite-Palette | 2e-3 | 1.95694e-3 |
| Geskinnte Position | 5e-3 | 8.35419e-4 |
| Geskinnte Normale | 5e-5 | 1.11461e-5 |

Alle Grenzen sind bestanden. Der größte Compositefehler bei Horse run liegt
mit 0,00195694 nahe an der unveränderten Grenze von 0,002; daraus folgt keine
zusätzliche Genauigkeitsreserve für ungeprüfte Clips oder Zeitpunkte.
Diese Grenzen bewerten den approximierten Animationspfad. Die bestehenden
Phase-B-Tests für identische Skinningmatrizen behalten ihre ursprünglichen
strengeren CPU/GPU-Arithmetikgrenzen; Shader und Gewichte werden nicht verändert.

## 26. Vertex Parity

**PASS:** 296.960 skalare Vertexpaar-Vergleiche über alle zehn Clipzeiten:
275.610 Body-/Mob-/Boss-/Mount- und 21.350 Hair-Vertices. Bei 25 Prozent jedes
Clips wertet `BackendTestAccess::Skin` aus `SkinningGpuReadback.h` beide Paletten
mit der unveränderten produktiven SkinVertex-Funktion auf der GPU aus: 29.696
Vertexpaare, entsprechend 59.392 tatsächlich ausgewerteten Vertices mit je
Position und Normale. Keine neue produktive Compute-Deformation.

ActorPath prüft ergänzend echte Vertex-Shader-Draws und liest die tatsächlich
gebundene Bone-Constantbuffer-Palette einschließlich Reihenfolge und Null-Tail
zurück. Skalarvergleich, isolierter GPU-Funktionsvergleich und produktiver Draw
sind getrennte Belege. Die Materialtextur dieses Tests ist diagnostisch;
Originaltexturen und manuelle Darstellung gehören zum Client-Smoke.

## 27. Player Results

**PASS:** Warrior novice, 75 Bones, general wait/walk/run/attack; 44 Posen,
88.280 skalare und 8.828 GPU-Vertexpaare. Zusätzlich novice LOD3 mit run:
11 Posen, 7.860 skalare und 786 GPU-Vertexpaare. Dieses Original-LOD reduziert
die Geometrie von 2.207 auf 786 Vertices, behält aber alle 75 Bones. Daraus
wird kein Nachweis einer reduzierten Bonehierarchie abgeleitet.

Zusätzlich bestanden: `warrior_4-1.gr2`, **74 Bones**, mit
`onehand_sword/run.gr2` und `onehand_sword/combo_01.gr2`: 22 Posen,
51.640 skalare und 5.164 GPU-Vertexpaare. Dieses Rüstungsmodell entspricht der
im manuellen Versuch relevanten Assetfamilie. Je Clip werden zwei zusätzliche
Quelltracks durch das vorhandene Zielbinding ausgelassen und ausdrücklich
als Bindingwarnung erfasst. Die Übereinstimmung der geprüften Posen ist
bestanden; Kampf-/Crowd-Laufzeit und ein vollständiger LOD-Wechsel dieses
Rüstungsmodells werden daraus nicht abgeleitet.

## 28. Mob Results

**PASS:** Wolf mit 40 Bones; laut Original-motlist 00=Idle, 02=Walk, 03=Run,
20=Attack. 44 Posen, 30.480 skalare und 3.048 GPU-Vertexpaare. Der echte
opt-in-ActorPath rendert zusätzlich run durch die unveränderte GPU-Pipeline.

Zusätzlich bestanden: `stray_dog.gr2` mit 38 Bones, 03=Run und 20=Attack:
22 Posen, 13.640 skalare und 1.364 GPU-Vertexpaare. Diese Originalclips ergänzen
die im manuellen Crowd-/Kampfversuch sichtbaren Wildhunde. Ihre numerische
Parität ist unabhängig von dem dort festgestellten wiederholten Importproblem.

## 29. Boss Results

**PASS:** Misterious diseased bosshost, 163 Bones, 00=Idle und 20=Attack.
22 Posen, 28.060 skalare und 2.806 GPU-Vertexpaare. Nichtkonstante
Scale/Shear-Daten sind vertreten; größter Scale/Shear-Komponentenfehler
1.72853e-6, größter Compositefehler 1.72997e-3.

## 30. Mount Results

**PASS für Skeleton-/Clip-/GPU-Parität:** Horse normal, 54 Bones, 00=Idle,
02=Walk, 03=Run; 33 Posen, 55.650 skalare und 5.565 GPU-Vertexpaare.
Der echte opt-in-ActorPath rendert zusätzlich Horse run. Größter Compositefehler
1.95694e-3 und Positionsfehler 8.35419e-4, jeweils innerhalb der unveränderten
Grenzen. Rider-/Saddle-Verknüpfung wird hiervon nicht abgeleitet; dafür ist der
gesonderte kurze Client-Smoke in Abschnitt 50 maßgeblich.

## 31. Hair/Attachment Results

**PASS:** Warrior hair_1_1 an vier Full-Body-Clips und an LOD3 run:
50 Vertexprüfungen mit 21.350 skalaren und 2.135 GPU-Vertexpaaren. Maximaler
Positionsfehler 3.81470e-4, Normalfehler 5.93439e-6. Hair verwendet die aktuelle
Body-Pose; seine CSV-Nullwerte für lokale Pose/Matrix sind Platzhalter für
diesen geteilten Besitzer und keine unabhängigen Nullfehler-Messungen.

Der echte ActorPath prüft Full -> LOD3 -> Full, genau dieselbe Body-Palette ohne
zusätzliches Hair-Sampling und Rückkehr zum ursprünglichen Remap. Die originale
rigide Waffe 00010.gr2 wird an `Bip01 R Hand` gebunden und gerendert; Bone-Lookup
und aktuelle Attachmentmatrix bleiben gültig. Rider-Verknüpfung und visuelle
Fernwirkung werden separat im Client-Smoke abgenommen. Kalte Modell-/Clip-
Kombinationen können weiterhin laden; die Korrektur der in Abschnitt 38
beschriebenen Cache-Lücke benötigt zusätzlich die manuelle Bestätigung.
Verzögerungsfreie wiederholte Rüstungs-/Weltwechsel werden nicht pauschal zugesagt.

## 32. Blending

Eigene Blend-Operation: Translation/ScaleShear lerp, Quaternion shortest-path
nlerp; Gewicht wird auf 0..1 begrenzt, ungültige Posen werden abgewiesen.
Die Core-API benötigt keine Animation State Machine.

Für überlappende bestehende Controls akkumuliert die private Integration
Translation/Scale gewichtet und richtet Quaternionen auf dieselbe Hemisphäre
aus; die Quaternion-Gewichtssumme wird einmal normalisiert. Dadurch entsteht
kein zusätzlicher Reihenfolgefehler durch wiederholte paarweise Normalisierung.
Zwei- und Drei-Control-Übergänge sind gegen die SDK-Palette geprüft.

## 33. Crossfade

**PASS:** Idle -> Run über 0,2 Sekunden sowie schnelle Idle -> Run -> Idle-
Wechsel mit drei überlappenden vorhandenen Controls. Beide Providerpfade
erhalten gleiche Clocks, Blendzeiten und Speedwerte. Der maximale
Compositefehler über diese Playbackprüfung einschließlich Speed/Clip-Ende
ist 0,00105286. Keine neue Graph-/Gameplaylogik; im opt-in-Frame keine
Reference- oder Import-Poseauswertung.

## 34. Looping

Deterministische Sekundenauflösung mit Loop/Clamp und definierter Dauer=0.
Negative Loopzeit wird in den gültigen Zeitraum gewrappt. Ein kontinuierlicher
Quellloop ist Voraussetzung für visuelle Kontinuität.

Der private Import hält vier neutrale Clipvarianten für die vorhandene
SDK-Randsemantik: einzelner begrenzter Zyklus, erster Loop, interiores
Loop-Umfeld und letzter Zyklus. Lokale Zeit und Loopindex werden beim Import
getrennt gesetzt, um Float-Auslöschung durch `(duration+t)-duration` zu
vermeiden. Die Runtime wählt anhand bestehender Control-Metadaten die bereits
importierte Variante; kein erneuter Trackimport im Frame. Alle in Abschnitt 24
genannten Originalasset-Grenzzeiten und das finite Clip-Ende sind bestanden.

## 35. Playback Speed

Eigene PlaybackClock unterstützt Rate und Pause/Resume; 0.5/1/2 werden
im Core geprüft. Reale Provider-Sessions bestätigen 0,5-faches Idle,
2-faches Run im Crossfade und einen endlichen Clip mit Rate 1 gegen die
Granny-Referenz, einschließlich gleichem IsPlaying-Zustand und SetMotionAtEnd.
Die reale Actor-Integration bewahrt bestehende Control-Clocks,
SpeedRatio und das historische ChangeMotion-Offset von 0,3 Sekunden.

## 36. Root Motion Decision

Root Motion ist relevant: Warrior und Wolf Walk/Run sowie Horse führen
LoopTranslation. Actor::__AccumulationMovement konsumiert die Ausgabe von
GrannyUpdateModelMatrix. Deshalb bleiben Controls und diese Bewegungs-
Extraktion privat erhalten. Granny liefert im opt-in-Frame keine Bone-Pose;
der Meilenstein behauptet keine Granny-freie Gameplay-Bewegung.

Das Originalinventar zeigt Y-LoopTranslation für Warrior Walk/Run
-176,858/-300, Wolf -75,8081/-255,45 und Horse -233,308/-493,784.
Diese Werte werden nicht in neue Gameplay-Bewegung übernommen; Servermovement,
SpeedRatio-Verwendung und Angriffstiming bleiben erhalten. Eine neutrale
Root-Motion-Extraktion ist hier nicht implementiert.

## 37. Missing Track Semantics

Fehlende Kanäle und nicht animierte Zielbones verwenden den definierten
Skeleton-Default. Ungültige Zielindices der neutralen Clip-API werden
abgewiesen. Im privaten Adapter darf das vorhandene SDK-Binding zusätzliche
Quelltracks aus einer vollständigen Animation bei einem anderen Ziel-Skeleton
auslassen; ihre Namen werden als `ignoredSourceTracks` erfasst und als
Bindingwarnung ausgegeben. Unbenannte Tracks bleiben Fehler.

Die 74-Bone-Rüstung `warrior_4-1.gr2` hat bei den beiden geprüften
onehand_sword-Clips jeweils **zwei** ausgelassene Quelltracks. Die verbleibenden
16 Modell-/Clip-Paare, einschließlich 75-Bone-novice-LOD3 und 38-Bone-StrayDog,
haben ignoredSourceTracks=0. Damit wird die echte reduzierte Zielbindung
geprüft, ohne fehlende Zielbones zu erfinden. Ein broadcasteter Clip ohne
passende Trackgruppe für ein starres Attachment bleibt `NoMatchingTracks`.

## 38. Performance Sanity

Die eigene skalare Foundation ist weiterhin **langsamer** als der optimierte
Granny-Sampler. Je Clip wurden 64 warme Wiederholungen gemessen: Referenz =
vorhandenes beschleunigtes Sampling plus World-/Composite-Erzeugung; Runtime =
Sample plus Evaluate plus BuildPalette. Import, GPU-Readbacks und Rendering
sind ausgeschlossen. Diese kurze Messung ist Orientierung, kein statistisch
abgesicherter Performancebenchmark und keine Messung der manuellen Crowd-Szene.
Die Zahlen dieses Abschnitts stammen aus der gesicherten Messreihe
`build/f1x/sliced-parity-Release.csv`, vor der reinen Live-Cache-Korrektur.

| Gruppe | Granny, Mittel µs | ZiiNAN, Mittel µs | Faktor |
|---|---:|---:|---:|
| Warrior, 4 Clips | 2,025 | 10,056 | 4,97 |
| Warrior LOD3, run | 1,916 | 10,048 | 5,25 |
| Rüstung 74 Bones, 2 Clips | 1,977 | 10,058 | 5,09 |
| Wolf, 4 Clips | 1,245 | 5,468 | 4,39 |
| StrayDog, 2 Clips | 1,300 | 5,765 | 4,43 |
| Boss163, 2 Clips | 5,714 | 21,609 | 3,78 |
| Horse, 3 Clips | 1,341 | 7,229 | 5,39 |

Über alle 18 Clipmittel: 2,055 -> 9,372 µs, **4,56-facher CPU-Aufwand**.
Für einen Vergleich bei gleicher Auswahl sind die ursprünglichen 14 Paare
maßgeblich: vorher 2,329 -> 10,023 µs, Faktor 4,30; aktuell 2,175 -> 9,789 µs,
Faktor 4,50. Die kleinen absoluten Unterschiede dieses Kurztests sind kein
belastbarer Nachweis besserer warmer Framezeiten. Aktuelle eigene Einzelwerte
liegen zwischen 5,367 und 21,627 µs, die Referenz zwischen 1,161 und 5,772 µs.
Kein Hochrechnen auf FPS und keine pauschale Crowd-GO-Aussage. Vergleichsbeleg:
`build/f1x/pre-lagfix-parity-Release.csv` gegen
`build/f1x/sliced-parity-Release.csv`. Diese zeitliche Messreihe wurde vor der
letzten reinen Cache-Lookup-Korrektur gesichert; ihr warmer Sampler ist gleich.

Der Import verfeinert nun jeden Bone separat mit festem Scratchspeicher.
Dadurch erzwingt eine schwierige Kurve nicht mehr zusätzliche Keys und volle
Poseauswertungen für alle übrigen Bones. Die bestehenden externen
Paritätsgrenzen bleiben unverändert. Die aktuelle Release-Paritätsstichprobe
misst bis 650,822 ms pro erstmaliger Modell-/Clip-Kombination, vorher bis
3.788,622 ms bei den ursprünglichen 14 Paaren. Die unabhängige Sonde derselben
sieben Vergleichsfälle liefert:

| Modell/Clip | Keys aktuell | Keykapazität vorher -> aktuell, MiB | Import vorher -> aktuell, ms |
|---|---:|---:|---:|
| Warrior run | 225.649 | 9.03 -> 5.17 | 2223 -> 1025 |
| Wolf attack | 416.377 | 16.54 -> 9.53 | 3684 -> 1039 |
| Boss163 idle | 241.531 | 9.94 -> 5.88 | 5707 -> 768 |
| Boss163 attack | 247.295 | 10.13 -> 6.01 | 4472 -> 786 |
| Horse idle | 29.682 | 1.15 -> 0.68 | 239 -> 78 |
| Horse walk | 159.105 | 6.03 -> 3.65 | 813 -> 312 |
| Horse run | 231.168 | 8.98 -> 5.30 | 2254 -> 423 |

Belege: `build/f1x/import-metrics-final.txt` und
`build/f1x/import-metrics-sliced-cache.txt`. Keybytes verwenden
`capacity()*sizeof(Keyframe)` über alle vier Randvarianten; Track-/Skeleton-
Metadaten, Quelldokumente und temporärer Import-Speicher fehlen darin.
Die Summe der sieben Keykapazitäten sinkt von 64.822.080 auf 37.977.960 Bytes
(61,82 -> 36,22 MiB); die nacheinander gemessenen Importzeiten summieren sich
von 19.392 auf 4.431 ms. Das ist keine Messung eines gemeinsamen Prozesspeaks.
Die Zahl der nativen Samplingaufrufe ist nach der Umstellung auf einzelne
Bones nicht direkt mit der früheren Zahl vollständiger Poseaufrufe vergleichbar.

Die bisherigen 32 MiB pro Modelldokument und 128 MiB pro Prozess waren für
die manuelle Szene zu klein. Die geloggten eindeutigen Modell-/Clip-Schlüssel
ergeben zusammen 313,49 MiB Keykapazität, darunter 73,95 MiB für zwölf
Wildhundclips und 71,89 MiB für 17 Warriorclips. Diese Summe unterschiedlicher
Imports ist kein gemessener gleichzeitiger RSS- oder Retentionspeak.

Die neue private Retention verwendet die Inhaltsidentität beider Quelldateien
und hält bis 128 MiB je Modelldigest/Modellindex sowie 512 MiB insgesamt.
Dadurch können fertige neutrale Clips auch das Ende aller SourceDocuments
überleben. Aktive Pins sind weiterhin nicht im starken Retentionsbudget
enthalten; Details und zusätzliche Eintragsgrenzen stehen in Abschnitt 20.
Die neuen Quellschlüssel- und Budgetregressionen bestehen in Release und
Debug; beide neuen nativen Smokes sind ebenfalls bestanden. Der Benutzer
akzeptiert die verbleibenden First-Use-/Warmup-Hitches für F1-X.
Erstimporte und echte spätere Misses bleiben synchron; die Sonde zeigt auch
nach der Importreduktion bis ungefähr eine Sekunde. Die größeren Grenzen
begründen daher noch keine Behauptung ruckelfreier Laufzeit.

**Manuelle Laufzeitabnahme weiterhin fehlgeschlagen:** manual-01 zeigte starke
Verzögerungen auch bei wiederholten Animationen und endete mit Renderer-Exit 5.
manual-02 zeigte erneut Verzögerungen beim Kampf und Laufen. Dabei wurden
gleiche Wildhundclips bis zu 16-mal importiert: Die LRU-Verdrängung entfernte
auch deren Sucheintrag, obwohl aktive Instanzen den fertigen Clip noch hielten.
Die ergänzende Suche über schwache Verweise bestand danach die dokumentierten
Release-/Debug-Gates; die 23 CSV-Zeilen behielten dieselben Paritätsfehler.

Manual03 enthielt diesen Live-Index-Fix, zeigte laut Benutzer aber weiter
kurze Stillstände des ganzen Bildes beim Laufen und Kämpfen. Verbleibende
LRU- und Source-Lifetime-Misses führten erneut zu synchronen Imports.
Der Agent beendete PID 67176 über Fenster-X: Exitcode 0, 140 Imports,
36,612591 s gesamte Importzeit, 85 Evictions, 1.780.982 eigene Posen und
0 Reference-Posen. CPU-Deformation, Fallbacks und Runtimefehler blieben 0;
alle Owner und RetainedImportKeyBytes waren nach Shutdown 0. Sauberer
Shutdown ist kein PASS der manuellen Performanceprüfung.

Der separate Renderer-Abbruch aus manual-01 wird durch die Importkorrekturen
nicht als behoben erklärt. Die neue Content-Retention muss manuell erneut
geprüft werden. Kalte Importkosten und der etwa vier- bis fünffache warme
CPU-Aufwand bleiben Grenzen der Foundation; Granny bleibt Produktionsstandard.

## 39. ozz-animation Decision

Keine neue Dependency für F1-X. ozz bietet Skeleton/Sampling/Blending,
SoA-Daten und MIT-Lizenz; benötigt aber eigene RawSkeleton-/RawAnimation-
Konvertierung und Anpassung unserer Matrix-/ScaleShear-Grenze.
Der Runtime-Code ist OS-neutral; Android wird upstream als inoffiziell
unterstützt geführt. Das ist kein hier durchgeführter Android-Nachweis.
Quellen: [Runtime-Dokumentation](https://guillaumeblanc.github.io/ozz-animation/documentation/animation_runtime/),
[Plattformen](https://guillaumeblanc.github.io/ozz-animation/documentation/features/),
[Lizenz](https://github.com/guillaumeblanc/ozz-animation/blob/master/LICENSE.md).

Private CMake-Einbindung wäre über ozz_animation/ozz_base mit deaktivierten
Tools/Samples/Tests möglich. Aktuelles upstream-CMake verlangt 3.30; keine
Version wurde in diesen Client aufgenommen. Die kleine eigene Basis spart
eine zweite Konvertierungs-/Datenordnungsschicht für diesen Meilenstein.
[CMake-Quelle](https://github.com/guillaumeblanc/ozz-animation/blob/master/CMakeLists.txt).

## 40. GLB/F2 Readiness

Die neutralen Tracks können aus beliebigen eigenen Daten gefüllt werden.
Der bestehende GlTFProvider validiert LINEAR/STEP/CUBICSPLINE-Samples, bewahrt
derzeit aber nur Kanalmetadaten; Times/Outputs werden nicht dauerhaft
exportiert. F2 muss diese Werte übernehmen, Nodes deterministisch auf Bones
abbilden und CubicSpline explizit behandeln/konvertieren. Kein produktiver
GLB-Character und keine GR2-Konvertierung wurde für F1-X eingeführt.

## 41. Public Header Leak Test

Automatisierte Prüfung auf SDK-/Loader-/OS-/Renderer-/Assimp-/ozz-Leaks sowie
portable Core-Kompilierung: **PASS** im Release-, Debug- und GCC/LP64-Gate.
`AnimationRuntime.Contracts` verwendet ausschließlich die öffentliche API.

## 42. Granny Dependency Before/After

Finale Zählung über 724 Quelldateien; Kommentare werden vor dem Zählen entfernt.
Reproduzierbar mit `tests/AnimationRuntime/audit_dependencies.ps1`.

| Messgröße | Vorher | Nachher |
|---|---:|---:|
| Direkte SDK-Includes | 1 | 1 |
| Pose-Aufrufe | 9 | 12 |
| Sampling-Aufrufe | 1 | 2 |
| Control-Aufrufe | 42 | 57 |
| Local-/World-Pose-Typvorkommen | 18 | 19 |
| Animationsbezogene SDK-Typvorkommen | 54 | 60 |
| Direkte Animation-SDK-Nutzung in 27 Actor-Dateien | 0 | 0 |

Alle gezählten SDK-Aufrufe liegen im privaten Provider/Adapter. Die neue
AnimationRuntime enthält bei sämtlichen Messgrößen 0. Die Actor-Nullwerte
stammen bereits aus D1-X; F1-X fügt einen unabhängigen Auswertungspfad hinzu,
verringert aber nicht künstlich die statische Gesamtzahl. Die zweite
Sampling-Aufrufstelle ist der ausdrücklich gezählte einmalige Import.

## 43. Unit Tests

**PASS im dokumentierten Gate:** Hierarchy/invalid data, Bindpose für
Root/Chain/Branches, Sampling TRS einschließlich non-uniform Scale/Shear,
Shortest-path, Loop/Clamp, Missing Tracks/Bindings, Blend, Matrixhierarchie,
Palette, Playback-Raten 0,5/1/2 und Pause/Resume. Ungültige Hierarchien,
Zeitwerte, Quaternions, Tracktargets und nichtendliche Daten werden abgewiesen.
Ein instrumentierter Allokationszähler beweist 100 vorbereitete Frames ohne
Heapallokation. Der Originalasset-Test prüft Importfehler ohne Fallback,
anschließende gültige Wiederverwendung und vollständigen Abbau.

Die frühere Cache-Regression bestand mit tatsächlich reservierten Keyarrays
an der damaligen 32-MiB-Dokumentgrenze. Auch der in manual-02 entdeckte Fall
eines aus dem LRU entfernten, aber aktiv gehaltenen Clips bestand: identisches
Importobjekt, kein erneutes SDK-Sampling, Freigabe nach dem letzten Besitzer.

Die neue `ImportCacheLifetime`-Regression besteht im Release-Gate mit
Content-Retention (37/37, 93,74 s) und im neuen Debug-Gate (37/37, 137,60 s).
Sie prüft:

- Vollständigen Abbau von Actors und beiden SourceDocuments bei weiterhin
  zurückgehaltenem neutralem Import; die schwachen Source-Verweise müssen ablaufen.
- Erneutes Laden identischer Originalbytes unter anderen Asset-IDs: dasselbe
  Importobjekt und keine neuen SDK-Importaufrufe, auch im echten Providerpfad.
- Dieselbe Asset-ID mit anderen gültigen Animationsbytes beziehungsweise
  anderen gültigen Modellbytes: neuer Import; andere Modellindices und
  Binding-IDs dürfen ebenfalls keinen falschen Treffer ergeben.
- 128-MiB-Modellgrenze und 512-MiB-Prozessgrenze mit LRU-Reihenfolge sowie
  Wiederverwendung eines verdrängten, weiterhin aktiv gehaltenen Imports.
- Source-Owner null vor explizitem Clear; neutrale Owner und aktuelle
  Retentionsbytes null danach, einschließlich wiederholtem Clear.

Die neuen Budgetfälle verwenden kleine gültige neutrale Clips mit ausdrücklich
**deklariertem Byteumfang**, um beide Produktionsgrenzen ohne künstliche
512-MiB-Allokation zu prüfen. Ein separater Teil mit einem realen Originalimport
vergleicht die gespeicherten Keybytes mit der tatsächlichen Summe der
Key-Vektorkapazitäten aller vier Randvarianten. Das prüft reales Accounting
und die Budgetlogik getrennt; es ist kein physischer 512-MiB-Stresstest.
ActorPath prüft ebenfalls Source-Abbau vor Clear und vollständigen neutralen
Abbau danach. Der Importfehler-Test leert vorher die Prozess-Retention, damit
ein Treffer die absichtliche ungültige In-Memory-Quellenänderung nicht verdeckt.

`Renderer.ProductionGpu` prüft weiterhin, dass ein absichtlicher
Legacy-State-Fehler Present=false liefert und genau einmal mit Grund und den
richtigen Subsystemflags protokolliert wird; ohne Diligent-Drawfehler.

`AnimationRuntime.StallAudit` ergänzt gezielt die Messinstrumentierung:
standardmäßig deaktiviert, vollständige Inhaltsidentitäten, korrekte
Aggregation über mehrere Updates/ausgelassene Bildausgaben, getrennte
inaktive/minimierte Intervalle, Zählerdifferenzen, verschachtelte Zeitbereiche,
einmaliger Scope-Abschluss, feste Puffergrenzen und konsistente CSV-Ausgabe.
Der Test ist ohne Windows-/Granny-/Rendererabhängigkeit unter GCC lauffähig.

## 44. Original Asset Parity

**Paritätsmessreihe vor der Content-Retention:** Release-Gate PASS 37/37,
97,13 s, einschließlich der damaligen Live-Cache-Regression. Die neue
Content-Revision besteht ebenfalls 37/37 in 93,74 s; alle 23 CSV-Zeilen und
13 Nicht-Timing-Spalten sind gegenüber Live-Index- und gesicherter
Sliced-Messreihe identisch. Die folgende Release-CSV umfasst 18 Modell-/Clip-Paare
und 198 Posevergleiche. Alle Paritätsfehler sind gegenüber der vorherigen
gesicherten Messreihe unverändert. Ihre Werte stammen aus
`build-c3x/windows/phase-f1x/parity-Release.csv`, die Gruppensummen aus
`build/f1x/final-release-parity-summary.json`. Matrixmax ist hier das Maximum
über Modell- und Composite-Komponenten; Winkel in Radiant. Abschließende
Build-/Debug-Zeiten stehen in Abschnitt 45/46.

| Asset | Clip | Samples | Max. Translation | Max. Rotation | Max. Matrix | Ergebnis |
|---|---|---:|---:|---:|---:|---|
| Warrior | general/wait | 11 | 1.52588e-05 | 4.03379e-06 | 9.68933e-04 | PASS |
| Warrior | general/walk | 11 | 1.52588e-05 | 3.94225e-06 | 1.06812e-03 | PASS |
| Warrior | general/run | 11 | 2.28882e-05 | 5.15916e-06 | 1.15204e-03 | PASS |
| Warrior | general/attack | 11 | 1.52588e-05 | 2.80189e-06 | 8.39233e-04 | PASS |
| WarriorLod3 | general/run | 11 | 2.28882e-05 | 5.15916e-06 | 1.15204e-03 | PASS |
| Wolf | 00, idle | 11 | 1.52588e-05 | 4.31043e-06 | 4.36783e-04 | PASS |
| Wolf | 02, walk | 11 | 1.90735e-05 | 4.93856e-06 | 7.71999e-04 | PASS |
| Wolf | 03, run | 11 | 1.76430e-05 | 4.39865e-06 | 5.31197e-04 | PASS |
| Wolf | 20, attack | 11 | 2.28882e-05 | 3.43917e-06 | 2.25067e-04 | PASS |
| Boss163 | 00, idle | 11 | 0 | 4.88940e-06 | 1.52588e-03 | PASS |
| Boss163 | 20, attack | 11 | 0 | 4.88522e-06 | 1.72997e-03 | PASS |
| Horse | 00, idle | 11 | 4.76837e-06 | 1.15116e-06 | 4.15802e-04 | PASS |
| Horse | 02, walk | 11 | 1.52588e-05 | 4.43464e-06 | 1.12629e-03 | PASS |
| Horse | 03, run | 11 | 4.57764e-05 | 4.46040e-06 | 1.95694e-03 | PASS |
| ArmoredWarrior | onehand_sword/run | 11 | 2.28882e-05 | 5.15916e-06 | 1.12915e-03 | PASS |
| ArmoredWarrior | onehand_sword/combo_01 | 11 | 1.10269e-05 | 3.38699e-06 | 7.07626e-04 | PASS |
| StrayDog | 03, run | 11 | 1.52588e-05 | 3.55685e-06 | 3.70026e-04 | PASS |
| StrayDog | 20, attack | 11 | 1.33514e-05 | 5.20313e-06 | 4.34875e-04 | PASS |

Getrennte Maxima über die Clips jeder Gruppe:

| Gruppe | Scale/Shear | Modellmatrix | Palette | Vertexposition | Normale |
|---|---:|---:|---:|---:|---:|
| Warrior | 0 | 3.81470e-04 | 1.15204e-03 | 3.54767e-04 | 9.24617e-06 |
| WarriorLod3 | 0 | 3.43323e-04 | 1.15204e-03 | 3.50952e-04 | 8.35210e-06 |
| ArmoredWarrior | 0 | 3.29971e-04 | 1.12915e-03 | 3.43323e-04 | 7.59959e-06 |
| Wolf | 0 | 7.71999e-04 | 6.14405e-04 | 7.53403e-04 | 9.22382e-06 |
| StrayDog | 0 | 3.30925e-04 | 4.34875e-04 | 3.17574e-04 | 7.51019e-06 |
| Boss163 | 1.72853e-06 | 8.39233e-04 | 1.72997e-03 | 8.31604e-04 | 8.16584e-06 |
| Horse | 0 | 8.23975e-04 | 1.95694e-03 | 8.35419e-04 | 1.11461e-05 |
| Hair, 5 Fälle | Body-Pose | Body-Pose | Body-Pose | 3.81470e-04 | 5.93439e-06 |

Die fünf Hair-Fälle ergänzen 50 Vertexzeitpunkte, keine 50 unabhängig
gesampelten lokalen Posen. Insgesamt sind 296.960 skalare und 29.696
GPU-Vertexpaar-Vergleiche bestanden. Playback-/Fehlerregression und echte
Actor-Draws sind ebenfalls bestanden; ihre separaten Aussagen stehen in
Abschnitt 23, 31 und 33. Alle externen Toleranzen bleiben unverändert.
Automatisierte Parität ersetzt keine manuelle Visual- oder Laufzeitabnahme.
CSV, Logs und binäre Artefakte bleiben in ignorierten Buildverzeichnissen.

## 45. Release Build

**PASS der Content-Retention:** Release-Acceptance-Build Exitcode 0,
112,81 s; **37/37 Fast-Gate-Tests**, 93,74 s. Darin Originalasset-Parität
25,57 s und echter Actor-/Diligent-Pfad 2,35 s. Das Gate verwendet den kleinen
Hair-LOD-Modus, keine vollständige 583-Fälle-Suite.
Logs: `build/f1x/release-content-build.log`, `release-fast-tests.log` und
`release-content-last-test.log`. Die Gatezeiten sind keine
Performancevergleichswerte. Dieser inkrementelle Release-Build enthält keine
Warnung. Frühere vollständige Builds hatten bekannte Third-Party-PDB-Warnungen
LNK4099 und drei C4834-Warnungen im unveränderten `MarkManager.cpp`;
daraus folgt keine generelle Warning-free-Aussage für das Projekt.

Nach Ergänzung der Stall-Instrumentierung: Messclient und zugehörige Targets
erneut gebaut, Exitcode 0; letzter inkrementeller Build 45,66 s
(`stall-release-final-build.log`). Gezielte Runtime-/Startoptionen-Prüfungen
6/6 in 29,22 s einschließlich Parität und ActorPath; nach finaler Benennung
der CSV-Spalten Format-/Header-/Auditchecks 4/4 in 0,27 s.
Logs `stall-release-tests.log` und `stall-final-format-tests.log`.
Diese gezielte Nachprüfung wird nicht als neuer vollständiger 37-Test-Gate
ausgegeben. Bekannte LNK4099-Warnungen der vorgebauten Bibliotheken bleiben.

## 46. Debug Build

**Build PASS der Content-Retention**, Exitcode 0, 95,66 s für alle
Acceptance-Targets einschließlich Client und Offline-Tool.
**Debug-Gate 37/37 PASS, 137,60 s**, Originalasset-Parität 63,79 s und
ActorPath 9,83 s. Logs `build/f1x/debug-content-build.log`,
`debug-fast-tests.log` und `debug-content-last-test.log`.
Vor der Importoptimierung überschritt die ursprüngliche Paritätsprüfung
das 180-s-Limit; mit Debug-Limit 300 s bestand sie in 196,36 s. Der finale
optimierte Test besteht innerhalb dieses Limits ohne Retry. Das neue Gate
lief vor den nativen Smokes. Gatezeiten sind keine Performancevergleichswerte.
Bekannte LNK4099-PDB-Warnungen sowie LNK4075/LNK4098 bleiben ausgewiesen;
keine neue Core-Warnung.

Die abschließende Stall-Instrumentierung einschließlich Client, Parity-/Actor-
Targets und bestehendem Terrain-GPU-Test ist in Debug gebaut: Exitcode 0,
letzter inkrementeller Abschluss 6,83 s (`stall-debug-final-build.log`).
Die fünf kurzen Contract-/Startoptionen-/Header-/Auditprüfungen bestehen in
0,79 s (`stall-debug-tests.log`). Ein zuvor falsch angegebener Buildzielname
war ein Aufruffehler, kein Compiler- oder Runtimefehler, und ist korrigiert.
Nach der akzeptierten Limitierung wurde kein weiterer Profiling-Lauf gestartet.

## 47. GCC/LP64

Bestehender Cygwin/GCC-Common-Build nach der Stall-Instrumentierung, ohne
Windowsclient: **PASS 11/11**, 0,39 s CTest, Build Exitcode 0.
Logs `build/f1x/stall-gcc-build.log` und `stall-gcc-tests.log`.
Zusätzlich isolierter Core-Build mit
`-Wall -Wextra -Wpedantic -Werror` erfolgreich. Kein Linux-Vollclient- oder
Android-Nachweis.

## 48. GR2 Regression

**PASS der Content-Retention vor der Stall-Instrumentierung:**
`build/f1x/runtime/f1x-granny-04`,
61,4 s, sechs Actors mit Player/NPC/Mob/Boss/Mount, Hair/Weapon und Rider,
Near/Far/Near sowie Idle/Walk/Run/Attack. Exitcode 0, 26.723
ReferencePoseSamples, 0 IndependentPoseSamples, 0 Imports, keine
Deformationen/Fallbacks und alle Owner beim Shutdown 0. 3.606 Weltframes,
davon 879/909/909/909 in den vier Animationsstufen.
Der frühere 61,6-s-Smoke `f1x-granny-02` bestand zusätzlich drei
Originalweltszenen A1/B1/A1 mit Rüstungswechseln. Aus unterschiedlichen
Szenarien oder nebenläufigen Tests werden keine FPS-Vergleiche abgeleitet.

## 49. GPU Skinning Regression

**PASS der Content-Retention-Smokes vor der Stall-Instrumentierung:** Unveränderte Phase-B-GPU-Pipeline.
Granny-Smoke 04: 25.810 GPUFrames;
ZiiNAN-Smoke 05: 22.834 GPUFrames (dieser Zähler zählt Skin-Draws, keine
World-FPS). In beiden Läufen AllCPUDeformationCalls=0,
AllCPUDeformationVertices=0, GPUFallbacks=0 und SkinPreparationFailures=0.
Die aktuellen Phase-B-Fast-Gate- und realen ActorPath-Prüfungen bestehen.

## 50. Native Runtime Smoke

**PASS mit der Content-Retention vor der Stall-Instrumentierung**,
`build/f1x/runtime/f1x-ziinan-05`: private, SHA256-identische Kopie
des damaligen Release-Clients, explizit `--animation-runtime=ziinan` und
produktiver GPU-Default. 60-s-Szenario in A1 mit sechs Actors: Warrior,
NPC9003, Wolf101, Boss691, Horse20101 und separatem Rider auf Mount20104.
Hair/Weapon sowie Near -> Far -> Near bleiben im vorhandenen Actorpfad.
Ein neuer Root-Pack nur in der Testkopie ersetzt den Login durch die Fixture.
Der originale Client-/Assetbestand bleibt unverändert.

Gesamte Prozesslaufzeit 61,4 s, 3.160 gerenderte Weltframes. Idle/Walk/Run/
Attack bei Player und Mob wurden jeweils in 693/819/809/839 Frames präsentiert.
Beide erforderlichen Nah-/Fernaufnahmen liegen vor. Die Logs bestätigen
alle sechs Actor-/Attachment-/Rider-Pfade, 23.747 IndependentPoseSamples,
0 ReferencePoseSamples, 0 AnimationRuntimeFailures, 32 Clipimporte
mit 24.195.600 Einzelbone-Importaufrufen. Gesamte Importzeit 7,370 s,
größter Import 0,727 s; drei Cachetreffer, keine Eviction. Größte starke
Cache-Keyretention 109.128.600 Bytes; nach Shutdown 0. Fünf sichtbare
Bindingwarnungen für jeweils einen im Ziel fehlenden Quelltrack. Keine
stillen Fallbacks. Die sechs Actors belegen Funktionen, keinen Crowd-Durchsatz.

Der erste Opt-in-Smoke mit drei Welt- und wiederholten Rüstungswechseln
(`f1x-ziinan-01`) hatte bereits Exitcode 0 und alle Ressourcen 0, war aber
**als Sicht-Smoke nicht bestanden**: 80 Imports ließen nur 71 Renderframes;
mehrere Screenshotaufträge wurden während Update-Catch-up vor dem nächsten
Present angefordert. Die Fixture fordert Aufnahmen nun beim Rendern an und
prüft präsentierte Frames für jeden Bewegungszustand. Die fokussierten
Smokes 02 bis 05 beheben diese Evidenzlücke. Sechs Actors erzeugen jedoch
nicht den in den manuellen Tests 02/03 beobachteten Cacheverdrängungsdruck.
Die aktuelle Content-Retention besteht den Smoke. Der anschließende manuelle
Stall-Audit und die akzeptierte First-Use-Grenze sind unten dokumentiert.

## 51. Manual Visual Result

Die zwei automatisierten Aufnahmen des aktuellen ZiiNAN05-Smokes wurden
visuell geöffnet: plausible Warrior-Pose, Haare und Schwert im Nahbild;
Boss, Horse und Rider im
Fernbild, keine sichtbaren Bone-Explosionen. Einzelbilder belegen kein
zeitliches Jitterverhalten und ersetzen keinen Login-Test.

Der erste SHA256-identische native Client wurde explizit im ZiiNAN-Modus gestartet:
`build/f1x/runtime/f1x-manual-01`, PID 71504. Login/Charakterauswahl/Ingame
wurden erreicht; der Benutzer meldet **starkes Laggen auch bei wiederholten
Animationen**. Dieser manuelle Lauf ist **NICHT bestanden**. Zusätzlich
beendete der Client mit Rendererfehler/Exitcode 5. Die damalige generische
Terrainmeldung lokalisierte den Fehler nicht eindeutig.

Der zweite Lauf `f1x-manual-02` erreichte erneut Ingame. Der Benutzer meldete
weiterhin Lag beim Kämpfen und Laufen. Die frischen Importmeldungen bewiesen
wiederholte Konvertierung identischer Wildhund-Clips (u.a. 31.gr2 mindestens
16-mal), obwohl andere Actors diese Daten noch hielten. Insgesamt 191
Clipimporte, 63,849 s Importzeit und 150 Cache-Evictions. Der Agent schloss
diesen Lauf über das beobachtete Fenster-X: Exitcode 0, keine neue
Rendererfehlermeldung, 2.795.757 unabhängige Posen, Reference=0, Fehler=0,
CPU-Deformation/Fallback=0 und alle Owner/RetainedImportKeyBytes=0.
Dies bestätigt den Shutdown, nicht die manuelle Performanceabnahme.

Der damalige Cache-Fix erhält schwache Verweise auf weiterhin aktive Imports
und bestand die zugehörige Release-/Debug-Regression. Dennoch meldete der
Benutzer in `f1x-manual-03`, PID 67176, weiter kurze Stillstände des ganzen
Bildes beim Laufen und Kämpfen. **Manual03 ist NICHT bestanden.** Der Agent
beendete den Lauf anschließend über Fenster-X: Exitcode 0, 140 Imports,
36,612591 s Importzeit, 85 Evictions, 1.780.982 unabhängige Posen,
Reference=0, Runtimefehler=0, CPU-Deformation/Fallback=0 und alle
Owner/RetainedImportKeyBytes=0. Der frühere Zwischenstand mit 49 eindeutigen
Imports war kein abschließender Nachweis ausbleibender Wiederholungen.

Verbleibende echte LRU- und Source-Lifetime-Misses werden nun mit der in
Abschnitt 20 beschriebenen neutralen Content-Retention adressiert. Die neuen
Release-/Debug-Tests und beide neuen Smokes sind bestanden.
`f1x-manual-04`, PID 71320, läuft als separate Kopie derselben geprüften
Release-Binärdatei mit `--animation-runtime=ziinan`. SHA256:
`0869DDA6B607BF1733473454734D838D253F00B0D8790DEDF49269272009EFB9`.
Der Benutzer bestätigt deutlich reduzierte Hitches, aber weiterhin kurze
Stillstände des gesamten Bildes. Der Agent schloss diesen Lauf über Fenster-X:
Exitcode 0, 66 Imports, 16,465049 s gesamte Importzeit, maximal 0,750173 s,
3.714 Cachetreffer und keine Eviction. 717.067 unabhängige Posen, Reference=0,
Runtimefehler=0, CPU-Deformation/Fallback=0 und alle Owner/Retentionsbytes=0.
Das belegt Verbesserung und sauberen Shutdown. Die anschließende separate
Stallprüfung `f1x-stall-01`, PID 67984, ist abgeschlossen: Der Benutzer
bestätigt wiederholtes Laufen/Angreifen, Töten und neue Wildhunde, erneute
gleiche Aktionen, beobachtete Hitches und Schließen per X.
Exitcode 0; 446.455 eigene Poseauswertungen, keine Granny-Reference-Posen,
keine CPU-Deformation/Fallbacks, keine Runtimefehler und alle Owner null.

Der Audit erfasst 62 Importe für 62 vollständige Inhaltsschlüssel, 1.942
Cachetreffer und keine Wiederholungsimporte oder Cacheverdrängungen.
Der größte Einzelimport dauert 773,113 ms. Alle aktiven Gameplay-
Bildintervalle über 40 ms enthalten Importe. Daneben bestehen zwölf
importfreie Intervalle zwischen 20 und 39,841 ms, deren Poseauswertung
maximal 1,439 ms benötigt. Sie werden nicht als Importhitches ausgegeben.
Maximale gesamte Posezeit pro aktivem Gameplay-Bildintervall: 6,644 ms
beim Weltbeitritt mit 1.039 Auswertungen über drei Updates; größte einzelne
aktive Gameplay-Pose: 0,2204 ms. Vollständige Zahlen und Messgrenzen stehen
in [phase-f1x-stall-audit.md](phase-f1x-stall-audit.md).

**Bekannte Performance-Limitierung:** First-Use-/Warmup-Hitches beim ersten
Auftreten von Inhalten/Clips bleiben bestehen und sind vom Benutzer für
F1-X akzeptiert. Nach Warmup ist die getestete Nutzung funktional stabil;
die warmen Wiederholungen zeigen keinen erneuten Import identischer Inhalte
und keinen belegten Pose-Hotpath als Stallursache. Das bedeutet ausdrücklich
nicht, dass jedes Bildintervall unter 20 ms liegt oder die Hitches behoben sind.

## 52. Resize/Minimize

Kurzer Diligent-Regressionstest bestanden. Eine manuelle Bestätigung von
Resize/Minimize/Restore fehlt. Wegen Exitcode 5 im ersten Lauf protokollieren
die bestehenden Renderer-Fehlerzweige jetzt im Diagnosemodus ihre erste
genaue Ursache mit Datei/Zeile; Present meldet zusätzlich die betroffenen
Teilsystemflags. Erfolgsbedingungen, Shader und Drawverhalten bleiben gleich.
Der gezielte Regressionstest provoziert eine ungültige Terrain-Stateeingabe
und prüft genau eine Ursache sowie die richtigen Flags ohne Diligent-Fehler.

## 53. Shutdown

**Automatisiert PASS der Content-Retention:** Granny04 und ZiiNAN05 Exitcode 0; die frischen
Shutdown-Audits enthalten jede erwartete Nullzählung genau einmal.
Manuell 01: tatsächliches Prozesshandle Exitcode 5 trotz Ressourcenabbau.
Manuell 02: über X beendet, tatsächliches Prozesshandle Exitcode 0 und
alle Ressourcen einschließlich RuntimeSkeletons/Clips und GPU-Owner 0.
Manuell 03: ebenfalls über X beendet, Exitcode 0 und alle Owner sowie
Retentionsbytes 0, trotz weiterhin fehlgeschlagener Performanceabnahme.
Der native Shutdown der neuen Content-Retention ist damit automatisiert
nachgewiesen; Manual04 und der aktuelle Stall-Audit wurden ebenfalls per X
mit Exitcode 0 und allen erfassten Ownern/Retentionsbytes null beendet.

## 54. Resource Lifetime

**Aktuelle native Audits und Release-/Debug-Tests PASS:** RuntimeSkeletons, RuntimeAnimationClips,
IndependentAnimationInstances, AssetDocuments, AnimationInstances,
MeshBindings, SourceTextures, SourceBuffers, SkinMeshes, BoneRemaps,
BonePalettes, PrototypeGeometry, PrototypePalettes und StaticSkinMeshes
sind beim Shutdown alle 0, ebenso RetainedImportKeyBytes. Das
Fehler-/Recovery-Szenario und ActorPath prüfen zusätzlich vollständige
Freigabe im Testprozess. Die neue Prozess-Retention hält neutrale Clips
bewusst über das Ende ihrer Modelldokumente hinaus; keine globalen
SourceDocument-, Skeleton- oder SDK-Owner. Tests prüfen Source-Owner vor
`ClearImportCache()` und alle neutralen Owner/RetainedImportKeyBytes danach.
Der private Clear-Hook wird nach `Main` vor dem Shutdown-Owner-Audit aufgerufen.
Die aktuellen nativen Smokes Granny04 und ZiiNAN05 bestätigen diesen
Shutdown-Pfad einschließlich der neuen Prozess-Retention.

## 55. Remaining Granny Dependencies

GR2-Decoder/Provider, einmaliger Adapterimport, Referenzsampler, bestehende
Controls/Root-Motion-Extraktion, Mesh-Binding und CPU-Referenzdeformer bleiben.
Die eigene AnimationRuntime-Bibliothek benötigt keine dieser Abhängigkeiten.

## 56. Production Default Decision

**Granny bleibt Default.** ZiiNAN bleibt explizites experimentelles Opt-in.
Die warme skalare Auswertung benötigt in der dokumentierten Stichprobe
etwa 4,56-mal den CPU-Aufwand der optimierten Reference. Erstimporte sind
nach der Optimierung kürzer, bleiben aber synchron und können sichtbare
Pausen verursachen. Die Live-Cache-Korrektur verhindert Mehrfachimporte noch
aktiver Daten; Manual03 zeigte weiterhin Pausen bei echten LRU-/Source-Misses.
Die neue Content-Retention überlebt Source-Abbau und vergrößert das Budget,
hat die sichtbaren Hitches laut Benutzer deutlich reduziert. Die vorhandene
Stallmessung bestätigt Erstimporte als Ursache der großen Pausen; der warme
Posepfad ist in dieser Stichprobe kein belegter Engpass. Kalte beziehungsweise später
verdrängte Kombinationen können weiterhin echte Misses auslösen.
Deshalb wird trotz enger Parität kein Production-Default-Wechsel empfohlen.
Die temporäre Granny-Importstufe soll gemäß aktualisierter Roadmap im nächsten
F2-X-Schritt durch den eigenen ZiiNAN-GR2-Reader ersetzt werden. Eine spätere
Default-Umschaltung benötigt ihre eigene Abnahme. F1-X endet hier ohne weitere
Cache-/Preload-/Persistent-Cache-Optimierung und ohne Beginn von F2-X.

## 57. Git Diff

Kein Commit/Push. Source-Checkout war vor Beginn sauber. Der Client-Checkout
hatte bereits eine Änderung in config/channel.inf und acht unversionierte
Rendererlogs; diese Baseline wird nicht bereinigt. Build-/Runtimeartefakte
bleiben in ignorierten Buildpfaden. `git diff --check` ist sauber,
alle 23 neuen Dateien sind ohne nachgestellte Leerzeichen; die vier
PowerShell-Hilfsskripte bestehen die Syntaxprüfung. 17 bestehende Dateien
geändert (539 hinzugefügte/126 entfernte Zeilen) und 23 neue Dateien.
Die neue Runtime, ihr privater Adapter, gezielte Tests und drei Berichte
bilden den zusätzlichen Umfang. Die Rendereränderungen ergänzen ausschließlich
die Diagnose des tatsächlich beobachteten Fehlers und die explizit aktivierte
Stallmessung; Shader und Renderingbedingungen sind unverändert. Der lesende
Game-Phase-Zugriff verändert keine Netzwerk- oder Gameplaylogik.

## 58. GO/NO-GO für F2-X

Die Foundation besitzt eine neutrale API, eigene TRS-/Pose-/Palettenauswertung
und einen echten Actorpfad ohne Granny-Frame-Pose-Sampling. Enge Parität und
unverändertes GPU-Skinning sind automatisiert belegt. Die neue Content-Retention
besteht Release (37/37, 93,74 s) und Debug (37/37, 137,60 s); der portable
GCC-Gate besteht abschließend 11/11. Die nativen Smokes Granny04 und ZiiNAN05
bestehen mit Exitcode 0 und allen erfassten Ownern/Retentionsbytes 0.
Die finale Stall-Instrumentierung besteht zusätzlich gezielte Release-/
Debug-Prüfungen und den GCC-Gate mit elf Tests. Der aktuelle manuelle
Stall-Audit bestätigt den eigenen Runtimepfad, Wiederverwendung ohne doppelte
Inhaltsimporte und sauberen Shutdown. Frühere fehlgeschlagene manuelle Läufe
bleiben historisch als solche gekennzeichnet.

**F1-X: GO und abgeschlossen anhand der Architektur- und Korrektheitskriterien.**
Die verbleibenden First-Use-/Warmup-Hitches sind eine ausdrücklich akzeptierte,
nicht behobene Performance-Limitierung. Die Nutzung nach Warmup war im kurzen
geprüften Ablauf funktional stabil; das ist kein Langzeit- oder FPS-Versprechen.
Granny bleibt Default, ZiiNAN bleibt Opt-in.

**F2-X: technisches GO als nächster Roadmap-Schritt** für den eigenen ZiiNAN
GR2 Reader an der neutralen Daten-/Runtimegrenze. Dieses GO ist kein Start
von F2-X. **STOP nach F1-X; keine F2-Implementierung und kein Commit/Push.**
