# F3/4-X – Production Migration / Granny Removal

Stand: 2026-09-14. **NO-GO. Gate A ist bereits am Produktions-Coverage-Preflight gescheitert. Gate B wurde nicht begonnen.**

Der native Reader lehnt **27 nachweislich über Client-Konfigurationen benötigte Dateien** ab. Ihre tatsächlichen Packbytes sind mit den abgelehnten Rohdateien identisch; der explizite Granny-Referenzprovider lädt sie erfolgreich. Ein Wechsel des Defaults würde damit bestehende Produktionspfade verlieren. Die STOP-Bedingung des Auftrags greift: „Wenn IRGENDEIN produktiver Pfad noch Granny braucht: STOP. Gate B NICHT beginnen.“ Dies ist keine Freigabe des unveränderten F2-X-Readers für Production und kein abgeschlossener Granny-Removal.

## 1. Ausgangslage / Baseline Freeze

Sauberer Source-Stand vor Beginn: `17858daf14e8bb8af9322f4fc31eb0b09ed865d7`. Keine Änderungen am produktiven Source, Reader, Animation, Prewarm, Renderer oder an Originalassets in diesem Arbeitsstand.

[Baseline-Metadaten](phase-f34-baseline.json) konservieren Commit, SHA-256/Größen der vorhandenen Build-/Referenzbelege und unveränderte F1-X-Toleranzen. Die bisherigen Berichte [F2-X](phase-f2x-ziinan-gr2-reader.md) und [F2-P](../performance/phase-f2p-120fps-load-warmup.md) bleiben vollständig erhalten.

| Nachweis vor F3/4 | Gesicherter Stand |
|---|---|
| Raw-Corpus | 9.166 total; 9.114 parsed; 52 rejected; **99,43 %** |
| F2-P Release | 43/43 bestehende gezielte Tests; späterer Spawn-/Namensfarben-Quick-Fix separat dokumentiert |
| F2-P Debug | 42/43 plus bestandener gezielter Warmup-Nachtest; kein nachträglich behaupteter vollständiger neuer Lauf |
| F2-P GCC/LP64 | 14/14 |
| Finaler manueller Release | `F484BE166DEB305A8141FD1A9A1AB26BF4C29E5D9DDB421E966DD1F3681741CE` |
| Vorhandene Debug-EXE | `525D32EF7750432F2370BBCB6B07B4FB35FB982A663FC7E8E015B3E34B7350FB` |
| Finaler F2-P-Lauf | 962 Native-Reads; 0 Granny-Reads/Pose-/Importsamples; 0 CPU-Deformation/Fallback; Ressourcen 0; Exit 0 |

Die Debug-EXE ist ein gesichertes vorhandenes Artefakt, kein Nachweis eines frischen Builds des finalen Quick-Fixes. Frühere manuelle Bestätigungen werden nicht als neue F3/4-Abnahme ausgegeben.

## 2. Gate-A Plan und ausgeführter Umfang

Baseline erfassen → alle 52 Rejects samt Config-/Packauflösung prüfen → nur bei vollständiger Coverage Golden-Daten fertigstellen, Default wechseln und kurze Build-/Runtime-Gates ausführen → erst dann Gate B.

Ausgeführt: Baseline, vollständiges Inventar aller 52 Einträge, tatsächlicher Pack-/Provider-Preflight, kurze Wiederprüfung der bestehenden Referenztests, Bericht. Der Produktions-Coverage-Fehler stoppt die nachfolgenden Migrationsschritte. Keine Parser-Sicherheitsprüfung wurde zur Erhöhung der Coverage gelockert; fehlende Semantik muss vor einem neuen Gate-A-Versuch korrekt implementiert und gegen Referenzen geprüft werden.

## 3. 52 Rejected Classification

Jede Datei mit Pfad, Kategorie, Reader-Grund, Nutzungseinstufung, Rassen-/Konfigurations-/Serververweisen und Disposition steht in der [CSV](phase-f34-rejected-classification.csv). Die [detaillierte JSON-Evidenz](phase-f34-rejected-classification.json) enthält zusätzlich SHA-256 der Rohdateien, genaue Referenzzeilen, Motion-Listen und Ergebnisse für rohe/gepackte Daten.

