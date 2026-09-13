# Milestone 11 – Restpfad-Audit und Validierung

Stand: 13.09.2026. Ausgangsstand `56a79e1`. Nur M11; Legacy D3D9Ex bleibt Standard.
**Ergebnis:** M11 für die dokumentierten Szenen erfolgreich geprüft, einschließlich korrigiertem
Questdialog. Release ON/OFF und beide vollständigen Testsuiten bestanden. Keine uneingeschränkte
Freigabe für ungetestete Custom-Assets oder Entfernung des weiterhin benötigten D3D9-Geräts.
Keine Änderungen an Originalpaketen, Server, Granny-Animation oder CPU-Skinning. Keine Commits/Publikation.

## 1. Vollständige Fundstellen-Tabelle

`docs/renderer-milestone11-d3d9.csv` enthält Datei, Zeile, Klasse, Funktion, API/Wrapper,
Kategorie, Erreichbarkeit, Diligent-Alternative, Aktion und geprüfte Aufrufkette.
Reproduzierbar mit `tests/Renderer/audit_d3d9.ps1`.
Scan: C/C++/Header in `src`, `extern`, `vendor`, `buildtool`; zusätzlich wurden Aufrufer in den
originalen `assets/root`-Pythondateien verfolgt. Generierte Buildkopien sind nicht neue Sourcepfade.
Der Index enthält bewusst auch SDK-Deklarationen, Kommentare und Namenskollisionen mit D3D11.
**3890 Fundstellen sind daher weder 3890 Renderpfade noch ausgeführte D3D9-Drawcalls.**
Die Kontextzuordnung ist ein überprüfter Quellenindex mit Funktions-/Dateirouten, kein vollständiger C++-AST-Beweis.
Indirekte Verbraucher wie `event.RenderEventSet` und Ausschluss-Scope werden ausdrücklich mit erfasst.

## 2. Kategorien und Restpfade

| Kategorie | Fundstellen | Behandlung |
|---|---:|---|
| A – normale Sonderpfade | 187 | Vorschalt-/Upload-Bridges für ursprüngliche Geometrie, Texturen und UI |
| B – Auswahl/Erstellung | 12 | bestehender Actor-Renderer, eigener Scope/Viewport/Licht |
| C – Shadow/Helper | 43 | dynamische Legacy-Schatten in Diligent bewusst deaktiviert |
| D – Entwicklerdarstellung | 91 | keine neue Debug-Engine |
| E – SDK/Tool/Dependency | 1208 | kein Editor-/SDK-Port |
| F – inaktiv/Kommentar | 346 | dokumentiert; keine Laufzeit-Calls behauptet |
| G – paralleler Legacy-Pfad/Bridges | 2003 | native GPU-Ausführung im Diligent-Frame gesperrt, CPU-Daten bleiben |

A/B bezeichnet den ursprünglichen M11-Handlungsbedarf, nicht einen nach Abschluss noch fehlenden Port.
Geprüfte visuelle Ketten stehen zusätzlich in `renderer-milestone11-analysis.md`.

## 3. Character Select / Create: Implementierung

`introSelect.py` / `introCreate.py::CharacterRenderer.OnRender` → `grp.ClearDepthBuffer`,
`SetOmniLight`, `SetViewport` → `chr.Deform/Render` → CharacterManager → `CActorInstance`
→ Granny-Meshgruppen → bestehender M5-Actor-Renderer.
Die Vorschau benutzt dieselben Actor-/Equipment-/Animationsobjekte, aber keine Outdoor-Map.
Der frühere `actorWorldFrame`-Filter unterdrückte deshalb die Vorschau. `ActorPreviewScope`
in den beiden Python-Actor-Eintritten aktiviert nur diesen vorhandenen Pfad.
Viewport, Projection sowie ursprüngliches Spot-/Point-Licht werden pro Draw übernommen;
ClearDepth wirkt auf die D3D11-Surface. Es gibt hier kein separates Offscreen-RenderTarget.
Rotation, Idle, Race/Shape/Haare und Erzeugung bleiben im originalen Python-/Actor-Code.

## 4. Guild Projection

