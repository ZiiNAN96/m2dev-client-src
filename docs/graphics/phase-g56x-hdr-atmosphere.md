# G5/6-X — HDR & Atmosphere

**G5/6-X = GO.** Release/Debug 64/64, GCC/LP64 34/34, Classic jeweils 12/12 bytegleich. Manuelle Login-/Relog-/UI-Abnahme erfolgreich bestätigt. Kein Commit, kein Push; STOP nach G5/6.

## 1. Start State

2026-09-15 vor jeder Änderung geprüft: beide Arbeitsbäume sauber, einschließlich
unversionierter Dateien. Source `feature/gdx-diligentfx`, HEAD `77645d2`
(`refactor(core): finalize DiligentFX renderer and library audit`), einen Commit
vor dem Remote. Assets `main`, HEAD `72d92593`, sauber. Arbeitsbranch für diesen
Meilenstein: `codex/g56-hdr-atmosphere`. Main wird nicht verändert.

Die eingecheckten Berichte bestätigen **G-DX-C = GO** und **C-LIB-X = GO**.
62/62 Release, 62/62 Debug, 33/33 GCC/LP64 und 12/12 identische Classic-Bilder
sind Ausgangsevidenz, noch keine erneuten G56-Ergebnisse. Der bisherige
Release-Client wurde vor dem ersten Build nach `build/g56x/baseline/` kopiert.
Kein Commit, Push, Staging, Reset oder History-Rewrite.

## 2. Diligent/DiligentFX Reuse