| Einstufung | Dateien | Ergebnis |
|---|---:|---|
| Aktive Client-Konfiguration nachgewiesen | **27** | Gepackt byteidentisch; Native lehnt ab; Granny lädt. Produktionsblocker. |
| Aktive Nutzung nicht abschließend geklärt | **20** | Gepackt byteidentisch; Native lehnt ab; Granny lädt. Nicht als unbenutzt freigegeben. |
| Abgelehnte Rohkopie wird im Pack überlagert | **5** | Gepacktes Ziel ist verschieden und native-lesbar; defekte Rohkopie bleibt rejected. |
| Gesamt | **52** | Keine Behauptung von 100-%-Coverage. |

Alle **52 Raw-Rejects wurden erneut reproduziert**. Die 47 byteidentischen gepackten Rejects wurden über den echten `CPackManager` gelesen. Alle 80 für die Klassifizierung herangezogenen Konfigurationsdateien sind mit den Packversionen byteidentisch. Der Pack-Ladereihenfolge liegt `PackInitialize` in `src/UserInterface/UserInterface.cpp` zugrunde; spätere Packs überschreiben frühere Einträge. Alle 52 gepackten Ziele laden über den expliziten Granny-Referenzprovider.

Die Einordnung „malformed“ bedeutet eine Verletzung des aktuellen Reader-Vertrags, nicht automatisch eine für Production unbrauchbare Datei. Insbesondere eindeutige Bone-/Tracknamen sind eine aktuelle Runtime-Anforderung, deren Verletzung Granny nicht am Laden hindert. Ein erfolgreicher Granny-Load allein ist wiederum kein visueller Paritätsnachweis.

Konkrete Blocker:

| Produktionspfad | Nachweis / fehlende Semantik |
|---|---|
| Krieger-Haar `hair_11_1.gr2` | `root/msm/warrior_m.msm`, HairIndex 5001 und weitere Varianten; mehrere Skeleton-Roots |
| Krustentier-Offizier/-Soldat | Rassen 3604/3601; MSM → motlist → run/walk.msa → GR2; PeriodicLoop/RootMotion |
| Ent-/Manticore-/Ogre-Bosse und Ogre-Soldat | Registrierte Lauf-/Angriffsclips; PeriodicLoop/RootMotion |
| Rotdieben-General | `back_damage.gr2`, auch aus anderem General-Set referenziert; nichtendliche Floats |
| Rotdieben-Soldat 2 | Impliziter `_lod_01.gr2`-Load aus `CRaceData::GetLODModelThing`; leeres Mesh |
| Doctor, Halloween-NPC, Historian, Pig-Pet | Registrierte Death-/Idle-/Walk-/Run-Clips bzw. Modelle; RootMotion oder nicht eindeutige/leere Namen |
| Setaou-Offizier / `ch_officer` | Rasse 2404 u. a.; aktuelles `d:/ymir work/monster2/ch_officer`-Set, Modell/LOD und `37.gr2` abgelehnt; lokaler Server-Spawn in `share/locale/english/map/metin2_map_skipia_dungeon_01/regen.txt:645` |

Die 20 ungeklärten Fälle umfassen beide alten `assassin.gr2`, Sura `run10.gr2`, `warrior_rabbit1_backup.gr2`, Rotdieben `front_damage.gr2` sowie 15 `season1/season2`-Offizierdateien. Zum Teil existieren alte lokale Motion-/Modelverweise, aber keine vollständig bewiesene aktuelle Einstiegsroute. Sie werden weder gelöscht noch pauschal als obsolete/editor assets bezeichnet. Dies bleibt eine offene Nutzungsklassifikation und verhindert ebenfalls ein vollständiges Gate-A-GO.

Die fünf `season3_eu/.../haven_dungeon/skipia_{boss,passc,passl,passp,passt}.gr2` haben eine falsche deklarierte Dateigröße. Das spätere `zone`-Pack liefert jeweils eine andere, erfolgreich nativ gelesene Datei. Die logischen Dungeon-Properties existieren; die abgelehnten physischen Rohkopien werden aktuell nicht gewählt. Die Corpuszahl bleibt davon unberührt.

## 4. Golden Reference Preservation

