# 2K-Auswahl für alle Terrain-Bodenarten

> Archiviert am 18.09.2026: vorhandene Berichte und Prüfaufnahmen wurden nach `evidence/` übernommen. Alte Testclients, Materialvorschauen und Buildausgaben wurden entfernt. [Vollständige erhaltene Textbelege](evidence/content-tx-p0.zip).

Stand: 16.09.2026, Gras/Waldboden v3. Aktueller Auftrag: **Sand und Erde sowie Lava/Vulkan aus dem Katalog übernehmen; bessere Gras-/Waldbodenquellen finden.** Die Materialauswahl für die genannten Bodenarten ist festgehalten. Die Suche und Galerie betreffen **218 Terrain-Referenzen** aus 37 TextureSets in **85 vorhandenen Maps**. Es erfolgte noch kein neuer Einbau dieser Materialien.

**Zwölf unterschiedliche Gras-/Waldboden-Basen** sind jetzt auf **57 Referenzen** verteilt. Statt 34 reinen Grasreferenzen dieselbe Grass004-Textur zuzuordnen, bleiben dichte, blättrige, olivgrüne, trockene und lückige Grasflächen sowie Moos-, Wurzel-, Nadel- und steinige Waldböden unterscheidbar. Grass004 bleibt gezielt für zwei passende Referenzen vorgesehen. Die neuen Vorschläge sind noch nicht vom Nutzer visuell freigegeben.

- Interaktive Materialübersicht mit Alt/Neu-Vergleich (historischer, entfernter Arbeitsstand)
- [Materialliste mit Quellen und direkten 2K-Downloads](evidence/content-tx-p0/terrain-2k-catalog/Auswahl.md)
- [Alle Zuordnungen als CSV](evidence/content-tx-p0/terrain-2k-catalog/alt-neu-zuordnung.csv)
- [Strukturierte Zuordnung und offizielle Download-Metadaten](evidence/content-tx-p0/terrain-2k-catalog/catalog.json)
- [Gesicherte Auswahlentscheidungen](evidence/content-tx-p0/terrain-2k-catalog/selection-decisions.json)

Bestätigte Quellen: Sand `Ground079L`, `Ground079S`, `Ground093A`; Erde `Ground085`, `Ground103`, `Ground106`, `Ground067`, `Ground087`, `mud_cracked_dry_riverbed_002`; Lava/Vulkan `Lava004`, `Lava002`, `Lava001`, `Rock031`. `Gravel040` ist für den Vulkan-Geröll-Layer 192 ausgewählt. Flusskiesel wurden nicht pauschal mit freigegeben. Bestätigte Quellen, Download-URLs und die 161 Zuordnungen außerhalb der Gras-/Waldbodenüberarbeitung sind unverändert. Die Galerie zeigt den jeweiligen Auswahlstatus und bietet einen Filter für bestätigte Materialien.

Abgedeckt: Gras, Wald-/Moosboden, Erde, Wege, Sand, Kies, Fels, Pflaster, Steinplatten, Schnee, Eis und Lava. Die neue Naturbodenauswahl berücksichtigt Bewuchsdichte, sichtbare Erdanteile und die ursprünglichen warmen bzw. kühlen Kartenfarben. Grass004 zeigt weiterhin die akzeptierte Dunkelgrün-Korrektur. Andere Quellenbilder sind noch unkorrigiert; Helligkeit, Kachelung, Größe und Übergänge benötigen beim Einbau einen Ingame-Vergleich. Besonders die helle Quelle für abgestorbenes Steppengras benötigt vor einem Einbau eine passende Abdunklung.

Der Katalog enthält **47 offizielle 2K-Quellen**: 46 direkte Materialbasen und eine verbleibende Quelle für offene Sondervarianten. Die Quellen sind CC0: [ambientCG](https://docs.ambientcg.com/license/) und [Poly Haven](https://polyhaven.com/license). Neu geprüft wurden neun Poly-Haven-Gras-/Waldboden-Farbdateien tatsächlich in 2048²; sechs davon wurden für diese Überarbeitung ausgewählt. Hinzu kommen die bereits geprüften Grasvarianten. Download-Metadaten stammen aus den offiziellen APIs. Vollständige Materialpakete wurden nicht gesammelt heruntergeladen. Kleinere Vorschaubilder sind in der eigenständig öffnenden HTML-Datei eingebettet.

Die abgebrochene Überarbeitung `revision-v2` bleibt ausschließlich als Arbeitsstand erhalten. Ihre generierten Ornamententwürfe und zusätzlichen Stein-/Pflastervorschläge wurden nicht in den aktuellen Katalog übernommen. Keine neue Bildgenerierung ausgeführt.

**18 Oberflächen benötigen eigene Varianten** aus den gewählten Basen, etwa Schnee auf Pflaster, Ornamente, Spinnweben und leuchtende Eisrisse. Diese Varianten sind Vorschläge und noch nicht erstellt. Bei **16 alten Referenzen fehlt die Originaldatei**; dort ist die Zuordnung anhand von Dateiname und Kontext ausdrücklich gekennzeichnet. Eine technische Schwarztextur wird beibehalten. Die Recherche erfasst Terrain-Layer einschließlich Terrain-Fels; Materialien separater Gebäude-/Objektmodelle sowie Wasser sind nicht Teil dieser Liste. 16 Atlas-Aliase ohne vorhandene Mapdaten wurden nicht als weitere Maps gezählt.

Kontrolle: alle 218 Zuordnungen und gültige Materialverweise geprüft; bestätigte Quellen/Zuordnungen gegenüber dem vorherigen Katalog verglichen. Flache Farbtexturen visuell gesichtet. Browserprüfung: zwölf Gras-/Waldbodenkarten, sechs bestätigte Erdbasen, insgesamt 14 bestätigte Quellen einschließlich des zweckgebundenen Vulkan-Gerölls sowie 62 entsprechend markierte Referenzen. Filter, Vergleichsansicht und schmale Darstellung funktionieren. Native Grafiktests gehören ausschließlich zum getrennten [Gras-Nachweis](content-tx-p0-grass-proof.md), nicht zur neuen Materialrecherche. Kein neuer Spieleinbau, Commit oder Push.
