# P0-L2 - Shader / PSO Cold-Load

Stand: 2026-09-17. Genau ein funktionaler Fix: identische Modern-Vertexshader
werden im bestehenden Renderer pro Geometrie-/Tangent-/Instancing-Kombination
geteilt. Alle Pixelshader, PSO-Varianten, Grafikoptionen und Qualitaetsstufen
bleiben erhalten. Der komplette GR2-Ordner ist gegen den P0-L-Ausgangsstand
hashidentisch (13 Dateien; `build-p0l2/gr2-before.json`). Kein Commit, kein Push.

## Gemessener Effekt

Frisch gemessener, bereits GR2-optimierter P0-L-Stand gegen denselben Stand mit
Vertexshader-Wiederverwendung. Je ein neuer Prozess, identische Original-Packs,
Konfiguration, Kamera, drei Actors und die Route Client -> A1 -> B1 -> A1.
Cold bezeichnet einen neuen Prozess; der Windows-Dateicache wurde nicht geleert.

| Messung (ms) | BEFORE | AFTER | Einsparung ms | Einsparung % |
| --- | --- | --- | --- | --- |
| A1 Cold TOTAL | 4007.301 | 3562.655 | 444.646 | 11.10% |
| Shader / PSO: gesamte exklusive Phase | 1688.150 | 1323.973 | 364.177 | 21.57% |
| HLSL-Kompilierung inkl. Include-Callbacks | 1651.191 | 1289.359 | 361.832 | 21.91% |
| Eigene CreateShader-Aufrufe (P0-L-Messumfang) | 1272.274 | 914.782 | 357.492 | 28.10% |

Die zuvor genannten ca. 1,248 s erfassten nur eigene CreateShader-Aufrufe.
Jetzt sind auch interne DiligentFX-Aufrufe erfasst: vor dem Fix 80 statt nur 36
Shader, danach 77 statt 33. Die alte Zahl wurde nicht als Baseline uebernommen.
Die gesamte Shader-/PSO-Phase enthaelt ausserdem Initialisierung, Audit- und
Diagnosekosten. Inklusive Untergruppen duerfen nicht addiert werden.

Dies ist ein einzelnes Vorher/Nachher-Paar, kein statistischer Benchmark.
Die beobachtete TOTAL-Differenz ist groesser als die direkt gemessene Shader-
Ersparnis. Insbesondere schwankt die unveraenderte GR2-Dekompression zwischen
1223,804 und 1171,967 ms. Die gesamte TOTAL-Differenz wird deshalb nicht allein
dem Fix zugeschrieben. Es wurde nichts vor den Messstart verschoben.

## Zerlegung der A1-Cold-Baseline

CPU-Wallclock; keine GPU-Ausfuehrungszeiten. `Summe exklusiv` zieht gemessene
Kinder ab. `Max einzeln` ist die inklusive Dauer eines einzelnen Aufrufs.
Include-Laden passiert innerhalb D3DCompile; native Shader-Cache-Lookups innerhalb
der PSO-Erstellung. Die Tabellen zeigen diese Verschachtelung explizit.

| Operation | Count | Summe inkl. ms | Summe exkl. ms | Max einzeln ms |
| --- | --- | --- | --- | --- |
| Shader-Source zusammenstellen / eingebettete Hauptquelle | 80 | 0.262 | 0.262 | 0.007 |
| Shader-Includes laden (eingebettete FX-Quellen) | 396 | 1.028 | 1.028 | 0.014 |
| D3DCompile / HLSL-Kompilierung | 80 | 1651.191 | 1650.163 | 306.879 |
| CreateShader: Kompilierung + Reflection / Objekt | 80 | 1654.234 | 2.781 | 306.949 |
| Diligent nativer Bytecode-Objektcache: Lookup inkl. Lock | 95 | 0.177 | 0.177 | 0.007 |
| Modern Mesh-Shader-Variantentabelle: Lookup | 10 | 0.009 | 0.009 | 0.001 |
| Modern Mesh-PSO-Tabelle: Lookup | 552 | 0.051 | 0.051 | 0.002 |
| PSO-Erstellung (Graphics + Compute) | 48 | 7.114 | 6.937 | 0.287 |
| SRB-Erstellung / Initialisierung statischer Ressourcen | 86 | 0.150 | 0.150 | 0.007 |
| DiligentFX / Adapter-Erstinitialisierung | 6 | 654.954 | 5.681 | 183.575 |
| Diagnose: vollstaendiger Shader-Eingabehash | 80 | 5.879 | 5.879 | 0.253 |
| Diagnose: PSO-Descriptor-/Bytecodehash | 48 | 0.565 | 0.565 | 0.042 |
| Diagnose: exakter Bytecodevergleich | 80 | 0.401 | 0.401 | 0.013 |

