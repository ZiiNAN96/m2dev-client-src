# P2 – Runtime Performance Optimization

17.09.2026. **P2: GO.** Drei begrenzte Änderungen: Mesh-Ressourcenbindungen wiederverwenden, Effekttexturen an ihre nativen Bilder binden und optionale Erfolgsdiagnosen gepuffert schreiben. Primary Limit der uncapped Offline-Szenarien: **CPU / Main-Thread-Submission**. Keine Änderung an Grafikqualität, Welt-Residency oder produktiver FPS-Taktung.

## Ausgangsstand und Messverfahren

| Feld | Wert |
| --- | --- |
| Source | `main` @ `ed3e744`; sauberer Start; P2 auf `feature/p2-runtime-performance` |
| Runtime | `main` @ `d54637a4`; produktive Binärdateien, Packs und Konfiguration unverändert |
| CPU / GPU | AMD Ryzen 7 9800X3D (8C/16T); NVIDIA GeForce RTX 5070 Ti |
| Treiber / Desktop | 32.0.16.1656; 2560 × 1440, 164 Hz |
| Messfenster | 1024 × 768; Modern, Shadows High (3 × 1536), GTAO, Water Ultra, Vegetation/Textures High, HDR/Bloom/Modern Sky an |
| Distanz / Inhalte | 25600; Originalpacks und unveränderte P1-Routen, Armor 11299 / Hair 1001 / Weapon 19 |
| Timing | Profiling: Present(0), kein Frame-Sleep; ursprüngliche alternierende 16/17-ms-Spielschritte; Produktion bleibt unverändert begrenzt |
| Umfang | 10 Abschnitte, jeweils 20–25 s nach mindestens 4 s Warmup; A1 Idle/Camera/Run/Traversal/20 Actors/Combat/50 Actors/Traversal Repeat/B1/C1 |
| Isolation | Privater Release-Client, privates Root-Paket mit genau einer geänderten Datei (prototype.py); kein Netzwerklogin, kein Deployment |

Die native Spiel-/UI-Aktualisierung bleibt bei etwa 60,6 Hz. Zusätzliche Renderframes verwenden den aktuellen Spielzustand; der im Deform-Pfad liegende Weapon-Trace-Spline wird im Profiling nur einmal pro Spielzeitpunkt fortgeschrieben. Sonst entstehen bei identischer Zeit doppelte Stützpunkte mit Division durch null. Diese Unterstützung ist kein Produktionsfix und wurde zusammen mit der Instrumentierung entfernt. Gemessen wird unlimitierter Render-/Submission-Durchsatz bei ursprünglicher Simulation, keine auf 800 Hz beschleunigte Spielwelt und kein neuer Frame-Pacing-Modus.

Die Routen entsprechen [P1](p1-runtime-baseline.md): Camera 360°/25 s; Run x=63500+600t; Traversal x=44000+1600 sin(πt/12), y=25600+1000 sin(πt/18), Kameradistanz 5500+1200 sin(πt/15), Rotation 8t. 20/50 Actors sind ein Player plus NPC 9003/Mob 101 ohne Server-KI. Combat startet die originalen Combo-/samyeon_d-/palbang_spin-Effekte. B1/C1 sind kurze Offline-Idles, keine vollständigen Gameplay-/UI-Abnahmen.

Frametime ist der Abstand präsentierter Process-Anfänge; FPS avg = 1000 / Mittelwert, FPS median = 1000 / Median. P95/P99 linear interpoliert, keine Ausreißer entfernt. CPU-Werte sind exklusive Wallclock-Scopes des Main-/Submission-Threads ohne Present/Sleep. GPU-Werte sind asynchron gelesene Timestamp-Intervalle; sie können Scheduling-/Submission-Leerlauf enthalten. CPU/GPU laufen parallel und werden nicht addiert. Kein PresentMon-Scanout-Nachweis, keine fixierten GPU-Takte, keine Worker-/Mutex-/Driver-Wait-Zerlegung. `wait_idle`/`flush` sind nicht separat instrumentiert und werden nicht als gemessene Null behauptet.

CPU `ui` und GPU `ui` enthalten den restlichen Forward-World-/UI-Callback, hier nur Offline-Window/Cursor. Sie sind keine isolierte Messung des Gameplay-HUD. Render-Submission steckt in den jeweiligen Draw-Phasen. GPU-Skinning steckt in Actor- und Shadow-Vertexshadern, nicht in einem separaten Compute-Pass. Instanzen/Uploads sind die instrumentierten Draw-/Upload-Zähler, kein vollständiges Speicher- oder Allocatorprofil.

Vorher und nachher benutzen dieselben groben CPU-/GPU-Scopes und gepufferten Detailereignisse. Exporte erfolgen nach der Messung; periodische vorhandene Terrain-/First-Use-Diagnosen sind während der Aufnahme gepuffert bzw. ausgesetzt. Die unveränderte synchrone Effect-Diagnose blieb bis Fix 3 aktiv, damit ihr Stutter isoliert nachweisbar ist. Der zusätzliche First-Draw-Probe wurde nur zur Ursachenanalyse verwendet; kleine Zwischenläufe enthalten dessen einmalige Zeitmarken. Der finale Vergleich benutzt wieder den ursprünglichen Probe-Umfang. Messaufwand wird nicht rechnerisch abgezogen.

