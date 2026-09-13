# Milestone 12 – Diligent als Produktions-Default

Stand 13.09.2026, Basis `d2a5a55` (M11). **Abgeschlossen und im dokumentierten M12-Umfang freigegeben.**
Nur Startauswahl/Fehlerweitergabe, Tests und Dokumentation. Kein M13 und kein D3D9-Removal.
Die [Analyse vor Änderungen](renderer-milestone12-analysis.md) enthält die acht Ausgangsfragen.

Der Benutzer hat die zuvor noch offene normale argumentlose Diligent-Prüfung ausdrücklich
bestätigt: „ich bestätige es es passt“. Diese Rückmeldung ist die zusammenfassende
Ingame-Abnahme; sie ist kein gesonderter technischer Nachweis jedes optionalen Sitzungswechsels.
Die zunächst offenen negativen EXE-Starttests wurden nach erneuter M12-Beauftragung
vollständig nachgeholt: ungültig/widersprüchlich/OFF+nicht verfügbares Diligent jeweils
Exit 2; vorhandene Diligent-Fenstervoraussetzung verletzt, sowohl Default als auch explizit,
jeweils Exit 4 trotz abgefangener Python-Ausnahme. Eindeutige Fehlerlogs, kein Legacy-Fallback.
Die zwei früheren Abbrüche an der Windows-Startfreigabe waren vor Prozessstart, keine
Rendererfehler; die erfolgreichen Wiederholungen ersetzen diese fehlenden Nachweise.

## Aktuell gültiger Startvertrag

| Build | Start | Backend |
|---|---|---|
| Diligent ON | `Metin2_Release.exe` | Diligent D3D11 |
| Diligent ON | `Metin2_Release.exe --renderer=diligent-d3d11` | identischer Diligent-Pfad |
| Diligent ON | `Metin2_Release.exe --renderer=legacy-d3d9` | expliziter Legacy D3D9Ex-Fallback |
| Diligent OFF | `Metin2_Release.exe` | Legacy D3D9Ex (Compile-time-Default) |
| Diligent OFF | `Metin2_Release.exe --renderer=legacy-d3d9` | Legacy D3D9Ex |
| Diligent OFF | `Metin2_Release.exe --renderer=diligent-d3d11` | klare Fehlermeldung, Exit 2 |

Kein automatischer Backendwechsel, kein Hot-Switching. Diligent setzt weiterhin `WINDOWED 1`
voraus; bei anderer Konfiguration explizit Legacy wählen oder bewusst Fensterbetrieb einstellen.
M12 verändert die gespeicherte Fensterkonfiguration nicht.

```powershell
# Produktion / bestehendes OFF-Cache ausdrücklich auf ON umstellen:
cmake -S . -B build -DM2_ENABLE_DILIGENT_D3D11=ON
cmake --build build --config Release --parallel 4

# Bewusst Legacy-only, ohne Diligent-Download/-Link:
cmake -S . -B build -DM2_ENABLE_DILIGENT_D3D11=OFF
cmake --build build --config Release --parallel 4
```

Frische CMake-Konfigurationen verwenden jetzt ON; ein vorhandenes explizites OFF wird nicht
überschrieben. Die erste ON-Konfiguration benötigt die schon dokumentierte gepinnte
DiligentCore-Abhängigkeit (Netzwerk oder `FETCHCONTENT_SOURCE_DIR_DILIGENTCORE`).
`--renderer-smoke-test` bleibt der isolierte fünf-Frame-EXE-Test, nicht der normale Spielstart.

## 1. Vorherige Backend-Auswahl

StartupOptions und das unbenutzte Defaultargument von CPythonApplication nannten Legacy.
WinMain parste einmal; normales Spiel übergab den gewählten Enum, Diligent war explizit opt-in.
CMake hatte OFF als Default. Kein Renderer-Config-/Environment-Override vorhanden.

## 2. Neue Backend-Auswahl

`WinMain` ermittelt die tatsächliche einkompilierte Availability durch
`IsDiligentTerrainAvailable()` und erstellt damit `StartupOptions`.
ON → Diligent, OFF → Legacy. Die Availability ist eine Build-Eigenschaft, kein Ergebnis
eines Hardware-Initversuchs. Hardwarefehler können daher keinen Default-Fallback auslösen.
CPythonApplication verlangt jetzt den ausdrücklich übergebenen Backendparameter;
`m_startupBackend` bleibt konstant und steuert unverändert denselben M11-Renderaufbau.