Ein persistenter Compile-/Render-State-Cache wird nicht abgefragt: Lookup 0,
Hit/Miss **nicht anwendbar**, da kein solcher Cache konfiguriert ist. Der native
Diligent-Cache speichert D3D11-Shaderobjekte innerhalb eines IShader; er vermeidet
keine erneute HLSL-Kompilierung fuer ein anderes IShader-Objekt.

Gemessene existierende Tabellen im A1-Cold: nativer Shader-Cache 13 Hits / 82
Misses; Modern-Mesh-Shader-Varianten 4 / 6; Modern-Mesh-PSOs 542 / 10. Nachher:
16 / 79, 4 / 6 und 542 / 10. Zusaetzlich werden nach dem Fix drei ModernVS neu
erzeugt und drei passuebergreifend wiederverwendet. Hit/Miss-Werte zaehlen die
instrumentierten Tabellen; eine nicht gemessene Tabelle wird nicht als 0 ausgegeben.

## Renderbereiche (A1 Cold)

Shaderzeiten sind inklusive Compiler-Include-Callbacks. Die Zeile Shadow
enthaelt auch Schattenvarianten von Vegetation und Terrain, damit jede
Kompilierung genau einmal zugeordnet wird. UI/Legacy/Effects verwendet gemeinsame
Renderer; es wurde keine isolierte Voll-UI-Abnahme ausgefuehrt.

| Bereich | Shader N vorher | Compile ms vorher | Max ms vorher | Shader N nachher | Compile ms nachher |
| --- | --- | --- | --- | --- | --- |
| PBR / IBL | 6 | 501.629 | 305.498 | 6 | 499.397 |
| Shadow | 8 | 379.931 | 306.879 | 5 | 27.113 |
| Vegetation | 2 | 69.582 | 43.260 | 2 | 68.821 |
| Terrain | 2 | 8.831 | 5.327 | 2 | 9.321 |
| Water / SSR | 20 | 279.577 | 68.546 | 20 | 276.762 |
| SSAO / PostFX | 24 | 176.247 | 36.488 | 24 | 174.004 |
| HDR / Bloom / Tone | 8 | 33.325 | 14.043 | 8 | 32.112 |
| Sky / atmosphere | 8 | 154.846 | 55.508 | 8 | 154.618 |
| Shadow / AO composition | 2 | 47.222 | 44.535 | 2 | 47.212 |
| UI / legacy / effects | 0 | 0.000 | 0.000 | 0 | 0.000 |

| Bereich | Operation | Count vorher | Summe ms | Max einzeln ms |
| --- | --- | --- | --- | --- |
| PBR / IBL | PSO | 4 | 0.779 | 0.267 |
| PBR / IBL | SRB | 3 | 0.013 | 0.004 |
| Shadow | PSO | 6 | 0.852 | 0.201 |
| Shadow | SRB | 6 | 0.019 | 0.005 |
| Vegetation | PSO | 2 | 0.410 | 0.287 |
| Vegetation | SRB | 2 | 0.008 | 0.004 |
| Terrain | PSO | 1 | 0.144 | 0.144 |
| Terrain | SRB | 15 | 0.016 | 0.003 |
| Water / SSR | PSO | 10 | 1.647 | 0.233 |
| Water / SSR | SRB | 9 | 0.018 | 0.004 |
| SSAO / PostFX | PSO | 13 | 1.659 | 0.184 |
| SSAO / PostFX | SRB | 11 | 0.019 | 0.007 |
| HDR / Bloom / Tone | PSO | 4 | 0.509 | 0.197 |
| HDR / Bloom / Tone | SRB | 4 | 0.006 | 0.002 |
| Sky / atmosphere | PSO | 5 | 0.670 | 0.192 |
| Sky / atmosphere | SRB | 5 | 0.017 | 0.004 |
| Shadow / AO composition | PSO | 1 | 0.191 | 0.191 |
| Shadow / AO composition | SRB | 1 | 0.004 | 0.004 |
| UI / legacy / effects | PSO | 2 | 0.254 | 0.184 |
| UI / legacy / effects | SRB | 30 | 0.032 | 0.004 |

Vorhandene gemessene Caches, BEFORE (n/a = diese Modern-Mesh-Tabelle gilt dort nicht):

