# F3-B – Redthief Production Animation Repair

Stand: 2026-09-14. **F3-B GO; erneuter Start von F3/4 Gate A möglich.**
Gate A selbst wurde in diesem Block nicht durchgeführt.

Der letzte produktiv referenzierte Reject ist durch eine gezielte Asset-Reparatur
geschlossen. `production referenced unsupported`: **1 → 0**. Source und das
tatsächlich geladene Pack enthalten dieselbe reparierte Datei. Release **9/9**,
Debug **9/9**, GCC/LP64 **16/16** sowie der automatische Redthief-Smoke und der
vom Benutzer bestätigte Login-Smoke sind bestanden.

Granny bleibt Production Default. Keine Reader-Validation wurde verändert;
kein Runtime-Sonderfall, kein SDK-Fallback, kein Commit/Push, kein Gate B/F5-X.
Die bereits vorhandenen, uncommitteten Änderungen aus F3-A bleiben erhalten.

## Ursprünglicher Defekt

Betroffen ist
`m2dev-client/assets/metin2_patch_dragon_rock_mobs/ymir work/monster2/redthief_general/back_damage.gr2`.
Der native Reader meldete `nonfinite float`. Es handelt sich um fehlerhafte
Quelldaten, nicht um eine fehlende GR2-Formatunterstützung.

Alle drei Transformkanäle von acht Bones waren vollständig beschädigt:

| Bones | Position | Quaternion | Scale/Shear |
|---|---:|---:|---:|
| `Bip01 L Finger2`, `Finger21`, `Finger22`, `Finger2Nub` | je 9/9 NaN | je 12/12 NaN | je 18/18 NaN |
| `Bip01 R Finger2`, `Finger21`, `Finger22`, `Finger2Nub` | je 9/9 NaN | je 12/12 NaN | je 18/18 NaN |

Damit waren **24 Kanäle / 312 Float-Kontrollwerte** betroffen. Die Werte hatten
das Bitmuster `0x7fc00000`. Es gab innerhalb dieser Kanäle keine gültigen
Keyframe-Werte, aus denen sich der ursprüngliche Verlauf interpolieren ließe.

Der bereits in F3-A erhobene Granny-Nachweis gehört exakt zum unten angegebenen
Original-SHA256: Granny lädt den Clip, liefert aber bei 0 %, 25 %, 50 % und
110 % der Dauer jeweils acht nichtfinite Composite-Matrizen. Betroffen sind
die Bone-Indizes 42–45 und 68–71 des 91-Bone-Modells. Die Originalprüfung bei
110 % war eine Loop-Probe, keine Prüfung der 100-%-Klemmgrenze.
Belege: `build/f3a/root-cause-probe.log` und die kopierten
`granny_reference_evidence` in `phase-f3b-compatibility-results.json`.

## Tatsächliche Referenzen und Motion-Semantik

| Race | `assets/root/npclist.txt` | Modell |
|---|---:|---|
| 3505 | Zeile 593 | `redthief_general/redthief_general.gr2`, 91 Bones |
| 3555 | Zeile 598 | `redthief2_general/redthief2_general.gr2`, 71 Bones |
| 3909 | Zeile 632 | ebenfalls `redthief2_general`, 71 Bones |

Die Verzeichnisse liegen unter `metin2_patch_dragon_rock_mobs/ymir work/monster2/`.
Beide MSM-Dateien wählen ihr jeweiliges Modell. Beide `motlist.txt` registrieren:

- Zeile 7: `GENERAL FRONT_DAMAGE back_damage.msa 100` → Motion-ID **5**.
- Zeile 11: `GENERAL BACK_DAMAGE back_damage.msa 100` → Motion-ID **8**.
- Beide `back_damage.msa`, Zeile 3, referenzieren dieselbe virtuelle Datei:
  `d:/ymir work/monster2/redthief_general/back_damage.gr2`.
- `MotionDuration` in der MSA: `0.666666`; GR2-Dauer: `0.666666687` Sekunden.

