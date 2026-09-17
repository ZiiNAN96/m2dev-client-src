# P1 – Runtime Performance Baseline & Frametime Analysis

## P1 SUMMARY

17.09.2026. **Baseline abgeschlossen; keine Performanceoptimierung.** Primärer begrenzender Faktor ist die bestehende Frame-Taktung: rund 60,6 FPS bei etwa 16,5 ms, überwiegend expliziter Sleep. Die Hardwarearbeit bleibt darunter. Daraus lässt sich keine unlimitierte Maximal-FPS ableiten. Der aktuelle Modern-Stand wurde unverändert gemessen, inklusive Schatten, AO, Wasser, HDR, Bloom, Vegetation und authored Effects/Auren.

A1 Idle: CPU aktiv 1.613 ms, GPU-Intervall 0.400 ms; Traversal: CPU 1.152 ms, GPU 0.586 ms. Alle gemessenen Sektoren bleiben resident. Erste Sichtbarkeit erzeugt zusätzliche Texturen/Buffer; das ist von Sektor-Neuaufbau zu unterscheiden.

## SYSTEM / STARTSTAND


| Feld | Wert |
| --- | --- |
| CPU | AMD Ryzen 7 9800X3D, 8 Cores / 16 Threads |
| GPU | NVIDIA GeForce RTX 5070 Ti, 16 GB; vom Diligent-Gerät selbst bestätigt |
| Driver | 616.56 / Windows 32.0.16.1656 |
| Resolution / Display | 1024 × 768, Fenster; Desktop 2560 × 1440, gemeldet 164 Hz |
| Graphics preset | Custom / Modern; Schatten High (3 × 1536), GTAO, Water Ultra, Vegetation High, Textures High, HDR/Bloom/Modern Sky an |
| View distance | 25600; gespeicherter Wert unverändert, wie beim bestehenden Offline-Routenprobe explizit gesetzt |
| VSync | Unverändertes Present(1); die Laufzeit darin wird separat gemessen |
| FPS cap | Bestehender nativer 16/17-ms-Takt (Ø 16,5 ms ≈ 60,61 Hz), unveränderter Frame-Skip-/Sleep-Pfad; kein neues Limit |
| Source | codex/g56-hdr-atmosphere @ 80f77391fe28e533c20e1e686525c7f444c6c2ca |
| Runtime | codex/g56-hdr-colors @ fabe77373bd561fb2f512449915ad5ab3c1fef62 |
| Source log -3 | 80f7739, 873d989, 73dc92e |
| Runtime log -3 | fabe7737, 548c43b4, 6f52f681 |
| Vorhandene Änderungen | Source: untracked p0l3-shader-pso-breakdown.md. Runtime: production-milestone-deployment.md geändert; p0l-map-loading-deployment.json und p0l2-shader-loading-deployment.json untracked. Erhalten. |

Andere Metin2-Prozesse waren vor dem Start geschlossen; der Start prüfte das erneut. Normale Desktop-Anwendungen blieben aktiv. GPU-Takte wurden nicht fixiert. Ein separater Snapshot meldete P3/1732 MHz; das erklärt zeitliche Variation nicht kausal. GPU-Zeiten sind kurze Szenariomessungen bei dynamischen Takten, keine normalisierte Hardwarevergleichszahl.

## Messdefinition / Grenzen

Isolierter Release-Client mit Originalpacks und privatem Root-Paket; nur `prototype.py` ist der Offline-Einstieg. Aktuelle Production-Prewarm-Funktion vor den Szenarien, mindestens 4 s Warmup pro Abschnitt, danach 20–25 s. Kein Netzwerklogin, keine künstliche Gameplay-/KI-Simulation, keine Galerie. Originalcharakter mit Armor 11299, Hair 1001 und Weapon 19; vorhandene Auren bleiben aktiv. „Idle“ bedeutet ohne zusätzlich gestartete Attack-/Skill-Effekte, nicht effektfreie Welt.
Frametime = Abstand der Process-Anfänge tatsächlich präsentierter Renderframes (übersprungene Updates werden mit akkumuliert); pro Abschnitt entfällt nur der erste Frame ohne vorherigen Messpunkt. FPS avg = 1000 / mittlere Frametime, FPS median = 1000 / mediane Frametime. P95/P99 linear interpoliert; keine Ausreißer entfernt. Es handelt sich um clientseitige Render-/Submission-Kadenz, nicht um per PresentMon belegte Monitor-Scanouts.
CPU = exklusive grobe Wallclock-Scopes auf dem Main-/Submission-Thread, ohne Present und Sleep. Kein eigener Renderthread in diesem Pfad. Thread-CPU-Zeit ist zusätzlich vorhanden, wegen Windows-Zeitquantisierung nur im Mittel aussagekräftig. Worker-Auslastung, globale Allocator-/Mutex-/Fence-Zeiten sind **nicht separat gemessen**. GPU = asynchron gelesene Diligent-Timestamps, keine blockierenden Readbacks während der Messung. Intervalle können GPU-Leerlauf durch Submission oder Scheduling einschließen; keine reine Shader-Occupancy-Messung. SSAO umfasst den gemeinsamen PostFX-Vorpass; Skinning ist Bestandteil der Actor-/Shadow-Vertexshader und nicht separat isoliert.
Keine Laufzeit-Dateiausgabe pro normalem Frame: begrenzte Speicherarrays, Export erst beim Shutdown. Der erste Erkundungslauf `pilot/` wird nicht für die finalen CPU-Werte benutzt; dessen feine vorhandene PSO-Lookup-Timer wurden für `baseline/` ausgeschlossen. Verbleibender Messaufwand wird nicht rechnerisch abgezogen. Die GPU- und CPU-Scopes überlappen nicht innerhalb ihrer jeweiligen Tabelle; CPU und GPU laufen parallel und dürfen nicht addiert werden.
Alle 14.251 präsentierten Frames innerhalb der Messabschnitte besitzen passende GPU-Samples; daraus entstehen 14.241 Frametime-Intervalle nach Abzug des ersten Frames je Abschnitt. Keine CPU-/GPU-Samples wegen voller Messarrays verworfen. Die letzte noch offene GPU-Abfrage beim Shutdown liegt außerhalb der Messabschnitte und wird nicht als Nullsample gewertet.

