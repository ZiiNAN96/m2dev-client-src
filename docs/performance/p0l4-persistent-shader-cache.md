# P0-L4 – Persistent Versioned Shader Bytecode Cache

Stand: 2026-09-17. **Technisches GO.** Ausschließlich ein persistenter, versionierter Shader-Bytecodecache; keine Folgeoptimierung.

Basis: `611cd5086e85fc8a558598fa46658e17e5266b7f` (`perf(loading): reduce GR2 and shader initialization costs`). Der vorhandene, uncommitted P0-L3-Bericht bleibt bytegleich erhalten und wird nicht in den P0-L4-Commit aufgenommen.

## EMPTY CACHE / PERSISTENT WARM CACHE – neuer Prozess

Gleicher finaler Release-Build, originale Packs und unveränderte Grafikoptionen. A1 ist in beiden Fällen der erste Mapload des jeweiligen Prozesses. Das ist kein Vergleich mit einem warmen Mapload derselben Session.

| A1-Messung | Empty cache | Persistenter Warm-Cache |
| --- | --- | --- |
| Prozess-ID | 37780 | 36100 |
| A1 total ms | 3813.9431 | 2293.4165 |
| Shader/PSO/FX-Block ms | 1502.6895 | 38.0942 |
| Shader requests | 77 | 77 |
| Cache hits | 22 | 77 |
| Cache misses | 55 | 0 |
| Runtime compiles | 55 | 0 |
| Compile time ms | 1241.1409 | 0.0000 |
| Cache read/validation time ms | 3.8961 | 6.5801 |
| Cache key time ms | 4.2032 | 4.0749 |
| Cache write time ms | 220.7490 | 0.0000 |
| Bytes loaded from cache | 20292 | 291836 |

**DIFFERENCE: 1520.5266 ms gespart, 39.87 % kürzer.**
Shaderkompilierung: **1241.1409 ms und 55 Aufrufe eingespart**; bekannte Shader im neuen Prozess: **77/77 Hits, 0 Misses, 0 Compiles**.

Der isolierte Cache war vor Test A leer. Die 22 Hits in A1 Empty sind identische Anfragen, die bereits im selben Prozess in die Dateischicht geschrieben wurden. Die 77 IShader-Anfragen und getrennten PSOs bleiben bestehen; es werden keine weiteren Shaderobjekte/Varianten zusammengelegt.

Der erstmalige Aufbau kostet in A1 **220.7490 ms Schreiben einschließlich Flush/Close/atomarem Replace**. Dieser Aufwand ist vollständig in der Empty-Zeit enthalten. Der beobachtete Vorteil von 1.521 s besteht daher nicht nur aus 1.241 s vermiedener Kompilierung: zusätzlich entfallen Cache-Schreibvorgänge, außerdem unterscheiden sich andere Ladeanteile um rund 56 ms. Keine Garantie einer schnelleren ersten Installation mit leerem Cache. Gegenüber P0-L3s historischem 3587-ms-Lauf ist der Empty-Lauf hier langsamer; das wird nicht als Cache-Gewinn umgedeutet.

Ein Messpaar, keine statistische Benchmarkserie. Windows-Dateicache nicht geleert; „cold“ bezeichnet den ersten Mapload eines neuen Prozesses. Der Compiler wird bei Hits nachweislich nicht aufgerufen, unabhängig davon, ob Windows die Cachedateien noch im RAM hat. Der Endpunkt bleibt MapLoadTrace stable-present: drei aufeinanderfolgende World-Presents bis 50 ms ohne neue Load-/Upload-/Compile-Aktivität. CPU-Wandzeit, keine GPU-Ausführungszeit.

Clientstart vor A1 separat:

| Fenster | Gesamt ms | Requests | Hits / Misses | Compiles | Compile ms |
| --- | --- | --- | --- | --- | --- |
| Empty client-setup | 1023.4318 | 48 | 30 / 18 | 18 | 661.4743 |
| Warm client-setup | 252.0955 | 48 | 48 / 0 | 0 | 0.0000 |

Damit trifft der Warm-Prozess über Clientstart und A1 insgesamt 125/125 bekannte Anfragen. 73 Cachedateien, 368420 Bytes einschließlich Header. Sämtliche Dateien bleiben über Warm-Smoke und Schlussmessung SHA256-identisch; keine temporären Dateien übrig.

## Vorhandene Infrastruktur und Implementierung

