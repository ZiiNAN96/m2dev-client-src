# G0-X – Graphics Settings Foundation

Datum: 2026-09-15. Windows x64 / Diligent D3D11; portabler GCC/LP64-Core.
Kein Commit/Push. G0-X endet bei der Settings-Grundlage. G1-X wird nicht begonnen.

**Ergebnis: GO für G0-X.** Release 57/57, Debug 57/57, GCC/LP64 32/32.
Manuelle Login-/Ingame-Abnahme bestätigt; abschließende UI-Korrekturen separat
nativ geprüft. Persistenz/Neustart, GR2/GLB/Vegetation, Fensterlebensdauer und
Shutdown-Ressourcen bestanden. Einzelheiten und Prüfgrenzen unten.

## 1. Existing Settings Audit

Vor Änderungen geprüft: `PythonSystem.{h,cpp}`, `PythonSystemModule.cpp`,
`PythonApplication.cpp`, `PythonBackground.cpp`, `MapManager.cpp`,
`MapOutdoorCharacterShadow.cpp`, `MapOutdoor.cpp`, `PythonGraphic.cpp`,
`DiligentD3D11Backend.cpp`, `WorldTree.cpp`, `VegetationRuntime.cpp`,
`assets/root/{uisystemoption,introloading,constinfo}.py` im benachbarten Client-Repository.

| Option / Speicher | Default im Code | Python / C++ Bindung | Bisheriges Apply / Wirkung |
|---|---|---|---|
| WIDTH/HEIGHT/BPP/FREQUENCY, `config/metin2.cfg` | 1024/768/32/0 | systemSetting.Get/SetConfig; CPythonSystem | Startup; OS-Mode-Liste; Resize separat über Fensterereignisse |
| WINDOWED | false; vorhandene Config: 1 | IsWindowed / app.Create | Diligent verlangt WINDOWED 1; alter Fullscreen wird beim Start ausdrücklich abgelehnt |
| GAMMA | 3 | SetConfig/ApplyConfig → CPythonGraphic::SetGamma | Diligent-Methode ist bereits vor G0 leer; keine aktive Gamma-Option |
| SHADOW_LEVEL | 3; Werte 0–5 | systemSetting.Get/SetShadowLevel; background.SetShadowLevel | Historisch sechs Stufen; dynamische Schatten im D3D11-Pfad deaktiviert; Boden-Schattenflag wird bei vorhandenem worldRenderer unterdrückt |
| FOG_LEVEL | 0 (dicht); vorhandene Config: 2 | systemSetting.Get/SetFogLevel → MapManager | Aktiv: Distanzfaktoren .75/1/1.25, Dichtefaktoren .000006/.000004/.000002 |
| VISIBILITY | 3 | alter Config-Tuple / GetDistance | Gespeichert, jedoch kein wirksamer Renderer-Consumer; wird nicht als tatsächliche Sichtweite migriert |
| Tatsächliche Sichtweite | Ziel 25600, adaptiv nach Renderlast | background.SetViewDistanceSet/SelectViewDistanceNum, app.SetSightRange | introloading definiert fünf Slots; Process aktualisiert Slot 0, Diagnose-Sichtweite bleibt vorhanden |
| Kameraabstand | 2500 / 3500 | constInfo → app.SetCameraMaxDistance | Live, Python-Sitzungswert; getrennt von Weltsichtweite |
| Vegetation | Native Asset-LOD/Cull-Grenzen | WorldTree → Vegetation::Instance::Update | Keine persistente Qualitätsoption vor G0 |
| Wasser | 30 vorhandene Frames, 70 ms | MapOutdoorWater | Keine Qualitätsoption; bestehende Animation/Materialien |
| DECOMPRESSED_TEXTURE | 0 | CPythonSystem Config | Persistiert, im aktuellen Rendering kein aktiver Qualitätsconsumer |
| OBJECT_CULLING | true | isViewCulling / alter Config-Tuple | Historischer gespeicherter Schalter ohne produktiven Consumer der Option |
| SOFTWARE_TILING | 0 auto; vorhandene Config: 2 | systemSetting / background / altes Optionsmenü | Übernommener Legacy-Schalter, bestehender Apply-Exit; kein neuer Diligent-Qualitätsmodus |
| SOFTWARE_CURSOR | false | SetConfig/ApplyConfig → SetCursorMode | Live; beibehalten |
| Effekte | konstante Python-Flags, z.B. Skill-Upgrade-Effekte | constInfo / app / chrmgr | Kein allgemeiner persistenter Effects-Quality-Regler |
| VSync | Present(1) | DiligentD3D11Backend | Kein VSync-Menü; unverändert |
| Frame limiter | m_iFPS=60, bestehende Zeitsteuerung | app.SetFPS | Kein neuer persistenter FPS-Regler; unverändert |
| Chat/Namen/Schaden/Shoptexte | true | systemSetting Flags | Bestehende Live-UI-Optionen und Persistenz bleiben erhalten |

