# Milestone F3-A – GR2 Production Compatibility Closure

Stand: 14.09.2026. Ausgangs-Commit: `500986389ef91742e0c7a6b96e67f4276aa4c791`. Source-Worktree zu Beginn sauber; Änderungen bleiben uncommitted.

## Entscheidung

**NO-GO für erneutes F3/4 Gate A.** 26 der 27 bestätigten Produktions-Rejects sind geschlossen. `redthief_general/back_damage.gr2` bleibt zu Recht abgelehnt: gespeicherte NaNs erzeugen auch in Granny ungültige Bone-Matrizen. Eine erfolgreich geladene Datei ist hier kein gültiges Referenz-Pose-Ergebnis.

**production referenced unsupported = 1**

Dieser Produktionszähler zählt jede nicht native lesbare, nachgewiesen benötigte Datei einschließlich Validation-Rejects. Er ist deshalb nicht mit der Corpus-Statusklasse `unsupported` gleichzusetzen, die jetzt 0 beträgt. Kein Produktionsfall wurde als „ungenutzt“ umklassifiziert, um das Gate zu erreichen.

Granny bleibt Default. Kein Granny-Removal, kein Gate B, kein F5-X, keine Änderung am GPU-Skinning oder Diligent-Renderer. Kein Commit/Push. Nach diesem F3-A-Abschluss STOP.

## Ausgangslage und vollständige Einzelbelege

Ursprünglich 52 Rejects: 27 produktiv referenziert, 20 usage unresolved, 5 defekte Rohkopien mit gültiger Pack-Überlagerung. Vorher: 9.114 parsed, 18 unsupported curve/type, 34 malformed.

Die [CSV](phase-f3a-compatibility-results.csv) enthält alle 52 Pfade, Assettypen, ursprüngliche Fehler, Reader-Stelle, Feld, Section-/Type-Adressen, Referenzbefunde und Vorher/Nachher-Status. Die [JSON](phase-f3a-compatibility-results.json) enthält zusätzlich die bisherigen Config-/Motion-Belegketten und Hashes. Alle 52 Raw-SHA-256 wurden gegen die F3/4-Klassifikation erneut geprüft und stimmen überein. Die Assets wurden nicht repariert oder ersetzt.

`tests/AssetRuntime/summarize_gr2_compatibility.py` verbindet den vollständigen neuen Corpus mit dem ursprünglichen Inventar und dem frischen Pack-/Referenzharness. Neue Rejects außerhalb der 52 werden ausdrücklich erkannt. Ergebnis: **0 neue Rejects**.

## Root Causes

| Primäre Signatur | Originale Rejects | Produktiv | Usage unresolved | Jetzt raw parsed |
|---|---:|---:|---:|---:|
| Doppelte Bone-Namen | 6 | 4 | 2 | 6 |
| Doppelte Track-Namen | 20 | 7 | 13 | 20 |
| Leeres Mesh | 1 | 1 | 0 | 1 |
| NaN-Kurven | 2 | 1 | 1 | 0 |
| Überlagerte defekte Rohkopie | 5 | 0 | 0 | 0 |
| PeriodicLoop | 14 | 13 | 1 | 14 |
| Mehrere Skeleton-Wurzeln | 4 | 1 | 3 | 4 |

Die primäre Signatur ist der erste ursprüngliche Reader-Reject. Eine Datei kann weitere Merkmale derselben Gruppen haben; Halloween `walk1` enthält beispielsweise PeriodicLoop, doppelte Tracks und eine Textmarke außerhalb der gekürzten Animation.

### PeriodicLoop

`TrackGroup.PeriodicLoop` ist ein gültiger Datensatz aus `Radius`, `dAngle`, `dZ`, `BasisX[3]`, `BasisY[3]`, `Axis[3]`. Die alte pauschale Ablehnung wurde durch typisierte, finite Werteprüfungen und native Bewegungsübernahme ersetzt. Beispiel Arzt: Loop-Objekt `0:66216`, Type `6:5056`; zugehörige Track-Gruppe `0:180`, Type `6:4064`. Weitere Type-/Feldadressen stehen in den Einzelbelegen.

Die Parameter beschreiben Bewegung pro Sekunde. Die Translation entsteht aus der integrierten Helix; `dAngle * elapsed * Axis` liefert den Rotationsvektor. Die stabile Halbwinkel-Darstellung vermeidet Auslöschung bei sehr großem Radius und sehr kleinem Winkel. Die Umsetzung liegt im GR2-Provider; der bisherige lineare LoopTranslation-Pfad bleibt erhalten. `RootMotion` als eigener expliziter Datensatz bleibt unsupported.