`RaceManager.cpp:212,223` bildet FRONT/BACK_DAMAGE auf diese IDs ab.
`ActorInstanceBattle.cpp:782` (`__HitGood`) wählt je nach relativer Ausrichtung
den Treffer vorne/hinten, spielt ihn einmal und stellt anschließend WAIT ein.
Die hintere Reaktion hat den bereits vorhandenen Front-Damage-Ersatzpfad.
Knockdown verwendet dagegen andere Motion-Typen und IDs 6/9 mit Aufstehlogik.

Zusätzlich übernimmt `RaceManager.cpp:305–324` FRONT_DAMAGE als **NAME_SPAWN,
ID 25**, wenn keine eigene Spawn-Motion registriert ist. Das gilt hier für
alle drei Races. Auch diese Registrierung bleibt durch die Reparatur erhalten.

`redthief2_general` besitzt keine eigene GR2-Trefferanimation; seine MSAs verweisen
bereits auf `redthief_general`. Die 71- und 91-Bone-Skelette sind **nicht identisch**.
Dieser vorhandene Shared-Animation-Vertrag wurde nicht verändert und für beide
Modelle samt LOD separat geprüft.

## Ersatzsuche und Skeleton-Prüfung

Der native Vergleich erfasste **9.166 GR2-Dateien / 4.415 gelesene Modelle**.
Genau zwei Modelle besitzen dieselben 91 Bone-Namen in derselben Reihenfolge,
dieselben Parent-Indizes und bitidentische Bind-/Inverse-Bind-Transforms:
`redthief_general.gr2` und dessen `redthief_general_lod_01.gr2`.
Beide besitzen genau einen Root, Index 0 / `Bip01`.
Vollständige Inventur: `build/f3b/skeleton-inventory.tsv`.

36 konkrete Clip/Modell-Paare wurden auf Modellstruktur, Gruppen, Ziele, Dauer,
Root-Bewegung sowie native geklemmte Pose/Palette geprüft. Die vollständigen
Zeilen stehen in `phase-f3b-evidence.json` und
`build/f3b/candidates-final-results.tsv`.

| Kandidatengruppe | Bones | Ergebnis |
|---|---:|---|
| redthief_boss: Damage, Damage1, Knockdown vorne/hinten | 89 | Namen/Reihenfolge/Parents abweichend; 79 Zielbones passen |
| redthief_boss2: dieselben sechs Cliparten | 95 | Struktur abweichend; 72 Zielbones passen |
| redthief_bow: Damage/Knockdown vorne/hinten | 87 | Struktur abweichend; 78 Zielbones passen |
| redthief_magic: Damage/Knockdown vorne/hinten | 110 | Struktur abweichend; 77 Zielbones passen |
| redthief_officer: Damage/Knockdown vorne/hinten | 68 | Struktur abweichend; 67 Zielbones passen |
| redthief_soldier2: Damage/Knockdown vorne/hinten | 60 | Struktur abweichend; 54 Zielbones passen |
| redthief_general: back_damage/front_damage | 91 | Beide Originalclips wegen NaN abgelehnt |
| redthief_general: back_knockdown/front_knockdown | 91 | Struktur passt, 1,166667 s; andere Motion-Semantik und andere Handhaltung |
| redthief_general: normal_attack1/wait1/walk/front_standup | 91 | Gültige Kanaldonoren untersucht; keine vollständige Hit-Ersatzanimation |

Die gültigen Kandidaten besitzen im verwendeten `Bip01`-TrackGroup
`LoopTranslation=(0,0,0)` und keinen PeriodicLoop. Die tatsächliche Loop-/Once-
Semantik entsteht durch Motion-Konfiguration und Control; die Kandidatenprobe
verwendet ausdrücklich Clamp an beiden Grenzen. Ein erfolgreicher technischer
Bind an Teile des Zielskeletts wurde nicht als Freigabe eines fremden Clips gewertet.

Eine Config-Umschaltung auf einen vollständigen anderen Clip ist damit nicht
begründet. Knockdown oder Aufstehen würden das Verhalten einer normalen
Trefferreaktion verändern. Die Config bleibt unverändert.

## Gewählte Reparatur und ihre Grundlage

