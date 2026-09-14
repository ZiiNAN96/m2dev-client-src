# Milestone C3-X – Cross-Platform Build & Android/Vulkan Prep

Stand: 2026-09-14, Europe/Vienna. Ausgangspunkt ist der unveränderte C2-X-Commit
`77e32a7` (`refactor(platform): introduce cross-platform client boundaries`).
C3-X ist ein gemeinsamer Build-/ABI- und isolierter Android-Clear-Bootstrap,
kein Android-Port des Metin2-Clients.

**Ergebnis: GO für C3-X auf dem beauftragten Mindestumfang, Success Level A erreicht.**
Die manuelle Windows-Prüfung ist vom Benutzer bestätigt; der frische Release-Client
endete mit Exitcode 0 und freigegebenen überwachten Rendererressourcen.
Android Levels B–E sind `environment-blocked`, nicht bestanden. Ohne NDK,
APK und Device-Nachweis wird keine Android-Unterstützung behauptet.

| Gate | Ergebnis | Evidenz |
|---|---|---|
| Frischer Windows x64 Configure | PASS | `build-c3x/windows-configure.log` |
| Windows Release Gesamtbuild | PASS; erster Cleanbuild 189,6 s, finale Ergänzungen 63,3 s | `windows-release-build.log`, `windows-release-final-build.log` |
| Windows Debug Gesamtbuild | PASS, Gesamtbuild Exit 0 in 142,1 s | `build-c3x/windows-debug-build.log` |
| Release Fast Gate | PASS, 15/15, 19,25 s | `build-c3x/windows-release-fast-gate.log` |
| Debug Fast Gate | PASS, 15/15 Fast-Gate-Prüfungen (davon 9/9 Platform), 9,93 s | `build-c3x/windows-debug-fast-gate.log` |
| Echter zweiter Compiler / LP64 | PASS, GCC 12.4.0, 4/4 gemeinsame Tests | `build-c3x/cygwin-common-test.log` |
| Windows PE32+/AMD64 / Imports | PASS, 68/68 PE32+/AMD64; 0 alte Grafikimports | `build-c3x/windows/evidence/` |
| Windows Ingame-/Fenstertest / Shutdown | PASS: Benutzerbestätigung, Exit 0, überwachte Ressourcen 0 | `build-c3x/runtime/windows-fast-02/` |
| Android Configure / Compile / Link / APK | ENVIRONMENT-BLOCKED | SDK/NDK/JDK/Gradle nicht gefunden |
| Device / Vulkan First Present | NOT VERIFIED | kein nutzbarer Device-/Emulatornachweis |
| Langzeit-/Stress-/Fuzztests | NOT RUN, wie beauftragt | ausschließlich expliziter Fast-Gate-Filter |

Die Logpfade in der Tabelle ohne Präfix liegen ebenfalls unter `build-c3x/`.
Die ergänzenden Audits enthalten Fundstellen und reproduzierbare Details:
[Toolchains](phase-c3x-toolchain-audit.md),
[Compiler/LP64/ABI](phase-c3x-portability-audit.md),
[Dependencies/Android](phase-c3x-dependency-audit.md),
[Android-Buildanleitung](../../android/README.md).

## 1. Toolchain Audit

Vor den Sourceänderungen wurden ausschließlich vorhandene Installationen inventarisiert.

| Werkzeug | Tatsächlich gefunden |
|---|---|
| CMake | 4.4.3, `C:\Program Files\CMake\bin\cmake.exe` |
| Visual Studio | 2022 Community 17.14.4, Installation 17.14.36202.13 |
| MSVC | Toolset 14.44.35207, Compiler 19.44.35209, Hostx64/x64 |
| Windows SDK | 10.0.22621.0 und 10.0.26100.0 |
| Ninja | 1.12.1, unter dem Visual-Studio-CMake-Verzeichnis, nicht auf PATH |
| clang / clang-cl | in den geprüften PATH-/LLVM-/VS-Pfaden nicht gefunden |
| Zweiter Hostcompiler | `C:\cygwin64\bin\g++.exe`, GCC 12.4.0, x86_64-pc-cygwin |
| Android SDK/NDK/Toolchain, adb, Studio | in geprüften Umgebungsvariablen und üblichen Installationspfaden nicht gefunden |
| Java/JDK, Gradle, nutzbarer Emulator | nicht gefunden |

