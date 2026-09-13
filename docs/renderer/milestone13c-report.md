# Milestone 13C – Abschlussbericht

Stand: 2026-09-13. Ausgangspunkt: `04017a9` (M13B), sauberer Source-Checkout.
M13C umfasst ausschließlich den finalen D3D9-Cleanup. Keine Phase-B-Funktionen,
keine Änderungen an Server, Originalpaketen, Granny-Runtime oder Skinning-Verfahren.

**Status: M13C und Phase A abgeschlossen.** Release/Debug, Renderer-Tests,
vollständige Testsuite und neue manuelle Welt-/Ingame-Abnahme erfolgreich.
Der normale Client benötigt keine D3D9-SDK/API-Abhängigkeit mehr. Keine Phase B.

## Architektur / Änderungen (Punkte 1–14)

| Nr. | Prüfpunkt | Ergebnis |
|---|---|---|
| 1 | D3D9 Includes | d3d9.h/d3d9types.h aus Projektquellen entfernt; tatsächliche Compiler-Abhängigkeiten separat geprüft. |
| 2 | D3DX Includes | d3dx9.h/d3dx9math.h und native Shader-/Texture-/Mesh-Helper nicht mehr eingebunden. |
| 3 | Native Typen | Device/Texture/Surface/VB/IB/Declaration/Shader/COM-Matrixstack-Typen und Felder entfernt; keine Dummy-COM-Typedefs. |
| 4 | Enums / Defines | `Renderer/DrawStateTypes.h`: benötigte CPU-Materialwerte, Textur-/Sampler-/Blend-/Depth-/Cull-Werte und Vertexlayout. 142 Werte gegen bisherige Definitionen geprüft. |
| 5 | Math | `Math/Math.h`: gepackte eigene Typen auf Windows-SDK DirectXMath, ohne zusätzliche Math-DLL. Row vectors, Translation Zeile 4, RH, Quaternionreihenfolge und unaligned Layout bleiben. |
| 6 | D3DX Helper | Tote GPU-Texture-/Surface-Erzeugung, Shaderassembler und Debugmesh-Erzeugung entfernt. Bestehende CPU-Asset-Pipeline mit DDS/STB weiterverwendet. |
| 7 | Libraries | d3d9.lib/d3dx9.lib nicht mehr gelinkt. `WindowsInput` enthält nur unabhängiges dinput8/dxguid. |
| 8 | SDK-Pfade | Kein DirectX9 SDK nötig. Gemeinsames extern/include bleibt für unabhängige mitgelieferte Abhängigkeiten; tatsächliche Includes enthalten kein D3D9/D3DX9. |
| 9 | Dateien / Klassen | Siehe Löschungen und Umbenennungen unten. Aktive CPU-Helfer erhalten. |
| 10 | StateManager | `CDrawState`, `DrawStateView`, `CaptureMaterialState`: ausschließlich CPU-Daten/Scopes/Owner. Native Draw-, Shader-, Stream-/Index-Fassaden und wirkungslose Aufrufe entfernt. |
| 11 | Flags | M2_ENABLE_DILIGENT_D3D11 und seine OFF-Äste entfernt. Diligent unbedingter Produktionspfad; M2_BUILD_RENDERER_TESTS steuert nur Tests. |
| 12 | CLI | Default und `--renderer=d3d11`; `--renderer=diligent-d3d11` bleibt Alias desselben Backends. Keine Legacy-Auswahl, kein Fallback/Hot-Switch. |
| 13 | Tests | Aktive CPU-/Diligent-/Lebensdauer-/Material-/UI-/Texttests erhalten, Math/Source/Import-Audits ergänzt; ungebautes natives Vergleichsprogramm begründet entfernt. |
| 14 | Dokumentation | Haupt-README und neue aktuelle Renderer-Anleitung; dieser Bericht, initiales Audit, vollständiges Trefferinventar. Alte Milestones ausdrücklich historisch. |

Konkrete Löschungen: `GrpD3DXBuffer`, `GrpVertexShader`, `GrpPixelShader`,
`GrpShadowTexture`, der nicht angebundene `SpeedGrassWrapper` (je .h/.cpp).
Der alte `DDSTextureLoader9` entfällt; sein benötigter CPU-DDS-Vertrag liegt jetzt in
`DDSImageData`. Unterstützte Texturformate, Kanalreihenfolge und Mipdaten bleiben;
ungültige/unvollständige Daten werden zurückgewiesen.

