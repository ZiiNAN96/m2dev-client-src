# G5/6-X — HDR & Atmosphere

**Designänderung im laufenden G5/6-X:** Classic-Farbcharakter + Modern-Lichttiefe.
Legacy ohne Override bleibt diffus; echtes PBR benötigt Materialdaten/Override.
Atmosphäre bleibt Sky/Horizon/Sonne, die Modern-Welt bleibt ohne Nebelschleier.
Nebelqualität entfernt. **G5/6-X = GO. Der Benutzer meldete zuvor fehlenden
Rüstungs-/Waffenschimmer und anschließend eine zu schwache blaue Rüstungs-Aura in Modern. Beide Korrekturen sind technisch geprüft; der Benutzer hat auch die vollständige Schlussabnahme einschließlich Relog und Beenden bestätigt.** Kein Neustart.

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

Fortsetzung mit verbindlicher Designänderung am 15.09.2026: Der vorhandene
G56-Stand war inzwischen als `ec3d41c` eingecheckt; Source-Arbeitsbaum sauber.
Die Änderung setzt auf diesem Stand im selben Source-Branch auf. Assets wurden
vom sauberen `72d92593` auf Arbeitsbranch `codex/g56-hdr-colors` gewechselt, um
die angeforderte Menübereinigung ohne Änderung an main vorzunehmen.
Diese Fortsetzung erzeugt keinen Commit/Push und startet keinen neuen Meilenstein.


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
Die angeforderte originalnahe Legacy-Diffuse-Beleuchtung ist Materialpolitik:
Texturfarbe × diffuse Sonnenbeleuchtung plus vorhandenes Map-Ambient. Der
pi-Faktor hebt nur die bestehende Umrechnung von Map-Licht zu Irradiance auf.
Kein erfundener BRDF, Metallic, BRDF-Specular oder Sky-IBL für Legacy. Diligent-GGX,
Normal Mapping und IBL bleiben der expliziten PBR-Route vorbehalten.
Der Modern-Distance-Fog wurde gemäß Designänderung vollständig entfernt.

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

HDR, konstante Exposure und der luminanzbasierte Tone Mapper bleiben erhalten.
Die neue diffuse Legacy-Route erhält dunkle Texturdetails ohne grauen Specular-
oder Fog-Zuschlag. Kein Ambient-Multiplikator; Shadow und SSAO bleiben aktiv.
Der erweiterte GPU-Test verlangt lesbare Grün-/Braun-/Rüstungs-Mitteltöne.
Die finale subjektive Farb-/Lesbarkeitsabnahme wurde vom Benutzer bestätigt (§52).

## 13. Color Preservation

Verbindliches Ziel: **Classic-Farbcharakter + Modern-Lichttiefe**.
Legacy-Diffuse wird linearisiert, mit diffusem Sonnenlicht und vorhandenem
Map-Ambient multipliziert, dann über Diligent Reinhard-Luminanz/sRGB ausgegeben.
Keine erfundene weiße BRDF-Specular-Schicht, automatische Metallic-Schätzung,
Sky-Reflexion, globale Fog-Mischung oder Entsättigung für Legacy.

Der GPU-Test prüft Grün, Braun und Blau einer Rüstung: Abweichung der normierten
linearen RGB-Anteile jeweils <0.02 trotz Licht/Tone Mapping. Extreme ungewollte
Metallic-/Roughness-/Environment-Werte bleiben für Legacy bytewirkungslos.
GLB-BaseColor wird im Modern-Pfad genau einmal angewandt; die Classic-Kopie
im TextureFactor wird dort nicht nochmals als zusätzlicher Spiel-Tint benutzt.
Echte Hit-/Selection-Tints bleiben erhalten. Keine Retusche von Vergleichsbildern.

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

Vorhandene Directional-Light-, Material-Diffuse-/Ambient-/Emissive-Fill-Daten
werden weiter in SceneLighting abgebildet. Map-Fog-Farbe ist ausschließlich
ein Input für den Sky-Horizont; Fog-Dichte/Near/Far werden der Modern-Welt nicht
zugeführt. Alte Map-Dateien funktionieren ohne Migration.
Sky-IBL-Stärke 0.15 gilt ausschließlich für explizite PBR-Materialien.

## 22. Horizon