Alle 188,964 Vorher- und 210,121 Nachher-Frametime-Intervalle besitzen GPU-Samples. CPU-/Detail-/GPU-Drops: 0. Je Lauf bleibt nur eine GPU-Abfrage nach dem letzten Messabschnitt offen; sie ist kein Nullsample. Spielzeit/Wandzeit und Tickzähler sind in den Rohdaten enthalten. Frühe Versuche mit noch aktivem Limiter, abweichendem Spieltakt, fehlenden Scopes oder parallel gestartetem Benutzer-Client sind ausdrücklich ausgeschlossen. Nach Bestätigung des Nutzers liefen die gültigen Messungen ohne zweiten Client.

## Uncapped Baseline / Abschlussvergleich

### BEFORE – neuer uncapped Ausgangsstand

| Szenario | Frames | s | FPS avg | FPS median | FT Median ms | P95 | P99 | Max |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A1_IDLE | 17644 | 25.012 | 705.4 | 712.0 | 1.404 | 1.531 | 1.718 | 9.146 |
| A1_CAMERA | 16917 | 25.012 | 676.4 | 684.6 | 1.461 | 1.635 | 2.012 | 9.720 |
| A1_RUN | 19605 | 25.013 | 783.8 | 769.6 | 1.299 | 1.513 | 1.657 | 5.035 |
| A1_TRAVERSAL | 24770 | 25.013 | 990.3 | 1024.6 | 0.976 | 1.291 | 1.522 | 5.553 |
| A1_ACTORS_20 | 13648 | 25.012 | 545.7 | 550.2 | 1.817 | 1.972 | 2.169 | 3.496 |
| A1_COMBAT | 16768 | 25.013 | 670.4 | 677.8 | 1.475 | 1.622 | 1.919 | 2.666 |
| A1_ACTORS_50 | 8538 | 20.013 | 426.6 | 428.7 | 2.332 | 2.500 | 2.667 | 3.832 |
| A1_TRAVERSAL_REPEAT | 24685 | 25.013 | 986.9 | 1021.5 | 0.979 | 1.286 | 1.524 | 2.188 |
| B1_IDLE | 23178 | 20.014 | 1158.1 | 1207.3 | 0.828 | 1.138 | 1.376 | 12.810 |
| C1_IDLE | 23211 | 20.014 | 1159.7 | 1221.2 | 0.819 | 1.234 | 1.472 | 2.071 |

### AFTER – alle drei Fixes

| Szenario | Frames | s | FPS avg | FPS median | FT Median ms | P95 | P99 | Max |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A1_IDLE | 20661 | 25.013 | 826.0 | 835.1 | 1.197 | 1.342 | 1.560 | 8.846 |
| A1_CAMERA | 20129 | 25.013 | 804.7 | 816.2 | 1.225 | 1.412 | 1.657 | 7.408 |
| A1_RUN | 22089 | 25.014 | 883.1 | 888.5 | 1.125 | 1.340 | 1.562 | 4.832 |
| A1_TRAVERSAL | 25818 | 25.013 | 1032.2 | 1082.3 | 0.924 | 1.425 | 1.652 | 6.653 |
| A1_ACTORS_20 | 16130 | 25.012 | 644.9 | 650.8 | 1.537 | 1.691 | 1.913 | 2.506 |
| A1_COMBAT | 20232 | 25.014 | 808.8 | 819.6 | 1.220 | 1.380 | 1.543 | 5.774 |
| A1_ACTORS_50 | 9829 | 20.013 | 491.1 | 495.5 | 2.018 | 2.237 | 2.527 | 3.348 |
| A1_TRAVERSAL_REPEAT | 25295 | 25.013 | 1011.3 | 1065.9 | 0.938 | 1.440 | 1.660 | 6.981 |
| B1_IDLE | 25212 | 20.013 | 1259.8 | 1303.3 | 0.767 | 1.022 | 1.337 | 1.825 |
| C1_IDLE | 24726 | 20.014 | 1235.4 | 1320.3 | 0.757 | 1.288 | 1.609 | 3.952 |

### CPU / GPU / Wartezeit – BEFORE → AFTER