## Szenarien


| Abschnitt | Ausführung |
| --- | --- |
| A1 IDLE | 1 Player bei (63500,59000); Kamera 5500 / 22° / 0° |
| A1 CAMERA | gleiche Position, 360° in 25 s |
| A1 RUN | Player mit Run-Animation; scripted Positionsfolge x=63500+600t, y=59000; kein Navigations-/Inputbenchmark |
| A1 WORLD TRAVERSAL | vorherige BUGFIX-X-Route: x=44000+1600 sin(πt/12), y=25600+1000 sin(πt/18); Kamera 5500+1200 sin(πt/15), Rotation 8t; über Sektorgrenze 25600 |
| A1 ACTORS | 20 bzw. 50 echte Instanzen: 1 Player, übrige NPC 9003 / Mob 101; gleiche Town-Kamera; native Animation/Rendering, keine serverseitige KI |
| A1 COMBAT | 1 Player, wiederholte Combo-Attacke und originale samyeon_d / palbang_spin-Effekte; alle 1,5 s explizit neu erzeugt |
| A1 TRAVERSAL REPEAT | exakt dieselbe Strecke nach bereits gesehenen Town-/World-Ansichten, ohne Mapreload |
| B1 / C1 | kurzes Idle bei (94300,27100) bzw. (44000,27200), je 1 Player |


## A1 / B1 / C1 FRAME METRICS


| Szenario | Frames | Sek. | FPS avg | FPS median | ms avg | Median | P95 | P99 | Max |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A1_IDLE | 1515 | 24.998 | 60.606 | 60.573 | 16.500 | 16.509 | 17.618 | 18.524 | 18.639 |
| A1_CAMERA | 1515 | 24.998 | 60.605 | 60.550 | 16.500 | 16.515 | 17.652 | 18.210 | 19.442 |
| A1_RUN | 1515 | 24.998 | 60.605 | 60.573 | 16.500 | 16.509 | 17.677 | 18.505 | 19.016 |
| A1_TRAVERSAL | 1515 | 24.998 | 60.606 | 60.568 | 16.500 | 16.511 | 18.004 | 18.707 | 19.559 |
| A1_ACTORS_20 | 1515 | 24.997 | 60.606 | 60.570 | 16.500 | 16.510 | 18.015 | 18.762 | 19.483 |
| A1_COMBAT | 1515 | 24.996 | 60.609 | 60.571 | 16.499 | 16.509 | 17.648 | 18.502 | 18.680 |
| A1_ACTORS_50 | 1212 | 19.997 | 60.608 | 60.580 | 16.499 | 16.507 | 17.749 | 18.572 | 19.015 |
| A1_TRAVERSAL_REPEAT | 1515 | 24.998 | 60.604 | 60.553 | 16.500 | 16.514 | 17.984 | 18.611 | 19.225 |
| B1_IDLE | 1212 | 19.998 | 60.606 | 60.555 | 16.500 | 16.514 | 17.733 | 18.173 | 18.801 |
| C1_IDLE | 1212 | 19.997 | 60.609 | 60.573 | 16.499 | 16.509 | 17.740 | 18.125 | 18.724 |

| Szenario | >8,33 ms | >16,67 ms | >20 ms | >33,33 ms | >50 ms |
| --- | --- | --- | --- | --- | --- |
| A1_IDLE | 1515 | 578 | 0 | 0 | 0 |
| A1_CAMERA | 1515 | 613 | 0 | 0 | 0 |
| A1_RUN | 1515 | 606 | 0 | 0 | 0 |
| A1_TRAVERSAL | 1515 | 609 | 0 | 0 | 0 |
| A1_ACTORS_20 | 1515 | 623 | 0 | 0 | 0 |
| A1_COMBAT | 1515 | 610 | 0 | 0 | 0 |
| A1_ACTORS_50 | 1212 | 509 | 0 | 0 | 0 |
| A1_TRAVERSAL_REPEAT | 1515 | 609 | 0 | 0 | 0 |
| B1_IDLE | 1212 | 500 | 0 | 0 | 0 |
| C1_IDLE | 1212 | 484 | 0 | 0 | 0 |

