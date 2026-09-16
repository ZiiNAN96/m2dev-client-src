# G8-X – Final Visual Integration & Sky Polish

Stand: 2026-09-16. **G8-X = GO. Finale Nutzerabnahme erteilt: „sehr gut alles“.**

Release **67/67**, Debug **67/67**, GCC/LP64 **39/39**. Classic in beiden Windows-Konfigurationen **12/12 bytegleich**.
Die abschließenden nativen Serien sind **PASS: 26 + 15 + 6 Ansichten**, jeweils Exit 0, geprüfte Fallback-/Shutdown-Zähler 0. Die Performance-Serie ist zusätzlich **4/4 PASS**. Alle finalen Läufe verwenden die unten genannte Release-Binary.

- [Finale Galerie](../../build-g8x/gallery/index.html)
- [Performance-Baseline](../../build-g8x/performance/baseline.md), [vollständige Messwerte](../../build-g8x/performance/baseline.json)
- [Texture Impact](../../build-g8x/texture-audit/texture-impact.md), [CSV](../../build-g8x/texture-audit/texture-impact.csv), [JSON mit Einzelreferenzen und Lücken](../../build-g8x/texture-audit/texture-impact.json)
- [Release-Protokoll](../../build-g8x/evidence/Release-final-tests.log), [Debug-Protokoll](../../build-g8x/evidence/Debug-final-tests.log), [portable Tests](../../build-g8x/evidence/portable-final-tests.log)
- [Zusammengeprüfte Nachweise und Quellhashes](../../build-g8x/evidence/verification.json), [vollständiger ungestagter Diff](../../build-g8x/evidence/git-diff.patch)

## 1. Baseline

Beim angenommenen Start waren beide Arbeitsbäume sauber. Source: `codex/g56-hdr-atmosphere`, HEAD `8d42cce` (CLEAN-X), davor `4cb5b9f` (G7). Assets: `codex/g56-hdr-colors`, HEAD `6c713237`, davor `40445c57`.
Der G7-Vergleich verwendet die erhaltene Release-Binary aus `build-cleanup-validation`, SHA256 `4aaa0276719f4907c405f23fd8d419274511770c22d8b3711977f319d406e5ea`.
G8 wurde frisch in `build-g8x/msvc` konfiguriert und in Release/Debug gebaut. Finale Release-SHA256: `4c362615c0b3b008a95f574a7e3591ad7059cee95cf96e3e681ea818c0212a8c`; Debug: `082982302bcf61dfe51a26b33d01a4a16029072ec9fe55de63e62e19f35db177`.

## 2. Visual Design Rules

Metin2 mit moderner Beleuchtung: lesbare Figuren, warme Böden, erkennbare Originalmaterialien, ruhiger Himmel. Keine globale Entsättigung, keine Filmfarbkorrektur. Der Nutzer hat das finale Zielbild am 2026-09-16 bestätigt.

## 3. Legacy Material Policy

Die vorhandene Trennung bleibt bestehen: Legacy-GR2 erhält Licht/Schatten/AO, aber keine erfundenen metallischen oder spiegelnden Materialien. Vollständiges PBR bleibt echten GLB-Materialdaten beziehungsweise expliziten GR2-Overrides vorbehalten. Der GPU-Test prüft, dass Metallic-/Roughness-/IBL-Werte die Legacy-Diffuse nicht verändern und explizites PBR weiterhin reagiert.

## 4. No World Fog Policy

Modern-Weltgeometrie und Partikel erhalten weiterhin keinen allgemeinen Environment-Distanznebel. `fogColor` dient ausschließlich als dezenter Horizontfarbwert. Opaque, Forward und Effects sind im GPU-Test gegen extremen Fog auf Bildgleichheit geprüft. Classic behält seinen bisherigen Nebel.

## 5. Sky Baseline

G7 lieferte einen sehr weichen, grauen Himmel und verwendete unterschiedliche zusätzliche Horizontmischungen für Himmel und Wasser. Die Galerie enthält unveränderte G7-Aufnahmen mit denselben Kameras. Frühere Versuche mit falschem Snow-Mapnamen oder ungünstiger Kamera gehören nicht zur finalen Galerie.

## 6. Final Sky Architecture

Die vorhandenen DiligentFX-Atmosphärenkoeffizienten, Optical-Depth-/Single-Scattering-LUTs und der bestehende Atmosphere-Owner bleiben maßgeblich. Der vorhandene Sky-Shader ergänzt eine selbst geschriebene zweidimensionale Wolkenschicht. Low verwendet 128×64, die normale/hohe Stufe 512×256 FP16. Keine zweite Engine, keine externe Sky-Datei und keine neue Runtime-Abhängigkeit.

