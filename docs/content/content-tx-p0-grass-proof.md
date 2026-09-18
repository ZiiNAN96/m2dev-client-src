> Aktueller Stand 16.09.2026: Alle Bodentexturen sind wieder original; sämtliche Katalog-Ersetzungen sind deaktiviert. Großflächiges 3D-Gras bleibt im Release-Client abgeschaltet. [Rücksetzung und Prüfung](terrain-original-restoration.md). Der folgende Bericht ist historisch.

> Archiviert am 18.09.2026: vorhandene Berichte und Prüfaufnahmen wurden nach `evidence/` übernommen. Alte Testclients, Materialvorschauen und Buildausgaben wurden entfernt. [Vollständige erhaltene Textbelege](evidence/content-tx-p0.zip).

# CONTENT-TX-P0 – Grass-Terrain-Proof

Stand: 16.09.2026. **CONTENT-TX-P0 abgeschlossen: technisches GO und visuelles GO für die Grass-Art-Direction erteilt.** Kein Commit, kein Push, kein Deployment in den Originalclient. Eine Übernahme in den Produktionsclient bleibt ein separater Auftrag.

## Erweiterung: dunkelgrünes 2K-Gras auf allen passenden Maps

Auf den Folgeauftrag „kannst du es auf alle maps anwenden?“ wurden die tatsächlich geladenen TextureSets aller **85 vorhandenen Maps** geprüft. Der normale isolierte Testclient enthält jetzt **130 passende Grass-Layer in 50 Maps** mit derselben freigegebenen dunkelgrünen 2K-DDS. In 49 dieser Maps sind passende Layer tatsächlich in den Terrainmasken verwendet; eine weitere referenziert sie nur im TextureSet. Auf den übrigen Maps gibt es keine passenden reinen Grasflächen. Seegras, schneebedeckte Flächen, Gras mit eingebetteten Felsen und die irreführend als Gras benannte dunkle Dungeon-Erde bleiben erhalten.

Ein kurzer nativer Lauf bestand mit **neun Ansichten auf sieben repräsentativen Maps**, inklusive Classic-Kontrolle und Rückkehr zu Modern: Exit 0, keine Grass-Platzierungen, keine verbleibenden geprüften Ressourcen und keine Diligent ERROR/FATAL. **12.189 Originaldateien** sind SHA256-identisch; Source- und Binärhashes sind gegenüber dem bisherigen Proof unverändert. Nicht jede Map wurde visuell geprüft; eine manuelle Gesamtfreigabe wird daraus nicht abgeleitet. Kein neuer Build war für diese Asset-Zuordnung erforderlich.

Der laufende normale Testclient wurde während der anschließenden Texturrecherche nicht unterbrochen. **Bereits geladene Maps benötigen einen Neustart**, um alle neuen Dateien sicher zu laden. Der Original-/Produktionsclient bleibt unverändert. Kein Commit oder Push.

[Map- und Slotzuordnung](evidence/content-tx-p0/all-maps/Map-Zuordnung.md) · [Prüfnachweis](evidence/content-tx-p0/all-maps/evidence/result.json) · [Kartenansichten](evidence/content-tx-p0/all-maps/evidence/map-proof.jpg).

Betriebshinweis: Der verworfene erste Inventurstart in `all-maps/runtime-audit` blieb als fensterloser Prüfprozess (PID 37848, Start 16.09.2026 18:43:14) aktiv. Windows verweigerte das gezielte Beenden mit „Access denied“, auch im erhöhten Werkzeugaufruf. Keine Sicherheitsumgehung versucht. Die anschließend erfolgreich verwendeten Inventur- und Grafikprüfungen `runtime-audit-v2` und `runtime-proof` endeten regulär mit Exit 0. Dieser alte Hilfsprozess ist kein erfolgreich abgeschlossener Shutdown-Nachweis.

## Aktuelle Auflösung: Grass004 dunkelgrün in 2K