Track-Gruppen werden weiterhin nach Modellname gewählt. Bei Crustacean, Ent-Boss 2 und Manticore liegen die PeriodicLoop-Daten teilweise ausschließlich in weiteren, nicht an das Modell gebundenen Gruppen. Diese Daten werden gültig eingelesen und nicht als Bewegung des eigentlichen Modells verwendet.

### Doppelte Namen und mehrere Wurzeln

GR2-Skeletons sind über Bone-Indizes und Parent-Indizes identifiziert; Displaynamen müssen nicht eindeutig sein. Alle ursprünglichen Namen, Bone-Indizes, Parents, Bind- und Inverse-Bind-Matrizen bleiben erhalten. Beispiele: Halloween `bone01` an 12/71, Historian `Bone_li03` an 27/30, Ch-Officer `rar_01`/`rar_02`/`rar_03`. Hair `hair_11_1` hat zwei Wurzeln an 0 und 1.

Die kleine notwendige neutrale Anpassung ist `RuntimeSkeleton::SourceSemantics::IndexedForest`, ausschließlich vom GR2-Reader angefordert. Der Standard anderer Aufrufer bleibt `NamedTree`. Der vorhandene Evaluator kann mehrere Roots bereits auswerten; seine Mathematik, die Pose-Auswertung und die Bone-Palette wurden nicht geändert. Kein synthetischer Root und keine Verschiebung der Bone-Indizes.

Track-Bindung erfolgt pro Ziel-Bone. Bei gesetztem Sorted-Flag verwendet Granny einen Midpoint-Binärsuchtreffer, sonst den ersten Treffer. So bindet z. B. der Historian beide gleich benannten Bones an Track 94; Halloween an Track 63. Die Reihenfolge gleichnamiger Tracks bleibt erhalten und alle Kurven werden weiter validiert.

Granny vergleicht kodierte Namen als **signed 8-bit bytes**, einschließlich Terminator. Dies wird plattformunabhängig explizit abgebildet. Eine anfängliche C++-Sortierungsprüfung erzeugte im ersten neuen Vollscan 102 zusätzliche Rejects bei Skelett-/Zombie-Soldaten sowie Bär/Panda. Die Referenzdiagnose bewies die abweichende Byte-Sortierung. Nach der Korrektur und 131.072 SDK-Vergleichen ist der erneute Vollscan frei von diesen Regressionen. Der erste Scan `corpus-final` ist ein überholter Diagnose-Zwischenstand; maßgeblich ist ausschließlich **`corpus-verified`**.

### Leere Mesh-Datensätze

Das LOD `redthief2_soldier2_lod_01` enthält die leeren Modelle/Meshes `Object01` und `]` neben dem normalen Modell `Bip01`. Ein Vertexcount von 0 ist bei passender leerer Topologie gültig. Der deklarierte Vertex-Typ wird weiterhin geprüft, ebenso alle vorhandenen Material-/Bone-/Gruppendaten. Indizes ohne Vertices bleiben ungültig. Die exponierten Layout-/Stride-/Indexwidth-Metadaten entsprechen bei leerer Geometrie Granny: Unknown/0/Unknown. Modell- und Mesh-Reihenfolge werden beibehalten.

### Hinter dem ersten Reject verborgene Folgefehler

- **Float-Zeitpunkte im Spline-Import:** Beim Arzt führte die Mischung aus ungerundeter Interpolationsgewichtung und auf Float gerundetem Prüfzeitpunkt zur künstlichen Refinement-Grenze. Gewicht und Unterteilung verwenden jetzt denselben tatsächlich darstellbaren Zeitpunkt. Grenzwerte, Key-Budget, Grad-Unterstützung und maximale Unterteilungstiefe bleiben unverändert. Die Runtime-Sampler wurden nicht geändert.
- **Textmarken gekürzter Clips:** Halloween `walk1` trägt `end` bei 4,66666651 s trotz 1 s Clipdauer (`0:31112`, Type `6:4960`). Diese Annotation ist Metadatum und wird mit unverändertem Zeitwert/Order erhalten, wie bei Granny. Negative und nichtfinite Zeiten bleiben ungültig. Die Dauerprüfung für echte Kurven-Keys bleibt unverändert. Die Referenz-Metadatenparität prüft jetzt auch Text-Events.