Die gepinnte Diligent-Core-Version bietet `IBytecodeCache` mit In-Memory-Map und Load/Store-Serialisierung, aber keine Dateiablage, atomare Veröffentlichung, Compiler-DLL-Identität, Stage-/Payload-Prüfsumme oder vorgeschaltete Größen-/DXBC-Validierung für beschädigte Dateien. Ihr Gesamtarchiv ist deshalb keine passende fertige Persistenz für die geforderten unabhängig reparierbaren Einträge. Es gab keinen angebundenen persistenten Produktcache.

Die kleine Dateischicht verwendet den vorhandenen Diligent-XXH128-ShaderCreateInfo-Hasher für Source, rekursive Includes und Descriptors. Kein zweiter In-Memory-Shader-/PSO-Cache; die vorhandenen Session-Caches bleiben unverändert. `ShaderBytecodeCache::Compile` sitzt direkt vor dem bestehenden FXC-Aufruf im generierten `ShaderD3DBase.cpp`. Dadurch erfasst sie sowohl unsere Shader als auch intern von DiligentFX erzeugte Shader. DiligentFX und der DiligentCore-Vendor-Checkout werden nicht editiert. Der Build-Hook prüft den erwarteten gepinnten Aufruf exakt und bricht bei nicht passender Integrationsstelle die Konfiguration ab.

HIT: validierten DXBC-Blob zurückgeben, danach normaler Diligent-IShader-/PSO-Pfad. MISS: exakt denselben D3DCompile-Aufruf mit unveränderten Parametern/Flags ausführen; nur erfolgreichen, validierten Bytecode speichern. DXC ist nicht Gegenstand dieser D3D11/FXC-Integration; alle 125 gemessenen Source-Anfragen benutzen den abgedeckten Pfad. Mitgelieferter Bytecode nimmt unverändert Diligents vorhandenen ByteCode-Pfad.

### Schlüssel und automatische Invalidierung

Der 128-Bit-Schlüssel umfasst:

- vollständige ShaderCreateInfo-Identität einschließlich Source und Include-Inhalten, EntryPoint, Stage, Macros, Sprache, Versions-/Reflection-Optionen und CompileFlags;
- den tatsächlich zusammengesetzten Compilertext einschließlich generiertem HLSL-Preamble, seine Länge und zusätzliche FXC-Macros;
- effektives Zielprofil und effektive FXC-Flags, einschließlich Debug-/Release-Unterschied und Matrixlayout;
- Backend/Compilerfamilie `D3D11/FXC` und Hash der tatsächlich geladenen D3DCompile-DLL; falls diese Identität nicht sicher ermittelt werden kann, wird der Cache umgangen;
- eigene Cache-Version und Formatversion, beide derzeit 1;
- DiligentCore-Pin `b036337d68be2353c9950a85929acf796b9a6d50` plus automatisch erzeugte SHA256-Fingerprints der relevanten Core-Compiler-/HLSL-/Hasher-Implementierungen.

Normale Änderungen von Source, Includes, Defines, EntryPoint oder Compilerkontext führen zu einem anderen Dateinamen/MISS. Keine manuelle Löschung nötig. Dateiname und Shader-Anzeigename allein sind keine Identität. Ein Versionswechsel lässt alte Einträge unbenutzt; keine zusätzliche Eviction-/Cleanup-Optimierung eingeführt.

### Format und Schreibsicherheit

Pro Schlüssel genau eine `.shadercache`-Datei. Explizit Little Endian, ohne C++-Struct-Padding:

| Offset | Inhalt |
| --- | --- |
| 0–7 | Magic `M2DXBC01` |
| 8–11 | Formatversion uint32 |
| 12–15 | Shader-Stage uint32 |
| 16–31 | Cache-Key XXH128 |
| 32–39 | Bytecode-Größe uint64 |
| 40–55 | DXBC-Prüfsumme XXH128 |
| ab 56 | vollständiger DXBC-Blob |

Beim Lesen: exakte Dateilänge, maximal 16 MiB Bytecode, Header/Key/Stage/Version, Payload-Prüfsumme, DXBC-Containergrenzen und D3DReflect-Stage/-ShaderModel prüfen. Ungültige/truncated Dateien ergeben MISS, normale Kompilierung und Ersatz des Eintrags. Keine Diligent-Fehlerdiagnose für erwartbare Cachekorruption. Fehlender/unzugänglicher/nicht beschreibbarer Cache ist keine Startvoraussetzung.

