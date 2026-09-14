# F1-X – gezielte Prüfung der Bildstillstände

Stand: 14.09.2026. **Abgeschlossen: F1-X GO mit ausdrücklich akzeptiertem
First-Use-/Warm-up-Limit.** Der manuelle Lauf ordnet die großen Bildstillstände
dem synchronen Erstimport zu. Die eigene warme Poseauswertung ist in dieser
Messung nicht der Engpass. Zwölf kleinere importfreie Intervalle über 20 ms
bleiben sichtbar; der Bericht behauptet weder durchgehend kurze Frames noch
einen vollständig behobenen Laufzeitfehler.

Der Benutzer bestätigt die geforderte Wildhundsequenz, die beobachteten
Stillstände und das Beenden über Fenster-X. Er akzeptiert die belegte
First-Use-Grenze für F1-X und verlangt den Abschluss ohne weitere Cache-,
Preload- oder Persistenzoptimierung und ohne neue Profilierung. Daraus folgt
ein technisches GO für die spätere F2-X-Arbeit am eigenen GR2-Reader;
**F2-X wird jetzt nicht begonnen. Nach F1-X STOPP.**

## Auftrag, Durchführung und Belege

Geprüft wurden Bildintervalle über 20 ms beim wiederholten Laufen und Kämpfen
im selben Wildhundgebiet: laufen, wiederholt angreifen, Wildhunde töten, neue
Wildhunde erscheinen lassen und dieselben Aktionen erneut ausführen.
Die Benutzerbestätigung bezieht sich auf diese Sequenz. Die gesamte native
Aufnahme dauerte ungefähr 309 s einschließlich ungefähr 119 s vor dem
Welteintritt für Login/Laden sowie weiterer Wartezeit. Die angeforderten
60–90 s aktiver Aktionen sind daher keine behauptete Gesamtlaufzeit und
wurden nicht als getrenntes, exakt markiertes Zeitfenster aufgezeichnet.

Der Lauf `f1x-stall-01`, PID **67984**, verwendete
`--renderer-diagnostics --animation-runtime=ziinan --animation-stall-audit`
und den bestehenden produktiven GPU-Skinning-Pfad. SHA256 des gestarteten
Clients laut [artifact.txt](../../build/f1x/runtime/f1x-stall-01/artifact.txt):

`DDDA774842FD10523391DBD6F236417FF26592E7D63A0D3C484C518EB86375DC`

Die Auswertung verwendet ausschließlich die bereits vorhandene Aufnahme:
[Frame-CSV](../../build/f1x/runtime/f1x-stall-01/animation-stalls.csv),
[Inhaltsereignisse](../../build/f1x/runtime/f1x-stall-01/animation-stall-content.csv),
[Summary](../../build/f1x/runtime/f1x-stall-01/animation-stall-summary.txt) und
[stall-analysis.json](../../build/f1x/runtime/f1x-stall-01/stall-analysis.json).
Die Schlüsselzählung und ausgewählten Framezeilen wurden direkt gegen beide
CSVs geprüft. A/B bezeichnet hier die konkurrierenden Ursachen Erstimport
und warmer Runtime-Hotpath innerhalb desselben Laufs, keinen zusätzlich
behaupteten Granny-Vergleichslauf.

Paritätstoleranzen, Rendering- und Skinningarchitektur bleiben unverändert.
Die nachfolgende Dokumentation der Messmethode erklärt die Reichweite der
Ergebnisse; sie beauftragt keine weitere Aufnahme oder Optimierung.

## Eingebaute Messung und Framegrenzen

Die Erfassung ist standardmäßig aus. Der CLI-Schalter steht in
[StartupOptions.h](../../src/Renderer/StartupOptions.h); die Aktivierung und
spätere Ausgabe erfolgen in [UserInterface.cpp](../../src/UserInterface/UserInterface.cpp).
[AnimationStallAudit.h](../../src/AssetRuntime/AnimationStallAudit.h) enthält
`Enable`, `BeginProcess`, `Presented`, `EndProcess`, `WorkScope`, `External`,
`CacheLookup`, `ImportKey` und `Write`. Die Uhr ist `std::chrono::steady_clock`;
alle ausgegebenen Zeiten sind CPU-seitig beobachtete Millisekunden.

[AnimationStallFrame.h](../../src/Renderer/AnimationStallFrame.h) umfasst jeden
Aufruf von [CPythonApplication::Process](../../src/UserInterface/PythonApplication.cpp)
mit einem Scope. Er übernimmt außerdem die Differenzen der vorhandenen
CPU-Deformation-, GPU-Fallback- und GPU-Vorbereitungszähler. Frühe Rückgaben
laufen ebenfalls durch den Scope-Abbau.