| Bereich | Native Shader Hits / Misses | Lookup ms | Max ms | Mesh-Shader Hits / Misses | Mesh-PSO Hits / Misses |
| --- | --- | --- | --- | --- | --- |
| PBR / IBL | 2 / 6 | 0.025 | 0.005 | 1 / 2 | 49 / 3 |
| Shadow | 4 / 8 | 0.025 | 0.004 | 2 / 3 | 451 / 5 |
| Vegetation | 2 / 2 | 0.016 | 0.004 | 1 / 1 | 42 / 2 |
| Terrain | 0 / 2 | 0.003 | 0.002 | n/a | n/a |
| Water / SSR | 0 / 20 | 0.038 | 0.007 | n/a | n/a |
| SSAO / PostFX | 2 / 24 | 0.034 | 0.004 | n/a | n/a |
| HDR / Bloom / Tone | 0 / 8 | 0.009 | 0.002 | n/a | n/a |
| Sky / atmosphere | 1 / 8 | 0.016 | 0.004 | n/a | n/a |
| Shadow / AO composition | 0 / 2 | 0.005 | 0.004 | n/a | n/a |
| UI / legacy / effects | 2 / 2 | 0.008 | 0.003 | n/a | n/a |

Die 48 PSOs enthalten einen Compute-PSO fuer die Atmosphaere. Der bisherige
P0-L-Zaehler `CreateGraphicsPipelineState=24` erfasste nur eigene Graphics-Aufrufe.
SRBs koennen mapabhaengige Texturen enthalten und deshalb trotz bestehendem PSO
neu entstehen. Die gemessenen SRB-Kosten sind hier kein relevanter Engpass.

DiligentFX-/Adapter-Erstinitialisierung, BEFORE (Kinder bereits in obigen Zeilen):

| Block | Count | Gesamt inkl. ms | Eigenanteil exkl. ms | Max ms |
| --- | --- | --- | --- | --- |
| G7-SSR-first | 1 | 183.575 | 1.363 | 183.575 |
| bloom-first | 1 | 31.615 | 1.323 | 31.615 |
| SSAO-first | 1 | 159.343 | 1.310 | 159.343 |
| PostFX-SSAO-first | 1 | 22.695 | 1.134 | 22.695 |
| atmosphere-tables | 1 | 159.996 | 0.353 | 159.996 |
| BRDF-IBL | 1 | 97.730 | 0.199 | 97.730 |

Vollstaendige Operationen pro Bereich inklusive Source, Cache-Lookup und
Maximalwerten liegen in `build-p0l/l2-before/shader-summary.json` und
`build-p0l/l2-after/shader-summary.json`. Die unverkuerzte Shaderliste mit Makros
steht im [Runtime-Inventar](p0l2-runtime-shaders.md).

## Nachgewiesene Doppelarbeit und Auswahl des einen Fixes

In A1 Cold sind drei ModernVS-Bytecodes im Schattenpass exakt gleich dem
bereits kompilierten Farbpass: rigid (6508 Bytes), skin (8456 Bytes) und
instanced vegetation (8656 Bytes). Vergleich vollstaendiger Bytes, nicht nur
Shadernamen oder Hashes. Der Skinning-VS braucht dabei zweimal rund 306 ms.
Dies ist der groesste direkt nachgewiesene vermeidbare Compile-Block in A1 Cold.

| Doppelt kompilierter VS | Count | Compile ms | Max ms |
| --- | --- | --- | --- |
| skin | 1 | 306.879 | 306.879 |
| vegetation | 1 | 27.244 | 27.244 |
| rigid | 1 | 18.026 | 18.026 |

Vorher gemessene Summe dieser drei Duplikate: 352.149 ms.

Fix in `DiligentModernRenderer::Impl::MeshShaders`:

- Vorhandenes Vertexshader-Array behaelt passspezifische Referenzen; diese zeigen
  auf denselben Vertexshader fuer identische Geometrie, Tangents und Instancing.
- Nur Passbits werden aus dem Vertex-Key entfernt (`shaderIndex - pass * 3`).
  GDX_SKIN, GDX_AUX, GDX_TANGENT und H2_INSTANCED bleiben im Key.
- Der Vollstaendigkeitscheck prueft den Pixelshader des jeweiligen Passes.
  Dadurch funktioniert auch ein zuerst angeforderter Schatten-/Forward-Pass.
- Pixelshader bleiben passspezifisch; Blend, Depth, Cull, Mirroring, Layout und
  Render-Target-Formate der PSOs sind unveraendert.