## 7. Sun Integration

Weiterhin eine Quelle: `SceneLighting.sunDirection/sunColor/sunIntensity`. Beleuchtung, Schatten, sichtbare Sonnenscheibe, Atmosphäre und Wasser-Specular benutzen diese Daten. Der GPU-Test prüft Position, Kompaktheit und Verdeckung der Sonnenscheibe. Morgen/Mittag/Abend in der Galerie sind diagnostische Sonnenrichtungen, kein neues Tageszeitsystem.

## 8. Sky Colors

Die bestehende Aerosoldichte wurde auf 0,12 kalibriert, das Sky-Signal mit moderater Blaugewichtung abgestimmt. Der Horizont erhält höchstens sechs Prozent des vorhandenen Environment-Farbwerts, nach oben weich abfallend. Die getrennten zusätzlichen Horizontmischungen in sichtbarem Himmel und Wasser wurden entfernt. Das betrifft keine Terrain- oder Objekttextur.

## 9. Clouds

Eine weiche, periodische 2D-Schicht mit sechs gewichteten Rauschfrequenzen; keine Volumetrik, kein Raymarching. Eine vorhandene Environment-Cloud-Texturreferenz aktiviert 0,65 Deckung; ohne Referenz bleibt der Himmel klar. Die Textur selbst wird nicht ersetzt oder zusätzlich geladen.

Absolute monotone Zeit statt Frame-Akkumulation; Perioden der beiden Offsets 40 und 80 Minuten. Double-Präzision vor dem Upload und korrektes periodisches Wrapping auch bei negativen Koordinaten. Tests vergleichen 60/120 Schritte, wiederholte feste Zeit, Zeitsprung, Rückkehr und nicht endliche Eingaben.

## 10. HDR Final Calibration

Der vorhandene FP16-Weltpfad bleibt erhalten. Kalibriert wurde der Himmel über A1, B1, Wald, Feuerland und Schnee; die Welt erhält keinen zusätzlichen Farbfilter. Die fest eingestellten Kameras zeigen Player, NPC, Wolf, Boss, Reittier und Reiter. Dunkle Vegetationsbereiche bleiben als Eigenschaft der vorhandenen Assets sichtbar; sie wurden nicht durch einen H2-Pass verändert.

## 11. Exposure

Basis weiterhin 2,0, multipliziert mit dem bestehenden Environment-Exposure-Bias. Keine neue automatische Belichtung und kein pro Map erfundener Exposure-Override. Der GPU-Test prüft Lesbarkeit von Legacy-Mitteltönen und die Reaktion echter HDR-Emission.

## 12. Tone Mapping

Bestehender Diligent-first Tone-Mapping-Pfad unverändert. Grün/Braun/Rüstungsfarbe werden im GPU-Test auf Chromatizität geprüft. Highlights über 1 bleiben bis zur Ausgabe im HDR-Target. Keine LUT und keine neue Tone-Mapping-Technik.

## 13. Bloom

Threshold 2,0, Intensität 0,06, Radius 0,65 unverändert. Gewöhnliches Weiß unterhalb der Schwelle produziert im GPU-Test exakt keinen Bloom; echtes Emissive erzeugt einen messbaren Halo. Die Wolkenfarbe ist begrenzt, während die Sonnenscheibe HDR bleiben darf. Low/Medium standardmäßig aus, High/Ultra an; Custom-Off ist in der Galerie enthalten.

## 14. Armor/Weapon Effects

Originale Rüstungen 11299 und 12019 werden jeweils in Classic/Modern aufgenommen, einschließlich authored Surface-Shimmer und blauer Aura. Die echten Armor-/Sword-Effektpfade stehen im nativen Effect-Log. GPU-Regressionen prüfen Schimmerstärke, bewegte Sphere-Matrix, Blend-Faktoren und additive blaue Aura einschließlich Alpha-Fade. Waffenangriffe werden im ergänzenden Effektlauf über die registrierte Einhand-Combo ausgelöst.

## 15. Shadows

Bestehende CSM/PCF-Technik und Bias bleiben erhalten. Modern Low verwendet nun tatsächlich eine günstige 512er Kaskade statt ausgeschalteter Schatten; Medium 2×1024, High 3×1536, Ultra 4×2048. Classic-Presetwerte sind unverändert. Keine separate Sonnenrichtung für Schatten.