Es handelt sich um **Gildenland-Bodenmarkierung**, nicht um Gildenlogo, Flagge oder neue Decals.
Kette: Landdaten → `RegisterGuildArea/VisibleMarkedArea` → `CTerrain::AllocateMarkedSplats`
→ `RenderMarkedArea/RecurseRenderAttr/DrawPatchAttr`.
Geometrie: originale Terrain-Patchpositionen und LOD0-Stripindices. Textur: native generierte
Markierungsmaske, als CPU-Pixel übernommen, plus ursprüngliche weiße Attr-Textur.
Die Kameraraumprojektion verwendet inverse View, Tile-Translation und die originale
statische Shadow/Mask-Matrix. Zweite Stufe multipliziert Maskenalpha; RGB bleibt CURRENT.
Alpha pulsiert wie Legacy im Dreisekundenzyklus; SRCALPHA/INVSRCALPHA, ursprünglicher
Depth-Test/Write-State, Point/Clamp der Maske. Reihenfolge nach Map, vor Actors.
Kein neuer Decal-Manager. Testhilfe `background.testGuildArea` existiert nur bei aktivierten
Renderer-Diagnosetests und ruft die ursprüngliche lokale Landregistrierung auf, ohne Serverpakete.

## 5. DungeonBlock

Sichtbare **starre GR2-Sondergeometrie**, nicht bloß ein Collision-Helper.
Property `DungeonBlockFile` → `CArea::__SetObjectInstance_SetDungeonBlock`
→ `CDungeonBlock::Load/Update/Render` → `CDungeonModelInstance`
→ `RenderMeshNodeListWithTwoTexture(TYPE_RIGID, TYPE_BLEND_PNT)`.
Originale PNT2-Vertexdaten (tatsächlicher Drawstride 40 Bytes), Indices, Meshmatrizen,
Materialgruppen, zwei Texturstufen und ursprüngliche Blend-/Depth-States werden übernommen.
Der einmalige CPU-Abzug gehört zum Modell; Diligent-Ressourcen werden mit diesem freigegeben.
Keine Änderung an Portal-, Kollisions- oder Sichtbarkeitslogik.
Repräsentativ geprüft: originale Affendungeon-Map `metin2_map_monkeydungeon`.
Weitere DungeonBlock-Properties sind im Assetbestand vorhanden; nicht jede davon wurde live besucht.

## 6. Actor-Kategorien

`ClassifyActor` beschränkt NPCs nicht mehr künstlich auf niedrige Races;
normale Gegner, Metinsteine und Polymorph-Actors benutzen denselben Mobpfad.
Weitere bekannte native Typen bis einschließlich 10 gehen als `Special` durch denselben
PNT-/Materialvertrag; unbekannte Typen bleiben explizit unsupported. Starre Bodymodelle
werden ebenfalls erfasst. MountedPlayer/Mount und Attachments behalten ihre vorhandenen Scopes.
Im Test sichtbar: alle acht Player-Races, NPCs, Mob, Metinstein 8001, Pferd 20101;
im normalen Spiel zusätzlich mehrere NPC-/Mobmodelle. Kein neues Pet-/Mount-/Meshformat.
Nicht jede benutzerdefinierte Pet-/Event-/Building-Race wurde einzeln gespawnt oder abgenommen.

## 7. Shadow-Audit

Entscheidung **Option B**: `PythonBackground::RenderCharacterShadowToTexture`,
`MapOutdoorCharacterShadow::{Create,Begin,End}` sowie dynamische Receiver werden in
Diligent nicht ausgeführt. `SetDrawShadow/SetDrawCharacterShadow` erzwingen dort aus.
Legacy behält seinen Schattenpfad. Vorhandene gebackene Texturanteile bleiben Assetinhalt.
`CGraphicShadowTexture` hat keinen aktiven externen Instanz-/Renderaufrufer im Client;
als inaktiver Helfer behalten. Keine neue Shadow Map, Blob-/Projected-Shadow-Engine oder PBR.

## 8. RenderTarget-Audit

