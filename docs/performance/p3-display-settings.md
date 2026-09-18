# P3 – Anzeigeeinstellungen

Erweiterung des vorhandenen P3-Grafikmenüs. Source und Assets liegen weiterhin in den getrennten Checkouts `m2dev-client-src` und `m2dev-client`. Kein Production Deployment, Commit oder Push.

## Verhalten

- `GraphicsSettings` / `GraphicsRuntimeConfig` enthalten `resolutionWidth`, `resolutionHeight` und `displayMode`. FPS und VSync bleiben im selben Store. Qualitäts-Presets behalten alle fünf Anzeigewerte; Anzeigeänderungen verändern kein Qualitäts-Preset.
- `config/graphics.cfg` speichert additiv `RESOLUTION_WIDTH`, `RESOLUTION_HEIGHT`, `DISPLAY_MODE` (0 Fenster, 1 Randlos, 2 für künftig unterstütztes Exclusive), `FRAME_RATE_LIMIT` und `VSYNC`. Formatversion 1 und der vorhandene atomare Schreibpfad bleiben erhalten. Zukünftige unbekannte Config-Versionen werden nicht überschrieben.
- Alte Configs ohne Anzeigefelder migrieren die bisherige Fenstergröße. Ein alter Vollbildwunsch wird als Randlos auf dem aktuellen Desktop umgesetzt. Die alten Display-Felder in `metin2.cfg` dienen nur der Migration; aktuelle Abfragen lesen die zentralen GraphicsSettings.
- Die Auflösungsauswahl verwendet `MonitorFromWindow`, `GetMonitorInfoW` und `EnumDisplaySettingsW` für den Monitor des Fensters. Gleiche Breite/Höhe mit unterschiedlichen Bildfrequenzen erscheinen einmal. Fenster-Modi werden auf die bestehende UI-Mindestgröße 800 × 600 und den verfügbaren Arbeitsbereich einschließlich Fensterrahmen begrenzt. Es gibt keine fest codierte Auflösungsliste. Falls der Monitor keinen passenden Modus liefert, wird genau eine sichere Fenstergröße aus seinem Arbeitsbereich abgeleitet.
- Randlos belegt den aktuellen Monitor ohne Rahmen und ohne Taskleistenabzug; es verwendet die Desktop-Auflösung. Die Auflösungsauswahl ist in diesem Modus deaktiviert und zeigt die tatsächliche Desktop-Größe.
- **Exclusive Fullscreen wird nicht angeboten.** Die bestehende Präsentation verwendet ein Child-HWND und Diligents D3D11-SwapChain mit `DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT`. Der gepinnte Diligent-Code dokumentiert die fehlende D3D11-Fullscreen-Unterstützung dieses Flags (`build-deps/DiligentCore/Graphics/GraphicsEngineD3DBase/include/SwapChainD3DBase.hpp`). Native API-Anfragen für Exclusive werden abgelehnt; gespeichertes Exclusive fällt beim Start auf Fenster zurück.

## Live Apply und Sicherheit

Die UI stellt Anfragen an `CPythonSystem`. Erst am folgenden nativen Frame-Anfang werden Fensterstil/Geometrie geändert und `ITerrainPresentation::Resize` sowie `ResizeBackBuffer` ausgeführt. Synchrone `WM_SIZE`-Nachrichten während des Stilwechsels werden zusammengefasst. Device, Renderer, Meshes und Materialressourcen bleiben bestehen; der vorhandene Resize-Pfad erneuert die größenabhängigen SwapChain-/Modern-Ziele. Bildschirmgröße, Projektion, Mausskalierung und das vorhandene HUD werden angepasst.

Eine native Transaktion hält einen Snapshot des vorherigen zentralen Settings-Werts und der Fensterposition. Nach dem Wechsel fragt der bestehende QuestionDialog: **„Diese Anzeigeeinstellungen beibehalten?“** Ein monotones natives 15-Sekunden-Limit läuft auch ohne sichtbares Grafikmenü. Bestätigen speichert atomar und beendet die Transaktion erst nach erfolgreichem Speichern. Zurück, Escape, Schließen oder Timeout stellen die vorherigen Display-Werte wieder her. Parallel geänderte Qualität/FPS/VSync bleiben erhalten. Weitere Display-Wechsel während einer laufenden Vorschau werden abgelehnt.

Speichern und Shutdown verwenden bei einer unbestätigten Vorschau immer die vorherigen Display-Werte. Ein Prozessabbruch kann dadurch keine unbestätigte Auflösung in die Config übernehmen. Fehler beim Anwenden lösen sofortige Rückkehr aus. Ist der frühere Monitorzustand inzwischen ungültig, wird ein sicherer Fenstermodus gewählt. Scheitert auch dieser native Resize, wird mit einer Fehlermeldung beendet, statt einen erfolgreichen Wechsel vorzutäuschen.

Beim Start werden gespeicherte Werte erneut gegen den aktuellen Monitor geprüft. Ungültige Werte, nicht mehr vorhandene Auflösungen und nicht unterstützte Modi fallen auf eine aktuelle passende Fensterkonfiguration zurück. Es wird keine Desktop-Auflösung per `ChangeDisplaySettings` verändert; der alte Exclusive-Alt-Tab-Pfad bleibt für diese Modi ausgeschaltet.

## Validierung