| Zeilenart | Exakte Bedeutung |
|---|---|
| `process` | Ein vollständiger `Process()`-Aufruf einschließlich seines tatsächlichen Frame-Limit-Sleeps; Speicherung nur bei `frame_ms > 20`. |
| `display` | Intervall zwischen zwei erfassten Presentation-Abschlüssen, die jeweils einen echten Swapchain-Present enthalten. Enthält alle dazwischen ausgeführten Process-Schritte, deren Sleep und die Zeit außerhalb von Process. Speicherung ebenfalls nur bei `frame_ms > 20`. |

[DiligentD3D11Backend::Present](../../src/Renderer/DiligentD3D11Backend.cpp)
erhöht `swapchainPresents` ausschließlich nach dem ausgeführten
`swapChain->Present(1)`. Der Frame-Scope übernimmt diesen Zähler am Process-Anfang
und schließt ein Displayintervall nur bei einer Änderung ab. Ein erfolgreiches
[TerrainPresentation::Present](../../src/Renderer/TerrainPresentation.cpp)
ohne sichtbaren Inhalt erzeugt damit keine falsche Displaygrenze.

Der Zeitstempel der Grenze liegt nach Rückkehr der ganzen Presentation-Funktion,
nicht am Hardware-Scanout. Die erste Displayzeile reicht von `Enable()` bis zum
ersten erfassten Present und enthält gegebenenfalls Start-/Ladezeit; sie ist
separat zu behandeln. Ein abschließendes Intervall ohne weiteren Present wird
nicht als Displayzeile abgeschlossen. Lange letzte Process-Aufrufe und die
Gesamt-/Inhaltszähler bleiben dennoch erfasst.

Verspätete Updates können im unveränderten Client Renderframes überspringen;
minimierte Fenster rendern ebenfalls nicht. Der Audit führt diese
Process-Schritte weiter und aggregiert sie bis zur nächsten echten Displaygrenze.
`updates` zählt auf Displayzeilen die begonnenen Process-Aufrufe des Intervalls,
nicht ausschließlich Spielwelt-Updates. `presented=0` bedeutet auf Processzeilen,
dass kein Displayabschluss stattfand; es unterscheidet nicht selbständig zwischen
Frame-Skip, Minimize und fehlender Präsentation aus anderem Grund.

`outside_process_ms` misst auf Displayzeilen die Lücken zwischen Process-Ende
und dem nächsten Process-Anfang. Darin können Windows-Ereignisverarbeitung,
Fensterbewegung/Resize, andere Arbeit und Betriebssystempausen liegen. Das ist
keine Einzelmessung dieser Ursachen. Process- und Displayzeilen überschneiden
sich; sie dürfen nicht miteinander aufsummiert werden.

## Spielphase und Filterung

`game` wird am Process-Anfang aus
[CPythonNetworkStream::IsGamePhaseForDiagnostics](../../src/UserInterface/PythonNetworkStream.h)
gelesen: exakt `m_strPhase == "Game"`. `IsGameOnline()` wäre ungeeignet, weil
es bereits während des Handshakes gesetzt wird. `minimized` und `inactive`
stammen aus den bestehenden Fensterzuständen des Clients.

Auf Displayzeilen sind die Flags über die beteiligten Process-Anfänge mit ODER
verknüpft. Für die Gameplay-Auswertung gilt daher `game=1`, `minimized=0`,
`inactive=0`; abweichende Zeilen werden separat ausgewiesen. Mischintervalle
beim Phasenwechsel werden anhand des Ablaufs nicht als warmer Dauerbetrieb
gewertet. Diese Stichprobenflags bilden keine lückenlose Windows-Ereignisspur.
Ein Offline-Smoke kann die Welt darstellen, ohne Netzwerkphase `Game` zu haben.

## Zeitfelder und ihre Grenzen