## 16. SSAO

Bestehende FX-AO-Stufen unverändert: aus / niedrige SSAO-Stufe / hohe Stufe. Der historisch gespeicherte Enumname `GTAO` bezeichnet die hohe vorhandene FX-Qualität, keine neue AO-Engine. Keine Intensitätsanhebung und kein grauer Overlay-Pass.

## 17. Water

G7-Wassertechnik, Normalstärke, Fresnel, Absorption, Refraction und Roughness bleiben erhalten. Low nutzt eine Normalschicht ohne SSR/Refraction/Absorption, Medium zwei Schichten mit Refraction/Absorption, High zusätzlich SSR in halber Auflösung mit 64 Traversalschritten, Ultra volle Auflösung mit 128. Der vorhandene Water-GPU-Test prüft diese Pfade und Ressourcenwechsel.

## 18. Sky/Water Integration

Himmel und Wasser lesen denselben aktuellen Sky-Atlas einschließlich Wolken. Die gemeinsame Sonne steuert Scheibe und Sun Glint. PBR-IBL wird bei Beleuchtungs-/Horizont-/Qualitätswechsel neu gefiltert; wandernde Wolken lösen bewusst keine 42 Cubemap-Convolution-Passes pro Frame aus. IBL verwendet den kalibrierten klaren Himmel als niederfrequente Näherung. SSR bleibt räumlich ohne neue temporale History.

## 19. Terrain

Keine Textur ersetzt. A1-Pflaster und warme Erde, B1-Grün, dunkler Fels und helle Schneeflächen sind dokumentiert. Der GPU-Farbtest schützt Grün/Braun zusätzlich unabhängig vom Kamerabild. Texturqualität und Sichtweite bleiben an ihre vorhandenen echten Stufen gebunden.

## 20. Vegetation

Weiter Registry → ZVEG → GLB mit dem vorhandenen ZiiNAN-Renderer. Keine neuen Bäume, keine Blatt-Transmission, kein 3D-Gras. Waldaufnahmen enthalten Bark/Leaves und Schatten. Das Audit klassifiziert Rinde/Blätter anhand der nativen Part-Kinds, nicht nur anhand von Dateinamen.

## 21. Effects

Der zusätzliche Lauf verwendet die original registrierte Sura-Schutz-Aura, den Feuer-Debuff, Rüstungs-/Waffeneffekte und Schnee in echten Maps. Dabei wurde ein bereits mit G7 reproduzierbarer Fehler gefunden: neu erzeugte Schneepartikel konnten vor ihrem ersten Farbdraw eine uninitialisierte Billboard-Basis lesen. `CSnowParticle::Init` setzt diese beiden Richtungsvektoren jetzt auf null. Der korrigierte native Schneelauf endet mit Exit 0; der alte Fehllauf mit nicht endlicher Vertexposition bleibt als negative Evidenz erhalten.

## 22. UI

UI bleibt nach Tone Mapping. Der GPU-Test weist bytegenau unveränderte UI-Farbe bei verändertem Exposure/Bloom nach. Die Galerie enthält diagnostischen Text; sie ersetzt keine vollständige manuelle HUD-/Inventory-/Chat-/Damage-Number-Abnahme im normalen Client. Keine UI-Implementierung geändert.

## 23. Low Preset

Modern HDR; 1×512 Schatten, AO aus, Bloom aus, Sky 128×64, Water Low, SSR aus. Sichtweite 12800, vorhandene niedrige Vegetationsdistanz und Medium-Texturqualität. Die zusätzliche Schattenarbeit ist im Performancevergleich separat ausgewiesen.

## 24. Medium Preset

2×1024 Schatten, niedrige FX-AO-Stufe, Bloom standardmäßig aus, Sky 512×256, Water Medium und SSR aus. Sichtweite 19200. Keine rein kosmetische Presetbezeichnung.

## 25. High Preset

3×1536 Schatten, hohe FX-AO-Stufe, Bloom an, Sky 512×256, Water High und SSR halbaufgelöst. Sichtweite 25600. Empfohlener Ausgangspunkt für die finale Nutzerabnahme.

## 26. Ultra Preset

4×2048 Schatten, hohe FX-AO-Stufe, Bloom an, Sky 512×256, Water Ultra und SSR vollaufgelöst/128 Schritte. Sichtweite 38400 und vorhandene höhere Vegetations-/Texturstufe. Kein eigener Ultra-Sky nur für eine zusätzliche Bezeichnung.

