# G-DX-C – Closure Pass

Stand: 2026-09-15. Branch `feature/gdx-diligentfx`, Ausgangs-HEAD
`ca63fd11959971699844421bae78083f82d8c018`. Ergänzt den
[G-DX-Bericht](phase-gdx-diligentfx-rebuild.md). Kein Commit, Push oder Folge-Meilenstein.

**GO für G-DX-C im geprüften Umfang.** Die vier Closure-Punkte sind abgenommen.
Die verbleibenden Kosten eines bewussten Classic/Modern-Wechsels sind unten
separat ausgewiesen; eine allgemeine Performance-Abnahme wird nicht behauptet.

## 1. Shadow Caster außerhalb der Kamera

Der Client sammelt Schattenwerfer in einem eigenen Durchlauf. Die Sichtprüfung
verwendet die Vereinigung der von DiligentFX erzeugten Kaskaden-Frusta und
Diligent Core `GetBoxVisibility`. Die einzelnen Shadow-Draws werden durch die
jeweilige Kaskadenprojektion begrenzt. Kamerasichtbarkeit und explizites
Verstecken eines Objekts bleiben getrennt. Der Sammeldurchlauf ändert keine
Kamera-Sichtbarkeitsflags und zeichnet keine Farbe oder Effekte.

Die Auswahl umfasst geladene Actors/Mounts, Map-Objekte, Vegetation und Terrain.
Außerhalb der Kamera benötigte Actors erhalten eine aktuelle Pose auf dem
bestehenden GPU-Pfad. Bereits für die Kamera vorbereitete Actors werden dabei
nicht erneut deformiert. Alpha-Masken, Materialien und Skinning verwenden
dieselben Renderer wie der Farbdurchlauf. Alpha-Blending bleibt wie in G-DX
vom Shadow-Caster-Pass ausgeschlossen.

Der kleine FX-Adapter ergänzt `DistributeCascadeInfo.LightSpaceDepthPadding`.
Er erweitert die Receiver-Grenzen zur Lichtquelle hin, begrenzt durch
Kamera-Farplane und Shadow-Reichweite. FX berechnet weiter sämtliche Kaskaden,
Matrizen, Stabilisierung und Filterränder. Ein zusätzlicher benannter Float4-Wert
in `CascadeAttribs` skaliert den Receiver-Bias mit dem Verhältnis der ursprünglichen
zur erweiterten Tiefe. Damit bleibt der bisherige Bias `.0005` pro Kaskade in
Welteinheiten erhalten. Raster-Bias und PCF bleiben erhalten. Die Zwischenlösung
mit einem pauschal verkleinerten Bias wurde nach sichtbaren Terrain-Artefakten
verworfen; sie zählt nicht als visueller PASS.

GPU-Nachweis: Dreieck bei `x=1.3..1.7, z=-1`, vollständig außerhalb des
Kamera-Ausschnitts `x=-1..1` in dieser Tiefe. Es wird ausschließlich als Caster
eingereicht und beschattet den sichtbaren Receiver bei `z=-2`.
Die Bilddifferenz beträgt **30.060 RGB-Helligkeitsstufen**. Ein weit entferntes
Objekt wird durch die Licht-Frusta abgewiesen. Vier Sonnenrichtungen bestehen
ebenfalls; die Schattenzentren wandern passend zur Sonne.

Belege: `build/gdxc/release-terrain-prewarm-test-details.log`,
`build/gdxc/debug-terrain-prewarm-test-details.log`,
`build/gdxc/gdxc-off-camera-reference.png`,
`build/gdxc/gdxc-off-camera-shadow.png`.

## 2. Zehn Debug-Effekt-Bindungsfehler

Ursache waren acht ungebundene `SecondaryTexture`- und zwei ungebundene
`EffectTexture`-SRVs. Laufzeit-Verzweigungen im Shader entfernten die ungenutzten
Ressourcen nicht aus der Shader-Reflection.

Vier kompilierte Varianten bilden die tatsächliche Nutzung der beiden Texturen
ab. Der PSO-Schlüssel enthält beide Variantenbits; Ressourcenlayout, Shader und
SRB stimmen dadurch überein. Nur aktive Texturen und Sampler werden gebunden.
Es werden keine Ersatz-SRVs eingesetzt, um Validierungsfehler zu verdecken.