Die gezielten Tests und nativen Nachweise liegen unter `build-p3-display/`. Reproduzierbare Tests: `tests/Graphics/GraphicsSettingsTest.cpp`, `DisplayConfigurationTest.cpp`, `p3_display_entry.py`.

Der native Test prüft tatsächliche HWND-Clientgröße und Fensterstil gegen Store, Runtime-Snapshot und UI-Auflösung; er benutzt die vorhandenen Grafikdialog-Callbacks. Die Matrix umfasst Fenster → Randlos → Fenster, mehrere enumerierte Größen, Bestätigen, Abbrechen, Schließen während einer Vorschau, Timeout bei verborgenem Grafikdialog, Schutz vor dem Speichern unbestätigter Werte und alle sechs FPS/VSync-Kombinationen. Ein separater Prozess prüft den Neustart. Weitere Starts prüfen ungültige gespeicherte Auflösungen und Modi.

Finaler Release-Build: **PASS**. Die erste Sandbox-Konfiguration konnte Windows-SDK-Header nicht sehen; der Build mit Zugriff auf die installierte Visual-Studio-x64-/SDK-Umgebung bestand. Gezieltes CTest-Gate: **10/10 PASS** (FramePacing, Presets, Custom, Invalid, LiveApply, Display, Persistence, WriteRestart, ReadRestart, DisplayConfiguration). Keine breite Suite und kein Debug-/Android-Build.

| Nativer Lauf | Ergebnis |
| --- | --- |
| `final-world` | PASS, 31,76 s. Modern-A1-Welt, Charakter und vorhandenes HUD; Fenster ↔ Randlos; 800 × 600, 1280 × 800 und 1600 × 900. 1589 Modern-Frames; Schatten/AO/HDR und Größenänderungen aktiv. |
| Bestätigung / explizites Abbrechen / Timeout ohne sichtbares Grafikmenü / Schließen | PASS; HWND-Größe/-Stil, UI-Größe, Settings und Runtime stimmen nach den Wechseln überein. Die zwei Textzeilen wurden bei 800 × 600 visuell geprüft. |
| FPS/VSync nach den Wechseln | PASS; alle sechs Kombinationen im zentralen Runtime-Snapshot und im tatsächlichen Present-Capture vorhanden. Kein neuer FPS-Leistungsbenchmark. |
| `restart-window` | PASS; gespeicherte 1600 × 900, Fenster, 120 FPS und VSync Aus in neuem Prozess. Die beim vorherigen Prozessende unbestätigte Randlos-Vorschau wurde nicht gespeichert. |
| `persist-borderless` / `restart-borderless` | PASS; bestätigte 1920 × 1080/Randlos in neuem Prozess, alle Settings identisch. Scrollbare dynamische Modusliste visuell geprüft. |
| `fallback-resolution` | PASS; gespeicherte 1234 × 777 wird auf aktuell unterstütztes Fenster zurückgesetzt. |
| `fallback-exclusive` | PASS; gespeicherter Modus 2 fällt auf Fenster zurück. |
| `fallback-malformed` | PASS; übergroße, negative und nicht numerische Werte fallen sicher zurück. |
| Fehler und Shutdown | Alle sieben finalen Prozesse Exit 0, leeres Python-Fehlerlog, Diligent ERROR/FATAL 0/0. Modern-/Water-Renderer sowie überwachte Settings-/Asset-/GPU-Ressourcen nach Shutdown 0. |
| Produktionsdateien | SHA256-Vergleich für Release/Debug, Root-Pack und beide Configs: unverändert. Vorherige Nutzeränderungen bleiben erhalten. |

Alle sieben finalen Läufe verwenden denselben Release-Hash; Nachweis: `build-p3-display/summary.json`, `protected-before.json`, `tests.log`, `build-release.log` sowie die einzelnen Laufordner mit `display-run.jsonl`, Screenshots, `frame-pacing.csv`, `source-resource-audit.log`, `launch.json` und `exit.json`. Der erste reine UI-Pilot liegt separat unter `matrix`; seine Ergebnisse werden nicht als Welt-Test ausgegeben. Der vorhandene Renderer-Resize-Pfad wurde wiederverwendet; der Welt-Lauf erzeugte nur einen Lighting-Buffer und behielt die bestehenden PSO-Varianten.

**Stand des obigen technischen Gates:** echte manuelle Alt-Tab-/Mehrmonitor-Abnahme sowie Abnahme im normalen Produktionsclient waren noch nicht ausgeführt. Die native Umschaltung verwendet keinen Exclusive-Aktivierungszweig; dies ersetzt keine manuelle Alt-Tab-Prüfung. Exclusive-Wechsel sind wegen fehlender Backend-Unterstützung nicht anwendbar. Kein vollständiger Netzwerk-/Relog-Test.

**Freigabe zur Finalisierung, 18.09.2026:** Der Nutzer hat die manuelle Sicht-/Alt-Tab-Abnahme ausdrücklich mit PASS bestätigt und separate Display-Commits, die Integration nach main sowie einen frischen Release und Production-Tests autorisiert. Die neue Produktionsabnahme wird mit den exakten Main-Commits und Deployment-Hashes in `m2dev-client/docs/production-milestone-deployment.md` dokumentiert. Die Benutzerconfigs bleiben lokale Laufzeitwerte; fehlende FPS-/VSync-Schlüssel verwenden die bestehenden Defaults (60/Ein) und werden beim Speichern ergänzt. Keine neuen globalen Config-Defaults erforderlich. Kein Push.