## 27. Custom

Änderung einer Einzeloption setzt weiterhin Custom. Im nativen Ablauf: High → Bloom aus → Custom. Settings-/Persistenztests decken diesen Vertrag ab.

## 28. Live Apply

Zwei vollständige Zyklen Low → Medium → High → Ultra → Custom → Low am B1-See, zusätzlich die Presetfolge im Hauptlauf. Der Renderer-Owner bleibt bei Modern-Wechseln erhalten; Welt-/Mapobjekte werden pro Kamera neu geladen. GPU-Tests prüfen zusätzlich Target-/Resize-/Style-Lifetime. Die Protokolle enthalten effektive Settings je Ansicht.

## 29. Map Matrix

| Szene | Map | Position x/y | Kamera Distanz / Pitch / Rotation |
| --- | --- | --- | --- |
| A1-Platz | `metin2_map_a1` | 63500 / 59000 | 4500 / 15 / 0 |
| B1-Nordufer | `metin2_map_b1` | 68900 / 53200 | 5000 / 55 / 40 |
| Wald | `metin2_map_trent` | 15000 / 15000 | 4000 / 25 / 0 |
| Feuerland/Fels | `metin2_map_n_flame_01` | 50000 / 50000 | 4500 / 25 / 0 |
| Schnee | `map_n_snowm_01` | 30000 / 30000 | 4000 / 25 / 0 |
| B1-See | `metin2_map_b1` | 71500 / 55000 | 5000 / 18 / -70 |
| B1-Küste | `metin2_map_b1` | 18000 / 80000 | 6000 / 15 / 90 |

Höhenversatz, Sonnenzustand, Preset und Effektparameter stehen vollständig in den jeweiligen `views.json`. Gleiche Kamera in G7/G8 und Classic/Modern; Wasser- und G8-Wolkenzeit für Bilder fest auf 10 Sekunden. Partikel und Figurenanimationen sind nicht eingefroren.

## 30. A1

Platz mit Player/Mobs, Gebäuden, Boden und Vegetation; zusätzlich engere Armor-Aufnahmen und eine erhöhte Horizontansicht. Die Sonnentestkameras liegen oberhalb des Terrains, damit der Himmel tatsächlich sichtbar ist.

## 31. B1

Nordufer für Terrain-/Presetvergleich; separater See und Küstenblick für sichtbares Wasser. Die erste Norduferkamera wird ausdrücklich nicht als alleiniger Wassernachweis verwendet.

## 32. Vegetation Map

Trent in Classic/Modern. Originale Leaf-Atlanten und Silhouetten bleiben erhalten. Dunkle Kronen, Alpha-Kanten und alte Detailauflösung sind Content-/H2-Kandidaten, keine in G8 heimlich ausgetauschten Assets.

## 33. Water Map

Der B1-See wurde anhand des originalen Water-Grids gewählt: Wasserhöhe 19018, Terrain am Bezugspunkt 18851,5. Der Blick zeigt Wasserfläche, Ufer und Gebäude. Ergänzend Küste sowie Feuer-/Auraeffekte in Seenähe. Wasserpixel und Himmel verwenden denselben kalibrierten Atlas.

## 34. Texture Impact Audit

16.709 relevante Dateien aus 50.983 effektiven Packeinträgen, 85 Maps mit TextureSets, 896 platzierte Environment-Assets und 697 verschiedene Texturreferenzen. Die Packpriorität entspricht der Reihenfolge des Produktionsclients; spätere Einträge gewinnen.

Terrain zählt nur tatsächlich in `tile.raw` vorkommende, aufgelöste Splat-Indizes. Eine Asset-Referenz ist hier eine Tile-Datei, bei GR2/ZVEG ein unterschiedliches platziertes Modell. Maps sind unterschiedliche Mapwurzeln, keine serverseitig aktiven Maps. Millionen Tile-Samples werden separat ausgewiesen; doppelte Tile-Ränder sind enthalten. Vegetations-LOD-Referenzen werden pro Modell/Textur dedupliziert.

Kategorien: Grass 36, Dirt 63, Path 2, Rock 6, Stone 118, Wood 20, Cliff 5, Bark 26, Leaves 5; 418 bleiben unklassifiziert. Kategorien dürfen überlappen. Water Normals sind die bereits vorhandene generierte 128×128-Textur aus `DiligentWater.cpp`; es existiert keine Quelldatei für einen Texturaustausch.