| Szenario | CPU aktiv ms | GPU ms | CPU Δ | Present ms | Sleep ms | Draws/Frame |
| --- | --- | --- | --- | --- | --- | --- |
| A1_IDLE | 1.388 → 1.181 | 0.345 → 0.348 | -14.9% | 0.027 → 0.027 | 0.000 → 0.000 | 977.4 → 977.4 |
| A1_CAMERA | 1.450 → 1.214 | 0.354 → 0.355 | -16.3% | 0.027 → 0.027 | 0.000 → 0.000 | 1054.8 → 1054.9 |
| A1_RUN | 1.245 → 1.093 | 0.504 → 0.504 | -12.2% | 0.029 → 0.038 | 0.000 → 0.000 | 838.0 → 845.0 |
| A1_TRAVERSAL | 0.952 → 0.882 | 0.524 → 0.525 | -7.4% | 0.056 → 0.086 | 0.000 → 0.000 | 516.5 → 515.9 |
| A1_ACTORS_20 | 1.802 → 1.521 | 0.429 → 0.380 | -15.6% | 0.029 → 0.028 | 0.000 → 0.000 | 1124.4 → 1124.4 |
| A1_COMBAT | 1.463 → 1.209 | 0.366 → 0.360 | -17.4% | 0.027 → 0.026 | 0.000 → 0.000 | 1051.8 → 1051.8 |
| A1_ACTORS_50 | 2.313 → 2.005 | 0.529 → 0.481 | -13.3% | 0.029 → 0.030 | 0.000 → 0.000 | 1244.4 → 1244.4 |
| A1_TRAVERSAL_REPEAT | 0.958 → 0.897 | 0.526 → 0.529 | -6.3% | 0.054 → 0.090 | 0.000 → 0.000 | 521.5 → 521.5 |
| B1_IDLE | 0.808 → 0.741 | 0.295 → 0.297 | -8.3% | 0.053 → 0.051 | 0.000 → 0.000 | 429.4 → 429.4 |
| C1_IDLE | 0.790 → 0.726 | 0.365 → 0.371 | -8.2% | 0.070 → 0.082 | 0.000 → 0.000 | 413.4 → 413.4 |

**PRIMARY:** CPU-bound in allen getesteten Szenarien. CPU-Arbeit liegt deutlich über dem GPU-Intervall; weder Sleep noch VSync deckeln die Messung. **SECONDARY:** Welt-/Shadow-Submission, bei 50 Actors zusätzlich Animation/Pose und Actor-Submission. **STUTTER:** erste Sichtbarkeit/Laden, optionale synchrone Diagnose-I/O sowie einzelne nicht kausal aufgelöste Scheduling-/Present-Ausreißer. Die Maxima werden vollständig ausgewiesen; kein allgemeines Versprechen aus einem einzelnen Maximalframe. GPU-Zeiten belegen Headroom, sind aber kein separat saturierter GPU-Durchsatzbenchmark.

### Verbleibende Present-Spitzen / keine pauschale Frametime-Verbesserung

Traversal und C1 verbessern CPU-Mittelwert und CPU-P95/P99, ihre vollständigen Frametime-P95/P99 fallen in diesem Lauf jedoch schlechter aus. Das wird nicht als universelle Verbesserung oder als erledigtes Frame Pacing gewertet. Die Zuordnung benutzt die Arbeit von Frame N und das Intervall bis Frame N+1, nicht fälschlich dessen nachfolgenden CPU-Scope.

| Abschnitt | CPU P95 ms | CPU P99 ms | Present P95 ms | Present P99 ms | Present im längsten AFTER-Frame |
| --- | --- | --- | --- | --- | --- |
| Traversal | 1.096 → 1.006 | 1.298 → 1.114 | 0.294 → 0.559 | 0.588 → 0.816 | 5.730 |
| Traversal Repeat | 1.098 → 1.023 | 1.285 → 1.197 | 0.281 → 0.560 | 0.565 → 0.810 | 6.142 |
| C1 Idle | 0.905 → 0.845 | 1.080 → 1.006 | 0.445 → 0.550 | 0.703 → 0.898 | 2.732 |

Beim längsten Repeat-Frame entfallen 6,142 von 6,981 ms auf Present(0), nur 0,837 ms auf aktive CPU-Arbeit. Es gibt dabei keinen Pack-Read/Texture-Create-/World-Rebuild-Burst. Der genaue Anteil von DXGI-/Desktop-/Driver-Scheduling innerhalb von Present ist nicht aufgelöst; die höhere unlimitierte Submission-Kadenz allein beweist keine Kausalität. P2 reduziert die belegte Runtime-Arbeit und den Diagnose-Burst; eine einheitliche Verbesserung sämtlicher P95/P99/Max-Werte wird ausdrücklich nicht behauptet. Produktionslimiter und Grafik-/Gameplay-Verhalten bleiben erhalten; ein weiterer Pacing-Fix wurde nicht begonnen.

## TARGET 1 – Mesh-Bindings in Static World / Shadows

BEFORE: Static World 0,269 ms und Shadows 0,250 ms gehören bei A1 Idle zu den größten CPU-Blöcken. `DrawMesh` suchte pro Draw bis zu 22 Shader-Variablen nach Namen und setzte auch unveränderte Buffer, Sampler und Texturen erneut. Der lokale Diligent-D3D11-Pfad sucht diese Namen linear; wiederholtes Set erzeugt zusätzliche Ressourcen-/Referenzarbeit. Hinzu kamen stringbasierte Unbind-Schleifen in beiden World-End-Pfaden.

FIX: Pro Mesh-SRB die vorhandenen optionalen Variablen einmal auflösen und nur bei geänderter Ressourcenidentität binden. Beide bisherigen Asset-Unbind-Grenzen bleiben erhalten und invalidieren gleichzeitig die gespeicherten Identitäten. SRB/PSO-Lebensdauer bleibt gekoppelt; kein zusätzliches Asset-Ownership. Konstanten werden weiterhin pro Draw hochgeladen, CommitShaderResources und alle Draws/Cascades bleiben bestehen. Kein neues Batching, keine Sortierung, kein geändertes Culling und keine Shaderänderung.

