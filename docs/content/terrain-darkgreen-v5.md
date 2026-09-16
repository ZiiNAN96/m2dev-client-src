# Dunkelgrünes Gras – Testclient v5

Die gewünschte stärkere Abdunklung ist im bestehenden Testclient eingebaut: sechs Grasmaterialien, 128 zugeordnete Layer. Grass001–004 erhalten zusätzlich −1,25 EV und den linearen RGB-Faktor [0,72; 0,90; 0,96]. Bei leafy_grass und sparse_grass greift diese Änderung nur anteilig auf grüne Vegetation. Trockengras, Erdbereiche und andere Bodenarten bleiben in ihrem bisherigen Farbton.

Die Auflösung bleibt 2048 × 2048 mit zwölf Mipstufen. Die bisherige Grass004-Freigabe wurde durch die neue ausdrückliche Farbkorrektur ersetzt. Die v4-Dateien und deren Prüfbilder bleiben als Vergleich erhalten; neue Dateien liegen in darkgreen-v5/payload, der normale Testclient lädt diese über seine aktualisierten Layer-Dateien.

Fünf native Ansichten auf A1/B1/C1, Wald und Kap bestanden mit Exit 0, leerem Fehlerlog und null erfassten Ressourcen nach dem Beenden. Der Vergleich aus gleicher Kameraposition zeigt deutlich dunkleres Gras. Sichtprüfung durchgeführt; endgültige Benutzerfreigabe steht aus. Der bestehende Flammen-Dungeon-Fehler aus dem Basiseinbau bleibt außerhalb dieser Farbkorrektur.

Gemessenes Verhältnis der linearen Helligkeit auf den grünen Bildflächen der A1-Vergleichsaufnahme: 0.402.

[Vorher/Nachher](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build-content-tx-p0/terrain-test-v4/darkgreen-v5/game-comparison.jpg)

[Prüfergebnis](C:/Users/ZiiNAN/Documents/GitHub/m2dev-client-src/build-content-tx-p0/terrain-test-v4/darkgreen-v5/result.json)