`CPythonSystem::LoadConfig/SaveConfig` verwalten weiterhin `metin2.cfg`.
G0 ergänzt eine getrennte `config/graphics.cfg`. Der alte Config-Parser überspringt
jetzt unvollständige Zeilen und begrenzt Tokens; SAVE_ID wird terminierend kopiert.
`m_OldConfig` erhält auch bei fehlender Datei einen definierten Default.
Keine Änderung an Konto-/Serverkonfiguration, Protokoll, FPS- oder VSync-Verhalten.

## 2. Architecture

```text
Systemoptionen / Python systemSetting
                 ↓
CPythonSystem → Graphics::Store (ein Besitzer, Mainthread)
                 ↓ Validate / PresetSettings / Resolve
          GraphicsSettingsChanged (Bitmaske + Revision)
                 ↓ Process-Framegrenze
          GraphicsRuntimeConfig (Wertkopie)
                 ├─ DiligentD3D11Backend
                 ├─ Background / View Distance
                 ├─ MapManager / Fog
                 ├─ WorldTree → Vegetation Runtime
                 └─ MapOutdoorWater
```

`src/Graphics` kennt weder Python, Renderer noch Asset-Importer. Nur der
Game-Settings-Adapter nutzt die Datei-API. Render-Consumer lesen numerische Felder.

## 3. GraphicsSettings

Öffentliche Enums für Preset, Style, Schatten, AO, Wasser, Vegetation und Texturen;
HDR/Bloom/ModernSky/HQFog als gewünschte Flags; Sichtweite als float in nativen
Welteinheiten. Zusätzlich `fogLevel` für die tatsächlich vorhandene Nebeldichte.
`ShadowQuality::LegacySolo=2` erhält die sechste historische Stufe verlustfrei.
Keine GPU-Datenbank, FPS-, Wetter- oder zusätzliche Remasteroptionen.

## 4. RuntimeConfig

Kompakter, nicht besitzender Werttyp: Revision, aufgelöste Qualität/Flags,
Sichtweite, Vegetations-Distanzfaktor, Nebeldichte/-distanz und bestehende
Wasser-Framezeit. `legacyShadowLevel`/`shadowTextureSize` halten die alte Anbindung
bereit; effektive Schatten sind wegen der bestehenden D3D11-Fähigkeiten Off.
Keine Dateinamen, Strings, Container oder Python-Objekte.

## 5. Presets

Alle Presets sind vollständig und reproduzierbar. Eine Preset-Wahl behält den
bereits gewählten Style; sie erzwingt Modern nicht. Werte mit „reserviert“ werden
gespeichert, ergeben im aktuellen Renderer jedoch keine neuen Features.

| Preset | Schattenwunsch (reserviert) | AO (reserviert) | HDR/Bloom/Sky/HQFog (reserviert) | Wasserwunsch (reserviert) | Vegetation | Texturenwunsch (reserviert) | Sichtweite |
|---|---|---|---|---|---|---|---:|
| Low | Off | Off | aus | Low | Low | Medium | 12800 |
| Medium | Medium (alt 3) | SSAO | aus | Medium | Medium | High | 19200 |
| High | High (alt 4) | GTAO | an | High | High | High | 25600 |
| Ultra | Ultra (alt 5) | GTAO | an | High | Ultra | Ultra | 38400 |

## 6. Low

Explizite niedrige Auswahl: Vegetationsfaktor .65, halbes Sichtweitenziel.
AO/HDR/Bloom aus. Keine automatische Umstellung bestehender Nutzer auf Low.

## 7. Medium

Vegetationsfaktor .85, Sichtweitenziel 19200. SSAO ist ausschließlich ein
persistierter Zukunftswunsch. Aktuell gleiche Wasser-/Texturimplementierung.

## 8. High

Vegetationsfaktor 1, Sichtweitenziel 25600. Damit bleiben diese beiden Parameter
auf dem bisherigen Niveau. Neue hochwertige Feature-Wünsche bleiben reserviert.

## 9. Ultra

Vegetationsfaktor 1.25, Sichtweitenziel 38400. Verlängert die Verwendung bestehender
LOD-Stufen, ohne neue Geometrie, Vegetationstechnik oder Renderer-Passes einzuführen.

## 10. Custom

Eine wirksame Einzeländerung setzt `preset=Custom`. Identische Werte sind ein
No-op ohne Revision/Event. Rückkehr zu zufällig passenden Werten setzt das Preset
nicht automatisch zurück. Laden prüft die vollständige Konsistenz deklarierter
Presets und verwendet bei Abweichungen Custom. Custom-Auswahl erhält Einzelwerte.

## 11. Classic/Modern

