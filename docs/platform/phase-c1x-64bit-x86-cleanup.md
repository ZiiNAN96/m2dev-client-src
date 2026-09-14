# Phase C1-X – 64-Bit Audit & x86 Cleanup

Stand: 2026-09-13  
Status: C1-X-Quellcode-, x64-Build- und Binary-Audit abgeschlossen; die verpflichtenden authentifizierten Runtime-Gates sind blockiert beziehungsweise ungeprüft. Daher bleibt die Freigabe für C2-X ein `NO-GO`.

Dieser Bericht beschreibt ausschließlich den Phase-C-Block C1-X. Er startet weder Android-, Vulkan-, Asset-Runtime-, Granny-Ersatz-, Visual-Remaster- noch Gameplay-Arbeit. Als historische Ausgangsevidenz gelten die vom Auftrag vorgegebenen Phase-B-Ergebnisse; sie werden nicht als neue C1-X-Testergebnisse ausgegeben.

Statusnotation:

- `BESTÄTIGT`: im Quellstand, Buildsystem oder untersuchten Binärartefakt direkt nachgewiesen.
- `UNGEPRÜFT`: ohne belastbare Runtime-Evidenz; kein positives Ergebnis wird abgeleitet.
- `BLOCKIERT`: in diesem Lauf aus einem konkret protokollierten Grund nicht sicher ausführbar.
- `RISIKO`: bekannte Grenze, die vor einer Freigabe berücksichtigt werden muss.

## 1. Build-Architektur vorher/nachher

| Bereich | Vor C1-X | C1-X-Stand |
|---|---|---|
| Zielarchitektur | Vorhandene Client- und Testartefakte waren bereits PE32+ AMD64. Die CMake-Konfiguration erzwang die Architektur jedoch nicht; Dokumentation und CI verwendeten eine generische Konfiguration ohne `-A x64`. | CMake verlangt `sizeof(void*) == 8` und bei MSVC ausdrücklich die Compilerarchitektur `x64`. Eine Win32-Konfiguration bricht mit einer erklärenden Fehlermeldung ab. README und Windows-CI konfigurieren ausdrücklich mit `-A x64`. |
| Toolchain | Visual Studio 2022, Toolset v143, MSVC 19.44 und Windows SDK 10.0.26100 wurden für den Audit-Build erkannt. | Unverändert; C1-X ändert weder Compilerfamilie noch Sprachstandard. |
| Konfigurationen | `Debug`, `RelWithDebInfo` und `Release`. | Die lokalen Multi-Config-Ziele bleiben erhalten. Die CI-Matrix baut `Debug` und `Release`, da diese beiden Konfigurationen die C1-X-Pflicht-Gates bilden. |
| Runtime Library | MSVC `MultiThreadedDebug` (`/MTd`) in Debug, `MultiThreaded` (`/MT`) in den übrigen Konfigurationen. | Unverändert und weiterhin einheitlich für Client und eingebundene Diligent-Ziele vorgesehen. |
| relevante Defines | Unter anderem `WIN32_LEAN_AND_MEAN`, `BUILDING_GRANNY_STATIC` und `GRANNY_THREADED`; `_WIN64` kommt bei einem echten MSVC-x64-Ziel vom Compiler. | Keine künstliche „fake-x64“-Definition. Die Architektur wird durch Generator, Pointerbreite und Compiler-Architektur geprüft. |
| Architekturwarnungen | Keine projektspezifische harte Schranke gegen Pointer-Verkleinerungen. | Die First-Party-Ziele behandeln MSVC C4302, C4311 und C4312 als Fehler. |

Alle im aktuellen Client-Linkpfad geprüften vorgebauten Bibliotheken liegen als x64-Varianten vor. Damit blockiert keine dieser Bibliotheken den Windows-x64-Build. Umgekehrt sind diese Artefakte keine belastbare Win32- oder ARM64-Basis; C1-X erklärt deshalb x64 zum einzigen unterstützten Client-ABI.

Negativtest: Eine frische VS2022-Konfiguration mit `-A Win32` endet gezielt in `CMakeLists.txt` mit „m2dev-client supports only the x64 ABI“ (Exitcode 1). Damit scheitert x86 früh und erklärend statt erst beim Linken.

## 2. Pointer/Integer-Fundstellen

Die Fundstellen wurden in die verlangten Klassen eingeordnet. Die Tabelle zeigt repräsentative produktive Treffer und die nach dem Fix verbleibende Bewertung.

| Fundstelle/Muster | Klasse vor Fix | Bewertung und Maßnahme |
|---|---:|---|
| `TEnvironmentData*` wurde als Terrain-Lichtversion in `DWORD` abgelegt | C | Auf `std::uintptr_t` umgestellt und über `MapManager`, `MapOutdoor` und `TerrainPatch` durchgängig pointerbreit geführt. |
| 64-Bit-Modulcallback wurde über einen Cast mit 32-Bit-`ULONG`-Basis und `DWORD`-Adresse aufgerufen | C | Callback exakt auf `DWORD64` korrigiert; der unsichere Funktionszeigercast entfällt. |
| Python-Bridge schrieb über einen `unsigned long long*`-Type-Pun in `T**` und übergab Pointer an das Vararg-Format `"K"` | C | Durch `PyLong_AsVoidPtr` und `PyLong_FromVoidPtr` ersetzt; der Pointer-Roundtrip wird separat getestet. |
| `HKL` wurde für Sprach-/IME-Masken nach `DWORD` verkleinert | B | Die beabsichtigte Bitprüfung erfolgt jetzt über `ULONG_PTR`; der Sprachanteil wird mit `LOWORD` extrahiert. |
| `size_t` wurde für Named Pipes, Discord-Frames und Winsock ungeprüft nach `DWORD`, `uint32_t` oder `int` konvertiert | B | Vor Konvertierungen gelten Obergrenzen; Winsock erhält maximal `INT_MAX`, Pipe-Aufrufe maximal `DWORD_MAX`, Discord-Frames maximal ihre feste Nutzlast. |
| Guild-Symbolgröße wurde aus `long` ohne vollständige Protokollgrenze in `uint16_t` übernommen | B | Dateilesung wird vollständig geprüft; die Gesamtpaketlänge darf `uint16_t` nicht überschreiten. |
| `std::string::npos` wurde über `int` und Negativvergleiche behandelt | B | Suchpositionen sind nun `std::string::size_type` und werden explizit gegen `npos` geprüft. |
| Windows-Handles (`HWND`, `HANDLE`, `HINSTANCE`) in den geprüften produktiven Pfaden | A | Native Handletypen bleiben erhalten; es wurde keine produktive Ablage in 32-Bit-Integern nachgewiesen. |
| Pointer als Schlüssel in Maps/Hashes | A | Die geprüften Schlüssel bleiben echte Pointer (`const void*`) und nutzen pointerbreite Standard-Hashes. |
| Diligent-Buffergrößen und native Ressourcenobjekte | A | Größen bleiben in den Diligent-/Renderer-Typen; Downcasts an 32-Bit-Grenzen werden vorab validiert. |

