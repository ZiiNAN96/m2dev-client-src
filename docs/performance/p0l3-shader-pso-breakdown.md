# P0-L3 SUMMARY

Stand: 2026-09-17. Analyse abgeschlossen; kein P0-L3-Performance-Fix implementiert.

## Checkpoint P0-L / P0-L2

Checkpoint: **`611cd5086e85fc8a558598fa46658e17e5266b7f`** auf `codex/g56-hdr-atmosphere`.
Der angeforderte Commit war beim Beginn dieser Analyse bereits HEAD. Titel und Body stimmen vollständig mit dem Auftrag überein; daher kein zweiter Commit und kein Amend.

```text
perf(loading): reduce GR2 and shader initialization costs

- optimize GR2 decompression symbol lookup
- preserve byte-identical decompression output
- reuse byte-identical modern vertex shaders
- reduce A1 cold map loading time
- preserve warm-load behavior
- keep rendering output and runtime behavior unchanged
- validate A1/B1/A1 offline smoke and clean shutdown
```

Der vollständige Commit-Diff wurde geprüft: Source, Buildtool-Source, Tests und Dokumentation; keine EXEs, Logs, Traces, Screenshots, Testclients oder Backups. Alle 57 Dateien stimmen per SHA256 mit dem validierten P0-L2-Manifest überein (`build-p0l3/checkpoint-vs-validated.json`, Differences=[]). Historische Gleichheitsnachweise bleiben in `docs/performance/p0l-map-loading.md` und `p0l2-shader-loading.md`: GR2 903 Dateien / 7224 Sections / 60042028 identische Bytes / 2226 abgelehnte truncated Sections; Modern-VS alle 36 Varianten, 24 exakte Pass-Vergleiche. Diese Tests wurden für P0-L3 nicht wiederholt.

Runtime-Repository separat geprüft: `codex/g56-hdr-colors`, HEAD `548c43b4`. Die beiden vorhandenen untracked Deployment-Belege `docs/p0l-map-loading-deployment.json` und `docs/p0l2-shader-loading-deployment.json` bleiben unverändert. Kein Push, kein weiterer Commit.

## Neue Baseline und Messgrenze

| Messfenster | Gesamt ms | Shader-/PSO-/FX-Block ms | neue Shader | neue PSOs |
| --- | --- | --- | --- | --- |
| client-setup | 1835.2220 | 1592.5426 | 48 | 68 |
| A1-cold | 3587.2964 | 1323.5136 | 77 | 48 |
| B1-first-in-session | 392.8022 | 1.5799 | 0 | 4 |
| A1-warm | 165.8564 | 0.1282 | 0 | 0 |

**A1 COLD TOTAL: 3587.2964 ms. A1 WARM TOTAL: 165.8564 ms.** B1: 392.8022 ms.

Ein frischer Release-Prozess, ein A1-Cold-Load, dann B1 und A1-Warm; keine Mittelwerte aus älteren Messungen. „Cold“ bedeutet erster Mapload dieser Client-Session, nicht geleerter Windows-Dateicache. Der separat ausgewiesene Clientstart liegt vor dem A1-Messfenster. Die vollständige Prozesslaufzeit einschließlich Vorbereitung, späterer Frames und Shutdown beträgt 10.179862 s.

Gemessen wurde der optimierte Checkpoint mit temporären, funktional neutralen Timern in einem privaten Offline-Testclient unter `build-p0l/l3-analysis`, mit Originalpacks und Original-Grafikkonfiguration. Modern, Preset 4, Shadows 4, AO 2, Water 3, Vegetation 2, Textures 1, HDR/Bloom/ModernSky an, ViewDistance 25600, Fog aus. Keine Qualitätsänderung. Das Originalprogramm wurde nicht ersetzt. Diagnose-EXE SHA256: `6F870B210244BBD1EEE34869E3050E3C1D6826804E8904D8043EFF0584048D4D`.

Endpunkt: bestehender MapLoadTrace-Endpunkt nach drei aufeinanderfolgenden World-Presents bis 50 ms ohne neue File-/Upload-/Compile-Arbeit. Gemessen wird CPU-Wandzeit auf dem synchronen Lade-/Renderpfad, keine GPU-Ausführungszeit. Screenshots des vorhandenen Smoke-Skripts liegen nach diesem Endpunkt; keine Visual-Galerie oder visuelle Abnahme durchgeführt.

## SHADER / PSO / DILIGENTFX TOTAL

**1323.5136 ms**, davon 1319.0624 ms in `Shaders / PSOs` und 4.4512 ms zuvor darin verschachtelte Grafikressourcen. Die zweite Phase wurde für diese Analyse explizit herausgelöst und für die Vergleichssumme wieder hinzugerechnet. Kein doppeltes Addieren von Konstruktoren, FX-Aufrufen und deren Shader-/PSO-Kindaufrufen.

### BREAKDOWN — exklusive, addierbare Zeiten