Der gefundene .NET-Android-Manifest-Eintrag ist kein SDK-/NDK-Nachweis. Keine große
Softwareinstallation, kein SDK-Download, keine Emulatorinstallation, kein blindes Upgrade.
Vollständige konkrete Pfade/Versionen und Suchgrenzen stehen im Toolchain-Audit.
Der erste eingeschränkte MSBuild-Aufruf konnte den Benutzer-SDK-Pfad nicht lesen;
der tatsächliche Configure/Build lief anschließend mit Zugriff auf die installierte Toolchain.

## 2. Compiler Portability Audit

Der kuratierte Audit unterscheidet A portable, B Wrapper nötig, C Windows-only,
D Third-Party-/Architekturblocker und E inaktiven/kommentierten Bestand.
Geprüft wurden MSVC-Schlüsselwörter, Pragmas, Inline-ASM/Intrinsics, CRT-Namen,
Dateisystemfunktionen, `_MSC_VER` und SEH. Treffer sind lexikalisch, keine Fehlerzählung.

Beispiele: feste Control-Pakete/Packstrukturen sind A; `__forceinline`, `_ftelli64`,
`_getcwd`, `_snprintf` im Vollclient B; GPU-Exports/IME/Windows-Backends C;
Granny/SSE-Deformer und importierte Runtime-Binaries D; auskommentierte alte
Tabellen und CRT-Aufrufe E. Es erfolgte keine pauschale Umschreibung.

## 3. LLP64 vs LP64 Audit

Windows x64: Pointer/size_t 8 Byte, long 4. Cygwin x64 und Android ARM64: Pointer/
size_t 8, long 8. GCC bestätigt das zweite Datenmodell tatsächlich zur Laufzeit.
Der portable Test verlangt feste Wire-/Disk-Breiten, 64-Bit-Handles und Little Endian.

Konkrete Änderung: Wasserhöhen verwenden `std::int32_t` statt `long`.
Der produktive Decoder liest die bisherigen signed32- oder legacy-unsigned16-Bytes
mit exakter Längenprüfung und alignment-sicherem `memcpy`. Dabei wurde ein bestehender
Fallthrough beseitigt, der nach der 16-Bit-Konvertierung nochmals 32-Bit-Daten las.
Die Windows-Dateibytes und Wasser-/Rendering-Mathematik bleiben unverändert.

Restliche `long`-/DWORD-/Alignment-Risiken im ausgeschlossenen Vollclient sind
dokumentiert, insbesondere TDepth, DirectXMath-Verfügbarkeit, rohe NetStream-
Headerdereferenzen und Python-C-long-Brücken. Der Vollclient ist nicht LP64-fertig.

## 4. ABI / Struct Results

`PackFormat.h` extrahiert die bisherigen Diskstrukturen unverändert aus den
Crypto-/Container-Includes. Der Runtimeheader prüft weiterhin Sodium-Key/Nonce-Größen.
PacketReader/-Writer prüfen über Restlängen statt überlaufender Additionen;
Nullbreiten und leere Byteoperationen sind gezielt getestet.

| Reales Format | Geprüfter Vertrag |
|---|---|
| Packheader / Indexeintrag | 40 / 310 Byte, Alignment 1, 64-Bit-Offsets und Größen |
| Control-Pakete | Phase 5, Ping 8, Pong 4, Challenge 72, Response 68, Complete 76 Byte |
| SkinningVertex / Matrix / Materialgruppe | 40 / 64 / 12 Byte, Alignment 4, bestehende Bone-/Vertexfeldbreiten |
| NativeWindow/Cursor/Socket | 8 Byte; NativeMessage 32, Alignment 8, wParam/lParam bei 16/24 |
| Pointer / Serialisierung | Roundtrip über 32 Bit, nullptr, Socket 0, Little-Endian-Golden-Bytes, unaligned Puffer |
| Wasser | echte Produktion: signed32/unsigned16, negative/hohe Werte, Guards und fehlerhafte Längen |
| Grenzen | SIZE_MAX, unveränderter Puffer/Cursor bei Fehler, Strings mit Breite 0, Nullpadding |