## 3. Command-Line-Werte

`--renderer=diligent-d3d11` und `--renderer=legacy-d3d9` unverändert, schreibungsgenau mit `=`.
Identische Wiederholung erlaubt; widersprüchliche oder unbekannte/leer angegebene Werte
bleiben dauerhaft ungültig. Fremde Clientoptionen bleiben unangetastet.
Der Default und sein explizites Gegenstück unterscheiden sich nur in der protokollierten
Auswahlquelle, nicht in Rendering-/Material-/Kompositionsflags.

## 4. Config-Verhalten

Kein neues Configsystem und keine Rendererwahl in bestehenden Dateien eingeführt.
`M2_RENDERER_AUDIT` / `config/renderer-audit.enabled` sind weiterhin nur optionale Diagnose.
Fenster-/Schatten-/Audio-/Servereinstellungen des normalen Nutzerruntimes wurden nicht ersetzt.

## 5. ON-Build-Verhalten

Default Diligent, beide ausdrücklichen Backendwerte verfügbar.
`renderer-startup.log` zeigt einmal `Renderer: Diligent D3D11` bzw. `Renderer: Legacy D3D9Ex`,
danach `Selection=default|explicit DiligentCompiled=1` und beim normalen Ende den Exitcode.
Bestehende Diligent- und Legacy-Implementierungen werden unverändert gewählt.

## 6. OFF-Build-Verhalten

Default Legacy, explizites Legacy ebenfalls funktionsfähig; `DiligentCompiled=0`.
Explizites Diligent endet vor Config/Python/Geräteerzeugung mit Fehlermeldung/Exit 2.
Dies ist kein stiller Fallback: kein Argument bedeutet hier bewusst den compile-time verfügbaren Default.

## 7. Init-Fehler-Verhalten

Kein Retry mit anderem Backend. Ungültige/widersprüchliche Auswahl oder nicht einkompiliertes
Diligent: Exit 2 und eindeutiges Startlog/MessageBox.
Renderer-Geräte-/Presentation-/Fenstervoraussetzungsfehler: gelatchter Fehler in
CPythonApplication, ursprüngliche Python-RuntimeError-Meldung, normales Teardown, Exit 4.
Weitere von Main erkannte Startfehler erhalten Exit 3 statt pauschal 0.
Die frühere Lücke war real: `prototype.py` fängt RuntimeError ab, Main/WinMain ignorierten
den Fehler und gaben 0 zurück. Der neue Fehlerzustand überlebt dieses Abfangen.
Die vorhandene Windowed-Voraussetzung wird vor Fenster-/Fullscreen-/Geräteänderungen geprüft.
Der Test `startup_failure_entry.py` fängt genau diesen vorhandenen Fehler absichtlich ab;
er enthält keine produktive Fehler-Injection und erzeugt keinen Legacy-Fullscreen-Fallback.
Am echten Client bestanden: Default PID62460 sowie explizit Diligent PID63296, beide
Exit 4 nach 0,1 s. `expected-startup-failure.log` bestätigt das Abfangen durch Python;
`renderer-startup.log` bestätigt weiterhin Diligent, den Initfehler und ausdrücklich keinen
Fallback. Der vorhandene Fensterbetriebsfehler wird vor Fenster-/Geräteerzeugung ausgelöst.
Ungültige Auswahl PID65392, widersprüchliche Auswahl PID64408 und OFF+explizit Diligent
PID65020 enden jeweils mit erwartetem Exit 2 und passendem Fehlerlog, vor dem normalen Spielstart.
Ungültige Initialisierungsparameter und doppelte Initialisierung werden zusätzlich am
realen Backend geprüft. Ein physischer Treiberausfall oder eine tatsächlich nicht
D3D11-fähige Grafikkarte wurde nicht erzwungen/simuliert.

## 8. Default-Start