Das 120-FPS-Budget wird wegen der unveränderten Taktung nicht erreicht. Ø ≈60,6 FPS bedeutet nicht, dass jeder Frame unter 16,67 ms liegt; die Quantile zeigen die bestehende Taktungs-/Scheduling-Streuung.

## CPU VS GPU / WAITS


| Szenario | CPU aktiv avg | CPU P99 | CPU max | Thread CPU avg | GPU avg | GPU P99 | GPU max | Present avg | Sleep avg |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A1_IDLE | 1.613 | 2.221 | 3.056 | 1.671 | 0.400 | 1.083 | 1.558 | 0.036 | 14.848 |
| A1_CAMERA | 1.703 | 2.660 | 11.470 | 1.774 | 0.417 | 1.100 | 1.499 | 0.037 | 14.757 |
| A1_RUN | 1.454 | 2.313 | 3.710 | 1.516 | 0.583 | 1.292 | 1.474 | 0.036 | 15.007 |
| A1_TRAVERSAL | 1.152 | 2.236 | 2.957 | 1.176 | 0.586 | 1.220 | 1.560 | 0.037 | 15.309 |
| A1_ACTORS_20 | 2.046 | 2.855 | 4.413 | 2.021 | 0.480 | 1.159 | 1.646 | 0.037 | 14.415 |
| A1_COMBAT | 1.692 | 2.425 | 3.069 | 1.702 | 0.419 | 1.089 | 1.216 | 0.036 | 14.768 |
| A1_ACTORS_50 | 2.635 | 3.383 | 5.494 | 2.669 | 0.567 | 1.208 | 1.450 | 0.036 | 13.827 |
| A1_TRAVERSAL_REPEAT | 1.124 | 1.657 | 2.592 | 1.145 | 0.588 | 1.293 | 1.543 | 0.037 | 15.335 |
| B1_IDLE | 0.871 | 1.329 | 1.548 | 0.825 | 0.337 | 0.987 | 1.374 | 0.036 | 15.591 |
| C1_IDLE | 0.875 | 1.339 | 1.490 | 0.812 | 0.419 | 1.077 | 1.424 | 0.036 | 15.587 |

**PRIMARY LIMIT (A1): synchronization / bestehende Frame-Taktung. SECONDARY LIMIT:** CPU-Submission in der Bewegung als größter aktiver CPU-Bereich; GPU-Passkosten variieren nach Szene und Takt. Keine dieser Arbeitszeiten begrenzt die gemessenen ≈60,6 FPS dauerhaft. **STUTTER LIMIT:** anhand der >20-ms-Grenze unten; kein ungeprüfter CPU-/GPU-bottleneck behauptet.
Explizite WaitForIdle/Flush-Aufrufe liegen laut Quellprüfung im Shutdown sowie Screenshot-/Wasser-Diagnosereadback. Diese Readbacks wurden nicht benutzt; Shutdown liegt außerhalb der Samples. Globale Driver-Flushes, blockierende MapBuffer-Interna, Worker-Joins und Mutex/Fence-Waits wurden nicht separat abgegriffen. Die reservierten `wait_idle`/`flush`-CSV-Spalten sind keine instrumentierten Nachweise und werden als **N/A** behandelt, nicht als gemessene Null.

## TOP 5 CPU COSTS


| Rang / Subsystem (A1 Idle) | ms/Frame | % aktive CPU | Verhalten / mögliche Ursache |
| --- | --- | --- | --- |
| 1. Static World / Visibility / Submission | 0.333 | 20.639 | stabile Basiskosten; residenter World-Durchlauf und viele einzelne Draws |
| 2. Shadow-Submission | 0.287 | 17.775 | stabile Basiskosten; wiederholte Cascade-Draws und Binding-Arbeit |
| 3. Forward World / UI-Callback-Rest | 0.208 | 12.906 | stabile Basiskosten; grober gemessener Scope |
| 4. Vegetation-Rest | 0.164 | 10.141 | stabile Basiskosten; grober gemessener Scope |
| 5. Terrain-Submission | 0.148 | 9.152 | stabile Basiskosten; Splat-/Patch-Submission |

Prozentwerte beziehen sich auf aktive CPU, nicht die 16,5-ms-Wallframetime. Kein einzelner stabiler Subsystemblock erreicht ~10 % der gesamten Wallframetime; deshalb keine breite Mikroprofilierung. Vollständige Phasen einschließlich World Update, Actors Update, Animation, Skinning Preparation, Effects, Culling, LOD, Python und Resource Cache: `build-p1/summary.json`. Die Rohphase `ui` ist der exklusive Rest des Render-Callbacks: Sie umfasst auch nicht einzeln abgegrenzte Forward-World-Arbeit (Blocker, Items, Snow, Fly) und darf nicht als reine UI-Kosten gelesen werden. Eigentliche UI hier nur Offline-Window/Cursor, ohne Gameplay-HUD/Inventar; ihr separater Anteil ist N/A.