| Kosten | ms | % Shaderblock | % A1 Cold |
| --- | --- | --- | --- |
| shader load | 1.1619 | 0.088 | 0.032 |
| shader compile | 1281.5029 | 96.826 | 35.723 |
| shader object creation | 5.9542 | 0.450 | 0.166 |
| PSO creation | 3.2263 | 0.244 | 0.090 |
| SRB | 0.1449 | 0.011 | 0.004 |
| DiligentFX own initialization | 2.9853 | 0.226 | 0.083 |
| cache lookup | 0.3138 | 0.024 | 0.009 |
| graphics resources | 4.4512 | 0.336 | 0.124 |
| waits | 0.0206 | 0.002 | 0.001 |
| other | 23.7525 | 1.795 | 0.662 |
| Summe | 1323.5136 | 100.000 | 36.894 |

Shader load: 77 Source-Aufbauten plus 372 Include-Öffnungen; eingebettetes HLSL/FX-Source-Factory, keine gelieferten Shader-Binaries. Object creation: 2.6131 ms Diligent-IShader-Eigenarbeit plus 3.3411 ms native D3D11-Shaderobjekte. Letztere entstehen teilweise erst beim PSO-Aufbau, wenn Ressourcenbindungen im Bytecode angepasst werden; deshalb 79 native Objekte bei 77 IShader-Erzeugungen.

Grafikressourcen im Shaderblock: 9 Buffer, 60 Texturen, 18 Sampler; 0.1404 / 4.2453 / 0.0655 ms. Das sind CPU-API-Zeiten, keine GPU-Upload-Stoppuhr. Außerhalb des Blocks bleiben weitere 26.6197 ms Grafikressourcen unverändert in ihrer eigenen Mapload-Phase.

„Other“ enthält 10.7953 ms Diagnose-Hashes/Bytecode-Vergleiche, 12.5908 ms Eigenzeit der bestehenden FirstUse-Shader-/PSO-Wrapper einschließlich ihres Datei-Loggings und 0.3664 ms weiterer Wrapper. Diagnosekosten sind in der Baseline enthalten; die Messung behauptet keine identische Laufzeit bei ausgeschalteter Instrumentierung. Zusätzliche L3-Hash-Erfassung wurde wieder entfernt.

### Shader Compilation nach Stage

| Stage | Aufrufe | exklusiv ms | D3DCompile inkl. Includes ms |
| --- | --- | --- | --- |
| Vertex | 36 | 409.4424 | 409.7256 |
| Pixel | 40 | 819.2706 | 819.8871 |
| Geometry | 0 | 0.0000 | 0.0000 |
| Compute | 1 | 52.7899 | 52.8141 |

77 Compileraufrufe, **1282.4268 ms einschließlich Include-Callbacks**, durchschnittlich **16.6549 ms**. Für die addierbare Aufteilung werden 1281.5029 ms Compiler-Eigenzeit benutzt; die 0.9239 ms Include-Zeit steht unter shader load. Langsamster Shader: Actors `ModernVS`, GDX_SKIN=1, AUX/SHADOW/FORWARD/TANGENT/H2_INSTANCED=0, **299.1750 ms** inklusive / **299.1489 ms** exklusiv.

## SUBSYSTEM TOP COSTS

Disjunkte Zuordnung nach aufrufendem Subsystem und konkretem Shader-/PSO-Namen. Compiler- und PSO-Spalten sind Teilmengen der Gesamtspalte, nicht zusätzliche Kosten. Schatten-Varianten von Actors/Vegetation/Terrain zählen zu Shadows. Die gemeinsame Shadow/AO-Composite-Stufe zählt dort ebenfalls; PBR helpers bedeutet BRDF/IBL-Vorberechnung, Material-PBR verbleibt bei Actors/Static World/Vegetation. Shared PostFXContext wird separat ausgewiesen.

| Subsystem | gesamt ms | Compile ms | PSO exklusiv ms | Ressourcen ms | FX-Eigenarbeit ms | % Block |
| --- | --- | --- | --- | --- | --- | --- |
| Actors / GPU Skinning | 342.5358 | 340.3321 | 0.1415 | 0.0000 | 0.0000 | 25.881 |
| SSR | 181.6582 | 177.3528 | 0.4663 | 0.7460 | 0.7032 | 13.725 |
| Sky / Atmosphere | 158.8262 | 152.9662 | 0.3074 | 0.6531 | 0.3942 | 12.000 |
| SSAO | 157.7971 | 153.2799 | 0.5117 | 0.7144 | 0.8060 | 11.923 |
| Water | 103.7404 | 98.7211 | 0.2254 | 0.0000 | 0.0088 | 7.838 |
| PBR helpers | 96.3263 | 94.6701 | 0.0544 | 0.2525 | 0.2115 | 7.278 |
| Shadows + shared composition | 79.7953 | 73.7039 | 0.4991 | 0.0941 | 0.0238 | 6.029 |
| Vegetation | 71.9840 | 69.6654 | 0.2627 | 0.0000 | 0.0000 | 5.439 |
| Static World | 61.3446 | 58.8219 | 0.2174 | 0.0000 | 0.0000 | 4.635 |
| Bloom | 30.8649 | 28.3562 | 0.1511 | 1.3139 | 0.4080 | 2.332 |
| Shared PostFXContext | 22.2874 | 20.1982 | 0.1338 | 0.6772 | 0.4298 | 1.684 |
| Terrain | 10.6704 | 9.7656 | 0.0607 | 0.0000 | 0.0000 | 0.806 |
| Tone Mapping | 4.5724 | 3.6695 | 0.0345 | 0.0000 | 0.0000 | 0.345 |
| Effects | 0.6990 | 0.0000 | 0.1603 | 0.0000 | 0.0000 | 0.053 |
| Shared/unattributed overhead | 0.4116 | 0.0000 | 0.0000 | 0.0000 | 0.0000 | 0.031 |
| HDR | 0.0000 | 0.0000 | 0.0000 | 0.0000 | 0.0000 | 0.000 |
| UI/Text | 0.0000 | 0.0000 | 0.0000 | 0.0000 | 0.0000 | 0.000 |

