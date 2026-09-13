# M12 – Analyse vor Änderungen

Basis `d2a5a55`, sauberer Source-Checkout. Ausschließlich Produktionsauswahl und Validierung.
M11-Renderpfade, dynamische Schatten aus, native D3D9-Ressourcen bleiben unverändert.

1. **Bisheriger Default:** `Renderer/StartupOptions.h::backend=LegacyD3D9`; `WinMain`
   erstellt Optionen einmal und übergibt den Enum über `Main` an `CPythonApplication`.
   Auch deren bisher unbenutztes Konstruktor-Defaultargument nennt Legacy.
2. **CLI:** `--renderer=diligent-d3d11`, `--renderer=legacy-d3d9`, zusätzlich
   `--renderer-smoke-test` für den isolierten Bootstrap. Fremde Optionen bleiben erhalten.
   Identische Wiederholung erlaubt, widersprüchliche Auswahl dauerhaft ungültig.
3. **Config/Environment:** keine Rendererwahl vorhanden. `WINDOWED` beeinflusst den
   unterstützten Fenstermodus. `M2_RENDERER_AUDIT` bzw. `renderer-audit.enabled` aktivieren
   nur Diagnose, keine Backendwahl. Kein neues Configsystem nötig.
4. **Unbekannt/leer:** MessageBox und Exit 2 in WinMain, kein Fallback.
5. **Initfehler:** OFF + explizit Diligent bereits Exit 2. Presentation-/Fenstermodusfehler
   erzeugen Python RuntimeError ohne Backendwechsel. Aber originale `prototype.py` fängt
   den Fehler ab; `Main`/`WinMain` geben danach pauschal 0 zurück. M12 muss diesen
   Renderer-Startfehler bis zum Prozess-Exit erhalten. Der Bootstrap hat schon Fehlercodes.
6. **Native Annahmen:** Auswahl vor Konfiguration, Python und Gerätestart. Danach erzeugt
   `CreateDevice` weiterhin absichtlich das native D3D9Ex-Asset-/Buffergerät, auch für
   Diligent. Das ist keine Legacy-Fallbackwahl und wird nicht entfernt. Diligent unterstützt
   derzeit nur Fensterbetrieb; diese bestehende Voraussetzung wird früh geprüft, bevor
   eine nicht unterstützte Fullscreen-Konfiguration den Desktopmodus verändern kann.
7. **Tests:** `Renderer.StartupOptions` erwartet derzeit immer Legacy. Vier Teststarter
   lassen für `Backend=legacy` das Argument weg: run_effects, run_mounts, run_trees,
   run_actor_regression. Diese müssen Legacy explizit anfordern, sonst wären Vergleiche falsch.
8. **Dokumentation:** README/CMake-Beschreibung und M1-Startanleitung sind Einstiegspunkte.
   M1–M11-Berichte enthalten historische Default-/Opt-in-Aussagen. Diese werden als historische
   Belege beibehalten, aber mit sichtbarem Verweis auf die aktuelle M12-Auswahl versehen.

## Minimale Änderungspunkte

- `CMakeLists.txt`: frische Konfiguration standardmäßig Diligent ON. Vorhandenes OFF-Cache
  bleibt eine explizite Legacy-only-Konfiguration, nicht still auf ON überschreiben.
- `StartupOptions`: Default anhand der tatsächlich einkompilierten Availability; WinMain
  verwendet `IsDiligentTerrainAvailable()`. Kein neues globales Feature-/Configsystem.
- `UserInterface.cpp`: einmaliges lesbares Startlog, Auswahlquelle, Initfehler-/Exitweitergabe.
- `PythonApplication`: gewählter Backendparameter verpflichtend, gelatchter Renderer-
  Startfehler, bestehende Fenstervoraussetzung früh und mit verständlicher Meldung prüfen.
- Auswahltests prüfen ON/OFF-Defaults, Gleichheit Default/Explizit, Wiederholungen und Fehler.
  Teststarter erhalten explizites Legacy sowie einen separat benannten argumentlosen Lauf.
- Private originale Auswahl-/Welt-/Normalclient-Fixtures, keine Produktivdateien ersetzen.

Validierung: beide Release-Konfigurationen/vollständige Suiten; tatsächliche EXE-Startmatrix;
Default und explizit Diligent gleicher Welt-/UI-/Shutdownpfad; Legacy explizit; erwartete
Fehlerfälle ohne Fallback; Ingame-Benutzerprüfung, A1/B1/A1, Fensterzustände, Ressourcen/Telemetrie.
Keine M13-Arbeit, kein Commit/Push oder D3D9-Removal.