## TOP 5 GPU COSTS


| Rang / Pass (A1 Idle) | GPU ms | % GPU-Intervall | Draws/Frame |
| --- | --- | --- | --- |
| 1. GTAO / gemeinsamer PostFX-Vorpass | 0.161 | 40.132 | 18.000 |
| 2. Terrain | 0.065 | 16.301 | 179.000 |
| 3. Bloom | 0.034 | 8.586 | 12.000 |
| 4. Forward World / UI-Callback-Rest | 0.034 | 8.503 | 73.000 |
| 5. Sky / Atmosphere | 0.032 | 7.882 | 1.000 |

| Pass | Idle GPU ms | Traversal GPU ms | Idle Draws | Idle Dreiecke |
| --- | --- | --- | --- | --- |
| Shadows | 0.030 | 0.046 | 585.000 | 201,843 |
| Terrain | 0.065 | 0.103 | 179.000 | 51,968 |
| Static World | 0.015 | 0.003 | 96.000 | 33,911 |
| Actors inkl. Vertex-Skinning | <0.001 | <0.001 | 10.000 | 3,063 |
| Vegetation | 0.002 | 0.005 | 23.000 | 8,094 |
| Water / SSR | <0.001 | 0.164 | 0.000 | 0 |
| GTAO / gemeinsamer PostFX-Vorpass | 0.161 | 0.160 | 18.000 | 18 |
| Lighting / Composition | 0.019 | 0.022 | 1.000 | 1 |
| Sky / Atmosphere | 0.032 | 0.032 | 1.000 | 1 |
| Effects | 0.003 | 0.005 | 29.393 | 59 |
| Bloom | 0.034 | 0.039 | 12.000 | 12 |
| HDR / Tone Mapping | 0.005 | 0.005 | 1.000 | 1 |
| Forward World / UI-Callback-Rest | 0.034 | 0.003 | 73.000 | 13,155 |
| Other | 0.000 | 0.000 | 0.000 | 0 |

PBR/Legacy-Materialarbeit ist in den Geometrie-Pässen enthalten; Lighting ist die anschließende Composition. Es gibt keinen separat gemessenen universellen PBR-Pass. GPU-Peaks sind nicht automatisch Frametime-Spikes: CPU-Submission und GPU-Verarbeitung sind asynchron.

## DRAW CALLS / GEOMETRIE / UPLOADS


| Szenario | Draws | Indexed | Instanced* | Instanzen* | PSO calls | SRB commits | Dreiecke | MapBuffer | bekannte Upload KiB |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A1_IDLE | 1028.393 | 968.000 | 48.000 | 132.000 | 110.000 | 1028.393 | 312,126 | 1032.785 | 488.655 |
| A1_CAMERA | 1056.349 | 995.646 | 54.982 | 150.119 | 111.855 | 1056.349 | 322,311 | 1061.354 | 503.097 |
| A1_RUN | 854.276 | 772.425 | 79.431 | 204.040 | 135.037 | 854.276 | 233,625 | 854.236 | 513.966 |
| A1_TRAVERSAL | 517.385 | 410.702 | 31.267 | 94.739 | 133.419 | 517.385 | 161,840 | 536.182 | 539.773 |
| A1_ACTORS_20 | 1123.393 | 1063.000 | 48.000 | 132.000 | 110.000 | 1123.393 | 398,070 | 1146.787 | 840.155 |
| A1_COMBAT | 1050.774 | 987.000 | 48.000 | 132.000 | 111.772 | 1050.774 | 317,006 | 1058.549 | 503.699 |
| A1_ACTORS_50 | 1243.392 | 1183.000 | 48.000 | 132.000 | 110.000 | 1243.392 | 525,390 | 1296.784 | 1380.155 |
| A1_TRAVERSAL_REPEAT | 521.964 | 415.292 | 31.267 | 94.733 | 133.422 | 521.964 | 163,997 | 540.744 | 542.061 |
| B1_IDLE | 429.404 | 295.000 | 9.000 | 18.000 | 81.000 | 429.404 | 102,712 | 507.809 | 205.349 |
| C1_IDLE | 413.387 | 347.000 | 21.000 | 42.000 | 126.000 | 413.387 | 137,239 | 423.774 | 192.589 |

Alle Werte Mittel pro gerendertem Frame. Diligent-Zähler erfassen tatsächliche Draw-/Indexed-/Map-/Commit-Aufrufe und Dreiecke einschließlich Schatten. PSO-Aufrufe sind **keine Zahl tatsächlicher Zustandswechsel**; SRB-Commits sind **keine Materialwechselzahl**. Echte Wechselzahlen sind N/A. *Instanced = Modern-Mesh-Draws mit NumInstances>1, summierte Instanzen dieses Teilbereichs. Vertex-Zähler erfasst nur Modern-Mesh-Referenzen, nicht alle Pass-Vertices; daher keine scheinpräzise Gesamtvertexzahl.