Nach dem C1-X-Quellcodeaudit ist keine bekannte produktive Klasse-C-Fundstelle im bearbeiteten Clientpfad offen. Die vollständigen Release- und Debug-x64-Builds bestätigen außerdem, dass die als Fehler aktivierten Pointerdiagnosen C4302, C4311 und C4312 in den First-Party-Zielen nicht mehr anschlagen.

## 3. Behobene Truncations

Folgende konkrete Breitenfehler oder ungesicherte Verkleinerungen wurden minimal korrigiert:

- Terrain-Lichtcache: Pointeridentität wird nicht mehr auf 32 Bit abgeschnitten.
- Crash-Modulauflösung: Stackadresse und Modulbasis bleiben `DWORD64`; die Bereichsprüfung verwendet ein überlaufsicheres, halboffenes Intervall.
- Python-C++-Bridge: Pointer werden über die dafür vorgesehenen CPython-APIs kodiert und dekodiert; die bisherigen `"K"`-Pointer-Varargs und der Pointer-Type-Pun entfallen.
- Python-Zahlkonvertierungen: `int`, `unsigned int`, `BYTE` und `WORD` erhalten Bereichs- und Python-Fehlerprüfungen statt stiller Verkleinerung.
- Python-Bytefolgen: Längen für `"s#"` werden erst nach Prüfung nach `Py_ssize_t` überführt.
- Netzwerk-/IPC-Grenzen: Named-Pipe-, Discord-, Winsock- und Guild-Symbol-Längen werden vor dem schmaleren API-/Protokolltyp geprüft.
- Dateipfade und Container: Stringpositionen, Packgrößen, Streamoffsets sowie Eintragsanzahlen werden ohne ungeprüfte `size_t`-Downcasts verarbeitet.
- Legacy-Resource-API: Packdateigrößen werden in synchronen und asynchronen Ladepfaden vor `OnLoad(int, ...)` gegen `INT_MAX` geprüft. Subimage-Pfade verwenden `string::size_type`/`npos` und einen dynamischen String statt eines festen 256-Byte-Zielpuffers.
- GPU-Skinning/Granny: kumulierte Vertex-/Index- und Deform-Offsets werden vor der Übergabe an 32-Bit-Rendererfelder gegen Überlauf geprüft.

Es erfolgte ausdrücklich keine globale Umstellung aller `int`, `DWORD` oder `long`; geändert wurden nur Stellen mit ABI-, Pointer- oder Größenbezug.

## 4. Struct-/ABI-Audit

Windows x64 verwendet LLP64: Pointer und `size_t` sind 64 Bit breit, `long` und `DWORD` bleiben 32 Bit. Genau diese Differenz war für Pointer-Casts kritisch, während ein pauschales Ersetzen von `long` bestehende Binärlayouts gefährdet hätte.

Ergebnisse:

- In den zentral geprüften gepackten Client-Wire- und Packstrukturen wurden keine Pointerfelder gefunden.
- Die serialisierten Felder von `CItemData::TItemLimit`, `TItemApply` und `TItemTable` verwenden nun ausdrücklich `int32_t` statt ABI-abhängig formulierter `long`-Semantik. Das bestehende Windows-Binärausmaß bleibt 236 Byte.
- `TPlayerSkill::tNextRead` und `TPacketGCTime::time` sind auf `int64_t` festgelegt. Im aktuellen x64-Build bleibt die erwartete Größe erhalten; das Layout hängt nicht mehr von einer `time_t`-Option ab.
- `#pragma pack(1)` bleibt nur an den bestehenden binären Grenzen aktiv. Die neuen Tests sichern Größen und Offsets, nicht interne Implementierungsobjekte.
- Es wurde keine neue Raw-Struct-Serialisierung eingeführt und kein bestehendes Dateiformat migriert.
- Die geprüften Wire-/Dateitypen enthalten weder virtuelle Funktionen noch Pointer. Vorhandene Unions und Bitfelder wurden nicht als rohe pointerabhängige Persistenzgrenze identifiziert; ihre 32-Bit-Felder bleiben ausdrücklich Teil des bestehenden Formats.
- `memcpy`/`memset` bleibt an trivialen Packet-, Tabellen- und Bytegrenzen zulässig. Ein neues Kopieren polymorpher oder nicht trivialer C++-Objekte wurde nicht eingeführt; die irrtümliche Übergabe des nicht trivialen `SGradeData` an eine Python-Vararg-Funktion wurde entfernt.
- Player-/Item-, Pack- und ausgewählte Netzwerkstrukturen werden im Client statisch geprüft. DB- und Save/Load-Gegenstellen außerhalb dieses Windows-Clients wurden inventarisiert, aber nicht verändert.

Der relevante x86/x64-Unterschied ist LLP64: Pointer, `size_t`, `WPARAM` und `LPARAM` wachsen auf 64 Bit, `int`, `long` und `DWORD` dagegen nicht. Die kritischen externen Layouts enthalten keine solchen Pointertypen und behalten daher die dokumentierten Größen; interne polymorphe Klassen sind ausdrücklich keine Raw-Serialization-Formate.

`RISIKO`: Die Gegenstellen im Linux-Server verwenden an einzelnen Stellen weiterhin LP64-empfindliche `long`-/`time_t`-Annahmen. Diese liegen außerhalb des Client-C1-X-Diffs und werden in Abschnitt 34 gesondert benannt.

## 5. Packet Layouts

Die neue ABI-Testdatei sichert derzeit folgende Client-Protokollgrenzen zur Compilezeit:

| Typ | Erwartung |
|---|---|
| `TItemPos` | Größe 3; `window_type` Offset 0; `cell` Offset 1 |
| `TPlayerItemAttribute` | Größe 3; `sValue` Offset 1 |
| `TItemData` | Größe 46; `flags` Offset 5; `aAttr` Offset 25 |
| `TPacketGCPhase` | Größe 5; `phase` Offset 4 |
| `TPacketGCKeyChallenge` | Größe 72; `server_time` Offset 68 |
| `TPacketGCKeyComplete` | Größe 76; `nonce` Offset 52 |
| `TPacketCGMarkLogin` | Größe 12; `handle` Offset 4 |
| `TPlayerSkill` | Größe 10; `tNextRead` Offset 2 |
| `TPacketGCTime` | Größe 12; `time` Offset 4 |

