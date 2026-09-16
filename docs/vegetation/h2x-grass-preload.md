# H2-X Nachkorrektur: Gras beim Kartenladen

Die gesamte Grasverteilung wird jetzt während `CMapOutdoor::Load` vorbereitet. Der vorherige, mit der Kamera wandernde und wieder verworfene Zellcache ist entfernt. Beim Vorbeilaufen werden weder Kandidaten erzeugt noch Terrainmasken oder Höhen neu ausgewertet.

Alle Kartenteile werden berücksichtigt, auch außerhalb der aktuell gestreamten Terrain-Nachbarschaft. Vorhandene Terrainlader lesen die benötigten CPU-Daten; entfernte Terrain-Geometrie und Splat-Texturen werden dafür nicht angelegt. Position, Höhe, Rotation, Größe, Windphase und Dichterang bleiben bis zum Kartenwechsel erhalten. Kompakte Datensätze benötigen 32 Byte pro Büschel; die Render-Batches verwenden dieselbe Vegetation-/Instancing-Pipeline.

Die bisherige kurze Sichtweite erzeugte zusätzlich einen sichtbaren Nahbereich. Low / Medium / High / Ultra reichen jetzt 50 / 75 / 100 / 130 Meter. Der vorhandene Impostor übernimmt ab ungefähr 35 Metern; die letzten 40 Prozent der Reichweite werden weich ausgeblendet. Die Gesamtverteilung bleibt bei Qualitäts- und Classic/Modern-Wechseln unverändert; Classic zeichnet weiterhin kein modernes Gras.

## Nachweise

Die aktuelle Beweisserie und Messwerte werden unter `build-h2x/grass-preload/` gespeichert. `result.json` bindet Tests, Client-/Content-Hashes, native Ladezähler, Performance und Git-Diff an den geprüften Stand. Die alten Messungen unter `build-h2x/performance/` gehören zum H2-Stand vor dieser Korrektur.

| Prüfung | Ergebnis |
| --- | --- |
| Release FAST GATE | PASS 72/72, 70,95 s |
| Debug FAST GATE | PASS 72/72, 166,57 s |
| GCC/LP64 | PASS 43/43, 5,66 s |
| Classic | 12/12 bytegleich in Release und Debug |
| Nativer Client | PASS 12 Ansichten, 4.875 Frames, Exit 0 |
| Diligent / CPU-Deformation / GPU-Fallback / Shutdown-Ressourcen | jeweils 0 |
| Source-/Link-/Binäraudit | PASS, keine Legacy-SDK-Abhängigkeit |

LP64-Vorläufe trafen auf gemischte Dateirechte vorhandener generierter Testausgaben. Drei Ausgabepfade wurden innerhalb des Buildordners gesichert; alle 30 enthaltenen Dateien wurden danach per Hash geprüft. Mit frischen Ausgaben lief das vollständige Gate erfolgreich. Produktionscode und Testanforderungen wurden dafür nicht geändert.

Der native Test prüft die vollständigen Ladezähler vor dem ersten Weltbild, eine Laufstrecke über mehrere Graszellen, einen zuvor nicht gerenderten Kartenteil, den Rückweg, alle vier Presets, Classic/Modern sowie B1 → A1 → B1. Nach Map-Destroy müssen die gespeicherten Grasplatzierungen null sein. Beim Lauf- und Presettest müssen Anzahl, Speicherbedarf und Vorbereitungsvorgang unverändert bleiben.

Der GPU-Test vergleicht das Bild aus kompakten vorbereiteten Grasdaten bytegenau mit den normalen Instanzen und prüft die Sichtbarkeit jenseits der bisherigen 26-Meter-Grenze. Bestehende Legacy- und Classic-Prüfungen bleiben aktiv.

[Aktuelle Galerie mit zwölf geprüften Ansichten](../../build-h2x/grass-preload/gallery.html). Die endgültige Laufkamera folgt der Geländeoberfläche; der entfernte Grasbereich liegt bei B1 (71800, 16600), außerhalb der ursprünglichen Terrain-Nachbarschaft. Frühere Kamera-Vorläufe bleiben separat erhalten und sind nicht Bestandteil dieser Galerie.