Echte EXE `startup-on-default`: Default → Diligent, Initialize/WM_SIZE/Shutdown erfolgreich,
PID63256 Exit 0. Normalclient `normal-on/default` PID53064 und Weltfixture `world-on/default`
PID65804 starten ebenfalls ohne Rendererargument; Log bestätigt Default Diligent.
Normalclient: Exit 0 nach 344,9 s; Weltfixture: alle sechs Phasen, Exit 0 nach 151,1 s.
Die visuelle Benutzerabnahme des normalen Defaultclients ist mit „ich bestätige es es passt“ erfolgt.

## 9. Expliziter Diligent-Start

Echte EXE `startup-on-explicit`: PID60080, Initialize/WM_SIZE/Shutdown, Exit 0.
Dieselbe private Weltfolge mit explizitem Argument: PID15548, alle sechs Phasen,
Exit 0 nach 151,1 s. Identische Konfigurationsquelle
für Default und Diligent, unveränderte Pakete/Geometrie/Shader. Kein byteidentischer
Zeit-/Animations-/Screenshotverlauf verlangt oder behauptet.

## 10. Expliziter Legacy-Start

Echte EXE `startup-on-legacy`: PID66724, Initialize/WM_SIZE/Shutdown, Exit 0.
Die vier bestehenden Teststarter fordern Legacy jetzt ausdrücklich an; ihre alte
Argumentlosigkeit würde sonst versehentlich Diligent prüfen. Normaler expliziter Legacyclient
PID54980: Benutzer bestätigt den angefragten Ingame-Vergleich mit „passt“; Exit 0 nach 47,1 s.
Die Antwort ist eine zusammenfassende Sichtprüfung, kein separat protokollierter Nachweis
jedes optional angefragten Teleports/Relogs.

## 11. Login

Neue normale Default-Sitzung mit Originalpaketen durchgeführt; Benutzer meldete sich selbst an.
Keine Zugangsdaten gelesen/eingegeben, keine Konten-/Serveränderungen durch Testskripte.
Der normale Defaultclient war ingame (Actor-/Schadenseffekt-Telemetrie); die anschließende
ausdrückliche Benutzerbestätigung schließt die visuelle Abnahme. Grundlage ist diese neue
Bestätigung, nicht das frühere „passt“ auf die reine Startfreigabe-Frage.

## 12. Character Select

Normale Auswahl samt 3D-Figur, UI/Text und Auswahlwechsel war Teil der jetzt bestätigten Default-Prüfung.
Bestehender argumentloser Vorschautest PID58168: alle acht Races/16 Phasen und ursprüngliche
Questtext-Seiten 0/8/16 erfolgreich, sämtliche 16 JPEG-Aufnahmen erfolgreich, Exit 0 nach
161,1 s. Figuren/Materialien/Animation und Questtext in sichtbaren Phasen kontrolliert.
Keine neue Previewengine. Der Titel enthält historisch weiterhin „milestone 11“.

## 13. Ingame

Default-Testauftrag umfasst Terrain/Gebäude/Bäume, Player/NPCs/Mobs, Rüstung/Waffe/Haare/Mount,
Effekte/Skills/Schadenszahlen, Wasser/Himmel, Bodenitems, UI/Chat/Nameplates und Questdialoge.
Der Benutzer bestätigt die Default-Prüfung insgesamt; keine nicht erreichbaren Punkte wurden
genannt. Keine Einzelbeobachtungen oder M11-Antworten als neue technische Messungen ausgegeben.

## 14. Mapwechsel

Originale A1 → B1 → A1-Folge in beiden Diligent-Startvarianten, plus Dungeon/Guild/A1
als bestehender unveränderter M11-Sonderpfadtest. Normaler Server-Mapwechsel war separat
angefragt und gehört zur Benutzer-Gesamtabnahme; kein einzelner Server-Teleport protokolliert.
Offline-Mapload und echter Server-Teleport sind getrennte Nachweise.
OFF-Default PID49816 hat dieselbe vollständige Sechs-Phasen-Folge ohne Rendererargument
mit Exit 0 nach 150,8 s durchlaufen. Sein ursprünglicher Legacy-Screenshotpfad meldet
wie bereits in M11 `LockRect 0x8876086c` / Aufnahmeergebnis 0; keine OFF-JPEG-Belege
behauptet und keine Screenshotmodernisierung vorgenommen.