## Verbleibender produktiver Blocker: NaN-Quelldaten

`metin2_patch_dragon_rock_mobs/ymir work/monster2/redthief_general/back_damage.gr2`

Die aktive Zuordnung ist weiterhin belegt: `redthief_general/motlist.txt` und `redthief2_general/motlist.txt`, jeweils Zeilen 7 und 11, verwenden `back_damage.msa` für FRONT_DAMAGE und BACK_DAMAGE. Die bestehende vollständige MSM/MSA-/Race-/Server-Belegkette steht in den Einzelbelegen. Die gepackten GR2-Bytes sind identisch zur defekten Rohquelle.

Schon `TransformTracks[14].PositionCurve.Controls[0].Real32[0]` enthält `0x7fc00000` bei Section 0, Offset 8556, Elementtype Section 6, Offset 4640. Weitere Position-/Orientation-/ScaleShear-Controls enthalten ebenfalls NaNs. `File::Float` lehnt diese weiterhin mit `nonfinite float` ab.

Der explizite Granny-Harness lädt Modell und Clip und sampelt bei `0`, `0.25 * duration`, `0.5 * duration`, `1.1 * duration`. **An jedem Zeitpunkt sind acht Composite-Bone-Matrizen nichtfinite:** rechts Bones 42–45 und links 68–71 (`Finger2`, `Finger21`, `Finger22`, `Finger2Nub`). Der zweite Clip `front_damage.gr2` zeigt dasselbe Problem, seine aktive Nutzung bleibt aber ungeklärt.

Es wurden keine NaNs auf Null gesetzt, keine defekten Tracks ausgelassen und keine Bind-Pose-Ersatzwerte eingeführt. Zum Schließen des verbleibenden Falls wird ein inhaltlich korrektes Produktionsasset oder ein belastbarer Nachweis einer tatsächlich nicht mehr benötigten Referenz benötigt; danach neue Reader-/Pose-Parität und erneutes Gate. Ein Reader-Workaround würde die vorgegebene Finite-Validierung verletzen.

## 27 bestätigte Produktionsfälle vorher/nachher

**27 Rejects → 26 parsed / 1 Validation-Reject.** Alle 26 ebenfalls durch den tatsächlichen Pack-Ladepfad lesbar. Die Parität ist repräsentativ pro Fehlerklasse, nicht eine visuelle Einzelabnahme jedes Assets.