HDR und UI/Text: 0 zusätzliche, separat diesen Systemen zuordenbare Shader-/PSO-Erzeugungen im A1-Fenster. HDR benutzt Material-, Bloom- und Tone-Mapping-Pfade der Tabelle. `G56 HDR world scene` kostet 0.0452 ms Textur-Erzeugung außerhalb des Shaderblocks; `G7 opaque HDR refraction source` weitere 0.1844 ms im Water-Pfad außerhalb des Blocks. UI/Text ist kein eigenständig belasteter Pfad dieses Offline-Maptests; die Null ist keine Messung des vollständigen Spiel-UI. Legacy/Effects/initiale Rendertechnik wird bereits im Clientstart kompiliert: dort 48 Anfragen / 18 eindeutige Eingaben / 1565.4587 ms Compiler-Eigenzeit, separat vom angefragten A1-Block.

## COUNTS

API-Grenze: Shader requests = zentrale `IRenderDevice::CreateShader`-Aufrufe; actual creations = erfolgreich erhaltene IShader mit Bytecode. PSO requests/creations = zentrale Graphics-/Compute-PSO-Aufrufe im erfolgreichen Lauf. „Unique“ verwendet den gepinnten XXH128-Hasher über die relevanten Descriptor-Inhalte inklusive Shader-Source/Includes/Macros bzw. Shader-Bytecode; Namen sind keine Identität. Zusätzlich wurde Bytecode-Gleichheit exakt über vollständige Bytefolgen geprüft. Die Diagnose-Maps sind keine Produkt-Caches.

| Messfenster | Shader req / unique / created | unique DXBC | PSO req / unique / created | SRBs | FX ctor / Initialize |
| --- | --- | --- | --- | --- | --- |
| client-setup | 48 / 18 / 48 | 18 | 68 / 44 / 68 | 2 | 0 / 0 |
| A1-cold | 77 / 55 / 77 | 49 | 48 / 47 / 48 | 86 | 7 / 1 |
| B1-first-in-session | 0 / 0 / 0 | 0 | 4 / 4 / 4 | 72 | 0 / 0 |
| A1-warm | 0 / 0 / 0 | 0 | 0 / 0 / 0 | 42 | 0 / 0 |

A1 Cold nach Subsystem; S = Shader req/unique/created, P = PSO req/unique/created. Eindeutige Schlüssel können in mehreren Subsystemen vorkommen; deshalb unique-Spalten nicht zu einem globalen Unique-Wert addieren.

| Subsystem | S | P | SRB | Native Shader hit/miss | Mesh Shader hit/miss | Mesh PSO hit/miss | FX PSO hit/miss |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Actors / GPU Skinning | 2/2/2 | 1/1/1 | 1 | 0/2 | 0/1 | 43/1 | 0/0 |
| SSR | 14/8/14 | 7/7/7 | 6 | 0/14 | 0/0 | 0/0 | 1/7 |
| Sky / Atmosphere | 8/7/8 | 5/5/5 | 5 | 1/8 | 0/0 | 0/0 | 0/0 |
| SSAO | 16/9/16 | 9/9/9 | 7 | 2/16 | 0/0 | 0/0 | 31/9 |
| Water | 6/5/6 | 3/3/3 | 3 | 0/6 | 0/0 | 0/0 | 0/0 |
| PBR helpers | 2/2/2 | 1/1/1 | 0 | 0/2 | 0/0 | 0/0 | 0/0 |
| Shadows + shared composition | 7/7/7 | 7/7/7 | 7 | 7/7 | 2/3 | 451/5 | 0/0 |
| Vegetation | 2/2/2 | 2/2/2 | 2 | 2/2 | 1/1 | 42/2 | 0/0 |
| Static World | 2/2/2 | 2/2/2 | 2 | 2/2 | 1/1 | 6/2 | 0/0 |
| Bloom | 6/4/6 | 3/3/3 | 3 | 0/6 | 0/0 | 0/0 | 9/3 |
| Shared PostFXContext | 8/6/8 | 4/4/4 | 4 | 0/8 | 0/0 | 0/0 | 16/4 |
| Terrain | 2/2/2 | 1/1/1 | 43 | 0/2 | 0/0 | 0/0 | 0/0 |
| Tone Mapping | 2/2/2 | 1/1/1 | 1 | 0/2 | 0/0 | 0/0 | 0/0 |
| Effects | 0/0/0 | 2/2/2 | 2 | 2/2 | 0/0 | 0/0 | 0/0 |
| Shared/unattributed overhead | 0/0/0 | 0/0/0 | 0 | 0/0 | 0/0 | 0/0 | 0/0 |
| HDR | 0/0/0 | 0/0/0 | 0 | 0/0 | 0/0 | 0/0 | 0/0 |
| UI/Text | 0/0/0 | 0/0/0 | 0 | 0/0 | 0/0 | 0/0 | 0/0 |