Die unveränderte Diligent-Scattering-LUT liefert die Sky-Farbe. Die vorhandene
Map-Fog-Farbe darf nur am Sky-Horizont bis zu 12 Prozent beitragen.
Pixel mit World-Depth erhalten keine Atmosphären-Farbmischung.

## 23. Fog

**Modern: kein sichtbarer allgemeiner Weltnebel.** Entfernt sind die
Distance-/Density-Fog-Mischungen aus opakem Composite und transparentem Mesh-
Pass. HDR-World-Effekte ignorieren alte Fog-Zustände. MapManager aktiviert
Environment-Fog nur noch in Classic.

Auch die alten HTP-/STP-Terrainwege behalten in Modern Texturen bis zur
vorhandenen Sicht-/Splatgrenze: kein Austausch gegen einfarbige Fog-Patches,
keine STP-DiffuseAlpha-Nebelüberblendung. Bestehende LOD-/Sichtweitenregeln
bleiben. Kein neues Far-Fading zum Kaschieren der Sichtweite.
Classic behält seinen bisherigen Fog und dessen Einstellung.

## 24. Sky/Fog Integration

Atmosphäre → Sky/Horizon/Sonne. Welt → Materialfarbe, Directional Lighting,
Shadows, SSAO, HDR/Tone Mapping. Der GPU-Test erzwingt extremen aktiven
Environment-Fog und prüft bytegleiche World-Ausgabe für opake, transparente
und Partikel-Pässe. Alte Alpha-/Blend-Regeln bleiben erhalten.

## 25. PBR under HDR

Die vorhandene neutrale `MaterialModel`-Kennung steuert den Shader:

| Material | Modern-Darstellung |
|---|---|
| Legacy/GR2 ohne Override | Original-Diffuse, diffuse Sonne und Map-Ambient; kein GGX/Metallic/Sky-IBL; originaler Item-Schimmer bleibt erhalten |
| GLB ohne authored `pbrMetallicRoughness` | Diffuse-Route; Containerformat allein erzeugt kein Metallic |
| GLB mit PBR-Block | Voller Diligent-PBR-Pfad mit den authored/default Werten dieses Blocks |
| Expliziter Legacy-/GR2-MaterialOverride | Voller Diligent-PBR-Pfad |

Der Override-Vertrag bleibt atomar und an die exakte Asset-/Materialidentität
gebunden. Normal-/Roughness-/Metallic-/AO-Materialkarten werden nur in PBR
ausgewertet; echtes Emissive bleibt authored. GPU-Tests bestätigen unterschiedliche
Metall-/Dielektrikum-Ausgabe sowie die einmalige BaseColor-Anwendung über den
echten Material-Bridge-Helfer. Laufzeit-Zähler erfassen beide Materialrouten.


**Nachbesserung aus der Benutzerabnahme: originaler Item-Schimmer.** Der zuvor
fehlende `ActorMaterialStage::Specular` übernimmt jetzt die vorhandene Sphere-
Map aus der Actor-/Static-Bridge. Kamerabezogene Reflexionskoordinaten entstehen
wie im Classic-Pfad an den Vertices; die bestehende bewegte Texturmatrix,
Sampler, Textur-Alpha und Item-Stärke bleiben erhalten. Der Beitrag wird
linear vor Tone Mapping addiert, unabhängig vom Materialmodell. Keine
PBR-Aktivierung, Metallic-Schätzung oder zusätzliche Reflexion für normale
Legacy-Materialien. Keine neuen Targets oder per Frame erzeugten Ressourcen.

**Warum nicht Diligent-GGX?** Dieser Schimmer ist eine vorhandene, ausdrücklich
von Item-Daten aktivierte Spielgrafik. Diligent-GGX würde weder die originale
Sphere-Map noch ihre animierte Farbwirkung ersetzen. Der kleine Adapter erhält
die Spielsemantik auf der vorhandenen Diligent-Shader-/Resource-Infrastruktur.

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

Diese Faltungen beeinflussen ausschließlich PBR. Legacy-Materialien erhalten kein zusätzliches reflektiertes Sky-Licht.

## 27. Shadow Regression

Unveränderter Diligent ShadowMapManager/PCF-Pfad. G-DX-Tests einschließlich
Alpha-Cutoff, Off-Camera-Caster, transparenten Empfängern, Sun-Richtungen und
Shadow-/AO-Trennung bestehen. Das große B1-Tor zeigt weiterhin klare Schatten.

## 28. SSAO Calibration