`Renderer.ProductionGpu` zählt Diligent ERROR/FATAL-Meldungen über den offiziellen
Engine-Callback und verlangt am Testende exakt null. Die vorhandenen Bild- und
Materialprüfungen bleiben bestehen. Der ursprüngliche Debug-Test besteht mit
**0 ERROR/FATAL**. Auch die vollständigen Release-/Debug-Gate-Logs enthalten
exakt null gezählte Diligent ERROR/FATAL-Diagnosen.

## 3. Erstnutzung und Loading-Prewarm

`gdxc-first-use.log` misst Shader-Erstellung, PSO-Erstellung, BRDF/IBL,
erstes PostFX/SSAO, den gesamten Prewarm und den ersten sichtbaren World-Draw.
Die Einträge enthalten monotone Zeitstempel und die Phase `loading` oder
`runtime`. Verschachtelte FX-Gesamtzeiten dürfen nicht zu ihren Shader-/PSO-
Teilzeiten addiert werden. Die Diagnostik ist an Renderer-Diagnostics gebunden.

Der echte Netzwerkpfad fordert nach der Actor-Vorbereitung einen World-Prewarm
an. `SetGamePhase` plant zunächst eine verzögerte Curtain-Überblendung. Daher
beginnt die Vorbereitung erst beim ersten World-Render nach `GameWindow::Open`,
mit dessen tatsächlicher Kamera und Szene. Ein verborgener vollständiger
World-Durchlauf bereitet die real verwendeten Mesh-, Terrain-, Shadow- und
FX-Pipelines vor. Zusätzlich kompiliert er alle 18 bekannten Mesh-Shader-
Varianten und vier Terrain-Shader-Varianten (je Vertex-/Pixel-Shader), damit
Kamera-/Materialwechsel keine weiteren Kompilierungen dieser Shader auslösen.
Terrain-PSOs mit anderer Topologie oder Blend-Einstellung teilen dieselben
Shader. Ein GPU-Test prüft alle 22 Varianten und unveränderte Varianten-Zähler
in anschließenden Welt-Frames.
Erst danach wird der sichtbare Durchlauf eingereicht.
Fehler bei der Vorbereitung führen zum fehlgeschlagenen Shutdown.

Der kalte native GR2-Lauf ohne Netzwerk-Prewarm misst **2.289,47 ms** für den
ersten Gesamtframe, davon **1.296,86 ms** World-CPU. BRDF/IBL benötigt **89,55 ms**,
das erste PostFX/SSAO **174,29 ms**. Später erstmals verwendete Mesh-Pixel-Shader
benötigen etwa **70 ms**, neue PSOs liegen in den beobachteten Ereignissen unter
1 ms. Deshalb werden die Shader vorgezogen; keine allgemeine
Performance-Optimierungsphase.

Der erste echte Login-Lauf misst **2.801,35 ms Loading-Prewarm** und **7,01 ms**
für den ersten sichtbaren World-Draw. Es werden danach keine Mesh-Shader mehr
erstellt. Eine später sichtbare Terrain-Variante kostet noch **21,11 ms**;
deshalb ergänzt der finale Stand den Cache und Prewarm aller vier Terrain-
Varianten. Das ist die letzte gezielte Änderung dieses Closure Pass.

### Messung des finalen Clients

| Ereignis | Loading-Prewarm | Erster sichtbarer World-Draw, CPU |
|---|---:|---:|
| Login / erster Welteintritt | 2.826,55 ms | 6,15 ms |
| Logout / Relog / zweiter Welteintritt | 1.871,39 ms | 3,89 ms |

BRDF/IBL beim ersten Loading: **88,92 ms**; erstes PostFX/SSAO: **169,25 ms**.
Beide liegen innerhalb der obigen Loading-Gesamtzeit. Die gemessene Spitze
späterer PSO-Erstellung beträgt **0,227 ms**. Außerhalb der unten beschriebenen
Stilwechsel gibt es nach dem ersten Present keine weiteren Shader-Erstellungen.
Dies sind lokale CPU-Messungen, keine GPU-Framezeiten oder garantierten Budgets.
Während dieses manuellen Laufs liefen keine Builds oder GPU-Testprogramme.

