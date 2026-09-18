# Originale Bodentexturen wiederhergestellt

> Historischer Nachweis vom 16.09.2026. Der damalige Testclient und die alten
> Arbeitsverzeichnisse wurden inzwischen entfernt. [Erhaltene Textbelege](evidence/content-tx-p0.zip)
> und [abschließendes Prüfergebnis](evidence/content-tx-p0/terrain-test-v4/original-terrain-v10/result.json)
> wurden bei der [Strukturbereinigung](../maintenance/repository-root-structure-cleanup.md)
> gesichert. Angaben zu Programmen, Backups und Arbeitsständen im folgenden Text
> beschreiben den damaligen Zustand.

Stand: 16.09.2026. Auf ausdrücklichen Benutzerwunsch sind im Originalclient und im normalen Testclient **alle Bodentexturen wieder original**. Der zuvor begonnene Transfer neuer Katalogtexturen ist damit aufgehoben. Neues Gras, Waldboden, Sand, Erde, Lava und Vulkanmaterialien sind nicht mehr aktiv.

## Aktiver Stand

- Originalclient: `C:\Users\ZiiNAN\Documents\GitHub\m2dev-client`.
- Normaler Testclient: `C:\Users\ZiiNAN\Documents\GitHub\m2dev-client-src\build-content-tx-p0\terrain-test-v4\runtime-manual`.
- 319 verbliebene Boden-Ersetzungen auf 84 Maps deaktiviert; 147 Gras-Zuordnungen waren bereits zuvor zurückgesetzt. Keine aktiven Ersatztexturen mehr.
- Beide Clients laden wieder ihre ursprünglichen Terrain-Dateien aus dem unveränderten Pack-Bestand. Die alten Texturen behalten ihre ursprüngliche Auflösung; es erfolgt keine künstliche 2K-Vergrößerung.
- Das großflächige 3D-Gras bleibt abgeschaltet. Bäume und Büsche bleiben erhalten.
- Die Release-Programmdatei bleibt auf dem bereits getesteten CONTENT-TX-P0-Stand, SHA256 `6f35b7f16d7ccafea526495b920e7777fcce5d3e276e13ce74f15938a7217376`. Sie fällt ohne `terrain/modern/` vollständig auf die Originalfarben zurück, sowohl in Modern als auch Classic. Ein Zurückkopieren der älteren H2-EXE würde auch die entfernte 3D-Gras-Erzeugung zurückbringen und wurde deshalb nicht durchgeführt.
- `Metin2_Debug.exe`, alle 91 Packs, Original-Startskripte, Karten-/TextureSet-Daten und persönliche Konfigurationen wurden nicht verändert. Der Debug-Client wurde in diesem Texturauftrag nicht neu gebaut; die Aussage zum abgeschalteten 3D-Gras gilt für Release.

## Ersatztexturen auf Benutzerwunsch gelöscht

Nach der Rücksetzung wurden am 16.09.2026 alle gesicherten Ersatz-Bodentexturen, älteren Testkopien, Materialdownloads und eigenen Texturentwürfe entfernt. Das betrifft die Ersatzdateien im Originalclient sowie die Arbeitskopien unter `build-content-tx-p0`.

- 4.597 Dateieinträge gelöscht; wegen mehrfach verwendeter Hardlinks entsprechen sie 127 eigenständigen Dateien und rund 2,92 GB Dateidaten.
- Keine Ersatz-Bodentextur im geprüften Arbeitsbereich verblieben. Die ehemaligen Sicherungsordner enthalten nur noch Metadaten beziehungsweise leere Unterordner; sie stellen keine Textursicherung mehr dar.
- Originaltexturen, alle Packs, Konfigurationen und Programme unverändert. 12.641 geschützte Dateien erneut per SHA256 geprüft.
- Vergleichsbilder, Katalogvorschauen, Prüfnachweise und Herkunftsangaben bleiben als Dokumentation erhalten. Historische Ersatztextur-Prüfläufe können ohne erneuten Materialbezug nicht mehr wiederholt werden.
- Die ältere Release-EXE-Sicherung in `terrain-test-v4/production-deployment-v9/backup` bleibt erhalten.