| Isolierter 25-s-Idle-Vergleich | BEFORE | Nur Fix 1 | Änderung |
| --- | --- | --- | --- |
| FPS avg | 705.4 | 812.2 | +15.1% |
| CPU aktiv ms | 1.388 | 1.202 | -13.4% |
| Shadows CPU ms | 0.250 | 0.128 | -48.7% |
| Static World CPU ms | 0.269 | 0.261 | -2.7% |
| P95 ms | 1.531 | 1.343 | -12.3% |
| P99 ms | 1.718 | 1.481 | -13.8% |
| GPU ms | 0.345 | 0.347 | kein GPU-Qualitätseingriff |
| Draws/Frame | 977.4 | 977.4 | unverändert |

Der Fix-1-Sanity enthält außerdem 20 s mit 50 Actors und 15 s Combat, Exit/Diagnostik PASS. Diese kürzere Reihenfolge hat eine andere vorherige Sichtbarkeit von Weltobjekten; ihre Actor-/Combat-FPS werden daher nicht als exakter Vergleich mit der vollständigen Sequenz verkauft. Dafür gilt ausschließlich BEFORE/AFTER oben.

## TARGET 2 – Resource Lifecycle

Die 30 Reads und 30 Texture Creations im zweiten Traversal sind exakt `d:/ymir work/effect/etc/dust/dust.dds`, gerendert durch `d:/ymir work/effect/etc/dust/dust.mse`. `EffectRenderBridge` besaß einen globalen weak_ptr-Cache. Nach Ende einer kurzlebigen Effektinstanz löschte deren `EffectResources` den letzten starken GPU-Handle; beim nächsten Staub-Effekt wurde das Pack erneut geöffnet, dieselbe Textur decodiert und hochgeladen. Es handelt sich nicht um Terrain-, Vegetation- oder Sektor-Neuladen.

FIX: Das bereits vorhandene `CGraphicImage::GetAssetTexture` verwenden. Es lädt die bereits decodierten nativen Bilddaten über denselben Uploader hoch und speichert das Ergebnis an der Bildressource. Der vorhandene Decoder ist identisch zum bisherigen DDS/STB-Loader; Bytes/Mips/Format bleiben erhalten. Die pro Frame gültige Binding-ID wird synchron auf das native Bild aufgelöst. Kein neuer globaler Residency-Cache, keine Änderung an Bild-/Map-Preload. Uploader-Lifetime-Token verhindert Wiederverwendung bei Renderer-Neuanlage an gleicher Adresse; Device-/Resource-Clear invalidiert den Cache.

| Beleg | BEFORE | AFTER |
| --- | --- | --- |
| Traversal Repeat Pack Reads | 30 | 0 |
| Traversal Repeat Texture Creations | 30 | 0 |
| 15-s-Combat, isoliert Fix 1 → Fix 2: Reads / Creates | 60 / 60 | 0 / 0 |
| Voller 25-s-Combat: Reads / Creates | 102 / 102 | 0 / 0 |
| Effect-Uploader Peak Texturen | 10 | 13 |
| Effect-Uploader Peak Mip-Payload Bytes | 530432 | 653312 |
| Effect-Uploader beim Shutdown: Texturen / Bytes | 0 / 0 | 0 / 0 |

Die Speicherwirkung ist explizit gemessen: Peak-Payload Δ +122,880 Bytes. Das sind die summierten nativen Mip-Bytes lebender Effekt-GPU-Texturen, keine vollständige Treiber-/VRAM-Allokationsmessung. Die CPU-Bilddaten existierten bereits vorher. Die Retention endet mit der nativen Bildressource; alle gezählten Ressourcen sind beim Shutdown frei. Der Regressionstest simuliert 30 kurzlebige Effektbesitzer, prüft identische Pixel/Alpha sowie Freigabe bei Device-Destroy und Resource-Clear; der bestehende Test deckt auch Uploader-Neuanlage an derselben Adresse ab.

## TARGET 3 – Effect Burst / optionale Diagnose-I/O

Der wiedergefundene Burst gehört zu `d:/ymir work/effect/etc/fall/fall_7.mse` und dessen erster sichtbarer `fall_2.tga`. Die feine Messung (`effect-focus`, Frame 8763) zeigt:

| Operation | CPU ms | Einordnung |
| --- | --- | --- |
| Effect-Submit insgesamt | 8.9646 | First use, CPU-Wallclock |
| Diagnosezeile schreiben / flush | 8.3156 | Hauptursache, optionale Verbose-Diagnostik |
| Texture Load gesamt | 0.5336 | enthält Pack/Decode/Upload |
| Pack lesen/dekomprimieren | 0.2470 | Teil des Texture Load |
| CreateTexture | 0.1790 | Teil des Texture Load |
| Erster Draw gesamt | 0.0954 | einschließlich PSO |
| PSO-Erstellung | 0.0851 | Teil des ersten Draw |