## 15. Relog

Ingame → Character Select → Ingame im neuen Default angefragt, sofern reproduzierbar.
Die zusammenfassende Benutzerabnahme liegt vor; keine gesonderte Relog-Messung behauptet.
Keine Netzwerkinstrumentierung oder künstliche Serversitzung dafür eingeführt.

## 16. Resize

Echte EXE-Bootstraps mit WM_SIZE erfolgreich; Backendtests prüfen mehrere Größen und Readback.
Mehrfaches Vergrößern/Verkleinern im normalen Default war zusätzlich Teil des bestätigten Testauftrags.
Der Versuch, den Rand des isolierten Vorschaufensters zu ziehen, änderte seine Größe
nicht; das zählt nicht als erfolgreicher interaktiver Resize-Nachweis.

## 17. Minimize/Restore

Dreimal im normalen Default angefragt und vom Benutzer zusammenfassend bestätigt.
Im isolierten Default-Vorschaufenster wurden drei
Minimize-/Restore-Zyklen über die Fensteroberfläche ausgeführt; Figuren/Questtext danach
korrekt und weiter animiert. Der Benutzer bestätigte den zusammenfassenden Legacy-Auftrag.
Keine frühere M11-Bestätigung als neue M12-Bestätigung ausgegeben.

## 18. Shutdown

Alle drei ON-EXE-Bootstraps, beide Diligent-Weltläufe, der normale Defaultclient und der
normale explizite Legacyclient sowie die Default-Vorschau: Exit 0. OFF-EXE-Bootstraps Default PID45612 und explizites
Legacy PID13228 ebenfalls Exit 0 einschließlich Initialize/WM_SIZE/Shutdown.
Alle fünf negativen Starttests haben den erwarteten Exit 2 oder 4, nicht 0. Diese
kontrollierten Fehlercodes sind bestandene Negativtests, keine normalen Clientabstürze.

## 19. Offene Ressourcen

M11-Ownership/Shutdown unverändert. Überwachte Text/UI/World/Effekt/Tree/Actor/Attachment/
Mount/Objektzähler wurden im normalen Defaultclient sowie beiden Diligent-Weltläufen geprüft:
sämtliche Shutdown-Zähler 0. Gleiches Ergebnis in der abgeschlossenen Default-Vorschau.
Ressourcenfreiheit bezieht sich auf die expliziten Besitzerzähler, nicht auf beliebige OS-Caches.
Private Prozessspeicher-Spitze/letzte Messung vor Exit: normaler Default 650,0/645,9 MiB;
Welt Default 535,9/478,9 MiB; Welt explizit 528,1/479,8 MiB. Unterschiedliche geladene Maps
und Assetcaches machen Start-/Endwerte nicht zu einem Leaktest. Die Weltwiederholungen
zeigen keinen monoton steigenden Verbrauch; diese kurzen Läufe ersetzen keinen Dauertest.

## 20. Native D3D9-Drawcalls im Defaultpfad

Private Default-Fixtures aktivieren die vorhandene optionale M11-Telemetrie.
Tatsächliche Draws/States/Texture-Binds/Targets getrennt von unterdrückten Versuchen;
Start-/Reset-/Asseterzeugung nicht als per-frame Renderzählung missverstehen.
Normaler Default: 172 Messpunkte, Welt Default: 75, Welt explizit Diligent: 74.
Default-Vorschau: weitere 80 Messpunkte.
In jedem Messpunkt tatsächliche `draws=0 states=0 texture_binds=0 target_changes=0`.
Unterdrückte Kompatibilitätsversuche bleiben getrennt sichtbar und sind keine nativen Draws.
Belege: jeweilige `native-render-audit.log`, nicht allein die Auswahlmeldung.

## 21. Build-Ergebnisse

Release ON `build/milestone12/release-on.log`: erfolgreich, Exit 0.
Release OFF `build/milestone12/release-off.log`: erfolgreich, Exit 0.
Abschließend wiederhergestellter ON-Build `release-on-final.log`: erfolgreich, Exit 0;
`build/CMakeCache.txt` bestätigt `M2_ENABLE_DILIGENT_D3D11:BOOL=ON`.
Vorhandene Python/zlib-PDB-Linkwarnungen unverändert. Kein Build-/Testsystem deaktiviert.