Persistente Style-Grundlage vorhanden. Migration wählt Classic. Modern kann intern
gewählt und gespeichert werden, erzeugt aber ohne implementierte Fähigkeiten
keinen Remaster-Stack. Kein zweiter Renderingpfad. Die UI bietet Modern noch nicht
als wirkungslose Schaltfläche an.

## 12. Feature Flags

`GraphicsFeatures::{ShadowsEnabled,UseHDR,UseBloom,UseModernSky,AOQuality}` lesen
effektive Runtime-Werte. **implemented now:** Vegetations-LOD/Cull-Abstand,
Weltsichtweitenziel, bestehender Nebel, aktuelle Wasserparameter.
**reserved for G1–G8:** dynamische Schattenqualität, AO, HDR, Bloom, ModernSky,
HQFog, neue Wasser-/Texturstufen und die visuelle Modern-Unterscheidung.
Capability-Auflösung erfolgt zentral in `Resolve`, auch für Modern bleiben
unimplementierte Effekte aus. Vorhandene gebackene Asset-Beleuchtung bleibt erhalten.

## 13. Config Version

`graphics.cfg`: VERSION 1, lineares Schlüssel/Wert-Format, 64-KiB-Grenze.
Alle 13 Settings werden serialisiert; Floats mit `max_digits10` und klassischer
Locale. Keine Abhängigkeit von Windows-`long`, nativen Struct-Dumps oder JSON-Bibliotheken.

## 14. Migration

Fehlende Sidecar-Datei: Schatten 0–5 und Nebel 0–2 aus `metin2.cfg`, Classic,
Custom, aktuelle Vegetation High und tatsächliches bisheriges Sichtweitenziel
25600. Historisches VISIBILITY bleibt erhalten, da es kein echtes View-Distance-
Setting war. Version 0/unversionierte Sidecars übernehmen bekannte Felder mit
Defaults für fehlende Werte. Unbekannte Felder/Kommentare bleiben beim Speichern
kompatibler Dateien erhalten. Zukunfts-/ungültige Versionen werden nicht überschrieben.

## 15. Validation

Enums werden geprüft, Nebel auf 0–2 und Sichtweite auf 6400–38400 begrenzt.
NaN/Inf/überlaufende oder unvollständige numerische Tokens ergeben sichere Defaults.
Zu große/unlesbare Dateien werden gemeldet und nicht ersetzt. Ungültige
Preset-Zuordnungen werden Custom. Alte View-Slot-APIs erhalten Index-/Finite-Grenzen.

## 16. Save/Load

`GetGraphicsSettings`, `ApplyGraphicsSettings`, `ApplyGraphicsPreset`,
`LoadGraphicsSettings`, `SaveGraphicsSettings` existieren zentral und als Python-
Bindings. Speichern schreibt zuerst eine geprüfte temporäre Datei und ersetzt
danach atomar (Windows MoveFileExW, POSIX rename). Fehler werden an UI/Caller
zurückgegeben; eine Zukunftsversion bleibt bytegleich. Das Menü speichert beim
Schließen, der normale Client weiterhin über SaveConfig. Kein Datei-I/O beim Rendern.

## 17. Change Notification

`GraphicsSettingsChanged` enthält gezielte Bits für Renderer, Schatten, Vegetation,
Sichtweite, Nebel, Wasser und reservierte Features. Mehrere Änderungen vor einem
Frame werden zusammengefasst. `ConsumeChanges` liefert den letzten Snapshot;
kein globales „reload everything“, keine Callback-Registrierungen mit Fremdlebensdauer.

## 18. Runtime Apply

Der Store bindet sich beim Erzeugen an den Hauptthread. Mutationen von Workerthreads
werden vor Zugriff auf Settings verworfen. Settings-Getter sind Mainthread-APIs
mit Debug-Assertion. `CPythonApplication::Process` publiziert Änderungen an der
Framegrenze; UI-Änderungen während Update wirken spätestens im folgenden Frame.
Asset-Loading-Worker greifen weder auf Store noch Render-Snapshot zu.

## 19. Renderer Integration

Diligent übernimmt die Runtime-Sicht bei geänderter Revision in BeginFrame.
Drawpfade verwenden die publizierten numerischen Werte. Keine Python-Abfragen,
Dateien, String-Parsing oder Settings-Map-Lookups im Render-Hotpath.
Kein neuer Pass, Shader oder PSO. Renderer-Reset erzeugt keine Settings-Besitzer.

## 20. Vegetation Integration

WorldTree übergibt den aufgelösten Distanzfaktor im RenderContext.
`Instance::Update` verwendet `distance / factor` für bestehende LOD/Cull-Grenzen.
`factor = qualityFactor × viewDistance / 25600`. Default High/25600 ergibt exakt 1.
Das System verwendet dieselben vorhandenen Geometrien, Texturen, Wind- und
Collision-Daten. Keine Reloads, Uploads oder Änderungen an Assets bei Quality-Wechsel.