Beim Schreiben: exklusive neue Tempdatei mit Prozess-ID und atomarer Sequenz im selben Verzeichnis; vollständiger Header und Payload; FlushFileBuffers; Close; MoveFileEx(REPLACE_EXISTING | WRITE_THROUGH). Ein Leser sieht den bisherigen vollständigen Eintrag oder den neuen vollständigen Eintrag. Eigene fehlgeschlagene Tempdateien werden entfernt; nach einem harten Prozessabbruch verbleibende Tempdateien werden nie als Eintrag gelesen. Gleichzeitige Prozesse veröffentlichen unabhängig vollständig geschriebene Dateien.

### Ablage und Zähler

Standard: `%LOCALAPPDATA%/ZiiNAN/m2dev-client/cache/shaders/`, außerhalb des Source-/Runtime-Git-Checkouts. Ein absoluter Override ist mit `--shader-cache-dir=...` oder `M2_SHADER_CACHE_DIR` möglich; Kommandozeile hat Vorrang. Relative/unauflösbare Pfade führen zum normalen Compilerpfad, nicht zu einem Cache im aktuellen Source-Verzeichnis. Testcache: `build-p0l4/runtime-cache`, bereits durch `/build-*/` ignoriert. Keine zusätzliche `.gitignore`-Regel und keine Runtime-Repository-Änderung erforderlich.

Requests/Hits/Misses/Runtime-Compiles, Byteanzahl, Key-/Read-/Write-/Compilezeit, invalid/write-failure sowie Bytecode-Hashes laufen ausschließlich über den vorhandenen opt-in MapLoadTrace. Kein neues dauerhaftes Datei-Log oder Log-Spam im normalen Betrieb. Read-Zeit umfasst Öffnen, Lesen, Prüfung und Diagnose-Eigenarbeit, nicht nur physische Disk-I/O.

## INVALIDATION / Robustheit – PASS

Der gezielte native `ShaderBytecodeCacheTest` benutzt den tatsächlichen `CompileD3DBytecode`-Produktpfad:

1. Frisch kompilieren, dann HIT; vollständige Bytefolgen müssen identisch sein.
2. Genau ein Macro für einen Shader ändern: MISS und andere Bytefolge. Unveränderter zweiter Shader weiterhin HIT. Änderung zurücknehmen: ursprünglicher Shader wieder HIT.
3. Source, Include-Inhalt, EntryPoint und CompileFlags separat prüfen; Includes wiederherstellen und ursprünglichen HIT nachweisen.
4. Magic, Formatversion, Stage, Key, Größe, Checksumme und Payload einzeln beschädigen; Header und Payload abschneiden; ungültiges DXBC sogar mit neu passender äußerer Prüfsumme prüfen. Jeweils normale Kompilierung, bytegleiche Reparatur, danach HIT.
5. Cachepfad auf eine reguläre Datei richten: Lesen/Schreiben nicht möglich, Shader kompiliert trotzdem erfolgreich.
6. Gleichzeitige Anfragen auf einen noch nicht vorhandenen Eintrag: gültige atomare Veröffentlichung, danach bytegleicher HIT, keine verbliebenen Tempdateien.

Fixture-Änderungen betreffen ausschließlich isolierte Testshader und Dateien unter dem Buildverzeichnis. Keine Production-Shaderlogik oder temporäre Production-Versionänderung bleibt zurück. Test-Exit 0; `build-p0l4/cache-test-final.log`.

## Bytecode-Gleichheit

Alle 73 unterschiedlichen Eingabeschlüssel liefern in Empty-, Warm- und Schlussprozess dieselben DXBC-Prüfsummen. Die frisch gespeicherten 73 Dateien wurden zusätzlich vor/nach Warm-Läufen SHA256-verglichen. Der native Cache-Test vergleicht vollständige Bytefolgen. Repräsentative Production-Shader:

| Shader | Bytecode Bytes | SHA256 des DXBC | Fresh = cached |
| --- | --- | --- | --- |
| Actors/GPU Skinning ModernVS | 8456 | f991ee1e9621e96f31a0a9244e347121905387752cb24b13ac7a75867bdc85ea | PASS |
| Terrain | 2516 | 851f55900e0ecf42ddb561bb5d105bdc7388f44009536b9d6ce2fc2ce536d414 | PASS |
| Vegetation | 8656 | 7c0b16887c240ccd443107b3d06ce6b920d239d52cee74c687e0f01f71eace0e | PASS |
| Water | 16168 | 86b9b59ffe33569c2b5401aae9d1e8c31ff95971609f2a2307d96a6e4be78778 | PASS |
| PBR | 2232 | 3145d45317dbf80daf108efba109ed8d8bd463764bed8516c891277f4ba95641 | PASS |
| PostFX/SSAO | 6920 | 65cd4f3071b4180ae18adb3b1d2d6e282f99966eaae52432aad8ac3ef12ddfdf | PASS |