Die Umstellung der Zeitfelder verändert das erwartete x64-Wirelayout nicht. Alle Layout-Asserts kompilieren in Release und Debug. Eine End-to-End-Protokollprüfung gegen den aktuellen Server sowie World-/Relog-Verkehr blieb mangels erreichbarem Server und Testanmeldung `UNGEPRÜFT`.

## 6. File Layouts

Das bestehende Packformat bleibt unverändert und wird explizit abgesichert:

| Typ/Feld | Erwartung |
|---|---|
| `TPackFileHeader` | Größe 40; `entry_num` Offset 0; `data_begin` Offset 8; `nonce` Offset 16 |
| `TPackFileEntry` | Größe 310 |
| `file_name` | Offset 0, feste Kapazität 261 Byte inklusive Terminator |
| `offset` | Offset 261, `uint64_t` |
| `file_size` | Offset 269, `uint64_t` |
| `compressed_size` | Offset 277, `uint64_t` |
| `encryption` | Offset 285 |
| `nonce` | Offset 286 |

Loader und PackMaker prüfen Indexmultiplikationen, Datei-/Streamgrenzen, NUL-Terminierung, Pfadlängen, Lese-/Schreibresultate, Kompressionsgrößen und Offsetadditionen. Allokationsfehler in Packindex, Dekompressionsziel, verschlüsseltem Zwischenpuffer und losem Disk-Fallback werden zu einem sauberen `false`; Teilergebnisse werden geleert. Fehlerhafte oder abgeschnittene Packdaten werden abgewiesen. Item- und Mob-Proto-LZO-Daten prüfen äußere Header, komprimierte Eingabegröße, erwartete dekomprimierte Elementgröße und terminierte Textfelder, bevor Strukturen kopiert werden. Verschlüsselte LZO-Längen müssen zusätzlich 8-Byte-ausgerichtet und als `int` darstellbar sein, bevor die bestehende Crypto-API aufgerufen wird.

Alle 91 im Runtime-Bestand vorhandenen Packs wurden gegen die feste Header-/Entry-Geometrie geprüft. Das berechnete `data_begin` und die vorhandenen Einträge stimmen mit dem beibehaltenen Layout überein; C1-X verlangt keine Packmigration.

Der allgemeine `CDiskFileLoader` besitzt weiterhin eine absichtliche `int`-Schnittstelle. Er verwendet zwar `_fseeki64`/`_ftelli64`, lehnt Dateien über `INT_MAX` aber explizit ab. Das ist eine dokumentierte Altformat-/API-Grenze; C1-X baut kein neues Großdatei- oder Packformat.

## 7. Windows Handle Audit

- `HWND`, `HANDLE`, `HINSTANCE`, `WPARAM`, `LPARAM`, `LRESULT` und `SOCKET` bleiben in den geprüften Pfaden in ihren nativen Windows-Typen.
- Der Modulenumerationscallback entspricht nun exakt der 64-Bit-Signatur von `EnumerateLoadedModules64`; ein ABI-widriger Funktionszeigercast ist entfernt.
- IME-Layoutwerte werden über `ULONG_PTR` behandelt. Nur die beabsichtigten Sprach-/Kennungsbits werden anschließend extrahiert.
- Named Pipes verwenden `DWORD` nur an der WinAPI-Grenze und erst nach einer Obergrenzenprüfung.
- Winsock verwendet den vorgeschriebenen `int`-Längenparameter mit einem geprüften Maximalblock statt eines unkontrollierten `size_t`-Casts.

Es wurde keine 32-Bit-Ablage eines produktiven Window- oder Kernel-Handles festgestellt.

## 8. Memory/Allocator Audit

Der bestehende `CDynamicPool` bleibt konzeptionell erhalten, wurde aber gegen Größenüberlauf und inkonsistenten Zustand gehärtet:

- Chunkanzahl, Chunkgröße, `sizeof(T)`-Multiplikation sowie Vector-`max_size()` werden vor der Allokation geprüft.
- Ein fehlgeschlagenes `malloc` führt zu `std::bad_alloc` statt zur Aufnahme eines Nullchunks.
- Wirft der Placement-New-Konstruktor, wird der Slot wieder in die Freiliste gestellt.
- Pack-, Proto- und LZO-Pfade prüfen Prozess-/Containergrenzen; eine fehlschlagende Dekompressionsallokation wird sauber abgewiesen.
- Pointerarithmetik in Datei- und Packgrenzen verwendet Subtraktionsprüfungen oder geprüfte 64-Bit-Addition, sodass die Validierung nicht selbst überläuft.
- Actor-/Effect-Pools verwenden denselben gehärteten Poolmechanismus; im Audit wurde keine separate 32-Bit-Pointerablage in ihren Freilisten gefunden.
- Granny-Speicher bleibt Eigentum der bestehenden Granny-API. An der Clientgrenze werden negative oder nicht darstellbare Mesh-/Bone-/Vertex-/Indexzahlen abgewiesen, bevor Rendererressourcen angelegt werden.
- Diligent-Ressourcen bleiben Referenz-/Interfaceobjekte; Größen und Offsets werden vor 32-Bit-API-Feldern geprüft. Pythonobjekte bleiben unter CPython-Referenzzählung, Pointerwerte werden nicht als schmale Integer allokiert oder gespeichert.

Keine neue Allocatorarchitektur wurde eingeführt. Die malloc-basierte Poolausrichtung bleibt eine dokumentierte Designgrenze für hypothetische künftig über-ausgerichtete Typen; im geprüften Bestand wurde daraus kein produktiver C1-X-Fehler abgeleitet.

## 9. Python Bridge

Die Python-Version bleibt unverändert. C1-X beseitigt die Pointer-as-Integer-Vararg-Hacks:

- `PyTuple_GetPointer` nutzt `PyLong_AsVoidPtr` und prüft Python-Fehler.
- `Py_BuildPointer` nutzt `PyLong_FromVoidPtr`.
- Fenster-, Bild-, Text-, Thing-, Item-, Skill-, Quest- und Textloader-Bindings verwenden diese gemeinsame Pointergrenze.
- Der Quest-Rückgabewert verwendet `"N"` für das bereits neu erzeugte Pythonobjekt und wahrt damit die Referenzbesitz-Semantik.
- Die früheren Makroumleitungen von `PyLong_AsLong` auf `PyLong_AsLongLong` und von `PyLong_AsUnsignedLong` auf die 64-Bit-Variante sind entfernt.
- Schmale Ergebnisparameter werden nur nach expliziter Bereichsprüfung beschrieben.
- `s#`-Längen werden als `Py_ssize_t` übergeben und zuvor gegen dessen Maximalwert geprüft.
- Zeitwerte werden mit `PyLong_FromLongLong` gebaut; echte None-Rückgaben verwenden `Py_BuildNone` statt des ungültigen Formats `"None"`.
- Die zusammengesetzte Quest-Rückgabe verwendet das passende Format `sNsi`. `skillGetGradeData` gibt den bereits bestehenden einzelnen `wMotionIndex`-Integer zurück, statt ein nicht triviales `SGradeData`-Objekt illegal durch Varargs zu reichen; außerdem wurde das überzählige Exception-Argument entfernt. Im Projektbestand wurde kein Asset-Script-Aufrufer dieser Bindung gefunden.

Der neue Plattformtest führt einen echten CPython-Pointerroundtrip sowohl mit einer realen Adresse als auch mit einem synthetischen Wert oberhalb von 32 Bit durch. Er lief als Einzeltest in Release und Debug mit Exitcode 0. Da nur die x64-Release-ABI von `python314_static.lib` vorliegt, blendet der Test – wie die bestehende Clientgrenze – `_DEBUG` ausschließlich während des Einbindens von `Python.h` aus. Damit stimmen Header-Inlines und Bibliothek überein; `PyLong_FromVoidPtr` und `PyLong_AsVoidPtr` werden weiterhin real ausgeführt.

`RISIKO`: Der Debug-Gesamtbuild verwendet `/MTd`, die einzige Python-Static-Library `/MT`. Das erklärt den verbleibenden CRT-Linkhinweis; er wird nicht mit `/NODEFAULTLIB` kaschiert. Langfristig braucht der Bestand eine passende x64-Debug-Python-Library oder eine vollständig einheitliche CRT-Kette.

## 10. Granny

- Inventarisierte Version: Granny 2.11.8.
- Einbindung: vorgebaute statische x64-Bibliothek (`granny2_static.lib`), ABI-sensitiv.
- Status für Windows x64: verfügbar; kein x64-Blocker im geprüften Linkpfad.
- C1-X ersetzt oder aktualisiert Granny nicht.

An der Clientgrenze wurden negative Zähler sowie Überläufe bei kumulierten Vertex-, Index-, Rigid- und Deform-Zahlen abgefangen. Die Übergabe an 32-Bit-GPU-Felder erfolgt nur nach einer `uint32_t`-Prüfung. Das stärkt x64-Sicherheit, ohne das Granny-Datenmodell oder die Skinning-Implementierung auszutauschen.

## 11. Externe Libraries

| Komponente | Form/Architektur | ABI-Bewertung |
|---|---|---|
| Granny 2.11.8 | statisch, x64 | ABI-sensitiv; für x64 vorhanden; kein Ersatz in C1-X |
| Python 3.14.3 | `python314_static.lib`, x64 | ABI-sensitiv; Pointerbridge separat gehärtet |
| SpeedTree 1.6 | statisch, x64; getrennte Debug-/Release-Libs | ABI-sensitiv; im x64-Linkpfad verfügbar |
| WebView2Loader | statisch, x64 | im x64-Linkpfad verfügbar |
| DirectInput (`dinput8`, `dxguid`) | x64-Importbibliotheken | Windows-x64 verfügbar |
| Audio (`AudioLib`/miniaudio) | aus Source beziehungsweise Header-Implementation | wird mit dem Client-ABI gebaut; keine separate x86-Audio-DLL im finalen Verzeichnis |
| Video (DirectShow/COM, `strmiids`) | Windows-System-ABI | keine mitgelieferte architekturgebundene Video-DLL; finale Systemmodule sind x64 |
| DiligentCore | aus Source, gepinnter Stand `b036337`, D3D11 | wird für das Ziel-ABI gebaut; kein Backendwechsel |
| zstd 1.5.7 | aus Source, statisch | mit Zieltoolchain gebaut |
| LZO 2.10 | aus Source | mit Zieltoolchain gebaut; Eingabegrößenprüfung ergänzt |
| libsodium | aus Source, statisch | mit Zieltoolchain gebaut |
| FreeType 2.13.3 | aus Source | mit Zieltoolchain gebaut |

Die untersuchten vorgebauten Bibliotheken sind sämtlich x64. Es wurde keine Library gefunden, die den Windows-x64-Client blockiert. In den finalen Build-Verzeichnissen liegen keine lokalen DLLs; sämtliche physisch aufgelösten Systemimporte wurden als x64 bestätigt. Dynamisch durch Windows oder WebView zur Laufzeit nachgeladene Module können erst in einem erfolgreichen Main-Client-Runtime-Lauf vollständig beobachtet werden.

## 12. x86-only ASM/Intrinsics

Im produktiven First-Party-Clientcode wurde kein erforderlicher `__asm`-/Naked-Function-Pfad und kein Calling-Convention-Hook als x64-Blocker gefunden. Der vorhandene SSE-Code in `EterGrnLib/Deform.cpp` ist für den aktuellen Windows-x64-Baselinepfad nutzbar. CPUID-/SSE-Annahmen in der Geräteerkennung bleiben x86/x64-spezifisch.

Für ein späteres ARM64-Ziel wären diese Stellen sowie die vorgebauten Granny-, Python-, SpeedTree-, WebView2- und DirectInput-Artefakte erneut zu lösen. Das ist kein C1-X-Arbeitsauftrag und wurde nicht begonnen.

## 13. Calling Conventions

MSVC x64 verwendet eine vereinheitlichte Plattform-ABI; die alten x86-Schlüsselwörter ändern dort nicht die Übergaberegeln. Geprüft wurden insbesondere WinAPI-, Python-, Granny- und Renderergrenzen.

Der konkrete ABI-Fehler im Crashpfad bestand nicht im Schlüsselwort `CALLBACK`, sondern in den falsch breiten Callbackparametern und dem erzwungenen Funktionszeigercast. Die Signatur ist nun exakt. Für die übrigen geprüften produktiven Callbacks wurde kein x86-only-Aufruftrick nachgewiesen. Es wurden keine Hooks oder Naked-Wrapper neu eingeführt.

## 14. Format Specifiers