Algorithmus und Stärke unverändert. SSAO bleibt auf indirekte Beleuchtung
begrenzt und räumlich; ungültige Motion History wird nicht benutzt.
Nach Exposure/Tone Mapping war keine zusätzliche Stärkeänderung erforderlich.

## 29. Legacy GR2 Proof

Originale lokale GR2-Geometrie/Diffuse laufen über nativen Reader,
neutrale Animation und GPU-Skinning. Ohne PBR-Override bleibt das Material diffus,
mit modernem Licht/Shadow/SSAO/HDR. Der originale, durch Item-Daten aktivierte
Rüstungs-/Waffenschimmer ist ausdrücklich erhalten und kein erfundenes PBR.

Neuer identischer nativer Vergleich: Classic / Modern vor Korrektur / Modern
nach Korrektur, originale Rüstung **11299** und Schwert **19**, Kamera 850/18/0
an B1 64000/55300. Originale Item-Stärken und normaler Armour-Specular-Startschalter.
Actor-Log bestätigt **stage=3 auf GPU-skinned part=0 und rigid part=1**;
**2720 tatsächliche Schimmer-Draws**, Materialmodell weiterhin Legacy,
CPU-Deformation/Fallbacks=0, Exit 0, Diligent/Shutdown=0.

GPU-Regression: sichtbar, proportional zur Item-Stärke, mit originaler Matrix
beweglich; ohne aktivierten Effekt bleibt normale Legacy-Ausgabe bytegleich.
15/16 bestehende GPU-Rohbilder sind ebenfalls bytegleich; beim PBR-Materialbild
weichen 3274 von 1950000 Farbkanälen um höchstens einen 8-bit-Schritt ab.
PBR-Material-/Normal-/Roughness-/Metallic-Gates sind grün.

## 30. Player Proof

Neuer Classic/Modern-Vergleich mit demselben finalen Client und fester
B1-Kamera 1800/22/0 an 64000/55300. Die unveränderten Aufnahmen zeigen erhaltene
Haut-/Rüstungsfarben und erkennbare Schattenseiten. G-DX bleibt als B-Referenz.
Idle-Zeitpunkte können abweichen; identische Kamera bedeutet keine identische
Animationsphase. Die endgültige subjektive Abnahme wurde vom Benutzer bestätigt (§52).

## 31. Mob Proof

Derselbe dunkle Wolf neben dem Legacy-Player im B1-Nahvergleich.
Gesicht, Rücken und Beine bleiben in der neuen Aufnahme unterscheidbar;
der diffuse Materialpfad erhält die Texturfarben ohne graue Reflexionsschicht.
Keine Fluss-/Unterwasserkamera als Ersatz. Finales Benutzerurteil siehe §52.

## 32. Building Proof

Großes bestehendes B1-Steintor, Kamera 4000/18/0 an 64000/55300.
Neue A/C-Aufnahmen zeigen Materialfarbe, Relief, Vorder-/Seitenflächen sowie
Boden-/Torschatten ohne allgemeinen Weltnebel. Der Schattenkontrast bleibt
deutlich räumlich. Die vorhandene G-DX-Aufnahme ist die B-Referenz.

## 33. Terrain Proof

Trockene B1-Gras-/Erde-/Wegfläche an 68900/53200, Kamera 5000/55/40.
Alle drei Versionen wurden an dieser Kamera aufgenommen, G-DX mit dem erhaltenen
Referenzbinary. Grün und Braun bleiben in Modern erkennbar; rechts liegt
bestehendes Wasser. Die hohe Kamera zeigt zugleich die volle Baumkrone.
Die zuerst erzeugte A1-Flussansicht wird nicht als Terrain-Farbproof verwendet.
Terrain bleibt diffus; der farblose Fog-Patch-Ersatz entfällt nur in Modern.

## 34. Vegetation Proof

Bestehende ZiiNAN-Vegetation auf B1 an 68900/53200; Kamera 3500/20/0,
Zielhöhe +500. Gleiches Motiv für Stamm, Äste und Bodenschatten. Bei dieser
niedrigen Kamera blendet der bestehende Modern-Pfad die Baumkrone stark aus;
das ist bereits in der G-DX-/bisherigen G56-Referenz sichtbar und wird nicht
als vollständige Kronenparität ausgegeben. Die zusätzliche hohe Terrain-Kamera
in §33 zeigt die volle Krone und ihre grünen Blattfarben in allen drei Ständen.
Keine neuen Bäume, Transmission, Grass oder Wind. Classic-Goldens bleiben grün.

