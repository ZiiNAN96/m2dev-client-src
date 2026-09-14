# C3-X Compiler-/LP64-/ABI-Audit

Stand: 2026-09-14. Ergänzung zum [C3-X Abschlussbericht](phase-c3x-cross-platform-android-prep.md).
Dieser Audit trennt den portablen Bootstrap vom weiterhin Windows-gebundenen Vollclient.
Eine erfolgreiche portable Prüfung beweist weder einen Android-Vollclient noch eine Android-Runtime.

## 1. Compiler-Portabilität

Bestandsaufnahme mit `rg` in `src`, nur `.h`/`.cpp`, ohne generierte `src/PythonModules/**`.
Die Zahlen zählen Trefferzeilen einschließlich Kommentaren und deaktivierter Zweige, keine
aktiven Compilerfehler. Third-Party-Header/Binaries werden separat bewertet.

| Muster | Trefferzeilen |
| --- | ---: |
| `__declspec` | 2 |
| `__pragma` | 0 |
| `__forceinline` | 24 |
| `__int8` / `__int16` / `__int32` | jeweils 0 |
| `__int64` | 1 |
| `__asm` / `asm` | 0 |
| `_MSC_VER` | 32 |
| `__try` / `__except` / `__finally` | 0 |
| geprüfte Secure-CRT-Namen (`_snprintf_s`, `_vsnprintf_s`, `fopen_s`, `strcpy_s`, `sprintf_s`, `strcat_s`, `localtime_s`) | 22 |
| `_stricmp` | 5 |
| `_snprintf` | 56 |
| `_access` / `_mkdir` / `_getcwd` / `_findfirst` | 9 |
| `#pragma comment` | 5 |
| `long` | 441 |

Die Intrinsic-Prüfung ist zusätzlich nötig: fehlende Inline-Assembler-Treffer bedeuten keine
ARM64-Portabilität. `EterGrnLib/Deform.cpp` enthält SSE-Typen und `_mm_*`; `EterLib/GrpDevice.cpp`
verwendet `<intrin.h>` und `__cpuid`. Beide bleiben im Windows-Vollclient.

| Klasse | Bedeutung | Konkrete Belege und Behandlung |
| --- | --- | --- |
| A | portabel | `EterLib/ControlPackets.h`: feste Integerbreiten; `PacketReader.h`/`PacketWriter.h`: `memcpy` statt unaligned Integer-Dereferenzierung; `Platform/NativeTypes.h`: `void*`; `PlatformFilesystem.h`: `uint64_t`-Offsets, `size_t`-Pufferlängen; `Renderer/SkinningData.h`: Standard-C++ und feste GPU-Feldbreiten. Diese Header sind Bestandteil des portablen ABI-Tests. |
| B | Wrapper oder gezielte Anpassung nötig | `EterBase/Singleton.h`, `Utils.h`, `UserInterface/AbstractSingleton.h`: `__forceinline`; `EterBase/FileLoader.cpp:271`: `__int64`/`_ftelli64`; `EterLib/PathStack.cpp:19,27`: `_getcwd`; `UserInterface/Locale.cpp:114`: `_stricmp`; `PRTerrainLib/TextureSet.cpp:64,82`: `_snprintf`. Erst beim Portieren dieser Module durch passende Standard-/Platform-Funktionen ersetzen. |
| C | bewusst Windows-only | `UserInterface/UserInterface.cpp:40,41`: GPU-Auswahlexporte mit `__declspec(dllexport)`; `EterLib/IME.h:7`: `imm32.lib`; `Platform/Windows/*`: HWND, HANDLE, DWORD, WinSock, Shell, Clipboard, Registry; Windows-Video und WebView. Nicht Teil des Android/Common-Bootstraps. |
| D | Dependency-/Architektur-Blocker | `EterGrnLib/Deform.cpp`: SSE-Code und Granny-Typen; `extern/include/SpeedTreeRT.h:26-34`: optionales Windows-DLL-Import/Export; `extern/include/radtypes.h` enthält Android/AArch64-Zweige, ein Headerzweig liefert aber keine ARM64-Runtime-Binary. Siehe Dependency-Matrix im Hauptbericht. Keine Änderungen an Granny/Skinning. |
| E | Kommentar / inaktiver Altbestand | `EterBase/StdAfx.h:33,34`: auskommentierte `_access_s`-Makros; `UserInterface/PythonItemModule.cpp:91`: auskommentiertes `_snprintf`; `GameLib/ItemData.h` auskommentierter alter Tabellenentwurf mit `long`. Nicht als aktive Portierungsblocker zählen und nicht pauschal löschen. |