Kurze Sichtkontrolle der beiden A1-Bilder: Terrain, Vegetation und drei Actors vorhanden, kein erkennbarer Shader-/Material-Ausfall. Kein pixelidentischer Screenshotvergleich dynamischer Animationen und keine vollständige manuelle Produktabnahme behauptet. Shader-, Material-, PSO- und Qualitätslogik bleiben unverändert.

## FAST GATE – PASS

- Release-Client und gezielter nativer Cache-Test gebaut, Exit 0. Bestehende Python/zlib-PDB-Linkerwarnungen; keine finalen Buildfehler.
- Empty A1 und Warm A1 in unterschiedlichen Prozessen bestanden.
- Warm-Prozess A1 → B1 → A1 bestanden. B1 391.4862 ms, A1 derselben Session warm 216.1933 ms; beide ohne Shaderrequests/Compiles.
- Invalidierung, Korruption, unzugängliches Cacheverzeichnis und Bytecode-Gleichheit bestanden.
- Alle 30 bestehenden Fehler-/Fallback-/Shutdown-Ressourcenprüfungen 0, insbesondere Diligent ERROR/FATAL, GPU fallback und CPU deformation. Zusätzlich Modern/Water/SSR-Fallbackprüfungen 0; Python-Fehlerlog leer. Alle drei akzeptierten Prozesse Exit 0.
- Keine volle Testsuite, kein GCC/LP64 (keine portable Core-Schnittstelle verändert), keine lange Visualprüfung, kein Netzwerklogin.

Die ersten beiden Diagnoseversuche `l4-empty-a1` / `l4-warm-process` wurden aus dem Ergebnis ausgeschlossen: die Umgebungsvariable kam im nativen Windows-Start nicht an, sodass der Standardcache verwendet wurde. Danach explizite CLI-Pfadübergabe ergänzt, Release und gezielten Test erneut gebaut/ausgeführt und Empty/Warm frisch wiederholt. Der tatsächlich verwendete Pfad wird jetzt in jedem Trace geprüft; für den Vergleich zählen ausschließlich `l4-final-empty` und `l4-final-warm`. Der Standardcache aus den Vorversuchen wurde nicht gelöscht.

## Neue vollständige Load-Breakdown nach technischem GO

Genau **ein** zusätzlicher Prozess für die Schlussmessung: PID **23852**, A1 Cold **2349.9764 ms**, 77/77 Hits, 0 Misses, 0 Compiles, Shaderblock **37.3680 ms**. Dieser Lauf ersetzt nicht nachträglich das A/B-Messpaar oben.

Exklusive Phasen; Summe entspricht A1 total:

| Phase | ms exklusiv | % A1 |
| --- | --- | --- |
| Assets | 1470.5234 | 62.576 |
| Actors | 568.8807 | 24.208 |
| File access | 115.6801 | 4.923 |
| Terrain | 53.8597 | 2.292 |
| Shaders / PSOs | 37.3680 | 1.590 |
| Other | 26.3434 | 1.121 |
| GPU resources | 26.0217 | 1.107 |
| Vegetation | 20.1488 | 0.857 |
| First frames | 12.3930 | 0.527 |
| Static objects | 8.4008 | 0.357 |
| Synchronization | 4.0885 | 0.174 |
| Textures | 3.8584 | 0.164 |
| Residency | 1.0082 | 0.043 |
| Effects | 0.7469 | 0.032 |
| Materials | 0.2815 | 0.012 |
| Metadata | 0.2751 | 0.012 |
| Sky atmosphere HDR | 0.0771 | 0.003 |
| Water | 0.0211 | 0.001 |
| Gesamt | 2349.9764 | 100.000 |

Top 10 einzelner exklusiver Kostenposten; verschachtelte inklusive Zeiten werden nicht addiert:

| # | Posten | Aufrufe | ms exklusiv | % A1 |
| --- | --- | --- | --- | --- |
| 1 | GR2 section decompression | 6648 | 1227.4097 | 52.231 |
| 2 | GR2 animation curves | 693 | 435.2824 | 18.523 |
| 3 | Actors | 3 | 132.1708 | 5.624 |
| 4 | GR2 mesh | 178 | 110.6974 | 4.711 |
| 5 | mapped read decompress decrypt | 6471 | 80.6906 | 3.434 |
| 6 | GR2 header checksum | 831 | 53.8294 | 2.291 |
| 7 | splat generation | 20 | 37.7853 | 1.608 |
| 8 | GR2 relocations | 831 | 36.3040 | 1.545 |
| 9 | file lookup and loose read | 7388 | 32.3712 | 1.378 |
| 10 | unattributed / frame pacing | 1 | 26.3434 | 1.121 |