## 35. Animated GLB Proof

[Nativer F5-Lauf](../../build/f5x/runtime/g56-colors-animated-01/f5x-character-smoke.log):
**9 Zustände, 9 Captures, 883 Frames, Exit 0**; Idle, Walk, Run, Attack,
Damage, Death und MultiInstance mit 20 Actors. Authored PBR, Attachments,
HDR, Shadows/SSAO, Bloom und Sky bleiben aktiv. Die Galerie zeigt tatsächlich
Idle (0), Walk (1) und Attack (4). Dieser native Lauf stammt aus der Farb-/Fog-
Revision; die abschließenden Release-/Debug-GLB-GPU-Gates sind erneut grün.

**17660 GPUFrames**, CPU-Deformation/Fallbacks=0. Tatsächlich gerendert:
**12664 PBR-Draws** und **45597 Legacy-Draws** der Umgebung. Alle Owner und
Diligent ERROR/FATAL am Shutdown=0. Der GPU-Materialtest ergänzt Normal-/AO-/
Metallic-/Roughness- und Override-Nachweise.

## 36. Emissive/Bloom Proof

GPU-Off/On-Paar mit Emission 8; normale weiße Kontrollfläche bleibt bytegleich.
Zusätzlich Emission 2 vs. 8, UI-BYPASS und eine Sky-IBL-Kontrolle.
Die PNG-Dateien sind ausschließlich verlustfreie Konvertierungen der GPU-Rohbilder.

## 37. Effects

Water, Fire, Skills, Particles, Weapon Trails, World Snow, Items/Flying und
PCBlocker liegen vor Tone Mapping; Damage/Miss danach. Alpha-/Blend-Semantik
bleibt erhalten, HDR-Welteffekte erhalten keinen Environment-Fog.

**Zweite Nachbesserung: blaue Aura um die Rüstung.** Die Originaleffekte
`armor-4-2-1.mse` und `armor-4-2-2.mse` wurden bereits korrekt eingereicht.
Die sRGB-Dekodierung ihrer additiven Leuchtstärke schwächte besonders die
weichen Farbverläufe so weit ab, dass die Aura gegenüber Classic verschwand.
Additive World-Effekte (Blend ADD, Ziel ONE) behandeln die authored Combiner-
Werte jetzt als Emissionsintensität. Quellfaktor und Alpha-Fade bleiben erhalten;
normale Alpha-Flächen werden weiterhin von sRGB nach linear gewandelt.
Der Effekt bleibt in HDR vor Bloom/Tone Mapping, mit normalem Depth-Test.
Keine pauschale Helligkeitsverstärkung, PBR-Änderung oder Nebelwirkung.

**Warum ein Adapter?** Die authored Legacy-Blendsemantik gehört zum Spiel.
Der kleine Zweig im vorhandenen Diligent-Effektshader benötigt weder neue
Targets noch zusätzliche Shader-Varianten oder per Frame erzeugte Ressourcen.

GPU-Regression vergleicht blaue additive Partikel mit derselben HDR-Emission:
Alpha 0/128/255 sowie SRC_ALPHA und SRC_COLOR; maximal 2/255 Abweichung.
Der native A/B/C-Vergleich mit Originalrüstung **12019**, Schwert 19 und Kamera
850/18/0 zeigt Classic, Modern vorher und korrigiertes Modern. Die Fixture
aktualisiert nun auch die Partikelsimulation, die im regulären Client bereits
von UpdateGame läuft. **346 Frames, 2768 Sphere-Map-Draws, Exit 0, Diligent und
Shutdown=0**. Originale Aura-, Rüstungs- und Waffenpartikel im Effektlog belegt.
Benutzerbestätigung: **"Aura und Schimmer sind wieder sichtbar"**.

Der vorherige [native Effektlauf](../../build/g56x/colors/effects-run.log)
(Fire, Melee, AoE, Snow, Drain; 5 Phasen/5 Captures, 23.72 s, Exit 0) bleibt
als frühere Evidenz markiert. Die finalen GPU-Gates prüfen Blend-Parität,
Fog-Unabhängigkeit, UI-Grenze und Bloom erneut.

## 38. UI