## 21. Shadow Foundation

Sechs historische Werte werden verlustfrei gespeichert. Legacy-Getter und
background.SetShadowLevel laufen über dieselbe Source of Truth; Welt-Refresh
liest das aufgelöste Kompatibilitätsfeld. Die vorhandenen 512/1024/2048-Zielgrößen
bleiben vorbereitet. **Kein aktiver dynamischer Schattentest behauptet:**
BeginRenderCharacterShadowToTexture liefert bereits im Ausgangsstand false,
GRAPHICS_CAPS_CAN_NOT_DRAW_SHADOW ist true, SetDrawShadow unterdrückt den alten Pfad.
Keine Änderung dieser Fähigkeiten in G0; keine klickbare Schatten-Fakeoption.

## 22. Water Foundation

WaterQuality-Wünsche werden persistiert. Der Runtime-Resolver mappt alle drei
Stufen auf die vorhandene High-Darstellung und 70-ms-Framezeit.
MapOutdoorWater bezieht diese Zeit aus dem Snapshot. Alle 30 Texturen,
Blend-/Fog-/Animations-/Draw-Einstellungen bleiben unverändert. Kein neuer Wassereffekt.

## 23. Fog/Sky Foundation

Die tatsächlich vorhandenen Nebel-Distanz- und Dichteparameter werden beim Apply
aufgelöst. MapManager benötigt hierfür keine Python-/UserInterface-Abhängigkeit
mehr. Map-spezifisches bFogEnable, Dichte und reservierte Environment-Sky-Skalierung
bleiben bestehen. ModernSky/HQFog sind ausschließlich Zukunftswerte.

## 24. UI Integration

Das bestehende Systemoptionsfenster erhält den Button „Grafikeinstellungen“.
Ein kleines BoardWithTitleBar verwendet vorhandene Metin2-Widgets und Assets.
Aktive Bedienelemente: Preset, Vegetation, Sichtweite, Nebel. Schließen speichert;
bei Schreibfehler bleibt das Fenster offen und meldet den Fehler. Wiederöffnen
liest aktuelle Werte. Die alte Nebelauswahl wird beim Schließen synchronisiert.
Kein Polling des Stores in produktiven UI-Frames.

Der Sichtweiten-Slider verhindert rekursive native OnMove-Aufrufe: Beim Ziehen
werden nur Labels/andere Optionen aktualisiert; programmatische Refreshes sind
geschützt. Die lokale ComboBox-Variante hebt geöffnete Listen vor ihre Geschwister
und schließt andere Listen, damit keine Beschriftungen durch das Dropdown zeichnen.

## 25. UI Labels

Deutsch: Niedrig/Mittel/Hoch/Ultra/Benutzerdefiniert; Nebel Dicht/Mittel/Leicht;
Sichtweite als Prozent des bisherigen 25600-Ziels. Kurze Erklärungen nennen
Detailentfernung und adaptive Sichtweite. Graue Hinweise nennen kommende
Schatten-/Modern-/AO-/HDR-/Bloom-/Wasser-/Texturfunktionen. Kein UI-Redesign.

## 26. Live Apply Matrix

| Setting | G0-Aktion | Sichtbare Wirkung jetzt |
|---|---|---|
| Preset | Validate/Resolve, gezielte Bits | Vegetation/Sichtweite/Nebel |
| Vegetation | bestehende LOD/Cull-Parameter | Ja, bei ausreichender Entfernung |
| Sichtweite | Ziel für vorhandene adaptive Weltsichtweite + Vegetationsfaktor | Ja; Map-/Frustum-/Streaminggrenzen bleiben |
| Nebel | numerische Dichte/Distanz | Ja, wenn Map-Nebel aktiviert ist |
| Schatten | Kompatibilitätsmodus/Zielgröße | Dynamischer Pfad nicht verfügbar |
| Wasser | Wunsch + bestehende Runtime-Parameter | Gleiche aktuelle Darstellung |
| Texturen/AO/HDR/Bloom/Sky/HQFog/Style | Wunsch gespeichert, Fähigkeit zentral geprüft | Reserviert |

## 27. Restart-required Matrix

Alle neuen G0-Settings benötigen keinen Neustart für die vorhandenen Fähigkeiten.
Auflösung/BPP/Frequenz/Fenstermodus bleiben außerhalb des neuen Modells mit
bisherigem Startup-Verhalten. Native Fenstergrößenänderung bleibt live.
Legacy Tiling behält seine alte Apply-/Exit-Oberfläche. Fullscreen ist im
aktuellen Diligent-Backend weiterhin nicht unterstützt. Kein FPS-/VSync-Neudesign.