Umbenannte produktive Helfer: StateManager → DrawState, NativeStateView → DrawStateView,
NativeMaterialSnapshot → MaterialStateSnapshot, SpeedTreeForestDirectX →
SpeedTreeForestRenderer, VertexShaders → TreeVertexData.
NativeResourceAudit mit inaktivem Allocator und künstlichen Nullzählern wurde durch
SourceResourceAudit mit echten CPU-Owner-Zählern ersetzt. D3D9-Abwesenheit wird durch
Source-, Compiler-, Link- und Binary-Prüfung belegt, nicht durch einen Dummy-Zähler.

Ressourcenwrapper besitzen nur noch die bereits vorhandenen CPU-Quellen. Terrain-Alpha,
Granny-Posen/CPU-Vertices und SpeedTree-Geometrie werden unverändert berechnet.
Im bestehenden Logo-Videohilfspfad greifen Locks auf die CPU-Bildquelle zu, nicht auf
einen entfernten, ohnehin nullen nativen Texture-Pointer. Kein neues Videosystem.

Die entfernten Testdateien `TerrainGpuTest.cpp`, `TerrainSplatGpuChecks.h`,
`StaticObjectGpuChecks.h`, `ActorMaterialGpuCases.h`, `TreeGpuChecks.h`,
`EffectGpuChecks.h`, `EffectRuntimeGpuChecks.h`, `WorldGpuChecks.h`,
`UIGpuChecks.h`, `TextGpuChecks.h`, `FloatingTextGpuChecks.h`,
`DamageRuntimeGpuChecks.h` gehörten ausschließlich zum seit M13B **nicht mehr
gebauten** nativen Vergleichsprogramm. Der aktive Target `TerrainGpuTest` baut
weiterhin `DiligentGpuTest.cpp`; ActorGpuChecks und ActorStateIsolationChecks bleiben.
Keine aktive M13B-Testprüfung wurde dafür entfernt. NativeStateViewTest wurde als
DrawStateTest mit echter CPU-State-/Owner-Isolation weitergeführt; Tests fiktiver
unterdrückter Native-Draws entfallen.

## Source / Build / Binary (Punkte 15–19)

15. **Source-Audit:** Produktionsquellen enthalten keine relevante D3D9/D3DX9
API-, Header- oder Typabhängigkeit. Der automatische NoLegacyArchitecture-Test
prüft auch Buildskripte und verbietet Geräte-/Drawzugriffe im CPU-State.
16. **Verbleibende Treffer:** Jede Textfundstelle steht mit Datei, Zeile und
Kategorie in [milestone13c-source-audit.csv](milestone13c-source-audit.csv).
Produktionsquellen enthalten nur erläuternde Kommentare bzw. die ausdrücklich
gewünschten ZiiNAN-Kommentare; keine ausführbaren D3D9-Treffer. Andere Treffer sind
historische Dokumentation, externe ungenutzte SDK-Dateien oder Test-/Auditwerkzeuge.
17. **Binary-Audit:** Release und Debug: 0 D3D9-/D3DX9-Includes in jeweils 1.070
protokollierten Übersetzungseinheiten inklusive gebauter Dependencies, 0 alte
Linker-Libraries, 0 notwendige d3d9.dll/d3dx9_*.dll-Imports. Direkte und
anwendungseigene transitive Imports geprüft. Nur der Client selbst ist als
anwendungseigenes Binary beteiligt; übrige Imports sind Windows/API-Sets.
18. **Release:** vollständiger Build erfolgreich, Exitcode 0.
19. **Debug:** vollständiger Build erfolgreich, Exitcode 0; auch Debug-spezifische
Assertions auf entfernte native Objekte bereinigt.

Unabhängig erhalten: DirectInput sowie DirectShow/CMovieMan mit DDRAW.dll.
Windows-SDK `d3dtypes.h`/`d3dcaps.h` kommen über alte Videointerfaces hinzu;
das sind **keine D3D9-SDK-Abhängigkeiten**. OS-/Treiber-interne DLL-Ladevorgänge
werden nicht mit direkten Clientimports gleichgesetzt. Video-Wiedergabe war
nicht Teil der neuen manuellen Ingame-Abnahme.

## Laufzeit / Regression (Punkte 20–27)