## Laden, Speicher und begrenzte Performance-Prüfung

| Karte | Vorbereitete Kartenteile | Grasbüschel | Platzierungsdaten | Vorbereitung |
| --- | ---: | ---: | ---: | ---: |
| B1 | 20/20 | 578.476 | 17,65 MiB | 78,71 ms |
| A1 | 20/20 | 625.189 | 19,08 MiB | 74,69 ms |
| B1 erneut | 20/20 | 578.476 | 17,65 MiB | 71,33 ms |

Speicherangaben bezeichnen die kompakten Platzierungsdaten; Zellindex, Container, geteilte Assets und GPU-Batches kommen hinzu. Die Vorbereitung wurde vor dem ersten Weltbild gemessen. Die B1-Daten und der Vorbereitungsvorgang bleiben während Lauf, Fernwechsel, Rückkehr und Presetwechsel unverändert.

Jeweils die ersten 180 markierten Frames nach zwei Sekunden Aufwärmen, B1-Nahkamera (1300, 24, -70), ohne gleichzeitigen Build/GPU-Test und ohne entfernte Ausreißer:

| Preset | CPU Mittel / P95 ms | GPU Mittel / P95 ms |
| --- | ---: | ---: |
| Low | 1,269 / 1,606 | 0,273 / 0,588 |
| Medium | 2,181 / 2,441 | 0,418 / 0,945 |
| High | 5,340 / 5,775 | 0,722 / 1,234 |
| Ultra | 11,249 / 12,491 | 1,034 / 1,789 |

Die größere Reichweite erhöht den Renderaufwand. CPU ist Frame-Arbeit ohne Present/Limiter; GPU stammt aus der bestehenden Frame-Abfrage. Einzelne Ausreißer bleiben: im ersten normalen B1-Fenster maximal 184,77 ms CPU / 122,65 ms GPU, im High-Nahfenster maximal 63,73 ms CPU. Ein bereits vorhandener anderer Client blieb unverändert geöffnet. Dies ist eine begrenzte Vegetationsprüfung, kein allgemeines FPS-/Hitch-GO; die Ausreißer wurden nicht ursächlich zugeordnet. Vollständige Messwerte und Methodenfenster stehen in `result.json` und den nativen CSV-Dateien.

## Abnahme

Der geprüfte normale Testclient ist sichtbar geöffnet: Modern / High, originales Startskript, keine Diagnoseargumente. Startbeleg: `build-h2x/grass-preload/manual-launch.json`, PID 3508. Release-SHA256: `959d03d5ff6e560a2043e300688811852ff56784d14236aea541a0ab003964cd`.

Dieser erste Testlauf änderte das separate Asset-Repository noch nicht. Auf den anschließenden Nutzerauftrag wurde der geprüfte Stand am 16.09.2026 in den normalen Spielordner übernommen: Release-EXE, vollständiges Vegetationspaket in den Root-Quellen und `root.pck`, dazu Modern/High-Konfiguration. [Übernahmebericht](../../../m2dev-client/docs/h2-vegetation-deployment.md) mit Sicherung, Umfang und gezieltem Pakettest. Drei native B1-Ansichten mit ausschließlich gepackten Vegetationsdaten bestanden; vor dem ersten Weltbild waren 20/20 Kartenteile und 578.476 Grasplatzierungen vorbereitet.

Kein Commit, kein Push; keine Folgephase begonnen. Technische Prüfungen PASS; das endgültige H2-GO bleibt bis zur Sichtabnahme durch den Nutzer offen. Der [Git-Diff](../../build-h2x/grass-preload/git-diff.patch) und `grass-preload/result.json` dokumentieren den geprüften Stand vor der Paketübernahme; aktuelle Übernahmebelege liegen unter `build-h2x/deployment-20260916/`. Der ausschließlich offline verwendete `PackContentAudit` erhielt dafür einen Modus zum Prüfen zusätzlicher Paketeinträge bei Erhalt aller bestehenden Inhalte. [Vegetation Content Priority](../../build-h2x/content/content-priority.md) unverändert.
