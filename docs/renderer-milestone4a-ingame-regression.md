# Milestone 4A – finale Ingame-Regression, 12.09.2026

> Historischer Milestone-Stand. Aktuell seit [M12](renderer-milestone12.md): Diligent D3D11 ist im ON-Build der Default; Legacy wird explizit mit `--renderer=legacy-d3d9` gewählt. OFF-Builds bleiben Legacy-only. Frühere Testergebnisse und Auswahlbeschreibungen unten gelten für ihren damaligen Stand.

Status: Nachprüfung des gefundenen 4A-Regressionsfehlers abgeschlossen, Build und 9/9 Tests bestanden. Keine vollständige Ingame-Abnahme behauptet: die unten genannten Lücken bleiben offen. Keine Migration über 4A hinaus.

## Auftrag und Abgrenzung

Legacy-Ingame wurde auf Nutzerwunsch verkürzt; danach wurde ausdrücklich nur die Diligent-Prüfung wieder aufgenommen. Getrennte Test-EXEs, eigene Config-/Logverzeichnisse, originale Packs nur lesend über Junctions. Kein Deployment in den normalen Clientordner, keine Änderung der normalen Schattenkonfiguration oder anderer laufender Spielclients.

## Legacy-Ingame (vor der Korrektur)

- Normaler Start ohne Renderer-Argument, PID 44360. Nutzer hat sich selbst angemeldet, die Map betreten und das Bild bestätigt.
- In C1/Pyungmoo gesehen: Spieler/Reittier, NPCs und nahe Kreaturen, Gebäude/Props, Bäume, Terrain, HUD/Inventar, Minimap/Kartenansicht und Quest-/Charaktereffekte. Inventar per Taste geöffnet; mehrere Kameraansichten und Ortswechsel beobachtet.
- Rund fünf Minuten / 31 Prozessmesspunkte. Private Bytes zwischenzeitlich etwa 476 MiB, zuletzt 466,74 MiB; kein offensichtlich fortlaufender Anstieg in diesem kurzen Ausschnitt. Kein Nachweis langfristiger Leak-Freiheit.
- A1 → B1 → A1, wiederholtes Minimize/Restore, vollständige Skill-/Mob-/anderer-Spieler-Prüfung und Shutdown wurden auf Nutzerwunsch übersprungen. Legacy-Exitcode nicht erfasst. Charakterauswahl nicht separat visuell abgenommen.
- Keine beobachtete Assertion, kein beobachteter Crash oder Device-Fehler. Bestehende Schadenseffekt-Diagnosen, `invalid idx 0` (Gildenmarken) und `Cannot find item by 1315` wurden nicht als Rendererfehler ausgegeben oder als Nebenaufgabe behoben.

## Diligent: tatsächlich gefundener Fehler

Normaler Start jeweils mit `--renderer=diligent-d3d11`, Login/Charakterauswahl durch den Nutzer.

| Lauf | Ergebnis | Beendigung |
|---|---|---|
| PID 50456, ursprüngliche Schattenstufe 3 | Terrain sichtbar, Objekt-Draws 0; aktiver Schattenreceiver-Pfad außerhalb des bestehenden 4A-Vertrags | Fenster-X, Exit 0 |
| PID 54980, isoliert Schattenstufe 0 | Weiterhin keine Gebäude, Objekt-Draws 0; Nutzer bestätigt | Fenster-X, Exit 0 |
| PID 30328, korrigierter Adapter, isoliert Schattenstufe 0 | Terrain, Gebäude, Steinlaterne und Props im normalen Ingame sichtbar; Objekt-Draws im Log bestätigt | Fenster-X, Exit 0 |

### Ursache und minimale Korrektur

`assets/root/introselect.py` ruft `grp.SetOmniLight()` auf. `CPythonGraphic::SetOmniLight()` in `src/EterPythonLib/PythonGraphic.cpp` aktiviert Licht 0 und Punktlicht 1. `CMapManager::BeginEnvironment()` ersetzt nur Licht 0. Der 4A-Adapter in `src/GameLib/StaticObjectBridge.cpp` lehnte alle zusätzlichen Lichter ab, obwohl der schattenfreie Objektpfad `SELECTARG1(TEXTURE)` verwendet und damit die beleuchteten RGB-Werte ignoriert. Der frühere isolierte Maptest hatte die normale Charakterauswahl nicht durchlaufen.

