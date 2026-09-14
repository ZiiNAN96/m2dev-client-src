# Phase C2-X – Platform Abstraction

Stand: 2026-09-14. C2-X kapselt den produktiven Windows-Pfad; es ist weder ein Nachweis eines portablen Clients noch der Beginn von C3-X. Windows x64 mit Diligent D3D11 bleibt das einzige Produktionsziel.

## Abschlussergebnis

**C2-X abgeschlossen: GO für C3-X auf der ausdrücklich angepassten C2-X-Prüfgrundlage. C3-X wurde nicht begonnen; es gilt STOP.**

| Gate | Finaler Status |
|---|---|
| Frische Release-/Debug-Gesamtbuilds | PASSED |
| Platformtests | PASSED — Release 5/5, Debug 5/5 |
| PE-/Importaudit | PASSED — 62/62 PE32+/AMD64; keine direkten D3D8-/D3D9-/D3DX-Imports |
| broad GPU coverage | PASSED (583 representative cases) |
| Hair LOD | PASSED |
| runtime/world/window/shutdown regression | PASSED |
| Release-Logging, Diagnosemodus standardmäßig aus | PASSED |
| long CPU/GPU image stability suite | intentionally aborted / not fully rerun for C2-X — **NOT PASSED** |
| Vollständige Debug-Renderer-Suite | NOT RUN |

Auf ausdrücklichen Benutzerwunsch wurde der laufende lange Stress-Test beendet. Bis zum Abbruch wurde kein Testfehler beobachtet oder protokolliert; dies ist keine Aussage über die nicht abgeschlossenen Prüfungen. Der bereits abgeschlossene einzelne Parity-Bildvergleich bestand; die unvollständige Langzeitsuite erhält dagegen kein PASS und wird als abgebrochen, nicht als fehlgeschlagen, gewertet. C2-X verändert Plattformgrenzen und Diagnoseausgaben, nicht GPU-Skinning-/Renderalgorithmen; die relevanten GPU- und Runtime-Gates sind erfolgreich. Deshalb ist die vollständige Wiederholung der Phase-B-Langzeitsuite keine verbleibende C2-X-Abnahmebedingung. Es werden keine weiteren Langzeit-, Stress- oder Fuzztests gestartet.

Der genaue Abbruchzustand und die Evidenzzuordnung stehen zusätzlich in [Final validation disposition](phase-c2x-validation-status.md).

## Evidenzstatus und Abgrenzung

| Status | Bedeutung |
|---|---|
| PASS | in diesem C2-X-Arbeitsstand mit gespeicherter Evidenz nachgewiesen |
| USER-BASELINE | vom Benutzer bestätigter C1-X-Ausgangszustand; kein neuer C2-X-Nachweis |
| NOT RUN | nicht durchgeführt; kein PASS und kein nachgewiesener Fehler |
| SKIPPED/ABORTED | bewusst nicht bis zum Ende ausgeführt; niemals als PASS zu lesen |

Die C1-X-Laufzeitangaben im älteren Bericht sind überholt. Für diesen Bericht gilt die ausdrückliche Benutzerbestätigung als `USER-BASELINE`: C1-X Release/Debug, x64-/ABI-/Rendererprüfungen und die manuelle Windows-Laufzeitprüfung waren erfolgreich. Der lange Fuzzer wurde in C1-X bewusst abgebrochen und bleibt `SKIPPED/ABORTED`. Der Zstandard-`playTests`-Fehler mit `/cygdrive/c/.../rle-first-block.zst` bleibt ein separates MSYS-/Cygwin-Harnessproblem.

Der produktive Audit umfasst eigene Dateien unter `src/**` mit C/C++-Endungen. Ausgeschlossen sind Vendor-/Extern-/Buildbäume, `src/PythonModules/**` (generierte große Blobs) sowie die Tools `src/DumpProto/**` und `src/PackMaker/**`. Baseline ist der unveränderliche Git-Commit `9aa71a2674e271becd586743ab02019a68900348`.

Reproduzierbarer Aufruf:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File tests/Platform/audit_platform_boundaries.ps1 `
  -Revision HEAD `
  -OutputCsv docs/platform/phase-c2x-before-counts.csv