Grenzen: keine fehlenden Packs; 18 Source/Pack-Abweichungen, 10 nicht aufgelöste platzierte GR2-Dateien, 12 nicht gefundene referenzierte Texturen, 59 fehlende Splat-Indizes, ein GR2-Parsefehler außerhalb der aufgelösten platzierten GR2-Modelle. 308 nicht aufgelöste Map/Property-CRC-Paare (92 CRCs, 19.263 Platzierungen) enthalten auch nicht modellbasierte beziehungsweise nicht auditierte Property-Typen und werden nicht still als null gezählt. 30 ersetzte Zeichen beim Dekodieren alter Texte sind dokumentiert. Die Zahlen beschreiben den aufgelösten Bestand, keine vollständige Sichtbarkeitsabdeckung.

## 35. Texture Top List

Priorisierung nach belegter Mapverbreitung und Referenzen; keine behauptete Bildschirmfläche oder Spielerfrequenz. Die vollständige Top-50 steht im Audit.

| Kandidat | Kategorie | Maps | Asset-Referenzen |
| --- | --- | ---: | ---: |
| `tree/compositemapb1.dds` | Leaves / Atlas | 47 | 15 |
| `tree/pagodatreebark.dds` | Bark | 45 | 5 |
| `zone/b/obj/general_obj_stone02.dds` | Stone | 36 | 5 |
| `terrainmaps/dungeon/field 01.dds` | Dirt | 21 | 290 |
| `terrainmaps/b/field/field 04.dds` | Dirt | 17 | 174 |
| `terrainmaps/b/grass/grass 03.dds` | Grass | 14 | 85 |
| `terrainmaps/b/tile/tile01.dds` | Stone / Path | 13 | 52 |
| `terrainmaps/b/field/field 01.dds` | Dirt | 11 | 115 |
| `terrainmaps/b/grass/grass 02.dds` | Grass | 11 | 98 |
| `zone/devils_dragon_island/camp_tent_woodhut_00.dds` | Wood | 4 | 10 |
| `zone/devilcave/dc_enterance_rock.dds` | Rock | 4 | 2 |
| `terrainmaps/g/field/cliff_swp_05.dds` | Cliff | 2 | 2 |

Alle Pfade haben das Präfix `d:/ymir work/`. Die Liste mischt bewusst die Kategorien; eine nur nach Mapzahl sortierte Gesamtliste steht im JSON/CSV. Die beiden Path-Kandidaten wurden zusätzlich als Pflasterbilder visuell geprüft.

## 36. Future Texture Content Pass

CONTENT-TX-X kann mit häufigem Grass/Dirt/Pflaster beginnen; originale Farbwirkung und Kachelmaß müssen dabei erhalten bleiben. Bark/Leaf-Atlanten separat mit H2 koordinieren. Water Normals benötigen keinen externen Texturtausch. In G8 wurden keine Terrain-/Environment-Texturen ersetzt.

## 37. Performance

Eine begrenzte G7/G8-Messung auf derselben B1-Kamera, vier Presets, 1024×768 und sechs Figuren. Je Preset die ersten 180 warmen Frames nach zwei Sekunden; damit liegt das Screenshot-Schreiben außerhalb des Messfensters. Keine Ausreißerfilterung. G8-Wolken laufen mit Produktionszeit, Wasserzeit ist in beiden Läufen fixiert. Hardwareinventar: Ryzen 7 9800X3D, RTX 5070 Ti und integrierte Radeon; der vorhandene Messzähler protokolliert den ausgewählten Adapter nicht.

GPU-Mittel G7 → G8: Low 0,252 → 0,257 ms; Medium 0,459 → 0,489 ms; High 0,723 → 0,797 ms; Ultra 0,944 → 0,903 ms. CPU-Mittel: 0,962 → 1,164; 1,123 → 1,421; 1,324 → 1,385; 1,788 → 1,673 ms. Keine grobe Regression in dieser Szene; kleine Differenzen sind bei einem einzelnen kurzen Lauf nicht statistisch abgesichert.

Average, Median, P95 und P99 für CPU-/GPU-Frames stehen in der verlinkten Baseline. CPU bedeutet vorhandene verstrichene Frame-Arbeit ohne Present/Frame-Limiter, nicht Betriebssystem-CPU-Auslastung. GPU nutzt bestehende asynchrone Duration-Queries. Low hat wegen seiner neuen Schatten mehr Arbeit. Diese begrenzte Szene liefert keinen Anspruch auf universelle Frameraten.