| Pfad | Ergebnis |
|---|---|
| Auswahl/Erstellung | Viewport auf Hauptsurface, kein separates RT |
| Minimap/Atlas | Texturen und maskierte Quads, keine laufende RT-Erzeugung |
| Character Shadow | native RT-/Depth-Wechsel in Diligent deaktiviert |
| `CGraphicShadowTexture` | inaktiver Bibliothekshelfer |
| Snow Blur | private Blurflag initial FALSE, keine Aktivierung; Partikel separat M7 |
| Reflections | vorhandene statische Sphere-/Materialtexturen, keine neue Spiegel-RT-Engine |
| UI/Item/Model Preview | vorhandene Actor-/Bildpfade; kein zusätzlicher aktiver Normalclient-RT gefunden |
| Screenshot | vollständige D3D11-Surface vor Present → temporäres Staging → RGB → JPEG |
| Debug/Tools/Postprocess | kein normaler zusätzlicher sichtbarer RT-Pfad belegt |

Screenshot-Readback blockiert nur bei expliziter Aufnahme (Copy/Wait/Map), nicht jeden Frame.
Der Python-Aufruf bestätigt die Annahme des Auftrags; das spätere Speichern wird separat protokolliert.
Der Legacy-JPEG-Stub war bereits funktionslos; Diligent nutzt den bereits eingebundenen stb-Encoder.
UTF-16-Dateipfad, Rückgaben beim Schreiben/Schließen und RGB-Abmessungen werden geprüft.

## 9. Minimap/Atlas

`PythonMiniMap::Render` übernimmt die originalen 36 CPU-Vertices/neun Quads, Kamera-/UV-
Transformation und zweite Alphamasken-Textur. Marker, Atlas, Gildenflaggen und Bilder benutzen M9.
Die alten pauschalen UI-Ausschlüsse in `Render` und `RenderAtlas` sind entfernt.
Rotation, Positionen und Clipping kommen weiter aus derselben Minimap-/Fensterlogik.
Keine neue Kartenerzeugung; alle einschlägigen Snapshot-Reads laufen über `NativeStateView`.

## 10. Debug-/Tool-Pfade

Collision/Bounding-Volumes, Cube/Sphere/Cylinder, Terrain-Wireframe, Bone-`_TEST` und
Ambience-Helfer bleiben Entwicklerpfade. `game.py`-Konsole startet mit `collision=False`.
`CScreen::RenderBox3d` ist dagegen **nicht pauschal Debug-only**: UI-`RenderBox2d`
verwendet ihn ebenfalls und wird durch M9 abgedeckt.
`CScreen::RenderBillboard` hat keine Aufrufer (nicht mit SpeedTree-Billboards verwechseln).
TerrainDecal/Decal/SpeedGrass und alte Shader-/Logohelfer haben keine aktiven externen
Normalclient-Aufrufer. ScreenFilter aktiviert sich nicht; Snow-Blur ist inaktiv.
Externe SDK-/Tooldefinitionen wurden nicht umgebaut. Debugdarstellung ist in Diligent nicht zugesichert.

## 11. State-Helper und native Geräteabhängigkeit

`CStateManager` behält seine bisherigen Save-/Restore-Stacks und Legacy-Ausführung.
Nur Diligent: beim Start werden gültige native Defaults einmal in eine CPU-Sicht übernommen.
Render-/TextureStage-/Sampler-State, Material, Matrizen, Viewport, Scissor, Lichter und
benötigte Shaderkonstanten werden dort gepflegt; Snapshot-Bridges lesen nicht mehr den
laufenden D3D9-GPU-State. Gebundene Texturen/Shader haben starke COM-Besitzer.
Generierte Masken werden vor ihrer alten erzwungenen Freigabe ausdrücklich entbunden.
Fenster-Diligent verändert nicht die ungenutzte native Fullscreen-Gamma-Rampe.
`Renderer.NativeStateView` vergiftet den echten Device-State nach Initialisierung und
prüft unabhängige CPU-Snapshots, Bindings/Lifetime, Resize und fünf unterdrückte Drawvarianten.
**Weiterhin benötigt:** D3D9Ex-Gerät/Bootstrap/Reset sowie native Texturdecoder, Ressourcen-
und CPU-Skinning-Buffer. Null Drawcalls bedeutet ausdrücklich nicht „D3D9 kann gelöscht werden“.

## 12. Migrierte Restpfade / Dateien