Die bisherigen Windows-ABI-/Python-Tests bleiben zusätzlich erhalten. Netzwerk-
und Packprotokolle wurden nicht geändert; keine generische Big-Endian-Unterstützung.

## 5. CMake Architecture

Der Root bietet `M2_BUILD_WINDOWS_CLIENT` (Windows-Standard),
`M2_BUILD_ANDROID_BOOTSTRAP` (Android-Standard), Platform- und Renderer-Testoptionen.
64-Bit ist Pflicht; Windows-Produktion verlangt x86_64, Android ausdrücklich
`arm64-v8a`. Ein Hostcompiler darf nicht als Android-Toolchain ausgegeben werden.

`M2PlatformCommon` und `M2RendererInterface` tragen die neutralen Schnittstellen.
Ein Common-only-Configure benötigt weder Diligent noch die Game-Dependencies.
MSVC-Schalter sind compilerabhängig; Windows-Defines, Include-Wurzeln und die
vollständigen `vendor/src/extern`-Targets werden nur im Windows-Clientzweig hinzugefügt.

## 6. Windows / Common / Android Source Separation

| Zweig | Targets / Inhalte |
|---|---|
| Common | Platform-Header, feste Packet-/Pack-/Wasserformate, portable ABI-/Lifecycle-Prüfungen |
| Windows | M2Platform, WindowsFeatures, D3D11-Renderer, kompletter Client und bisherige Tests |
| Android | M2PlatformAndroid, M2RendererVulkanBootstrap, NativeGlue, M2AndroidBootstrap |

`PlatformTargetAudit.cmake` prüft die tatsächlich konfigurierten eigenen Targets
auf falsche Windows-/Game-Targets, Quellen und Bibliotheken und schreibt ein
Targetinventar. Common/Android enthält kein DirectInput, WebView2, WinSock,
DirectShow, Shell/Registry, Windows-Crashdump oder D3D11.
Der GCC-Compile-Datenbanktest fand keine geprüften Windows-Flags/-Bibliotheken.

## 7. Android Platform Skeleton

`src/Platform/Android` enthält Application, Window, Input, Filesystem, Time,
DynamicLibrary und das gemeinsame Lifecycle-Modell. Implementiert ist nur der
Bootstrapbedarf. Die vorhandenen Desktop-`PlatformWindow`-/Keyboard-`PlatformInput`-
Klassen werden nicht durch vorgetäuschte Android-Implementierungen ersetzt.

## 8. Android Lifecycle

NativeActivity plus NDK `android_native_app_glue` ist der kleinste geeignete Einstieg.
`ALooper_pollOnce` verarbeitet native Ereignisse und blockiert ohne renderbare Surface.
Der tatsächliche Rendergate konsumiert das hostgetestete Lifecycle-Modell:
resumed, focused, positive Ausdehnung, Renderer vorhanden, App nicht zerstört.
Pause, Fokusverlust, Surfaceverlust und Shutdown sperren Present; späte Callback-
Zustandsänderungen können eine zerstörte App nicht wieder renderfähig machen.

## 9. Android Window / Surface

`ANativeWindow` wird retained und erst nach Freigabe von Swapchain/Context/Device
losgelassen. INIT_WINDOW initialisiert, Größen-/Konfigurationsereignisse aktualisieren,
TERM_WINDOW beendet den Renderer vor dem Fenster. Eine neue Surface initialisiert
einen neuen Renderer. Reale Android-Callback-/Orientierungsfolgen sind ungeprüft.

## 10. Android Input Skeleton

Touch down/move/up/cancel wird mit Pointer-ID und Position an die neutrale
`Platform::TouchEventHandler`-Grenze übersetzt. Pause, Fokus- und Surfaceverlust
brechen aktive Touches ab. Keine Bewegung, Kamera, Skills oder UI-Aktionen sind
gebunden; kein Android-Gameplay-Input wird behauptet.

## 11. Android Filesystem