| Pfad unter `m2dev-client/assets` | Ursache | Vorher | Nachher |
|---|---|---|---|
| `NPC/ymir work/npc/doctor/die.gr2` | PeriodicLoop | Reject | Parsed |
| `metin2_patch_dragon_rock_mobs/ymir work/monster2/crustacean_officer/run.gr2` | PeriodicLoop | Reject | Parsed |
| `metin2_patch_dragon_rock_mobs/ymir work/monster2/crustacean_officer/walk.gr2` | PeriodicLoop | Reject | Parsed |
| `metin2_patch_dragon_rock_mobs/ymir work/monster2/crustacean_soldier/run.gr2` | PeriodicLoop | Reject | Parsed |
| `metin2_patch_dragon_rock_mobs/ymir work/monster2/crustacean_soldier/walk.gr2` | PeriodicLoop | Reject | Parsed |
| `metin2_patch_dragon_rock_mobs/ymir work/monster2/ent_boss1/normal_attack.gr2` | PeriodicLoop | Reject | Parsed |
| `metin2_patch_dragon_rock_mobs/ymir work/monster2/ent_boss2/run.gr2` | PeriodicLoop | Reject | Parsed |
| `metin2_patch_dragon_rock_mobs/ymir work/monster2/ent_boss2/walk.gr2` | PeriodicLoop | Reject | Parsed |
| `metin2_patch_dragon_rock_mobs/ymir work/monster2/manticore_boss2/special_attack.gr2` | PeriodicLoop | Reject | Parsed |
| `metin2_patch_dragon_rock_mobs/ymir work/monster2/ogre_boss2/run.gr2` | PeriodicLoop | Reject | Parsed |
| `metin2_patch_dragon_rock_mobs/ymir work/monster2/ogre_soldier/normal_attack1.gr2` | PeriodicLoop | Reject | Parsed |
| `metin2_patch_dragon_rock_mobs/ymir work/monster2/redthief2_soldier2/redthief2_soldier2_lod_01.gr2` | Leeres Mesh | Reject | Parsed |
| `metin2_patch_dragon_rock_mobs/ymir work/monster2/redthief_general/back_damage.gr2` | NaN-Kurven | Reject | Reject: NaN |
| `metin2_patch_easter1/ymir work/pc/warrior/hair/hair_11_1.gr2` | Mehrere Skeleton-Wurzeln | Reject | Parsed |
| `metin2_patch_eu4/ymir work/npc2/halloween1/halloween1.gr2` | Doppelte Bone-Namen | Reject | Parsed |
| `metin2_patch_eu4/ymir work/npc2/halloween1/wait.gr2` | Doppelte Track-Namen | Reject | Parsed |
| `metin2_patch_eu4/ymir work/npc2/halloween1/wait1.gr2` | Doppelte Track-Namen | Reject | Parsed |
| `metin2_patch_eu4/ymir work/npc2/halloween1/walk1.gr2` | PeriodicLoop | Reject | Parsed |
| `metin2_patch_xmas/ymir work/npc2/pig_young1/walk.gr2` | PeriodicLoop | Reject | Parsed |
| `patch2/ymir work/npc2/historian/historian.gr2` | Doppelte Bone-Namen | Reject | Parsed |
| `patch2/ymir work/npc2/historian/run.gr2` | Doppelte Track-Namen | Reject | Parsed |
| `patch2/ymir work/npc2/historian/wait.gr2` | Doppelte Track-Namen | Reject | Parsed |
| `patch2/ymir work/npc2/historian/wait1.gr2` | Doppelte Track-Namen | Reject | Parsed |
| `patch2/ymir work/npc2/historian/walk.gr2` | Doppelte Track-Namen | Reject | Parsed |
| `season3_eu/ymir work/monster2/ch_officer/37.gr2` | Doppelte Track-Namen | Reject | Parsed |
| `season3_eu/ymir work/monster2/ch_officer/skipia_officer.gr2` | Doppelte Bone-Namen | Reject | Parsed |
| `season3_eu/ymir work/monster2/ch_officer/skipia_officer_lod_01.gr2` | Doppelte Bone-Namen | Reject | Parsed |

## 20 Fälle mit ungeklärter Verwendung

**20 usage unresolved bleiben 20 usage unresolved; davon jetzt 19 parsed und 1 NaN-Reject.** Kein Fall wurde ohne neuen positiven Referenznachweis zu produktiv oder ungenutzt umklassifiziert. Zusätzlich zum bestehenden F3/4-Inventar wurde nur gezielt nach Backup-/run10-/alten Season-Pfaden in Root-Configs, UserInterface und Server-Quests gesucht; keine neuen Treffer. Die Motlists des Rotdieben-Generals wurden direkt geprüft. Das ist kein vollständiger erneuter Client-Audit und kein Beweis der Nichtverwendung. Beleg: `build/f3a/usage-review.log`.

