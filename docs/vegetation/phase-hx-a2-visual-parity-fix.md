# H-X Gate A2 – Blattdarstellung / Visual parity fix

Stand: 15.09.2026. **A2: GO – Benutzer-Sichtprüfung mit „passt“ bestätigt. Gate B: NOT STARTED.** Kein Commit/Push.

Die erneute Sichtprüfung des A2-Fixes wurde vom Benutzer am 15.09.2026 mit „passt“ bestätigt. Diese Rückmeldung dokumentiert die visuelle Abnahme; zusätzliche, nicht einzeln bestätigte Bedienungsprüfungen werden daraus nicht abgeleitet. Gate B wird separat beauftragt.

## Ursache und Korrektur

Der gemeldete Fehler wurde an den Originalbäumen bei den A1-Brücken reproduziert: Beim Kameraschwenk wurden die Blätter einfarbig hell, während Stamm, Fronds und die ausgeschnittene Alpha-Silhouette erhalten blieben. Dieselben Kameras blieben unter SpeedTree Reference korrekt.

`CMapManager::BeginEnvironment` behandelt Dichtenebel unterschiedlich: Branch/Frond/Billboard verwenden exponentielle Dämpfung; die ursprünglichen SpeedTree-Blätter erhalten dagegen über `CSpeedTreeForest::SetFog` eine lineare Shader-Nebelspanne **0 bis 2.3 / density**. Die allgemeinen Zustände `FogStart` und `FogEnd` werden im Dichtenebelzweig nicht gesetzt. Der ZiiNAN-Blattpfad benutzte trotzdem diese unpassenden, gegebenenfalls nullgleichen Werte für seine lineare Blatt-Nebelberechnung. Dadurch wurde die Texturfarbe durch die Nebelfarbe ersetzt. Je nach Blickwinkel verdeckten Fronds die fehlerhaften Blätter teilweise, wodurch der Fehler zunächst nur bei bestimmten Kameras auffiel.

Die einzige produktive A2-Korrektur liegt in `src/Vegetation/VegetationRenderer.cpp`: Für Leaves bei `TerrainFog::Exp` wird dieselbe Blatt-Nebelspanne wie bei SpeedTree gesetzt. Bei Dichte 0 bleibt die Dämpfung aus. Kein anderer Renderer, Shader, Decoder, Exporter oder Materialzustand wurde für A2 geändert. Die zwischenzeitliche Diagnose in `WorldTree.cpp` wurde vollständig entfernt.

## Vergleich der 14 Zustände

Referenz: `SpeedTreeWrapper`, `SpeedTreeForestRenderer`, `TreeRenderBridge`, `DiligentTreeRenderer`. ZiiNAN: `WorldTree`, `VegetationRenderer`, vorhandener Mesh-Renderer. Der neue Bildtest lädt ZiiNAN-Texturen über `CResourceManager` / `CGraphicImage::GetAssetTexture`; Referenzpfade stammen unabhängig aus `CSpeedTreeRT::GetTextures`.