**Übernommen wurden ausschließlich bereits vorhandene konstante Kanäle aus
`redthief_general/normal_attack1.gr2`.** Die originale Körperbewegung, alle
gültigen Fingerkanäle, sämtliche Knots, Degrees, Zielnamen, Track-Reihenfolgen,
Gruppen, Dauer und Root-Motion-Metadaten bleiben erhalten.

Die Auswahl stützt sich auf folgende überprüfte Quelldaten:

1. `NORMAL_ATTACK1` ist für denselben Character registriert (40 %); seine MSA
   nennt exakt diesen Donor. Es wird weder auf ein fremdes Skeleton noch auf
   eine zufällige, lediglich ladbare Animation zurückgegriffen.
2. Alle **48 erhaltenen Kanäle der 16 übrigen Finger-Bones** in back_damage sind
   konstant. Dieselben 48 Donorkanäle sind ebenfalls konstant. Maximale
   Komponentendifferenzen: Position **1,526e-5**, Quaternion **6,0e-8**,
   Scale/Shear **6,0e-8**. Diese vorhandene Griffhaltung stimmt damit innerhalb
   der bestehenden F1-X-Toleranzen überein.
3. Die 24 benötigten Donorkanäle sind über sämtliche Kontrollwerte exakt
   konstant. Es ist kein zeitliches Resampling der 1,5-s-Angriffsanimation nötig.
4. Die gültigen Clips `wait1` und `walk` bestätigen unabhängig diese Fingerhaltung:
   maximale Abweichungen der ausgewählten Kanäle zum Donor bleiben bei
   Position unter 1,526e-5, Quaternion bei 1,19e-7 und Scale/Shear bei 1,2e-7.
5. Knockdown/Standup/Run wurden als Donoren verworfen: Ihre erhaltenen
   Fingerrotationswerte weichen deutlich ab, etwa um **0,49435** bei Knockdown
   gegenüber back_damage. Ein bloß konstantes Donorsignal wäre kein ausreichender
   semantischer Nachweis.

Dies ist eine gezielte Reparatur mit einer belegten, bereits vorhandenen
Griffhaltung aus demselben Character. Die verlorenen ursprünglichen
Finger-Keyframes werden nicht als exakt wiedergewonnen ausgegeben. Es werden
keine neuen Transformwerte berechnet, keine NaNs interpoliert und keine Bones
auf eine erfundene Null-/Bind-Pose gesetzt. Bei den GR2-Identitätskanälen wird
die durch das vorhandene Donorformat definierte Identität materialisiert.

Der generische Offline-Helfer `GR2ConstantChannelRepair` arbeitet mit der expliziten
24-Zeilen-Auswahl `phase-f3b-repair-plan.tsv`. Er verweigert nichtkonstante
Donorkanäle, das Überschreiben gültiger Quellwerte, Mehrdeutigkeiten und bestehende
Ausgabedateien. Vor Ausgabe muss der unveränderte native Reader die gesamte
reparierte Animation akzeptieren.

## Format, Invarianten und Pack-Zuordnung

Die acht GR2-Sections werden regulär unkomprimiert gespeichert. Version, Tag,
Root-Referenzen, logische Section-Größen, Relocations und Marshalling-Tabellen
bleiben erhalten; physische Offsets, Dateigröße und CRC werden neu geschrieben.
Eine erneute `File`-/`Read`-Prüfung kontrolliert Header, CRC, Sections, Typen und
Animation. Der Helfer vergleicht jede logische Section: **nur die 312 ausgewählten
Float-Werte dürfen abweichen**. Der vollständige Patch-Nachweis steht in
`build/f3b/repair.log`.

| Artefakt | Vorher | Nachher |
|---|---|---|
| back_damage.gr2 Größe | 22.364 Bytes | 39.732 Bytes |
| back_damage.gr2 SHA256 | `083b0e312850d9df28d4f2cd4cfbcf82c45b9a5278c5ac494128803286aa157b` | `2e0f34887edff2f4cfddbb97ae7d9f1a15236752f1330ddaf4f998f4479c51b4` |
| Pack SHA256 | `cd2cd8dbb762011512816f32f9e17850a2fb61439d89725cb119a8c18017bfeb` | `112383a062903dde39d555164aea7fd8abf1b06959212eda8680c4b04f46083b` |