Bestehende F2-X-Parity-Harnesses, Original-Testpfade, Resultatlogs und F1-X-Grenzen wurden nicht geändert oder entfernt. Metadaten/Hashes und historische Maximalfehler sind in der Baseline konserviert. Der vorhandene `GR2ParityTest` prüft weiterhin elf Modelle (Warrior, Hair, Weapon, Wolf, Boss, Mount, Building, Prop und drei Randfälle), 13 Loop-Clips plus endlichen Attack sowie Pose-/Palette-/Vertex-Parität einschließlich GPU-Readback.

**Noch nicht abgeschlossen:** eigenständige deterministische Golden-Fixtures mit ausgewählten Referenzmatrizen/Vertexwerten und SDK-freie Golden-Tests. Der Baseline-Hashkatalog ersetzt diese nicht. Da Gate A vorher scheitert und Granny bestehen bleibt, wurde kein Reference-Harness abgelöst. Vor einem künftigen Removal ist dieser Schritt zwingend nachzuholen; keine proprietären GR2-Dateien wurden dupliziert.

## 5. Default Provider Switch

Nicht vorgenommen. `GR2ReaderMode.h` und `Renderer/StartupOptions.h` behalten Granny als Default. ZiiNAN bleibt explizit anwählbar. Kein neuer Production-Client wurde ausgeliefert.

## 6. No-Fallback Policy

Der bestehende `LoadModel` wählt für `.gr2` genau einen Provider. Der Audit setzt explizit ZiiNAN und prüft vor/nach jedem nativen Load den Granny-Read-Zähler: **kein Silent-Fallback**. Die anschließenden Granny-Loads sind ausdrücklich getrennte Referenzaufrufe dieses Auditprogramms. Sie sind kein Production-Fallback und kein behaupteter Null-Granny-Lauf.

## 7. Player Coverage

Kein neuer vollständiger Playerklassen-/Geschlechter-Gate. Warrior-Referenzdaten bestehen im kurzen Baseline-Test. Das produktive Krieger-Haar 5001 ist ein bestätigter Blocker. Sura `run10.gr2` bleibt ungeklärt; der vorhandene separate Fix für Sura-Clips mit leeren Model-Records wurde nicht verändert. Keine pauschale Assassin-/Sura-/Shaman-/Wolfman-Freigabe.

## 8. NPC / Mob / Boss

Bestehende Wolf-/Boss-Referenzen bestehen. Neue Produktions-Coverage scheitert an den unter Punkt 3 nachgewiesenen Dateien; eine bestandene Wolf-Fixture deckt z. B. den Setaou-Offizier nicht ab.

## 9. Mount

Vorhandene Horse-Referenz im Parity-Test bestanden. Kein neuer Mount/Rider-Production-Smoke; Pig-Pet-Walk ist als separater produktiver NPC/Pet-Pfad betroffen.

## 10. Hair / Attachments

Bestehende Hair-/Weapon-Referenzdaten bestehen. Kein neuer vollständiger Near/Far/Near-, Shield- oder Armor-Smoke; aktives Easter-Hair verhindert GO.

## 11. Static World

Bestehende Building-/Prop-Referenzdaten bestehen. Die fünf Haven-Rohkopien sind aktuell überlagert, ihre gepackten Ersatzdateien nativ lesbar. Kein neuer vollständiger World-/Camera-Blocker-Smoke.

## 12. Character Select

Keine neue manuelle Prüfung mit einem nativen Default-Client, da der Switch gestoppt wurde. Frühere F2-P-Beobachtungen ersetzen diesen Nachweis nicht.

## 13. Collision / Pose Lifetime

Keine Änderung an vorzeitigem Bone-Matrix-Zugriff oder am korrigierten Shutdown der Collision-Pool-Besitzer. Der vorhandene Reader-Safety-Test besteht erneut. Der Audit endet mit `AssetDocuments=0`, `GR2ReaderResources=0`. Das ist kein vollständiger neuer Client-Shutdown-Nachweis.

## 14. Prewarm

F2-P-Logik unverändert: Local Player, minimale relevante Clips und erste Pose/Renderressourcen vor World Reveal; danach bekannte Umgebungsactors. `GR2WarmupTest` besteht in Release/Debug sowie im portablen Testsatz. Keine neue Prewarm-Architektur.

## 15. Performance Sanity

