> Historischer G1/G2/G3/4-Bericht vom frueheren main. Die damalige eigene Renderer-Implementierung wurde durch den [DiligentFX-Neuaufbau](phase-gdx-diligentfx-rebuild.md) und G8/H2/P0 ersetzt. Dieser Bericht beschreibt keine aktuelle Implementierung oder erneute Abnahme. Bei MAIN-INTEGRATION-X als Historie erhalten.

# G1-X — Material System 2.0 / PBR Foundation

Datum: 2026-09-15. Windows x64, Diligent D3D11, natives GR2 und GLB; GCC/LP64 für den neutralen Kern.

**G1-X: GO. Release 66/66, Debug 66/66, GCC/LP64 38/38; separate manuelle G1-Abnahme bestätigt und Shutdown-Ressourcen 0.** STOP nach G1; G2 wurde nicht begonnen. Kein Stage, Commit oder Push.

[Visual Proof mit Originalbildern](../../build/g1x/visual-proof.html) · [Source-Diff](../../build/g1x/source.patch) · [G1-Menü-Diff](../../build/g1x/client-ui-g1-only.patch) · [Laufzeitzähler](../../build/g1x/runtime-summary.json).

Source-Baseline: `ca63fd1` (G0-X). Die G0-UI im benachbarten Asset-Checkout war bereits uncommitted; G1 ergänzt dort nur den funktionierenden Grafikstil.
Lokale Build-, Laufzeit- und Bildbelege liegen unter `build/g1x/` sowie den jeweiligen CTest-Arbeitsverzeichnissen. Sie werden nicht gestagt.

## 1. Material Baseline Audit

Vor den Produktionsänderungen: Release-G0-Binary gesichert; `AssetRuntime.GlTFRender`, `AssetRuntime.GlTFCharacterRender`, `AssetRuntime.GR2Render` bestanden (3/3, 11,80 s).
Zwölf frisch erzeugte Bilder bilden die Vergleichsbasis: drei Market-Stall-Blend-Kameras und neun Character-Bilder. Ältere statische Market-Stall-Bilder wurden **nicht** als frische Baseline gewertet.

| Source | Materialtyp / Texturslots | Alpha / Blend | Cull / Depth | Shaderpfad | Modernisierbar in G1? |
|---|---|---|---|---|---|
| GR2-Provider | DiffuseColor, optional Opacity, Materialgruppen | ursprünglicher Cutout-/Blend-Zustand | ursprüngliche Zweiseitigkeit / Passzustand | Materialpalette → Static/Actor | Ja, neutrale Defaults und Sidecar |
| GLB-Provider | BaseColor-Faktor/-Map, eingebettetes PNG/JPEG oder kanonischer Packpfad | OPAQUE/MASK/BLEND | doubleSided / Blend ohne Z-Schreiben | gleiche Materialpalette | Ja, Metallic/Roughness |
| Statische PNT-Weltmodelle | Diffuse, optional Kameramaske | nativer Opaque-/Blocker-Pass | aufgenommener Zustand | DiligentStaticObjectRenderer | Ja |
| Spieler, Mob, NPC, Reittier | Diffuse, alte Sphere-Map bei Specular | native Actor-Stages | native Mesh-/Passzustände | DiligentActorRenderer → gemeinsamer Meshshader | Ja, GPU-Skinning |
| Haar und Waffen/Attachments | gleiches Palettensystem, getrennte Materialgruppen | bestehende Cutout-/Blend-Stages | zugehöriger Zustand | gleicher rigid/skinned Renderer | Ja |
| Vegetation | Diffuse, Vertexfarbe, UV1, Wind-/Card-Kanäle | Branch/Frond/Leaf/Card-Semantik | partabhängig | Auxiliary-Vertexshader | Bleibt Legacy |
| Dungeon/PWNT | zwei Legacy-Texturstufen | bestehender Welt-/Effect-Pass | bestehend | WorldRenderer | Bleibt Legacy |
| Terrain/Wasser/Himmel | eigene Layer-/Stage-Verträge | bestehend | bestehend | separate World/Terrain-Pfade | Außerhalb G1 |
| Effekte/Partikel/UI | Textur, Vertexfarbe, ggf. zweite Stufe | eigene Blend-/Sortierregeln | bestehend | separate Effect/UI-Pfade | Außerhalb G1 |