| Nr. | Zustand | SpeedTree Reference | ZiiNAN / Ergebnis |
|---|---|---|---|
| 1 | Leaf/Frond-Texturpfad | SPT-Verzeichnis + SDK-Composite-Datei, Erweiterung `.dds` | Identischer, kanonischer externer GLB-URI; unabhängig gegen SDK geprüft. B1/B3-Bäume verwenden `compositemapb1.dds` / `compositemapb3.dds`, Fern/Palme `compositemapn2.dds`. |
| 2 | Format / Alpha | Diese Atlanten sind DDS DXT3 / BC2 mit Alpha von 0 bis 255 | Identische Farb-/Alpha-Bytes und Zeilenabstände in sämtlichen Mipmaps; kein Alpha-Verlust beim Cache-Upload. B1/B3 je 6 Mips, N2 5. |
| 3 | Alpha-Test | Aktiv, `GREATER` | Aktiv, `StaticObjectAlphaTest::Greater`; Mask-Darstellung bleibt erhalten. |
| 4 | Alpha-Referenz | Teil-/LOD-abhängiger SDK-Wert, als 8-Bit-Wert verglichen | ZVEG bewahrt eigene Alpha-Werte für Branch, Frond, beide Leaf-Slots und Billboard. GLB-Grundwert 84/255 wird beim Draw durch den gewählten LOD-Wert ersetzt; kein gemeinsamer pauschaler Leaf/Frond-Cutoff. |
| 5 | Blend-Modus | Normale Vegetation ohne Alpha-Blend; Kamera-Verdeckung mit Blend | Gleiches Verhalten. Leaf-Alpha wurde nicht auf Opaque oder allgemeines Blend umgestellt. |
| 6 | Src/Dst Blend | SRC_ALPHA / INV_SRC_ALPHA, ADD | Identisches vorhandenes Blend-Paar beim Kamera-Maskentest; keine Premultiplikation eingeführt. |
| 7 | Depth write | Normaler Baum-/Kamerablockerpfad schreibt Tiefe | `depthWrite=true`, unverändert. |
| 8 | Depth test | Aktiv, LESS_EQUAL | Aktiv, LESS_EQUAL im verwendeten Mesh-Pipelinezustand; unverändert. |
| 9 | Culling / beidseitig | Branch CW; Fronds/Leaves/Billboard ohne Culling | Gleiches Teilverhalten. Die 118 GLBs enthalten jeweils `doubleSided=[false,true,true,true]`. |
| 10 | UV-Ausrichtung | `SetTextureFlip(true)` und ursprüngliche SDK-UVs | UV0 wird kopiert, ohne zusätzliche Bild-/UV-Spiegelung. Drehungen und versetzte Weltkoordinaten im Bildvergleich enthalten. |
| 11 | Texture stages / sampler | Stage 0: Texture × Diffuse für RGB und Alpha, trilinearer Filter. Branch/Frond können Stage-1-Selbstschatten verwenden; Leaves/Billboards verwenden diesen Schatten nicht. Referenz-Bridge erfasst geerbte Stage-1-Sampler. | Gleiche Textur- und Teilzuordnung. Native Sampler stehen auf Wrap / Linear-MIN-MAG-MIP; die Vergleichssampler sind explizit gleich eingestellt. Kamera-Maske multipliziert Alpha; Leaf-Maskenkoordinate bleibt wie im Legacy-Shader (0,0). Keine Änderung an Samplern für A2; keine pauschale Behauptung, dass jeder geerbte Welt-Samplerzustand separat protokolliert wurde. |
| 12 | Vertex alpha/color | Gepackte statische SDK-Farbe, Texture × Vertex-Alpha | `COLOR_0` erhält RGBA einschließlich Alpha; BGRA-Auslegung wird beim Export berücksichtigt. Keine Materialfarbe ersetzt die Vertexfarbe. |
| 13 | Fog / Materialfarbe | Leaves: eigene lineare Shader-Spanne, auch bei Dichtenebel; andere Teile: jeweiliger Weltnebel | **Fehler gefunden und korrigiert:** Blattspanne bei Dichtenebel nun 0 .. 2.3/density. Das vermeintlich weiße Blattmaterial war die Nebelfarbe. RGB-/Alpha-Texturen blieben korrekt. |
| 14 | Billboard-Zustand | Composite-Atlas, eigener Alpha-Wert, kein Culling, normaler Weltnebel | Unverändert; Nah/Mittel/Fern/Mittel/Nah inklusive Billboard mehrfach gegen SDK geprüft. |

`material-alpha-audit.json` bestätigt für **118 GLBs / 472 Materialien** MASK, den Grund-Cutoff und die Beidseitigkeit. GLB/ZVEG enthalten externe DDS-Verweise; es wurden keine Texturbytes konvertiert oder eingebettet. Die Atlasdaten enthalten vollständig transparente Texel. Das beobachtete Problem stammt weder von verlorenen Legacy-Alpha-Flags noch von einer TGA/DDS- oder Premultiplied-Alpha-Umwandlung. Separate TGA-Renderfälle waren hier nicht nötig, da beide Pfade für diese Referenzbäume tatsächlich DDS laden.

## Automatisierte Nachweise

- Release-Client und betroffene Debug-Testziele gebaut. Bekannte Linkerwarnungen bleiben vorhanden, insbesondere LNK4099; kein Anspruch auf einen warnungsfreien Build.
- Neuer Vorher-Test: **FAIL / Exit 1**, 850 Fälle, darunter 170 Dichtenebel-Fälle. 100 davon überschritten bereits die alten Bildschwellen. Maximaler mittlerer RGB-Fehler 31.9275/255, maximale deutlich abweichende Pixelfläche 20.1613%.
- Nachher **Release PASS 850/850 und Debug PASS 850/850**, identische TSV-Dateien. 34 reale Baumvarianten × 5 Kamera-/Umgebungsfälle × Nah/Mittel/Fern/Mittel/Nah. Enthalten sind Nebel aus, linearer Nebel, Kamera-Alpha/Blend, Weltkoordinaten mit Kameradrehung, Dichtenebel mit absichtlich ungesetzten allgemeinen Start-/Endwerten.
- Neue strengere Schwellen: IoU > 0.99, mittlerer RGB-Fehler < 0.25/255, Anteil der Pixel mit Kanalabweichung >12 kleiner 0.5%. Gemessen: minimale IoU **0.999598**, maximaler RGB-Fehler **0.0909175/255**, maximaler Anteil **0.3852%**. Kleine Float-/Rasterunterschiede bei großen Weltkoordinaten bleiben messbar.
- 15 unterschiedliche Texturen: **Format, Dimensionen, alle Mipmaps und sämtliche Farb-/Alpha-Bytes identisch** zwischen direktem Referenz-Dekoder und Produktions-Cache. Wind-Determinismus, Resize/Minimize/Restore und GPU-/Vegetationsressourcen 0 wurden im Bildtest erneut geprüft.
- Kurze Vertrags-/Boundary-/ConverterFailure-Gates: **Release 3/3, Debug 3/3 PASS**. Keine lange Stress-/Fuzz-Suite und keine erneute komplette Renderer-Suite.