Die Korrektur überspringt ausschließlich die RGB-Lichtprüfung für diesen bereits unterstützten texturierten Pfad und erhält die Material-Alpha-Modulation. Bei `MODULATE` bleiben zusätzliche Lichter ausgeschlossen. Keine Veränderung des nativen Lichtzustands, kein Umbau von StateManager, Granny, Terrain, UI oder Shadern. Kein Schatten-, Alpha-Test-, Blend- oder neues Materialfeature.

`CaptureStaticMapObjectDraw` macht dieselbe lesende Adapterprüfung für den GPU-Test aufrufbar. `tests/Renderer/StaticObjectGpuChecks.h` reproduziert das verbliebene Punktlicht, prüft den weiterhin abgelehnten beleuchteten Fall sowie RGB-Parität und Material-Alpha 0,4 (GPU-Wert 102/255). Vor der Korrektur scheitert der Test exakt bei `texture-only adapter after character selection`; danach bestehen alle fünf Renderer-Tests.

## Tatsächliche Diligent-Sichtbarkeit / bekannte Einschränkungen

- Sichtbar: migriertes Terrain/Splatting und zugelassene starre opake PNT-Gebäude/Props mit unterstützten Diffuse-Texturen, bei ausgeschalteten Schatten.
- Weiterhin Legacy, im Diligent-Weltbild nicht zusammengesetzt: Spieler, NPCs, Mobs, Mounts/Attachments, SpeedTree/Bäume, Effekte, UI/Text/Minimap; außerdem Wasser und nicht unterstützte Objektvarianten. Die Start-/Login-UI ist vor Aktivierung der Diligent-Weltfläche weiterhin sichtbar. Das ist keine Freigabe als vollständig spielbarer Diligent-Client.
- Weiterhin ausgeschlossen: animierte/skinned/PNT2-Objekte, Blend/Opacity, Specular, aktive Alpha-Tests, zusätzliche Texturstages, zusätzliche RGB-Lichter bei beleuchteten Varianten, Schattenreceiver und nicht unterstützte Texturformate. Beispielsweise Stonedoor-/Bellhouse-Materialvarianten und Flag-/Drum-Texturformate werden im aktuellen C1-Log ausdrücklich ausgeschlossen. Vollständige Objektzahl-Parität wird daher nicht behauptet.
- Alpha-Test-Flächen sind nicht migriert; keine neue Cutout-Unterstützung zur Behebung fehlender Objekte. Der Alpha-Erhalt-Test betrifft opake Material-Alpha-Modulation, nicht Alpha-Test.
- Sichtbare Objekte zeigen keine offensichtliche verzerrte Position/Rotation/Skalierung oder falsche Texturen in den aufgenommenen Ansichten. Keine pauschale Abnahme sämtlicher Objektvarianten; synthetische GPU-Tests prüfen Transformation, Spiegelung, nichtuniforme Skalierung, Culling und gemeinsamen Terrain-/Objekt-Depth in beiden Zeichenreihenfolgen.
- Das normale Fenster hat unverändert keinen frei ziehbaren Resize-Rand. Keine neue Fensterfunktion ergänzt; Größenwechsel und Suspend/Restore werden durch die bestehenden Backend-/GPU-Tests geprüft.
- Hintergrund/Clear-Fläche und fernes Terrain sind nicht vollständig Legacy-paritätisch; keine Hintergrundmigration als Nebenaufgabe.

## Verifikation nach der Korrektur

- Release-Build mit Diligent ON: Exit 0.
- Neuer gezielter Reproduktionstest vor der Korrektur: erwarteter CTest-Exit 1.
- Finale Renderer-Tests: 5/5, Exit 0; Startup-Auswahl, Legacy D3D9, Diligent D3D11, Textur-CPU und Terrain-/Objekt-GPU-Parität einschließlich Resize/Depth/Lifetime.
- Vollständige vorhandene Testsuite: 9/9 bestanden, Exit 0, 483,98 Sekunden. Zusätzlich zu den fünf Renderer-Tests: fullbench, fuzzer, zstreamtest und playTests. Keine deaktivierten Tests oder gelockerten Toleranzen. Reproduktion: `ctest --test-dir build -C Release --parallel 3 --timeout 900 --output-on-failure`, mit Git-Bash `/usr/bin` vorn im PATH für die bestehenden Bibliothekstests.
- Reales korrigiertes Diligent-Ingame: Nutzer bestätigt Gebäude/Props sowie die angeforderten mehrminütigen Kamerabewegungen, mehrere Bereiche und dreimal Minimize/Restore mit „ja“. Unterschiedliche Ansichten wurden zusätzlich direkt aufgenommen. Nutzer bestätigt ausdrücklich: kein NPC und kein Charakter sichtbar; diese Systeme werden weiterhin nicht ins Diligent-Weltbild zusammengesetzt.
- A1 → B1 → A1 im echten Ingame nicht durchgeführt: Nutzer kann die Wechsel ohne sichtbare UI nicht ausführen („leider nicht“). Frühere isolierte Maptests werden nicht als nachträglicher Ersatz für diesen fehlenden Ingame-Test ausgegeben.
- Shutdown des korrigierten Clients: Nutzer schließt per Fenster-X, Exit 0. Lauf 11:24:29–11:31:06 CEST, rund 6 Minuten 37 Sekunden. `release live_objects=0`, `shutdown object_geometry=0 object_textures=0`.
- Korrigierte EXE SHA256: `A30A2C54EC6F2E61C45DE2A4263D06FDDE6097217BD710DA0AE556A1570CB0F6`.