Auf Wunsch „mindestens 2k einbauen“ enthält der normale isolierte Testclient jetzt **2048 × 2048 Pixel mit zwölf Mipstufen**. Grundlage ist die echte `Grass004_2K-PNG_Color.png` aus dem offiziellen ambientCG-2K-Paket. Es wurde nicht aus 1K hochskaliert. Die freigegebene Farbkorrektur bleibt exakt gleich: −0,8 EV und linearer RGB-Tint `(0,8; 1; 1)`. Die komplette DDS benötigt rund 21,33 MiB; die freigegebene 1K-Version bleibt gesichert.

Die 2K-DDS wurde zunächst in `build-content-tx-p0/runtime-manual/terrain/modern/metin2_map_a1/slot-005.dds` installiert; die spätere Erweiterung auf weitere Maps ist oben dokumentiert. Der vorhandene Terrainloader reicht Auflösung und alle Mipstufen direkt an den GPU-Uploader weiter. Der kurze native A1-Lauf mit Modern → Classic → Modern bestand: drei Aufnahmen, Exit 0, Classic-Weltbild in diesem Lauf pixelgleich zur Originalreferenz, alle Grass- und Shutdown-Zähler sowie Diligent ERROR/FATAL 0. Alle 317 damals geschützten Dateien sowie die Produktquellen und Binärdateien waren unverändert.

[Aktuelle 2K-Aufnahme](evidence/content-tx-p0/grass004-comparison/darkgreen-2k-v3/gallery/Grass004-darkgreen-2k.jpg) · [2K-Import](evidence/content-tx-p0/grass004-comparison/darkgreen-2k-v3/evidence/import.json) · [2K-Prüfnachweis](evidence/content-tx-p0/grass004-comparison/darkgreen-2k-v3/evidence/result.json). Material und Farbe wurden zuvor in 1K freigegeben; eine gesonderte manuelle 2K-Sichtabnahme wird nicht aus dem automatisierten Test abgeleitet. Die folgenden Abschnitte dokumentieren die bisherigen Varianten.

## Angeforderter Gegenvergleich: D Grass004

Nach Freigabe von C wurde auf Nutzerwunsch **Grass004** als weitere Vergleichsvariante eingebaut. Rückmeldung: „das neue sieht besser aus nur zu hell das gras soll dunkler sein dunkel grüner“. Der normale isolierte Testclient erhielt daraufhin **Grass004 darkgreen v2**; C und das erste Grass004 bleiben als Referenzen erhalten. **Nach Neustart des Testclients wurde darkgreen v2 mit „okay das nehmen wir“ visuell freigegeben und als gewählte Variante bestätigt.** Auflösung dieser damaligen DDS: **1024 × 1024 Pixel, elf Mipstufen**.

Darkgreen v2 reduziert die Helligkeit um **−0,8 EV** und den Rotanteil im linearen Licht zusätzlich auf **80 %**, um den Gelbstich zu reduzieren. Kombinierte RGB-Faktoren: `(0,45948; 0,57435; 0,57435)`. Struktur, UV-Skalierung und Quelldatei bleiben erhalten; alle elf Mipstufen werden aus der korrigierten Quelle in linearem Licht erzeugt. Nur die DDS von A1-Slot 005 im Testclient wurde ersetzt. Der normale Testclient wurde anschließend zum sicheren Neuladen neu gestartet.

[Grass004 vorher/jetzt](evidence/content-tx-p0/grass004-comparison/darkgreen-v2/gallery/Grass004-before-after.jpg) · [aktuelle dunkelgrüne Version](evidence/content-tx-p0/grass004-comparison/darkgreen-v2/gallery/Grass004-darkgreen.jpg) · [aktueller Prüfnachweis](evidence/content-tx-p0/grass004-comparison/darkgreen-v2/evidence/result.json).

Auch darkgreen v2 wurde mit derselben A1-Kamera und Modern → Classic → Modern geprüft: **PASS**, drei Aufnahmen, Exit 0, Grass-Zähler und Shutdown-Ressourcen 0, Diligent ERROR/FATAL 0. Classic behält dieselbe dokumentierte minimale Abweichung (81 Pixel, maximal 3/255) zur ursprünglichen Referenz. Alle 317 geschützten Dateien, Produktquellen und Binärdateien sind unverändert. Die folgenden Angaben beschreiben den ursprünglichen unkorrigierten Grass004-Import.