```

Für einen nicht gespeicherten Vergleich kann `-Revision WORKTREE` verwendet werden. `-DetailCsv <datei.csv>` erzeugt zusätzlich jede Rohfundstelle. Alle Counts sind lexikalische Fundstellen, keine Laufzeit-Aufrufzählung und kein AST. Gleichnamige neutrale Methoden, Deklarationen, Diagnoseblöcke und Kommentare können deshalb einzelne API-Metriken treffen; das kuratierte Inventar bewertet ihre tatsächliche Rolle. Die gespeicherte Baseline, das kuratierte API-Inventar und das Preprocessor-Inventar liegen in:

- `docs/platform/phase-c2x-before-counts.csv`
- `docs/platform/phase-c2x-after-counts.csv`
- `docs/platform/phase-c2x-windows-api-audit.csv`
- `docs/platform/phase-c2x-preprocessor-audit.csv`

## 1. Windows API Audit

Der HEAD-Audit umfasst 654 produktive Quelldateien, 316 Header und 24 CMake-Dateien. Wesentliche Baselinewerte:

| Metrik | Vorkommen | Dateien |
|---|---:|---:|
| `windows.h`-Includes | 21 | 21 |
| Windows-Familienheader gesamt | 42 | 34 |
| davon in Headern | 23 | 18 |
| native Handle-/Message-Typen | 182 | 38 |
| davon in Headern | 72 | 15 |
| Window-/Event-API-Aufrufe | 66 | 17 |
| DirectInput-Symbole | 243 | 5 |
| Time-/Sleep-Aufrufe | 70 | 21 |
| Win32 Thread-/Sync-Aufrufe | 17 | 3 |
| Winsock-Aufrufe | 32 | 5 |
| Dynamic-Library-Aufrufe | 10 | 2 |
| Registry-Aufrufe | 11 | 2 |
| Cursor/Clipboard/Shell-Aufrufe | 34 | 13 |
| Win32-Datei-Aufrufe | 32 | 13 |
| C-stdio-Dateiaufrufe | 47 | 29 |
| `fstream`-Verwendungen | 27 | 13 |
| `std::filesystem`-Verwendungen | 10 | 4 |
| WebView2-Symbole | 15 | 1 |
| DirectShow-/Video-Symbole | 26 | 5 |

Das kuratierte CSV enthält 89 relevante Fundstellen mit Datei, Zeile, Besitzer/Funktion, Abhängigkeit, Kategorie, Ist-Rolle, C2-X-Aktion und Zielgrenze. Es ist die menschenlesbare Entscheidungsbasis; das Skript liefert die vollständige, maschinenreproduzierbare Zählung.

Die Nachherzählung wurde aus dem eingefrorenen produktiven WORKTREE erzeugt: 679 Quelldateien/330 Header gesamt und 666 Quelldateien/328 Header im Core-Scope. `gesamt` umfasst dabei bewusst auch die neuen Windows-Backends; `Core` schließt `src/Platform/Windows/**` und `src/UserInterface/Windows/**` aus. Daher darf eine steigende Gesamtzahl nicht als steigendes Leakage gelesen werden.

| Metrik | HEAD gesamt | C2-X gesamt | HEAD Core | C2-X Core |
|---|---:|---:|---:|---:|
| `windows.h`-Includes | 21 | 26 | 21 | 17 |
| Windows-Familienheader | 42 | 51 | 42 | 35 |
| Windows-Familienheader in Headern | 23 | 21 | 23 | 18 |
| native Handle-/Message-Typen | 182 | 143 | 182 | 101 |
| native Typen in Headern | 72 | 35 | 72 | 35 |
| Window-/Event-Aufrufe | 66 | 60 | 66 | 28 |
| Time-/Sleep-Aufrufe | 70 | 27 | 70 | 23 |
| Winsock-Aufrufe | 32 | 28 | 32 | 24 |
| Dynamic-Library-Aufrufe | 10 | 5 | 10 | 1 |
| DirectInput-Header | 1 | 1 | 1 | 0 |
| WebView2-Symbole | 15 | 15 | 15 | 0 |
| DirectShow-/Video-Symbole | 26 | 28 | 26 | 7 |

Der Gesamtbestand wächst erwartungsgemäß um explizite Backendincludes, während die Core-Leaks sinken. Die lexikalische Window-Metrik enthält nach C2-X auch neutrale gleichnamige Methoden wie `GetClientRect`/`SetWindow...` und deren Delegationen; Time enthält auskommentierte bzw. deaktivierte Performancepfade. Die lexikalische Metrik `directinput_symbols` steigt von 243 auf 362, weil `EterLib/Input.h` die bestehenden `DIK_*`-Namen nun ausdrücklich als wertgleiche Kompatibilitätskonstanten deklariert. Entscheidend ist hier: der DirectInput-Header fällt im Core von 1 auf 0; der Symbolcount ist kein Include-/SDK-Abhängigkeitscount.

Die Inventarspalte `result_status` trennt 57 implementierte, 6 teilweise umgesetzte, 14 verschobene, 7 bewusst Windows-only behaltene, eine nur auskommentiert erhaltene und 4 echte Interop-Grenzen. Vorschläge sind damit nicht als erledigte Arbeit missverständlich.

## 2. Kategorien

| Kategorie | Entscheidung in C2-X |
|---|---|
| A – Window / Event Loop | `PlatformWindow` mit Win32-Pimpl |
| B – Input | neutrales `PlatformInput`; DirectInput nur im Win32-Backend |
| C – Filesystem / Paths | `Platform::Filesystem`, ohne Änderung des CWD-Vertrags |
| D – Time / Sleep | `Platform::Time` für produktive Zeitquellen |
| E – Threads / Synchronisation | Audit und risikoarme Entkopplung; keine Neuarchitektur |
| F – Networking | Winsock-Start/Ende, Fehler und native Socketgrenze gekapselt |
| G – Dynamic Libraries | move-only RAII-Wrapper für DLLs und Symbole |
| H – Cursor / Clipboard / Shell | kleine Dienste; Cursor am Window-Objekt |
| I – Debug / Diagnostics | Header neutralisiert und große Logs explizit geschaltet |
| J – bewusst Windows-only | WebView2, Video, IME und Prozessdiagnostik klar begrenzt |

## 3. Platform Layer Architektur

`src/Platform` enthält neutrale öffentliche Header und Windows-Implementierungen unter `src/Platform/Windows`. `M2Platform` bündelt Window, Input, Filesystem, Time, Networking, Dynamic Library, Clipboard und Shell. `M2PlatformWindowsFeatures` kapselt WebView2 separat. Beide veröffentlichen nur `src` als Include-Wurzel; Windows-Systembibliotheken bleiben private Linkabhängigkeiten.

```text
Client / Game / Renderer
        |
        +-- PlatformWindow / PlatformInput
        +-- Filesystem / Time / Networking / DynamicLibrary
        +-- Clipboard / Shell
        |
        +-- Windows-only feature boundary: WebView2, Video, IME
                    |
                    +-- Win32 / DirectInput / Winsock / COM
```

Es wurden keine Android-Dateien und keine alternative Backend-Abstraktion ohne aktuellen Verbraucher angelegt.

## 4. Window Abstraction

`Platform/PlatformWindow.h` bietet Create/Destroy, Sichtbarkeit, Fokus, Minimize/Restore, Position/Größe, Client-/Window-Rechteck, Cursor- und Mouse-Capture-Dienste sowie ein Child-Render-Surface. `EterLib/MSWindow.*` und `MSApplication.*` sind Kompatibilitätsschichten über dieser Grenze. Der konkrete Klassenregistrierungs-, Fenster- und Style-Code liegt in `Platform/Windows/Win32PlatformWindow.cpp`.

Die Lebensdauer wird durch Pimpl und RAII getragen. `PlatformChildWindow` verwaltet den Terrain-Child und stellt die Wiederherstellung von `WS_CLIPCHILDREN` auch bei mehreren Child-Instanzen sicher. Native Windowtest, langer Ingame-Run, mehrfaches Resize/Minimize/Restore und X-Shutdown bestätigen die Grenze. Status: `PASS`.

## 5. Event Loop

`PlatformWindow::PollEvents()` kapselt `PeekMessage`, `TranslateMessage` und `DispatchMessage`. Ein Aufruf konsumiert höchstens eine Nachricht und liefert `Idle`, `Dispatched` oder `Quit`. `CMSApplication::PollEvents()` delegiert darauf; der Clientloop drainiert nur `Dispatched` und reagiert gesondert auf `Quit`. Dadurch bleibt die Win32-Pump-Semantik erhalten, ohne Message-API im neutralen App-Lifecycle zu benötigen.

## 6. Native Handle Boundary

`Platform::NativeWindowHandle` ist ein opaker Wrapper um `void*`; `NativeMessage` verwendet feste Integerbreiten. Native Fensterhandles werden nur an echten Grenzen entpackt:

- Diligent-Initialisierung in `RendererBootstrap.cpp` und `TerrainPresentation.cpp`;
- Windows-IME in `PythonApplication.cpp`/`PythonIMEModule.cpp`;
- WebView2 im Windows-Featurebackend;
- Windows-Video/COM/GDI im Windows-Featureverzeichnis.

`GrpDevice` und der CPU-Rendererpfad führen kein `HWND`/`HDC` mehr durch ihre neutralen Schnittstellen. Restbestand: `PythonApplication.h` und die IME-Header besitzen weiterhin echte Windows-Interop-Typen und sind nicht als portable Header einzustufen.

## 7. Input Abstraction

`PlatformInput` veröffentlicht `ScanCode = uint16_t`, Zustandsabfrage und KeyDown-/KeyUp-Callbacks. Der DirectInput-Gerätezugriff, Acquire/Unacquire und Zustandsvergleich liegen ausschließlich in `Win32PlatformInput.cpp`. `EterLib/Input.h` hält die bestehenden `DIK_*`-Namen als wertgleiche Adapter auf `Platform::InputKey` aufrecht; Gameplay- und Python-Code müssen `dinput.h` nicht mehr inkludieren.

Die ABI-Werte 0–255 und das Flankenverhalten bleiben absichtlich unverändert. Kein Touch- oder neues Action-Mapping wurde eingeführt.

## 8. Input Actions

Hardware und Aktion bleiben getrennte Ebenen:

| Aktion | bestehender Verbraucher | Hardwarepfad |
|---|---|---|
| Movement | `PythonPlayerEventHandler.cpp`, `PythonPlayerInputKeyboard.cpp` | ScanCode-Zustand/Key-Flanken |
| Attack / Interact | `PythonPlayerInputMouse.cpp`, Player-Eventhandler | Mouse-Buttons und Zielzustand |
| Skills / Hotkeys | Python-UI und `PythonPlayerInputKeyboard.cpp` | vorhandene DIK-kompatible Codes |
| Camera | `PythonApplication.*` und Mausereignisse | Mouse-Move/Wheel/Buttons |
| Inventory / UI | Python-UI-Keyhandler | KeyDown/KeyUp |
| Chat / IME | `PythonIME.*`, Python-UI | native Message-Interop an der IME-Grenze |
| Escape / Menu | `PythonApplication`/UI-Keyhandler | `InputKey::Escape` |

C2-X ändert keine Bindings und erzwingt kein neues Gameplay-Action-System.

## 9. Filesystem

`Platform::Filesystem` stellt Normalize/Join/Exists/ReadFile/FileSize, einen move-only `File`-Wrapper, Verzeichnislisten und Pfaddienste bereit. `EterBase/FileBase`, `FileDir`, `TempFile` und risikoarme Hilfen in `Utils` verwenden diese Grenze. Pfade überqueren die Win32-Grenze als UTF-8 und werden intern nach UTF-16 konvertiert; `OpenCFile` bewahrt für Legacy-Konfigurations-/Logaufrufer ausdrücklich die bisherige schmale CRT-Interpretation.

`FileSize(path, out)` unterscheidet Fehler von einer leeren Datei; der Ein-Argument-Overload liefert für beide Fälle 0 und dokumentiert diese historische Mehrdeutigkeit.

## 10. Path Normalization

`Normalize` wandelt Backslashes in `/` um und löst `.`/`..` lexikalisch. Absolute Laufwerkswurzeln bleiben absolut, `C:foo` bleibt laufwerksrelativ und wird nicht versehentlich zu `C:/foo`. Bei UNC-Pfaden werden Server und Share gegen `..` geschützt. `Join("pack", "root.pck")` bleibt `pack/root.pck`; Pack-/Locale-Lookups erhalten somit relative Semantik. Es wurde keine neue feste `C:\\`-Annahme eingeführt.

## 11. Writable Paths

Die bestehenden Windows-Verträge bleiben bewusst verbraucherspezifisch:

- `WritableDirectory()` = aktuelles Arbeitsverzeichnis;
- `ConfigDirectory()` = relativer Pfad `config`;
- Konfigurationen in `PythonSystem.cpp:380,488,546,565` bleiben über `OpenCFile` und dessen unveränderte Narrow-CRT-Semantik CWD-relativ;
- Packs und Patch-Packs sind Leseeinträge relativ zum übergebenen Packordner (`UserInterface.cpp:61-103,158-173`); eine getrennte Patch-Schreibablage wurde nicht gefunden;
- Locale bleibt unter `config/locale.cfg` relativ;
- der kleine `renderer-startup.log` liegt im CWD; große Rendererlogs sind diagnosegeschaltet;
- Screenshots verwenden weiterhin Windows `Documents/METIN2` mit Zeitstempelname (`EterPythonLib/PythonGraphicModule.cpp:835-878`), nicht den CWD;
- WebView2 verwendet Windows-Temp plus `Metin2/WebCache` (`Win32WebView.cpp:29-31`);
- Guild Marks bleiben unter dem relativen Ordner `mark/`;
- allgemeine temporäre Dateien verwenden das Windows-Tempverzeichnis über `TemporaryDirectory()`/`CreateTemporaryFileName()`.

Eine spätere Android-App-Storage-Zuordnung ist möglich, wurde aber nicht implementiert. Vor C3-X müssen Logs/Screenshots/Cache/Patchdaten noch verbraucherweise einer schreibbaren Pfadklasse zugeordnet werden.

## 12. Time

`Platform::Time` kapselt `QueryPerformanceCounter/Frequency`, `timeGetTime`, `GetTickCount`, Sleep sowie die 1-ms-Timerperiode. `MonotonicNanoseconds()` ist monoton und fällt bei fehlendem QPC auf den bestehenden Tickbereich zurück. Die produktiven Timer-/Frame-/Timeout-Verbraucher wurden schrittweise auf die API gesetzt.

Verbleibende `timeGetTime`-Fundstellen in `EterGrnLib/ThingInstance.cpp`, `PythonCharacterManager.cpp` und `MapOutdoorUpdate.cpp` liegen in nicht definierten `__PERFORMANCE_CHECKER__`-Diagnoseblöcken und sind keine aktive Release-Zeitquelle. Der `Sleep`-/Socketblock in `EterBase/error.cpp:151-195` ist vollständig auskommentiert. `Sleep` im aktiven `UserInterface/ProcessScanner.cpp` gehört zur bewusst Windows-spezifischen Prozessdiagnostik. Kommentare mit alten Calls zählen nicht als aktive Kopplung.

## 13. Threading

Kein Threading-Redesign wurde vorgenommen. Bestehende Synchronisations- und Loadersemantik blieb erhalten; Sleep im produktiven Loader läuft über `Platform::Time`, und der produktive Loader verwendet bereits Standard-C++-Threading. Die alten `EterLib/Thread.*`-/`Mutex.*`-Wrapper führen weiterhin `HANDLE`/`CRITICAL_SECTION`, besitzen aber keine In-Tree-Callsite. Sie sind toter Legacy-Restbestand, keine aktive Core-Laufzeitabhängigkeit. Entfernen wäre ein separater Cleanup, nicht Teil dieses Milestones.

## 14. Networking

`Platform::Networking` kapselt `WSAStartup`, `WSACleanup`, Close, Fehlerabfrage, WouldBlock, Hostname und IPv4-Auflösung. `NativeSocket` ist ein opaker `uintptr_t`-Wert mit eindeutigem Invalid-Sentinel. Protokoll, Paketlayout und Verbindungslogik bleiben unverändert. `NetStream.cpp` verwendet `recv`/`send`/`select` und die WinSock-Konvertierung weiterhin direkt als bewusst beibehaltenes Windows-Netzwerkbackend außerhalb von `Platform/Windows`; C2-X behauptet hier keine vollständige Socketabstraktion. Der Test ruft Startup zweimal und Shutdown paarig auf.

## 15. Dynamic Libraries

`Platform::DynamicLibrary` ist move-only und gibt das Modul per RAII frei. `Load` erwartet UTF-8, `LoadSystem` baut einen Pfad aus dem Windows-Systemverzeichnis, `GetSymbol` gibt bei fehlendem Symbol `nullptr` zurück. Die produktiven IME-DLL-Zugriffe wurden darauf umgestellt. Das alte Makro `SAFE_FREE_LIBRARY` in `EterBase/Utils.h` hat keine Callsite und bleibt als toter Headerrest dokumentiert.

## 16. Cursor / Clipboard / Shell

Cursor-Laden, -Setzen, Sichtbarkeit, Position und Capture laufen über `PlatformWindow`. `Platform::Clipboard` kapselt UTF-8-Text und Win32-Clipboardbesitz. `Platform::Shell` kapselt Ziel-/URL-Öffnung. Der bestehende diagnostische `WinExec("errorlog.exe", ...)`-Pfad bleibt Windows-only. Die Tests verändern absichtlich keinen Clipboardinhalt. Ein UI-Redesign fand nicht statt.

## 17. WebView2

WebView2 bleibt Windows-only. Der neutrale Aufruf liegt in `PlatformWebView.h`, COM/WebView2-Code in `Platform/Windows/Win32WebView.cpp`, und CMake baut ihn im separaten Target `M2PlatformWindowsFeatures` nur unter `WIN32`. Der Core hängt nicht von WebView2-Interfaces ab. Es gibt kein Cross-Platform-WebView-Backend. Build und Audit decken die Grenze ab; eine eigene WebView-Runtimeausübung wurde nicht durchgeführt.

## 18. Video

DirectShow/DirectDraw/COM bleiben unverändert Windows-only. Die vorher im allgemeinen UserInterface-Verzeichnis liegenden Implementierungen wurden nach `UserInterface/Windows/Win32MoviePlayer.*` und `Win32ApplicationLogo.cpp` verschoben und nur im Windows-Zweig eingebunden. `MovieMan.h` ist eine schmale Fassade; `PythonApplication.h` besitzt noch DirectShow-Forward-Deklarationen und Windows-Typen und bleibt daher dokumentierte Windows-Kopplung. Video wurde weder ersetzt noch funktional umgebaut. Build und Audit decken die Grenze ab; eine eigene Videowiedergabe-Runtimeprüfung wurde nicht durchgeführt.

## 19. DirectInput

`dinput.h` und `DirectInput8Create` liegen im Win32-Inputbackend. Die stabile DIK-Wertetabelle steht ohne DirectInput-Header in `InputKeyCodes.h`. Bestehende Namen im EterLib-Adapter behalten ihre numerischen Werte, sodass Python/Game-Logik keine neue Übersetzungsschicht und keine Bindingänderung erhält.

## 20. Diligent Window Integration

Rendererbootstrap und Terrain-Presentation erhalten das opake Handle von `PlatformWindow`. Der Renderer verwaltet weder RegisterClass/CreateWindow noch die Message Pump. Diligent D3D11 bleibt der einzige aktivierte Backendpfad; Vulkan, GL, GLES, Metal, WebGPU und D3D12 bleiben im Configure deaktiviert.

## 21. Platform Types

`NativeTypes.h` enthält nur `NativeWindowHandle`, `Point` und `Rect`; kein Windows-Header ist erforderlich. `NativeMessage` verwendet `uintptr_t`/`intptr_t`, `NativeSocket` `uintptr_t`, und `NativeCursorHandle` `void*`. `PlatformHeaderBoundaryTest` prüft Größe, Standardlayout, Kopierbarkeit sowie move-only Verträge der Ressourcentypen.

Nicht neutral sind weiterhin ausdrücklich IME/MMIO-/Bild-/Granny-/SpeedTree-Header mit Windows-Typen. Das ist bekannte verbleibende Kopplung, kein Versprechen eines heute bereits portablen Builds.

## 22. Include Boundaries

HEAD hatte 42 Windows-Familienincludes in 34 Dateien, davon 23 in 18 Headern. Nach C2-X sind es gesamt 51 in 38 Dateien, weil die neuen expliziten Backends ihre benötigten SDK-Header selbst besitzen; im Core sinkt der Wert auf 35 in 26 Dateien. Headerleaks sinken gesamt 23 → 21 und im Core 23 → 18. Der neue Headertest inkludiert alle Platform-Public-Header sowie `EterBase/Debug.h`, `EterBase/CRC32.h` und `Renderer/DrawStateTypes.h` und bricht bei Leak von Windows, Winsock, DirectInput, Shell oder Multimedia ab.

Noch vorhandene direkte Headergrenzen umfassen unter anderem `EterImageLib`, `EterLib/IME.h`, `EterGrnLib/Material.h`, `EterLocale/StdAfx.h`, `SpeedTreeLib/StdAfx.h` und einzelne EterBase-Legacyheader. Diese sind entweder echte Windows-/SDK-Interopgrenzen oder expliziter Restbestand. Der gespeicherte finale Produktionscount steht in `phase-c2x-after-counts.csv`; spätere reine Test-/Dokuänderungen verändern seinen `src`-Scope nicht.

## 23. Preprocessor Audit

Der C/C++-HEAD-Scan fand zwei `_WIN32`-Direktiven in `FontManager.cpp` und 18 `_MSC_VER`-Direktiven in 17 Dateien. Zusätzlich besitzt das außerhalb der C/C++-Metrik liegende Windows-Resource-Script eine `_WIN32`-Direktive. Alle 21 Fundstellen sind in `phase-c2x-preprocessor-audit.csv` eingestuft:

- echte Plattformgrenze: Fontverzeichnis und Windows-Resource;
- unnötig: alte `_MSC_VER > 1000/1200/1400`-Zweige, die im unterstützten Toolchainbereich konstant sind;
- Dead Code: historische Nicht-MSVC-/vor-VC6-Guild-Mark-Zweige;
- Tool-only: keine Fundstelle im produktiven Auditscope; Tools wurden bewusst separat ausgeschlossen.

Die Ergebniskennzeichnung weist 16 Einträge als verschobenen Legacy-Cleanup, zwei als beibehaltenen Dead Code und drei als echte Windows-only-Grenze aus. Es wurden keine `ANDROID`-Makros ergänzt.

## 24. CMake / Buildstruktur

Top-Level-CMake stoppt weiterhin bei Nicht-x64 und nun auch explizit bei Nicht-Windows. `src/Platform/CMakeLists.txt` listet Windowsquellen unter `if(WIN32)`. DirectX- und WebView-Vendorwrapper sind Windows-begrenzt. UserInterface bindet die Windows-Videoquellen und Windows-Systembibliotheken konditional ein. `M2RendererBootstrap` hängt an `M2Platform`; CMake bleibt Source of Truth.

Die 19 vorhandenen `GLOB_RECURSE`-Quelllisten bleiben als Buildstruktur-Risiko dokumentiert; C2-X ersetzt sie nicht flächendeckend.

## 25. Clean Configure

Status: `PASS` für Configure, nicht für den Gesamtbuild.

`build-c2x-clean/configure.log` stammt aus dem frischen Verzeichnis und meldet Visual Studio 17 2022, MSVC 19.44, Windows SDK 10.0.26100.0, Zielprozessor x86_64/AMD64 sowie D3D11 `TRUE`; alle alternativen Grafikbackends sind `FALSE`. Die Diligent-Deprecation-Hinweise sind Fremdcodewarnungen, keine C2-X-Fehler.

## 26. Solution Generation

Status: `PASS` für die Generierung.

`build-c2x-clean/m2dev-client-src.sln` und die CMake-generierten `.vcxproj` liegen vor. Die Solution enthält unter anderem `M2Platform`, `M2PlatformWindowsFeatures`, `PlatformServicesTest`, `PlatformHeaderBoundaryTest` und `PlatformWindowInputTest`. Der Multi-Config-Generator mappt Debug|x64 und Release|x64; die x64-Guards aus C1-X bleiben aktiv. CMake, nicht die Solutiondatei, ist die Quelle.

## 27. Platform Tests

Neue Tests:

- `Platform.Services`: Normalize/Join inklusive UNC und drive-relative Pfaden; relative Pack-/Configsemantik; File-RAII, Read/Size, leer/fehlend; monotone Zeit/Sleep; verschachteltes Winsock Startup/Shutdown; Kernel32-Symbol und Fehlerpfade.
- `Platform.HeaderBoundaries`: neutrale Includes, Typverträge, Input-Werte sowie DrawState-/Debug-/CRC32-Grenze. Der Clipboardinhalt wird nicht verändert.

Die finalen Release- und Debug-Plattformrunden melden jeweils 5/5 grün: `AbiLayouts`, `PointerRoundTrip`, `Services`, `HeaderBoundaries` und `WindowInput` (Release 0,25 s, Debug 0,51 s). `WindowInput` erzeugt echte native Handles, prüft Move/Resize, zwei Child-Surfaces, drei Minimize/Restore-Zyklen, DirectInput-Initialisierung und `WM_CLOSE`; globale Accessibility-Einstellungen und Input-Injection werden bewusst nicht verändert. C2-X führt gezielt Tests über `-R ^(Platform|Renderer)` aus; Vendor-Fuzzer und Zstd-`playTests` sind nicht Bestandteil dieses PASS. Status: `PASS`.

Die separate Release-Renderer-Runde endete auf Benutzerwunsch vorzeitig: 21 abgeschlossene Prüfungen bestanden, von 24 geplanten. `Renderer.SkinningStability.parity` war bereits erfolgreich abgeschlossen (371,04 s); `Renderer.SkinningStability.stress` wurde laufend abgebrochen und erhält kein PASS. `Renderer.SkinningFallbackParity` und `Renderer.SkinningBenchmark` wurden nicht gestartet. Die vollständige Debug-Renderer-Suite wurde nicht ausgeführt. Der Runner endete mit Abbruchcode −1, nicht mit einem erfolgreichen Suite-Abschluss. Es gibt keine laufenden zugehörigen Testprozesse mehr. Rohbelege: `build-c2x-clean/renderer-release-tests.log` und `build-c2x-clean/evidence/renderer-release-aborted-ctest.log`. Die vollständige lange CPU/GPU-Bildstabilitätssuite bleibt **INTENTIONALLY ABORTED / NOT FULLY RERUN FOR C2-X, NOT PASSED**.

## 28. x64 Regression

C1-X ist `USER-BASELINE`: x64 only, Pointer-/ABI-Checks und PE32+/AMD64 waren bestätigt. C2-X verändert diese Regeln nicht. Die finalen C2-X-Gesamtbuilds Release und Debug endeten jeweils mit Exitcode 0. Der frische Binaryaudit prüfte 62/62 Client-, Tool-, Platform-/Renderer-Test-EXEs und Diligent-DLLs aus beiden Konfigurationen als PE32+/AMD64; direkte D3D8-/D3D9-/D3DX-Imports: 0. Release- und Runtime-EXE stimmen mit SHA-256 `0F7F33CEF84F4EB8A856A7693E5338DE8063FAC595F0CC59380E04ED2340783C` überein. Evidenz liegt unter `build-c2x-clean/evidence/c2x-binary-audit.csv`, `c2x-binary-imports.csv` und `c2x-binary-audit-summary.txt`. Status: `PASS`.

## 29. GPU Skinning Regression

GPU-Skinning bleibt Produktionsdefault; `--skinning=cpu` bleibt explizite Referenz/Fallback. Der frische Release-Smoke protokolliert Skinning `gpu` und Diligent D3D11. Der benutzerbegleitete Diagnose-Run deckte Spieler, NPC/Mobs, Reittier, Rüstung/Haare, UI, Effekte, Wasser und Bäume über die vollständige Worldfolge ohne sichtbare Auffälligkeit ab. Der Source-Audit meldet `AllCPUDeformationCalls=0`, `GPUFallbacks=0`, `SkinPreparationFailures=0` und alle Owner nach Shutdown 0; `GPUFrames=670007` sind Actor-Ereignisse, keine Videoframes. Automatische breite Coverage ist repräsentativ, nicht der gesamte Assetkatalog: 583 Fälle (224 Hair-LOD, 320 Player, 39 NPC/Mob/Mount/Waffen-Beispiele); darunter 8 Player-Varianten × 5 Shapes × 5 Motions für 200 Body- und 120 Hair-Fälle, die Bosse `fire_dragon`/`diseased_bosshost` in der Mobkategorie und fünf Mountmodelle. Release Coverage (292,57 s) und Hair-LOD (92,87 s) bestanden. Status: `PASSED` für broad GPU coverage, Hair LOD und manuelle/Lifecycle-GPU-Regressionsprüfung. Die vollständige lange CPU/GPU-Bildstabilitätssuite wurde bewusst nicht vollständig wiederholt und ist **NOT PASSED**, siehe Abschnitt 27.

## 30. Runtime Regression

C1-X Login → Character Select → Ingame ist `USER-BASELINE`. C2-X-Evidenz ist davon getrennt:

- automatisiert: Release-Smoke PID 44716, Exitcode 0, Initialize, `WM_SIZE`/Resize, fünf Frames, Shutdown, D3D11, GPU-Default und `VerboseDiagnostics=0`;
- benutzerbestätigt: Diagnose-Run PID 64780 mit `--renderer-diagnostics`, 317,3 s, Login/Ingame, komplette Input/UI/Rendering- und Worldmatrix, Relog und X-Shutdown ohne Auffälligkeit, Exitcode 0;
- zusätzlich beobachtet: normaler Release-Run PID 67936, 64,7 s, Ingame, `VerboseDiagnostics=0`, GPU-Default, X-Shutdown und Exitcode 0.

Status: `PASS` für die geforderte Windows-Runtime-Regressionsmatrix. WebView2 und Video bleiben ausdrücklich nicht separat ausgeübt.

Ein bereits laufender fremder/offizieller Clientprozess wurde nicht berührt und ist keine C2-X-Evidenz.

## 31. World Regression

A1 → B1 → A1 → Dungeon → Gildenkarte → A1 wurde vom Benutzer mit der frischen C2-X-Release-EXE vollständig und ohne Auffälligkeit bestätigt. Status: `PASS`.

## 32. Input Regression

Automatisch sind ScanCode-Werte, echte Window-/DirectInput-Initialisierung und PlatformInput-Verträge abgedeckt. Der Benutzer bestätigte im frischen C2-X-Ingame-Run Eingabe, UI und Darstellung ohne Auffälligkeit. Status: `PASS` für die geforderte manuelle Input/UI-Matrix; die separate WebView-/Videoausübung ist davon nicht umfasst.

## 33. Resize / Minimize

Mehrfaches Resize und 3× Minimize/Restore wurden sowohl durch den nativen Windowtest als auch manuell ohne Auffälligkeit bestätigt. Status: `PASS`.

## 34. Relog

Ingame → Character Select → Ingame wurde vom Benutzer ohne Auffälligkeit bestätigt. Status: `PASS`.

## 35. Shutdown

Beide expliziten C2-X-Ingame-Runs wurden über X geschlossen und endeten mit Exitcode 0. Der Diagnose-Run meldet nach Shutdown Terrain-, Text-, UI-, Water-, World-, Effect-, Particle-, Tree-, Actor-, Attachment-, Mount- und Object-Bestände 0 sowie alle Source-Audit-Owner 0. Status: `PASS`. Fremde/bestehende Clientprozesse blieben unangetastet.

## 36. Logging Cleanup

`Renderer::defaultVerboseDiagnostics` ist im Debugbuild `true` und sonst `false`; `--renderer-diagnostics` schaltet große Diagnoseausgaben explizit ein. Actor, Effect, Static Object, World, Tree, Terrain, UI und Text sowie der Source-Resource-Audit respektieren den zentralen Schalter. Skinning-Benchmarkausgaben bleiben separat und nur über ihren eigenen expliziten Benchmarkpfad aktiv. `renderer-startup.log` bleibt als kleiner Startupnachweis zulässig.

Der normale Release-Ingame-Run (PID 67936) lief mit `VerboseDiagnostics=0` und erzeugte keine großen Rendererlogs; übrig blieb der kleine `renderer-startup.log` mit 130 Bytes sowie Dateien des Testharness. Der getrennte Diagnose-Run protokollierte `VerboseDiagnostics=1` und erzeugte die erwartete Lifecycle-Evidenz. Status: `PASS`.

## 37. Bekannte Blocker

1. Die lange CPU/GPU-Bildstabilitätssuite wurde ausdrücklich auf Benutzerwunsch abgebrochen und nicht vollständig für C2-X wiederholt: 21 abgeschlossene Release-Prüfungen bestanden; Stress abgebrochen; FallbackParity/Benchmark nicht gestartet; vollständige Debug-Renderer-Suite nicht ausgeführt. Kein beobachteter Testfehler vor Abbruch, aber kein PASS für die abgebrochene Prüfung oder Gesamtsuite. Diese Wiederholung ist aus der angepassten C2-X-Abnahme ausgenommen und kein verbleibender C3-X-GO-Blocker. Release/Debug-Gesamtbuild, Platformtests und Binaryaudit sind PASS. Es folgen keine weiteren Tests.
2. WebView2- und Videowiedergabe wurden gebaut und strukturell geprüft, aber nicht separat zur Laufzeit ausgeübt.
3. C1-X-Fuzzer: bewusst `SKIPPED/ABORTED`, niemals PASS.
4. Zstd `playTests`: separates MSYS-/Cygwin-Harnessproblem mit `/cygdrive/c/.../rle-first-block.zst`; die Datei existiert am nativen Windows-Pfad. Kein C2-X-Plattformfehler und kein grüner Test.
5. Bekannte Warnungen bleiben: Debug-Linker LNK4075/LNK4098/LNK4099, vorhandene DumpProto-Formatwarnungen C4477/C4313 und MarkManager C4834. Die Plattformänderungen erzeugen keine neuen Plattform-Compilerwarnungen; der Build wird trotzdem nicht als „warning-free“ bezeichnet.
6. Restkopplung an Windows besteht bewusst bzw. historisch in IME, MMIO/Image, Granny, SpeedTree, Video und Prozessdiagnostik sowie einzelnen Legacyheadern. C2-X schafft Grenzen, aber noch keinen Cross-Platform-Build.
7. Die alten ungenutzten Thread/Mutex-Wrapper und weitere als `deferred` markierte Headerleaks bleiben expliziter Cleanup-Restbestand.
8. Der Diagnose-Run enthält neben einem unveränderten Legacy-Schadens-Trace zwei `invalid idx 0`-Meldungen aus `MarkManager.cpp:282,309`, nachdem die private Runtime mit leerem `mark`-Verzeichnis startete. Es gibt keine Renderer-Failure-Meldung; die Guild-Mark-/Fixture-Ursache ist nicht abschließend diagnostiziert und verhindert die Aussage „alle Logs fehlerfrei“.

Git-Diff des final geprüften Arbeitsstands: 87 geänderte/verfolgte Dateien mit +842/−2376 Zeilen und 38 neue Dateien, somit 125 betroffene Dateien. Die Zeilensumme betrifft nur verfolgte Dateien und enthält nicht den Inhalt der neuen Dateien. Inhaltlich: neuer Platform-Layer und Windows-Featureverzeichnisse; Migration von Window/Input/Filesystem/Time/Networking/DLL/Clipboard/Shell; Renderer- und UI-Lifecycleanpassungen; Logging-Gates; Windows-konditionale CMake-Quellen; neue Audit-, Platform- und Runtime-Hilfen sowie Abschlussdokumentation. Die zwei im Git-Diff entfernten alten Videoquelldateien wurden unter die Windows-Grenze verlagert, nicht ersatzlos gelöscht. `git diff --check` endet mit Exitcode 0. Generierte Buildartefakte unter `build-c2x-clean` gehören nicht in den Quell-Diff; das bereits vorhandene untracked `build-clean/` blieb unangetastet. Kein Commit oder Push; vorhandene Runtime-Clients und Einstellungen wurden nicht überschrieben.

## 38. Klare GO/NO-GO-Empfehlung für C3-X

Final: **GO für C3-X auf der ausdrücklich angepassten C2-X-Prüfgrundlage**.

Der finale Quellstand baut frisch in Release und Debug. Platformtests (je 5/5), Binary-/Importaudit (62/62), broad GPU coverage (583 repräsentative Fälle), Hair LOD sowie die benutzerbestätigte Runtime-/World-/Input-/Window-/Relog-/Shutdown-Matrix sind bestanden. Before/After-Grenzzählung und Windows-Restkopplungen sind dokumentiert; der normale Releasebetrieb erzeugt keine großen Rendererlogs. Damit ist die technische Grundlage für den nächsten gesondert freizugebenden Milestone gegeben.

Die vollständige lange Phase-B-CPU/GPU-Bildstabilitätssuite wurde auf Benutzerwunsch bewusst nicht nochmals vollständig ausgeführt: **intentionally aborted / not fully rerun for C2-X, NOT PASSED**. Bis zum Abbruch wurde kein Testfehler beobachtet. Die vollständige Debug-Renderer-Suite wurde nicht gestartet. Das GO beruht auf den bestandenen C2-X-Gates und der ausdrücklichen Begrenzung der Wiederholungsprüfung, nicht auf einem angenommenen PASS für abgebrochene oder ausgelassene Tests. Nicht separat ausgeübte WebView-/Videofunktionen, bekannte Warnungen und Windows-Restkopplungen bleiben die in Abschnitt 37 benannten Einschränkungen. Android, ARM64 und Vulkan sind damit noch nicht unterstützt.

**STOP. C3-X wurde nicht begonnen.** Keine weiteren Langzeit-, Stress- oder Fuzztests. Kein Android, Vulkan, Gradle, NDK, Touch, Granny-Replacement, Asset Runtime, PBR oder Visual Remaster in diesem Milestone.