Keine neue Performanceoptimierung oder A/B-Reihe. Historische F2-P-Werte bleiben gesichert: mit Prewarm keine Asset-Imports in den geprüften Cold-/Warm-Gameplayphasen; finale manuelle lokale Vorbereitung 237,843 ms minimale Clips und 2,027 ms erste Pose/Ressourcen. Diese historischen Messungen sind kein neues F3/4-Performance-GO. Debug-Allokationsgrenzen gelten weiterhin mit der dokumentierten STL-Proxy-Abgrenzung.

## 16. Release Gate A

Auditprogramm Release frisch gebaut. Bestehende kurze Baseline-Tests erneut **6/6 bestanden, 15,69 s**: StartupOptions, GR2Warmup, GR2Safety, GR2Independence, GR2Parity, AnimationRuntime.Contracts. Keine neue vollständige Production-Build-Abnahme. Belege: `build/f34/audit-release-build-final.log`, `release-baseline-tests.log`.

Der erste Sandbox-Buildversuch scheiterte beim Zugriff von MSBuild auf die Windows-SDK-Registrierung. Der danach autorisierte Build außerhalb der Sandbox besteht; kein SDK- oder Renderer-Workaround im Source. Frühere externe LNK4099/LNK4075/LNK4098-Hinweise bleiben im Baseline-Bericht korrekt dokumentiert.

## 17. Debug Gate A

Vorhandene Baseline-Testprogramme erneut **6/6 bestanden, 52,46 s**, gleiche gezielte Auswahl; `build/f34/debug-baseline-tests.log`. Kein neuer vollständiger Debug-Clientbuild und kein bestandenes Production-Gate behauptet.

## 18. GCC Gate A

Vorhandener GCC-12.4/Cygwin-LP64-Testsatz erneut **14/14 bestanden, 0,43 s**; `build/f34/gcc-baseline-tests.log`. Kein neuer GCC-Code und kein portabler Client-Rendernachweis. Keine langen Suites, kein Fuzzer.

## 19. Runtime Gate A

Nicht durchgeführt: frischer Client ohne Reader-Schalter mit Login → Character Select → Ingame. Das ausführbare Pack-/Provider-Preflight ersetzt diesen geforderten visuellen/servergebundenen Runtime-Smoke nicht.

## 20. Multi-Map Gate A

Nicht durchgeführt; A1 → B1 → A1 und zusätzliche Dungeon-Abdeckung bleiben nach Behebung der Blocker erforderlich. Ein konkreter lokal konfigurierter Skipia-Spawn ist bereits als Gegenbeispiel vorhanden.

## 21. Granny Counters Gate A

Kein neuer Production-Default-Lauf, folglich kein vollständiger neuer Nullzählernachweis. Audit: **52 Raw-Rejects, 47 identische Packed-Rejects, SilentFallbacks=0, AssetDocuments=0, GR2ReaderResources=0**; erwarteter Entscheidungs-Exitcode **2 (NO-GO)**. Der Reference-Audit nutzt Granny absichtlich. CPU-/GPU- und gesamter Client-Shutdown werden darin nicht gemessen.

## 22. Gate-A Decision

**FAILED / NO-GO.** Aktive Clientpfade benötigen Semantik, die der native Reader aktuell ablehnt. Keine Default-Umstellung und kein Gate B. Vor der Fortsetzung sind native Unterstützung/Parität für aktive Multi-Root-, Namens-/Track-, PeriodicLoop/RootMotion- und problematische Mesh-/Floatfälle sowie die offenen Nutzungsentscheidungen erforderlich. Ein Granny-Fallback oder bloßes Entfernen der Validierung ist keine Lösung.

## 23. Granny Dependency Audit

Kein Gate-B-Finalaudit. Der unveränderte Ausgangszustand enthält weiterhin produktive Abhängigkeiten: `src/AssetRuntime/Granny/Native.h` inkludiert das SDK; Provider/AnimationAdapter/EterGrnLib enthalten SDK-Typen/-Aufrufe; `extern/library/Granny/CMakeLists.txt` importiert `granny2_static.lib`. Bestehende Tests sind Referenzkategorie B; Dokumentation C; SDK-Binaries/Headers E. Kein vollständiger Dead-Code-Kategorie-D-Nachweis.