- Modellinstanz-Adressen werden als Pointer mit `%p` ausgegeben statt als 32-Bit-Hexwert behandelt.
- Crashadressen bleiben explizit 64 Bit und werden passend formatiert.
- Größen werden an schmalen APIs nicht allein durch ein Format kaschiert, sondern vor der Konvertierung geprüft.
- CPython-`Py_BuildValue`-Formate werden nicht mehr für rohe C++-Pointer missbraucht; Pointer laufen über `PyLong_FromVoidPtr`.
- `s#` erhält die von CPython erwartete `Py_ssize_t`-Länge.
- In `ThingInstance`, `PythonNetworkStreamCommand`, `InstanceBaseEffect` und `DrawState` wurden belegte `%u`-/`%zu`-Typfehler sowie fehlende oder überzählige Vararg-Argumente korrigiert. Die Emoticon-/Affect- und Draw-State-Diagnosen reichen nun genau die vom Format geforderten Werte durch.

Es fand keine mechanische globale Formatstring-Umschreibung statt; geändert wurden nur belegte Pointer-/Größengrenzen.

## 15. Hash/Pointer Keys

Die geprüften pointerbasierten Maps verwenden den Pointertyp selbst. `TextureBinding` hasht den tatsächlichen `const void*` mit `std::hash<const void*>`; es existiert dort kein Zwischenweg über `DWORD` oder `unsigned int`.

Die Terrain-Lichtversion war dagegen keine Hashfunktion, sondern benutzte die Pointeridentität als Cachetoken. Dieser Sonderfall ist nun ausdrücklich `std::uintptr_t`. Es wurden keine neuen pointerbasierten IDs eingeführt.

## 16. Sentinel Values

- `INVALID_HANDLE_VALUE` bleibt in einem nativen `HANDLE`-Feld und wird nicht in einem 32-Bit-Integer gespeichert.
- String-Suchfehler werden als `std::string::npos` behandelt statt über `int < 0` oder einen abgeschnittenen `-1`-Wert.
- Negative Netzwerkgrößen werden vor der Konvertierung nach `size_t` abgewiesen.
- Größen- und Offsetgrenzen verwenden `numeric_limits` des tatsächlichen Zieltyps statt untypisierter `0xFFFFFFFF`-Annahmen.

Im geprüften produktiven Clientpfad blieb kein bekannter Pointer-Sentinel in einem 32-Bit-Feld offen.

## 17. Renderer/Diligent

Diligent D3D11 bleibt der einzige Produktionsrenderer. C1-X fügt weder Vulkan noch einen weiteren Backendpfad hinzu.

- Diligent wird als Source-Abhängigkeit für das ausgewählte x64-Ziel gebaut.
- Renderer-First-Party-Ziele erhalten dieselben harten Pointer-Konvertierungswarnungen wie der übrige Client.
- Native Diligent-Objekte und Ressourcenhandles bleiben in ihren API-Typen; es wurde keine produktive 32-Bit-Pointerablage gefunden.
- Buffergrößen, Strides und Offsets werden an 32-Bit-Datenmodellgrenzen geprüft, statt Pointerbreite mit Ressourcenbreite gleichzusetzen.
- Vorhandene Phase-B-Lifecycle- und GPU-Tests bleiben eingebunden.

**BESTÄTIGT:** Der finale Renderer-Testlauf besteht in Release mit 24/24 und in Debug mit 24/24 Tests.

## 18. GPU Skinning

GPU-Skinning bleibt der vorgesehene Standardpfad; CPU-Skinning bleibt Fallback/Referenz. C1-X verändert diese Priorität nicht.

Härtungen:

- kumulierte Deform-Offsets werden zunächst 64-bittig berechnet;
- negative Granny-Zähler werden verworfen;
- Vertex-, Index-, Rigid- und Deform-Summen werden vor `int`-/`uint32_t`-Übernahme geprüft;
- Mesh- und Bone-Zähler werden erst nach Plausibilitätsprüfung gespeichert;
- Actor-Source-Uploads brechen bei nicht darstellbaren 32-Bit-Zählerwerten kontrolliert ab.
- Statisch geteilte Mesh-/Bone-Ressourcen bleiben über ihre echten Objekt-/Interfaceidentitäten referenziert; im geprüften Sharingpfad wurde kein Pointer in einer 32-Bit-ID abgelegt.

Die automatisierten Renderer-/Skinning-Tests prüfen unter anderem Hair LOD, Parität, Stabilität, Fallback und Benchmark. Die Ingame-Regressionsmatrix für Player, NPC, Mob, Mount, Hair LOD und Shape-Wechsel blieb jedoch `UNGEPRÜFT`; aus Unit-/Harness-Evidenz allein wird keine Live-Welt-Stabilität behauptet.

## 19. Static Asserts

Die neuen Assertions konzentrieren sich auf echte Plattform- und Binärgrenzen:

- Plattform: `sizeof(void*) == 8`, `sizeof(long) == 4`, Pointerbreite von `LONG_PTR`, `ULONG_PTR`, `WPARAM`, `LPARAM`, `LRESULT`, `HANDLE`, `HWND` und `Py_ssize_t`.
- Packformat: Größen und kritische Offsets von `TPackFileHeader` und `TPackFileEntry`.
- Spiel-/Itemdaten: Größen und ausgewählte Offsets von `TItemPos`, `TPlayerItemAttribute`, `TItemData` und `CItemData::TItemTable` samt Unterstrukturen.
- Netzwerk: Größen und Offsets ausgewählter Phase-, Key-, Mark-, Skill- und Zeitpakete.

Damit schlagen unerwartete Pack-, Alignment- oder Toolchainänderungen beim Kompilieren fehl. Interne Klassen ohne externes Binärversprechen wurden nicht mit inflationsartigen Größenasserts eingefroren.

## 20. Compiler Warnings

Die vollständigen x64-Builds des finalen C1-X-Quellstands sind in Release und Debug mit Exitcode 0 abgeschlossen. Der Bestand ist nicht warnungsfrei: MSVC meldet weiterhin ältere Narrowing-, Signed/Unsigned-, Enum-, möglicherweise uninitialisierte sowie Drittanbieterwarnungen. Diese wurden nicht global unterdrückt und nicht durch eine kosmetische Gesamtzahl schöngerechnet.

C1-X macht die für diesen Block entscheidenden MSVC-Diagnosen C4302, C4311 und C4312 in First-Party-Zielen zu Fehlern. Keine dieser Pointer-Konvertierungsdiagnosen blockierte den finalen Release- oder Debug-Build. Die produktiven belegten Vararg-, Größen- und Pointerprobleme wurden gezielt behoben.

Im Debug-Link bleiben nachvollziehbare Bestandsmeldungen:

- LNK4075: inkrementelles Linken wird wegen LTCG ignoriert;
- LNK4098: `/MTd` des Debug-Produkts trifft auf die einzige vorhandene Release-/`/MT`-Python-Static-Library;
- LNK4099: fehlende PDBs in vorgebauten Bibliotheken, unter anderem Python, zlib und SpeedTree.