UI folgt dem SDR-Weltabschluss. GPU-Pixel RGB 208/96/32 bleibt unabhängig
von Exposure/Bloom exakt gleich. Der finale native Menütest besteht mit
16 Captures einschließlich Neustart. Nebelqualität ist entfernt; die echte
Classic-Nebelwahl ist in Modern sowohl im Grafik- als auch im alten Systemdialog
einschließlich Beschriftung ausgeblendet. Aktualisierung erfolgt bei Menüaktionen,
ohne Produktions-Polling. HUD/Schadenszahlen/Skills wurden zusätzlich in der manuellen Schlussabnahme vom Benutzer bestätigt (§52).

## 39. GraphicsSettings

Nebelqualität wurde aus Menü, öffentlicher Settings-API und Runtime-Config
entfernt. Alte `HIGH_QUALITY_FOG`-Dateiwerte werden validiert, kompatibel
eingelesen und beim Speichern entfernt; sie verändern keine Einstellung.
Keine Fake-Option. `fogLevel` bleibt eine echte Classic-Option und wird in
Modern in beiden Dialogen samt Beschriftung ausgeblendet. Bloom und Himmelqualität
bleiben. Die kleine Menübereinigung liegt im Assets-Arbeitsbranch; keine
sonstige UI-Neugestaltung.

## 40. Presets

| Preset | HDR/Tone Map | Bloom | Sky | Modern-Weltnebel |
|---|---|---|---|---|
| Modern Low | an | aus | Low | keiner |
| Modern Medium | an | aus | High | keiner |
| Modern High | an | an | High | keiner |
| Modern Ultra | an | an | High | keiner |

Classic behält seine eigene bisherige Nebeldarstellung. Keine zusätzlichen
Atmosphärenstufen oder unbeschrifteten Wirkungslos-Schalter.

## 41. Live Apply

[Finaler nativer Settings-Test](../../build/g56x/colors/settings-run-final.log):
**14 Schritte + 2 Neustartschritte PASS**, 16 Captures, 30.17 + 6.11 s, beide
Exitcodes 0. Nebelqualität fehlt in Dialog/API; beide Classic-Nebelwahlen
und ihre Beschriftungen sind ausschließlich in Classic sichtbar.
Bloom/Sky, Speichern/Laden, Classic↔Modern, Map-Wechsel, Dropdown-Schließen,
gedrückter Sichtweiten-Slider, Resize und Minimize/Restore sind enthalten.
Alle Ressourcen=0; finales Menü aus dem Assets-Branch im isolierten Root-Pack.

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

[Finale Performance-Sanity](../../build/g56x/colors/performance.md) und
[Rohzusammenfassung](../../build/g56x/colors/performance.json). Gleiche A1→B1→A1-
Sequenz, neun Kamerasegmente, 1024×768/VSync, erste 60 Frames je Segment
ausgeschlossen. Alle vier Läufe isoliert; finale Schimmer-Version erst nach
Abschluss der Builds/Gates und nach Beenden des vorherigen manuellen Clients.

| Messbereich über neun Kameras | G-DX | G56 bisher | Farb-/Fog-Revision | inklusive Item-Schimmer |
|---|---:|---:|---:|---:|
| CPU-Frame-Median ohne Present/Limiter, ms | 0.886–1.088 | 0.872–1.048 | 0.871–1.039 | 0.874–1.069 |
| GPU-Frame-Median, ms | 0.290–0.332 | 0.357–0.393 | 0.353–0.396 | 0.361–0.393 |
| Prozess Private Bytes Maximum, MiB | 621.2 | 652.2 | 662.5 | 666.6 |
| Frames | 3501 | 3490 | 3484 | 3480 |

Kurze Sanity, keine allgemeine FPS-Garantie oder Performance-Finish-Phase.
Unverändert: HDR-Scene **6291456 B**, Atmosphäre/Sky/IBL **9318384 B**, eigene
Targets **79572976 B**, upstream Bloom zusätzlich **5242872 B**. Keine neuen
Targets für Schimmer; er nutzt die bestehende Sphere-Map und Materialbindung.
Über 3480 Frames: CPU-Einreichung Atmosphäre 116.00 ms,
Bloom 58.55 ms, ToneMap 9.21 ms,
Composite 57.41 ms. Keine isolierten GPU-Passzeiten;
Prozess Private Bytes ist keine vollständige GPU-Heap-Messung. World-Fog-Pass=0. Diese Messreihe liegt vor der anschließenden Aura-Korrektur; dafür wird keine neue Performance-Messung behauptet. Der kleine Shaderzweig fügt keine Targets, Varianten oder Draws hinzu.