**Größter verbleibender Posten: GR2-Section-Dekompression, 1227.4097 ms, rund 52.23 % des A1-Loads.** Nur berichtet; weder GR2 noch Animation, Asset Runtime, Terrain oder Vegetation weiter optimiert.

## Reproduktion, Nachweise und Scope

Alle drei akzeptierten Prozesse verwenden dieselbe Release-EXE, SHA256 `2FBF32D636C7A899C61B02414EAE93CD18A6C7490C9FB427ADC4DCEAA882E096`.

Die Testskripte erstellen private Clients mit Originalpacks, Originalkonfiguration und Offline-Mapprobe. Modern Preset 4, Shadows 4, AO 2, Water 3, Vegetation 2, Textures 1, HDR/Bloom/ModernSky an, ViewDistance 25600, Fog aus. Der Originalclient wurde nicht deployed oder ersetzt; seine Release-EXE und vorhandenen Deployment-Belege sind hashgleich geblieben. Daher ist dies ein technisches natives Gate, keine Behauptung einer neuen Abnahme im installierten Originalclient.

Gezielte Reproduktion mit neuen Evidenznamen und einem noch nicht vorhandenen absoluten Cachepfad:

```powershell
# In der vorhandenen VS-x64-Umgebung, Release:
cmake --build build-h2x/msvc --config Release --target UserInterface ShaderBytecodeCacheTest --parallel 6
ctest --test-dir build-h2x/msvc -C Release -R '^Renderer.ShaderBytecodeCache$' --output-on-failure
# Native Clientstarts benötigen wie bisher den normalen autorisierten Windows-Start.
tests/Loading/run_shader_cache_probe.ps1 -Name cache-empty -CacheDirectory C:/absolute/isolated-cache
tests/Loading/run_shader_cache_probe.ps1 -Name cache-warm -CacheDirectory C:/absolute/isolated-cache -ShaderLifecycle
python.exe tests/Loading/verify_probe.py build-p0l/cache-empty --a1-only
python.exe tests/Loading/verify_probe.py build-p0l/cache-warm --shader-lifecycle
python.exe tests/Loading/verify_shader_cache.py build-p0l/cache-empty build-p0l/cache-warm --output build-p0l4/comparison.json
```

Nach GO genau ein weiterer `run_shader_cache_probe.ps1` ohne ShaderLifecycle; `verify_probe.py --a1-only`, `summarize_trace.py` und `verify_shader_cache.py --breakdown <Pfad>` ergänzen die Abschlussprüfung. Keine existierenden Caches müssen gelöscht werden.

Lokale Belege, absichtlich nicht gestagt: `build-p0l4/build-release.log`, `cache-test-final.log`, `comparison.json`, `cache-after-final-empty.json`, `preservation.json`, `validated-source.json`, `validated-binary.json`; unter `build-p0l/l4-final-empty`, `l4-final-warm`, `l4-final-breakdown` jeweils `cache-run.json`, `exit.json`, `fast-gate.json`, `map-load-trace.tsv`, `summary.json`, `hashes.json` und native Logs. Frühere verworfene Versuche bleiben getrennt erhalten.

Source-Scope: `src/Renderer/ShaderBytecodeCache.{h,cpp}`, `buildtool/ShaderBytecodeCache.cmake`, `PrepareShaderBytecodeCache.py`, ein Include in `Diligent.cmake`, fokussierte Loading-Tests und deren CMake-/Probe-Anbindung. Keine GR2-/Asset-/Terrain-/Vegetations-/Textur-/Shaderlogik-/Materiallogik-/DiligentFX-Änderung. Alle 13 GR2-Quelldateien und der P0-L3-Bericht bytegleich geprüft. Runtime-Repository bleibt auf `548c43b4` mit seinen zwei vorbestehenden untracked Deployment-Belegen.

Commit bei diesem GO: **`perf(shaders): add persistent bytecode cache`**, ausschließlich die genannten Source-/Testdateien und dieser Bericht. Kein Cache, Buildoutput, Log, Screenshot oder Backup im Commit. **Kein Push. STOP nach Commit; keine GR2-Folgeoptimierung und kein P1.**