| Bereich | Wesentliche Integrationsstellen |
|---|---|
| Preview | PythonCharacterModule, ActorRenderData, StaticObjectBridge, DiligentStaticObjectRenderer |
| Guild | MapOutdoorRender, TerrainPatch, AreaTerrain, WorldRenderData |
| Dungeon | DungeonBlock, ModelInstanceRender, DiligentEffectRenderer |
| animierte/blended Map-Things | MapOutdoorRender → DrawSpecialMapObject → gemeinsamer Actor-Meshhook |
| weitere Actors | ActorRenderBridge, ActorRenderData, Model/ModelInstanceUpdate |
| Minimap/Atlas | PythonMiniMap, UIRenderBridge, zweistufiger Material-Snapshot |
| Notice Banner | DibBar/BlockTexture → originaler CPU-DIB → M9 |
| Questdialog | PythonEventManager::RenderEventSet → vorhandener M10-Textpfad |
| Lens Flare | LensFlare, GrpScreen → WorldPart::LensFlare |
| Screenshots | PythonGraphic, TerrainPresentation, DiligentD3D11Backend, ScreenshotJPEG |
| State-Isolation | StateManager, NativeStateView und sämtliche Snapshot-Bridges |

`RenderGame` setzt die Weltgrenze nun ausdrücklich auch ohne vorangegangenes Terrain,
damit Innenräume nicht von `HasTerrain` des letzten Frames abhängen.

## 13. Bewusst nicht migriert

Neue dynamische Schatten, Debug-/Editorrenderer, inaktive Bibliothekshelfer und native
Asset-/Bufferabhängigkeiten. Keine neue Engine für Effekte, UI, Schriften, Animation oder Materialfeatures.
Die vorhandenen Upload-/Materialverträge prüfen weiter inkompatible/fehlende Assets.
Für beliebige Custom-Assets außerhalb dieser Verträge gibt es keine pauschale Kompatibilitätsgarantie.

## 14. Native Drawcalls pro Frame

Optionale Datei `config/renderer-audit.enabled` (alternativ Audit-Umgebungsvariable) aktiviert
`native-render-audit.log`. Gemessen werden tatsächliche zentrale native Draw-, State-,
Texture-Bind- und RT-Wechselaufrufe, separat von unterdrückten Aufrufversuchen.
In `world-02`, `world-03`, `normal-01` Diligent, `quest-before`, `quest-after` und `normal-quest-final`:
alle protokollierten Frames `draws=0 states=0 texture_binds=0 target_changes=0`.
Logging alle 120 Frames und bei tatsächlichen nichtnull Aufrufen; Start-/Reset-Assetaufrufe
sind keine Frame-Rendertelemetrie. Dies ist kein systemweiter GPU-Tracer.
Der sichtbare Nachweis stammt zusätzlich aus den echten Szenen und Bildern, nicht aus dem Sperren der Draws.
Ausgewertete Logzeilen: world-03 73, normal-01 198, quest-after 80, normal-quest-final 39;
keine davon mit einem tatsächlichen nichtnull nativen Renderaufruf.

## 15. Login

Originalpakete, normale Clients in privaten Arbeitsverzeichnissen, Benutzer hat sich selbst
angemeldet. Legacy PID63056 und Diligent PID60820: UI/Text/Login bestätigt.
Keine Zugangsdaten durch Testskripte, keine Originalpaketänderung.

## 16. Character Select: Tests

`preview-cpu-02`: 16 Phasen/all acht Races, Grundkörper sowie Rüstung/Haarvarianten,
Idle und Rotation; Exit 0 nach 161,1 s. Benutzer bestätigte Darstellung und dreimal
Minimize/Restore. Normaler Legacy-/Diligent-Auswahlpfad ebenfalls vom Benutzer bestätigt.
Der erweiterte Questvergleich benutzt dieselbe originale `introSelect.CharacterRenderer`-
Klasse und ergänzt nur einen deckenden Testhintergrund gegen Bildreste des leeren Fixtures.
Final `quest-after` (ON) und `preview-legacy-final` (OFF): alle 16 Phasen abgeschlossen,
Questseitenwechsel und Vorschau ohne akkumulierte Bildreste; Legacy-Fenster ebenfalls visuell geprüft.

## 17. Ingame