## 28. Platform Neutrality

GraphicsSettings, Presets, Validation, Serializer, Change-Tracking und RuntimeConfig
kompilieren ohne Windows/Python/Diligent. Die Datei-Implementierung kapselt den
einzigen OS-Unterschied beim atomaren Ersetzen; öffentliche Header bleiben neutral.
GCC/LP64 nutzt denselben Core und dieselben Tests. Kein Android-Projekt geändert.
Spätere Plattformprofile können zentral Resolve/Presets erweitern.

## 29. Pipeline/Rebuild Strategy

ApplyCategory definiert Live, PipelineRebuild, RestartRequired. Aktuell sind alle
G0-Änderungen Live, da keine neuen Pipeline-Funktionen implementiert sind.
Gezielte Änderungsbits und Revision erlauben später bedarfsgerechte Rebuilds.
Schattenzielgröße ist kompatibel vorbereitet; aktuell wird kein Schattenziel angelegt.
Kein Quality-abhängiger Pipelinebau in Frame-Loops.

## 30. Tests

Neue kurze Tests: Graphics.Presets, Custom, Invalid, LiveApply, Vegetation,
Persistence, WriteRestart, ReadRestart, unter Windows zusätzlich DiligentSnapshot.
Alle Tests prüfen reale Verträge; Restart verwendet zwei getrennte Prozesse.
`tests/Graphics/prepare_runtime.ps1` erstellt private Client-/Pack-/Config-Kopien.
`run_runtime.ps1` führt das echte Menü zweimal aus und prüft den Neustartzustand.
Die acht portablen Graphics-Tests plus `Graphics.DiligentSnapshot` erweitern das
bisherige Gate von 48 auf 57 Windows- bzw. 24 auf 32 portable Prüfungen.
Kein Stress-/Fuzz-/Langzeittest. Python-Syntax beider UI-Dateien und des nativen
Testeintritts zusätzlich geprüft.

Windows-Gate (Release und Debug):

```powershell
ctest --test-dir build-hx-clean -C Release -j 2 -R '^(Graphics|AssetRuntime|AnimationRuntime|Vegetation|AssetTool|Platform)\.|^Renderer\.(StartupOptions|DiligentD3D11|ProductionGpu|NoLegacyArchitecture|HairLodQuick)$' --output-on-failure
```

Debug verwendet denselben Filter mit `-C Debug`. Portables Gate:
`ctest --test-dir build-hx-common -C Release --output-on-failure`.
Builds jeweils über CMake; keine Änderung generierter Projektdateien.

## 31. Preset Tests

Alle vier vollständigen Tabellenwerte einschließlich reservierter Flags geprüft;
beide Styles behalten wirksame Feature-Gates. No-op, Custom nach Einzeländerung,
Style-Erhalt, inkonsistente deklarierte Presets und sechs Legacy-Schattenstufen geprüft.

## 32. Save/Load Tests

Alle Werte inklusive Modern und nicht-ganzzahliger Sichtweite roundtrip;
Ersetzen einer vorhandenen Datei, Lesefehler/Schreibfehler, Zukunftsversion-
Erhaltung und neue Store-Instanz nach echtem Prozessneustart werden geprüft.

## 33. Invalid Config

Explizite Fälle: alle ungültigen Enum-/Bool-Werte, negatives/überlaufendes Token,
NaN/Inf, fehlender Wert, angehängter Müll, Sichtweiten-Min/Max, zu große Datei,
Version 0/1/Zukunft/defekt, unbekannte Schlüssel, Kommentare und inkonsistente Presets.
Die gültige Ausgangsdatei wird bei einem abgelehnten Save nicht zerstört.

## 34. Classic Regression

Migration bewahrt die akzeptierten Vegetations-/Wasser-/Fog-Parameter.
Die vorhandenen Goldens sind grün. Nutzer bestätigt den normalen Login-,
Charakterauswahl-, Ingame- und Grafikmenütest mit „Ja, alles fehlerfrei geprüft“.
Native GR2-/Vegetationsbilder und die finalen Grafikmenübilder wurden geprüft.
Keine automatische Modern- oder Ultra-Auswahl. Keine Änderung an Material-,
Lighting-, Animations-, Skinning- oder Asset-Providerpfaden.

## 35. GR2 Regression

Native GR2- und Animations-Defaults bleiben unverändert. Bestehende Safety-,
Golden-, Compatibility-, Warmup-, GPU-/Hair-Prüfungen gehören zum Fast Gate.
**PASS:** `tests/AssetRuntime/run_gr2_smoke.ps1 -Name g0x-gr2-a1-b1
-BuildDirectory build-hx-clean -ProductionDefault -MultiMap`.
63 s, 3518 Frames, drei Phasen A1 → B1 → A1, sechs Screenshots, Exitcode 0.
Player/NPC/Mob/Boss/Mount/Rider sowie Hair/Weapon und Idle/Walk/Run/Attack geprüft.
933 native GR2-Reads, 25350 GPU-Frames; Granny-Reads, CPU-Deformation, Fallbacks,
Fehler und verbleibende Ressourcen 0. `log/syserr.txt` leer.
Nachweise: `build/g0x/gr2-smoke.log`, `build/f2x/runtime/g0x-gr2-a1-b1/`.