## 47. First Use

Loading-Prewarm für HDR/Atmosphäre/IBL/Bloom/Tone Mapping bleibt erhalten.
Initiales Ingame und Relog verwenden ihn. Bei bewusstem Classic→Modern-Wechsel
wird der freigegebene Owner mit Shadern neu aufgebaut; spätere Varianten
können beim ersten Auftreten entstehen. Keine Behauptung "0 Shader-Compiles
nach Startup". Alte manuelle Prewarm-Zahlen sind historische Evidenz.

## 48. Resource Lifetime

Persistente HDR/Lighting/Depth-, Bloom-, ToneMap-, Atmosphären-/Sky-/IBL-
und PostFX-Owner. Schimmer verwendet die existierende Textur-/Samplerbindung;
kein neuer RenderTarget oder per Frame erzeugter Owner. Die Mesh-Submission
hält ihre Sphere-Map bis zum Ende der jeweiligen Draw-/Shadow-Liste.

[Runtime-Audit](../../build/g56x/colors/runtime-audit.json): **27 abgeschlossene
Runtime-Verzeichnisse** einschließlich der ersten manuellen Sitzung, der
Schimmer-Vergleiche und des finalen Maplaufs. Alle 31 Nullfelder und acht
Renderer-Shutdownzeilen=0, ModernRenderers=0, Exit 0. Der aktuelle manuelle Abnahmelauf ist mit denselben Nullzählern geprüft. Die Aura-Vergleiche -01 liefen nach einem Fehler beim Erzeugen der Python-Fixture mit dem allgemeinen Testaufbau; sie gelten nicht als Aura-Nachweis. Die -02-Vergleiche verwenden die korrigierte Fixture.

## 49. Resize

Window-Targets und größenabhängige PostFX/Bloom-Ressourcen werden freigegeben
und passend neu angelegt; zugehörige SRBs werden zurückgesetzt.
Finaler GPU-Test **512×384→320×240 PASS** und nativer Fenster-Test **PASS**.
Größenunabhängige Atmosphäre/IBL bleibt bestehen.

## 50. Minimize/Restore

Finaler GPU-Test suspendiert bei 0×0 ohne Frame und stellt korrekt wieder her.
Finaler nativer Window-Test bestätigt **Minimize/Restore/Resize/OriginalSize**.
Die automatischen Captures und Nullzähler zeigen keinen verbleibenden Black
Screen oder Owner; beide Settings-Prozesse beenden sich mit Exit 0.

## 51. Map Change

Finale isolierte **A1→B1→A1-Serie inklusive Item-Schimmer PASS**,
3480 Frames, Exit 0. 218997 Legacy-Mesh-Draws,
185026 Terrain-Draws, 1215051 Shadow-Draws und
6960 originale Item-Schimmer-Draws.
CPU-Deformation/Fallbacks=0, Diligent ERROR/FATAL=0, alle Shutdown-Owner=0.
[Lauf](../../build/g56x/colors/shimmer-performance-run.log).

## 52. Login/Character Select

Die erste Benutzerfreigabe lag vor der verbindlichen Designänderung.
Danach meldete der Benutzer zuerst fehlenden Rüstungs-/Waffenschimmer und nach
dessen Korrektur zusätzlich die zu schwache blaue Aura um die Rüstung.
Beide Fehler wurden im laufenden Meilenstein nachgebessert (§25 und §37).
Die vorherige manuelle Sitzung `g56-colors-manual-02` ist mit Exit 0 beendet.

Der aktuelle isolierte Client `build/g0x/runtime/g56-colors-manual-03` enthält
die Aura-Korrektur und beide bereinigten Menüdateien. Release-Binary-SHA256:
`bb3cfb6d5a9619df2f93aae51f2ed8474377b831e0fa3d5576357b1c2d535434`.
Login/Select/Loading/Ingame, Relog, erneut Ingame und ShutdownClean sind im
[Lifecycle-Protokoll](../../build/g0x/runtime/g56-colors-manual-03/gdxc-lifecycle.log)
belegt. Beide Ingame-Einstiege erreichen WorldPresented. Exitcode 0,
68437 GPUFrames, CPU-Deformation/Fallbacks=0, Diligent ERROR/FATAL=0,
alle Ressourcen- und acht Renderer-Shutdownzeilen=0.