Die verschachtelten Zeiten dürfen nicht addiert werden. Vertex-/Konstanten-Upload, Commit und Draw sind in den feinen Rohdaten enthalten und bilden nicht den 8-ms-Block. Kein Runtime-Shadercompile in diesem Messabschnitt. Der Burst tritt bei neuer Diagnosemeldung/erster Sichtbarkeit auf, nicht bei jedem Partikel. Deshalb ist Shader-Prewarming hier nicht die passende Lösung.

FIX: Die höchstens 256 bereits deduplizierten Erfolgsmeldungen im Speicher sammeln und beim regulären Prozessende schreiben. Fehler halten den bisherigen Fail-Status und werden sofort inklusive ausstehender Meldungen geflusht. Es entsteht kein Hintergrundthread und kein permanentes Production-Detail-Logging; ohne `--renderer-diagnostics` bleibt dieser Pfad inaktiv. Dies beseitigt einen Diagnose-Stutter, keinen 8-ms-Shader-/GPU-Engpass der normalen Produktion.

| Waterfall Effect-Submit, erste Sichtbarkeit | ms |
| --- | --- |
| Neue vollständige Baseline | 8.1036 |
| Nach Fix 2, Logging noch synchron | 7.7908 |
| Nach Fix 3, vollständige Abschlusssequenz | 0.2550 |

Die optionalen Erfolgsmeldungen sind nach dem sauberen Exit weiterhin im Log vorhanden. Kein Effekt, keine Partikelzahl, Textur oder Blend-/Materialeigenschaft wurde abgeschaltet. Die allgemeinen maximalen Frametimes oben können andere Ursachen haben; dieser Fix adressiert den exakt identifizierten File-I/O-Block.

## CPU-Phasen

Mittelwerte in ms/Renderframe, BEFORE → AFTER. Update enthält Python-, World-, Actor- und Effect-Update; Animation/Pose, Skin-Preparation und Actor-Draw-Submission sind separat. Vegetation-Rest enthält nicht die ebenfalls ausgewiesene Instanz-/LOD-Arbeit.

| Szenario | Update | Static World | Shadows | Terrain | Vegetation | Instance prep | LOD | Animation | Skin prep | Actors draw | Effects | Forward/UI Rest |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A1_IDLE | 0.004 → 0.003 | 0.269 → 0.229 | 0.250 → 0.135 | 0.129 → 0.141 | 0.167 → 0.171 | 0.081 → 0.070 | 0.060 → 0.060 | 0.034 → 0.034 | 0.002 → 0.002 | 0.047 → 0.042 | 0.049 → 0.050 | 0.186 → 0.138 |
| A1_CAMERA | 0.004 → 0.003 | 0.328 → 0.271 | 0.277 → 0.146 | 0.132 → 0.145 | 0.169 → 0.173 | 0.089 → 0.077 | 0.060 → 0.062 | 0.028 → 0.028 | 0.002 → 0.002 | 0.047 → 0.043 | 0.052 → 0.051 | 0.156 → 0.116 |
| A1_RUN | 0.003 → 0.003 | 0.231 → 0.194 | 0.191 → 0.110 | 0.133 → 0.135 | 0.168 → 0.173 | 0.101 → 0.089 | 0.060 → 0.061 | 0.023 → 0.023 | 0.002 → 0.002 | 0.047 → 0.043 | 0.052 → 0.054 | 0.107 → 0.086 |
| A1_TRAVERSAL | 0.002 → 0.002 | 0.138 → 0.116 | 0.089 → 0.057 | 0.095 → 0.095 | 0.167 → 0.170 | 0.083 → 0.072 | 0.060 → 0.061 | 0.018 → 0.018 | 0.002 → 0.002 | 0.046 → 0.042 | 0.066 → 0.068 | 0.056 → 0.049 |
| A1_ACTORS_20 | 0.008 → 0.007 | 0.316 → 0.267 | 0.308 → 0.165 | 0.130 → 0.132 | 0.168 → 0.171 | 0.082 → 0.071 | 0.060 → 0.062 | 0.194 → 0.189 | 0.006 → 0.006 | 0.145 → 0.131 | 0.052 → 0.052 | 0.198 → 0.143 |
| A1_COMBAT | 0.004 → 0.004 | 0.314 → 0.266 | 0.273 → 0.144 | 0.129 → 0.130 | 0.169 → 0.170 | 0.083 → 0.069 | 0.060 → 0.062 | 0.033 → 0.033 | 0.002 → 0.002 | 0.047 → 0.041 | 0.051 → 0.051 | 0.195 → 0.143 |
| A1_ACTORS_50 | 0.015 → 0.013 | 0.316 → 0.269 | 0.362 → 0.197 | 0.131 → 0.133 | 0.170 → 0.171 | 0.082 → 0.072 | 0.060 → 0.063 | 0.425 → 0.412 | 0.011 → 0.012 | 0.312 → 0.284 | 0.053 → 0.055 | 0.204 → 0.154 |
| A1_TRAVERSAL_REPEAT | 0.002 → 0.002 | 0.146 → 0.124 | 0.089 → 0.058 | 0.094 → 0.096 | 0.167 → 0.171 | 0.083 → 0.072 | 0.059 → 0.062 | 0.018 → 0.018 | 0.002 → 0.002 | 0.046 → 0.043 | 0.066 → 0.069 | 0.056 → 0.050 |
| B1_IDLE | 0.002 → 0.001 | 0.029 → 0.026 | 0.052 → 0.035 | 0.094 → 0.093 | 0.145 → 0.144 | 0.072 → 0.055 | 0.060 → 0.061 | 0.020 → 0.019 | 0.002 → 0.002 | 0.047 → 0.042 | 0.123 → 0.122 | 0.054 → 0.045 |
| C1_IDLE | 0.002 → 0.002 | 0.024 → 0.023 | 0.088 → 0.057 | 0.093 → 0.094 | 0.137 → 0.142 | 0.097 → 0.079 | 0.061 → 0.062 | 0.020 → 0.020 | 0.002 → 0.002 | 0.046 → 0.043 | 0.065 → 0.065 | 0.049 → 0.042 |