## 36. GLB Regression

Bestehende Animated-GLB- und GPU-Tests bestanden.
**PASS:** `tests/AssetRuntime/run_character_smoke.ps1 -Name g0x-glb
-BuildDirectory build-hx-clean`: 17,53 s, 954 Frames, neun Stufen/Screenshots,
20 GLB-Actors über den regulären Character-Pfad, sechs Motion-Clips,
6712 GPU-Frames. Walk- und MultiInstance-Bilder geprüft; Exitcode 0,
Fehlerlog leer, Fallbacks/CPU-Deformation und verbleibende Ressourcen 0.
Nachweise: `build/g0x/glb-smoke.log`, `build/f5x/runtime/g0x-glb/`.

## 37. Vegetation Regression

Contracts/Goldens/RenderGoldens/NoLegacyDependency bleiben Bestandteil des Gates.
Der G0-Test ergänzt echte Low/High/Ultra-Culling-Wechsel ohne Asset-Neuerzeugung.
**PASS:** nativer A1/B1/A1-GR2-Lauf mit 591 erzeugten Instanzen,
143334 Vegetationsdraws, 110 LOD-Wechseln; Branches/Fronds/Leaves/Billboards aktiv.
G0-Menülauf prüft zusätzlich Low/Medium/High/Ultra und Custom live auf A1/B1.
Vegetation-Assets/-Instanzen/-RenderAssets/-Geometrien/-Buffers und Fehler nach
Shutdown 0. SpeedTree bleibt entfernt (`Vegetation.NoLegacyDependency` grün).

## 38. Release

**PASS: Build + 57/57, 27,22 s.** Buildverzeichnis `build-hx-clean`.
Nachweise: `build/g0x/release-final-build.log`, `build/g0x/release-tests.log`.
Nach Ergänzung der ausschließlich testaktiven Fenster-Bindung erneut gebaut
(`window-release-build.log`) und gezielt Graphics **9/9, 0,43 s** geprüft
(`window-release-tests.log`). Der finale native Menü- und GLB-Lauf verwendet
diesen Build. Seitdem nur UI-/Testskript-/Berichtsänderungen.
Vorhandene Vendor-/PDB-/LTCG-Diagnosen bleiben sichtbar; keine Behauptung eines
warnungsfreien Builds. Ein erster Sandbox-Configure scheiterte am verweigerten
SDK-Metadatenzugriff; der reguläre Windows-Build verwendet die installierte Toolchain.

## 39. Debug

**PASS: Build + 57/57, 113,90 s**, gleicher begrenzter Filter.
Nachweise: `build/g0x/debug-build.log`, `build/g0x/debug-tests.log`.
Ergänzte testaktive Fenster-Bindung erneut gebaut (`window-debug-build.log`),
gezielt Graphics **9/9, 0,34 s** (`window-debug-tests.log`).
Vorhandene LNK4099-/LNK4098-/LTCG- und MarkManager-Diagnosen bleiben dokumentiert;
das Ergebnis ist kein warnungsfreier Build.

## 40. GCC/LP64

**PASS: 32/32, 4,50 s.** Vollständiger portabler Build mit Cygwin GCC 12.
Nachweise: `build/g0x/gcc-final-build.log`, `build/g0x/gcc-tests.log`.
Enthält acht neue G0-Prüfungen und die bisherigen 24 portablen Regressionen.
Erster Sandbox-Build hatte Schreibzugriff auf Cygwin-Objekte verweigert; regulärer
Build erfolgreich. Bestehende GCC-Optimiererwarnungen im glTF-Code nicht beseitigt.

## 41. Runtime Smoke

**Manuelle Abnahme bestätigt:** `build/g0x/runtime/manual-01/` startete den normalen
Client mit echtem Login/Charakterauswahl/Ingame. Auf die konkrete Bitte, Presets,
Vegetation, Sichtweite und Nebel zu prüfen und normal zu schließen, antwortete der
Nutzer „Ja, alles fehlerfrei geprüft“. Prozess 14276 endete mit Exitcode 0;
GPUFrames=156255, VegetationDraws=181396, Shutdown-Zähler 0.