## 38. P-X Baseline

Gespeichert: Roh-CSV, Statistiken, Binärhashes, Actor-Draws, kumulative Mesh-/Terrain-/Shadow-/Water-Draws, Passzähler und CPU-Submit-Summen für Shadows/AO/Atmosphere/Bloom/ToneMap/Composite/Water/SSR. Die CPU-Passsummen enthalten Initialisierung und Presetwechsel; sie sind keine GPU-Passtimings. Fehlende einzelne GPU-Passtimer werden nicht erfunden. Die größere Sky-Textur kostet zusätzlich 786.432 Bytes, also 0,75 MiB.

## 39. First Use

Keine zusätzlichen Runtime-Varianten pro Wolkenframe; der neue Shader entsteht im vorhandenen Atmosphere-Konstruktor und wird vom normalen Loading-Prewarm erfasst. Der direkte Map-Test umgeht den Netzwerk-LoadingWindow-Pfad: seine kalten Frames enthalten bewusst Kompilierung. SkyPS-Kompilierung im Vergleich etwa 17,5 ms G7 / 56,2 ms G8, Atmosphere-Konstruktion etwa 117,3 / 157,0 ms.

Nach dem sichtbaren Start erreichte der normale Client tatsächlich Game. Sein Lifecycle-Log belegt LoadingPrewarmBegin → LoadingPrewarmEnd (3396 ms für die gesamte kalte Weltvorbereitung), dann WorldReadyForPresent → WorldPresented nach 3 ms. Einzelne später sichtbare Material-/Effect-PSOs werden weiterhin im bestehenden Pfad erstellt; beobachtete Einträge lagen unter 0,3 ms. Das ist ein konkreter Login-Nachweis, keine universelle Garantie gegen Hitches. Relog bleibt offen.

## 40. Resize

Native High-/Ultra-Fensterprobe: Resize auf 900×650, anschließend ursprüngliche Größe; zusätzlich die GPU-Resize-/Ressourcentests. Die Probe meldet für beide Stufen alle vier Ergebnisse mit 1. Kein neuer Resize-Pfad.

## 41. Minimize/Restore

High und Ultra: Minimize/Restore erfolgreich in derselben nativen Probe, zusätzlich suspend/resume der HDR-Targets im GPU-Test. Der Probeaufruf automatisiert die Fensterzustände; er ersetzt keine Behauptung über jede Desktop-/Monitor-Konfiguration.

## 42. Map Change

A1 → B1 → Trent → Feuerland → Schnee → A1 mit weiteren B1-Wechseln. Ein Environment-Horizontwechsel invalidiert jetzt den gecachten Himmel auch bei unveränderter Sonnenrichtung; der GPU-Test erzwingt genau diesen Fall. Wolkenzeit verwendet eine gemeinsame monotone Zeitbasis und startet nicht pro Map neu.

## 43. Relog

**Teilweise beobachtet, vollständiger Relog NOT RUN:** Der nach allen Gates sichtbar geöffnete normale Client protokollierte HandShake → Login → Select → Loading → Game → WorldPresented. Eine anschließende vollständige Relog-Rückkehr ins Spiel ist nicht nachgewiesen. Der Offline-Harness prüft reale Mapwechsel, aber keine Server-Session. Es wurde keine Anmeldung mit geratenen Zugangsdaten vorgenommen.

## 44. Classic Goldens

**PASS: 12/12 bytegleich**, jeweils in Release und Debug, gegen die erhaltenen Referenzen vor CLEAN-X. Screenshot-JPGs sind nicht der Bytegleichheitsnachweis. Die Modern-Low-Änderung berührt Classic-Presets nicht. Die Schneeinitialisierung korrigiert undefinierten Zustand vor einem Draw; der komplette Classic-Gate bleibt grün.

## 45. Diligent Diagnostics

Release-/Debug-GPU-Gates prüfen ERROR/FATAL = 0. Native finale Läufe prüfen zusätzlich `DiligentErrors=0`, `DiligentFatals=0` und keine Diligent-Diagnostikdatei mit Fehlerinhalt. Die Haupt-/Schneemapserie erzeugt 14 bekannte Game-Asset-Meldungen zu drei Property-CRCs; sie stimmen als Multimenge exakt mit G7 überein. Das sind bekannte Content-Lücken, kein behauptetes vollständig leeres Spiel-Errorlog.

## 46. CPU/GPU Fallbacks