Benutzerbestätigung Legacy: „paasst“. Diligent: „passt, aber questsschrift wird nicht angezeigt“.
Welt, Figuren/NPCs/Mobs, Bäume, Terrain/Gebäude, Wasser, Mount, UI/Minimap wurden damit
positiv bestätigt; Dialogschrift wurde als konkreter Fehler behandelt, nicht als Abnahme übergangen.
`world-03` zeigt zusätzlich Originalkarten, sechs Actortypen, Minimap/Atlas und Sichtverdeckung.
Nicht jedes Ground-Item-/Shop-/Custom-Asset wurde in dieser normalen Sitzung einzeln benannt.

## 18. Dungeon-Test

`world-03` Phase 3 lädt die Original-Affendungeonmap, rendert tatsächliche Dungeon-Draws,
Texturen, Cave-Geometrie und Effekte. Fenster und erzeugtes JPEG wurden visuell geprüft.
Fixture-Actors stehen teilweise außerhalb begehbarer Räume; dies ist keine serverseitige Weg-/Portalprüfung.
Legacy-Offlinefolge durchlief dieselbe Map ohne Crash; der kurze Dungeonabschnitt wurde
nicht rechtzeitig separat visuell aufgenommen. Kein pixelgenauer kompletter Mapvergleich behauptet.

## 19. Guild-Test

Lokale Original-Landregistrierung im Diagnosefixture, tatsächlich nichtnull `guild_draws`,
pulsierendes weißes Bodenrechteck auf A1/B1/Guildmap, Fenster/JPEG geprüft.
Native Zweistufen-Materialparität separat GPU-getestet. Kein Kauf, keine Server-Gildenänderung.
Eine serverseitig neu erworbene Gildenfläche wurde nicht für diesen Test erzeugt.

## 20. Combat / Mount

Normaler Legacy-/Diligent-Test: Benutzer bestätigte angefragtes Kämpfen, Skills/Schadenszahlen
und Auf-/Absteigen. Keine Combat-/Motionänderung in M11.
Beobachtete bestehende Laufzeitwarnungen: `invalid idx 0` im MarkManager in beiden Backends;
Diligent zusätzlich fehlende Combo-Motiondaten `mode 9` beim Mountangriff. Keine Renderer-Assertion.
Diese Asset-/Motionwarnungen sind nicht als neu behobene Rendererfehler ausgewiesen.

## 21. UI / Text einschließlich Questregression

Minimap/Atlas, Schrift, Inventory/Skills und Nameplates wurden in der normalen Sitzung bestätigt.
**Reproduzierbarer M11-Restfehler:** `CPythonEventManager::RenderEventSet` enthielt noch
`UIExcludeScope excludeSpecialText` aus M10. Dadurch waren originale Event-Textzeilen
vom Diligent-Textrenderer ausgeschlossen, obwohl gewöhnliche Labels korrekt erschienen.
Minimaler Fix: Scope und nun unbenutztes Include entfernen; ursprüngliche aktuellen/
gespeicherten Textzeilen direkt durch bestehenden M10-Renderer zeichnen. Keine Questlogikänderung.
`quest-before` reproduziert das Fehlen offline; Legacy zeigt dieselben farbigen Zeilen.
`quest-after` zeigt nach dem Fix dieselben originalen Event-Zeilen einschließlich Farben
und wechselnden sichtbaren Zeilenbereichen, durch Fensterbild und Produktions-JPEG bestätigt.
Live-Nachprüfung `normal-quest-final`, PID63860: Benutzer bestätigt den ursprünglich
betroffenen Quest-/NPC-Dialog einschließlich angefragtem Weiterblättern, Resize und
dreimal Minimize/Restore („sieht gzt aazs+“). Anschließend normal geschlossen, Exit 0.

## 22. A1 → B1 → A1

Automatisiert in `world-03` für beide Backends, anschließend Dungeon → Guildmap → A1;
sechs Phasen vollständig, Exit 0. Diligent-Normalclient: Benutzer antwortete auf den
angefragten Gesamtwechseltest positiv, mit der separaten Questschrift-Ausnahme.
Offline-Kartenwechsel sind nicht mit Server-Teleports gleichzusetzen.

## 23. Relog / Teleport

Normaler Diligent-Auftrag umfasste Character Select → Ingame und, wenn möglich, zurück;
Gesamtrückmeldung „passt“ mit Quest-Ausnahme. Keine separat instrumentierte Netzwerk-
Teleportsequenz oder serverseitige Logauswertung. Ein echter zusätzlicher Dungeon-Teleport
und Shoptransaktionen sind nicht als geprüft ausgewiesen.