Diese Warnungen werden nicht mit `/NODEFAULTLIB` oder einer pauschalen Warnungsabschaltung kaschiert. Der Python-CRT-Mix bleibt als technisches Risiko dokumentiert.

## 21. x64 Release Build

Status: `BESTÄTIGT`.

- Configure: Visual Studio 2022 mit explizitem x64-Generator, Toolset v143, MSVC 19.44 und Windows SDK 10.0.26100.
- DiligentCore: lokaler Source-Stand `b036337`, ausschließlich D3D11 für diesen Build.
- vollständiger Release-Build: erfolgreich, Exitcode 0.
- `Metin2_Release.exe`: PE32+, Maschine `0x8664`/AMD64, GUI-Subsystem; SHA-256 `16C07A28566D80F681E8C3B5306F17AF276B26B7D987B40877502D9E1B9E93A7`.
- `DumpProto.exe`: PE32+/AMD64, CUI; SHA-256 `325BCD2452E758C5F533A83DC29517D405AF5ABB06B22B82CC0AA7FCA5DDBDB8`.
- `PackMaker.exe`: PE32+/AMD64, CUI; SHA-256 `D41A6E281B14B0E58571FB18A877A1FBE0BBD3DEF8149CE43A3479170DAB96C5`.

Damit ist die Buildseite ein echter x64-Nachweis und keine durch Defines simulierte Lösung.

## 22. x64 Debug Build

Status: `BESTÄTIGT`.

- Configure: dieselbe echte VS2022-x64-Konfiguration wie Release.
- vollständiger Debug-Build mit `/MTd`: erfolgreich, Exitcode 0; Assertions und ABI-Compilezeitprüfungen wurden gebaut.
- `Metin2_Debug.exe`: PE32+, Maschine `0x8664`/AMD64, GUI-Subsystem; SHA-256 `B37A9CB4F2558D3EB852CAD88B4DAB8A04AF1220F86F8EDCAF7ED1935874975C`.
- `DumpProto.exe`: PE32+/AMD64, CUI; SHA-256 `3A6249AAEBE2EB5E4062AD2CBA94F7E55DA771C303745B5CDFB5A8D0150E6533`.
- `PackMaker.exe`: PE32+/AMD64, CUI; SHA-256 `DBEE05C93B52D8FEA4F4A891383E0D0186F6072116D4DB96769392616378AECB`.
- LNK4075, LNK4098 und LNK4099 bleiben mit den in Abschnitt 20 genannten Ursachen sichtbar; sie verhinderten den Build nicht.

Der Pointertest spiegelt bewusst die Release-ABI-Grenze der einzig vorhandenen Python-Library. Das löst die Header-/Library-Konsistenz für diesen Test, aber nicht das dokumentierte langfristige `/MTd`-gegen-`/MT`-Risiko der Gesamtanwendung.

## 23. Binary Dependency Audit

Status der final gebauten Artefakte: `BESTÄTIGT`.

- Release und Debug von `Metin2`, `DumpProto` und `PackMaker` sind sämtlich PE32+/AMD64. Die vier Tools importieren nur `KERNEL32.dll` und `ADVAPI32.dll`.
- In den finalen `bin`-Verzeichnissen liegen keine lokalen DLLs. Alle 20 physisch aufgelösten DLLs aus dem Importverbund sind x64-System32-Module: `ADVAPI32`, `bcrypt`, `COMDLG32`, `d3d11`, `D3DCOMPILER_47`, `dbghelp`, `DDRAW`, `DINPUT8`, `dxgi`, `GDI32`, `IMM32`, `KERNEL32`, `ole32`, `OLEAUT32`, `SHELL32`, `SHLWAPI`, `USER32`, `VERSION`, `WINMM` und `WS2_32`. `api-ms-win-core-path-l1-1-0` ist ein Windows-API-Set.
- Keine der sechs finalen EXEs importiert `d3d8.dll`, `d3d9.dll` oder eine `d3dx*.dll`.
- Die geprüften vorgebauten Linkbibliotheken für Granny, Python, SpeedTree, WebView2Loader, DirectInput und DXGUID sind x64.

Abgrenzung zum Runtime-Repository:

- Die dort liegenden `Metin2_Release.exe` und `Metin2_Debug.exe` sind zwar x64, aber veraltet und importieren noch D3D9/D3DX9_43. Sie sind keine gültige C1-X-Liefer- oder Runtimeevidenz und wurden nicht überschrieben.
- `config.exe` ist PE32/x86, importiert D3D8 und besitzt in den vier untersuchten Repositories keine auffindbare Source-Gegenstelle. Es läuft als separater WOW64-Helfer und wird als deprecated bewertet; es ist kein Modul des x64-Hauptclientprozesses.

Ohne erfolgreich gestarteten finalen Hauptclient bleibt die Beobachtung optional oder dynamisch nachgeladener Laufzeitmodule `UNGEPRÜFT`. Für die statische Importkette des finalen Builds wurde jedoch keine x86-DLL gefunden.

## 24. Runtime Smoke

Status: `BLOCKIERT`, deshalb `UNGEPRÜFT`.

Beim Runtime-Checkpoint lief bereits ein anderer `metin2client` (PID 66416) samt `m2CefBrowser`. Entsprechend der bestehenden sicheren Testregel wurde weder ein konkurrierender Client gestartet noch der fremde Prozess beendet oder bedient. Zusätzlich waren auf den lokalen Ports 11000, 11011, 11021, 11031 und 11041 keine Server-Listener erreichbar; Test-Zugangsdaten lagen nicht vor.

Die vorhandene private Startup-Harness kann mit einer explizit übergebenen finalen Client-Binary den D3D11-Lifecycle `Initialize → WM_SIZE → Shutdown` und Exitcode 0 prüfen. Sie wurde unter dieser Prozesslage bewusst nicht gestartet und ersetzt ohnehin nicht die Pflichtmatrix:

- Login
- Character Select
- Ingame
- Terrain
- Actors
- GPU Skinning
- UI/Text
- Effects
- Water
- Trees

Ein Start alter Runtime-Binaries, die Offline-Harness oder ein reiner Renderer-Test ersetzt dieses Gate nicht. Login, Character Select, Ingame, Terrain, Actors, GPU-Skinning, UI/Text, Effekte, Wasser und Bäume sind für den finalen Build daher nicht als grün ausgewiesen.

## 25. World Regression

Status: `BLOCKIERT`, deshalb `UNGEPRÜFT`.

Verpflichtende x64-Sequenz:

`A1 → B1 → A1 → Dungeon → Gildenkarte → A1`