Der Benutzer bestätigt ausdrücklich **"Aura und Schimmer sind wieder sichtbar"**
und anschließend die vollständige Schlussfrage zu Farben, Nebelfreiheit,
Player/Mob/Gebäude/Terrain/Vegetation, HUD/Schadenszahlen/Skills sowie
Relog→Ingame→Beenden: **"Alles unauffällig, Relog und Beenden erfolgreich"**.
Damit ist die erweiterte manuelle Abnahme für diesen aktuellen Stand **PASS**.

## 53. Release

Finaler vollständiger Release-Build mit Sphere-Map- und Aura-Korrektur **PASS**.
Begrenzter Gate **64/64 PASS, 54.82 s**, Classic **12/12 bytegleich**.
[Build](../../build/g56x/colors/aura-release-build-final.log) ·
[Tests](../../build/g56x/colors/aura-release-tests-final.log).
Frühere Farb-/Fog-/Schimmer-Gates bleiben separat erhalten. Bekannte Linker-
und LTCG-Hinweise bleiben dokumentiert; keine Warnungsfreiheitsbehauptung.

## 54. Debug

Finaler vollständiger Debug-Build mit Sphere-Map- und Aura-Korrektur **PASS**.
Begrenzter Gate **64/64 PASS, 186.23 s**, Classic **12/12 bytegleich**.
[Build](../../build/g56x/colors/aura-debug-build-final.log) ·
[Tests](../../build/g56x/colors/aura-debug-tests-final.log).
Frühere Farb-/Fog-/Schimmer-Gates bleiben separat erhalten. Bekannte Linker-
und LTCG-Hinweise bleiben dokumentiert; keine Warnungsfreiheitsbehauptung.

## 55. GCC/LP64

Finaler portabler GCC/LP64-Build **PASS**, **34/34 PASS, 5.78 s**,
einschließlich expliziter PBR-Zuordnung, Legacy-Diffuse-Default und kompatibler
Entfernung des Fog-Keys.
[Build](../../build/g56x/colors/gcc-build-final.log) ·
[Tests](../../build/g56x/colors/gcc-tests-final.log).
Keine GPU-Abnahme im rendererlosen Harness.

## 56. Diligent Diagnostics

Finale Release-/Debug-GPU-Gates und alle abgeschlossenen nativen Läufe:
**Diligent ERROR=0, FATAL=0**. Neue Materialbindung im nativen Sphere-Map-
Vergleich geprüft. Der bekannte Game-Log-Eintrag `invalid idx 0` bleibt
separat; die Game-Log-Größen stehen im Runtime-Audit. Die erste manuelle
Sitzung beendete sich ebenfalls mit Exit 0 und Nullzählern, trotz gemeldetem
visuellem Schimmerfehler. Technische Nullzähler ersetzen keine visuelle Abnahme.

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

Finale SHA256-Vergleiche nach der Aura-Korrektur gegen die erhaltene
G-DX-Referenz: **Release 12/12 und Debug 12/12 bytegleich**.
[Release](../../build/g56x/colors/aura-classic-Release.json) ·
[Debug](../../build/g56x/colors/aura-classic-Debug.json).
Rohbilder beider finalen Konfigurationen getrennt gesichert. Classic weiterhin
ohne HDR-/ToneMap-/Bloom-/Atmosphärenänderungen.

## 61. Visual Gallery

[A/B/C-Galerie](../../build/g56x/colors/visual-proof.html),
[44 Originalreferenzen mit SHA256](../../build/g56x/colors/visual-manifest.json).
Feste Kameras für Player/Wolf, Tor, Terrain und Vegetation dokumentieren die
Farb-/Fog-Revision. Zusätzlicher identischer Vergleich zeigt den anschließend
gemeldeten Rüstungs-/Waffenschimmerfehler vor und nach der Korrektur sowie Classic.
Alle GPU-Proofs stammen aus dem abschließenden Release-Gate: Legacy-Farben,
PBR-Materialreihe, GLB, Bloom, UI-Bypass und drei Schimmer-Kontrollen. Zusätzlich zeigt der native Aura-Vergleich (§37) die zweite Nachbesserung. Der neue additive GPU-Proof liegt separat als `aura-release-gpu-proof/g56-authored-aura.ppm` vor.
Weitere native Aufnahmen: drei Sonnenstände, GLB-Idle/Walk/Attack, Menü, Effekte.
Originale native JPEGs unverändert; GPU-PPM/BMP nur verlustfrei nach PNG.
Kronenausblendung bei niedriger Modern-Kamera offen benannt (§34).