| Szenario | sichtbare Terrainpatches | sichtbare Vegetation* | Vegetation culled* | Instance updates | Instance KiB | Vertex updates | Constant updates | Temp-Vector growth |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A1_IDLE | 43.000 | 28.000 | 708.000 | 0.000 | 0.000 | 29.393 | 1001.393 | 80.000 |
| A1_CAMERA | 42.726 | 32.566 | 703.434 | 0.304 | 0.051 | 29.702 | 1029.349 | 93.622 |
| A1_RUN | 42.759 | 41.147 | 694.853 | 0.131 | 0.025 | 37.026 | 814.265 | 119.922 |
| A1_TRAVERSAL | 28.260 | 26.938 | 709.062 | 0.114 | 0.017 | 58.683 | 474.385 | 78.755 |
| A1_ACTORS_20 | 43.000 | 28.000 | 708.000 | 0.000 | 0.000 | 29.393 | 1096.393 | 80.000 |
| A1_COMBAT | 43.000 | 28.000 | 708.000 | 0.000 | 0.000 | 32.774 | 1023.774 | 80.000 |
| A1_ACTORS_50 | 43.000 | 28.000 | 708.000 | 0.000 | 0.000 | 29.392 | 1216.392 | 80.000 |
| A1_TRAVERSAL_REPEAT | 28.260 | 26.935 | 709.065 | 0.108 | 0.016 | 58.672 | 478.964 | 78.747 |
| B1_IDLE | 43.000 | 15.000 | 619.000 | 0.000 | 0.000 | 103.404 | 402.404 | 44.000 |
| C1_IDLE | 30.000 | 23.000 | 569.000 | 0.000 | 0.000 | 35.387 | 386.387 | 67.000 |

Bekannte Upload-Bytes zählen die instrumentierten Material-/Terrain-/Effect-/Water-Konstanten, Splat-/Effect-/Water-Vertices, Instanzen und Skinning-Palette; FX-interne und UI-Uploads sind nicht vollständig byteinstrumentiert. Das ist eine **Untergrenze**, keine vollständige PCIe-/Driver-Transfermessung. Buffer-Mappingzahl umfasst dagegen den gesamten Diligent-Kontext. Unveränderte Vegetationsinstanzen werden im Idle nicht neu hochgeladen; Konstanten-/Passdaten werden weiterhin pro Draw geschrieben. Ob einzelne konstante Felder unverändert sind, wurde nicht per Bytevergleich bewiesen.
Allocation-Churn: gezählt wurden notwendige Kapazitätsvergrößerungen temporärer Vegetationsgruppen. Die Tabellenzahl ist ein echter Teilzähler, **keine globale allocations/frame- oder frees/frame-Zahl**. Frees, alle temporären Container und Allocator-Lock-Zeit sind N/A. `temp_bytes` ist nur eine Untergrenze angeforderter Elementbytes. Die gesamte Instanzvorbereitung begrenzt das mögliche Einsparpotenzial dieses Teilbereichs; keine große Heap-Profiling-Infrastruktur ergänzt.
GPU-Texture-Creates erscheinen als Abschnittssummen in der folgenden Streaming-Tabelle; Upload-Bytes dieser Texturen und dynamische Index-Uploads sind nicht separat erfasst. Allgemeine Static-Object-Visible-/Culled-Zahlen sind N/A; Actor-Mengen, Vegetation und Terrain sind separat belegt. Buffer-Creates ohne Instance-Markierung werden nicht automatisch als wiederholter Aufbau derselben statischen Geometrie klassifiziert: Dafür fehlt eine Ressourcenidentitätsverfolgung.

## WORLD / SECTOR / RESIDENCY / LOD


| Szenario | Terrain load/unload | Object load/unload | Instance Buffer creates | davon Wachstum | Terrain rebuild | Patch assignment | LOD-Erstwahl | echte Treewechsel | Bushwechsel | Repeat <0,5s | Terrain LOD netto |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A1_IDLE | 0/0 | 0/0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| A1_CAMERA | 0/0 | 0/0 | 30 | 12 | 0 | 0 | 49 | 0 | 0 | 0 | 389 |
| A1_RUN | 0/0 | 0/0 | 21 | 12 | 0 | 0 | 5 | 0 | 0 | 0 | 155 |
| A1_TRAVERSAL | 0/0 | 0/0 | 6 | 3 | 0 | 0 | 7 | 0 | 0 | 0 | 227 |
| A1_ACTORS_20 | 0/0 | 0/0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| A1_COMBAT | 0/0 | 0/0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| A1_ACTORS_50 | 0/0 | 0/0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| A1_TRAVERSAL_REPEAT | 0/0 | 0/0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 230 |
| B1_IDLE | 0/0 | 0/0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| C1_IDLE | 0/0 | 0/0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