| Pfad unter `m2dev-client/assets` | Ursache | Vorher | Nachher |
|---|---|---|---|
| `PC/ymir work/pc/assassin/assassin.gr2` | Mehrere Skeleton-Wurzeln | Reject | Parsed |
| `PC/ymir work/pc/sura/general/run10.gr2` | PeriodicLoop | Reject | Parsed |
| `metin2_patch_dragon_rock_mobs/ymir work/monster2/redthief_general/front_damage.gr2` | NaN-Kurven | Reject | Reject: NaN |
| `metin2_patch_easter1/ymir work/pc/warrior/warrior_rabbit1_backup.gr2` | Mehrere Skeleton-Wurzeln | Reject | Parsed |
| `season1/season1/monster/ch_officer/skipia_officer_attack.gr2` | Doppelte Track-Namen | Reject | Parsed |
| `season1/season1/monster/ch_officer/skipia_officer_attack1.gr2` | Doppelte Track-Namen | Reject | Parsed |
| `season1/season1/monster/ch_officer/skipia_officer_back_dead.gr2` | Doppelte Track-Namen | Reject | Parsed |
| `season1/season1/monster/ch_officer/skipia_officer_back_knockdown.gr2` | Doppelte Track-Namen | Reject | Parsed |
| `season1/season1/monster/ch_officer/skipia_officer_back_standup.gr2` | Doppelte Track-Namen | Reject | Parsed |
| `season1/season1/monster/ch_officer/skipia_officer_dead.gr2` | Doppelte Track-Namen | Reject | Parsed |
| `season1/season1/monster/ch_officer/skipia_officer_front_damage.gr2` | Doppelte Track-Namen | Reject | Parsed |
| `season1/season1/monster/ch_officer/skipia_officer_front_damage1.gr2` | Doppelte Track-Namen | Reject | Parsed |
| `season1/season1/monster/ch_officer/skipia_officer_front_standup.gr2` | Doppelte Track-Namen | Reject | Parsed |
| `season1/season1/monster/ch_officer/skipia_officer_run.gr2` | Doppelte Track-Namen | Reject | Parsed |
| `season1/season1/monster/ch_officer/skipia_officer_wait.gr2` | Doppelte Track-Namen | Reject | Parsed |
| `season1/season1/monster/ch_officer/skipia_officer_wait1.gr2` | Doppelte Track-Namen | Reject | Parsed |
| `season1/season1/pc/assassin/assassin.gr2` | Mehrere Skeleton-Wurzeln | Reject | Parsed |
| `season2/season2/monster/ch_officer/skipia_officer.gr2` | Doppelte Bone-Namen | Reject | Parsed |
| `season2/season2/monster/ch_officer/skipia_officer_back_dead.gr2` | Doppelte Track-Namen | Reject | Parsed |
| `season2/season2/monster/ch_officer/skipia_officer_lod_01.gr2` | Doppelte Bone-Namen | Reject | Parsed |

## Fünf Pack-Override-Fälle

Alle fünf Rohdateien bleiben mit `declared file size differs from input` abgelehnt. Die aufgelösten Packversionen bleiben nativ lesbar; keine Reparatur der Rohkopien.

| Pfad unter `m2dev-client/assets` | Ursache | Vorher | Nachher |
|---|---|---|---|
| `season3_eu/ymir work/zone/dungeon/haven_dungeon/skipia_boss.gr2` | Überlagerte defekte Rohkopie | Reject | Raw Reject; Pack native lesbar |
| `season3_eu/ymir work/zone/dungeon/haven_dungeon/skipia_passc.gr2` | Überlagerte defekte Rohkopie | Reject | Raw Reject; Pack native lesbar |
| `season3_eu/ymir work/zone/dungeon/haven_dungeon/skipia_passl.gr2` | Überlagerte defekte Rohkopie | Reject | Raw Reject; Pack native lesbar |
| `season3_eu/ymir work/zone/dungeon/haven_dungeon/skipia_passp.gr2` | Überlagerte defekte Rohkopie | Reject | Raw Reject; Pack native lesbar |
| `season3_eu/ymir work/zone/dungeon/haven_dungeon/skipia_passt.gr2` | Überlagerte defekte Rohkopie | Reject | Raw Reject; Pack native lesbar |

Der Produktions-Packloader prüfte alle 52 Einträge erneut: 47 Raw-/Packversionen byteidentisch; fünf überlagert. **50/52 aufgelöste GR2-Packversionen nativ lesbar**, zwei NaN-Clips abgelehnt; explizite Granny-Referenz lädt alle 52 Packversionen. Alle **80 Config-Vergleiche byteidentisch**. `SilentFallbacks=0`, `AssetDocuments=0`, `GR2ReaderResources=0`. Audit-Exitcode **2** ist die dokumentierte Reject-Antwort und kein bestandenes Production Gate.

## Neuer vollständiger Corpus

| Kennzahl | Ergebnis |
|---|---:|
| Total | 9.166 |
| Parsed | 9.159 |
| Unsupported version/compression/type | 0 |
| Validation rejected (`malformed`) | 7 |
| Other failure | 0 |
| Pack-overridden raw copies, Teilmenge der 7 | 5 |
| Usage unresolved, ursprüngliche Verwendungsklasse | 20 |
| Davon weiterhin raw rejected | 1 |
| Neue Rejects gegenüber dem ursprünglichen Corpus | 0 |
| production referenced unsupported | **1** |

Raw-Coverage: **99.923631 %**. Keine 100-%-Behauptung. Der vollständige Scanner liest und validiert Container/Modelle/Kurven; er sampelt nicht sämtliche Animationen. Dauer des finalen Scans: 28,9704 s. Belege: `build/f3a/corpus-verified/files.tsv`, `summary.tsv`, `build/f3a/corpus-verified.log`.