Read-only: Activity-`AAssetManager`. Schreibbar: absoluter privater
`internalDataPath`. Der Clear-Bootstrap braucht keine Dateien und erzwingt daher
keine Packmigration. Keine Windows-Pfade und keine CWD-Annahme.
Assets/Packs, Cache, Screenshots, Logs und Patches bleiben spätere Runtime-Arbeit.

## 12. Diligent Vulkan Audit

Unverändert gepinnt: DiligentCore v2.5.6,
`b036337d68be2353c9950a85929acf796b9a6d50`.
Die tatsächlichen Quellen enthalten Android-Plattformunterstützung, EngineFactoryVk,
CreateDeviceAndContextsVk, CreateSwapChainVk und `NativeWindow.pAWindow`.

Der richtige Targetname lautet `Diligent-GraphicsEngineVk-static`.
Bisher war Vulkan deaktiviert; die Windows-Dependency wurde nur für D3D11 gebaut.
Android benötigt xxHash, Vulkan-Headers, volk, SPIRV-Headers und SPIRV-Cross.
HLSL/glslang sind für Clear-only deaktiviert; keine Version wurde angehoben.

## 13. Vulkan Bootstrap

`src/Renderer/Vulkan/VulkanBootstrap.*` besitzt Device, Context und Swapchain,
cleart einen dunkelblauen Frame und ruft Present auf. Initialisierung/Resize/Present
prüfen Diligent-Fehlerrückmeldungen. Shutdown wartet auf Idle und gibt Ressourcen frei.
Der D3D11-Produktionspfad bleibt separat und unverändert.
Die Android-Datei wurde nur geprüft, nicht mit NDK kompiliert.

## 14. ARM64 Target

`M2AndroidBootstrap` ist ein reales SHARED-CMake-Ziel für `arm64-v8a`,
mit exportiertem NativeActivity-Einstieg und `libm2_android_bootstrap.so`.
Der Gradle-ABI-Filter und der Root-Guard erzwingen ARM64.
Keine armeabi-v7a-/x86-Android-Ziele. Noch kein erzeugtes ARM64-Binary.

## 15. APK / App Build

Kleines Projekt unter `android/`: Manifest, AGP/Gradle-Einstellungen, App-Target.
Geplant: Debug, Package `com.ziinan.m2bootstrap`, ABI `arm64-v8a`, min API 26.
Festgelegte Voraussetzung: AGP 8.9.2, Gradle 8.11.1, JDK 17, SDK 35,
Build Tools 35.0.0, NDK 27.0.12077973, CMake 3.31.6.

Vorgesehener Output relativ zum Repository-Elternverzeichnis:
`m2dev-client-builds/c3x/android/app/outputs/apk/debug/app-debug.apk`.
Dieser Pfad ist eine Konfiguration, kein vorhandenes APK. Keine Release-Signierung.

## 16. Device / Emulator Result

`device runtime not verified`. Kein vorhandener nutzbarer adb-/SDK-/Emulatorpfad
wurde gefunden. Kein neues Gerät oder Emulator eingerichtet. Ein Hostbuild oder
eine Manifestprüfung ersetzt keinen Device-Start.

## 17. Startup / Shutdown

Android-logcat-Marken für Platform, Surface, Vulkan, Swapchain, ersten Present-Rücklauf
und Shutdown sind implementiert. Keine Frame-Logflut.
Diligent Present liefert void: Der Marker bedeutet nur Rückkehr ohne gemeldeten
Fehler; ein tatsächlicher sichtbarer Frame benötigt Device-Beobachtung.

Windows-Startup der frischen Testkopie bestätigt D3D11, Skinning GPU,
`SkinningSelection=default`. Der abgeschlossene Lauf meldet Exitcode 0.
SourceTextures/SourceBuffers, SkinMeshes/Remaps/Palettes, PrototypeGeometry/Palettes
und die protokollierten Renderer-Shutdown-Owner stehen auf 0. CPU-Deformationen,
GPU-Fallbacks und SkinPreparationFailures stehen ebenfalls auf 0.

## 18. Third-Party Matrix