Die tatsächliche Pack-Datei ist
`m2dev-client/pack/metin2_patch_dragon_rock_mobs.pck`.
Vor dem Neubau wurden alle **2.405** decodierten Einträge mit der Source verglichen:
identisch, keine zusätzlichen Quelldateien. Der vorhandene PackMaker erzeugte
zuerst eine separate Staging-Datei. Der Vergleich Originalpack/Neupack bestätigte
exakt einen veränderten Eintrag, den oben genannten virtuellen back_damage-Pfad.
Alle anderen **2.404** Einträge sind decodiert byteidentisch.

Anschließend wurden Source und produktives Pack ersetzt; der erneute Vergleich
aller 2.405 Einträge bestätigt wieder vollständige Source/Pack-Identität.
Originaldatei und Originalpack liegen unter `build/f3b/before/`.
Der erste Source-Austausch scheiterte am PowerShell-Null-Argument für `File.Replace`;
der Austausch mit explizitem Backup-Pfad wurde anschließend erfolgreich ausgeführt
und vollständig nachgeprüft. Der abschließende Zustand ist konsistent.

Zusätzlich wurde die echte Pack-Reihenfolge aus `build/f34/pack-order.txt` mit dem
produktiven `CPackManager` geprüft: alle 52 bisherigen Reject-Pfade aufgelöst,
51 native Pack-Loads akzeptiert, nur das weiterhin unreferenzierte front_damage
abgelehnt; alle 80 geprüften Config-Einträge Source/Pack-identisch. Native
Provider-Dispatch: kein stiller Granny-Fallback, Dokument-/Reader-Ressourcen 0.
Der Audit-Helfer endet deshalb erwartungsgemäß mit Code **2** für den verbliebenen
unaufgelösten Nutzungsfall; die getrennte produktive Klassifikation ist **0**.

## Finite-, Binding- und Paritätsprüfungen

Für alle vier Produktionsmodelle (beide Varianten jeweils Basis/LOD):

| Modell | Bones | Gebundene Track-Ziele | Samples |
|---|---:|---:|---|
| redthief_general | 91 | 89 | 0/25/50/75/100 %: Pose, World, Palette finite |
| redthief_general_lod_01 | 91 | 89 | alle fünf bestanden |
| redthief2_general | 71 | 69 | alle fünf bestanden |
| redthief2_general_lod_01 | 71 | 69 | alle fünf bestanden |

Die zwei Bones ohne eigenen Track je Modell verwenden unverändert ihre vorhandene
Bind-Pose. Keine neuen Track-Ziele, keine Skeleton-/Bone-Order-Änderung.

Der SDK-freie `AssetRuntime.GR2RedthiefRepair` prüft außerdem die 24 übernommenen
Kanäle direkt gegen den Donor, ursprüngliche Kontrollwertzahlen, Clip-Metadaten,
Bind-Anzahl und freigegebene Runtime-Objekte. Er läuft auf Release, Debug und LP64.

Der bestehende native/Granny-Paritätstest wurde für alle vier Modelle mit
`animation-clamp` ausgeführt: zehn Zeitpunkte einschließlich der fünf geforderten
Prozentpunkte, kurz vor/nach Ende und jenseits der Clip-Dauer; außerdem
Root-Motion-, CPU-Referenzvertex- und D3D11-GPU-Vertexvergleich. Die Referenz wird
jetzt explizit auf finite World-/Composite-Matrizen geprüft, bevor Fehlermaxima
gebildet werden. Keine Toleranz wurde erhöht.

Beispiel Basis-Modell: Translation 1,04904e-5; Rotation 1,31502e-6 rad;
Scale 1,19209e-7; World 1,84059e-4; Palette 8,54492e-4; Root-Motion 0;
Vertexposition 1,98364e-4; Vertexnormalen 4,55976e-6. Alle vier Läufe bestanden
die ursprünglichen F1-X-Grenzen. Vollständige Werte:
`build/f3b/staged-parity-0.log` bis `staged-parity-3.log`.