## GPU-Phasen

Mittelwerte in ms, BEFORE → AFTER. GTAO schließt den gemeinsamen PostFX-Vorpass ein; HDR ist das abschließende Tone Mapping. Kleine Werte runden auf 0,000. GPU Skinning ist in Actors/Shadows enthalten.

| Szenario | Shadows | Terrain | Static | Actors | Vegetation | GTAO/PostFX | Water | Effects | Bloom | HDR | Sky | Lighting | Forward/UI Rest |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A1_IDLE | 0.024 → 0.024 | 0.059 → 0.059 | 0.013 → 0.014 | 0.000 → 0.000 | 0.002 → 0.002 | 0.138 → 0.138 | 0.000 → 0.000 | 0.002 → 0.002 | 0.038 → 0.039 | 0.005 → 0.005 | 0.028 → 0.029 | 0.014 → 0.015 | 0.020 → 0.021 |
| A1_CAMERA | 0.026 → 0.026 | 0.064 → 0.064 | 0.011 → 0.011 | 0.001 → 0.001 | 0.002 → 0.002 | 0.145 → 0.144 | 0.000 → 0.000 | 0.016 → 0.015 | 0.037 → 0.037 | 0.005 → 0.005 | 0.027 → 0.028 | 0.016 → 0.016 | 0.006 → 0.007 |
| A1_RUN | 0.041 → 0.041 | 0.076 → 0.077 | 0.006 → 0.007 | 0.000 → 0.000 | 0.015 → 0.015 | 0.141 → 0.142 | 0.122 → 0.120 | 0.005 → 0.005 | 0.039 → 0.039 | 0.005 → 0.005 | 0.026 → 0.027 | 0.015 → 0.015 | 0.011 → 0.011 |
| A1_TRAVERSAL | 0.037 → 0.036 | 0.090 → 0.091 | 0.003 → 0.003 | 0.000 → 0.000 | 0.004 → 0.004 | 0.139 → 0.140 | 0.155 → 0.154 | 0.009 → 0.008 | 0.038 → 0.038 | 0.005 → 0.005 | 0.026 → 0.027 | 0.015 → 0.015 | 0.003 → 0.003 |
| A1_ACTORS_20 | 0.045 → 0.043 | 0.060 → 0.059 | 0.014 → 0.014 | 0.004 → 0.004 | 0.002 → 0.002 | 0.215 → 0.172 | 0.000 → 0.000 | 0.002 → 0.002 | 0.037 → 0.035 | 0.006 → 0.005 | 0.029 → 0.029 | 0.015 → 0.015 | 0.002 → 0.001 |
| A1_COMBAT | 0.025 → 0.025 | 0.059 → 0.058 | 0.014 → 0.014 | 0.000 → 0.000 | 0.002 → 0.002 | 0.138 → 0.138 | 0.000 → 0.000 | 0.032 → 0.027 | 0.039 → 0.039 | 0.005 → 0.005 | 0.027 → 0.026 | 0.014 → 0.014 | 0.010 → 0.012 |
| A1_ACTORS_50 | 0.179 → 0.142 | 0.063 → 0.060 | 0.014 → 0.014 | 0.016 → 0.014 | 0.003 → 0.002 | 0.160 → 0.155 | 0.000 → 0.000 | 0.002 → 0.002 | 0.038 → 0.036 | 0.006 → 0.006 | 0.032 → 0.031 | 0.015 → 0.016 | 0.002 → 0.001 |
| A1_TRAVERSAL_REPEAT | 0.037 → 0.036 | 0.091 → 0.091 | 0.003 → 0.003 | 0.000 → 0.000 | 0.004 → 0.004 | 0.138 → 0.140 | 0.156 → 0.157 | 0.010 → 0.008 | 0.038 → 0.038 | 0.005 → 0.005 | 0.027 → 0.028 | 0.015 → 0.015 | 0.003 → 0.003 |
| B1_IDLE | 0.019 → 0.018 | 0.047 → 0.048 | 0.002 → 0.003 | 0.000 → 0.000 | 0.003 → 0.003 | 0.138 → 0.139 | 0.000 → 0.000 | 0.003 → 0.003 | 0.036 → 0.036 | 0.006 → 0.006 | 0.025 → 0.026 | 0.014 → 0.015 | 0.001 → 0.001 |
| C1_IDLE | 0.032 → 0.032 | 0.101 → 0.102 | 0.003 → 0.003 | 0.000 → 0.000 | 0.003 → 0.003 | 0.140 → 0.140 | 0.000 → 0.000 | 0.003 → 0.003 | 0.036 → 0.037 | 0.006 → 0.007 | 0.025 → 0.026 | 0.015 → 0.015 | 0.001 → 0.002 |