## 62. Zero Legacy Audit

Release und Debug: **0 direkte D3D9-/Granny-/SpeedTree-DLL-Imports**.
[Importlisten und Binary-SHA256](../../build/g56x/colors/zero-legacy.json).
Bestehende Source-/Link-/Unabhängigkeitsgates sind erneut grün. Native Runtimes:
GrannyFileReads=0, CPU-Deformation=0, GPUFallbacks=0. Production bleibt ZiiNAN
GR2, neutrale Animation und Vegetation. Vorhandenes DDRAW ist kein D3D9;
keine OS-Modulenumeration behauptet.

## 63. Git Diff

Source: `codex/g56-hdr-atmosphere`, Designrevision auf vorhandenem `ec3d41c`.
Assets: `codex/g56-hdr-colors`, HEAD `72d92593`; zwei Menüdateien geändert.
Die neue, ungestagte Datei `tests/Graphics/prepare_g56_shimmer.py` ist in beiden
Source-Patches enthalten; sie reproduziert den nativen Rüstungs-/Waffenvergleich.
Main-Referenzen unverändert. Kein Staging, Commit, Push oder Rewrite durch
diese Fortsetzung. Builds, Logs, Binaries und Bilder bleiben ignoriert.

- [Source-Designrevision ab ec3d41c](../../build/g56x/colors/source-revision.patch)
- [Gesamtes G56 ab 77645d2](../../build/g56x/colors/source-g56-full.patch)
- [Assets-Menüänderung](../../build/g56x/colors/assets-menu.patch)
- [Status, Patch-/Whitespace-Prüfung und Hashes](../../build/g56x/colors/git-audit.json)

Beide Arbeitsbäume mit explizitem Repo-Pfad geprüft; Index leer. Die Patches
enthalten keine Build-/Log-/Binary-/Screenshot-Dateien.

## 64. Known Limitations

- SDR-Ausgabe; D3D11-GPU-Abnahme unter Windows, portable Verträge unter GCC/LP64.
- Atmosphäre nur Sky/Horizon/Sonne; Single Scattering, keine Wetter-/Volumetric-Engine.
- Bestehende starke Kronenausblendung bei niedriger Modern-Kamera (§34); voller Blattfarbproof zusätzlich aus hoher Kamera.
- Vorhandene View-/LOD-/Splatgrenzen bleiben sichtbar möglich; kein Nebel kaschiert sie.
- Legacy-Diffuse ist originalnah, nicht Classic-pixelgleich: modernes Licht/Shadow/SSAO/Tone Mapping bleiben.
- GLB ohne authored PBR-Block verwendet gemäß neuer Materialpolitik Diffuse statt automatisch erfundenem Metallic.
- Expliziter Classic→Modern-Wechsel erstellt Ressourcen und Shader neu.
- Keine isolierten GPU-Passzeiten oder vollständige Treiber-Speicherbilanz.
- Bekannte Linkerwarnungen und getrennte Game-Log-Baseline bleiben dokumentiert.

## 65. GO/NO-GO

**G5/6-X = GO. Technische Gates und vollständige manuelle Abnahme PASS.**
Legacy-Farben ohne künstliches PBR, explizite PBR-Route, Sky/Horizon ohne
Weltnebel und bereinigtes Menü sind implementiert. Originaler Item-Schimmer
und additive blaue Aura sind wieder sichtbar und gezielt getestet.
Release 64/64, Debug 64/64, unveränderte portable Verträge GCC/LP64 34/34,
Classic je 12/12 bytegleich, Runtime-/Importaudits grün. Benutzer bestätigt
die vollständige aktuelle Schlussabnahme; Login→Select→Ingame→Relog→Ingame→
ShutdownClean mit Exit 0 und Nullzählern protokolliert (§52).
Kein Neustart des Milestones, kein Commit/Push. Nach G5/6 STOP.

## 66. Recommendation G7-X

G56 stellt Sun, Sky, HDR Scene Color und Atmosphären-Inputs als Grundlage bereit.
**G7 nicht begonnen.** Ebenso kein G8, H2, UI-Redesign, Texture Impact Pass oder
Performance Finish. Nach diesem Bericht, Tests, Galerie und Diff: **STOP**.