Drei gezielte Negativprüfungen bestanden: nichtkonstanter Donor,
bereits finite Source und unvollständige Kanalauswahl werden abgelehnt,
ohne eine Ausgabedatei zu erzeugen. Kein Fuzzer und keine lange Testsuite.

## Corpus und kurze Regression

| Kennzahl | F3-A | F3-B |
|---|---:|---:|
| Total | 9.166 | 9.166 |
| Parsed | 9.159 | **9.160** |
| Rejected | 7 | **6** |
| Production referenced unsupported | 1 | **0** |
| Usage unresolved | 20 | 20 |
| Neue Rejects | 0 | 0 |

Scan-Dauer: **28,7338 Sekunden**. Die sechs verbleibenden Raw-Rejects sind
`redthief_general/front_damage.gr2` (NaN, usage unresolved; kein Eintrag in den
geprüften Motlists) sowie fünf defekte haven_dungeon-Raw-Kopien, deren produktive
Pack-Versionen gültig sind. Ungeklärte Nutzung wurde nicht umklassifiziert, um
den Erfolgswert zu erreichen. Von den 52 F3-A-Dateihashes änderte sich nur der
ausdrücklich reparierte back_damage-Pfad.

| Prüfung | Ergebnis | Dauer |
|---|---|---:|
| Release Build | PASS | inkrementell |
| Release Fast Gate | **9/9 PASS** | 24,09 s |
| Debug Build | PASS | inkrementell |
| Debug Fast Gate | **9/9 PASS** | 72,19 s |
| GCC/LP64 Build | PASS | Cygwin GCC 12.4, Common-Ziel |
| GCC/LP64 Gates | **16/16 PASS** | 2,02 s |

Windows-Auswahl: StartupOptions, AnimationRuntime.Contracts, GR2Warmup,
GR2Safety, GR2Independence, GR2Parity, GR2Compatibility,
GR2CompatibilityParity und GR2RedthiefRepair. GCC enthält dieselben portablen
Vertragstests einschließlich des neuen Real-Asset-Tests, ohne Windows/SDK/GPU.
Dies ist keine Android-/Device-Freigabe.

Die finalen inkrementellen Build-Logs enthalten keine LNK4099/LNK4075/LNK4098;
das ist kein Nachweis eines warnungsfreien Clean Builds. Die bekannten Vendor-
CMake-Deprecation-Warnungen und früheren PDB-/Linker-Baselines bleiben separat.

## Client-Smokes

Beide Läufe verwenden den geprüften Release-Build und das reparierte Pack mit
`--renderer-diagnostics --animation-runtime=ziinan --gr2-reader=ziinan` sowie dem
unveränderten GPU-Default.

**Automatischer Redthief-Lauf – PASS:**
`build/f3b/runtime-verified/`, PID 9508, 21,68 s, Exitcode 0.
Die originale Map a1, drei Races 3505/3555/3909 und deren produktive Configs
werden geladen. Die Fixture fordert die Once-Motions 5, 8 und 25 an und stellt
danach jeweils WAIT ein. Alle fünf Phasen rendern (241/242/243/240/244 Frames),
alle drei Races haben nachgewiesene GPU-Submissions. Screenshot vorhanden und
auf vollständig dargestellte Körper/Gliedmaßen geprüft. Das Standbild ersetzt
keinen zeitlichen Vergleich mit der beschädigten Originalanimation.
Kein Asset-/Motion-Ladefehler; syserr leer.

**Login → Character Select → Ingame – PASS:**
isolierter Client unter `build/f2x/runtime/f3b-login/`, PID 68456.
Der Benutzer bestätigte den angeforderten Ablauf und die sichtbaren Animationen
mit **„alles super“**, anschließend normales Beenden, Exitcode 0. Dieser Lauf
ist vom automatischen Fixture-Lauf getrennt dokumentiert. Ein Redthief-Kampf
im normalen Server-Spiel wurde vom Benutzer nicht gesondert bestätigt.

