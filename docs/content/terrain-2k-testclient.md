> Aktueller Stand 16.09.2026: Alle Bodentexturen sind wieder original; sämtliche Katalog-Ersetzungen sind deaktiviert. Großflächiges 3D-Gras bleibt im Release-Client abgeschaltet. [Rücksetzung und Prüfung](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/docs/content/terrain-original-restoration.md). Der folgende Bericht ist historisch.

> Aktueller Testclient: [Alle Originalgräser v9](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build-content-tx-p0/terrain-test-v4/all-original-grass-v9/README.md). Sämtliche Gras-Ersetzungen entfernt. Die folgenden Angaben dokumentieren den Basiseinbau v4.

# Boden-2K-Testclient

Stand: 16.09.2026

26 echte 2048 × 2048-Materialien ersetzen 118 ausgewählte Alttexturen über 466 Layer-Zuordnungen in 84 vorhandenen Maps. 78 dieser Maps benutzen mindestens einen dieser Layer in ihren Bodenmasken; sechs referenzieren sie nur im TextureSet.

Testclient: [runtime-manual/Metin2_Release.exe](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build-content-tx-p0/terrain-test-v4/runtime-manual/Metin2_Release.exe)

Start im Ordner runtime-manual. Grafikstil Modern ist voreingestellt. Der normale Client und das frühere Gras-Testverzeichnis bleiben unverändert.

## Umfang

- Sand, Erde, Lava und Vulkan gemäß Katalogauswahl; zwölf unterschiedliche Gras-/Waldboden-Basen.
- Grass004 ist bytegleich mit der zuvor freigegebenen dunkelgrünen 2K-Fassung (−0,8 EV, Rotanteil in linearem RGB ×0,8). Die anderen Materialien behalten ihre Quellfarben.
- RGBA8-DDS mit zwölf Mipstufen, in linearem Licht gefiltert. Echte 2K-PNG-Quellen, keine vergrößerten Vorschaubilder.
- Nur Farbkarten; Normal-, Höhen-, Rauheits- und Emissionskanäle wurden nicht neu in den Renderer integriert.
- Bodenmasken, UV-Skalierung, Geometrie und Classic-Texturen bleiben unverändert. Großflächiges 3D-Gras bleibt abgeschaltet.
- Spinnweben-Boden dc_spider_00.dds bleibt original, weil die bestätigte Felsbasis das besondere Spinnweben-Detail nicht enthält. Weitere nicht ausgewählte Stein-, Pflaster-, Schnee- und Eisvorschläge wurden nicht eingebaut.

## Prüfung

- PASS: elf native Ansichten auf sieben Maps, darunter A1/B1/C1, zwei Wälder, Wüste und Vulkan; Modern → Classic → Modern, reguläres Ende mit Exit 0.
- PASS: alle installierten Dateien stimmen mit den 26 geprüften DDS-Dateien überein. Das Terrain-Ladeprotokoll bestätigt tatsächlich benutzte neue Dateien.
- PASS: keine Renderfehler im finalen Lauf, leeres syserr, erfasste Shutdown-Ressourcen und 3D-Gras-Zähler null.
- PASS: 12201 geschützte Original-/Quelldateien inklusive beider Programmstände hashgleich.
- Sichtprüfung der aufgenommenen Bilder durchgeführt; keine fehlenden Bodenflächen in diesen Ansichten. Nicht jede Map und nicht jedes Material wurde im Spiel visuell geprüft. Die gestalterische Freigabe durch den Benutzer steht aus.

## Bestehende Einschränkung

Der zusätzliche Flammen-Dungeon metin2_map_n_flame_dungeon_01 beendet den nativen Test mit Exit 5: failed_world=1, failed_terrain=0. Dasselbe passiert im getrennten Kontrolllauf ohne neue Bodentexturen. Fehlende Objekt-Lichttexturen werden ebenfalls gemeldet. Dieser Fehler ist kein Nachweis gegen das Texturpaket, bleibt aber ungelöst; der Dungeon ist nicht visuell abgenommen. Die offene Vulkanmap funktioniert im Test.

## Nachweise

- [Spielansichten](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build-content-tx-p0/terrain-test-v4/evidence/native-proof.jpg)
- [Materialübersicht](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build-content-tx-p0/terrain-test-v4/evidence/installed-materials.jpg)
- [Prüfergebnis](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build-content-tx-p0/terrain-test-v4/evidence/result.json)
- [Map-/Layer-Zuordnung](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build-content-tx-p0/terrain-test-v4/manifest.json)
- [Quellen, Lizenzen und Prüfsummen](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build-content-tx-p0/terrain-test-v4/imports.json)

## Materialquellen

| Material | Anbieter/Quelle | Lizenz |
|---|---|---|
| Grass001 | [Quelle](https://ambientcg.com/a/Grass001) | CC0 |
| Grass002 | [Quelle](https://ambientcg.com/a/Grass002) | CC0 |
| Grass003 | [Quelle](https://ambientcg.com/a/Grass003) | CC0 |
| Grass004 | [Quelle](https://ambientcg.com/a/Grass004) | CC0 |
| Gravel040 | [Quelle](https://ambientcg.com/a/Gravel040) | CC0 |
| Ground067 | [Quelle](https://ambientcg.com/a/Ground067) | CC0 |
| Ground079L | [Quelle](https://ambientcg.com/a/Ground079L) | CC0 |
| Ground079S | [Quelle](https://ambientcg.com/a/Ground079S) | CC0 |
| Ground085 | [Quelle](https://ambientcg.com/a/Ground085) | CC0 |
| Ground087 | [Quelle](https://ambientcg.com/a/Ground087) | CC0 |
| Ground093A | [Quelle](https://ambientcg.com/a/Ground093A) | CC0 |
| Ground103 | [Quelle](https://ambientcg.com/a/Ground103) | CC0 |
| Ground106 | [Quelle](https://ambientcg.com/a/Ground106) | CC0 |
| Lava001 | [Quelle](https://ambientcg.com/a/Lava001) | CC0 |
| Lava002 | [Quelle](https://ambientcg.com/a/Lava002) | CC0 |
| Lava004 | [Quelle](https://ambientcg.com/a/Lava004) | CC0 |
| Rock031 | [Quelle](https://ambientcg.com/a/Rock031) | CC0 |
| forest_ground_04 | [Quelle](https://polyhaven.com/a/forest_ground_04) | CC0 |
| forest_ground_06 | [Quelle](https://polyhaven.com/a/forest_ground_06) | CC0 |
| forrest_ground_01 | [Quelle](https://polyhaven.com/a/forrest_ground_01) | CC0 |
| forrest_ground_03 | [Quelle](https://polyhaven.com/a/forrest_ground_03) | CC0 |
| leafy_grass | [Quelle](https://polyhaven.com/a/leafy_grass) | CC0 |
| mossy_rock | [Quelle](https://polyhaven.com/a/mossy_rock) | CC0 |
| mud_cracked_dry_riverbed_002 | [Quelle](https://polyhaven.com/a/mud_cracked_dry_riverbed_002) | CC0 |
| sparse_grass | [Quelle](https://polyhaven.com/a/sparse_grass) | CC0 |
| withered_grass | [Quelle](https://polyhaven.com/a/withered_grass) | CC0 |