| Szenario | Veg LOD0* | LOD1* | LOD2* | Impostor* | Tree-/Bushwechsel/s | Terrain LOD0 | LOD1 | LOD2 | Terrainwechsel/s |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A1_IDLE | 11.000 | 0.000 | 0.000 | 0.000 | 0.000 | 13.000 | 9.000 | 21.000 | 0.000 |
| A1_CAMERA | 15.223 | 0.000 | 0.000 | 0.000 | 0.000 | 15.039 | 12.954 | 14.733 | 15.561 |
| A1_RUN | 20.879 | 0.000 | 0.000 | 0.000 | 0.000 | 12.248 | 9.945 | 20.567 | 6.201 |
| A1_TRAVERSAL | 11.671 | 0.000 | 0.000 | 0.000 | 0.000 | 14.435 | 9.120 | 4.705 | 9.081 |
| A1_ACTORS_20 | 11.000 | 0.000 | 0.000 | 0.000 | 0.000 | 13.000 | 4.000 | 26.000 | 0.000 |
| A1_COMBAT | 11.000 | 0.000 | 0.000 | 0.000 | 0.000 | 13.000 | 4.000 | 26.000 | 0.000 |
| A1_ACTORS_50 | 11.000 | 0.000 | 0.000 | 0.000 | 0.000 | 13.000 | 4.000 | 26.000 | 0.000 |
| A1_TRAVERSAL_REPEAT | 11.668 | 0.000 | 0.000 | 0.000 | 0.000 | 14.436 | 9.119 | 4.705 | 9.201 |
| B1_IDLE | 12.000 | 0.000 | 0.000 | 0.000 | 0.000 | 13.000 | 10.000 | 20.000 | 0.000 |
| C1_IDLE | 11.000 | 0.000 | 0.000 | 0.000 | 0.000 | 13.000 | 10.000 | 7.000 | 0.000 |

*Vegetationsverteilung aus gemessener sichtbarer Hauptpass-Anzahl und geprüfter Production-Policy abgeleitet, nicht separat gemessene stabile LOD-Bins: Alle 16 tatsächlich geladenen Vegetations-Assettypen sind Version 1 und werden nativ als Tree klassifiziert. `fixedTreeDetail=true` setzt deren stabile Auswahl auf LOD0. Die anfänglichen Rohbins `tree_lod0` bis `tree_impostor` ordnen historische authored Sample-Indizes fälschlich vier Detailstufen zu; sie werden ausdrücklich nicht als LOD-/Impostor-Verteilung verwendet. Nur ihre Summe zählt sichtbare Instanzen korrekt. Metadaten, Hashes und Policy-Nachweis: `build-p1/lod-policy-audit.json`. Sichtbare Vegetation in der vorherigen Upload-Tabelle zählt hingegen Farb- und Schattenpasses und kann Instanzen mehrfach enthalten.
Echte Wechsel trennen eine erste leere LOD-Auswahl von späteren Änderungen; wiederholt heißt weitere Änderung derselben Instanz innerhalb von 0,5 s. Die separaten Wechselzähler sind von der verworfenen Histogramm-Zuordnung unabhängig. Terrainwechsel zählen den finalen gespeicherten Zustand nach Nachbarschaftsausgleich, nicht Zwischenentscheidungen. Kein Hysterese-/LOD-Verhalten geändert.
**Explizite Antworten:** Beim normalen Laufen kein Terrain-/Object-Sektor-Neuaufbau; A1 hält durchgehend 20/20 Sektoren und 368 World-Trees. Instance Buffer werden bei neuer Sichtbarkeit angelegt/vergrößert: Traversal 6, identische Wiederholung 0. Terrain-Patch-Rebuilds und -Neuzuweisungen während der Abschnitte: siehe Nullzähler oben. Sichtbarkeit und LOD-Verteilung ändern sich erwartungsgemäß. Permanentes World-Load/Unload/Rebuild-Churn ist auf dieser Route nicht nachgewiesen. Der frühere Residency-Popping-Fehler ist hier **nicht messbar reproduziert**; sichtbares Popping wurde ohne manuelle Sichtprüfung nicht als behoben bestätigt. Vegetation bleibt resident; keine separate Vegetations-Sektorarchitektur. Grass ist im vorgefundenen Originalterrain-Stand 0 Placements und wurde nicht für P1 abgeschaltet.

## ASSET / STREAMING / CPU-BURSTS / FRAME SPIKES


| Szenario | Pack reads | GPU Buffer creates | GPU Texture creates | GR2 / GLB parses | Shader compiles | max CPU-I/O ms/Frame |
| --- | --- | --- | --- | --- | --- | --- |
| A1_IDLE | 0 | 0 | 0 | 0/0 | 0 | 0.000 |
| A1_CAMERA | 241 | 313 | 286 | 0/0 | 0 | 0.389 |
| A1_RUN | 12 | 31 | 12 | 0/0 | 0 | 0.129 |
| A1_TRAVERSAL | 51 | 34 | 56 | 0/0 | 0 | 0.138 |
| A1_ACTORS_20 | 0 | 0 | 0 | 0/0 | 0 | 0.000 |
| A1_COMBAT | 102 | 0 | 102 | 0/0 | 0 | 0.103 |
| A1_ACTORS_50 | 0 | 0 | 0 | 0/0 | 0 | 0.000 |
| A1_TRAVERSAL_REPEAT | 30 | 0 | 30 | 0/0 | 0 | 0.042 |
| B1_IDLE | 0 | 0 | 0 | 0/0 | 0 | 0.000 |
| C1_IDLE | 0 | 0 | 0 | 0/0 | 0 | 0.000 |