| Feld | Messpunkt und enthaltene Arbeit |
|---|---|
| `fingerprint_ms` | Gesamte private `Fingerprint`-Funktion des Granny-Providers einschließlich CNG-Kontext und SHA256 der vorhandenen Bytes; kein vollständiger Dateilade-/SDK-Decoding-Timer. |
| `import_ms`, `max_import_ms` | `ImportAnimation` nach Prüfung des Source-Bindings und erfolglosem Content-Lookup bis zur Rückkehr: Prüfung/Baking der vier Varianten, Keyaufbau, Retention und bestehende Importdiagnosen. Maximum eines Importversuchs im Intervall separat. |
| `pose_ms`, `max_pose_ms` | `EvaluateIndependent` einschließlich Kontrollen/Gewichte, Sampling, Blend, Hierarchie und Palette; Summe aller Aufrufe beziehungsweise Maximum eines einzelnen Aufrufs. Kein Import in diesem Pfad. |
| `palette_ms` | Ausschließlich `AnimationRuntime::BuildPalette`, als Untermenge von `pose_ms`. Weder Renderer-Palette-Capture noch GPU-Upload. |
| `update_ms` | Netzwerk/Input/Kamera/Ressourcen bis einschließlich `OnUIUpdate` in `Process()`. Python-Spielwelt-Update wird darüber aufgerufen. |
| `submission_ms` | Renderzweig vor Culling/BeginFrame bis nach UI-/Mouse-Render und `m_pyGraphic.End()`. Enthält `RenderGame`, dessen Deform-/Posearbeit und die Renderbefehle. |
| `presentation_ms` | Gesamter Aufruf von `TerrainPresentation::Present`: unter anderem Screenshot, EndFrame, periodische Rendererdiagnosen und tatsächlicher Present. |
| `present_wait_ms` | Unmittelbarer Aufruf `swapChain->Present(1)`, einschließlich dessen CPU-Wartezeit/VSync; Untermenge von `presentation_ms`. |
| `sleep_ms` | Tatsächlicher Frame-Limit-Sleep. Bewusstes Warten separat von Arbeit ausweisen. |
| `gpu_prepare_ms` | Differenz des bestehenden `prototypePrepareUs`: gesamte vorhandene GPU-Vorbereitung einschließlich Ressourcenanlage bei Bedarf und Palette-Upload. Keine gemessene GPU-Ausführungszeit. |
| `process_ms` | Gesamtdauer des Process-Aufrufs beziehungsweise dessen aufsummierte Anteile innerhalb eines Displayintervalls. |

Quellen der Import-/Posehaken:
[GrannyAssetProvider.cpp](../../src/AssetRuntime/Granny/GrannyAssetProvider.cpp)
und [GrannyAnimationAdapter.cpp](../../src/AssetRuntime/Granny/GrannyAnimationAdapter.cpp).
Die bestehenden GPU-Vorbereitung und CPU-/Fallbackzähler stammen aus
[ModelInstanceUpdate.cpp](../../src/EterGrnLib/ModelInstanceUpdate.cpp),
[Model.cpp](../../src/EterGrnLib/Model.cpp) und
[SkinningBenchmark.h](../../src/Renderer/SkinningBenchmark.h).

Verschachtelte Zeiten sind **nicht additiv**: insbesondere Palette in Pose,
Pose/GPU-Vorbereitung in Submission und Present-Wartezeit in Presentation.
Die unabhängige Runtime wird zeitlich gemessen; der Granny-Referenzpfad zählt
in diesem Audit lediglich `reference_poses`. Sein `pose_ms=0` bedeutet keine
kostenlose oder gemessene Reference-Poseauswertung. Restzeit darf nicht ohne
weitere Evidenz der eigenen Runtime zugeschrieben werden.

`MaxGamePoseFrameMs` im Summary ist die höchste aufsummierte unabhängige
Posezeit eines aktiven, nicht minimierten Gameplay-Displayintervalls.
`MaxGameSinglePoseMs` ist der höchste einzelne Poseaufruf solcher Intervalle.
Beide werden auch für Intervalle unterhalb der CSV-Schwelle fortgeschrieben.
Eine exakte Maximalzeit nur warmer, importfreier Intervalle unter 20 ms lässt
sich aus den gefilterten CSV-Zeilen dagegen nicht nachträglich rekonstruieren.

## Inhaltsidentität und Wiederholungsimporte

`animation-stall-content.csv` speichert die vollständigen Schlüssel, keine
gekürzten Digestpräfixe:

`(model_sha256, animation_sha256, model_index, animation_index, binding)`

Die beiden SHA256-Werte beziehen sich auf die ursprünglichen Modell- und
Animationsbytes. Pfad/Asset-ID allein sind keine Inhaltsidentität. Dieselben
Bytes mit einem anderen Modellindex oder Binding bilden einen anderen
vollständigen Importschlüssel. Digestwerte werden aus vorhandenen Metadaten
kopiert; der Ereignishaken hasht Dateien nicht nochmals.