Die anschließende Logprüfung fand neben dem bekannten `invalid idx 0` eine
Slider-Rekursion. Sie wurde behoben und mit gedrücktem nativen DragButton in
`native-02` sowie dem finalen `native-03` gezielt erneut geprüft. Eine im Bild
erkannte Dropdown-Überlagerung wurde ebenfalls lokal korrigiert. Der echte Login
wurde nach diesen beiden UI-Korrekturen nicht nochmals manuell wiederholt;
die finale Bedienung/Layout-Prüfung stammt aus dem echten nativen Offline-Menü.
Der alte manuelle Lauf wird ausdrücklich nicht als fehlerlogfrei ausgegeben.

**Automatischer nativer Menü-/Neustarttest:**
`tests/Graphics/prepare_runtime.ps1 -Name native-03`, danach
`tests/Graphics/run_runtime.ps1 -RuntimeDirectory build/g0x/runtime/native-03`.
Zwölf Schritte im ersten Prozess: vier Presets, Custom, echter gedrückter Slider,
Nebel, persistierte reservierte Felder bei ausgeschalteten Feature-Gates,
Save/Load, geöffnete Dropdowns, Fensterwechsel, A1/B1 und Menü schließen/öffnen.
Zweiter Prozess vergleicht alle 13 Werte mit dem gespeicherten JSON-Erwartungswert.
Die Automatisierung ist kein Serverlogin-Nachweis.

**Final native-03 PASS:** 25,27 s / 1386 Frames / 12 Screenshots sowie Neustart
5,23 s / 220 Frames / 2 Screenshots; beide Exitcode 0 und Fehlerlog leer.
`pressedSlider=PASS`, alle Fensterchecks 1, alle gespeicherten Werte identisch.
Finales Dropdown-Bild `g0x-09-0915_122554.jpg` geprüft: Optionen lesbar,
keine durchzeichnenden Geschwister. Details in `build/g0x/native-03.log`,
`g0x-settings.log`, `g0x-restart.log`, `source-resource-audit-{1,2}.log` und
`terrain-renderer-{1,2}.log` unter `build/g0x/runtime/native-03/`.

Der erste Harness-Versuch `native-01` war unvollständig: ungültiger Cleanup-Aufruf
und verweigerte externe Fenstersteuerung. Kein PASS dafür; Cleanup korrigiert,
Fensteraktionen erfolgen nun im eigenen Testclient. `native-02` bestand beide
Prozesse (25,26/5,24 s) mit leerem Fehlerlog; finaler Layoutnachweis ist `native-03`.

Finale Release-Binärdatei / native-03 / GLB SHA256:
`6952822B679F975C83F018932A39C516E453ED1D84E649E5777D618D2A121494`.
Manueller und GR2-Lauf verwendeten zuvor
`00CC5EBE17E974A2DE20809510A3C33E6495D2DA8B515862581246A23F595E0B`.
Die einzige folgende C++-Änderung war die testaktive Fenster-Bindung;
produktive C++-Settings-/Renderinglogik identisch.

## 42. Resize/Minimize

**PASS:** G0-Diligent-Test prüft Snapshot-Wechsel, Suspension, Resize und natives
Minimize/Restore. Der testaktive Aufruf im eigenen Client setzt das Fenster auf
900×650, minimiert/restauriert und stellt die ursprüngliche Größe wieder her.
`resize=1, minimize=1, restore=1, originalSize=1` im nativen Menülog;
anschließende Frames/Screenshots laufen weiter. Menü offen/geschlossen/wieder
geöffnet geprüft. Der Testaufruf fehlt bei `M2_BUILD_RENDERER_TESTS=OFF`.

## 43. Shutdown

Neue Zählung `GraphicsSettingsObjects` im vorhandenen Source-Resource-Audit.
**PASS:** Core-Tests prüfen Store-/Vegetationsobjekte 0. Manuell, GR2,
GLB und finale native Einstellungen/Neustart jeweils Exitcode 0.
`source-resource-audit.log`: GraphicsSettingsObjects, SourceTextures/SourceBuffers,
Vegetation-Owner/-Geometrien/-Buffers, AssetDocuments/AnimationInstances/MeshBindings,
GR2ReaderResources, CollisionResources, RuntimeSkeletons/RuntimeAnimationClips 0.
`terrain-renderer.log`: alle acht Shutdown-Zeilen für Text/UI/Wasser/Welt/Effekte/
Actors/Attachments/Mounts/Objects 0. Der finale Menü-Runner prüft diese Zeilen und
bewahrt Source- und Renderer-Audits getrennt für beide Prozesse auf.

## 44. Resource Lifetime

Ein Store im CPythonSystem, zwei triviale Runtime-Wertkopien im Rendering.
Keine Settings-Callback-Registry, neuen GPU-Ressourcen oder Asset-Owner.
Bestehende Vegetationsassets werden bei Apply weder geladen noch hochgeladen.
Native Ressourcenprüfung erfolgte nach dem tatsächlichen Client-Shutdown.
Aktivitätszähler wie GPUFrames/Draws/Created/LODChanges bleiben absichtlich positiv;
sie sind keine verbleibenden Ressourcen. Kein laufender Testclient zurückgelassen.

