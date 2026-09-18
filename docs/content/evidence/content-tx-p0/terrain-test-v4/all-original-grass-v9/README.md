# Alle ursprünglichen Grastexturen wieder aktiv

Alle Gras-Ersetzungen wurden aus dem aktuellen Testclient entfernt. Das umfasst sämtliche ursprünglich als Gras benannten Terrain-Dateien sowie neue dichte, trockene, spärliche und mit Erde gemischte Grasmaterialien. 132 weitere Overrides wurden reversibel archiviert; zusammen mit der vorherigen Rücksetzung sind 147 Layer-Zuordnungen in 51 Maps wieder auf den ursprünglichen gepackten Bestand zurückgesetzt. Es gibt keine aktiven neuen Grasmaterialien mehr. 319 andere Boden-Layer sind bytegleich mit dem vorherigen Stand.

PASS: sechs native Ansichten auf A1/B1/C1, Wald, Kap und Wüste; Exit 0, leeres Fehlerlog, keine Gras-Override-Ladungen, erfasste Shutdown-Ressourcen null. Großflächiges 3D-Gras bleibt abgeschaltet. 12201 geschützte Dateien sind hashgleich. Die ursprüngliche Auflösung der alten Texturen bleibt erhalten. Benutzerfreigabe steht aus.

## Bereits fehlende Originaldateien

Die Laufzeitprüfung des unveränderten Pack-Bestands findet 36 der 40 zurückgesetzten Originalreferenzen. Drei alte Dateien für season1/metin2_map_empirewar_a01 fehlen und werden dort von Bodenmasken verwendet: empirewar/field03.dds, grass01.dds und grass02.dds. Zusätzlich fehlt g/field/grass 01.dds; dessen Referenz in metin2_map_t2 hat keine verwendeten Maskeneinträge. Diese alten Datenlücken wurden durch die Rücksetzung wieder sichtbar und nicht durch neue Ersatztexturen verdeckt. Die Empirewar-Map ist nicht visuell freigegeben.

[Spielansichten](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build-content-tx-p0/terrain-test-v4/all-original-grass-v9/native-proof.jpg)

[Prüfergebnis und fehlende Originalreferenzen](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build-content-tx-p0/terrain-test-v4/all-original-grass-v9/result.json)