| `kind` | Bedeutung |
|---|---|
| `H` | Treffer im starken Content-Cache oder im schwachen Live-Index. |
| `M` | Content-Lookup ohne Treffer, beide Digests vorhanden. |
| `U` | Lookup ohne vollständige Digests; kein belastbar vergleichbarer Vollschlüssel. |
| `I` | Tatsächlich begonnener Importversuch nach Cache-Miss; ein Cache-Miss allein zählt nicht als Import. |

Ein instanzinterner Treffer wird separat als `instance_hits` gezählt und erzeugt
keinen zusätzlichen Content-Import. `I` steht am Importbeginn, nicht erst nach
erfolgreichem Abschluss; fehlgeschlagene Versuche müssen deshalb gegen
Runtimefehler und erfolgreiche Importzähler abgeglichen werden. `process=0`
bezeichnet ein Ereignis außerhalb eines aktiven Process-Scopes. `elapsed_ms`
sowie Process-/Displaynummer erlauben die Zuordnung zur Frameaufzeichnung.

Die verlangte Anzahl unnötiger Wiederholungsimporte wird zunächst mechanisch
pro Lauf ermittelt: nur vollständige Schlüssel mit `kind=I` zählen; für jeden
Schlüssel mit N Importversuchen ist die Wiederholungsanzahl `max(N-1, 0)`.
Diese Werte werden summiert und die betroffenen Schlüssel mit Anzahl genannt.
Fehlerwiederholungen und begründete echte Verdrängungen werden anschließend
getrennt erklärt. Treffer, unterschiedliche Inhalte unter gleichem Pfad und
ein erster Import in einem neuen Prozess sind keine Wiederholung desselben
Imports innerhalb des ausgewerteten Laufs. Bei `U`-Ereignissen bleibt die
Identitätsaussage für diese Ereignisse ausdrücklich offen.

## Dateien, Erfassungsgrenzen und Auswertung

Nach Rückkehr aus `Main` schreibt `Write()` im Arbeitsverzeichnis:

- `animation-stalls.csv`: Process- und Displayzeilen strikt über 20 ms; IDs,
  Kontextflags, Import-/Pose-/Cachezähler und die beschriebenen Zeitfelder.
- `animation-stall-content.csv`: alle aufgenommenen Content-Ereignisse mit
  vollständigem Schlüssel und Zuordnung, unabhängig von der 20-ms-Schwelle.
- `animation-stall-summary.txt`: Gesamtzahlen, Zeitmaxima und Dropzähler.

`Enable()` reserviert Platz für 20.000 Framezeilen und 100.000 Inhaltsereignisse.
Die neuen Messhaken schreiben während der Aufnahme keine Dateien. Bestehende
Import-/Rendererdiagnosen bleiben unverändert und können weiterhin I/O im
Frame verursachen; das ist Teil der gemessenen betreffenden Arbeit. Die
Aufzeichnung selbst benötigt Uhrabfragen und Speicherzugriffe. Ihre begrenzte
Speicherung ist kein Nachweis völlig kostenfreier Instrumentierung.

Für vollständige Aussagen müssen `DroppedFrames=0` und
`DroppedContentEvents=0` gelten. Andernfalls sind insbesondere Maxima aus der
CSV und Wiederholungszählungen nur eingeschränkt verwendbar. Die Aufnahme
endet durch normalen Client-Shutdown, damit die Dateien geschrieben werden;
ein Prozessabbruch vor `Write()` kann diese neuen Belege verlieren.

Für jeden aktiven Gameplay-Stall werden Displaydauer, Zahl der Process-Schritte,
Imports und Fingerprints, gesamte/maximale Posezeit, Update, Submission,
Presentation/Present-Wartezeit, Sleep und äußere Lücke nebeneinandergestellt.
Zeitliche Importkorrelation allein beweist noch keine vollständige Ursache:
die gemessene Importdauer muss einen entsprechenden Anteil der Pause erklären.
Warme, importfreie Problemframes werden ausdrücklich ebenfalls betrachtet.
CPU-Deformation und GPU-Fallback werden sowohl in den Intervallzeilen als auch
im abschließenden vollständigen Ressourcen-/Skinningaudit kontrolliert.

## Die fünf angeforderten Antworten