Eine private Offline-Harness bildet die Assetfolge `A1 → B1 → A1 → monkeydungeon → guild_01 → A1` ab. Sie wurde wegen des bereits laufenden anderen Clients nicht gestartet; zudem verwendet ihr Setuppfad vorhandene Runtime-Werkzeuge und ist kein authentifizierter Server-Mapwechsel. Zu protokollieren bleiben je echtem Übergang Ladeerfolg, Terrain/Actors/Effekte, GPU-Skinning-Status, unerwartete Fallbacks und Crashfreiheit. Ohne vollständige Live-Sequenz ist dieses Gate nicht grün.

## 26. Relog

Status: `BLOCKIERT`, deshalb `UNGEPRÜFT`.

Verpflichtende Sequenz, sofern mit dem verfügbaren Testkonto reproduzierbar:

`Ingame → Character Select → Ingame`

Der Lauf muss denselben finalen x64-Build wie Abschnitt 24 verwenden. Ohne erreichbaren Server und Anmeldung konnte weder der Netzwerkzustand noch UI-Neuaufbau, Actor-/GPU-Ressourcen oder der erneute saubere Welteintritt geprüft werden.

## 27. GPU Skinning Regression

Status: automatisierte Renderer-/Skinning-Tests siehe Abschnitt 33; Ingame-Matrix `UNGEPRÜFT`.

Unter x64 sind Player, NPC, Mob, Mount, Hair LOD und Shape-Wechsel zu prüfen. Erwartung: GPU-Skinning bleibt Default, kein unerwarteter CPU-Fallback, keine beschädigten Posen/Weights und keine durch die neuen Größenprüfungen ausgelöste Regression bei normalen Assets.

Die statischen Prüfungen und Renderer-Unit-/Smoke-Tests sind notwendige, aber keine hinreichende Ingame-Evidenz.

## 28. Resize

Status: `BLOCKIERT`, deshalb am Main-Client `UNGEPRÜFT`.

Die automatisierten Rendererfälle decken Lifecycle-/Resize-Bausteine ab, aber kein manuelles mehrfaches Resize des authentifizierten Hauptclients. Zu prüfen bleiben Swapchain-/Viewport-Wiederherstellung, fortgesetztes Rendering von Terrain, Actors, UI, Effekten, Wasser und Bäumen sowie zusätzliche Fallbacks oder Ressourcenlecks.

## 29. Minimize/Restore

Status: `BLOCKIERT`, deshalb `UNGEPRÜFT`.

Mehrfaches Minimieren und Wiederherstellen ist separat zu protokollieren. Ein bloßes Resize gilt nicht als Ersatz. Erwartet werden eine fortsetzbare D3D11-/Diligent-Ausgabe, intakte UI-/Textdarstellung und stabile GPU-Skinning-Ressourcen.

## 30. Shutdown

Status: automatisierte Rendererprozesse werden von der Testsuite geprüft; finaler Main-Client-Lauf `BLOCKIERT` und `UNGEPRÜFT`.

Pflichtwerte des finalen x64-Laufs:

- regulärer Exitcode 0;
- überwachte Ressourcen nach Shutdown 0;
- kein architekturspezifischer Crash im Exception-/Modulpfad;
- keine hängenden Client-, Renderer- oder Hilfsprozesse.

Die Phase-B-Ausgangsevidenz „Ressourcen nach Shutdown 0“ und Resultate isolierter Renderer-Harnesses werden nicht ungeprüft auf einen vollständigen C1-X-Main-Client-Lauf übertragen. Weil kein neuer Client gestartet wurde, kann C1-X für den Hauptclient weder Exitcode 0 noch Ressourcen nach Shutdown 0 behaupten.

## 31. Performance Sanity

Status: `UNGEPRÜFT`.

C1-X ist kein Optimierungsprojekt. Der vorhandene Renderer-Benchmark erkennt isolierte grobe Pfadabweichungen, ersetzt aber keinen Vergleich derselben Ingame-Szenen. CPU-/GPU-Frametimes und subjektive Stotterfreiheit gegenüber Phase B konnten ohne Main-Client-Lauf nicht belastbar verglichen werden; Hardware, Szene, Buildkonfiguration und Stichprobe müssen in einem später autorisierten Lauf protokolliert werden.

## 32. x86 Build Recommendation

Empfehlung für den Hauptclient: **C – x86 entfernen beziehungsweise als nicht unterstützt hart ablehnen.**

Begründung:

- Die produktive und geprüfte Bibliothekskette ist x64.
- Die Roadmap benötigt eine eindeutige, reproduzierbare ABI-Basis.
- Eine weiterhin scheinbar zulässige Win32-CMake-Konfiguration würde nur spät beim Linken oder durch abweichende Struct-/`time_t`-Annahmen scheitern.
- CMake, README und CI spiegeln jetzt diese tatsächliche Unterstützung wider.

Separater Sonderfall: `config.exe` sollte vorerst **B – deprecated** behandelt und getrennt vom Hauptclient dokumentiert werden. Es bleibt ein x86/WOW64-Hilfsprogramm, bis Source, Ersatz oder ein verifizierter x64-Neubau verfügbar ist. Dieser Sonderfall rechtfertigt keinen x86-Hauptclient.

## 33. Testsuite

Neu hinzugefügt wurden zwei kleine Plattformtests:

- `Platform.AbiLayouts`: Compilezeitprüfung kritischer Pack-, Packet-, Item- und Windows-x64-Layouts.
- `Platform.PointerRoundTrip`: CPython-Pointerroundtrip inklusive eines Werts oberhalb der 32-Bit-Grenze.

Beide tragen die Labels `platform;x64`. Die Architekturwarnungen werden außerdem auf First-Party-Quellziele und Renderer-Tests vererbt. Bestehende Renderer-, Lifecycle- und GPU-Skinning-Tests bleiben erhalten; es wurden keine relevanten Tests gelöscht.

Die vom Auftrag genannte Phase-B-Baseline beträgt 28/28 Release und 24/24 Debug. Das ist historische Ausgangsevidenz, nicht das C1-X-Endresultat.

Finale C1-X-Ergebnisse:

- Release, vollständige konfigurierte CTest-Suite (30 Tests): 29/30 bestanden in 1.554,33 s; ausschließlich Drittanbieter-`playTests` scheitert am dokumentierten `/cygdrive/...`-Pfad.
- Debug, vollständige konfigurierte CTest-Suite (30 Tests): 29/30 bestanden in 4.626,67 s; ausschließlich derselbe Drittanbieter-`playTests`-Pfadfehler.
- neue Plattform-/ABI-Tests Release: 2/2 bestanden.
- neue Plattform-/ABI-Tests Debug: 2/2 bestanden.
- Renderer-/GPU-Skinning-Tests Release/Debug: jeweils 24/24 bestanden.
- manuelle World-/Relog-/Lifecycle-Matrix: siehe Abschnitte 24–31; **BLOCKIERT/UNGEPRÜFT**, weil der fremde laufende Client nicht gestört wurde, keine lokalen Server-Listener vorhanden waren und Test-Zugangsdaten fehlten.