## Parität und Tests

`AssetRuntime.GR2Compatibility` ist SDK-frei und läuft unter MSVC und GCC/LP64. Technische Fixtures: Forest und doppelte Namen mit unveränderten Indizes, Cycle-/Parent-Negativfälle, Sorted-/Unsorted-Duplikate, kodierte Namen, leere Geometrie plus ungültige Indizes, PeriodicLoop und nichtfinite Parameter, gekürzte Textmarken plus negative/nichtfinite Zeiten, Float-Präzision bei steilem linearem Segment, NaN-Curve-Reject. Die bestehenden GR2Safety-Checks bleiben erhalten.

`AssetRuntime.GR2CompatibilityParity` nutzt Granny ausschließlich als explizite Referenz. **4.032** Sorted-/Unsorted-Duplikat-Lookups und **131.072** Byte-/Suffixvergleiche. Sechs vollständige statische Modelle: Hair, Historian, Halloween, Ch-Officer plus LOD, leeres Redthief-LOD. Geprüft werden Counts, Namen, Vertex-/Indexdaten, Materialien, Bones/Parents/Binddaten, Gewichte und Remaps.

Neun Animationspaarungen: Hair/Warrior-Wait, Doctor-Die, Ogre-Run, Crustacean-Officer-Run, Pig-Walk, Halloween-Walk1, Historian-Run, Ch-Officer-37 und Skeleton-Soldier-Bow-00. Zehn Pose-Zeitpunkte einschließlich Loopgrenzen; CPU-Vertexvergleich und D3D11-GPU-Readback; Root-Motion bei drei Geschwindigkeiten und vier Zeitpunkten. Sämtliche bisherigen Toleranzen unverändert. Ressourcenprüfung am Ende erfolgreich.

| Messung | Größte Abweichung, Compatibility Release | Unveränderte Grenze |
|---|---:|---:|
| translation | 3.05176e-05 | 0.0002 |
| rotation | 1.83593e-06 | 2e-05 |
| scale | 5.96046e-07 | 2e-05 |
| world | 0.000389099 | 0.002 |
| palette | 0.000610352 | 0.002 |
| root_motion | 1.52588e-05 | 0.0002 |
| position | 0.000427246 | 0.005 |
| normal | 5.39422e-06 | 5e-05 |

| Prüfung | Ergebnis | Beleg |
|---|---|---|
| Vollständiger Windows Release-Build | erfolgreich, Exit 0 | `build/f3a/release-build-final.log` |
| Vollständiger Windows Debug-Build | erfolgreich, Exit 0 | `build/f3a/debug-build-final.log` |
| GCC 12.4 / Cygwin LP64-Build | erfolgreich, Exit 0 | `build/f3a/gcc-build-final.log` |
| Release Fast Gate | **8/8**, 24,17 s | `build/f3a/release-tests-final.log` |
| Debug Fast Gate | **8/8**, 73,01 s | `build/f3a/debug-tests-final.log` |
| GCC/LP64 portable Tests | **15/15**, 0,71 s | `build/f3a/gcc-tests-final.log` |

Windows-Auswahl: Renderer.StartupOptions, GR2Warmup, GR2Safety, GR2Independence, GR2Parity, GR2Compatibility, GR2CompatibilityParity, AnimationRuntime.Contracts. Keine langen Suites und kein Fuzzer. Die Windows-Builds enthalten weiterhin bekannte **LNK4099**-Warnungen für fehlende PDBs der statischen Python-/zlib-Bibliotheken sowie im Debug-Build **LNK4075** am unveränderten TerrainGpuTest-Linkziel; sie werden nicht als warnungsfrei bezeichnet. Der anfängliche GCC-Testbuild benötigte einen expliziten `<algorithm>`-Include im neuen Test; die finalen Builds/Tests oben sind erfolgreich.

## Validierungs- und Scope-Grenzen

Unverändert: Container-Bounds/Offsets, Relocations, Counts, CRC, Allocation-Budgets, endliche Floatwerte, Parent-Bounds, Self-Parent-/Cycle-/Depth-Prüfungen, Quaternionvalidierung, Curve-Key-Counts/Time-Limits und Bone-Palette-Mathematik. Die gezielten Erweiterungen repräsentieren belegte GR2-Semantik. Ein Failure im ZiiNAN-Modus bleibt sichtbar; der tatsächliche Pack-Dispatch prüft zusätzlich, dass keine Granny-Dateilesung als Fallback erfolgt.