Pack reads sind synchrone logische Reads inklusive Pack-Decompression/Decrypt; keine behaupteten physikalischen Disk Reads. Auch nach vollständiger Map-Residency entstehen bei erster Sichtbarkeit GPU-Ressourcen. Combat erzeugt bewusst Effekte neu; dessen erneute Texture-Creates/Pack-Reads sind mit diesem Lifecycle verknüpft und dürfen nicht pauschal jeder regulären Skill-Ausführung zugeschrieben werden. Frame-I/O ist eine Summe; liegt sie unter 5 ms, kann darin kein einzelner gemessener I/O-Aufruf >5 ms verborgen sein. Shader compile bleibt 0.

| CPU-Burst Frame | Szenario | aktive CPU ms | größte CPU-Phase | Phasen-ms | Pack read / Buffer create / Texture create |
| --- | --- | --- | --- | --- | --- |
| 2128 | A1_CAMERA | 11.470 | cpu_static_world | 2.188 | 0 / 0 / 0 |
| 2092 | A1_CAMERA | 10.203 | cpu_effects | 8.258 | 1 / 1 / 1 |
| 11671 | A1_ACTORS_50 | 5.494 | cpu_static_world | 1.046 | 0 / 0 / 0 |

Größte summierte gemessene I/O-Zeit pro Frame: 0.389 ms; kein gemessener I/O-Stall >5 ms. Der größte CPU-Burst (11,471 ms) hat keine gezählten Resource-Creates/Pack-Reads; seine Ursache bleibt offen. Beim 10,203-ms-Burst fallen 8,259 ms im Effect-Scope und je ein Pack-Read/Buffer-/Texture-Create zusammen. Das belegt zeitliche Koinzidenz, nicht dass die Ressourcenerstellung die gesamte Phasenzeit verursacht. Beide bleiben unter der >20-ms-Frametime-Grenze.
**Frames >20 ms: 0.** Spitzenliste samt CPU/GPU, größter Phase, Sektor-/Buffer-/Asset-/Allocation-Ereignissen liegt in `build-p1/spikes.csv` und `.json`. Nicht instrumentierte globale Synchronisationsereignisse sind N/A. Keine unbelegten Ursachen oder erfundenen drei Stutter-Ursachen.

| Größte Frametime je Szenario (Top 3) | ms |
| --- | --- |
| A1_TRAVERSAL | 19.559 |
| A1_ACTORS_20 | 19.483 |
| A1_CAMERA | 19.442 |

**TOP STUTTER CAUSES:** Da kein Frame >20 ms auftrat, gibt es für diese Grenze keine belegte Ursachenrangliste. Separat vorhandene CPU-/GPU-Bursts beim ersten Sichtkontakt sind echte Messwerte, aber kein Beweis eines sichtbaren Stutters. Die bestehende Sleep-/Scheduling-Streuung erklärt den größten beobachteten Unterschied zwischen Arbeitszeit und Frameintervall; keine Frame-Pacing-Änderung vorgenommen.

## EFFECTS / ACTOR SCALING


| Szenario | CPU aktiv | CPU Actor Update | CPU Animation | CPU Skin Prepare | CPU Actor Render | CPU Effect Update+Render | GPU Actor | GPU Effects | GPU gesamt | Draws | Upload KiB |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A1_IDLE | 1.613 | 0.006 | 0.039 | 0.002 | 0.058 | 0.059 | <0.001 | 0.003 | 0.400 | 1028.393 | 488.655 |
| A1_COMBAT | 1.692 | 0.008 | 0.042 | 0.003 | 0.057 | 0.065 | <0.001 | 0.045 | 0.419 | 1050.774 | 503.699 |
| A1_ACTORS_20 | 2.046 | 0.012 | 0.214 | 0.006 | 0.176 | 0.062 | 0.005 | 0.002 | 0.480 | 1123.393 | 840.155 |
| A1_ACTORS_50 | 2.635 | 0.019 | 0.461 | 0.012 | 0.381 | 0.060 | 0.016 | 0.002 | 0.567 | 1243.392 | 1380.155 |

NPC/Mob-Verteilung: 20 = 1/10/9, 50 = 1/25/24. Sichtbare Mengen sind zusätzlich in `terrain-renderer.log` bestätigt. Keine große Crowd-Suite; keine KI-/Netzwerkskalierungsbehauptung. GPU-Skinning ist in Actor- und Shadow-Draws integriert; die GPU-Spalte Actor allein enthält daher nicht den gesamten Skinning-Anteil aller Cascades. Kameras/Settings bleiben gleich; GPU-Differenzen zwischen zeitlich getrennten Szenen sind wegen variabler Takte keine reine Effektkosten-Subtraktion.

## GENAU DREI NÄCHSTE OPTIMIERUNGSZIELE — KEIN FIX IMPLEMENTIERT