`_MSC_VER` wird im Altbestand vielfach nur für `#pragma once` oder Warnungen genutzt.
Andere Zweige ändern reale Semantik, etwa CRT-Namensmakros in `EterBase/StdAfx.h:27-36`.
Die rein textuelle Kategorie muss daher je Aufrufpfad geprüft werden.

## 2. LLP64 gegenüber LP64

| Typ / Annahme | Windows x64 (LLP64) | Android ARM64 / Cygwin x64 (LP64) | Regel |
| --- | ---: | ---: | --- |
| `void*`, `uintptr_t`, `intptr_t` | 8 | 8 | Native Handles/Pointer nicht über `long` transportieren. |
| `size_t`, `ptrdiff_t` | 8 | 8 | Speicherlängen/Indizes; Addition vor einer Grenzprüfung kann dennoch überlaufen. |
| `int` | 4 | 4 | Feste Datenformate verwenden ausdrücklich `int32_t`. |
| `long` | 4 | 8 | Niemals als implizite Wire-/Disk-Breite behandeln. |
| `uint32_t` / `int32_t` | 4 | 4 | 32-Bit-Felder bleiben binär identisch. |
| `uint64_t` / `int64_t` | 8 | 8 | Große Datei-/Pack-Offsets. |
| `DWORD` | 4 | kein portabler Plattformtyp | Kein neues Android-`typedef unsigned long DWORD`; verbleibende Nutzung im Vollclient bleibt Portierungsarbeit. |

### In C3-X konkret behoben

* `PackLib/PackFormat.h` enthält die unveränderten Pack-Disk-Strukturen ohne Sodium- oder
  Container-Abhängigkeiten. Die bisher bereits festen Breiten bleiben bestehen: Header 40 Byte,
  Indexeintrag 310 Byte, jeweils Alignment 1. `config.h` bindet den Header ein und prüft die
  bisherigen Crypto-Key-/Nonce-Größen gegen Sodium. Kein neues Dateiformat, keine Migration.
* `PRTerrainLib/Terrain.h`: Wasserhöhen speichern über `TerrainFormat::WaterHeight`
  ausdrücklich `std::int32_t` statt `long`. Der kleine produktiv verwendete Decoder
  `WaterHeightFormat.h` prüft die exakte Quelllänge vor einer 16-/32-Bit-Konvertierung.
  Der Wasser-Dateipfad behält Dateigrößen als `size_t` ohne DWORD-Verengung.
  Windows behält 4 Byte pro Höhe; LP64 liest ebenfalls die bestehenden 4-Byte-Diskwerte.
  Alle weiteren Zugriffe wurden geprüft: drei Zuweisungen nach `long` in `GameLib/AreaTerrain.cpp`
  bleiben werttreu; kein Aufrufer nimmt einen `long*` auf dieses Array.
* Im selben Wasser-Ladepfad wurde ein vorhandener Fehler sichtbar: Die alte 16-Bit-Variante
  konvertierte ihre Werte und fiel anschließend in eine 32-Bit-`memcpy` durch. Sie endet jetzt
  im Decoder nach der erfolgreichen Konvertierung. Dadurch gibt es kein Überlesen der halb so
  großen Datei und kein Überschreiben der konvertierten Höhen. Rendering-/Wasser-Mathematik
  bleibt unverändert.