### Vorhandene Cache-Ebenen — getrennte Zählgrenzen

| Cache / Guard | A1 requests: hits / misses | B1 | A1 Warm |
| --- | --- | --- | --- |
| Mesh shader variant | 10: 4 / 6 | 0 | 0 |
| P0-L2 ModernVS innerhalb neuer Mesh-Varianten | 6: 3 reuse / 3 create | 0 | 0 |
| Mesh PSO | 552: 542 / 10 | 400: 400 / 0 | 552: 552 / 0 |
| FX IsInitializedPSO guard | 80: 57 / 23 | 68: 68 / 0 | 68: 68 / 0 |
| IShader native D3D11 object cache | 95: 16 / 79 | 8: 8 / 0 | 0 |
| Persistenter HLSL/DXBC-Cache | nicht angebunden: 0 Hits; Miss-Zähler n/a | n/a | n/a |

Die Ebenen sind verschachtelt und dürfen nicht addiert werden. FX-Guard-Zähler messen IsInitializedPSO-Abfragen der eingebundenen Komponenten, keinen allgemeinen Descriptor-Cache. Weitere direkt erzeugte Copy-/Adapter-PSOs erklären, warum 23 FX-Misses nicht der vollständigen PSO-Zahl entsprechen. Nicht alle Terrain/Effects-Cachezugriffe besitzen eigene Lookup-Counter; ihre vollständigen Erzeugungen sind zentral erfasst. Native Shader-Hits verhindern keine HLSL-Kompilierung. Die 77 HLSL-Anfragen gingen alle in den Compiler; „77 Compileraufrufe“ ist der Befund, nicht ein erfundener Cache-Miss-Zähler.

## Duplikate

A1 Cold: **77 Eingaben, 55 eindeutige, 22 Wiederholungen**. Alle drei wiederholten Eingabegruppen:

- `FullScreenTriangleVS.fx`: 21 Aufrufe derselben Eingabe, davon PostFXContext 3, SSAO 8, Bloom 3, SSR 7; 20 Wiederholungen über vier Besitzer. Insgesamt 28.1832 ms Compiler-Eigenzeit für diese Gruppe.
- Atmosphere FullScreenTriangleVS: 2 Aufrufe, 1 Wiederholung; Gruppe 2.8811 ms.
- WaterScreenVS: 2 Aufrufe, 1 Wiederholung; Gruppe 9.7289 ms.

Diese Gruppen kosten zusammen 40.7932 ms; nur die Wiederholungen wären vermeidbar, die jeweils erste Kompilierung bleibt. Die aggregierten Trace-Zeilen enthalten nicht die Reihenfolge jeder Einzelausführung; deshalb kein erfundener exakter Einsparwert für „alle außer dem ersten“. 49 unterschiedliche vollständige DXBC-Bytefolgen, 28 bytegleiche Wiederholungen zeigen zusätzlich äquivalente Ergebnisse trotz unterschiedlicher Eingaben. P0-L2 wirkt weiter: drei ModernVS-Erzeugungen, drei Wiederverwendungen in Schatten-Passes.

A1 hat **48 PSO-Erzeugungen / 47 unterschiedliche Descriptor-Hashes**. Der eine gleiche Descriptor gehört zu `PostFXContext::ComputePreviousDepth` und `PostFXContext::CopyTextureDepth`, letzterer aus SSAO-first. Gleiche Copy-VS/PS und Depth-Copy-Konfiguration, unterschiedliche Cache-Einträge/Aufrufzwecke. Die zweite Erzeugung kostet nur 0.0328 ms inklusive / 0.0269 ms exklusiv. Keine Deduplication vorgenommen. Keine weiteren subsystemübergreifend gleichen PSO-Descriptor-Hashes in A1.