Unveränderter FX-Pin:
[`cb380ac52100672b5762f595acfb6609e0ecc248`](https://github.com/DiligentGraphics/DiligentFX/tree/cb380ac52100672b5762f595acfb6609e0ecc248),
Archiv-SHA256 `7117a1d0067ef0c36315900647904116234cf55cadc11682f6f6e70064658236`.
Der vorhandene geprüfte lokale Quelltext wurde vor Implementierung auditiert.

| Komponente | Entscheidung | Tatsächliche Nutzung / Grenze |
|---|---|---|
| Diligent textures / PSO / SRB | USE | FP16-Scene und Lighting-Anhänge, bestehender D3D11-Backend |
| PostFXContext | REUSE | Gemeinsame Kamera, Depth, PostFX-Pipelines; dieselbe Instanz wie SSAO |
| Bloom | USE | Unveränderte Bright-pass-/Downsample-/Upsample-Algorithmen, persistente R11G11B10-Float-Pyramide |
| ToneMapping.fxh | USE | Ein fester Reinhard-Luminanzoperator; Sättigung 1, feste Exposure |
| EpipolarLightScattering | ADAPT | Physikalische Koeffizienten, optische Dichte, Single-Scattering-LUT und Lookup |
| ShaderUtilities / Common constants | REUSE | Kamera, Rekonstruktion und Farbraumhilfen |
| Gesamter Epipolar-Frontend | Nicht eingebunden | Enthält Raymarch-/Shaft-Pässe, Luminanzadaption und zusätzliche Zwischenziele |
| TAA / SSR | Weiter inaktiv | Kein neuer Temporal- oder Wasserpfad |

`PrepareDiligentFX.py` entfernt ausschließlich die Bloom-Demo-UI und deren
ungenutztes SSR-Include und ergänzt wie bei SSAO den Readiness-Getter. Ein
erfolgreicher neutraler Placeholder gilt nicht als funktionierendes Bloom.
Shader-Include-Abschluss wird weiterhin vor dem Build geprüft.

**Warum nicht die vollständige Diligent-Atmosphärenklasse?** Sie koppelt die
Atmosphäre an epipolare Bildschirmabtastung, volumetrische Lichtstrahlen,
Scattering-Correction, Luminanz und zusätzliche 3D-Mehrfachstreuungspuffer.
G56 verlangt ausdrücklich keinen solchen Frame-Raymarching-/Volumetric-Pfad.
Stattdessen werden ihre getesteten allgemeinen Tabellenalgorithmen adaptiert.
Die numerische Integration findet einmal bei der Tabellenvorbereitung statt;
der sichtbare Frame fragt vorberechnete Tabellen ab. Keine eigene Streuungsformel.

**Custom-Adapter und Begründung:** ZiiNAN liefert den Z-up-/Zentimeter-Bezug,
Map-Horizontfarbe, feste Exposure, Qualitätswahl und den World/UI-Abschluss.
Eine kleine Sky-Ansicht übersetzt diese Daten in FX-Tabellenkoordinaten.
Der Sonnenkreis benutzt dieselbe Strahlrichtung wie Lighting/Shadows und eine
Winkelgröße von 32 Bogenminuten. Der upstream Sun-Shader liefert nur einen
weißen normierten Bildschirm-Quad ohne unsere HDR-Radiance-/World-Depth-Grenze.
Der ZiiNAN-Adapter setzt daher den winkelbasierten Kreis bei Sky-Depth ein.
Fog ist eine kleine exponentielle Distance-Policy; der Atmosphärenhorizont wird
als Zielfarbe wiederverwendet. FX besitzt keinen passenden isolierten
Map-Distance-Fog-Pass. Keine neue Schatten-, AO- oder PBR-Architektur.

## 3. Render Boundary Audit

Ausgangspfad in `CPythonApplication::RenderGame`: Legacy Sky/Cloud/LensFlare,
Environment, World/Actors, G-DX-End, Water/Snow/Map Effects, Manager Effects,
Items/Flying, PCBlocker und Legacy AfterLensFlare. Nameplates folgen im
Python-GameWindow erst nach `app.RenderGame()`; danach läuft Screen-UI.

Der bisherige G-DX-Ende-Punkt lag also **vor** einem Teil der World-Effekte.
Weapon Trails wurden während der Actor-Liste gezeichnet. Schadenszahlen sind
technisch `CEffectInstance`-Partikeleffekte; sie waren mit Welteffekten vermischt.

## 4. World/UI Boundary

Modern: World/Actors → Shadow/SSAO/Atmosphere-HDR-Komposition → Weapon Trails,
bestehendes Water, Snow, Fire/Map Effects, Skills/Particles, Items/Flying,
PCBlocker → Bloom → Exposure/Tone Mapping → Damage/Miss-Zahlen → Nameplates,
HUD, Chat, Inventory, Settings, Minimap, System-UI und Cursor.

Damage/Miss werden bei ihrer Erstellung semantisch als Screen-Overlay markiert.
Classic verwendet weiter den bisherigen ungeteilten Effect-Manager-Pass.
Simulation, Texturen, Alpha-Tests und Blend-Faktoren bleiben erhalten.
Modern ersetzt den alten Sky einschließlich alter Sonnen-/LensFlare-Ausgabe.

## 5. HDR Target

Modern komponiert Direct + Indirect + Emission in ein persistentes Scene-Target.
`End()` beendet die opake Komposition; `FinishWorld()` schließt nach Water,
World Effects, Trails, Items/Flying und PCBlockern die HDR-Welt ab.

## 6. HDR Format

Scene, Direct, Indirect, Emission: **RGBA16F**. Normals behalten RGBA16F,
Depth D24S8, Shadow Depth D32F und AO seine bestehenden Formate.
Bloom verwendet das geeignete upstream **R11G11B10_FLOAT**, einschließlich
seiner HDR-Ausgabe. Optische Dichte: RG32F für die zwei integrierten Dichtewerte;
Scattering/Sky/IBL: RGBA16F. Keine RGBA32F-Scene. SDR-Swapchain bleibt RGBA8;
kein HDR10, Windows HDR oder Monitor-HDR-Schalter.

## 7. PostFXContext

Dieselbe G-DX-Instanz liefert Kamera, Depth und allgemeine PostFX-Infrastruktur
für SSAO und Bloom. Kein zweiter History-/Kamerapfad. Readiness wird geprüft;
ein neutraler Placeholder wird nicht als erfolgreicher Effekt akzeptiert.

## 8. Exposure

Feste Basis **2.0**, unabhängig von Qualität und Kamera. Keine Auto-Exposure,
Luminanzadaption oder zeitliche Helligkeitspumpe.

## 9. Environment Exposure

`SceneLighting.exposureBias` ist ein optionaler Bias in Stops: Exposure =
`2 * exp2(bias)`. Begrenzung −3 bis +3; nicht endliche Werte werden 0.
Bestehende Maps verwenden 0 und benötigen keine Dateimigration. Noch kein
zusätzlicher Environment-Dateiparser für dieses optionale Feld.

## 10. Tone Mapping

Ein Production-Operator: **Diligent `ToneMapping.fxh`, Reinhard-Luminanz**,
Luminanzsättigung 1. Danach Diligent Linear→sRGB und SDR-Ausgabe.
Keine Auswahl verschiedener Tone Mapper im Menü.

## 11. Highlight Preservation

Der GPU-Test unterscheidet Emission 2 und 8 nach der Ausgabe; Emission 8 bleibt
unter 255. FP16-Lighting-Anhänge verhindern das Abschneiden vor der Addition.
Bloom-Off/On und die unveränderten Ausgabebilder liegen in der Galerie.

## 12. Black Crush Prevention

Kalibrierung über korrektes HDR, feste Exposure und Tone Mapping. Der nahe
B1-A/B/C-Vergleich zeigt hellere Haut/Rüstungsdetails und einen lesbaren Wolf;
Tor- und Körperschatten bleiben klar. Kein Ambient-Multiplikator und keine
Aufhebung von Shadow oder SSAO.

## 13. Color Preservation

Keine Grading-, Entsättigungs-, Vignette- oder Filmstufe. Die Vergleichsbilder
behalten grüne Vegetation, braune Erde, gelbe Kleidung und rote Rüstungsteile.
Reinhard erhält die linearen Farbverhältnisse vor der SDR-Konvertierung.

## 14. Bloom

Direkte Wiederverwendung der gepinnten DiligentFX-Bloom-Klasse und ihrer Shader.
Intensität **0.06**, Radius **0.65**, nur Aus/Ein für Benutzer. Persistente
Pyramide; keine per Frame erzeugten Targets. Keine Lens-Flares oder Lens Dirt.

## 15. Bloom Threshold

Threshold **2.0**, Soft Threshold **0.05**, vor Exposure. Das kontrollierte
weiße Material mit 0.5 linearer Emission ist bei Bloom Off/On **bytegleich**;
Emission 8 erzeugt einen messbaren Halo. Die Blend-/Texture-Combiner alter
Welteffekte werden nicht künstlich mit einem Emissive-Multiplikator verstärkt.

## 16. Visible Sun

32 Bogenminuten Winkeldurchmesser, HDR-Radiance aus derselben Sun Color und
Intensity. Ein Kreis ausschließlich bei Sky-Depth, durch Geometrie verdeckbar.
Kein HUD-Sprite, keine zweite Sonne und keine alte Modern-LensFlare-Ausgabe.

## 17. Sun Consistency

Einzige Quelle: `SceneLighting.sunDirection/sunColor/sunIntensity`.
Lighting und ShadowMapManager verwenden die Strahlrichtung; sichtbarer Kreis
und Atmosphäre ihre Gegenrichtung. GPU-Projektion links/rechts stimmt auf
weniger als 3 Pixel mit der analytischen Projektion überein; Depth-Verdeckung
ist geprüft. Der bestehende G-DX-Test bestätigt passende Schattenbewegung.

## 18. Sun Elevation

GPU-Nachweise für links/rechts/hoch sowie vier Shadow-Sonnenrichtungen
links/rechts/hoch/niedrig bleiben grün. Die drei nativen Entwicklungszustände
verwenden genau dieselbe SceneLighting-Änderung für alle Verbraucher.

## 19. Atmosphere

Diligent berechnet physikalische Luftkoeffizienten, optische Dichte und
Single-Scattering-LUT. Diese Tabellen werden bei Vorbereitung erzeugt.
Der sichtbare Frame verwendet Lookups; kein Bildschirm-Raymarching,
volumetrischer Fog oder Cloud-Raymarching. Keine vollständige Wetterengine.

## 20. Sky

Modern Low/High: 128×64 / 256×128 Sky-Ansichten derselben physikalischen LUT.
Zenith, Horizon und Sonnenstreuung ändern sich gemeinsam mit der Sun.
Classic behält den alten Himmel. Alte Cloud-/Flare-Schichten werden in Modern
nicht zusätzlich über den neuen Himmel gelegt.

## 21. Existing Map Environment Integration

Vorhandene Directional-Light-, Material-Diffuse-/Ambient-/Emissive-Fill- und
Fog-Daten werden in ZiiNAN auf SceneLighting abgebildet. Fehlende Werte haben
validierte Defaults. Der kleine atmosphärische IBL-Anteil beträgt 0.15 und
ist separat begrenzt; bestehendes Map-Ambient bleibt erhalten.

## 22. Horizon

Die Sky-LUT liefert die richtungsabhängige Horizontfarbe. Der bestehende
Map-Fog-Farbton fließt mit maximal 12 Prozent am Horizont ein. Es gibt keine
unabhängige zweite Horizontfarbe im opaken Composite.

## 23. Fog

Low verwendet Distance-Linear-Fog, High eine exponentielle Distanzkurve;
vorhandene Density-Maps behalten ihre Dichte-Semantik. Near/Far/Qualität kommen
von der Map und GraphicsRuntimeConfig. Ungültige Intervalle werden deaktiviert.

## 24. Sky/Fog Integration

Opake Welt und moderner transparenter Mesh-Pass mischen zur gleichen
richtungsabhängigen Sky-/Horizontfarbe. Ferne Kontraste und Schatten werden
mit der Welt abgeschwächt. Legacy-Particle-Combiner behalten ihre vorhandene
Fog-/Alpha-Semantik und werden anschließend in lineares HDR überführt.

## 25. PBR under HDR

Diligent-PBR-Materialtests und GLB-Renderfälle prüfen Roughness, Metallic,
Normal Maps, Emissive und IBL. Materialparameter und BRDF bleiben erhalten;
HDR vergrößert den darstellbaren Lichtbereich. Keine neue PBR-Implementierung.

## 26. IBL

Der vorhandene BRDF-LUT-/PBR-Pfad wird weiterverwendet. Die neue Sky-Ansicht
speist Diligents `ComputeIrradianceMap.psh` und `PrefilterEnvMap.psh`:
16×16 Diffuse-Cube, 32×32 Specular-Cube mit sechs Mips, jeweils sechs Flächen
und 128 Samples bei Änderung. Kein sichtbarer Sun Disk im IBL, also keine
zweite direkte Sonne. Vorhandene Map-Füllbeleuchtung bleibt separat.

**Warum dieser Adapter?** Die getesteten FX-Faltungen passen; nur ihre
Sphere-Map-Abfrage wird auf den Z-up-Sky-Atlas abgebildet. Rotationen und
Filteralgorithmus stammen aus dem gepinnten PBR-Renderer. Keine Reflection
Probes oder neue Cubemap-Engine. Der GPU-Test weist den zusätzlichen
richtungsabhängigen indirekten PBR-Beitrag nach.

## 27. Shadow Regression

Unveränderter Diligent ShadowMapManager/PCF-Pfad. G-DX-Tests einschließlich
Alpha-Cutoff, Off-Camera-Caster, transparenten Empfängern, Sun-Richtungen und
Shadow-/AO-Trennung bestehen. Das große B1-Tor zeigt weiterhin klare Schatten.

## 28. SSAO Calibration

Algorithmus und Stärke unverändert. SSAO bleibt auf indirekte Beleuchtung
begrenzt und räumlich; ungültige Motion History wird nicht benutzt.
Nach Exposure/Tone Mapping war keine zusätzliche Stärkeänderung erforderlich.

## 29. Legacy GR2 Proof

Originale lokale GR2-Geometrie und Diffuse-Assets laufen über den nativen
Reader, neutrale Animation und GPU-Skinning in Diligent PBR/HDR.
Der A1→B1→A1-Lauf und die manuelle Spielsitzung verwenden diese Production-Route.
Kein Granny-Import oder Runtime-Fallback.

## 30. Player Proof

Galerie: B1-Nahaufnahme, identischer Standort und Kamera in Classic, altem G-DX
und G5/6. G5/6 verbessert Haut-/Rüstungsdetails; die Körper- und Torschatten
bleiben. Echte Idle-Animationen laufen weiter, deshalb kein Pixelvergleich
unterschiedlicher Animationszeitpunkte.

## 31. Mob Proof

Derselbe Wolf steht in allen drei Nahaufnahmen neben Player/NPC/Boss auf
trockenem B1-Boden. Gesicht, Rücken und Beine bleiben sichtbar. Die vorherigen
Fluss-/Unterwasser-Testkameras sind nicht als Mob-Abnahme verwendet.

## 32. Building Proof

Großes vorhandenes B1-Steintor, ermittelt aus den lokalen Map-/Property-Daten.
A/B/C zeigt Reliefs, Seitenflächen, Boden und Schatten bei gleicher Kamera.
Zusätzlich dokumentiert die entfernte B1-Ansicht Gebäude/Vegetation/Fog.

## 33. Terrain Proof

A1 und B1 mit Grass/Dirt/Path, gleicher Szene und Kamera. Farbzeichnung bleibt
sichtbar, Schatten sind erhalten. Die Performance-Serie enthält zusätzlich
identische A1→B1→A1-Nah-/Fernkameras.

## 34. Vegetation Proof

Vorhandener ZiiNAN-Pagoda-Baum auf B1, aus dem Property-/Mapbestand ausgewählt.
A/B/C zeigt Stamm, Äste, Blätter und Bodenschatten. Der H-X-GPU-Gate prüft
zusätzlich die bestehenden Typen, Alpha, Fog, Wind, LOD und Ressourcen.
Keine Leaf Transmission, neuen Bäume, Grass- oder Wind-Systeme.

## 35. Animated GLB Proof

Native F5-Charaktersequenz: **20 Actors, Idle/Walk/Attack**, Material-/Attachment-
und Neustufenfälle, PBR/HDR, Shadows/SSAO, Bloom und Atmosphäre.
GPUFrames=17640, CPU-Deformation=0, GPUFallbacks=0, Diligent ERROR/FATAL=0;
alle erfassten Skeleton-/Clip-/GPU-Owners am Shutdown 0.
Galerie enthält native Aufnahmen und die kontrollierten 20-Actor-A/B/C-Bilder.

## 36. Emissive/Bloom Proof

GPU-Off/On-Paar mit Emission 8; normale weiße Kontrollfläche bleibt bytegleich.
Zusätzlich Emission 2 vs. 8, UI-BYPASS und eine Sky-IBL-Kontrolle.
Die PNG-Dateien sind ausschließlich verlustfreie Konvertierungen der GPU-Rohbilder.

## 37. Effects

Audit: Water, Fire, Skills, Particles, Weapon Trails, World Snow sowie Items/
Flying/PCBlocker liegen vor `FinishWorld()`. Traces behalten ihre bisherige
Kamera-Sortierung. Damage/Miss-Effekte werden beim Erzeugen als Screen markiert.

Zusätzlicher begrenzter nativer 20-Sekunden-Lauf: Feuer, Nahkampf, Flächeneffekt,
Snow, Drain; fünf Captures, kein Stress-/Dense-Test. Exitcode 0, kein Game-/Python-
Fehler, Diligent ERROR/FATAL=0, CPU-Verformung/Fallback=0. Skills/Schadenszahlen
wurden außerdem in der manuellen Sitzung ohne Auffälligkeiten bestätigt.

## 38. UI

HUD, Chat, Nameplates, Damage/Miss-Zahlen, Inventory, Settings, Minimap und Cursor
folgen dem SDR-Weltabschluss. Der GPU-Test erhält den UI-Pixel **RGB 208/96/32**
exakt trotz unterschiedlicher Exposure/Bloom-Einstellung. Derselbe Farbwert
als Welteffekt reagiert auf Exposure. Benutzerabnahme: keine Auffälligkeiten.

## 39. GraphicsSettings

Bestehende Felder Bloom, Himmelqualität und Nebelqualität aktiviert; kein
UI-Redesign und keine Asset-Repository-Änderung. Modern erzwingt internes HDR,
Classic löst HDR/Bloom/Sky/Fog-Features auf inaktiv auf.

## 40. Presets

| Preset | HDR/Tone Map | Bloom | Sky | Fog |
|---|---|---|---|---|
| Modern Low | an | aus | Low | Low |
| Modern Medium | an | aus | High | High |
| Modern High | an | an | High | High |
| Modern Ultra | an | an | High | High |

Keine zusätzlichen künstlichen Atmosphärenstufen. Andere bestehende Preset-
Qualitäten behalten ihre reale G-DX-Bedeutung.

## 41. Live Apply

Nativer Settings-Lauf mit 14 Schritten plus zwei Neustartschritten, insgesamt
**16 Captures**, erfolgreich. Bloom Off/On, Sky/Fog Low/High, Classic/Modern,
UI, Persistenz, Map-Wechsel und Fensterübergänge sind abgedeckt.
Der erste Versuch scheiterte an einer Testabfrage eines nicht exportierten
Diagnosefeldes, wurde korrigiert und vollständig neu in einem frischen Root ausgeführt.

## 42. Morning Test

Strahlrichtung vor Normalisierung (−0.94, 0.12, −0.32). Nativer finaler Capture:
Sonne nahe dem Horizont, Streuung und Fernwirkung aus denselben Sun-Daten.
Die Probe existiert nur mit `M2_BUILD_RENDERER_TESTS`.

## 43. Noon Test

Strahlrichtung (−0.15, 0.12, −0.98): hohe Sonne und zur Höhe passende Streuung.
Native Aufnahme plus kontrollierter GPU-Projektions-/Shadow-Test.

## 44. Evening Test

Strahlrichtung (0.94, 0.12, −0.32): entgegengesetzter niedriger Sonnenstand.
Dieselben Map-Farben/Intensitäten, keine separate Abendbeleuchtung oder zweite Sonne.

## 45. Day/Night Boundary

Nur drei feste Entwicklungseinstellungen. Kein Tageszyklus, Serverzeit,
Nacht, Mond, Sterne, Wetter, volumetrische Wolken oder Cloud Shadows.
SSR/TAA bleiben inaktiv; keine G7-/G8-/H2-Erweiterung.

## 46. Performance

[Messdaten und Methodik](../../build/g56x/performance.md),
[CSV-Auswertung mit p95](../../build/g56x/performance.json).
Gleiche A1→B1→A1-Szene, neun Map-/Kameraabschnitte, 1024×768, VSync,
identische Schatten-/AO-Stufe. Erste 60 Frames je Abschnitt verworfen.
Während dieser zwei Läufe keine parallelen Builds oder anderen Testclients.

| Größe | G-DX | G5/6 |
|---|---:|---:|
| CPU-Mediane über die neun Abschnitte | 0.886–1.088 ms | 0.872–1.048 ms |
| GPU-Frame-Mediane | 0.290–0.332 ms | 0.357–0.393 ms |
| Max. privater Prozessspeicher | 621.2 MB | 652.2 MB |
| Gezählt: eigene Scene/Shadow/BRDF/Atmo-Targets | 54,525,952 B | 79,572,976 B |

GPU-Zuwachs paarweise etwa **0.06–0.08 ms**; keine allgemeine FPS-Garantie.
HDR-Scene allein 6,291,456 B (6 MiB); Atmosphäre + beide Sky-Stufen + IBL
9,318,384 B. Zusätzlich upstream Bloom: **5,242,872 B**, aus konkreten
Target-Abmessungen berechnet. Internes PostFX/AO und Treiber-Overhead sind
nicht vollständig in `ownedTargetBytes` enthalten.

CPU-Einreichung im G56-Lauf, Summe / 3490 Frames: Atmosphere 111.371 ms
(einschließlich Initialisierung/Änderungen), Bloom 59.1805 ms, Tone Mapping
9.1091 ms, Composite 59.9376 ms. Das sind **keine GPU-Effektzeiten**.
Fog liegt im Composite/Forward-Pass und hat keine isolierte Messung.
Keine Performance-Finish-Phase, kein Stresslauf.

## 47. First Use

Die bestehenden Loading-Prewarm-Aufrufe bereiten HDR, Atmosphären-LUTs/IBL,
Bloom und Tone Mapping vor dem ersten World Present vor. Manuelle Sitzung:
Prewarm 5440.99 / 1513.86 ms; erste sichtbare World-CPU-Abgabe 8.9842 / 3.0222 ms.
Diese Werte entstanden während weiterer Abnahmearbeiten, nicht als isolierter
Vergleichsbenchmark.

Ein bewusster Classic→Modern-Wechsel erstellt den freigegebenen Modern-Owner
neu. Dafür gibt es Shaderaufbau im Umschaltframe; bestehende G-DX-Material-
Varianten können danach bei ihrem ersten Auftreten entstehen. Das wird nicht
als "0 Shader-Kompilierungen nach Start" ausgegeben. Initiales Gameplay und
Relog verwenden den Loading-Prewarm. Runtime-PSO-Maximum der Sitzung 0.4543 ms.

## 48. Resource Lifetime

HDR/Lighting/Depth, Bloom, ToneMap, Atmosphäre/Sky/IBL und PostFX sind persistente
Owner-Ressourcen. Sun/Fog verwenden gemeinsame Konstanten und haben keinen
eigenen zusätzlichen Frame-Target. Sky/IBL werden nur bei Sun-/Qualitätsänderung
neu gefüllt. Alle Cube-RTVs werden einmal angelegt.

Native Map-, Settings-, GLB-, Effects- und echte Login/Relog-Läufe enden mit
ModernRenderers=0 und freigegebenen Source-, Asset-, Animation-, GR2-, GLB- und
Vegetation-Owners. [Manuelle Auditdaten](../../build/g56x/manual-final.json)
prüfen die benannten Zero-Felder. Das ist Owner-/Referenz-Evidenz, keine
Behauptung einer vollständigen Treiber-Heap-Messung.

## 49. Resize

Window-Targets und größenabhängige PostFX/Bloom-Ressourcen werden freigegeben
und passend neu angelegt; zugehörige SRBs werden zurückgesetzt.
GPU-Test 512×384→320×240 sowie nativer Fenster-Test erfolgreich.
Größenunabhängige Atmosphäre/IBL bleibt bestehen.

## 50. Minimize/Restore

GPU-Test suspendiert bei 0×0, rendert dort keinen Frame und stellt korrekt
wieder her. Nativer Window-Test bestätigt Minimize/Restore/Resize/OriginalSize.
Kein Black Screen, Crash oder zurückbehaltener Owner in der Abnahme.

## 51. Map Change

A1→B1→A1 im finalen HDR-Client: **3490 Frames**, Settings auf Bloom/Sky/Fog High,
Exitcode 0. SceneLighting/Fog werden pro Map gelesen; Sun-/Sky-/IBL-Cache prüft
aktuelle Werte. Kein stale Environment oder PostFX-Binding beobachtet.

## 52. Login/Character Select

Echte Sitzung: Start→Login→Select→Loading→Game, Relog, erneut Select/Loading/
Game, ShutdownClean. Zwei World-Presents und zwei GameWindow-Open/Close-Paare.
Benutzer bestätigt: **"Abnahme erfolgreich, keine Auffälligkeiten"**.
Exitcode 0 und alle geprüften Zero-Felder erfüllt.

Die Sitzung verwendete den vollständigen Production-Pfad. Der danach ergänzte
CMake-Compile-Schalter betrifft ausschließlich die Entwicklungssonnenprobe;
bei −1 bleibt die normale Map-Beleuchtung gleich. Finaler Client und Probe
wurden anschließend separat gebaut und in den nativen Sonnenansichten geprüft.

## 53. Release

Vollständiger Release-Build **PASS**. Begrenztes Gate **64/64**: bisherige 62 plus
`Graphics.HDRAtmosphereConfig` und `Graphics.HDRAtmosphereGpu`.
[Finales Testlog](../../build/g56x/release-tests-final.log).
Keine Vendor-Fuzzer/Benchmarks. Bekannte LNK4099-PDB-/LTCG-Hinweise bleiben;
der Build wird nicht als warnungsfrei bezeichnet.

## 54. Debug

Vollständiger Debug-Build **PASS**, dasselbe begrenzte Gate **64/64**.
[Finales Testlog](../../build/g56x/debug-tests-final.log).
Diligent-Development-Validierung aktiv; der neue GPU-Test zählt ERROR/FATAL
explizit. Bekannte LNK4098-/LNK4099-Linkerwarnungen sind nicht verschwiegen.

## 55. GCC/LP64

Portabler Cygwin-GCC-Build und **34/34** Tests PASS: bisherige 33 plus die neue
Exposure-/Sun-/Fog-/IBL-Validierung. Kein Diligent-GPU-Code im rendererlosen
Harness. [Testlog](../../build/g56x/gcc-tests.log).
Der erste eingeschränkte Build konnte vorhandene Cygwin-Objektdateien nicht
schreiben; mit normalem Zugriff wurde er erfolgreich abgeschlossen.

## 56. Diligent Diagnostics

Neuer GPU-Test und alle finalen nativen G56-Läufe: **0 ERROR, 0 FATAL**.
Der native Client protokolliert die Diligent-Factory-Severities und beendet bei
ERROR/FATAL mit Fehlercode. Keine schweigende Fehlerunterdrückung.
Der bekannte einmalige Game-Log-Eintrag `invalid idx 0` in der echten Sitzung
ist kein Diligent-Bindungsfehler und wird im Audit separat erhalten.

## 57. GR2 Regression

Native Goldens, Repair-/Safety-/Compatibility-/Warmup-/Independence-Prüfungen,
Render-/Hair-LOD-Test und Map-Lauf grün. CPU Deformation=0, GPUFallbacks=0,
GrannyFileReads=0. Keine neue Character-/Animation-Engine.

## 58. GLB Regression

Provider-/Contract-/Material-/Static-/Animated-Renderfälle grün, einschließlich
20 Actors, Attachments und Resize. Nativer F5-HDR-Lauf ergänzt den GPU-Gate.
RuntimeSkeleton/RuntimeAnimationClip bleiben der gemeinsame Runtime-Vertrag.

## 59. Vegetation Regression

H-X-Contracts, Boundary, Goldens, NoLegacyDependency und RenderGoldens grün.
A/B/C verwendet die vorhandenen kompilierten ZiiNAN-Vegetationsassets.
Keine SpeedTree-Runtime oder automatische Legacy-Route.

## 60. Classic Goldens

**12/12 bytegleich in Release und Debug**, SHA256 gegen die erhaltene G-DX-
Classic-Referenz. [Release](../../build/g56x/classic-Release.json),
[Debug](../../build/g56x/classic-Debug.json). Classic benutzt keinen neuen
HDR-/ToneMap-/Bloom-/Atmosphäre-Renderpfad.

## 61. Visual Gallery

[Interaktive A/B/C-Galerie](../../build/g56x/visual-proof.html),
[Manifest mit Pfaden und SHA256](../../build/g56x/visual-manifest.json).
Player/Wolf, großes Tor, Terrain, Vegetation, animated GLB, drei Sonnenstände,
Emissive Off/On, PBR, UI und native Feuer-/Skill-/Snow-Aufnahmen.

A/B/C-Weltbilder wurden mit erhaltenem Baseline-Client bzw. G56 und denselben
Map-/Kameraangaben aufgenommen. Das kontrollierte GLB-B-Bild stammt aus der
vorhandenen G-DX-Galerie; es ist ausdrücklich als erhaltene Referenz markiert.
Kein Screenshot-Retouching. Rohbilder und Testlogs bleiben erhalten.

## 62. Zero Legacy Audit

**D3D9=0, Granny=0, SpeedTree=0** im Source-/Link-Gate und in Release-/Debug-
DLL-Imports. Native Route: ZiiNAN GR2, AnimationRuntime, Vegetation und GPU-
Skinning. [Importaudit](../../build/g56x/zero-legacy.json).
Vorhandenes DDRAW für historische Plattformfunktionen wird nicht als D3D9
umgedeutet. Keine Behauptung einer durchgeführten OS-Modulenumeration.

## 63. Git Diff

Source-Branch `codex/g56-hdr-atmosphere`; ursprünglicher HEAD unverändert.
[Review-Patch einschließlich neuer Dateien](../../build/g56x/g56-review.patch),
[Abschlussstatus](../../build/g56x/git-final.txt).
Assets-Repository/main bleibt sauber. Kein Stage, Commit, Push oder Rewrite.
Builds, Logs, Binaries und Bilder liegen in ignorierten Build-Verzeichnissen.

## 64. Known Limitations

- SDR-Ausgabe; Windows D3D11-GPU-Abnahme, portable Config unter GCC/LP64.
- Single Scattering, keine Mehrfachstreuung, volumetrischen Medien oder Wetter.
- Legacy-Particle-Fog-/Blend-Semantik bleibt erhalten; keine generelle Neugestaltung alter Effekte.
- Map-Bias ist als SceneLighting-Eingang vorbereitet, ohne neues Map-Dateiformat.
- Expliziter Classic→Modern-Wechsel baut freigegebene Ressourcen neu auf.
- Keine isolierten GPU-Passzeiten und keine vollständige Treiber-Speicherbilanz.
- Bekannte Linkerwarnungen und der getrennt dokumentierte Game-Log-Baseline-Eintrag bleiben.

## 65. GO/NO-GO

**G5/6-X = GO.** Alle angeforderten Kernfunktionen, finalen Gates,
A/B/C-Nachweise, native Lifecycle-/Effekt-/GLB-Prüfungen und Benutzerabnahme
sind erfüllt. Finaler Release-Gate 64/64 in 58.45 s, Debug 64/64 in 137.00 s;
Classic jeweils 12/12 bytegleich. Die Grenzen in Abschnitt 64 bleiben ausdrücklich bestehen.

## 66. Recommendation G7-X

G56 stellt Sun, Sky, HDR Scene Color und Atmosphären-Inputs als Grundlage bereit.
**G7 nicht begonnen.** Ebenso kein G8, H2, UI-Redesign, Texture Impact Pass oder
Performance Finish. Nach diesem Bericht, Tests, Galerie und Diff: **STOP**.