Die ausführliche Matrix nennt Dependency, Windowsform, lokale Sourceverfügbarkeit,
Androidstatus, Schweregrad und spätere Arbeit. Importierte Granny-/SpeedTree-/
Python-/WebView-Archive wurden zusätzlich mit dumpbin geprüft: 482 Maschinen-
einträge x64; keine ARM64-Library daraus abgeleitet.

LZO 2.10, zstd 1.5.7, libsodium 1.0.20, mio und FreeType besitzen Source und
portable Kandidatenpfade, sind jedoch nicht NDK-validiert. Der Bootstrap benötigt
sie nicht. Audio nutzt vorhandenen miniaudio-Source mit Android-Backends, sein
Clientwrapper und Storage/Lifecycle sind weiterhin Portierungsarbeit.

## 19. Granny

Header/API 2.11.8.0; importierte Windows-x64-Static-Library. Runtime-Source ist
hier nicht vorhanden. Ein Android-ARM64-Headerzweig beweist weder lizenzierte Source
noch eine passende Binary. Direkte Clientmodule: EterGrnLib, GameLib, ScriptLib,
UserInterface. Zentraler Vollclient-Linkblocker; Klärung für D/F.
Keine Portierung oder Ersetzung.

## 20. SpeedTree

SpeedTreeRT-Header bezeichnet Release 1.6.0; tatsächliche Binaryversion darüber
hinaus nicht unabhängig bestimmt. Windows-x64-`speedtree_static[d].lib`,
Clientwrapper vorhanden, proprietäre Runtime-Source fehlt.
Vollclient-Blocker; kein Austausch und keine Vegetationsänderung.

## 21. Python

Vorhandene Header 3.14.3, `python314_static.lib`, Windows-`pyconfig.h`,
`SIZEOF_LONG=4`, pathcch/bcrypt. CPython-Runtime-Source fehlt lokal.
Upstream Python 3.14 beschreibt Android-Embedding, aber passende ARM64-Runtime,
Konfiguration, stdlib und eigene C-API-Module müssen separat gebaut/integriert werden.
Kein Python-Upgrade.

## 22. Networking

C2-X kapselt Start/Ende, Close, Fehler und Teile der IPv4-Auflösung.
NetStream braucht weiterhin Socket/Create/Connect/recv/send/select/ioctlsocket.
POSIX fehlen FD-/Invalid-Regeln, errno/EAGAIN/EWOULDBLOCK/EINPROGRESS,
fcntl-nonblocking und `select(maxFd+1,...)`.
Kein Netcode-Rewrite und keine Android-Serververbindung.

## 23. Threading

Win32 Critical Sections, HANDLE-Threads, _beginthreadex und Events bleiben in
ausgeschlossenen Clientmodulen. Der Android-Einstieg verwendet den NDK-Glue-Thread;
kein Win32-Pump-/Event-Nachbau und kein Threading-Redesign.

## 24. Dynamic Libraries

Windows-RAII-Grenze bleibt erhalten. Android implementiert dlopen/dlsym/dlclose
für .so; Linkernamensräume und zulässige Systemlibraries bleiben Android-Verträge.
Die Implementierung wird als Android-Libraryquelle gebaut, aber vom Clear nicht gebraucht.
Time und DynamicLibrary kompilierten zusätzlich mit GCC/LP64 und
`-Wall -Wextra -Werror`; dies ist keine NDK- oder .so-Laufzeitprüfung.

## 25. Diagnostics

Windows-Diagnostics-Code und normales Opt-in-Verhalten unverändert. Der kurze
Ingame-Lauf nutzt ausdrücklich `--renderer-diagnostics` für die Abnahme.
Android verwendet nur logcat; Windows-Crashdumps sind ausgeschlossen.
Vulkanvalidation ist optional und standardmäßig aus; ein angeforderter Layer
ist ohne installierten Layer/Device nicht als aktiviert oder bestanden zu werten.

## 26. Windows Release Build

Frischer eigener Buildbaum `build-c3x/windows`, Visual Studio 17 2022, x64.
Die unveränderte vorhandene Diligent-Source wurde als FetchContent-Source genutzt;
Windows-Bibliotheken/Objekte wurden im neuen Baum neu gebaut.
Gesamtbuild PASS; finale Release-EXE SHA256:
`45AAE9BB2D79A82F3D8262B0A540129ED1927A7933EDC235BF30F4ACA82D5082`.