| Nr. | Ergebnis | Beleg und Einordnung |
|---|---|---|
| 1 | **Stall korreliert mit Import: JA, für die großen Pausen.** | Von 34 aktiven Gameplay-Displayintervallen über 20 ms enthalten 22 Imports. Alle 22 Intervalle über 40 ms enthalten Imports; die zwölf importfreien Intervalle reichen nur bis 39,8406 ms. |
| 2 | **Wiederholungsimporte identischer Inhalte: 0.** | 62 Importereignisse für 62 vollständige Schlüssel, 1.942 Content-Treffer, 0 unkeyed Imports/Misses, 0 Evictions und 0 verlorene Datensätze. |
| 3 | **Maximale einzelne Importzeit: 773,1129 ms.** | Aufgezeichnet im Welteintrittsintervall Display 6996; das gerundete Summary meldet 773,113 ms. |
| 4 | **Maximale Posezeit pro aktivem Gameplay-Displayintervall: 6,6438 ms.** | Welteintritt mit 1.039 Poseauswertungen über drei Process-Aufrufe. Maximum unter den aufgezeichneten importfreien Gameplay-Stalls: 1,4389 ms. Höchster einzelner aktiver Gameplay-Poseaufruf über alle Intervalle: 0,2204 ms. |
| 5 | **F1-X GO mit akzeptiertem First-Use-/Warm-up-Limit.** | Die großen Pausen entsprechen A; B ist in dieser Aufnahme nicht als Engpass belegt. Technisches GO für die spätere F2-X-Arbeit auf Basis der abgeschlossenen Architektur-/Korrektheitsnachweise; keine F2-X-Implementierung, danach STOPP. |

Die 62 `I`-Ereignisse entsprechen den 62 erfolgreichen Imports im abschließenden
Ressourcenaudit; Runtimefehler sind 0. Kein Schlüssel tritt doppelt auf.
`DroppedFrames=0` und `DroppedContentEvents=0` bestätigen die vollständige
begrenzte Aufnahme. Insgesamt erfasste sie 17.910 Process-Aufrufe und 17.548
Presentation-Abschlüsse; 49 Displayintervalle liegen über 20 ms, davon 34
mit dem aktiven Gameplayfilter. Die insgesamt 91 CSV-Zeilen enthalten zusätzlich
Processzeilen und dürfen nicht als 91 unabhängige Bildschirmstalls gezählt
werden. Die gesamte gemessene Importzeit beträgt 15.350,7 ms.

## Konkrete Zuordnung der Pausen

Alle Werte der folgenden Zeilen gehören jeweils zu demselben Displayintervall;
sie sind keine zusammengetragenen Maxima verschiedener Frames. Zeiten in ms.

| Display / Situation | Gesamtes Intervall | Imports / Importzeit | Posezeit / Aufrufe | Update | Submission | Presentation | Sleep |
|---|---:|---:|---:|---:|---:|---:|---:|
| 6996, Welteintritt | 5.353,9249 | 26 / 4.890,9578 | 6,6438 / 1.039 | 5.283,0459 | 55,3888 | 0,0560 | 13,6003 |
| 12687, späterer Erstimport | 722,8249 | 1 / 704,3462 | 0,5313 / 42 | 705,8832 | 2,6026 | 0,0571 | 14,0186 |
| 11503, längster importfreier Ausreißer | 39,8406 | 0 / 0 | 0,6214 / 73 | 19,1076 | 6,1070 | 0,0414 | 14,0414 |
| 7107, höchste Posezeit der importfreien Stallzeilen | 20,3299 | 0 / 0 | 1,4389 / 150 | 0,4765 | 10,9175 | 0,0440 | 8,7814 |

Beim Welteintritt erklären Imports rund 91,4 % des Displayintervalls, beim
späteren Beispiel rund 97,4 %. Der erste Fall aggregiert drei Process-Aufrufe;
sein Posemaximum ist keine typische einzelne warme Frameauswertung. Die
6,6438 ms enthalten bereits die 2,2872 ms BuildPalette-Mathematik. Im späteren
722,8249-ms-Beispiel entfallen nur 0,5313 ms auf die eigene Poseauswertung.
Die Zuordnung der großen Pausen beruht somit auf gemessenen Zeitanteilen,
nicht allein auf dem gleichzeitigen Auftreten eines Imports.