* `PacketReader`/`PacketWriter`: Grenzprüfungen verwenden Restlängen bzw. prüfen den Offset
  vor der Subtraktion. Ein `SIZE_MAX`-Argument kann nicht über `position + length` auf einen
  kleinen erlaubten Wert umbrechen. Stringbreite 0 wird abgelehnt, leere Byteoperationen
  sind erlaubt, ein Nullstring kann unverändert als Nullpadding geschrieben werden.
  Die Tests vergleichen bestehende Little-Endian-Wirebytes; kein Protokoll wurde geändert.

### Verbleibende Risiken außerhalb des Bootstraps

* `UserInterface/Packet.h` hat nach C1-X keine aktiven `long`-Felder, hängt aber noch an
  `StdAfx.h`/Gameplay-Headern. Der bestehende Windows-Test deckt zusätzliche Gameplay-Pakete ab.
* `GameLib/ItemData.h` hat bereits `int32_t` für Limits/Applies/Values/Sockets, aber noch
  Windows-Typen wie `DWORD`. Der vollständige Tabellenheader zieht über `Thing.h` Granny ein;
  diese Dependency wird nicht für einen vermeintlich portablen Pakettest versteckt.
* `EterLib/GrpBase.h:29-34`: `TDepth` überlagert `float`, `long`, `DWORD`. Auf LP64 würde der
  `long`-Anteil Größe/Alignment verändern. Legacy-Vertexdaten müssen vor Portierung dieses
  Moduls separat gehärtet werden. Der Android-Clear verwendet diese Struktur nicht.
* `PRTerrainLib/TerrainType.h`, Terrain-/Map-/UI-Zähler und Python-`long`-Brücken bleiben
  Bestand des Windows-Vollclients. Keine globale Ersetzung von `long`: Python-C-API-Aufrufe
  wie `PyLong_AsLong` sind tatsächlich C-`long` und brauchen später Wertebereichsentscheidungen.
* `PlatformWebView.h:7` transportiert einen Windows-HRESULT als `long`. Die WebView-Funktion
  ist Windows-only und bleibt aus Android/Common heraus; vor einer gemeinsamen WebView-API
  muss der Ergebnistyp ausdrücklich definiert werden.
* `EterLib/NetStream.cpp:537,564,565,594` dereferenziert Header als `uint16_t*`. Das ist noch
  ein Alignment-Risiko im Vollclient; der portable `PacketReader` arbeitet mit `memcpy`.
* Android ARM64 und die geprüften Windows/Cygwin-Ziele sind Little Endian. Der neue Test
  lehnt Big Endian ausdrücklich ab. Raw-Struct-/Native-Endianness-Serialisierung wurde nicht
  als beliebig endian-portabel ausgegeben.

## 3. ABI- und Laufzeittests

`tests/Platform/PortableAbiTest.cpp` ist ein Standard-C++20-Programm ohne Python-, Windows-,
Sodium-, Granny- oder Diligent-Linkabhängigkeit. Es verwendet echte Produktionsheader.

| Prüfung | Vertrag |
| --- | --- |
| Datenmodell | Pointer/size_t/intptr_t 64 Bit; `long` 32 auf Windows, 64 auf LP64; IEEE-754 Float32; Little Endian. |
| Control-Pakete | Phase 5, Ping 8, Pong 4, Challenge 72, Response 68, Complete 76 Byte; Alignment 1 und Feldpositionen. |
| Pack-Datei | Header 40, Eintrag 310 Byte; Alignment 1; feste 64-Bit-Offsets/Größen; bekannte Bytes und Werte über 4 GiB. |
| Wasser-Datei | Produktiver Decoder mit modernen signed32-Golden-Bytes und legacy unsigned16-Werten; unaligned Quelle, negative/hohe Werte, Guards, zu kurze/zu lange/leere Quelle. Fehler verändern keine Ausgabe. |
| Renderer | SkinningVertex 40/Alignment 4, Matrix 64/Alignment 4, Materialgruppe 12; Boneindizes 8/16 Bit; Mesh-/Vertexoffset 32 Bit. Keine Skinning-Codeänderung. |
| Native Handles | Window/Cursor/Socket 8 Byte; NativeMessage 32/Alignment 8; wParam/lParam bei 16/24; nullptr und gültige Socket-0-Semantik. |
| Pointerroundtrip | Lokaler Pointer, nullptr und Wert oberhalb 32 Bit durch uintptr_t und NativeMessage. Synthetische Pointer werden nicht dereferenziert. |
| Serialisierung | Golden Bytes für U16/U32/I32/U64/Float32 und Kontrolle an bewusst ungerader Pufferadresse. |
| Grenzfälle | SIZE_MAX-Längen/Offsets, verworfene Operationen ändern weder Cursor noch Ziel, Stringbreite 0, Nullpadding, leere Byteoperationen. Kein Fuzzer. |