- Ownership bleibt beim bestehenden Modern-Renderer; Referenzen werden beim
  Renderer-Shutdown beziehungsweise Style-Wechsel freigegeben.

Der fokussierte CPU-Compilervergleich kompiliert alle 36 bestehenden Kombinationen
mit HLSL-Preamble und Include-Quellen der gepinnten Diligent-Version sowie denselben
Produktionsflags. 24 exakte Passvergleiche bestanden, inklusive Reflection-Daten
im Bytecode (`build-p0l2/equivalence.txt`). Kein Shadercode wurde geaendert.

Weitere Befunde, bewusst unveraendert: A1 hat 22 wiederholte identische
Shader-Inputs, vor allem FullScreenTriangleVS in FX, sowie einen identischen
PostFXContext::CopyTextureDepth-PSO. Beim Client-Setup entstehen 30 identische
Shader-Inputs und 24 identische PSO-Deskriptoren ueber gemeinsame Legacy-/Effect-
Rendererinstanzen. Diese Setup-Kosten liegen ausserhalb des A1-Timers.
Kein zweiter Cache-/PSO-/Threading-Fix wurde implementiert.

Die PSO-Duplikatsuche verwendet Diligents XXH128/HashCombiner auf den vollstaendigen
Graphics-/Compute-CreateInfos: Pipeline-/Resource-Layout, Blend, Depth/Stencil,
Rasterizer, Input-Layout, Formate, Topologie, Samples, Signaturen und Shaderbytecode.
Sie dient ausschliesslich der Diagnose und steuert keine Wiederverwendung.

## Laufzeit-Kompilierung und vorhandene Cache-Infrastruktur

48 Shader werden im Client-Setup aus eingebetteten Quellen kompiliert (BEFORE
1621,573 ms, AFTER 1571,711 ms). A1 erzeugt danach die oben aufgeschluesselten
80/77 Shader. Modern/Fx-Ressourcen werden beim ersten benoetigten Welt-/Renderpass
angelegt. Die Quellen werden beim Build eingebettet, aber nicht zu DXBC vorkompiliert.
Die Laufzeitkompilierung bedient die vorhandenen Geometrie-, Pass- und FX-Makros;
es gibt derzeit kein ausgeliefertes passendes Bytecodearchiv.

Gepinnte Infrastruktur geprueft:

- Core `b036337d68be2353c9950a85929acf796b9a6d50`, FX
  `cb380ac52100672b5762f595acfb6609e0ecc248`.
- `IBytecodeCache` / `CreateBytecodeCache` ist vorhanden. Der Key verarbeitet
  Source/Includes, Makros, Entry, Stage, CompileFlags, Compiler und Sprachversionen.
  Eine Produktionsintegration braeuchte zusaetzlich kontrollierte Persistenz,
  Versionierung und Fehlerbehandlung. Sie wurde nicht neu gebaut.
- `IRenderStateCache` und FX-`pStateCache` existieren, sind aber nicht verdrahtet.
  `DILIGENT_NO_ARCHIVER=ON`, `ARCHIVER_SUPPORTED=FALSE`,
  `RENDER_STATE_CACHE_SUPPORTED=FALSE`; die FX-Attribute bleiben nullptr.
- Der interne D3D11-Shaderobjektcache hilft wiederholten PSOs desselben IShader,
  erkennt jedoch keinen sourcegleichen, separat kompilierten IShader.

Fuer den gewaehlten Fix reicht die vorhandene Session-Variantentabelle. Kein
persistenter Cache, kein Precompile-System, keine Parallelisierung und keine
Compiler-/Optimierungsflags wurden eingefuehrt oder geaendert.

## Lifecycle und Synchronisation

| Phase | TOTAL vorher ms | TOTAL nachher ms | Compile N vor/nach | PSO N vor/nach | SRB N vor/nach |
| --- | --- | --- | --- | --- | --- |
| client-setup | 1888.981 | 1859.238 | 48 / 48 | 68 / 68 | 2 / 2 |
| A1-cold | 4007.301 | 3562.655 | 80 / 77 | 48 / 48 | 86 / 86 |
| B1-first-in-session | 443.276 | 442.789 | 0 / 0 | 4 / 4 | 72 / 72 |
| A1-warm | 167.028 | 164.079 | 0 / 0 | 0 / 0 | 42 / 42 |