Aktueller gezielter Textscan in `src/**/*.{h,cpp}`: 1 Zeile mit `granny.h`, 195 Zeilen mit `granny_*`-Bezeichnern, 196 Zeilen mit `Granny...(`. Dies sind Regex-Zeilen, keine semantisch gezählten Runtime-Aufrufe. **Kein Zero-Audit-PASS.**

## 24. Provider Removal

Nicht begonnen; Gate A failed. `GrannyAssetProvider` bleibt erhalten.

## 25. Adapter Removal

Nicht begonnen; Granny-Animation-/Mesh-/Model-/Reference-Adapter bleiben erhalten.

## 26. Include Removal

Nicht begonnen; produktive SDK-Includes sind weiterhin vorhanden.

## 27. Type Removal

Nicht begonnen; produktive Granny-Typen sind weiterhin vorhanden.

## 28. Runtime Call Removal

Nicht begonnen; produktive SDK-Aufrufe sind weiterhin vorhanden.

## 29. Linker Cleanup

Nicht begonnen; Granny-Linkabhängigkeit bleibt erhalten.

## 30. CMake Cleanup

Nicht begonnen. Einzig hinzugefügt: explizites `EXCLUDE_FROM_ALL`-Audit-Testtool; der normale Client-Linkpfad bleibt unverändert. Kein Granny-freies Configure nachgewiesen.

## 31. EterGrnLib Status

Unverändert; noch kein Granny-freier Compat-Layer. Kein Rename.

## 32. Clean Configure

Nicht begonnen; `build-f34-clean` wurde nicht angelegt. Gate B ist gesperrt.

## 33. Clean Release

Nicht begonnen; kein Granny-freier Clean-Release-Build.

## 34. Clean Debug

Nicht begonnen; kein Granny-freier Clean-Debug-Build.

## 35. Binary Audit

Kein neuer Granny-freier Client vorhanden; daher kein finaler Import-/Dependency-Audit. Historische x64/D3D11-Architektur bleibt unverändert, ist aber keine F4-Freigabe.

## 36. GCC / LP64

Baseline-Testwiederholung unter Punkt 18. Kein Gate-B-Clean-Build.

## 37. Golden Static Tests

Vorhandene Live-Referenzparität in Release/Debug bestanden. Eigenständige SDK-freie Golden-Tests noch offen (Punkt 4).

## 38. Golden Animation Tests

Vorhandene Live-Referenzparität in Release/Debug bestanden. SDK-freie Golden-Samples noch offen. Keine Toleranzen angehoben.

## 39. GPU Skinning

GPU-Default nicht verändert. Vorhandener GR2-Parity-Test enthält GPU-Vertex-Readback und besteht. Kein neuer vollständiger Production-Nullzählerlauf.

## 40. GLB Regression

GlTF-Provider unverändert; portable GlTF-Provider-/Dependencytests im 14er Testsatz bestanden. Kein Gate-B-Render-Smoke.

## 41. Asset Tool Regression

Offline Asset Tool und Assimp unverändert. Wegen Gate-A-Stop kein neuer E2-X-Smoke und keine entsprechende PASS-Behauptung.

## 42. Final Runtime Smoke

Nicht begonnen; es existiert kein Granny-freier finaler Client.

## 43. Final Multi-Map Smoke

Nicht begonnen; Gate B nicht erreicht.

## 44. Shutdown

Pack-/Provider-Audit gibt Reader-/Asset-Dokumente vollständig frei. Sein Exit 2 ist die explizite NO-GO-Entscheidung, kein Crash. Historischer F2-P-Client: Exit 0 und Ressourcen 0. Finaler F3/4-Client-Shutdown nicht geprüft.

## 45. Source Zero-Audit

**Nicht bestanden / nicht erreicht.** Produktiver Source enthält Granny weiterhin; siehe Punkt 23. Keine gegenteilige Behauptung aus einem begrenzten Reader-Core-Audit.

## 46. Runtime Zero-Audit

**Nicht erreicht.** Keine Granny-freie Production-Runtime. Der native Dispatch hat nachweislich keinen Silent-Fallback; das beseitigt keine vorhandenen Granny-Link-/Referenzpfade.

## 47. Remaining GR2 Unsupported Cases