Clientstart separat: 48 Shader / 18 unique und 68 PSOs / 44 unique; diese Wiederholungen liegen vor A1 und werden nicht als A1-Einsparung angesetzt. B1: vier neue, bereits über die gesamte Session unbekannte PSO-Descriptor-Hashes; A1 Warm: keine neuen. Keine beobachtete wiederholte identische Shader-/PSO-Erzeugung beim Mapwechsel.

## LIFETIME

| Arbeit / Besitzer | Einordnung | Beobachtung und Grenze |
| --- | --- | --- |
| Modern Mesh-/Terrain-Shader, Material-/Shadow-PSOs | SESSION GLOBAL im Backend/ModernRenderer | Shaderarrays und Pipeline-Maps bleiben über ResetFrame und Mapwechsel erhalten. |
| PBR BRDF/IBL, ShadowMapManager, Sky/Atmosphere, Water/SSR | SESSION GLOBAL bei unveränderter Konfiguration | Erst A1; keine neuen Konstruktoren/Shader bei B1/A1 Warm. |
| Shared PostFXContext, SSAO, Bloom, Tone Mapping | SESSION GLOBAL bei konstanter Fenstergröße/Stil | Prepare läuft weiter, IsInitializedPSO trifft. Resize darf Fensterressourcen und einige FX-Instanzen bewusst neu aufbauen. |
| Terrain-/Material-SRBs, Mapgeometrie und gebundene Texturen | MAP LOCAL / ASSET LOCAL | SRBs 86 → 72 → 42; keine entsprechende erneute Shaderkompilierung. Asset- und Mapressourcen gehören nicht zum session-globalen Shadercache. |
| Vier erstmals benötigte B1-PSOs | SESSION GLOBAL, lazy variant | Je ein Terrain- und Shadow-Terrain-PSO plus zwei Effects-PSOs. Keine Descriptor-Duplikate. |
| Identische Fullscreen-Shader / zwei Depth-Copy-PSOs | verdächtige Wiederholung innerhalb erster Sessioninitialisierung | Getrennte Technikbesitzer; kleine Teilkosten, kein Map-Lifetime-Fehler. |

Backend::Impl besitzt ModernRenderer. ResetFrame löscht Frame-/Caster-Listen, keine Shaderarrays/PSO-Maps. Style-Wechsel und Shutdown zerstören den Renderer; Resize ruft ReleaseWindowResources auf und setzt unter anderem PostFX/AO/Bloom zurück. Diese Fälle wurden nicht als Mapwechsel ausgegeben und hier nicht erneut getestet. Keine Behauptung unveränderter Lebensdauer über Device-/Fensterneuanlage.

## Runtime Compilation

A1: 77 HLSL-Aufrufe, kein mitgelieferter Bytecode, synchroner FXC-Pfad (`ShaderCompiler=0`, DEFAULT → FXC in D3D11), VS/PS/CS-Profil aus Stage/ShaderModel. Der Release-Build definiert NDEBUG, nicht DILIGENT_DEBUG. 75 Aufrufe haben CompileFlags=8 (PACK_MATRIX_ROW_MAJOR), die beiden PostFX-Copyshader Flags=0. D3DCompile setzt STRICTNESS, optional ROW_MAJOR; DEBUG wird nur bei DILIGENT_DEBUG gesetzt. Kein SKIP_OPTIMIZATION und kein explizites OPTIMIZATION_LEVEL3 im aktiven Pfad; kein Wechsel dieser Flags vorgenommen. Async-Creation ist nicht aktiviert.

Im gegenwärtigen Production-Pfad ist Runtime-Compilation beim erstmaligen Anfordern notwendig, weil die Creation-Infos Source liefern und kein vorgeschalteter Bytecodecache angebunden ist. D3D11/IShader selbst verlangt diesen wiederholten Source-Pfad nicht: ShaderCreateInfo kann auch ByteCode aufnehmen. Native Shaderobjekt- und Mesh-/FX-Caches gelten nur innerhalb des laufenden Prozesses.

Die gepinnte Core-Version enthält IBytecodeCache (Load/GetBytecode/AddBytecode/RemoveBytecode/Store), ist aber hier nicht in die Shadererzeugung integriert. Der Hash berücksichtigt Source samt Includes, EntryPoint, ShaderDesc, Sprache, Compiler-Auswahl, Versionsfelder, CompileFlags, Reflection-Option und Macros; der Bytecodecache ergänzt DeviceType. Tatsächliche Compiler-DLL-Version, Build-/Engine-Preamble und aus dem Gerät effektiv abgeleitetes Zielprofil benötigen zusätzlich eine bewusste Cache-Versionierung. Den vorhandenen Hash blind als vollständige persistente Gültigkeitsgarantie zu verwenden wäre unzureichend.

RenderStateCache ist kein aktuell aktiver Produkt-Cache: DILIGENT_NO_ARCHIVER=ON, ARCHIVER_SUPPORTED/RENDER_STATE_CACHE_SUPPORTED false und kein FX-pStateCache gesetzt. Ein vollständiger PSO-Cache ist durch die gemessenen PSO-Kosten nicht als nächster Eingriff begründet.