Bestehende Warnungen wurden gegen die C2-X-Buildlogs abgegrenzt: LNK4099 (fehlende
Drittanbieter-PDBs), Debug LNK4098 (Python/CRT) und LNK4075, Formatwarnungen
C4313/C4477 im unveränderten DumpProto sowie C4834 in MarkManager. Dazu kommen
Makro-/Compileroption- und Dependency-CMake-Warnungen. Kein Anspruch auf einen
warnungsfreien Build; keine Änderung zur bloßen Unterdrückung dieser Meldungen.

## 27. Windows Debug Build

PASS, Gesamtbuild Exit 0 in 142,1 s. Derselbe frisch konfigurierte x64-Baum, eigenständige Debugartefakte.
Keine alte C2-X-EXE wird als neue Abnahme ausgegeben.

## 28. Platform Tests

Release: die fünf bisherigen Tests ABI-Layouts, Python-Pointerroundtrip,
Services, HeaderBoundaries und WindowInput bestehen erneut.
Zusätzlich vier neue gemeinsame Tests; insgesamt 9/9 Platformtests.
Debug: PASS, 15/15 Fast-Gate-Prüfungen (davon 9/9 Platform), 9,93 s.

## 29. Cross-Platform Tests

Echter GCC-12.4-Cygwin-LP64-Configure, Compile, Link, Ausführung: 4/4 PASS.
PortableHeaders, PortableAbi, AndroidLifecycle und SourceBoundaries.
Die vollständige alte Renderer-Headerprüfung benötigt DirectXMath und bleibt
Windows-getestet. Dies wird nicht durch eine gefälschte Androiddefinition verdeckt.

Ubuntu-GCC/Clang-CI wurde als kurzer Common-only-Workflow ergänzt; gehostete
Ausführung hier nicht beobachtet. Konfigurations-Gegenprüfungen: PASS: echter Win32-Configure wird wegen 32 Bit abgelehnt; Android-Option mit Hostcompiler ohne NDK wird abgelehnt.

## 30. Renderer Smoke

Release: 6/6 gezielte Renderer-Smokes bestanden. D3D11 Init/Clear/Present/
Resize/Minimize/Restore/Shutdown/Reinit, ResourceSource, GPU-Startupdefault,
SkinningInstance, NoLegacyArchitecture und HairLodQuick.
HairLodQuick: beide Ninja-Varianten, je ein Haarmodell, sieben vollständige
LOD-Übergänge, 14 Fälle, CPU-/GPU-Binding-/Bildprüfung und Ressourcenfreigabe.
Keine große Crowd, kein Benchmark, keine 583 Fälle, keine Stability-Suite.

Debug: 6/6 ausgewählte Rendererprüfungen bestanden; der gesamte Fast Gate beträgt 15/15 in 9,93 s.
HairLodQuick meldet 14 Fälle/6230 Samples, Positions-/Normal-/Native-Pose-Delta 0,
D3D11 validation enabled, warnings 0 und alle geprüften GPU-Owner 0.
Produktions-Skinning, Palette, Actor-Materiallogik
und Hair-LOD-Code sind unverändert; geändert wurde nur der Testausschnitt.

## 31. Windows Runtime Smoke

Frische Release-Testkopie: `build-c3x/runtime/windows-fast-02`, PID 71200,
SHA256 identisch mit Abschnitt 26. Pack/BGM-Junctions lesen die vorhandenen
Spieldaten; Konfiguration und Logs liegen in der isolierten Kopie.
Die normale installierte Runtime-EXE wurde nicht überschrieben.
**PASS.** Der Benutzer bestätigt den angeforderten manuellen Test mit
„test ausgeführt sieht gut aus“. Damit gelten Login → Charakterauswahl → Ingame,
Bewegung/Maus/UI/Rendering, Resize, Minimize/Restore und Schließen als manuell
bestätigt. Diese Sichtprüfung ist Benutzer-Evidenz; sie wird nicht aus Logs abgeleitet.