Die zwölf importfreien Ausreißer werden nicht wegdefiniert. Der längste
enthält 19,1076 ms Update, 14,0414 ms Frame-Limit-Sleep, 6,1070 ms Submission
und 0,5407 ms außerhalb von Process; die darin enthaltene Posezeit beträgt
0,6214 ms. Das belegt keinen Pose-Hotpath als Ursache dieses Ausreißers.
Der höchste Posewert der zwölf aufgezeichneten importfreien Stallzeilen liegt
bei 1,4389 ms. Nicht gespeicherte importfreie Intervalle unter 20 ms lassen
keine separate nachträgliche Maximalwertbestimmung dieser Untergruppe zu.

Das Summary-Maximum eines einzelnen aktiven Gameplay-Poseaufrufs, 0,2204 ms,
bezieht auch Displayintervalle unter 20 ms ein. Es darf nicht durch das
kleinere Maximum der gefilterten CSV-Stallzeilen ersetzt werden. Das globale
Maximum über alle Phasen beträgt 0,6638 ms und ist ebenfalls getrennt zu halten.
Keiner dieser Werte rechtfertigt die Behauptung, sämtliche Frames seien unter
20 ms oder die beobachteten Stillstände seien technisch beseitigt.

## Korrektheit, Shutdown und Abschlussentscheidung

Der Benutzer beendete die Aufnahme über Fenster-X; das erfasste Prozesshandle
meldet [Exitcode 0](../../build/f1x/runtime/f1x-stall-01/exit.txt).
Der [abschließende Ressourcenaudit](../../build/f1x/runtime/f1x-stall-01/source-resource-audit.log)
bestätigt 446.455 unabhängige Posen, 0 Reference-Poseaufrufe,
0 AnimationRuntimeFailures, 0 CPU-Deformationen, 0 GPU-Fallbacks und
0 SkinPreparationFailures. Alle Source-, Instanz-, Skeleton-, Clip-,
Paletten- und GPU-Owner sowie RetainedImportKeyBytes sind nach Shutdown 0.

Die vorhandenen fokussierten Prüfungen bestehen: Release 6/6 in 29,22 s,
einschließlich Originalasset-Parität und echtem Actorpfad; abschließende
Format-/Header-/Auditprüfungen 4/4 in 0,27 s; GCC 11/11 in 0,39 s.
Belege: [Release](../../build/f1x/stall-release-tests.log),
[Formatprüfung](../../build/f1x/stall-final-format-tests.log),
[GCC](../../build/f1x/stall-gcc-tests.log). Der bereits gestartete
[Debug-Build](../../build/f1x/stall-debug-final-build.log) ist abgeschlossen,
Exitcode 0; die fünf kurzen abschließenden
[Diagnoseprüfungen](../../build/f1x/stall-debug-tests.log) bestehen in 0,79 s.
Die vollständige übergeordnete
Architektur-/Paritätsabnahme steht im
[F1-X-Hauptbericht](phase-f1x-ziinan-animation-runtime.md).

**A ist für die großen Pausen belegt.** Der erste Einsatz einer neuen
Modell-/Clip-Kombination benötigt in F1-X noch den synchronen Übergang
Granny -> neutraler RuntimeClip. In dieser Aufnahme wurde jeder vollständige
Inhaltsschlüssel genau einmal importiert; weitere Actors und wiederholte
Aktionen konnten vorhandene Inhalte wiederverwenden. Das ist ein Nachweis
für diesen Lauf, keine Zusicherung für jede mögliche Szene oder jeden
Speicherdruck. Kalte Inhalte können weiter sichtbare Pausen verursachen.

Der Benutzer hat dieses First-Use-/Warm-up-Limit ausdrücklich akzeptiert.
Damit lautet die abschließende Empfehlung **F1-X GO innerhalb dieser Grenze**.
Sie bestätigt Foundation, Architektur und Korrektheit; sie behauptet keine
ruckelfreie Erstbenutzung und beseitigt die kleineren importfreien Ausreißer
nicht. Die Messung trägt kein NO-GO wegen eines überlasteten warmen
Runtime-Hotpaths. Granny bleibt Produktionsstandard; die eigene Runtime
bleibt der explizite Opt-in-Pfad.

Für F2-X besteht ein **technisches GO zur späteren Arbeit am eigenen GR2-Reader**.
Dieser soll die Granny-basierte Konvertierungsstufe ersetzen und neutrale
Daten direkt liefern; dadurch entfallen nicht zwangsläufig sämtliche
Dateilade- oder Vorbereitungskosten. Diese spätere Richtung wird hier nur
festgehalten. Es erfolgen keine weitere Profilierung, keine zusätzliche
Cache-/Preload-/Persistenzarchitektur und keine F2-X-Implementierung.
**F1-X abgeschlossen; STOPP.**