ModernRenderer gehoert zum Backend. Mesh-/Terrain-Shader, PSO-Tabellen, Atmosphere,
Water, PostFX, SSAO und Bloom bleiben beim normalen Mapwechsel bestehen.
`ResetFrame` leert Draw-/Caster-Listen, nicht Pipeline-Caches. Map-Teardown tauscht
mapabhaengige Geometry/Texturen/Vegetation und deren Bindings aus. Resize kann
Fenster-/History-Ressourcen erneuern; Classic/Modern-Wechsel und Shutdown duerfen
den Renderer freigeben. Diese Regeln wurden nicht veraendert.

A1 -> B1 benoetigt vier neue PSOs (zwei Effect- und zwei Terrain-Varianten), keine
Kompilierung. B1 -> A1 braucht weder Shader noch PSO neu. Keine wiederholte
gesamte Renderer-Initialisierung durch Mapwechsel nachgewiesen.

Der Compiler wird synchron und seriell auf dem Main Thread aufgerufen. Keine
asynchrone Compilation ist in diesen eigenen/FX-CreateInfos angefordert.
Das Compile-Warten ist damit CPU-Arbeit innerhalb D3DCompile, kein GPU-Fence-Wait.
Der gemessene native Cache-Lock inklusive Lookup summiert sich auf 0,177 ms.

Im untersuchten Shader-/PSO-Erstellungspfad keine WaitForIdle-/Flush-/Fence-Waits.
`DiligentD3D11Backend::Shutdown` behaelt Flush + WaitForIdle. Screenshot-WaitForIdle
liegt nach dem stabilen Messendpunkt. Water::ReadPeak hat einen WaitForIdle nur
hinter `waterTestReadback`; dieser Testpfad ist im Probe aus. Keine Synchronisation
wurde entfernt. Present bleibt als getrennte Synchronisationsphase erhalten.

## FAST GATE und Auslieferung

- Release/UserInterface: PASS. Vorhandene Python/zlib-PDB-Linkwarnungen sind keine
  Rendererfehler; der finale Build endet mit Exit 0.
- Je einmal A1 Cold -> B1 -> A1 Warm, drei Actors, kurzer Smoke, 189 Frames: PASS.
- Je sauberer nativer Exit 0; keine Python-Fehler oder Renderer-Failure-Eintraege.
- Diligent ERROR/FATAL = 0, GPU fallback = 0, CPU deformation = 0.
- Alle 30 geprueften Shutdown-/Fehlerzaehler = 0, zusaetzlich Modern/Water-Renderer
  sowie Terrain-/Object-Ressourcen freigegeben. Details in `fast-gate.json`.
- Ein kurzer Blick auf beide A1-Aufnahmen zeigt Terrain, Actors und Vegetation
  weiterhin vorhanden; keine lange Visualpruefung oder manuelle Produktabnahme.
- Keine grosse Suite, kein Content-Test, kein Netzwerk-Login/Relog.
- Quell- und Originalclient-Repository getrennt geprueft; kein Commit/Push.

Der exakt getestete Release wurde in den normalen `m2dev-client` kopiert.
SHA256: `9039416138E0DBBC61E94B6A0CE7DEA72F401024E5E1FA5943A12D5A11D8490F`.
Der vorherige P0-L-Release ist unter `build-p0l2/deployment/backup/Metin2_Release.exe`
gesichert. 97 Pack-/Config-/Debug-Dateien blieben hashidentisch. Beleg:
`build-p0l2/deployment/receipt.json` und im Originalclient
`docs/p0l2-shader-loading-deployment.json`. **Client neu starten.**

## Reproduktion / Belege

- Baseline: `build-p0l/l2-before/` (map-load-trace.tsv, summary.json,
  shader-summary.json, fast-gate.json, hashes.json, Screenshots und native Logs).
- Nachher: `build-p0l/l2-after/`, gleicher Umfang und gleiche Root-Pack-Quelle.
- Release-Logs: `build-p0l/build-l2-baseline-final.log`,
  `build-p0l/build-l2-after.log`.
- Getrennte opt-in Messpunkte in generierten Kopien von drei gepinnten Core-
  Dateien; der DiligentCore-Checkout bleibt sauber. Der FX-SRB-Messpunkt wird
  im vorhandenen PrepareDiligentFX-Schritt eingebaut. Normale Starts aktivieren
  die Messung nicht. COST-Zeilen haben jetzt zusaetzlich `max_ms` als letzte Spalte.
- Befehle fuer eine gezielte Wiederholung stehen in `tests/Loading/README.md`.

P0-L2 abgeschlossen. STOP nach genau einem Shader-Fix.