Der Benutzer hat zusätzlich mehrfach **Classic ↔ Modern** umgeschaltet und
diese Bedienung bestätigt. Classic gibt den Modern-Renderer wie bisher frei;
bei Rückkehr wird er neu aufgebaut. Die drei beobachteten Initialisierungs-
Abschnitte dauern **955,61 / 961,74 / 963,71 ms**, vom Beginn des BRDF-Shaders
bis zum Ende des Composition-Shaders. Die 60 Runtime-Shader-Ereignisse des
Logs liegen sämtlich in diesen drei Abschnitten. Diese Kosten bleiben beim
bewussten Stilwechsel bestehen und werden nicht als beseitigt dargestellt.
Der Benutzer meldet für den Durchlauf keine wahrgenommenen Hänger oder
Darstellungsfehler. Es wurde keine allgemeine Optimierung der Stilwechsel
begonnen. Die nach solchen Wechseln noch fehlenden Varianten werden beim
Relog wiederum im Loading vervollständigt.

Beleg: `build/gdxc/manual-final.json` und unveränderte Rohlogs in
`build/g0x/runtime/gdxc-manual-02/`.

## 4. Echter Client-Lifecycle

Separate Clients: `build/g0x/runtime/gdxc-manual-01` und der finale
`build/g0x/runtime/gdxc-manual-02`. Normale Anmeldung,
Charakterauswahl, Netzwerkverbindung und Spieloberfläche; der Benutzer übernimmt
die Anmeldung. Lokaler Auth- und Spielport sind erreichbar.

`gdxc-lifecycle.log` erfasst die echten Netzwerkphasen, Prewarm, erste
Welt-Präsentation und Shutdown. Die private Runtime ergänzt passive Open/Close-
Protokolle und einen Screenshot nach 90 Game-Frames je Eintritt. Sie ersetzt
keine Login-, Spiel- oder Protokollfunktion. Der Produktions-Runtime-Ordner
wird nicht verändert.

**PASS:** Start → Login → Character Select → Ingame → Logout → Login →
Character Select → Ingame → regulärer Shutdown, in einem Prozess (PID 22056).
Der finale Lauf endet nach **137,4 s mit Exitcode 0**. Die beiden GameWindow-
Instanzen zeichnen **4.313 und 1.252 Frames**; beide Open/Close-Paare und beide
`WorldPresented`-Ereignisse sind protokolliert. Je Eintritt liegt ein echter
Ingame-Screenshot vor. Der Benutzer bestätigt ausdrücklich:
„Relog und Beenden erfolgreich, keine Auffälligkeiten“.

Der erste manuelle Lauf hatte nur einen Welteintritt. Er wird deshalb nur als
Login-/Ingame-/Shutdown-Nachweis geführt, nicht rückwirkend als Relog-PASS.

Am Ende: sämtliche überwachten Source-/Asset-/Animation-/Vegetation-/Collision-
Owner **0**, `ModernRenderers=0`, CPU-Deformation **0**, GPU-Fallback **0**,
Granny-Reads **0**, Skin-/Animation-/Prewarm-Fehler **0**. Der finale Lauf enthält
899 native GR2-Reads und 784 Prewarm-Anforderungen. `ownedTargetBytes` ist eine
Statistik der zuvor angelegten Renderziele, kein Live-Leak-Zähler.

`renderer-failure.log` enthält nur den Aktivierungshinweis. `syserr.txt` enthält
weiterhin bekannte `MarkManager.cpp: invalid idx 0`-Meldungen und vorhandene
DamageEffect-Diagnostik. Es wird kein leeres Fehlerlog behauptet.

Finale Release-Binärdatei, auch im manuellen Lauf verwendet:
`7f87f90d7c149da63fb9f307a69afb27ab4ec6246fb68a6f2afcf5971e991ceb` (SHA256).

## 5. Abnahme