## PSO Creation

48 Aufrufe, 47 eindeutige relevante Descriptor-Hashes, **6.8861 ms inklusive**, durchschnittlich **0.1435 ms**. Addierbare PSO-Eigenzeit **3.2263 ms**, durchschnittlich **0.0672 ms**; native Shaderobjekte/Lookups stehen separat. Langsamste einzelne Aufrufe: Actors Mesh 0.2953 ms, Vegetation Mesh maximal 0.2850 ms, Static World Mesh maximal 0.2406 ms, WaterComposite 0.2286 ms, SSR Spatial Reconstruction 0.2256 ms. Kein PSO-Create nahe 10 ms.

Mesh-Schlüssel enthalten Cull, Blend, DepthWrite, ShaderIndex und Mirrored; Terrain-Schlüssel Strip/List, Blend, Shadow und Solid. Die vollständige Descriptor-Prüfung schließt Blend/Rasterizer/Depth, InputLayout, Topologie, RT-/DS-Formate, Ressourcenlayout und Shader-Bytecode ein. Gleich benannte PSOs sind deshalb nicht automatisch Duplikate. B1s vier neue Hashes passen zum erstmaligen Bedarf legitimer Varianten; kein Beleg für ein Wegwerfen bestehender PSOs. Keine Varianten zusammengelegt.

## DiligentFX und Adapter

Konstruktoren umfassen echte make_unique-Aufrufe einschließlich Initializer-Listen. Eigenzeit ist nach Abzug erfasster Kinder; inklusive Zeit zeigt den Aufrufumfang. Die Eigenarbeit aus Konstruktoren, Initialize, PrepareResources, PrepareShadersAndPSO und den FX-Erstaufrufen summiert sich auf **2.9853 ms**. Kompilierung bleibt unter shader compile, PSOs unter PSO creation, Ressourcen unter graphics resources.

| Konstruktor | Anzahl A1 / B1 / Warm | inklusive ms | exklusive ms |
| --- | --- | --- | --- |
| PostFXContext | 1 / 0 / 0 | 3.0497 | 0.0433 |
| Bloom | 1 / 0 / 0 | 0.0242 | 0.0091 |
| Water | 1 / 0 / 0 | 0.0088 | 0.0088 |
| Sky-atmosphere | 1 / 0 / 0 | 158.9368 | 0.0082 |
| SSR | 1 / 0 / 0 | 0.0141 | 0.0069 |
| SSAO | 1 / 0 / 0 | 0.0248 | 0.0036 |
| ShadowMapManager | 1 / 0 / 0 | 0.0016 | 0.0016 |

ShadowMapManager::Initialize zusätzlich 1 / 0 / 0, 0.1163 ms inklusive / 0.0222 ms exklusiv. Das ist ein Initialize-Aufruf, keine zweite Konstruktion. PBR helpers sind Funktionen, kein zusätzlicher Klassenkonstruktor.

| Prepare-Methode A1 | Aufrufe | inklusive ms | exklusive ms |
| --- | --- | --- | --- |
| ScreenSpaceAmbientOcclusion::PrepareShadersAndPSO | 4 | 156.5095 | 0.2763 |
| ScreenSpaceReflection::PrepareShadersAndPSO | 1 | 180.4352 | 0.2326 |
| PostFXContext::PrepareShadersAndPSO | 5 | 18.6699 | 0.0977 |
| ScreenSpaceAmbientOcclusion::PrepareResources | 4 | 0.7658 | 0.0839 |
| Bloom::PrepareShadersAndPSO | 4 | 29.2373 | 0.0810 |
| ScreenSpaceReflection::PrepareResources | 1 | 0.7872 | 0.0508 |
| Bloom::PrepareResources | 4 | 1.3149 | 0.0338 |
| PostFXContext::PrepareResources | 4 | 0.2582 | 0.0050 |

Die Subsystemtabellen liefern dazu Shader-/PSO-/SRB-Zahlen und Ressourcenzeiten; insbesondere PostFXContext 8 Shader / 4 PSOs, SSAO 16 / 9 einschließlich seiner Depth-Copy-Anforderung, Bloom 6 / 3, SSR 14 / 7, PBR helpers 2 / 1. PrepareResources und PrepareShadersAndPSO sind wiederholt aufrufbare Guards: B1 und A1 Warm jeweils 12 Aufrufe pro Methodenkategorie, aber keine FX-Neukonstruktion/Initialize und 68/68 erfolgreiche PSO-Guard-Treffer. Das ist keine teure Mehrfachinitialisierung. Die 647.8341 ms inklusiven FX-Erstaufrufe oder der 158.9368-ms-Sky-Konstruktor dürfen nicht zusätzlich zur Compile-Summe gerechnet werden.

## Synchronisation