## 22. Testsuite

Vollständige ON-Suite: 17/17, Exit 0, 449,29 s; darin alle 13 Renderer-Tests bestanden.
Defaulttest prüft beide Availability-Werte, Gleichheit Default/Explizit, explizites
Legacy, Smoke-Flag sowie wiederholte/ungültige/widersprüchliche Optionen.
Vollständige OFF-Suite: 15/15, Exit 0, 447,69 s; darin alle 11 Renderer-Tests bestanden.
Sie lief erst nach abgeschlossenem ON-Lauf, niemals paralleles Reconfigure im selben Buildverzeichnis.
EXE-Startup-Matrix und private Szenen sind zusätzlich zur CTest-Suite dokumentiert.
Alle sechs betroffenen PowerShell-Teststarter wurden zusätzlich ohne Parserfehler geprüft.
Nach dem abschließenden ON-Rebuild nochmals Renderer ON: 13/13, Exit 0, 15,71 s
(`renderer-on-final.log`). Kein C++-Produktionscode wurde zwischen den vollständigen
Suiten und diesem abschließenden Build geändert.
Beim nachgeholten Abschluss wurden ausschließlich die fünf negativen EXE-Startfälle
ausgeführt und dieser Bericht ergänzt. Kein weiterer C++-/Build-/Teststarter-Fix war nötig;
die vorhandenen vollständigen ON/OFF-Suiten und der abschließende ON-Build bleiben gültig.

## 23. Legacy Regression

Expliziter Fallback-Bootstrap bestanden; neue normale Ingame-Sitzung PID54980 vom Benutzer
bestätigt und mit Exit 0 beendet. OFF-Default-/Legacy-Bootstraps ebenfalls bestanden;
automatische OFF-Weltfolge und vollständige OFF-Suite ebenfalls bestanden.
Legacy-Renderer, D3D9-Draws/States, Schatten und Ressourcenpfade nicht modernisiert/entfernt.

## 24. Dokumentation

README beschreibt den aktuellen Produktionsstart und ON/OFF-Cacheunterschied.
CMake-Beschreibung nennt Diligent als Produktions-Default. 33 historische Milestone-
Dokumente erhalten jeweils einen kurzen aktuellen Verweis statt rückwirkend geänderter Testergebnisse.
Keine Dokumentationsneustrukturierung. M11-CSV bleibt historischer Source-Audit seines Commits.

## 25. Verbleibende D3D9-Ressourcenabhängigkeiten

Native D3D9Ex-Geräteerzeugung, Texturtypen/-decoder, Buffer/CPU-Skinning-Uploads,
StateManager/Kompatibilitätssicht, D3DX/SDK-Libraries bleiben bewusst vorhanden.
In Diligent erzeugt der alte Devicepfad Ressourcen; das ist keine zweite sichtbare Legacy-
Komposition und kein automatischer Fallback. Dynamische Diligent-Schatten weiter deaktiviert,
Legacy-Schatten unverändert. Kein Vollbild-/Treiber-/Schattenredesign als Beifang.

## 26. Empfehlung für M13 / STOP

M12 ist im dokumentierten Fensterbetriebs-/Testszenen-Umfang freigegeben. Diligent ist
Produktions-Default des ON-Builds, explizites Legacy bleibt funktionsfähig; kein stiller
Fallback und keine gemessenen nativen D3D9-Drawcalls in den dokumentierten Diligent-Szenen.
M13 nur separat beauftragen. Vor tatsächlichem Removal
native Asset-/Buffer-/Device-Verträge vollständig auflösen und außerhalb des normalen
Spielflusses verbliebene Helfer/Tools bewerten. Nicht lediglich D3D9-Libraries löschen,
solange Loader/CPU-Skinning diese noch verwenden. Kein M13-Code in dieser Änderung.

## Git-Diff und Belege