| Prüfung | Ergebnis |
|---|---|
| Release-Build und begrenztes Gate | PASS, 62/62, 103,97 s |
| Classic-Bilder nach Release | PASS, 12/12 byte-identisch |
| Debug-Gate / Classic-Vergleich | PASS, 62/62, 164,80 s; 12/12 byte-identisch |
| GCC/LP64-Build und portables Gate | PASS, 33/33, 2,25 s |
| Off-camera Shadow GPU-Test | PASS |
| Nativer GR2-Lauf vor finaler Bias-Korrektur | Funktion/Lifetime PASS, 3 Kartenphasen, 3.501 Frames, 63,3 s; Bildbefund korrigiert |
| Nativer GR2-Lauf mit finaler Shadow-/Bias-Korrektur | PASS, 3 Kartenphasen, 3.424 Frames, 6 Bilder, 63,4 s, Exit 0; Terrain-Bilder geprüft |
| Nativer GLB-Lauf mit finaler Shadow-/Bias-Korrektur | PASS, 20 Actors, 9 Stufen/Bilder, 889 Frames, 17,92 s, Exit 0 |
| GPU-Nachprüfung nach Bias-/Mesh-Prewarm-Ergänzung | Release 4/4, Debug 4/4 |
| GPU-Nachprüfung nach Terrain-Shader-Cache | Release 1/1, Debug 1/1 |
| Echter Login und Shutdown, erster manueller Lauf | PASS, 3.252 Game-Frames, 102,9 s, Exit 0; Benutzer bestätigt keine Auffälligkeiten |
| Gezielter Ingame-Logout/Relog im finalen Client | PASS, 2 Welteintritte, 5.565 Game-Frames, 137,4 s, Exit 0; manuell bestätigt |
| Finale Release-/Debug-Client-Builds nach Terrain-Cache | PASS |
| Shutdown-Owner / CPU-Deformation / GPU-Fallback / Granny | jeweils 0 |
| Binärimport-Audit | D3D11 vorhanden; D3D9, Granny, SpeedTree nicht importiert |

Die Builds enthalten bekannte Linker-/PDB-Warnungen; sie sind nicht warnungsfrei.
Keine Langzeit-, Stress- oder Fuzz-Prüfungen. Automatische Fixture-Läufe ersetzen
den echten Login-/Relog-Nachweis nicht.

Die vollständigen Gates liefen vor den abschließenden Shader-Cache-Ergänzungen;
danach wurden die jeweils betroffenen GPU-Tests erneut ausgeführt. Der finale
Terrain-Cache wurde zusätzlich im zweiten echten Lifecycle-Lauf geprüft. Der
zweite native GR2-Lauf diente Bild- und Lifetime-Prüfungen und lief zeitweise
neben einem Build; seine Zeiten gelten nicht als ungestörte Performance-Messung.

## 6. Abschlussstatus

**GO für G-DX-C:** Off-camera-Caster, die zehn Effekt-Bindungsfehler,
Erstnutzung beim Welteintritt und der echte Login-/Relog-/Shutdown-Lifecycle
sind abgeschlossen. Der ursprüngliche G-DX-NO-GO ist für diese vier Punkte
aufgehoben. Die beschriebenen Stilwechsel-Kosten und sonstigen in G-DX bereits
abgegrenzten Funktions-/Plattformgrenzen bleiben dokumentiert.

Branch `feature/gdx-diligentfx`, HEAD
`ca63fd11959971699844421bae78083f82d8c018`, main
`ea1142c5224311c9ed7cfe604bd3471d775dea54` unverändert; Index leer.
Kein Commit, Push oder Folge-Meilenstein. Der vorhandene `DDRAW.dll`-Import
ist Baseline und wird nicht als D3D9 interpretiert.

### Prüfbare Artefakte

- [Visual Proof](../../build/gdxc/visual-proof.html)
- [Strukturierte Abnahme](../../build/gdxc/validation-summary.json)
- [Gesamter G-DX/G-DX-C-Diff einschließlich neuer Dateien](../../build/gdxc/source.diff)
- [Nur im Closure Pass hinzugekommene/geänderte Dateien mit SHA256](../../build/gdxc/closure-files.json)
- [Abschließender Git-Zustand](../../build/gdxc/git-final.txt)

Weitere Rohbelege unter `build/gdxc/`: `release-final-tests.log`,
`debug-final-tests.log`, `portable-tests.log`, `classic-Release.json`,
`classic-Debug.json`, `release-closure-gpu-tests.log`,
`debug-closure-gpu-tests.log`, `release-terrain-prewarm-test.log`,
`debug-terrain-prewarm-test.log`, `native-gr2-final-02.log`,
`native-glb-final.log`, `manual-final.json`, `release-final-dependents.txt`.