Unverändert 18 Unsupported- und 34 Malformed-Rohdateien. Alle 52 erneut geprüft; keine Datei umgeschrieben. Genau 5 abgelehnte physische Rohkopien werden aktuell überlagert, 27 sind aktive Blocker, 20 bleiben in der Nutzung ungeklärt. Weitere Reader-Arbeit ist für die aktive Coverage zwingend, kein optionaler F5-Schritt.

## 48. F2-P Preservation

Produktive Implementierung und bestehende Leistungsbelege unverändert. Startup-/Warmup-/Safety-/Animation-Vertragstests bestehen erneut. Keine neuen Aussagen zu sichtbaren Freezes, World Reveal oder beliebigen Maps ohne Runtime-Test.

## 49. Git Diff / Reproduktion

Änderungen ausschließlich in `tests/AssetRuntime` und `docs/assets`: ein explizites Auditprogramm mit CMake-Ziel, ein reproduzierbarer Klassifikationshelfer, Baseline-/Reject-Metadaten und dieser Bericht. Keine Änderungen in `src`, `extern`, `vendor` oder Originalassets. Builds und Rohlogs liegen ignoriert in `build/f34`; keine generierten Libraries/PDBs, SDK-Pfade oder proprietären GR2-Dateien für Git hinzugefügt. Kein Stage, Commit oder Push.

`git diff --check` besteht. Sieben geänderte/neue Dateien insgesamt; `git diff --stat` allein zeigt unversionierte neue Dateien noch nicht. Vollständiger Patch einschließlich aller neuen Dateien: `build/f34/review.patch`. Python-Syntax und Konsistenz der 52 CSV-/JSON-Einträge sind geprüft. Die vorbestehende Änderung an `m2dev-client/config/channel.inf` und die dort vorhandenen Logs blieben unangetastet; die Release-EXE besitzt weiterhin denselben Baseline-Hash.

Reproduktion aus dem Source-Root, mit verfügbarem Python 3 und bestehender MSVC-x64-Umgebung:

```powershell
python tests/AssetRuntime/prepare_gr2_migration_audit.py --corpus build/f2x/corpus-final/files.tsv --output build/f34
cmake --build build-c3x/windows --config Release --target GR2MigrationAudit --parallel 8
build-c3x/windows/tests/AssetRuntime/Release/GR2MigrationAudit.exe ../m2dev-client build/f34/pack-order.txt build/f34/inputs.tsv build/f34/packed-audit.tsv
# Exit 2 = vorhandene gepackte Rejects; Exit 1 = Audit-/Konfigurationsfehler.
# Exit 0 allein waere noch keine Runtime-/Nutzungs-/Gate-A-Freigabe.
python tests/AssetRuntime/prepare_gr2_migration_audit.py --corpus build/f2x/corpus-final/files.tsv --output build/f34 --packed-audit build/f34/packed-audit.tsv
```

Der letzte Schritt verlangt für alle referenzierten Konfigurationen identische Packbytes; unbekannte aktive Nutzung wird niemals automatisch als unbenutzt gewertet. Der Generator benötigt den lokalen Rohbestand und die lokale Serverkonfiguration, kopiert diese jedoch nicht.

## 50. GO / NO-GO

**F3: FAILED / NO-GO. F4: NOT STARTED. F3/4-X insgesamt: NO-GO.** Der Auftrag ist bis zu seiner harten Stop-Bedingung bearbeitet; die Produktionsmigration und der vollständige Removal sind nicht abgeschlossen.

## 51. Recommendation für F5-X

F5-X nicht beginnen. Zuerst die belegten Gate-A-Coverage-Lücken lösen, die 20 offenen Nutzungsfälle entscheiden, echte Golden-Fixtures sichern und Gate A vollständig wiederholen. Anschließend erst das hier unverändert offene Gate B. Kein Visual Remaster, PBR, Android oder UI-FPS-Thema begonnen.

## 52. Finaler Architekturstatus

Aktueller produktiver Default: GR2 → Granny-Provider → bestehende Asset-/Animationspfade → GPU Skinning → Diligent D3D11. ZiiNAN GR2 Reader/Animation bleiben opt-in. GLB → GlTF Provider bleibt verfügbar. Die Zielarchitektur mit ausschließlich nativem GR2-Reader und vollständig entferntem Granny ist **noch nicht erreicht**.