95 native Shadercache-Lock-Akquisitionen und 62 Shader-GetStatus(true)-Abschlussprüfungen im A1-Block: zusammen **0.0206 ms**, keine auffällige Einzelwartezeit. Der Mutex wird für die Dauer von Lookup/Objekterzeugung gehalten; der Timer misst die Akquisition, die restliche Arbeit steht in eigenen Kategorien. Shader-Completion betrifft hier synchron fertig kompilierte Objekte.

Keine ausgeführte WaitForIdle-/Flush-/Fence-/Worker-Join-Stelle im untersuchten Shader-/PSO-Erstellungspfad. D3D11Backend::Shutdown enthält Flush/WaitForIdle nach dem Test, Screenshot-WaitForIdle liegt nach dem Load-Endpunkt; optionaler Water-Readback ist kein aktivierter Shaderload-Pfad. Separat gibt es 1.6994 ms unter der allgemeinen Mapload-Synchronisationsphase; diese werden nicht in den Shaderblock umgebucht. D3DCompile blockiert den Aufrufer bis zu 299.1750 ms, ist hier Compilerarbeit und kein nachgewiesener GPU-Wait. Treiberinternes Warten wurde nicht mit ETW/GPU-Traces aufgeschlüsselt; die gemessenen PSO-/nativen Shader-API-Aufrufe zeigen keinen 10–20-ms-Ausreißer.

## TOP 10

Sortierte einzelne teure Operationen innerhalb des Blocks, jeweils Compiler-Eigenzeit; bei diesen zehn Zeilen jeweils ein Aufruf. Keine überlappenden Subsystem- oder FX-Oberzeiten in derselben Rangliste.

| # | Operation | ms exklusiv | % Shaderblock | % A1 Cold |
| --- | --- | --- | --- | --- |
| 1 | Actors ModernVS (GPU Skinning) | 299.1489 | 22.603 | 8.339 |
| 2 | PBR PrecomputeBRDF_PS | 93.0905 | 7.034 | 2.595 |
| 3 | Water WaterCompositePS | 69.5620 | 5.256 | 1.939 |
| 4 | SSR ComputeSpatialReconstructionPS | 69.3170 | 5.237 | 1.932 |
| 5 | Sky SkyPS | 57.3675 | 4.334 | 1.599 |
| 6 | Sky PrecomputeSingleScatteringCS | 52.7899 | 3.989 | 1.472 |
| 7 | Shared Shadow/AO CompositePS | 44.7837 | 3.384 | 1.248 |
| 8 | Vegetation ModernPS | 43.4539 | 3.283 | 1.211 |
| 9 | Actors ModernPS | 41.1832 | 3.112 | 1.148 |
| 10 | Static World ModernPS | 40.7929 | 3.082 | 1.137 |

## NEXT TARGET — genau ein Kandidat, nicht implementiert

**Persistenter, versionierter D3D11-Shader-Bytecodecache vor der zentralen HLSL-Kompilierung**, unter Wiederverwendung des vorhandenen Diligent-IBytecodeCache.

- Subsystem: gemeinsame Shadererzeugung einschließlich Modern Material/GPU Skinning und eingebundener FX-Pfade.
- Gemessene Kosten: 77 Compileraufrufe, 1282.4268 ms inklusive / 1281.5029 ms exklusiv; 96.83 % des Shaderblocks. Größter einzelner Aufruf: Actors ModernVS 299.1750 ms.
- Ursache: jeder neue Prozess kompiliert aus HLSL; vorhandene PSO-/Shaderobjekt-Caches überleben den Prozess nicht. Erste Mapinitialisierung dominiert, kein erneutes Kompilieren beim Mapwechsel.
- Potenzieller Fix: passenden DXBC-Blob vor CreateShader/Compile laden; bei fehlendem/ungültigem Eintrag unveränderten Source-Pfad ausführen und erfolgreich erzeugten Bytecode atomar für Folgestarts speichern. Original-Compilerflags und Varianten behalten.
- Erwartetes Potenzial: bei bereits gefülltem, gültigem Cache etwa **1.0–1.2 s** weniger A1-Cold-Zeit pro neuem Prozess; rechnerisches Compiler-Maximum 1.2824 s minus Lookup/I/O/Validierung. Das ist eine begründete Schätzung, **kein gemessener Fix-Gewinn**. Erster Start mit leerem Cache spart die eindeutigen Kompilierungen nicht; geringe Wiederholungen innerhalb dieses Starts wären Teil desselben Caches. A1 Warm hat schon 0 Compileraufrufe und daher hier kein vergleichbares Potenzial. Clientstart-Kompilierung wurde separat gemessen und nicht zusätzlich in diese A1-Schätzung aufgenommen.
- Risiko: falsche Cache-Gültigkeit kann falsche Shader laden. Source/Includes/Macros, Stage/EntryPoint, effektives Shaderprofil, Compiler-DLL/Engine-Version, Preamble, Debug/Release-/Compileflags und Backend müssen die Gültigkeit bestimmen. Korruption/Versionswechsel muss zum unveränderten Compile-Pfad zurückfallen. Keine gemeinsame PSO-Identität aus Shader-Bytegleichheit ableiten.
- Betroffener Code: zentraler D3D11-CreateShader-/ShaderD3DBase-Pfad, angebunden über die bestehende gepinnte Core-Integration und einen session-globalen Cachebesitzer; `Graphics/GraphicsTools/interface/BytecodeCache.h` / `src/BytecodeCache.cpp`. Modern `Impl::Shader` allein wäre unvollständig, weil FX intern ebenfalls CreateShader aufruft. `PrepareShaderLoadAudit.py` bleibt Diagnose, nicht der Speicherort eines Produkt-Caches.