| Zähler | Redthief-Fixture | Manueller Login-Lauf |
|---|---:|---:|
| NativeGR2Reads | 140 | 907 |
| IndependentPoseSamples | 4.932 | 167.276 |
| GPUFrames | 3.630 | 166.557 |
| GrannyFileReads | **0** | **0** |
| ReferencePoseSamples / ImportPoseSamples | **0 / 0** | **0 / 0** |
| AllCPUDeformationCalls / Vertices | **0 / 0** | **0 / 0** |
| GPUFallbacks | **0** | **0** |
| AnimationRuntimeFailures | **0** | **0** |
| Exitcode | **0** | **0** |
| alle geprüften Shutdown-Ressourcen | **0** | **0** |

Geprüfte Shutdown-Ressourcen umfassen SourceTextures/Buffers, AssetDocuments,
GR2ReaderResources, RuntimeSkeletons/Clips, AnimationInstances,
IndependentAnimationInstances, MeshBindings, SkinMeshes, BoneRemaps/Palettes,
PrototypeGeometry/Palettes, StaticSkinMeshes und RetainedImportKeyBytes.
Im manuellen Fehlerlog steht ausschließlich die bekannte MarkManager-Baseline
`invalid idx 0`, kein Asset-Runtime-Fehler.

Der erste automatische Start innerhalb der Sandbox scheiterte vor dem Fixture-
Lauf mit `0xc0000142` (Prozessinitialisierung). Er zählt nicht als Smoke-PASS.
Der danach autorisiert außerhalb der Sandbox ausgeführte, frische Lauf ist der
oben ausgewiesene PASS. Es wurden keine Benutzerprozesse beendet.

## Git-Diff und Nachweisdateien

Source-HEAD als Ausgangspunkt: `500986389ef91742e0c7a6b96e67f4276aa4c791`.
F3-B ändert **keinen Produktions-Reader- oder AnimationRuntime-Code**; die
betreffenden Dateien wurden vor/nach F3-B per SHA256 auf Identität geprüft.
Der gegen HEAD sichtbare Produktionscode-Diff stammt weiterhin aus F3-A.

Neu für F3-B unter `tests/AssetRuntime/`:
`GR2MotionCandidateAudit.cpp`, `GR2CurveDonorAudit.cpp`,
`GR2ConstantChannelRepair.cpp`, `PackContentAudit.cpp`,
`GR2RedthiefRepairTest.cpp`, `redthief_runtime_entry.py`,
`run_redthief_smoke.ps1`. Dazu begrenzte Ergänzungen in `CMakeLists.txt`
(Diagnosehelfer EXCLUDE_FROM_ALL und kurzer Regressionstest) sowie
`GR2ParityTest.cpp` (Clamp-CLI und explizite Finite-Referenzprüfung).

Im Client-Repository ist ausschließlich die GR2-Quelldatei durch F3-B verändert.
`config/channel.inf` war bereits vorher geändert; die vorhandenen Renderer-Logs
wurden nicht entfernt. Das produktive `.pck` ist Git-ignoriert und muss bei einer
späteren Übernahme des Asset-Commits separat mit PackMaker neu erzeugt werden.
Die tatsächliche Pack-Aktualisierung auf diesem Rechner ist geprüft.

Dauerhafte Begleitdateien:

- `phase-f3b-compatibility-results.json`: alle 52 Klassifikationszeilen, vorher/
  nachher, Source-SHA256 und produktive Pack-Ergebnisse.
- `phase-f3b-evidence.json`: Kandidaten, Donorvergleich, relevante Hashes,
  Paritätswerte und Smoke-Zähler.
- `phase-f3b-repair-plan.tsv`: die explizit übernommenen 24 Kanäle.

Vollständige lokale Laufbelege liegen unter `build/f3b/`; die getrennten
Login-Belege unter `build/f2x/runtime/f3b-login/`. Reproduktion der reparierten
GR2: `GR2ConstantChannelRepair <Original-back_damage> <normal_attack1.gr2>
<phase-f3b-repair-plan.tsv> <neue-Ausgabedatei>`, danach den kurzen Real-Asset-
Test und Pack-Inhaltsvergleich ausführen. Der Helfer benötigt zum GR2-Laden
keine Granny-Runtime.

**F3-B abgeschlossen. GO für den separat anzustoßenden F3/4-Gate-A-Lauf.
STOP: kein Production-Default-Wechsel, keine Granny-Entfernung und keine
weitere Phase in diesem Block.**