GTAO ist ein größerer Teil der kleinen GPU-Zeit, limitiert hier aber nicht die Frame-Kadenz. AO, Bloom, HDR und Wasser bleiben deshalb unverändert. Unterschiedliche mittlere Draw-Anzahlen bei bewegter Kamera entstehen auch durch zeitabhängige Sichtbarkeit/Effekte und unterschiedliche Frame-Sampling-Dichte; keine Draws wurden als Optimierung entfernt.

## Actor Scaling

| Actors | FPS avg vorher/nachher | CPU aktiv ms | Actor Update ms/Tick | Animation ms/Frame | Skin prep ms/Frame | Actor Submission ms | Actor GPU ms |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | 705.4 → 826.0 | 1.388 → 1.181 | 0.005 → 0.006 | 0.034 → 0.034 | 0.002 → 0.002 | 0.047 → 0.042 | 0.000 → 0.000 |
| 20 | 545.7 → 644.9 | 1.802 → 1.521 | 0.011 → 0.011 | 0.194 → 0.189 | 0.006 → 0.006 | 0.145 → 0.131 | 0.004 → 0.004 |
| 50 | 426.6 → 491.1 | 2.313 → 2.005 | 0.017 → 0.018 | 0.425 → 0.412 | 0.011 → 0.012 | 0.312 → 0.284 | 0.016 → 0.014 |

Bei 50 Actors ist Animation/Pose ein großer CPU-Posten; GPU-Skinning bleibt aktiv und alle CPU-Vertex-Deformation-/Fallback-Zähler bleiben null. Die gemeinsame Mesh-Optimierung hilft auch Actor-/Shadow-Submission. Ein zusätzlicher Eingriff in Animation/Gameplay wurde nach Auswahl der drei belegten Ziele nicht begonnen. Das ist ein dokumentierter verbleibender Kandidat für separat beauftragte Arbeit, kein P3-Start.

## Draws, Uploads und Ressourcen

| Szenario | Instanced draws/frame | Instances/frame | Bekannte Uploads/frame | Upload KiB/frame | Buffer creates | Texture creates | Pack reads | Shader compiles |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| A1_IDLE | 48.0 → 48.0 | 132.0 → 132.0 | 980.8 → 980.8 | 463.2 → 463.2 | 0 → 0 | 0 → 0 | 0 → 0 | 0 → 0 |
| A1_CAMERA | 55.0 → 55.0 | 150.1 → 150.3 | 1058.6 → 1058.6 | 502.3 → 502.3 | 313 → 313 | 290 → 290 | 245 → 243 | 0 → 0 |
| A1_RUN | 80.3 → 80.0 | 207.0 → 205.9 | 836.0 → 843.0 | 512.0 → 513.0 | 29 → 29 | 10 → 9 | 10 → 9 | 0 → 0 |
| A1_TRAVERSAL | 31.3 → 31.4 | 94.8 → 94.8 | 533.0 → 532.3 | 539.6 → 539.4 | 34 → 34 | 55 → 25 | 50 → 19 | 0 → 0 |
| A1_ACTORS_20 | 48.0 → 48.0 | 132.0 → 132.0 | 1146.8 → 1146.8 | 840.7 → 840.7 | 0 → 0 | 0 → 0 | 0 → 0 | 0 → 0 |
| A1_COMBAT | 48.0 → 48.0 | 132.0 → 132.0 | 1058.5 → 1058.5 | 504.2 → 504.2 | 0 → 0 | 102 → 0 | 102 → 0 | 0 → 0 |
| A1_ACTORS_50 | 48.0 → 48.0 | 132.0 → 132.0 | 1296.8 → 1296.8 | 1380.7 → 1380.7 | 0 → 0 | 0 → 0 | 0 → 0 | 0 → 0 |
| A1_TRAVERSAL_REPEAT | 31.3 → 31.3 | 94.7 → 94.7 | 538.3 → 538.3 | 542.0 → 542.0 | 0 → 0 | 30 → 0 | 30 → 0 | 0 → 0 |
| B1_IDLE | 9.0 → 9.0 | 18.0 → 18.0 | 506.8 → 506.8 | 205.4 → 205.4 | 0 → 0 | 0 → 0 | 0 → 0 | 0 → 0 |
| C1_IDLE | 21.0 → 21.0 | 42.0 → 42.0 | 422.8 → 422.8 | 192.6 → 192.6 | 0 → 0 | 0 → 0 | 0 → 0 | 0 → 0 |

## World Stability