## 24. Resize

Renderer-GPUtests prüfen wechselnde Größen sowie Readback-Abmessungen.
Normaler Diligent-Test umfasste mehrfaches Vergrößern/Verkleinern, positiv gemeldet.
Native State View wurde zusätzlich mit 640er Resize und sauberem Shutdown geprüft.

## 25. Minimize / Restore

Benutzerbestätigt: dreimal Preview PID56300, Sonderwelt PID51192, normale Legacy-/Diligent-
Clients. Keine beobachteten Devicefehler oder verschwundenen Weltsysteme danach.
Finaler normaler Questtest PID63860 ebenfalls positiv bestätigt. Für den bereits automatisch
beendeten isolierten `quest-after` wurde kein separates Minimize/Restore-Ja nachgereicht;
die finale Live-Bestätigung wird nicht als dessen getrennte Bestätigung ausgegeben.

## 26. Shutdown / Exitcodes

| Belegordner | Backend / PID | Ergebnis |
|---|---|---|
| preview-cpu-02 | Diligent 56300 | Exit 0 / 161,1 s |
| world-03 | Diligent 51192 | Exit 0 / 151,2 s |
| world-03 | Legacy 53064 | Exit 0 / 150,9 s |
| world-04 | Legacy 34892 | Exit 0 / 152,4 s |
| normal-01 | Legacy 63056 | Exit 0 / 69,2 s |
| normal-01 | Diligent 60820 | Exit 0 / 397 s, Benutzer geschlossen |
| quest-before | Diligent 67536 | Exit 0 / 161,2 s |
| preview-off | Legacy 62640 | Exit 0 / 160,8 s |
| quest-legacy-check | Legacy 64832 | Exit 0 / 160,7 s |
| quest-after | Diligent 24392 | Exit 0 / 161,3 s, 16 Phasen |
| normal-quest-final | Diligent 63860 | Exit 0 / 82 s, Benutzer geschlossen |
| preview-legacy-final | Legacy 45548 | Exit 0 / 160,7 s, 16 Phasen |

Frühere fehlgeschlagene/überholte Entwicklungstests bleiben in ihren Belegverzeichnissen;
sie sind keine erfolgreichen Endtests. Es wurde kein Nutzerclient zwangsweise beendet.

## 27. Ressourcen / Stabilität

In den abgeschlossenen Diligent-Sonderwelt-/Normal-/Previewläufen alle überwachten
Shutdownzähler 0: Text-/UI-Texturen und -Buffer, World-/Wasserressourcen, Effekte/Partikel,
Trees, Actors, Attachments, Mounts, statische Objekte. GPUtests prüfen ihre jeweiligen Besitzer.
Kein beobachteter neuer Lifetime-/Devicefehler. Normal-01: Private Bytes nach Cache-Warmup
ungefähr 714 MB, zuletzt etwa 706–707 MB, Handles 1096 stabil. World-03 zuletzt etwa
483 MB/1050 Handles. Das ist ein begrenzter Mehrminutentest, kein Langzeit-Leakbeweis.
Quest-Nachtest und normaler finaler Client: ebenfalls alle überwachten Shutdownzähler 0.
Letzter normaler Client: am Ende ca. 638 MB, 1095 Handles, kein anhaltender Anstieg.

## 28. Release ON/OFF

Visual Studio 2022, Release x64. ON `jpeg-world-build.log` erfolgreich vor Questfix.
OFF `release-off-final.log` und ON `release-on-final.log` erfolgreich einschließlich Questfix/Gamma-Gate.
Die finale getestete normale ON-Kopie stimmt mit `build/bin/Release/Metin2_Release.exe`
überein (SHA256 `B078FE866357B49AB1B169CC8BD0BD8EB2306846BD691287A523B36AC6C17BEF`).
OFF-Vergleichskopie: `5889CA316B68133DEBCD5D4E69ACF2B979C1596070EC979DB5466A772A0593ED`.
Vorhandene Python/zlib-PDB-Linkerwarnungen bleiben; keine Compilerfehler.

## 29. Testsuite