Produktionscode geändert: sieben Dateien in `AnimationRuntime` und `AssetRuntime/GR2`. Die einzige Runtime-Anpassung ist die ausdrücklich angeforderte Source-Semantik im Skeleton-Initializer; Sample/Evaluate/Blend/BuildPalette bleiben unangetastet. Neue technische Fixtures/Diagnosewerkzeuge und Paritätserweiterung liegen unter `tests/AssetRuntime`. Kein geänderter Renderer, kein GPU-Skinning, kein Datenasset und keine Spiel-/Server-Config. `src/AssetRuntime/GR2ReaderMode.h` bleibt mit Granny als Default unverändert.

## Native Runtime Smoke

**Nicht ausgeführt.** Die ausdrückliche Voraussetzung „alle 27 produktiven Rejects geschlossen“ ist wegen des NaN-Clips nicht erfüllt. Deshalb keine Login-/Character-Select-/Ingame-Abnahme und keine Behauptung zu Ingame-CPU-Deformation, GPU-Fallback, Pose-/Importzählern oder Exitcode. Die freigegebenen Ressourcen des Paritäts-/Packharness ersetzen diesen Smoke nicht.

## Git / Reproduktion / Stop

Kein Commit, Push, Reset, Restore, Clean, Stash oder Rebase. Die Änderungen liegen im Source-Worktree. Der Client-Worktree enthält weiterhin vorhandene `config/channel.inf`-Änderung und Diagnose-Logs mit Zeitstempeln vom Morgen; keine davon wurde für F3-A geändert oder bereinigt. Kein neu gebauter Client wurde installiert oder gestartet.

Finale Diff-Prüfung: `git diff --check` erfolgreich; ausschließlich die genannten Reader-/Test-/Berichtsdateien im Source-Diff. Die [Ergebnis-JSON](phase-f3a-compatibility-results.json) enthält die Hashes der Inventar-/Corpus-/Pack-/Diagnoseeingaben; [Evidence-JSON](phase-f3a-evidence.json) fixiert finale Build-/Testlogs und beide gebauten Client-Binaries.

Reproduktion im Source-Repository:

```powershell
cmake --build build-c3x/windows --config Release --parallel 8
cmake --build build-c3x/windows --config Debug --parallel 8
cmake --build build-c3x/cygwin-common --parallel 8
ctest --test-dir build-c3x/windows -C Release -R '^(Renderer.StartupOptions|AssetRuntime.GR2(Warmup|Safety|Independence|Parity|Compatibility|CompatibilityParity)|AnimationRuntime.Contracts)$' -V
ctest --test-dir build-c3x/windows -C Debug -R '^(Renderer.StartupOptions|AssetRuntime.GR2(Warmup|Safety|Independence|Parity|Compatibility|CompatibilityParity)|AnimationRuntime.Contracts)$' -V
ctest --test-dir build-c3x/cygwin-common --output-on-failure
cmake --build build-c3x/windows --config Release --target GR2CompatibilityProbe GR2MigrationAudit
& build-c3x/windows/src/AssetRuntime/Release/GR2CorpusScanner.exe ../m2dev-client/assets build/f3a/corpus-verified
& build-c3x/windows/tests/AssetRuntime/Release/GR2CompatibilityProbe.exe ../m2dev-client/assets build/f34/inputs.tsv > build/f3a/root-cause-probe.log
& build-c3x/windows/tests/AssetRuntime/Release/GR2MigrationAudit.exe ../m2dev-client build/f34/pack-order.txt build/f34/inputs.tsv build/f3a/packed-audit.tsv
python tests/AssetRuntime/summarize_gr2_compatibility.py
```

Pack-/Input-Manifeste stammen aus dem vorhandenen F3/4-Audit; dessen Generator ist `tests/AssetRuntime/prepare_gr2_migration_audit.py`. Python-Aufruf ggf. mit der vorhandenen vollständigen Runtime-Pfadangabe ausführen. Der Corpus-Scanner liefert Coverage als Bericht; Exit 0 allein bedeutet nicht 100 % Coverage. Der Pack-Audit liefert aktuell erwartungsgemäß Exit 2.

**F3-A: NO-GO; STOP.** Der verbleibende produktive NaN-Clip ist vor einer erneuten Gate-A-Freigabe fachlich zu klären. Kein weiterer Migrationsschritt wurde begonnen.