Separater technischer Nachweis: `exit.txt` meldet `PID=71200 ExitCode=0 Seconds=2230`;
`renderer-startup.log` enthält ebenfalls `ExitCode=0`. Der Prozess ist beendet.
`source-resource-audit.log` bestätigt `SkinPreparationFailures=0`,
`AllCPUDeformationCalls=0`, `AllCPUDeformationVertices=0`, `GPUFallbacks=0`,
`CPUReferenceFrames=0` sowie alle dort überwachten Source-/Skin-/GPU-Owner auf 0.
`terrain-renderer.log` bestätigt sämtliche protokollierten Text-/UI-/Wasser-/World-/
Effect-/Tree-/Actor-/Attachment-/Mount-/Object-Shutdown-Owner auf 0;
`static-object-adapter.log` meldet abschließend `live_objects=0`.

Die 2230 Sekunden sind die gesamte Prozesslebensdauer einschließlich Warte- und
manueller Bedienzeit. Daraus wird kein automatisierter Langzeit-/Stresstest und
keine vollständige Phase-B-Stabilitätsabnahme abgeleitet. Keine neue Testsuite
wurde für diese abschließende Benutzerbestätigung gestartet.

Die finale Freigabe stützt sich auf den tatsächlichen Prozessabschluss und die
Benutzerrückmeldung; der zuvor gesendete Schließauftrag allein war kein Exitnachweis.

Ein erster Setupversuch während des letzten Linkvorgangs konnte die gesperrte
EXE nicht kopieren; er startete keinen Client. Der zweite Versuch nach Buildende
nutzt das nachweislich fertige Binary. Nur dieser Lauf zählt als Runtime-Evidenz.

## 32. Android Configure

ENVIRONMENT-BLOCKED: NDK-Toolchain fehlt. Kein Host-Configure wird als
Android-Configure bezeichnet. Reproduzierbarer Ablauf steht in android/README.md.

## 33. Android Compile

ENVIRONMENT-BLOCKED. Die Android-spezifischen NDK-/Diligent-Übersetzungseinheiten
wurden nicht kompiliert. GCC-Common-/Clock-/dlopen-Prüfungen sind getrennte Hostnachweise.

## 34. Android Link

ENVIRONMENT-BLOCKED. Kein .so erzeugt, keine ARM64-Linkbehauptung.
Der Bootstrap-Linkgraph schließt die bekannten proprietären Clientblocker aus.

## 35. Android Package

ENVIRONMENT-BLOCKED. Manifest syntaktisch geprüft, Gradle-Projekt strukturell
vorhanden; Gradle/SDK/NDK/JDK fehlen. Kein APK, keine Installation.

## 36. Vulkan First Present

NOT VERIFIED. Weder VkDevice-/Swapchain-Erstellung noch First Present noch
Vulkanvalidation wurden auf Android ausgeführt. Level E nicht erreicht.

## 37. Bekannte Blocker / Future Work

| Component | Current Windows status | Android status | Severity | Required future work | Target phase |
|---|---|---|---|---|---|
| Toolchain / Device | MSVC/GCC vorhanden | SDK/NDK/JDK/Gradle/Device fehlen | blockiert B–E | separat Toolchain bereitstellen, Bootstrap bauen und kurz starten | C3-X-Nachverifikation |
| Granny | produktive x64-Binary | kein passendes Runtimeartefakt | hoch, Vollclient | lizenzierte Runtime/Source oder separat freigegebene Alternative klären | D/F-Planung |
| SpeedTree | produktive x64-Binary | keine ARM64-Runtime | hoch, Bäume | Runtimeverfügbarkeit und späteren Ansatz klären | spätere Runtime-Portierung |
| Python | Windows static 3.14.3 | Runtime/Config/Extensions fehlen | hoch, Skript/UI | passender ARM64-Build und Embedding/stdlib | spätere Android-Portierung |
| Assets/Packs | bestehender Windows-Vertrag | Activitypfade vorhanden, keine Packruntime | hoch, Spielinhalte | AAsset/Storage/Pack-Vertrag definieren | Phase D |
| Video | DirectShow funktioniert im Windowszweig | ausgeschlossen | mittel, Feature | Android-Media-Grenze | spätere Android-Portierung |
| Web content | WebView2 Windows | ausgeschlossen | mittel, Feature | Android-Web-Grenze | spätere Android-Portierung |
| Input | Windows Input unverändert | Touchereignisse ohne Aktionen | hoch für Gameplay | Action-Mapping und Touchbedienung | spätere Gameplay-Portierung |
| Networking | WinSock-Client unverändert | POSIX-Operationen fehlen | hoch, Login/Spiel | Socket-/Error-/Nonblocking-Abstraktion | spätere Android-Portierung |
| Audio | miniaudio Windows | Sourcekandidat, Wrapper nicht integriert | mittel | Storage, Fokus/Pause, Androidbackend | spätere Android-Portierung |
| Renderer math / Legacy structs | Windows-DirectXMath/LLP64 | nicht allgemeiner Commonpfad | hoch für Vollrenderer | explizite Mathdependency, übrige LP64-Felder/Alignment härten | vor Vollrenderer-Portierung |