| Target | Gemessene Kosten / Potenzial | Risiko | Codebereich |
| --- | --- | --- | --- |
| TARGET #1: Static-/Shadow-Submission und wiederholte Draw-Konstanten | A1 Idle Static 0.333 + Shadow 0.287 ms CPU; 1028.393 Draws und ≥488.655 KiB bekannte Uploads/Frame. Gemessenes Kostenbudget ist die obere Grenze; ein Teilgewinn muss erst belegt werden. Keine FPS-Zusage unter bestehendem Limit. | mittel: Cascade-Culling, Materialzustände und authored Effekte müssen identisch bleiben | Renderer/DiligentModernRenderer.cpp; GameLib/MapOutdoorRender.cpp; StaticObjectBridge |
| TARGET #2: Erste Sichtbarkeit / Ressourcen-Cache und Effect-Lifecycle | Camera: 313 Buffer- und 286 Texture-Creates; ein 8,259-ms-Effect-Scope fällt mit je einem Read/Buffer-/Texture-Create zusammen. Wiederholte Strecke: 0/30 Creates und 30 Reads. Potenzial: vermeidbare First-Use-/Lifecycle-Arbeit glätten; der kausale Kostenanteil ist noch offen. Größten CPU-Burst nicht pauschal Ressourcen zuordnen. | mittel: Mehrspeicher und längerer Map-Prewarm gegen First-Use-Kosten abwägen; Effect-Lifecycle gesondert prüfen | EterLib/StaticObjectTextureLoader.cpp; Renderer/DiligentEffectRenderer.cpp; Renderer/DiligentStaticObjectRenderer.cpp |
| TARGET #3: GTAO / gemeinsamer PostFX-Aufwand | Idle 0.161 ms GPU, Traversal 0.160 ms. Optimierbare Obergrenze ist der gemessene Pass; tatsächlicher Teilgewinn offen, bei fixem FPS-Takt derzeit primär GPU-Reserve. | mittel bis hoch: Bildqualität und DiligentFX-Passabhängigkeiten; AO nicht einfach abschalten oder Qualität reduzieren | Renderer/DiligentModernRenderer.cpp::End; bestehende DiligentFX PostFXContext / ScreenSpaceAmbientOcclusion |


## FAST GATE / INTEGRITÄT / NACHWEISE

Release-Instrumentation gebaut; A1 A–F, 1/20/50 Actors, A1-Repeat und B1/C1 offline ausgeführt. `baseline/fast-gate.json`: Exit 0, Diligent ERROR/FATAL 0, GPUFallbacks 0, AllCPUDeformationCalls/Vertices 0, geprüfte Shutdown-Ressourcen 0. Keine vollständige Testsuite, kein Netzwerklogin, keine Visual-Galerie, kein GCC/LP64. Der initiale SDK-Zugriffsfehler in der Sandbox war kein Produktfehler; der Build in der installierten VS-x64-Umgebung bestand.
Produktions-EXE, Packs, Configs und vorhandene Runtime-Dokumente sind hashgleich: 112 geprüfte Dateien, 0 Abweichungen (`protected-before.json`, `protected-after.json`, `final-integrity.json`). Im privaten Root-Verzeichnis unterscheidet sich von 342 Dateien nur `prototype.py`. Alle 17 temporär instrumentierten Source-Dateien wurden bytegenau auf die gesicherten Originale zurückgesetzt; zwei Hilfsheader entfernt. Patch, Instrumentierungsquellen und private Mess-EXEs liegen ausschließlich unter `build-p1/`. Der normale Release-Build nach Wiederherstellung bestand ebenfalls (Exit 0); der wiederverwendete Buildoutput enthält wieder den normalen Source-Stand. Kein Deployment in `m2dev-client`, kein Commit, kein Push, kein P2.

### Kontrolle mit unveränderter Production-EXE


| Szenario | FPS avg | Frametime P99 | Max | RenderGame CPU avg |
| --- | --- | --- | --- | --- |
| A1_IDLE | 60.620 | 18.343 | 19.627 | 1.420 |
| A1_TRAVERSAL | 60.611 | 18.589 | 19.849 | 0.951 |

Privater Client, unveränderte produktive EXE per SHA256, derselbe Offlinepfad/Settings. Nur Python-Clock um RenderGame und Zeitabstände der Render-Callbacks; keine nativen P1-Timer oder GPU-Queries. Unterschiedliche Prozesse, Warmzustände und dynamische GPU-Takte erlauben keine exakte Overhead-Subtraktion. Diese Kontrolle prüft die Größenordnung der Kadenz, nicht eine behauptete Beschleunigung.

Rohbelege: `build-p1/baseline/{p1-frames.csv,p1-gpu.csv,p1-lod-events.csv,p1-run.log,p1-adapter.txt,launch.json,exit.json,fast-gate.json}`; vollständige Statistik `build-p1/summary.json`; Spitzen `build-p1/spikes.csv`; Route `build-p1/entry.py`; Analyse `build-p1/analyze.py`; Build-/Restaurierungs-/Hashbelege im selben Evidenzverzeichnis. Hauptmessung und ältere Erkundungsläufe sind ausdrücklich getrennt. **STOP nach P1.**