| Messabschnitt | Terrain/Area loads | Terrain/Area unloads | Patch rebuilds | Instance creates | Instance growth | Tree changes / rapid repeats |
| --- | --- | --- | --- | --- | --- | --- |
| BEFORE A1_TRAVERSAL | 0 / 0 | 0 / 0 | 0 | 6 | 3 | 0 / 0 |
| BEFORE A1_TRAVERSAL_REPEAT | 0 / 0 | 0 / 0 | 0 | 0 | 0 | 0 / 0 |
| AFTER A1_TRAVERSAL | 0 / 0 | 0 / 0 | 0 | 6 | 3 | 0 / 0 |
| AFTER A1_TRAVERSAL_REPEAT | 0 / 0 | 0 / 0 | 0 | 0 | 0 | 0 / 0 |

Die erste Traversierung hat vor und nach P2 identisch sechs Render-Instanzbuffer-Neuanlagen und drei Kapazitätserweiterungen bei erstmaliger Sichtbarkeit. Das sind keine Sector Reloads oder Patch-Neuaufbauten. Bei identischer Wiederholung sind sowohl Neuanlagen als auch Erweiterungen null. Terrain/Areas bleiben innerhalb der A1-Strecke resident; B1/C1 sind absichtliche Mapwechsel außerhalb der Messabschnitte. Kein World-/Terrain-/Vegetation-/LOD-Algorithmus wurde geändert. Diese Aussage beruht auf Zählern und gezielten GPU-Readbacks; eine zusätzliche manuelle World-Galerie wurde nicht durchgeführt.

## Fast Gate und Produktionsstand

**FAST GATE: PASS**

| Prüfung | Status |
| --- | --- |
| Vollständiges uncapped BEFORE / AFTER, je 10 Abschnitte | PASS |
| Fix-1 / Fix-2 kurze Sanity-Läufe | PASS |
| Diligent ERROR/FATAL; GPU fallback; CPU deformation | 0 / 0; 0; 0 |
| Gezählte Shutdown-Ressourcen einschließlich Effect-Texturen | 0 |
| Regulärer Exit beider vollständiger Läufe | 0 |
| Release-Build ohne temporäre Instrumentierung | PASS |
| AssetRuntime.EmbeddedMaterial; Graphics.DiligentFXModern; Graphics.DiligentFXMaterials | PASS (3/3, finaler normaler Release) |
| Originalclient Binärdateien/Packs/Konfiguration hashgleich | PASS (110 Dateien) |
| Netzwerklogin / komplette Suite / große Galerie | NOT RUN, gemäß P2-Scope |

Die GPU-Prüfungen decken Material-/Texturwechsel auf demselben SRB, Unbind/Rebind über mehrere Frames, PBR/Legacy-Materialien, AO/Shadow-Komposition, Off-Camera-Caster, Transparenz sowie Resize/Minimize ab. Der zusätzliche Bild-Lifetime-Test deckt wiederholte kurzlebige Effektbesitzer und die Freigabegrenzen ab. Temporäre Scopes, Bufferexporte, uncapped-Taktung und Weapon-Trace-Profilinghilfe sind aus dem endgültigen Source entfernt. Der normale Release-Build behält seine vorhandene Frame-Taktung; es wurde keine FPS-Option eingebaut.

Zusätzlicher kurzer Smoke des endgültigen normalen Release: Idle 4 s + Combat 8 s nach jeweils 4 s Warmup, Exit 0 und dieselben Null-Fehler-/Shutdown-Gates. Die Profiling-API ist nachweislich nicht mehr vorhanden; der native FPS-Zähler meldet in beiden Abschnitten 61 FPS. Beleg: `build-p2/release-smoke/`. Dieser Smoke ist keine zweite uncapped Vergleichsmessung.

Source/Tests/Dokumentation sind der einzige Commit-Inhalt. Keine Logs, Builds, Testclients, Caches oder Screenshots. Runtime bleibt auf `main`; kein Push und kein Deployment. Ein durch den zwischenzeitlich vom Nutzer gestarteten Originalclient aktualisiertes Runtime-Log wurde erhalten. Nächster sinnvoller Schritt ist die Entscheidung des Nutzers über einen getrennten Production-Deployment-/Abnahmeschritt. **Nach P2 STOP.**

## Lokale Evidenz (ignoriert, nicht im Commit)

Unter `build-p2/`: `before/` und `after/` mit `summary.json`, `p1-frames.csv`, `p1-gpu.csv`, `p2-details.tsv`, `p1-run.log`, `fast-gate.json`, `p2-texture-memory.tsv`, Launch-/Exit-/Hashbelegen. `fix1/` und `fix2/` enthalten die isolierten Zwischengates; `effect-focus/effect-analysis.json` die feine Burst-Zerlegung. `checkpoints/before/`, `checkpoints/fix1/`, `checkpoints/fix2/`, `checkpoints/after/` enthalten Quell-/Instrumentierungssnapshots. `build-after.log`, `build-production.log`, `build-tests.log`, `tests.log`, `validation.json` und die Integritätsbelege dokumentieren den Abschluss. `entry.py`, `prepare.ps1`, `run.ps1`, `analyze.py` dokumentieren Messung und Auswertung. Diese privaten Artefakte sind keine Voraussetzung für den produktiven Client.