## 38. Success Level A–E

| Level | Bewertung |
|---|---|
| A | nachgewiesen: getrennte Buildstruktur, echte GCC/LP64-Commontests, ABI-/Diskhardening, strukturelles ARM64-Target |
| B | nicht erreicht: kein Android-NDK-Compile |
| C | nicht erreicht: kein ARM64-Link/APK |
| D | nicht erreicht: kein Device-/Emulatorstart |
| E | nicht erreicht: kein Android-Vulkan-Device/Swapchain/First Present |

Finale Entscheidung: **C3-X GO im beauftragten Mindestumfang; Level A erreicht.**
Release/Debug, Fast Gates, ABI-/Binärnachweise und der manuell bestätigte
Windows-Ingame-/Fenster-/Shutdown-Test sind erfolgreich. Der stabile C2-X-Commit
bleibt unverändert. Die fehlende Android-Umgebung begrenzt den Nachweis auf Level A;
Android B–E bleiben environment-blocked beziehungsweise device runtime not verified.
Das ideale GO mit Android-APK und Vulkan First Present wurde nicht erreicht.
Kein Level wird aus vorhandenem Quellcode allein aufgewertet.

## 39. Git Diff / Build Hygiene

PASS: 47 geänderte/neue Dateien, 0 staged, 0 generierte Artefakte im Diff,
0 neue/geänderte Dateien über 100 MB; git diff --check erfolgreich.
C2-X-Commit 77e32a7357396e08a5ed0bf2cb6a64a7fc0fd0a3 bleibt unverändert als Rücksprungpunkt.
Source, CMake, Tests, Dokumentation und kleine Android-/CI-Dateien bilden den Diff.
`build/`, `build-*/`, Android-Caches/Intermediates und local.properties sind ignoriert;
vorhandene absichtlich versionierte Third-Party-Binaries werden nicht pauschal ignoriert.
Keine lokalen SDK-/NDK-Pfade in Buildkonfigurationen. Konkrete Installationspfade
erscheinen nur im verlangten Audit und in ignorierter lokaler Evidenz.

Keine Commits, Pushes, Resets, Rebases, Restores, Cleans oder Stashes.
Vorhandene Änderungen/Logs im separaten `m2dev-client` wurden nicht bereinigt.

## 40. Nächster Roadmap-Schritt / STOP

Zuerst den begrenzten C3-X-Android-Nachweis mit bereitgestellter SDK-/NDK-/JDK-/
Gradle-Umgebung und einem passenden Gerät nachholen: Configure, Compile, Link,
Debug-APK, kurzer Lifecycle und sichtbarer Vulkan-Clear. Die Installation großer
Werkzeuge benötigt einen eigenen Auftrag. Danach Phase D separat planen/freigeben.

**STOP nach C3-X.** Keine Asset Runtime, Granny-/SpeedTree-Ersetzung, Python-
Migration, vollständige Android-Portierung, Touch-Gameplay, Android-Login,
Terrain/Actors/UI-Portierung, Visual Remaster oder PBR begonnen.