Materialgruppen und Indexbereiche bleiben erhalten. Die bisherige Diffuse-/Sphere-Auflösung wurde für PNT-Materialien in einmalig erzeugte Runtime-Handles verlegt. Kameramasken verwenden Objektidentitäten statt Pfadstrings im Cache. Unveränderte separate Dungeon-/Effect-Pfade sind keine PBR-Pfade.

## 2. MaterialAsset 2.0

`src/AssetRuntime/MaterialData.h` definiert neutrale Faktoren, fünf Maps, UV-Transformationen, Alpha und Zweiseitigkeit. `MaterialAsset` erhält `model` und `pbr`; seine bisherigen flachen Legacy-Felder bleiben unverändert erhalten. Dadurch müssen vorhandene Consumer nicht auf eine neue Legacy-Struktur umgestellt werden.

## 3. Material Models

`Legacy` und `PBRMetallicRoughness`. Unbekannte neutrale Modelle erhalten sichere Legacy-Defaults. Optionales glTF-Unlit/Specular-Glossiness bleibt bei der vorhandenen Basisfarben-Approximation; erforderliche nicht unterstützte Erweiterungen werden diagnostisch abgelehnt.

## 4. Legacy Compatibility

Modern-Fallback: alte Diffuse als BaseColor, Metallic 0, Roughness 0,8, geometrische Normale, AO 1, Emissive 0. Fehlende Zusatzmaps erfordern keine Asset-Migration. Ein Sidecar ändert weder Legacy-Texturidentität noch Geometrie, Alpha oder Cull-Zustand.

## 5. Classic Path

Classic verwendet weiterhin die bisherigen Shader und die lineare UNORM-Ansicht der bestehenden Texturbytes. Faktoren, Texturstufen, Fog, Alpha, Blend, Cull, Skinning und Kamera-Blocker bleiben im Legacy-Pfad. Die zusätzlichen PBR-Daten beeinflussen diesen Shader nicht.

## 6. Modern Path

Der gemeinsame PNT-Renderer wählt über die aufgelöste G0-Konfiguration den PBR-Shader. Dieselben neutralen Materialzustände dienen starren und animierten Modellen; der Shader kennt weder GR2 noch cgltf. Auxiliary-/Vegetation- und andere getrennte Weltpfade bleiben unverändert.

## 7. glTF PBR Mapping

Übernommen werden BaseColor-Faktor/-Map, Metallic, Roughness, gemeinsame MetallicRoughness-Map, Normal inklusive Scale, Occlusion inklusive Strength, Emissive-Faktor/-Map, AlphaMode/Cutoff und doubleSided. Auch ohne expliziten PBR-Block gelten die glTF-Defaults Metallic=1/Roughness=1; ohne zugewiesenes Material ebenso. Classic behält seine bisherige Approximation.

Maps unterstützen UV0 und je eine affine `KHR_texture_transform`-Abbildung. Die bisher für Classic gebackene Base-UV bleibt erhalten; Modern erhält zusätzlich die ursprüngliche UV0. Andere UV-Sets werden mit einer konkreten Unsupported-Diagnose abgelehnt.

## 8. Metallic/Roughness Packing

G multipliziert Roughness, B multipliziert Metallic. Ein GPU-Pixeltest mit R=32/G=64/B=192 erwartet Roughness 64 und Metallic 192; beide bleiben lineare Daten.

## 9. AO

R ist die Occlusion-Komponente. Strength interpoliert zwischen 1 und dem Texel. Der GPU-Test erwartet R=32; direkte Beleuchtung bleibt mit und ohne AO pixelgleich. Nur der vorhandene Ambient-Anteil wird abgedunkelt.

## 10. Normal Mapping