Produktionsänderungen: CMake-Default, StartupOptions, WinMain-Logging/Ergebnisweitergabe,
verpflichtender Backendparameter und gelatchter Initfehler/Frühprüfung in PythonApplication.
Keine Änderungen an Renderer-Draws, StateManager, Granny, Effekten, UI oder Schatten.
Teständerungen: Parsermatrix, explizites Legacy in vier Startern, getrenntes Defaultfixture,
realer EXE-Startup-Runner und erwarteter Initfehler ohne Desktop-/Kontenänderung.
Alle Belege unter `build/milestone12/`; Tests verwenden private EXE-/Config-/Paketverzeichnisse.
Keine normale Runtime-EXE ersetzt, kein Commit/Push. Abschließendes `git diff --check` sauber.
46 vorhandene Dateien geändert, fünf neue Dateien; davon 33 historische Dokumente mit
jeweils nur einem kurzen aktuellen Hinweis. Tracked-Diff: 186 Ergänzungen / 41 Entfernungen;
die fünf neuen Analyse-/Berichts-/Testdateien kommen hinzu. Produktion betrifft sechs Dateien
einschließlich beider CMake-Dateien; keine Renderimplementierung verändert.
Die Runtime enthält weiterhin ausschließlich ihre bereits vorgefundenen Änderungen an
`config/channel.inf`; keine davon wurde zurückgesetzt. Alle gestarteten M12-Clientprozesse
sind beendet, kein fremder Spielclient wurde beendet.

## Gemessene Start-/Exit-Matrix

| Lauf unter `build/milestone12/` | PID | Ergebnis |
|---|---:|---|
| `startup-on-default` | 63256 | Default Diligent, Bootstrap/Resize/Shutdown, Exit 0 |
| `startup-on-explicit` | 60080 | explizit Diligent, Bootstrap/Resize/Shutdown, Exit 0 |
| `startup-on-legacy` | 66724 | explizit Legacy, Bootstrap/Resize/Shutdown, Exit 0 |
| `startup-off-default` | 45612 | Default Legacy, Bootstrap/Resize/Shutdown, Exit 0 |
| `startup-off-legacy` | 13228 | explizit Legacy, Bootstrap/Resize/Shutdown, Exit 0 |
| `normal-on/default` | 53064 | Originalpakete, Ingame-Telemetrie, Exit 0; Benutzerabnahme ausdrücklich bestätigt |
| `normal-on/legacy` | 54980 | Originalpakete, Benutzer „passt“, Exit 0 |
| `world-on/default` | 65804 | alle sechs Phasen, Exit 0 |
| `world-on/diligent` | 15548 | alle sechs Phasen, Exit 0 |
| `world-off/default` | 49816 | alle sechs Phasen, Exit 0; alter Screenshot-Readback fehlschlägt |
| `preview-on/default` | 58168 | alle 16 Phasen, Exit 0 |
| `failure-on/default` | 62460 | Initfehler trotz Python-Catch weitergegeben, kein Fallback, erwarteter Exit 4 |
| `failure-on/diligent` | 63296 | gleicher Initfehler explizit, kein Fallback, erwarteter Exit 4 |
| `startup-on-invalid` | 65392 | ungültiger Rendererwert, erwarteter Exit 2, PASS |
| `startup-on-conflict` | 64408 | widersprüchliche Rendererwerte, erwarteter Exit 2, PASS |
| `startup-off-unavailable` | 65020 | explizit Diligent nicht einkompiliert, kein Fallback, erwarteter Exit 2, PASS |

Unbekannter/widersprüchlicher Wert ist sowohl in der Parsermatrix als auch durch echte
EXE-Läufe geprüft. Die drei Exit-2-Teststarter prüfen den Prozess-Exitcode und das
Startlog; bei ungültig/widersprüchlich darf nicht einmal eine Backendwahl protokolliert sein.
Die zwei Exit-4-Läufe wurden separat anhand Prozess-Exit, Startlog und Python-Catch-Beleg
verifiziert. Kein erzwungener Treiberausfall; dieser Test weist den realen Fehlerweitergabe-
und No-Fallback-Vertrag nach, nicht jede mögliche Hardware-Fehlerursache.
Ein anfänglicher abgebrochener Preview-Start ist durch den vollständigen Wiederholungslauf
ersetzt. Keine abgebrochene Windows-Startfreigabe wird als Client-Exitcode ausgegeben.