Die nativen Instrumente prüfen AllCPUDeformationCalls/Vertices = 0, GPUFallbacks = 0, GrannyFileReads = 0. ZiiNAN GR2/AnimationRuntime/GPU-Skinning und ZiiNAN-Vegetation bleiben Produktionsdefaults. Architektur-/Dependency-Gates schützen D3D9-, Granny- und SpeedTree-Freiheit; der Windows-Import-/Header-/Linkaudit verwendet die finalen Buildverzeichnisse.

## 47. Resource Lifetime

Nach Exit werden Modern-/Water-Owner, SourceTextures/Buffers, Skin-/Collision-/Material-/Animation-/GR2-/Vegetation- und Frontend-Ressourcen geprüft. Die existierenden Owner umfassen Sky/Cloud-Atlas, HDR/Bloom/Atmosphere, Shadows/AO sowie Water/SSR. Die Angaben beziehen sich auf vorhandene Instrumentierung, nicht auf eine vollständige Treiber- oder Betriebssystem-Heapmessung.

## 48. Release

**67/67 PASS**, 67,33 s. Frischer Build im Visual-Studio-x64-Environment, vollständige absolute `tests/fast-tests.txt`, keine Vendor-Fuzzer. Die bestehenden Linkerwarnungen zu fehlenden Vendor-PDBs und CRT-Bibliotheken bleiben dokumentiert; kein Anspruch auf einen warnungsfreien Build.

## 49. Debug

**67/67 PASS**, 163,19 s, gleiche Fast-Gate-Liste und finale Quellen. GPU-Diagnostik und Classic-Goldens enthalten. XML-Nachweis: `build-g8x/evidence/Debug-final.xml`.

## 50. GCC/LP64

**39/39 PASS** mit Cygwin GCC 12.4, portablem Build und derselben Fast-Gate-Auswahl. Settings/Environment/Preset-/ABI-Verträge werden ohne künstliche GPU-Pässe geprüft. MSVC-/D3D11-Tests bleiben getrennt.

## 51. Visual Gallery

47 feste Ansichten pro Build: 26 Hauptansichten, 15 See/Küste/Live-Apply, sechs ergänzende Effektansichten, insgesamt 94 Bilder. G7/G8 stehen nebeneinander, Classic/Modern pro Szene direkt untereinander. Originale 1024×768-Client-JPGs ohne nachträgliche Farbkorrektur. Der zweite G7-Effektlauf beendet alle sechs Ansichten; der vorherige Fehllauf bleibt separat erhalten. Der uninitialisierte Schnee-Zustand war damit nicht bei jedem Heapzustand reproduzierbar.

## 52. User Visual Approval

**PASS – Nutzerabnahme am 2026-09-16.** Auf die abschließenden Fragen zu Himmel, Farben, Metin2-Charakter, dezentem Bloom, Wasser und Gesamtbild antwortete der Nutzer: **„sehr gut alles“**. Damit sind alle sechs Punkte der finalen Sichtprüfung bestätigt.

Nach allen Gates wurde `build-g8x/manual-acceptance/Metin2_Release.exe` sichtbar gestartet: PID 38460, Fenstertitel `METIN2`, reagierend, Modern High, finale Binary und originale Login-/Spielskripte. Der Runtime-Ordner ist unabhängig von den Produktionsconfigs. Startnachweis: `build-g8x/evidence/manual-client.txt`.

Der Client erreichte danach nachweislich die Spielwelt. Sein laufendes Game-Log enthält einmal `invalid idx 0` aus `CGuildMarkManager`; die Renderer-Failure-Datei enthält keine Fehlermeldung. Dieser Guild-Mark-Hinweis wird separat vom automatischen G8-Renderer-Gate geführt. Der laufende manuelle Client hat naturgemäß noch keinen Exit-/Shutdown-Nachweis.

Die Zustimmung stammt aus der Nutzerantwort; automatisierte Aufnahmen und Tests wurden getrennt davon bewertet.

## 53. Git Diff

Produktiv geändert: Atmosphere/Cloud-Konfiguration und Shader, gemeinsamer Sky-/Water-Horizont, Modern-Low-Schatten, vorhandenes Environment-Cloud-Signal sowie Initialisierung der Schneepartikelbasis. Diagnostik/Tests: feste Sky-Zeit, GPU-/Settings-Regressionen, reproduzierbare native Galerie-/Effektläufe, Pack-/Texture-Impact-Auswertung und auswählbares Buildverzeichnis im Dependency-Audit.