## Quellen, Reproduktion und Abschluss

Primäre Quellen sind der gemessene lokale Checkout und die gepinnten Abhängigkeiten: DiligentCore `b036337d68be2353c9950a85929acf796b9a6d50`, DiligentFX `cb380ac52100672b5762f595acfb6609e0ecc248`. Relevante Stellen:

- `src/Renderer/DiligentModernRenderer.cpp`: MeshShaders, GetPipeline, SubmitTerrain, ResetFrame, ReleaseWindowResources, PrepareShadows/FX-/Tone-Aufbau.
- `src/Renderer/DiligentD3D11BackendInternal.h` / `.cpp`: Rendererbesitz, Resize/Style/Shutdown.
- `src/Renderer/DiligentWater.cpp`, `DiligentAtmosphere.cpp`, `DiligentTerrainRenderer.cpp`: Adapter- und Ressourcenlebensdauer.
- `src/Renderer/ShaderLoadAudit.h`, `FirstUseAudit.h`, `src/EterBase/MapLoadTrace.h`: bestehende opt-in Messpunkte und exklusive Zeitsummen.
- `build-deps/DiligentCore/Graphics/GraphicsEngineD3DBase/src/ShaderD3DBase.cpp`: FXC-Aufruf/Flags; `GraphicsEngineD3D11/src/ShaderD3D11Impl.cpp` und `PipelineStateD3D11Impl.cpp`: Mutex, native Shaderobjekte, Ressourcen-Remapping.
- Core `Graphics/GraphicsTools/src/XXH128Hasher.cpp`, `BytecodeCache.cpp`, `Common/interface/HashUtils.hpp`: Identity-/Cache-Grenzen.
- FX `PostProcess/Common/src/PostFXContext.cpp`, SSAO/Bloom/SSR/ShadowMapManager-Quellen sowie `buildtool/Diligent.cmake`, `PrepareDiligentFX.py`.

Lokale, absichtlich nicht gestagte Belege: `build-p0l/l3-analysis/summary.json`, `map-load-trace.tsv`, `fast-gate.json`, `exit.json`, `hashes.json`; `build-p0l3/analysis.json`, `analyze.py`, `temporary-instrumentation.diff`, `instrumented-hashes.json`, `instrumentation-restored.json`, `checkpoint.diff`, `checkpoint-vs-validated.json`, `final-state.json`. Die Tabellen werden aus den frischen exklusiven Costs und den zentralen Events berechnet; Aufteilung und Subsystemsumme sind gegen 1323.5136 ms geprüft.

Temporäre L3-Messpunkte: sechs Source-/Buildtool-Dateien, ausschließlich opt-in Analyse, danach exakt per SHA256 zurückgespielt. Keine permanent ergänzte Profiling-Infrastruktur. Generierte Vendor-Kopien wurden beim anschließenden Release-Build ebenfalls wieder aus dem unveränderten Checkpoint erzeugt. Die vorhandene P0-L/P0-L2-Instrumentierung bleibt Teil des Checkpoints.

Release-Build mit Messpunkten PASS; abschließender Release-Rebuild des restaurierten Checkpoints PASS. Bestehende Python/zlib-PDB-Linkerwarnungen, keine Buildfehler. Ein kurzer nativer A1 → B1 → A1-Lauf PASS: Exit 0, drei stabile Maploads, alle 30 geprüften Fehler-/Fallback-/Ressourcen-Zähler 0, leerer Python-Fehlerlog, Modern/Water/SSR-Fallbacks 0. Kein weiterer nativer Benchmark nach der bytegenauen Source-Restaurierung; der Rebuild prüft die Rücknahme.

Alle 13 GR2-Quelldateien unverändert; übrige geschützte Implementierungen zeigen keinen Git-Diff. Original-Release-EXE weiterhin SHA256 `9039416138E0DBBC61E94B6A0CE7DEA72F401024E5E1FA5943A12D5A11D8490F`, Deployment-Belege unverändert. Beide Repository-Zustände und whitespace checks separat geprüft. Einziges neues Repository-Dokument: dieser Bericht; uncommitted. Build-/Trace-/Analyseartefakte bleiben ignoriert.

Keine komplette Regression Suite, kein GCC/LP64, keine Visual-Galerie, Contentprüfung oder Netzwerkanmeldung. Keine neue visuelle Produktabnahme behauptet. **STOP: kein P0-L3-Fix, kein weiterer Commit, kein Push.**