## Originalkarte A1, gleiche Kameras

`tests/Vegetation/a2_bridge_entry.py` verwendet feste Positionen der nächsten Bäume an allen neun vorhandenen A1-Brücken, jeweils 1600/4800 Abstand und 0/135 Grad Drehung. Die originale Map, Properties und Texturen werden geladen. Der Kartenlauf verwendet reale Zeit/Wind und reguläre LOD-Auswahl; er ist kein Anspruch auf bitgleiche vollständige Weltframes zwischen beiden Runtimes. Die kontrollierten SDK-Bildtests oben vergleichen dagegen identische LOD-Zustände bei deaktiviertem Wind.

**36 Vorher-, 36 Reference- und 36 korrigierte Aufnahmen.** Die korrigierte Kontaktübersicht und ausgewählte Aufnahmen wurden visuell geprüft: Keine einfarbig weißen/schwarzen Blattquads an diesen Kameras; Alpha-Ausschnitte und Texturfarbe sind erhalten. Die massiven weißen Kronen sind in den Vorher-Aufnahmen klar sichtbar und in den korrespondierenden Referenz-/Nachherbildern beseitigt. Ein davon unabhängiger schwarzer Fehler wurde nicht gesondert reproduziert; die zunächst offene Benutzerabnahme wurde anschließend mit „passt“ bestätigt.

Korrigierter Kartenlauf: **Exit 0, 24.9 s, 490 erzeugte Bauminstanzen, 35.807 Draws, 335 LOD-Wechsel**. Branches 8.505, Fronds 7.840, Leaves 12.180, Billboards 7.282. VegetationFailures=0, VegetationReferenceEntries=0. Alle Vegetations-, Mesh-/Asset-, CPU-Source- und Kollisionsressourcen am Ende 0. `log/syserr.txt` leer. Referenzlauf ebenfalls Exit 0 und Ressourcen 0.

Die Login-/Charakterauswahl-/Ingame-Bedienung und die persönliche Sichtprüfung an der vom Benutzer gemeinten Brücke wurden nicht durch eine automatische Benutzerfreigabe ersetzt. Kein Gate B begonnen.

## Artefakte der bestätigten A2-Prüfung

Alle Pfade relativ zum Source-Checkout:

- `build/hx/a2/a1-bridge-before-reference-fixed.png`: gleicher Baum / gleiche Kamera, Vorher – Reference – Nachher.
- `build/hx/a2/a1-bridge-fixed-contact.jpg`: alle 36 korrigierten A1-Kameras.
- `build/hx/a2/comparison.html`: alle 36 Dreiervergleiche mit Originalbildern.
- `build/hx/a2/visual-Release/visual-parity.tsv`, `visual-Debug/visual-parity.tsv`, `visual-Release/texture-parity.tsv`.
- `build/hx/a2/density-before/visual-parity.tsv`, `density-before.log`: absichtlich fehlgeschlagener Regressionstest vor der Korrektur.
- `build/hx/a2/bridge-before`, `bridge-reference`, `bridge-fixed`: Original-Screenshots, Kamera- und Ressourcenprotokolle.
- `build/hx/a2/material-alpha-audit.json`, `bridge-inventory.json`: Material-/Alpha- und Platzierungsnachweise.
- **Neue fertige manuelle Testkopie:** `build/hx/a2-test-client/start-ziinan.cmd` und `start-reference.cmd`. Kein Build erforderlich. Die alte Testkopie bleibt als Vorher-Stand erhalten.

Release-SHA256: `59912953700807AFF4098A14DD1F7876CCF3E51D9E733C5A78F0E23EE515957E`. 237 kopierte Vegetationsdateien per SHA256 bestätigt. Die ursprünglichen Packs sind nur verknüpft und dürfen in der Testkopie nicht bearbeitet werden.

Die erneute A2-Sichtprüfung wurde am 15.09.2026 mit „passt“ bestätigt. **A2: GO. Gate B: NOT STARTED; separater Auftrag erforderlich.**