| Nr. | Prüfung | Neues M13C-Ergebnis |
|---|---|---|
| 20 | Login | Normaler Client ohne Rendererargument: UI/Text/Input vom Nutzer bestätigt. |
| 21 | Charakterauswahl | 3D-Figur/Animation und Auswahl → Ingame vom Nutzer bestätigt. |
| 22 | Ingame | Angefragte Welt/Figuren/NPCs/Mobs, Ausrüstung/Mount, Effekte, Wasser, UI/Chat, Namen/Zahlen, Bodenitems und Questdialog vom Nutzer bestätigt; keine ungeprüften Punkte genannt. |
| 23 | World Regression | Offline sechs vollständige Phasen A1 → B1 → A1 → Dungeon → Gildenkarte → A1; sechs Screenshots, `completed phases=6`, Nutzer: „alles super“. |
| 24 | Resize | GPU-Tests mehrfach, beide finalen Start-Smokes per WM_SIZE; normaler Ingame-Test vom Nutzer bestätigt. |
| 25 | Minimize/Restore | Dreimal im Welttest und normalen Client vom Nutzer bestätigt; zusätzlich automatisches Suspend/Restore in GPU-Tests. |
| 26 | Shutdown | Welttest PID 69296: Exit 0 nach 151,1 s; normaler Client PID 69288: Exit 0 nach 51,4 s. Finale Start-Smokes PID 64424/65636 ebenfalls Exit 0. |
| 27 | Ressourcen | Beide großen Läufe: SourceTextures=0, SourceBuffers=0. Alle protokollierten GPU-Owner für Objekte/Actor/Attachments/Mounts/Bäume/Effekte/Wasser/UI/Text nach Shutdown 0. |

Normaler Sichttest: Nutzerantwort „passt“. Kein Code wird aus früheren M13B-Abnahmen
als neu validiert ausgegeben. Nach diesen Sichttests erfolgte ausschließlich
Whitespace-Bereinigung; der finale Dateistand wurde erneut Release/Debug gebaut und
der finale Client zusätzlich mit Default und explizitem d3d11 gestartet/geprüft.

Ressourcen im Weltlauf: privater Speicher Spitzenwert 436,7 MiB in der Dungeonphase;
letzte A1-Phase 346,4–355,9 MiB, maximal 926 Handles im Lauf. Terrain-/Backend-Aufräumen ist zusätzlich durch die GPU-Owner-/Reinitialisierungstests geprüft;
ein separater globaler Terrain-Buffer-Nullzähler ist im Live-Protokoll nicht vorhanden.
Kein monotoner
Speicheranstieg über die Kartenfolge. Keine Crash-/Assertion-/Device-Fehler in den
Laufprotokollen gefunden. Das ist ein mehrminütiger Regressionslauf, kein stundenlanger
Leak-/Lasttest und keine Zusage für sämtliche seltenen Assetvarianten.

## Tests / Belege

- Renderer Release: 15/15; erneut innerhalb der finalen Gesamtsuite bestanden.
- Renderer Debug, final: 15/15.
- Neue analytische Math-Suite: 653 Prüfungen.
- Isoliertes altes Math-Orakel: 11.004 Vergleiche; 142 Material-/Layout-Werte.
  Ausschließlich Diagnose unter build/milestone13c/math-probe, niemals Teil des Clientbuilds.
- Vollständige Suite: **19/19 bestanden**, Exitcode 0, 756,15 Sekunden (einschließlich Zstandard).
- `git diff --check`: sauber.

Belege relativ zum Repository: `build/milestone13c/release-verified.log`,
`debug-verified.log`, `renderer-release.log`, `renderer-debug-verified.log`,
`full-suite.log`, `full-details.log`, `dependency-audit/*`,
`world-release-final/*`, `normal-release-final/*`, `startup-*-final/*`.

## Diff / Grenzen / Abschluss (Punkte 28–30)

28. **Git-Diff:** überwiegend mechanische Umstellung geteilter Math-/State-Namen;
native toter Code entfernt, neue neutrale Math-/DDS-/State-Verträge und Audit-/Test-
Dokumentation hinzugefügt. 300 getrackte Dateipfade: +4.511/-13.303 Zeilen;
zusätzlich 21 neue Dateien (darunter neutrale Ersatzdateien und das vollständige
Treffer-CSV). Umbenennungen werden ohne Staging als Löschung/neue Datei angezeigt. Keine
Commits, Pushes, Resets oder Änderungen am Server/Originalruntime vorgenommen.
Löschungen sind im Git-Diff sichtbar und aus der Historie wiederherstellbar.
29. **Restaltlasten außerhalb des normalen Rendererbuilds:** ungenutzte SDK-Header und
d3d9/d3dx9-Libraries in extern, historische Berichte/Fixture-Audits, Diagnoseorakel im
ignorierten build-Verzeichnis. Unabhängige Windows-Video-/Input-Komponenten bleiben.
Keine dynamischen Schatten, Vollbild-Neuentwicklung oder Phase-B-Funktionen hinzugefügt.
30. **Phase A: vollständig abgeschlossen im vereinbarten D3D9-Cleanup-Umfang.**
Source-/Build-/Binary-Audit, beide Konfigurationen, vollständige Testsuite und neue
Runtime-Abnahme erfolgreich. STOPP: keine Phase-B-Modernisierung begonnen.