Der Windows-`Platform.AbiLayouts`-Test bleibt für Item-/Gameplay-Strukturen erhalten;
`Platform.PointerRoundTrip` bleibt für Windows/Python-Handles erhalten. Das portable Programm
ergänzt diese Verträge, ohne Windows-/Python-Layouts auf Android vorzutäuschen.

Die konkreten Compiler-/CTest-Ergebnisse stehen im Hauptbericht. Cygwin-LP64 ist ein echter
zweiter Compiler-/Datenmodellnachweis; es ist kein Ersatz für NDK-Link/APK/Device-Tests.
Der erweiterte PortableAbi-Test einschließlich des produktiven Wasser-Decoders wurde mit
Cygwin GCC 12.4.0 neu gebaut und ausgeführt: PASS, `pointer=8 long=8 size_t=8`, Little Endian,
keine Compilerwarnungen. Lokale Evidenz: `build-c3x/cygwin-common-water-build.log` und
`build-c3x/cygwin-common-water-test.log` im ignorierten Buildverzeichnis.

## 4. Noch offene Platform-Grenzen

Networking: C2-X abstrahiert Startup/Shutdown/Close/LastError/WouldBlock und IPv4-Auflösung.
`EterLib/NetStream.cpp` benötigt weiterhin Windows::ToSocket/FromSocket, direkte
`recv`/`send`/`select`, `SOCKET_ERROR`, `ioctlsocket(FIONBIO)` und WinSock-Adresstypen.
Für POSIX fehlen ein int-FD-Adapter, errno/EAGAIN/EWOULDBLOCK, fcntl(O_NONBLOCK),
das korrekte `select(maxFd + 1, ...)`, passende Größen-/Returntypen und Send-Signalbehandlung.
NativeSocket speichert breit genug, ist aber allein noch keine POSIX-Netzwerkimplementation.

Windows-Filesystem/Dynamic-Library-Implementationen kapseln HANDLE/HMODULE hinter
`uintptr_t`; diese Datei-Handles sind kein Diskformat. Der Android-Bootstrap benötigt keine
vollständige Client-DLL- oder Socketintegration. Eine spätere `.so`-Implementierung muss
`dlopen`/`dlsym`/`dlclose` und die tatsächlichen Android-Ladepfade verwenden.

Win32-Thread-/Event-/Critical-Section-Nutzung des Vollclients muss beim Einbeziehen der
jeweiligen Module getrennt werden. Die öffentlichen C2-X-Platform-Header enthalten keine
Win32-Threadtypen; der neue ABI-Test benötigt ausschließlich Standard-C++.

## 5. Geltungsgrenze

C3-X prüft einen kleinen Common-Teil mit GCC/LP64 und bereitet den Android-Bootstrap strukturell vor. Der Audit erklärt bewusst
verbleibende Vollclient-Blocker und behauptet keinen vollständig LP64-sauberen Metin2-Client.
Keine Granny-Ersetzung, kein Python-Upgrade, keine Asset-Runtime-Migration, keine Änderung an
Bone-Palette, Skinning-Mathematik, Actor-Material oder Hair LOD wurde hier vorgenommen.