Normalmaps werden linear gelesen, nach −1…1 dekodiert und mit Scale auf XY angewendet. Endliche negative und große Scales bleiben zulässig; eine gemeinsame Skalierung vor der Normalisierung verhindert Überlauf. Ungültige quantisierte Nullvektoren fallen auf die geometrische Normale zurück. Tangente, geometrische Normale und Handedness ergeben die Basis; Rückseiten werden berücksichtigt. Grundlage: [glTF Normal-Scale-Schema](https://raw.githubusercontent.com/KhronosGroup/glTF/main/specification/2.0/schema/material.normalTextureInfo.schema.json).

## 11. Tangents

Vorhandene glTF-Tangenten werden samt W-Handedness und Koordinatentransformation übernommen. Ein paralleler 24-Byte-Stream enthält Tangente/Handedness und ursprüngliche UV0; bestehende PNT-/Skinning-Layouts bleiben intakt. GPU-Skinning transformiert auch die Tangente.

Ohne Tangenten nutzt eine tatsächlich vorhandene Normalmap eine begrenzte Ableitungsbasis im Shader. Degenerierte UVs fallen auf die geometrische Normale zurück; gespiegelte UVs erhalten das passende Vorzeichen. Bevorzugter Produktionsweg bleibt die vorhandene Offline-Option `--generate-tangents`; deren Erzeugungs-/Orthogonalisierungstests werden mitgeprüft. Keine Startzeit-Tangentengenerierung.

## 12. Color Space

BaseColor und Emissive: sRGB. Normal, Metallic/Roughness und AO: linear. Faktoren sind lineare Materialwerte. Der Pixeltest mit einem 128er Farbtexel prüft Dekodierung und anschließende Ausgabe zurück auf 128; Datenkanäle werden dagegen direkt gelesen.

## 13. Texture Views

RGBA8/BGRA8/BGRX8 und BC1/2/3 nutzen typeless GPU-Speicher mit je linearer und sRGB-SRV. Eine Textur wird dafür nicht doppelt angelegt. Der Test verwendet dieselbe Textur in verschiedenen Semantiken und prüft die Zahl der GPU-Texturallokationen. Für B5G5R5A1 bleibt eine lineare View mit expliziter Shader-Farbdekodierung.

## 14. PBR Shader

`src/Renderer/PBRShader.h` enthält den gemeinsamen Shader, `DiligentPBRMaterial.inl` die PSO-/Binding-Integration. Es entstehen weder ein neuer Rendergraph noch zusätzliche Vollbild-Passes.

## 15. BRDF

Direktes Licht: Cook-Torrance mit GGX-Verteilung, Smith-GGX-Geometriefaktor und Schlick-Fresnel. Die diffuse Komponente wird mit `(1-F)*(1-metallic)` gewichtet; Metalle erhalten keine zusätzliche diffuse Energie. Die vorhandene Ambient-Approximation ist ausdrücklich kein physikalisches IBL.

Primärreferenzen: [glTF 2.0 Materialvertrag](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html), [Filament: physikalische Materialgrundlagen](https://google.github.io/filament/main/filament.html).

## 16. Current Light Integration

Der moderne Shader nutzt vorhandene gerichtete Lichtfarbe/-richtung und Ambient im Eye-Space. Keine neue Light-Verwaltung. Alte zusätzliche Punkt-/Spot-Lichtsemantik wird im Classic-Pfad bewahrt; ihr modernes Lighting gehört zu G2.

## 17. IBL Decision

Keine Cubemap, Irradiance-Convolution, Prefilter-Environment oder BRDF-LUT. Das Materialmodell bleibt unabhängig von späteren Lichtquellen.

## 18. Roughness

Validierung auf 0,045…1; nach Texture-Multiplikation erneut im Shader abgesichert. Die Untergrenze verhindert numerische Singularitäten. Legacy-Default 0,8.

## 19. Metallic

Validierung auf 0…1. Legacy-Default 0; glTF-Default 1. NaN/ungültige neutrale Daten erhalten sichere Defaults. Der Provider-Test prüft zusätzlich einen außerhalb des Intervalls liegenden glTF-Faktor.

## 20. Emissive

Linearer Faktor × sRGB-dekodierte Map; zum linearen Materialergebnis addiert. Ausgabe in den vorhandenen LDR-Render-Target, mit Begrenzung auf den darstellbaren Bereich und sRGB-Kodierung. Kein Bloom/HDR.

## 21. Alpha

OPAQUE/MASK/BLEND verwenden die bestehenden Zustands- und Sortierpfade. Alpha-Faktoren werden genau einmal angewendet. OPAQUE ignoriert Texturalpha; MASK verwendet den bisherigen quantisierten 8-Bit-Vergleich; BLEND bleibt SourceAlpha/InverseSourceAlpha. GPU-Prüfungen laufen für beide Stile. Keine OIT-Lösung; die bestehenden Transparenzgrenzen bleiben.

## 22. Double Sided

doubleSided wird über den vorhandenen Cull-State umgesetzt. Drei Cull-Zustände bleiben getrennt; kein globales Cull-Off. Ein GPU-Test prüft entgegengesetzte Cull-Richtungen und beidseitiges Zeichnen.

## 23. PSO Cache

Pro gemeinsamem Meshrenderer maximal 24 PBR-PSOs: rigid/skinned × drei Cull-Zustände × Blend × DepthWrite. Sie werden einmal initialisiert. Alpha-Test und Map-Präsenz sind Konstanten; die SRBs werden pro Material und benötigter Pipeline einmal gecacht. Abgelaufene Materialeinträge werden entfernt; wiederverwendete Speicheradressen können keine alten Bindungen übernehmen.

## 24. Shader Variants

Ein Pixelshader, zwei Vertex-Einstiegspunkte. Map-Bits vermeiden logische Samples für fehlende optionale Kanäle. Keine 2⁵-Map-Varianten. G1 nutzt einen gemeinsamen linearen Repeat-Sampler für moderne Materialmaps; glTF-Samplerwahl/anisotrope Materialprofile sind noch nicht erweitert.

## 25. Rigid

Materialkugeln, echtes Market-Stall-GLB, Welt-GR2 und starre Attachments verwenden die gemeinsame Produktionseinbindung. Der zusätzliche Stream verändert weder Indizes noch bestehende Vertexformate.

## 26. Skinned

GPU-Skinning-Paletten und Remaps bleiben bestehen. Der F5-Character erhält für den Proof Metallic/Roughness und eine Normalmap; Geometrie, Skin und sechs Clips bleiben gleich. Keine CPU-Deformation und kein Fallback.

## 27. Vegetation Status

Vegetation bleibt im vorhandenen Legacy-/Auxiliary-Pfad inklusive Vertexfarbe, Wind, Cards, Cutouts und LOD. MaterialAsset ist später nutzbar; in G1 keine PBR-Vegetationspflicht und kein SpeedTree.

## 28. GR2 Override Architecture

`CGraphicThing::OnLoad` fragt einmalig `<asset-path>.zmat` beim vorhandenen PackManager ab, vor Erzeugung der Materialpaletten. Die Anwendung erfolgt über `AssetDocument::ApplyMaterialOverrides`. Der Renderer sieht ausschließlich neutrale Runtime-Daten.

## 29. Override Format

Kleines textuelles `VERSION 1`, maximal 64 KiB und 256 Materialeinträge. Locale-unabhängige Zahlen, deterministische Serialisierung, quotierte Namen/Pfade; unbekannte Felder, Versionen, doppelte Identitäten und überschüssige Daten werden abgelehnt.

```text
VERSION 1
MATERIAL 0 0 "Material #bodsy"
METALLIC 0.75
ROUGHNESS 0.4
NORMAL_SCALE 0.75
MAP 1 "g1x/normal.png"
MAP 2 "g1x/metal_rough.png"
MAP 3 "g1x/occlusion.png"
```

Map-Indizes: 0 BaseColor, 1 Normal, 2 MetallicRoughness, 3 AO, 4 Emissive. Zusätzlich: BASE_COLOR RGBA, EMISSIVE RGB, OCCLUSION_STRENGTH. Pfade sind kanonische Pack-/Dateipfade, ohne Abhängigkeit vom globalen Legacy-Modellverzeichnis. Version 1 enthält keine separaten Alpha-/Cull-/UV-Overrides.

## 30. Override Resolution

Assetpfad über den Sidecar-Namen, danach Modellindex + Materialindex + exakter Materialname. Alle Identitäten werden vor der ersten Änderung validiert. Kein Pointer als persistente Identität. Fehlender Sidecar ist normal; fehlerhafte Metadaten behalten Legacy mit Diagnose bei.

## 31. Test Materials

`tests/Materials/generate_fixtures.py` erzeugt sechs kleine Materialkugeln und vier 8×8-Maps. A: raues Nichtmetall; B: glattes Metall; C: Normalmap; D: Roughness-Variation; E: Emissive; F: AO. Zusätzlich entsteht eine Materialvariante des vorhandenen F5-Charakters. Kein Produktions-Art-Asset wird ersetzt.

## 32. Real GLB Proof

`Materials.RealGLB` lädt das bestehende `market_stall.glb` durch ResourceManager → Provider → Modell/Palette → Produktionsrenderer. Drei Kamerabilder, Größenänderung und Freigabe werden geprüft. Keine spezielle Test-BRDF.

## 33. Legacy GR2 Proof

Die native Szene enthält Spieler, Mob/Boss/NPC, Gebäude, Waffen, Haar und Reittier. Ein separater Lauf ohne Sidecar prüft Classic und sichere Modern-Defaults. Der bestehende GR2-Gate testet außerdem Originalklassen und Attachments.

## 34. GR2 Override Proof

Der Proof wählt Original-Warrior-Novice, Modell 0, Material 0, exakten Namen `Material #bodsy`. Ein unabhängiger neutraler Test liest dieselben GR2-Bytes zweimal und wendet nur auf eine Materialbeschreibung Zusatzdaten an. Das private Client-Pack enthält allein den Sidecar und Zusatzmaps; SHA256 vor/nach der Vorbereitung belegt unverändertes Original-GR2.

## 35. Animated GLB Character Proof

`Materials.Character`: sechs Clips, 20 Instanzen, gemeinsame Mesh-/Texturressourcen, Hand-Attachment, numerische Palettenprüfung, Resize/Minimize/Restore und Null-Ressourcen nach Shutdown. Der native Client-Proof registriert nur in seiner privaten Root ein zusätzliches GLB-Test-Race.

## 36. Graphics Settings Integration

G0 bleibt die einzige Settingsquelle. `Graphics::Resolve` erzeugt `usePBR`; der Renderer liest diese aufgelöste Frame-Konfiguration. Presets bleiben bezüglich PBR vom gewählten Stil abhängig. Keine verteilten Benutzerflags und keine Fake-PBR-Einzeloption.

## 37. Classic/Modern Live Switch

Im bestehenden Grafikmenü ist „Grafikstil: Klassisch / Modern“ ergänzt. Auswahl → bestehender Apply-Pfad → nächste Frame-Konfiguration. Material- und Geometrieressourcen bleiben nutzbar; nur Shader/Bindung wechseln. Kein Neustart erforderlich.

## 38. Persistence

Stil wird über den G0-Speichervertrag gespeichert. Portabler Store-/Roundtrip-Test und zwei echte Client-Prozesse prüfen das Beibehalten der gesamten Grafik-Konfiguration.

## 39. Performance Sanity

Kurzer Vergleich derselben kleinen GPU-Testszene: zwölf Classic- und zwölf Modern-Frames. Gemessen wird CPU-Zeit um Begin/Draw/End; keine GPU-Zeitmessung oder Leistungszusage. PSO- und Material-Erzeugungszähler bleiben dabei konstant: 12 initialisierte rigid-PBR-PSOs, 15 Materialerzeugungen einschließlich der vorangehenden Fehler-/Kanaltests. Der Test fordert tatsächlich zwölf Draws je Stil.

Release: Classic 114 µs, Modern 7 µs für jeweils zwölf CPU-Submission-Frames; Debug: 334/162 µs. Diese winzige, bereits aufgewärmte Szene misst weder GPU-Kosten noch Spiel-FPS; daraus folgt keine allgemeine Geschwindigkeitsaussage.

`PBREnabledSamplesPerDrawSum` summiert aktivierte Map-Slots je Draw (inklusive optionaler Kameramaske). Das ist eine Diagnose der vorgesehenen Samples pro Fragment, **keine Messung tatsächlich ausgeführter GPU-Texture-Samples**. Ein materialseitiger Standard benötigt BaseColor; alle fünf Maps höchstens fünf plus Kameramaske.

## 40. Material Lifetime

Ein Material hält immutable Runtime-Zustände pro Uploader-Lebensdauer. Diese halten stabile GPU-Handles; die Adapter halten die zugrunde liegenden Bildreferenzen. PSO-Bindungen besitzen nur eine schwache Materialreferenz und werden nach Materialende entfernt. Der letzte Frame wird beim Renderer-Shutdown ausdrücklich abgeräumt, bevor die Zähler geprüft werden.

## 41. Error Handling

Tests behandeln fehlende Roughness-/Normalmap, beschädigte optionale PNG-Datei, ungültige Zahlen/Modi, ungültige Null-Normale, gespiegelte UVs und abgelehnte Sidecars. Optionale Maps fallen auf Faktoren/geometrische Normalen zurück. Strukturell ungültige Assets bzw. erforderliche unbekannte glTF-Erweiterungen werden diagnostisch abgelehnt; kein stiller Provider-Fallback.

## 42. Header Leak Test

Der vorhandene rekursive Public-Header-Gate umfasst `MaterialData.h`. Neutrale Daten enthalten Standard-C++-Typen; keine Windows-, D3D-, GR2-Reader- oder cgltf-Typen. Der portable Compiler baut Materialvalidierung, Serialisierung, Override-Auflösung und G0-Konfiguration.

## 43. Classic Visual Regression

Erster Nachvergleich: 12/12 frische Classic-Bilder bytegleich zur G0-Baseline, darunter Blend-Prop und GPU-Character. Der finale Vergleich bestätigt erneut **12/12 bytegleiche Bilder**: `build/g1x/classic-image-comparison.json`. GR2 und Vegetation ergänzen diesen begrenzten Golden durch echte Laufzeitszenen; keine Behauptung einer vollständigen Asset-Korpus-Bildabdeckung.

## 44. Modern Visual Proof

Produktionsbilder `g1x-balls-0.bmp` (Classic), `-1.bmp` (Modern), `-2…7.bmp` (BaseColor/Normal/Roughness/Metallic/AO/Emissive) unter `build-hx-clean/tests/Materials/`. Sichtbar geprüft: matte A-Kugel, schmaler Metallhighlight B, veränderte Normale C, Roughness-Variation D, farbige Emission E und Ambient-Occlusion F. Die exakten Kanaltests ergänzen diese Sichtprüfung. Die Galerie enthält zusätzlich reales GLB, animierten Character und native GR2-/Menüaufnahmen. Native animierte Screenshots dienen der Sichtprüfung, nicht einem pixelgenauen Zeitvergleich.

## 45. Release

**PASS: Build und 66/66, 44,85 s.** `build/g1x/release-verified-build.log`, `release-gate-verified.log`, `release-verified-details.log`. Danach nur eine zusätzliche Nicht-Bild-Fehlerfixture im Materialtest ergänzt: `Materials.Render` nochmals 1/1 PASS; keine Produktionsänderung.

Der unveränderte begrenzte Gate-Filter umfasst Graphics, Materials, AssetRuntime, AnimationRuntime, Vegetation, AssetTool, Platform sowie Renderer.StartupOptions/DiligentD3D11/ProductionGpu/NoLegacyArchitecture/HairLodQuick. Keine Fuzz-/Stress-Läufe.

## 46. Debug

**PASS: Build und 66/66, 130,23 s.** `build/g1x/debug-verified-build.log`, `debug-gate-verified.log`, `debug-verified-details.log`. Zusätzliche Nicht-Bild-Fehlerfixture danach ebenfalls 1/1 PASS. Ein erster Lauf erreichte 65/66; der PBR-Character lief in das Zeitlimit. Die isolierte Debug-Diagnose zeigte eine unzulässige Änderung der als mutable deklarierten Knochenpalette bei gemeinsamem Material mehrerer Figuren. Palette und Kameratextur/-sampler sind jetzt dynamic; Materialmaps bleiben einmalig gebunden. Der korrigierte Mehrinstanztest und der vollständige Gate sind grün; die endgültige Debug-D3D11-Prüfung meldet keine neuen Binding-Verstöße.

## 47. GCC/LP64

**PASS 38/38, 4,69 s.** Die Windows-only Test-Verknüpfung des neuen GR2-Override-Tests wurde durch die direkte portable GR2-Provider-Abhängigkeit ersetzt. `build/g1x/gcc-build-final.log`, `gcc-gate.log`.

## 48. GR2 Regression

Bestehender kurzer Original-GR2-Gate einschließlich Spieler, Mob, Waffen, Haar und Gebäude. Native A1/B1-Szene prüft zusätzlich reale Diffuse-Texturen, Kamerablocker und beide Materialstile.

## 49. GLB Regression

Bestehende statische/Blend-GLB-Gates sowie Market-Stall im modernen Materialpfad. Offline-Konvertierungs-Roundtrip bleibt im Gate.

## 50. Character Regression

Bestehender F5-Classic-Gate und zusätzlicher PBR-Gate. Idle/Walk/Run/Attack/Hit/Death, gemeinsame Animationsdaten und Hand-Attachment bleiben im selben Animations-/GPU-System.

## 51. Vegetation Regression

Vorhandene Vegetation-Core-/Rendergoldens und native A1/B1-Tree-Szene. Branches, Fronds, Leaves, Billboards und LOD-Aktivität werden anhand der Zähler geprüft. SpeedTree bleibt entfernt.

## 52. Runtime Smoke

Automatisierte private Client-Root: echte `CInstanceBase`/`CActorInstance`-Objekte, `app.RenderGame`, Grafikmenü, Bewegung/Kampf, beide Stile und Mapwechsel. Echter Server-Login, Character-Select und menschliche Bedienungs-/Sichtprüfung werden separat protokolliert; der Offline-Proof ersetzt sie nicht.

**Manuelle G1-Abnahme PASS, vom Nutzer am 2026-09-15 bestätigt:** „Ja, alles fehlerfrei geprüft und Client geschlossen“. Der normale Client unter `build/g1x/runtime/manual-g1` wurde mit echtem Login und Charakterauswahl geprüft: Klassisch ↔ Modern im Grafikmenü, Figuren/Gebäude, Bewegung/Kampf, Rückkehr zur Charakterauswahl und erneuter Spieleintritt sowie normales Schließen. Diese Bestätigung betrifft ausdrücklich G1 und ist unabhängig von der früheren G0-Abnahme. Der geprüfte Release-Client hat SHA256 `f9017a2fc65e35d4452c606a092ed38e9afca62658c5cd6445a80230dd07a3f2`.

## 53. Resize/Minimize

Der native G0-Testzugang lässt den Client sein eigenes Fenster in Classic und Modern vergrößern, minimieren, wiederherstellen und zur Originalgröße zurückkehren. Zusätzlich prüft der Character-GPU-Test das gerenderte Bild nach Restore.

## 54. Map Change

Kurze Folge A1 → B1 → A1; native Actor-/Map-Objekte werden neu geladen. Abgelaufene Materialbindungen werden beim Frame-Reset entfernt. Ein anschließender echter Prozessneustart prüft Persistenz und Neuaufbau. Beide Durchläufe (ohne und mit Sidecar) bestanden; insgesamt vier normale Prozessenden.

Zusätzlich wurde der echte Relog über die Charakterauswahl im manuellen G1-Lauf vom Nutzer als fehlerfrei bestätigt.

## 55. Shutdown

**Viermal Exitcode 0**, jede Sequenz vollständig: 12 Bilder/Schritte im Hauptlauf, 2 beim Neustart. Je acht Renderer-Shutdownzeilen mit Nullwerten. MaterialRuntimeObjects, PBRBindings, PBRPipelines, SourceTextures/Buffers, GR2-/GLB-/Animations-/Vegetations- und Collision-Ressourcen: 0. CPU-Deformation, GPU-Fallback und Vegetationsfehler: 0. Beide finalen nativen Fehlerlogs sind leer.

| Lauf | Hauptlauf | Neustart | Sidecar |
|---|---:|---:|---|
| `build/g1x/runtime/verified-defaults` | 29,36 s | 8,54 s | fehlt erwartungsgemäß, MaterialOverrides=0 |
| `build/g1x/runtime/verified-override` | 29,45 s | 9,46 s | MaterialOverrides=1 |

Der zweite Hauptlauf protokolliert 21.600 PBR-Draws, 54 erstellte Material-Runtime-Zustände und 60.694 Vegetations-Draws. Shader/PSO/Map-Bindungen entstehen nicht erneut pro Frame. Die beobachteten Aktivitätszahlen sind kein Performance-Benchmark.

**Auch der separate manuelle Client endete mit Exitcode 0** (`manual-g1/manual-exit.txt`), damit fünf normale Prozessenden. Sein `source-resource-audit.log` meldet alle erfassten lebenden Ressourcen einschließlich MaterialRuntimeObjects/PBRBindings/PBRPipelines, SourceTextures/Buffers, GR2, Animation, Vegetation und Collision mit 0; alle acht Renderer-Shutdownzeilen sind ebenfalls 0. CPU-Deformation, GPU-Fallback, SkinPreparationFailures, NativePrewarmFailures und AnimationRuntimeFailures: 0. Die Aktivitätszähler belegen 335.031 PBR-Draws, 898 native GR2-Lesevorgänge und 370.830 GPU-Skinning-Frames. Das manuelle Fehlerlog ist nicht leer: vorhandenes `invalid idx 0` sowie Damage-Diagnostik (`AddDamageEffect`/`ProcessDamage`). Die gezielte Prüfung fand keine PBR-, Optional-Map-, Override-, AssetRuntime-Fehler, Diligent-Fehler oder Python-Tracebacks.

## 56. Resources

Getrennte Nachweise: MaterialRuntimeObjects/PBRBindings/PBRPipelines, SourceTextures/SourceBuffers, Geometry-/Texture-Zähler je Renderer, GR2-/GLB-Dokumente, Animation/Skinning, Vegetation und Collision. Aktivitätszähler wie PBRDraws/MaterialOverrides bleiben erwartungsgemäß positiv und werden nicht als Leaks bewertet.

## 57. Known Limitations

- PBR verwendet derzeit gerichtetes Licht plus vorhandene Ambient-Approximation; kein IBL, HDR, Bloom oder modernes lokales Licht.
- UV0, gemeinsamer linearer Repeat-Sampler; zusätzliche UV-Sets und Material-Samplerprofile benötigen spätere Erweiterung.
- Normaltangenten ohne Offline-Daten verwenden die begrenzte Ableitungsbasis; degenerierte UVs erhalten geometrische Normalen.
- Transparenz behält bestehende Sortierung/Blend-Grenzen; keine OIT, Transmission oder Specular/Glossiness.
- Separate Auxiliary-/Vegetation-, Dungeon-, Terrain-, Wasser-, Himmel- und Effect-Pfade bleiben Legacy.
- Bestehende Linker-/PDB-Warnungen sind keine warnungsfreie Build-Basis. Diagnostik und manuelle Abnahme werden getrennt ausgewiesen.

## 58. Git Diff

Quelländerungen: neutraler Materialvertrag und glTF-Mapping; GR2-Sidecar-Einstieg; Material-/GPU-Caches und paralleler Tangentenstream; gemeinsamer PBR-Shader; G0-Stilauflösung; Tests und dieser Bericht. Asset-Checkout: vorhandenes Grafikmenü um Stil ergänzt. Keine Produktions-Geometrie/-Textur geändert. Der Warrior-GR2-SHA256 bleibt `7AC881B0C60CED40C9388D6186BDF02A353A58382D12804345CB65FC3DC1FA74`. Unabhängige vorhandene Asset-/Config-/Log-Änderungen bleiben unberührt. `build/g1x/source.patch` enthält die Source-Änderungen einschließlich 21 neuer bewusster Dateien; `client-ui-g1-only.patch` isoliert G1 gegenüber der gesicherten G0-Menüdatei. `git diff --check` ist sauber. Kleine GLB-/PNG-Proof-Fixtures sind bewusst unter `tests/Materials/fixtures`; Builds, Logs, Shader-/Bildausgaben bleiben ungestagt im Buildbereich.

## 59. GO/NO-GO

**GO für G1-X.** Finale Release-/Debug-/GCC-Gates bestanden; Classic 12/12 bytegleich zur G0-Baseline; Modern-PBR mit GLB, animierter Figur und optionalem GR2-Sidecar nachgewiesen. Automatisierte native Läufe und der separate manuelle Login-/Darstellungs-/Relog-Test sind bestanden. Alle fünf Prozesse endeten normal mit Exitcode 0 und erfassten Shutdown-Ressourcen 0. Die oben dokumentierten Grenzen bleiben bestehen; keine offenen G1-Abnahmepunkte. STOP nach G1, kein Stage/Commit/Push und kein Beginn von G2.

## 60. Recommendation G2-X

Als separat zu beauftragender Folgeschritt kann G2 vorhandene neutrale Materialinputs mit einer gesondert geplanten Lichtarchitektur versorgen. G1 endet hier: keine begonnenen Shadows, SSAO/GTAO, IBL, HDR/Bloom oder Sky-/Water-Remaster-Arbeiten.