ON vor Questfix: `suite-on-01.log` **17/17**, 477,72 s.
`jpeg-gpu.log`: **13/13** Renderer-Tests; einschließlich 70 nativer Effekt-/Zweistufen-
Materialparitätsfälle, Vorgänger-State-Variation, D3D11-Readback und JPEG-Roundtrip.
Neue Tests: `Renderer.NativeStateView`, `Renderer.ScreenshotJPEG`.
OFF `suite-off.log`: **15/15**, 457,15 s, einschließlich Questfix/Gamma-Gate.
Final ON `suite-on-final.log`: **17/17**, 457,78 s, einschließlich aller **13 Renderer-Tests**.
`Renderer.TerrainGpuParity` 15,87 s; `NativeStateView`, `ScreenshotJPEG`, beide Backend-
Lifecycle-Tests und alle Policytests bestanden. Testläufe jeweils Exit 0.
Kein paralleles Reconfigure/Build während einer laufenden CTest-Suite.

## 30. Legacy Regression

Standardargumente bleiben unverändert; normaler Start ohne Rendereroption erfolgreich,
Benutzer bestätigte Welt/Character Select/UI/Text/Minimap/Mount/Combat und Shutdown.
OFF-Preview benutzt dieselben originalen Texte. Legacy-Screenshots melden im lokalen
Backbufferpfad `LockRect 0x8876086c`; dies und der alte JPEG-Stub bestanden schon vorher
und wurden nicht nebenbei umgebaut. Keine andere Originalkonfiguration überschrieben.

## 31. Blocker / Grenzen der Freigabe

Kein offener reproduzierter Rendererfehler in den dokumentierten Testszenen. Questdialogfehler ist
reproduziert, minimal korrigiert, offline und vom Benutzer im ursprünglichen Dialog bestätigt.
Nicht als vollumfänglich assetgeprüft ausgeben: jedes Custom-Modell, jeder Shop/Pet/Eventtyp,
alle Dungeonmaps oder animierte/blended Map-Things, die in diesen Karten nicht vorkamen.
Der gemeinsame Material-/Geometriepfad für Letztere ist implementiert, aber kein tatsächlicher
Submit dieses seltenen Assettyps in den ausgewerteten normalen Sitzungen belegt.
Keine bekannten übrigen sichtbaren D3D9-Draws in den geprüften Szenen. Dynamische Schatten
sind bewusst aus, native D3D9-Ressourcenabhängigkeiten bestehen. Daher keine Empfehlung,
D3D9 bereits zu löschen oder Diligent ungeprüft für sämtliche Custom-Inhalte zu erzwingen.

## 32. Empfehlung für M12 / STOP

M11 für die dokumentierten Szenen technisch abnehmen; finale ON/OFF-Suiten und
Quest-Nachprüfung sind erfolgreich. M12 separat beauftragen: verbliebene Asset-/Langzeittest-Matrix schließen und
Default-Umstellung mit explizitem Fallback-/Rollbackplan bewerten. Geräte-/Assetabhängigkeiten
vor einem D3D9-Ausbau separat entkoppeln. Keine dieser M12-Arbeiten wurde hier begonnen.

## Git-Diff und Nachweisablage

Der Diff umfasst Restpfad-Bridges, CPU-State-Isolation, Diligent-Zweistufenmaterial,
Readback/JPEG, Diagnosezähler sowie Audit- und Regressionstests. Bestehende Legacy-
Implementierungen bleiben parallel; keine moderne Material-/Schatten-/Animationsengine.
64 bereits versionierte Dateien verändert: 830 Einfügungen, 161 Löschungen.
Zusätzlich 12 neue Dateien: drei Audit-/Berichtdateien, NativeStateView, JPEG-Helfer
mit Header, zwei C++-Tests, Diagnoseprobe, Auditskript und zwei Python-Szenarien.
Die aktuelle Questkorrektur selbst entfernt lediglich den Ausschluss-Scope samt unbenutztem
Include in `PythonEventManager.cpp`; die übrigen Änderungen gehören zur gesamten M11-Umsetzung.
Belege liegen unter `build/milestone11/<Testname>/`; Build-/CTestlogs daneben.
`git diff --check` nach abschließender Dokumentation: erfolgreich, keine Whitespacefehler.
Alle neuen maßgeblichen Integrationsstellen sind sparsam mit `ZiiNAN` markiert.