## 45. Known Limitations

- Schatten waren im Ausgangsstand bereits nicht aktiv. G0 stellt keine Rendering-Fähigkeit her.
- Wasser-/Textur-/Modern-/AO-/HDR-/Bloom-/Sky-/HQFog-Stufen sind bewusst reserviert.
- Sichtweite bleibt adaptiv und durch vorhandene Map-/Streaming-/Frustumgrenzen begrenzt.
- Keine Hardwareerkennung, plattformspezifischen Presets, Android-Ausführung oder neuen FPS-/VSync-Optionen.
- Keine PBR-, CSM-, Atmosphere-, Weather-, Vegetation-2.0- oder UI-2.0-Arbeit.
- Keine Langzeit-/Stress-/Fuzz-Prüfung. Manuelle Login-/Optik-Abnahme ist bestätigt;
  die danach korrigierten UI-Details wurden nativ automatisiert und anhand von Bildern geprüft.

## 46. Git Diff

Source-Repo enthält Modell/Dateiadapter, Runtime-Anbindungen, Python-Bindings,
kurze Tests und diesen Bericht. Client-Repo enthält ausschließlich G0-Änderungen
an `assets/root/uisystemoption.py` plus `uigraphicssettings.py`.
Bereits vor G0 im Client-Repo vorhanden: modifiziertes Redthief-GR2,
`config/channel.inf`, `.hx-a2-work/` und mehrere unversionierte Logs.
Diese Änderungen sind nicht Teil des G0-Diffs. Nichts gestaged, kein Commit/Push.
Builds, Logs, temporäre Configs und Bilder bleiben unter ignorierten Buildpfaden.

G0-Umfang: **32 Dateien im Source-Repo (18 geändert, 14 neu)** sowie
**2 UI-Dateien im Client-Repo (1 geändert, 1 neu)**. Die neuen Source-Dateien sind
`src/Graphics/*`, `src/Renderer/GraphicsConfig.h`, `tests/Graphics/*` und dieser Bericht.
Vollständige lokale Patches einschließlich neuer Dateien:
`build/g0x/source.patch` und `build/g0x/client-ui.patch`;
Zusammenfassung `build/g0x/diff-summary.txt`. `git diff --check` bestanden,
Index beider Repositories unverändert/leer. Keine Buildartefakte gestaged.

## 47. GO/NO-GO

**GO für G0-X – Graphics Settings Foundation.**

| Kriterium | Ergebnis / Nachweis |
|---|---|
| Zentrale Source of Truth / öffentliche API | PASS – Store, Adapter, Legacy-Kompatibilität |
| Presets / Custom / Classic-Modern-Grundlage | PASS – deterministische Tests, echte Menüauswahl |
| Persistenz / Migration / ungültige Configs | PASS – portable Dateitests und echter Clientneustart |
| RuntimeConfig ohne Parsing im Hotpath | PASS – numerischer Snapshot, Revision, gezielte Änderungsbits |
| Renderer / Vegetation / Fog / Water angebunden | PASS – vorhandene Parameter, reservierte Fähigkeiten aus |
| Classic / GR2 / GLB / Vegetation | PASS – Goldens, native Bilder und kurze Originalasset-/GLB-Läufe |
| Release / Debug / GCC-LP64 | PASS – 57/57, 57/57, 32/32 |
| Echter Login → Charakterauswahl → Ingame | PASS – Nutzerbestätigung, manueller Client Exitcode 0 |
| Finale UI-Korrekturen | PASS – native-03, gedrückter Slider und Dropdown-Bild; Login danach nicht wiederholt |
| Resize / Minimize / Restore / Menülebensdauer | PASS – Diligent-Test und native-03 |
| Shutdown / Ressourcen | PASS – Exitcode 0, Settings/Renderer/Vegetation/tracked resources 0 |
| Remaster / FPS / Android / lange Tests | NOT RUN – ausdrücklich außerhalb G0 |
| Stage / Commit / Push | Nicht durchgeführt |

Kein offener G0-Funktionsfehler in den abschließenden Prüfungen. Die bekannten
Baseline-Diagnosen und die begrenzte visuelle Prüfabdeckung sind in 38–45 benannt.
Native-01 ist ein korrigierter Testharness-Fehllauf und zählt nicht als Abnahme.

## 48. Recommendation G1-X

Ein separat beauftragter G1-X-Milestone kann auf den zentralen Fähigkeiten,
Settings und Änderungsbits aufbauen. Neue Renderfähigkeiten zuerst implementieren
und nachweisen, dann zentral freigeben und in der UI aktivieren.
G1-X ist nicht begonnen. **STOP nach G0-X.**