Keine Assets-/Serveränderungen, kein Staging, kein Commit, kein Push, kein Branch-/History-Umbau. `build-g8x` samt Bildern, Logs, Binaries und Profilingdaten ist ignoriert. Der Abschlussbericht und reproduzierbare Werkzeuge sind ungestagte Source-Änderungen.

## 54. Known Limitations

- Finale subjektive Sichtprüfung bestätigt. Eine vollständige automatisierte Ingame-UI-Prüfung und der Netzwerk-Relog sind weiterhin nicht nachgewiesen; die Nutzerabnahme wird nicht als Ersatz für diese Tests ausgegeben.
- 14 schon in G7 vorhandene Property-Meldungen in der Haupt-/Schneemapserie; See-/Performance-Serie separat fehlerfrei.
- 2D-Wolken statt volumetrischer Bewölkung; Wolken bewegen sich in sichtbarem Himmel/Wasser, PBR-IBL bleibt niederfrequent wolkenfrei.
- Audit-Lücken und nicht klassifizierte Texturen gemäß Abschnitt 34; keine Aussage über tatsächliche Bildschirmfläche.
- Performance ist ein kurzer Sanity-Lauf auf einem System. First-use-Kompilierung wird ausgewiesen; kein neues P-X- oder Framerate-Projekt.
- Ein früherer privater Normalclient aus einem unvollständig gepackten Testversuch blieb ohne Welt-Frameprotokoll aktiv (PID 16452). Windows verweigerte Beenden und Fensternachrichten mit Zugriff verweigert. Dieser Start ist kein bestandener Test; die endgültigen Aufnahmen stammen aus neuen, korrekt gepackten Verzeichnissen. Die Performance-Messung ist deshalb auch hinsichtlich Hintergrundprozessen keine vollständig isolierte Messung.
- Alte Assets können sichtbare Alpha-/Detailgrenzen zeigen. Keine H2-/Content-TX-Arbeit in G8.

## 55. GO/NO-GO

**G8-X = GO.** Die automatischen Gates und endgültigen nativen Serien sind grün; die finale Sichtprüfung wurde vom Nutzer am 2026-09-16 mit „sehr gut alles“ bestätigt. Die dokumentierten Mess- und Testgrenzen bleiben bestehen. G8 ist abgeschlossen; keine Folgephase wird begonnen.

## 56. Recommendation H2-X

Nach dem nun abgeschlossenen und bestätigten G8 kann ein eigener H2-Auftrag Bark/Leaf-Atlasqualität, Kronenlesbarkeit und Alpha-Silhouetten priorisieren. Die Texturreferenzen liefern dafür Kandidaten. H2, UI, Performance Finish und World Editor wurden nicht begonnen. Die Arbeit stoppt mit G8.

## Reproduktion

Alle Laufzeitverzeichnisse sind private Kopien mit neu gepackter `root.pck`; Produktions-Packs werden nur gelesen. Pro neuer Serie:

```powershell
tests/Graphics/prepare_runtime.ps1 -Name g8-example -BuildDirectory build-g8x/msvc -OutputDirectory "$PWD/build-g8x/example" -Manual
python tests/Graphics/prepare_g8_visual.py build-g8x/example final
build-g8x/msvc/bin/Release/PackMaker.exe --input build-g8x/example/test-root/root --output build-g8x/example/pack
tests/Graphics/run_g8_visual.ps1 -RuntimeDirectory build-g8x/example -BaselineErrorLog build-g8x/runtime-baseline-v2/log/syserr.txt
ctest --test-dir build-g8x/msvc -C Release --tests-from-file "$PWD/tests/fast-tests.txt" --output-on-failure
ctest --test-dir build-g8x/msvc -C Debug --tests-from-file "$PWD/tests/fast-tests.txt" --output-on-failure
ctest --test-dir build-g8x/portable --tests-from-file "$PWD/tests/fast-tests.txt" --output-on-failure
python tools/Vegetation/texture_impact.py build-g8x/evidence/texture-pack-audit.json test-data/vegetation build-g8x/texture-audit
python tests/Graphics/summarize_g8.py build-g8x
```

Weitere Fixture-Modi: `integration`, `effects`, `performance`. Pack-Audit-Eingabe wird mit `VegetationPackAudit ../m2dev-client pack-order.txt output.json --texture-impact` aus der Produktions-Packreihenfolge erzeugt; das Werkzeug bleibt offline. `--renderer-diagnostics` ist ausschließlich für die Testläufe erforderlich.