### Stabilität und Ressourcen

40 Messpunkte im Abstand von etwa zehn Sekunden. Private Bytes nach dem ersten Laden ungefähr 542 MiB, beim Besuch weiterer Bereiche bis 624,98 MiB, zuletzt 612,90 MiB; Working Set zuletzt 491,46 MiB und 1.153 Handles. Ressourcen wurden mit neuen Bereichen nachgeladen und teils wieder freigegeben; beispielsweise sinkt die Zahl lebender Terrain-Texturen trotz fortlaufendem Upload-Zähler. Kein durchgehender Anstieg bei unverändertem Inhalt in diesem Ausschnitt, aber auch kein Langzeit-Leak-Nachweis und kein geprüfter A1/B1-Wechselzyklus. Objekt-GPU-Ressourcen beim Shutdown nachweislich 0. Kein separater VRAM-/Treiber-Live-Object-Profiler verwendet.

Kein beobachteter Crash, keine Assertion und kein beobachteter Device-/Upload-Fehler in diesem Lauf. Die Logs sind ausdrücklich nicht leer: Gildenmarken-`invalid idx 0`, Suchpfad-Fallbacks aus dem unveränderten `src/GameLib/RaceManager.cpp` (`Will Find Another Path`, NPC-/Facility-MSM-Dateien) sowie bestehende Schadenseffekt-Diagnosen. Diese Meldungen belegen keine neu verursachte 4A-Rendererregression; NPC-/Effekt-Sichtbarkeit bleibt wegen der fehlenden Komposition unprüfbar. Keine fachfremde Daten- oder Effektkorrektur.

333 erstmalige erfolgreiche Objektmeldungen, 7 Ausschlüsse wegen Materialvertrag und 8 wegen Texturformat über den besuchten Bereich; dies sind Logereignisse, keine vollständige Objektinventur und kein Beweis für identische Objektzahlen zu Legacy. Keine offensichtlichen Doppelobjekte in den aufgenommenen Ansichten; vollständige visuelle Parität ausdrücklich nicht behauptet.

Der vollständige Bibliotheks-/Renderer-Testlauf lief teilweise gleichzeitig mit der Ingame-Prüfung. Die Prozessmessung ist daher keine FPS-/Performance-Benchmark-Abnahme. Der Release-Link meldet bekannte fehlende Python-PDB-Debuginformationen (LNK4099), keinen Buildfehler.

## Änderungen in dieser Regression

Gegenüber dem vor der Regression erfassten Dateihash-Baseline nur drei bestehende Source-/Testdateien geändert: `src/GameLib/StaticObjectBridge.cpp`, `src/GameLib/StaticObjectBridge.h`, `tests/Renderer/StaticObjectGpuChecks.h`. Dazu dieses neue Protokoll und isolierte Testartefakte unter `build/milestone4a-ingame-20260912`. Die schon zuvor uncommitteten 4A-Änderungen bleiben erhalten. Kein Commit/Push und kein Milestone 4B.

`git diff --check`: Exit 0. Das Runtime-Repository hat weiterhin nur seine schon vorhandene Änderung an `config/channel.inf`; keine experimentelle EXE dorthin kopiert. Legacy bleibt Standardbackend, produktive Schattenkonfiguration unverändert. Eine neue OFF-Konfiguration wurde für diese kleine Korrektur nicht erneut gebaut; die vorhandenen Legacy-Tests laufen im ON-Build mit.

**STOP nach dieser Regression.** Kein Milestone 4B, keine Migration von Charakteren, NPCs, Bäumen, Effekten oder UI.