## 34. Bekannte verbleibende Blocker

1. Login, Character Select, Ingame, World-Sequenz, Relog, Ingame-GPU-Skinning-Matrix, Resize, Minimize/Restore, Main-Client-Shutdown und Performance-Sanity sind nicht mit der finalen C1-X-EXE nachgewiesen. Ein bereits laufender fremder Client wurde bewusst nicht gestört; gleichzeitig fehlten lokale Server-Listener und Test-Zugangsdaten.
2. Der Drittanbieter-zstd-Test `playTests` scheitert unter dem nativen Windows-CTest an einem MSYS-Pfad `/cygdrive/c/.../rle-first-block.zst`, obwohl die Datei am entsprechenden Windows-Pfad vorhanden ist. Das ist kein C1-X-Plattformtestfehler, verhindert aber eine vollständig grüne Gesamtsuite.
3. Die Runtime-Repository-EXEs sind nicht der aktuelle Source-Build und tragen noch D3D9/D3DX9_43-Imports; sie dürfen nicht versehentlich als Testartefakt verwendet werden.
4. `config.exe` bleibt ein x86/D3D8-WOW64-Hilfsprogramm ohne aufgefundene Source-Gegenstelle. Es blockiert nicht den x64-Hauptclient, verhindert aber die Aussage „gesamtes ausgeliefertes Toolset ist x64“.
5. Im Linux-Serverquellstand bestehen außerhalb dieses Client-Diffs LP64-Protokollrisiken: Guild-War-Score-Pfade senden einen 4-Byte-Wert teilweise mit `sizeof(long)`, ein DB-Cache-Vergleich verwendet `sizeof(long) * 3`, und weitere Wirestrukturen tragen rohe `time_t`-Felder. Die betroffenen Gegenstellen müssen vor einer plattformübergreifenden ABI-Freigabe separat geprüft werden.
6. Der Debug-Link verbindet `/MTd`-First-Party-Code mit der einzigen vorhandenen `/MT`-Release-Python-Library und meldet deshalb LNK4098; hinzu kommen erklärtes LNK4075 und fehlende Drittanbieter-PDBs (LNK4099). Der Build ist erfolgreich, die CRT-Kette sollte aber langfristig vereinheitlicht werden.
7. `CDiskFileLoader` bleibt absichtlich auf `INT_MAX` begrenzt. Das ist transparent und sicher abgewiesen, aber keine Unterstützung für allgemeine Dateien über 2 GiB.
8. `PacketReader`/`PacketWriter` besitzen außerhalb der aktuell verwendeten Pfade noch keine umfassende globale Überlaufshärtung; C1-X hat daraus keine spekulative API-Neuentwicklung gemacht.
9. Der malloc-basierte `CDynamicPool` ist für die heute verwendeten Typen ausreichend ausgerichtet, aber kein zugesichertes Design für hypothetische künftig über-ausgerichtete Typen.

Keiner dieser Punkte wird durch Android-, Vulkan-, Asset-Runtime- oder Granny-Ersatzarbeit innerhalb C1-X umgangen.

## 35. Klare GO/NO-GO-Empfehlung für C2-X

**Aktuelle Empfehlung: NO-GO für C2-X.**

Die statische x64-Basis ist substanziell verbessert und die Buildseite ist erfüllt: x64 wird erzwungen, Release und Debug bauen vollständig, die sechs finalen EXEs sind PE32+/AMD64, ihre statische Importkette ist x64, die nachgewiesenen Pointer-Truncations sind korrigiert und kritische Binärlayouts sind festgeschrieben. Pack-/Datei-/Netzwerkgrenzen sind gehärtet und die gezielten Plattformtests laufen.

Das ausdrücklich verlangte Gesamterfolgskriterium ist dennoch nicht erfüllt. Ein GO darf erst ausgesprochen werden, wenn im selben finalen Quell-/Binärstand zusätzlich folgende Punkte grün und nachvollziehbar sind:

- vollständige bestehende Testsuite ohne den noch offenen zstd-Pfadharnessfehler;
- Runtime-Smoke mit Login, Character Select und Ingame;
- echte World-Sequenz und Relog;
- Ingame-GPU-Skinning-, Resize-, Minimize/Restore- und Shutdown-Regressionsprüfung;
- Main-Client-Exitcode 0 und überwachte Ressourcen nach Shutdown 0;
- Performance-Sanity sowie Freigabebewertung der dokumentierten Debug-CRT- und Server-Gegenstellen.

Bis dahin wird nach C1-X gestoppt; C2-X wird nicht begonnen.

## Git-Diff-Zusammenfassung

Der C1-X-Diff ist thematisch auf Architektur- und Binärsicherheit begrenzt:

- Build/CI/Dokumentation: harte x64-CMake-Schranke, explizites `-A x64`, Debug-/Release-CI-Matrix und harte MSVC-Pointerkonvertierungsdiagnosen.
- Plattformtests: neue ABI-Layout- und CPython-Pointerroundtrip-Tests.
- Pointer/Windows: Terrain-Lichttoken, 64-Bit-Crashcallback, IME-Handlearithmetik sowie sichere Named-Pipe-/Winsock-Grenzen.
- Python: gemeinsame Pointerkonvertierung, geprüfte Integerbreiten und korrekte `Py_ssize_t`-Längen.
- Serialization/Files: feste Item-/Zeitfeldbreiten, Packlayout-Assertions, überlaufsichere Pack-/PackMaker-/Proto-/LZO-Prüfungen und explizite Legacy-Dateigrenzen.
- Memory/GPU: gehärteter Dynamic Pool sowie geprüfte Granny-/GPU-Skinning-Zähler und Offsets.
- Scope: keine Android-, Vulkan-, Asset-Runtime-, Granny-Ersatz-, Visual-Remaster- oder Gameplay-Änderung; keine automatische Ersetzung der Runtime-Binaries.

Gesamtumfang des finalen Arbeitsbaums: 62 betroffene Dateien, 1.619 hinzugefügte und 390 entfernte Zeilen; darin enthalten sind 58 geänderte versionierte Dateien sowie vier neue Dateien (dieser Bericht und drei Plattformtestdateien).