Löschinventar mit Dateipfaden und SHA256: `replacement-texture-cleanup/deletion-plan.json` im [Belegarchiv](evidence/content-tx-p0.zip). Ergebnis: `docs/terrain-replacement-cleanup.json` im Originalclient. Der bereits geprüfte Originaltextur-Stand bleibt aktiv; für das Löschen inaktiver Dateien war kein neuer Spieltest nötig.

## Prüfung

**PASS:** frischer nativer Prüflauf mit der bytegleichen Release-Datei des Originalclients, dessen Packs und dessen gepackter Vegetation; kein loses Test-Vegetationspaket. Nur das Offline-Prüfskript liegt in einem privaten Root-Pack. Neun feste Ansichten: A1, B1, C1, Wald, Kap, Wüste, offener Vulkan sowie Classic auf A1 und Rückkehr zu Modern.

- `ExitCode=0 Seconds=68.1218146 PID=42832`; alle neun Bilder erzeugt.
- Die native Dateiprüfung findet keine der 319 deaktivierten Ersatztexturen; keine Terrain-Override-Ladung im gesamten Lauf.
- Gras-Platzierungen, Gras-Zellen, Gras-Tiles und Gras-Speicher in allen neun Ansichten null.
- Alle neun Aufnahmen durch Codex gesichtet: urspruengliche Bodenmuster sichtbar, keine leeren Terrainflaechen. Dies ersetzt keine Benutzer-Sichtabnahme.
- Fehlerlog leer; keine Diligent-Fehler/Fatals und keine fehlgeschlagenen Terrain-/World-Draws. Alle geprüften Shutdown-Ressourcen null.
- 439 Paket-, Konfigurations-, Root-Quelldateien und die Debug-Datei hashgleich; zusätzlich 12200 zuvor geschützte Karten-/TextureSet-, Quell- und Build-Dateien unverändert.
- Original `pack/root.pck` unverändert: SHA256 `0c1c6f47e6dce89d912363882789d76b3cfe1395cad0290e2b59a7d434092994`.

Der frühere private Runtime-Prüfordner wurde entfernt. [Maschinenlesbares Ergebnis](evidence/content-tx-p0/terrain-test-v4/original-terrain-v10/result.json); damalige Kopie im Originalclient: `docs/terrain-original-restoration.json`.

Ein normaler Netzwerklogin und eine neue Benutzer-Sichtabnahme wurden in diesem Rücksetzungsauftrag nicht durchgeführt. Dieser begrenzte Nachweis ist keine vollständige Abnahme aller Maps oder anderer Renderer-Meilensteine.

## Bereits vorhandene Datenlücken

Der Originalbestand enthält weiterhin 36 der 40 zuvor geprüften Grasreferenzen. Drei von Bodenmasken verwendete Dateien für `season1/metin2_map_empirewar_a01` fehlen bereits im Original: `empirewar/field03.dds`, `empirewar/grass01.dds`, `empirewar/grass02.dds`. Außerdem fehlt `g/field/grass 01.dds`, dessen Referenz auf `metin2_map_t2` keine verwendeten Maskeneinträge hat. Die Rücksetzung ersetzt diese Lücken nicht durch neue Materialien.

Der separat bereits nachgewiesene Fehler statischer Objekt-Lichttexturen im Flammendungeon `metin2_map_n_flame_dungeon_01` bleibt außerhalb dieses Auftrags. Der offene Vulkan `metin2_map_n_flame_01` ist Bestandteil des erfolgreichen Prüflaufs.

Kein Commit, kein Push. Historische Katalog- und Testberichte bleiben als Verlauf erhalten; dieser Bericht beschreibt den aktuell aktiven Stand.