Die angehängte `Grass004.png` ist ein Kugel-Vorschaubild. Verwendet wird die echte flache `Grass004_1K-PNG_Color.png` aus dem offiziellen [ambientCG-Material Grass 004](https://ambientcg.com/view?id=Grass004), Lizenz CC0. Import: 1024² RGBA8 DDS, elf in linearem Licht gefilterte Mipstufen, keine Farb- oder Helligkeitskorrektur. Normal/AO/Height/Roughness bleiben ungebunden. A1-Slot 005 und seine bisherige Kachelung bleiben unverändert; die vom Autor angegebene Materialgröße von 1,4 m wird für diesen direkten Vergleich nicht als neue UV-Skalierung übernommen.

C gegen Grass004 (historischer, entfernter Arbeitsstand) · Grass004 einzeln (historischer, entfernter Arbeitsstand) · [Import und Quellenhash](evidence/content-tx-p0/grass004-comparison/evidence/import.json) · [Prüfnachweis](evidence/content-tx-p0/grass004-comparison/evidence/result.json).

Identische A1-Kamera, Modern/High, Sonne und festgehaltene Umgebungszeit. Kurzer nativer Lauf mit Modern → Classic → Modern: **PASS**, drei Aufnahmen, Exit 0. A1/Slot 005 als einziger Override protokolliert; alle Grass-Zähler, Shutdown-Ressourcen und Diligent ERROR/FATAL jeweils 0. Classic gegen ursprüngliche Referenz: dieselben 81 geringfügig abweichenden Pixel wie bei der abgedunkelten C-Revision, maximal 3/255 Kanalstufen. Alle 317 geschützten Dateien sowie Source- und Binärhashes unverändert. Kein erneuter Build oder Gesamttest für diesen reinen Assetvergleich.

Die bisherige DDS ist unter `build-content-tx-p0/brightness-v2/imported/slot-005.dds` erhalten und zusätzlich im neuen Vergleichsnachweis gesichert. Der normale Testclient unter `build-content-tx-p0/runtime-manual/` lädt jetzt Grass004; bereits geladenes A1 benötigt Kartenwechsel oder Neustart. A1-Prüfpunkt: `/warp 4226 9055`. Die folgenden Abschnitte dokumentieren weiterhin die zuvor abgeschlossene C-Abnahme.

## Sichtkorrektur: C abgedunkelt

Erste Nutzerrückmeldung: **Ohne 3D-Gras sieht es besser aus; die neue Textur ist zu hell.** Daraufhin wurde C abgedunkelt. Die abschließende Rückmeldung nach dem A1-Hinweis lautet **„ja sieht gut aus“**; damit sind Grasentfernung und abgedunkelte Textur visuell bestätigt.

Nur die importierte BaseColor von A1-Slot 005 wurde offline um **−0,8 EV** abgedunkelt: linearer RGB-Faktor **0,57435**, ohne separate Farbton-/Sättigungsänderung. Quelle, Details, Scale, Masken und Rendererbeleuchtung bleiben unverändert; alle elf Mipstufen wurden erneut in linearem Licht erzeugt. Die folgenden ursprünglichen Messwerte dokumentieren weiterhin den ersten A/B/C-Lauf; die Galerie zeigt jetzt das abgedunkelte C.

C vorher/jetzt (historischer, entfernter Arbeitsstand) · aktuelles C (historischer, entfernter Arbeitsstand) · [Prüfnachweis der Korrektur](evidence/content-tx-p0/brightness-v2/evidence/result.json).

Kurzer A1-Lauf mit identischer Kamera und Modern → Classic → Modern: **PASS**, Exit 0, Grass-Zähler 0, Diligent ERROR/FATAL 0, Shutdown-Ressourcen 0. Alle 317 geschützten Quelldateien unverändert. Kein Sourcecode geändert, daher kein erneuter Build oder vollständiger Testlauf. Die neue Classic-JPEG-Aufnahme ist visuell unverändert, aber im Gegensatz zum ersten Lauf nicht exakt pixelgleich: 81 Pixel im Weltbild weichen um höchstens 3/255 Kanalstufen ab. Diese kleine Abweichung wird dokumentiert und nicht als exakte Gleichheit ausgegeben.

Die neue DDS liegt auch im isolierten normalen Testclient. Der zunächst auf B1 gestartete Charakter sah dort erwartungsgemäß die Originaltextur. Nach Klarstellung der A1-Begrenzung und dem Hinweis auf den geprüften A1-Punkt wurde die Optik bestätigt. Der Originalclient bleibt unangetastet. **Sichtabnahme des abgedunkelten C erteilt.**

## Umfang und Ausgangspunkt

- Source: `codex/g56-hdr-atmosphere`, `def2674` (anfangs sauber).
- Originalclient: `codex/g56-hdr-colors`, `6697736a` (anfangs sauber).
- Neue Designregel: flächiges Gras entsteht durch Terrainmaterial. Bäume, Büsche, Blumen und gezielte Bodendecker bleiben Vegetationsobjekte.
- Ausschließlich A1, Slot **005**, vorher `d:/ymir work/terrainmaps/b/grass/grass 01.dds`. Dieser Slot ist mit **315.247** Maskeneinträgen der häufigste Grass-Slot; Slot 006 hat 165.682, Slot 007 hat 7.723. Zählung einschließlich vorhandener Tile-Randdaten, keine Flächenmessung.
- Heightmaps, Geometrie, Splat-Masks, Layerverteilung, Paths, Dirt, Rocks, Buildings, Water und Spawns bleiben unverändert. **317 geschützte Dateien** einschließlich aller A1-Quelldaten, TextureSets und des gesamten Texture-Packs sind nachher SHA256-identisch.

## Entfernte Arbeit, erhaltene Systeme

Der Produktionsaufruf `PrepareNativeGrass`, der vollständige Kartendurchlauf `VisitGrassTerrain`, die gespeicherten Graszellen und der Aufruf `RenderNativeGrass` sind entfernt. Damit entstehen keine Grass-Platzierungen, Grass-GPU-Batches oder Grass-Asset-Uploads mehr. Die Grasdaten-Hilfsfunktionen und portable H2-Test-APIs bleiben für mögliche spätere Editor-/Bodendeckerarbeit erhalten, werden im Produktionspfad aber nicht aufgerufen.

Unverändert bleiben ZiiNAN Vegetation Runtime, Registry, Legacy-Asset-Fallback, Bäume/Büsche, Instancing, LOD/Impostors, Wind, Leaf Flutter, Foliage Lighting und Backlighting. Die vorhandene Vegetationsqualität steuert weiterhin die Baum-/Vegetationsparameter. Es gibt keine separate anklickbare Grass-Option und keine neue UI.

## Material und Import

[Vollständiges Inventar aller 68 Dateien](evidence/content-tx-p0/evidence/texture-inventory.md), [Hashes, Formate und Kanalwerte](evidence/content-tx-p0/evidence/source-inventory.json).

Sechs Varianten liegen als rohe Texturen sowie als Unreal/glTF-Materialexport vor. Vorhanden sind BaseColor, Normal, AO, Height, Metallic und Roughness; die Materialexporte enthalten zusätzlich bei Varianten 2 und 5 gepacktes Metallic/Roughness. Die `.gltf`/`.bin`-Dateien beschreiben Material und Vorschaumesh und werden nicht in die Spielwelt importiert.

Verwendet wird genau **`T_GrassMat3_basecolor.PNG`**, die reine Grasvariante ohne aufgemalte Wege/Steine/Schnee. Die rohe BaseColor wird verwendet, nicht die sichtbar anders aufbereitete BaseColor des Materialexports. Quelle ist ausschließlich lesend geöffnet worden.

| Parameter | Proof |
|---|---|
| BaseColor | 4096² Quelle → 1024² RGBA8 DDS, 11 Mipstufen |
| Farbraum | Autorierte sRGB-Werte; bestehender Modern-Terrainshader dekodiert nach Linear |
| Mipfilter | Offline in linearem Licht; initial ohne Farbkorrektur, aktuelle Revision mit −0,8 EV; keine künstlichen Maps |
| Metallic | 0; kein metallischer Term |
| Specular | 0; bestehende diffuse Terrainbeleuchtung |
| Roughness | Matt/diffus; kein Roughness-Sampler oder zusätzlicher PBR-Pfad |
| Normal/AO/Height | Reale Quellen inventarisiert, im Proof nicht gebunden |
| Texture Scale | Originalslot unverändert: U/V = 9, Offset = 0; HTP-Periode 3200/9 ≈ 355,6 Welteinheiten ≈ 3,56 m |
| Sampling | Bestehendes Wrap- und Mip-Filtering; keine Anti-Tiling-Erweiterung |

Die echte Normalmap zeigt eine tangententypische +Z-Verteilung. Ihre Y-Konvention wurde anhand Height-/Normaldaten geprüft, aber nicht belastbar für einen Terrain-Tangentenraum bestätigt. Deshalb wird kein Green-Flip vorgenommen und kein Normalmapping behauptet. Der vorhandene Terrainpfad unterstützt diffuse Farbe und Geometrienormalen; ein neuer Terrain-PBR-/Tangentenpfad liegt außerhalb dieses Proofs. Metallic/Roughness/AO werden somit auch nicht versehentlich als sRGB-Farben geladen.

Die DDS-Kanten unterscheiden sich durchschnittlich um 4,72 / 3,87 Kanalstufen; benachbarte innere Texel um 4,72 / 3,73. Damit entsteht keine auffällige zusätzliche Kantenstufe. Die Aufnahmen zeigen die vorhandenen Maskenübergänge unter Sonne, Schatten, SSAO, HDR und Tone Mapping. Langfristiges Bewegungsflimmern und die künstlerische Eignung bleiben Teil der Sichtabnahme.

## Modern-only Override

Der bestehende Pack-Dateizugriff und Terrain-Uploader laden optional `terrain/modern/<map>/slot-NNN.dds`. Die Zuordnung enthält Karte und Slot. Nur der isolierte C-/normale Testclient enthält **`terrain/modern/metin2_map_a1/slot-005.dds`**. Es gibt keinen globalen Austausch des alten Grass-Dateinamens und keine Änderung der TextureSet-Dateien.

Der Map-Loader erfasst vorhandene Override-Pfade; GPU-Texturen werden erst bei einem Modern-Draw geladen. Classic nutzt die Originaltextur. Original- und Modern-Farbmaterial teilen dieselbe unveränderte Splat-Maske und besitzen getrennte gecachte Bindings, sodass der Live-Wechsel Modern → Classic → Modern sicher funktioniert. Beide werden beim Kartenabbau freigegeben. Ohne Override-Datei bleiben die bisherigen Terrainfarben erhalten.

## Kurzer Performance-Sanity

Gleiche A1-Kamera, Modern/High, gleiche Umgebung und Sonne, festgehaltene Wind-/Himmelzeit. Erste 180 markierte Frames nach zwei Sekunden Aufwärmen; drei kurze nacheinander ausgeführte Läufe ohne gleichzeitigen Build/GPU-Test. CPU ist Framearbeit ohne Present/Limiter; GPU stammt aus der vorhandenen Frame-Abfrage. Keine Perzentile, kein Langbenchmark.

| Messwert | A: alt + 3D-Gras | B: alt, ohne Gras | C: neue Textur, ohne Gras |
|---|---:|---:|---:|
| Map-Load, ms | 153,37 | 71,65 | 76,56 |
| Grass-Preparation, ms | 78,43 | 0 | 0 |
| Grass-Platzierungen | 625.189 | 0 | 0 |
| Grass-Platzierungsdaten, MiB | 19,08 | 0 | 0 |
| Vegetation Draws / Frame, inkl. Schatten | 55 | 49 | 49 |
| CPU-Framearbeit Mittel, ms | 4,076 | 0,648 | 0,634 |
| GPU-Framezeit Mittel, ms | 0,562 | 0,491 | 0,490 |

Das kleine Fenster zeigt keine relevante Regression; die Grass-Arbeit entfällt eindeutig. Ladezeiten hängen auch von Cache und Systemlast ab und sind keine garantierte allgemeine FPS-/Ladezeitverbesserung. [Rohwerte und Auswertung](evidence/content-tx-p0/evidence/comparison.json).

## A/B/C und Prüfungen

Interaktive Galerie (historischer, entfernter Arbeitsstand) · alle drei nebeneinander (historischer, entfernter Arbeitsstand).

- **A:** Original Terrain + bisheriges 3D-Gras, unveränderte alte Release-EXE.
- **B:** Original Terrain, ohne flächiges 3D-Gras.
- **C:** Ohne 3D-Gras, neue BaseColor ausschließlich in A1-Slot 005.

Kamera: A1 `(13000, 9500, 17668.5)`, Distanz/Pitch/Rotation `(4500, 25, 0)`, 1024×768. Alle drei Zustände verwenden identische Einstellungen. Der frühere Kamera-Vorlauf auf dem Stadtpflaster ist separat unter `evidence/pilot-a` abgelegt und gehört nicht zur Galerie oder den Vergleichszahlen.

| Prüfung | Ergebnis |
|---|---|
| Betroffener Release-Client und Testtargets | PASS |
| Terrain CPU/GPU, Vegetation Contracts/Modern/GPU, benötigte Asset-Fixture | **6/6 PASS**, 14,02 s |
| A1 native A/B/C und Live-Stylewechsel | PASS, alle Läufe Exit 0 |
| Classic gegen unveränderte alte EXE | B und C im Weltbild **pixelgenau**; obere 50 Zeilen mit Zustandslabel ausgenommen |
| Neue Textur tatsächlich geladen | Genau ein protokollierter Override, A1/Slot 005 |
| Grass nach Load und nach Stylewechsel | Platzierungen/Zellen/Tiles/Bytes/Preparation jeweils 0 |
| Diligent ERROR/FATAL | **0/0** |
| Shutdown-Ressourcen | **0**, native A/B/C und Classic-Referenz |
| Geschützte Quellen/Mapdaten | **317/317 SHA256-identisch** |
| GCC/LP64 | NOT RUN: keine portable/core Logik geändert |
| Vollständige Classic-Golden-/Gesamtsuite | NOT RUN; gezielter A1-Bildvergleich durchgeführt |

Beim ersten Build-/Startversuch blockierte die eingeschränkte Umgebung Windows-SDK bzw. Clientinitialisierung. Die erlaubten Wiederholungen außerhalb dieser Umgebung bestanden. Der gemeinsame Modern-Teststarter erwartet auch bei einem Classic-only Lauf eine Modern-Logdatei; die Classic-Referenz wurde daher anschließend direkt anhand Exit 0, Aufnahme, leerem Game-Errorlog und Null-Ressourcenzählern validiert. Es wurde keine fehlende Modern-Datei als echte Renderstörung gewertet.

[Gebündelter Nachweis mit Binär-/Sourcehashes](evidence/content-tx-p0/evidence/result.json), [Importparameter](evidence/content-tx-p0/evidence/import.json). Das rohe Testprotokoll bleibt lokal unter `build/root-cleanup/final-gate/uncommitted-evidence/` erhalten und ist kein Commit-Inhalt.

## Sichtabnahme / STOP

Der normale isolierte Testclient liegt unter `build-content-tx-p0/runtime-manual/`, mit originalem Einstiegsskript und Modern/High. Er wurde sichtbar geöffnet und bleibt für die Sichtabnahme verfügbar. Die neue Farbe ist nur auf A1/Slot 005 sichtbar; die Entfernung des flächigen 3D-Grases gilt für den neuen Client allgemein.

Bekannte Grenze des ersten C: Es wirkte deutlich heller und gelbgrüner als die alte Grass-Textur. Nach der Rückmeldung wurde nur die Helligkeit wie oben dokumentiert reduziert. Die aktuelle Optik wurde vom Nutzer bestätigt. Es wurden keine weiteren Slots, Maps, Packs oder Materialarten ersetzt.

**Entfernung des 3D-Grases und abgedunkelte Textur: visuelles GO. Technische und visuelle Voraussetzungen dieses Proofs erfüllt. Produktionsübernahme nicht ausgeführt. CONTENT-TX-X wird nicht begonnen. STOP nach CONTENT-TX-P0.**

Abnahme: „ja viel besser“ zum abgedunkelten Vergleich, anschließend „ja sieht gut aus“ nach dem Hinweis auf A1 im normalen Testclient.
