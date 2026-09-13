# Milestone 13A – Ressourcenentkopplung

Stand: 2026-09-13. Ausgangsbasis: `f4c5ff4` (M12), Source-Worktree vor Arbeitsbeginn sauber.
Nur M13A. Kein Legacy-Renderer, Device, StateManager, Header oder Library entfernt.
**Implementiert, aber noch nicht zur Produktion freigegeben: normale Client-/Session-Abnahme offen.**
Frühere M12-Bestätigungen zählen nicht als M13A-Regression.

## 1. Vollständige Ressourcenfundstellen

Vor Implementierung erstellt: [Audit](milestone13a-audit.md) und
[Fundstellen vorher](milestone13a-resources-before.csv), 409 Quelltextstellen in 86 Dateien.
Der CSV enthält Datei/Zeile, Klasse bzw. Ressourcenfamilie, exakte Member-/Call-Statement,
nativen Typ, Erzeuger, Besitzer, Leser, Erreichbarkeit, Kategorie und Aktion.
LP-Aliase, D3DX-Meshes, native Erzeuger und Wrapper-Getter sind eingeschlossen.
Reine SDK-Typdefinitionen sind keine Client-Besitzer; deren Header-/Link-Abhängigkeiten bleiben E.
Kein Client-Owner/Erzeuger für Query oder StateBlock gefunden.

[Fundstellen nachher](milestone13a-resources-after.csv): 356 Stellen in 75 Dateien.
Dies ist kein Zielwert für Texttreffer: optionale Legacy-Member sollen ausdrücklich bleiben.
`tests/Renderer/AuditResources.ps1 -CheckGuards` kontrolliert zusätzlich alle 49 gefundenen
nativen Erzeugungsstellen gegen die neue Guard-Telemetrie. Erfolgreich ausgeführt.

## 2. Kategorien A–E

| Kategorie | Vorher-Snapshot | Nachher, präzisierte Zuordnung | Behandlung |
|---|---:|---:|---|
| A | 292 | 229 | Gemeinsame Ressourcenfamilien: neutrale Quelle/Identität, optionale native Seite |
| B | 79 | 82 | Separater Legacy-Pfad bleibt erhalten |
| C | 21 | 26 | Native Referenztests/Diagnose bleiben erhalten |
| D | 16 | 17 | Unbenutzte/auskommentierte Helper nicht gelöscht |
| E | 1 | 2 | Inaktive Build-/Headerpfade nicht gelöscht |

Die Nachher-Kategorien präzisieren auch gemischte Dateien: fünf Shadow-/Backup-Member
in MapOutdoor sind B, die beiden LensFlare-Texturbindungen A, FlyTrace-Texturcode ist
auskommentiert (D), SpeedGrass nicht aktiviert (E). Der vorherige Snapshot bleibt als
historische Erhebung erhalten; seine familienweite Zuordnung war dort gröber.
A im Nachher-CSV bezeichnet weiter die entkoppelte gemeinsame Familie, **nicht** eine
behauptete weiterhin notwendige native Ressource. Klassennamen in Sammelfamilien sind
gruppiert; das konkrete Member/Statement steht separat unverändert im CSV.

## 3. Texturabhängigkeiten

Betroffen: CGraphicTexture/ImageTexture, Image/SubImage, FontTexture, BlockTexture,
Granny-Materialien, Terrain-Splat/Markierung/Minimap, Tree-/Effect-/World-/UI-/Text-Bridges.
Native Pointer wurden als Existenztest, Identität, Atlas-Batch-Key oder Formatquelle benutzt.
Diese Leser verwenden jetzt `TextureBinding` bzw. `TextureResource`.
Die nativen Getter bleiben für Legacy und Referenztests verfügbar, sind im neutralen Modus null.

## 4. Texture-Metadata-Entkopplung

`Renderer/ResourceData.h`: `TextureDesc {width,height,mipLevels,format}` und `TextureResource`
besitzen Assetnamen, Mip-Pixel, Strides und Revision. `TextureSource.cpp` decodiert über
den bestehenden CPU-DDS/STB-Pfad und kopiert geliehene Decoderdaten in eigenen Speicher.
Kein GetLevelDesc/GetLevelCount/LockRect eines D3D9-Objekts als Diligent-Datenquelle.
StaticObjectBridge liest die neutrale Beschreibung; sein GetLevelDesc-Zweig bleibt Legacy.
Font-/DIB-Pixel und Terrain-Alpha entstehen weiter aus ihren ursprünglichen CPU-Produzenten.
Die fünf Alpha-Mips einschließlich A8/A4-Quantisierung sind gegen eine unabhängige Referenz geprüft.

## 5. Vertexbuffer-Abhängigkeiten

`CGraphicVertexBuffer` besitzt im neutralen Modus einen `CpuBuffer`; Lock/Copy/Unlock
arbeiten auf dessen Bytes. Granny-Skinning und Original-Geometrieproduzenten bleiben gleich.
ModelInstanceRender prüft Geometrie statt zwingend einen nativen VB. DungeonBlock liest
Bytekapazität aus dem Modell-Wrapper statt GetDesc und nutzt dessen CPU-Lock.
SpeedTree behält originale Geometrie/LOD/Strip-Daten, erzeugt aber keine nativen VBs.
PDT-/Software-Terrain-Ringbuffer werden übersprungen; vorhandene CPU-Vertices gehen an
die bisherigen Diligent-Submissions. Snow schreibt CPU-Vertices. Water prüft `IsEmpty()`
statt den nativen Zeiger, damit vorhandene CPU-Wassergeometrie nicht ausgeblendet wird.

## 6. Indexbuffer-Abhängigkeiten

`CGraphicIndexBuffer` verwendet denselben neutralen CPU-Lock-Vertrag. Auch die seltenere
`Create(faceCount, faces)`-Überladung benutzt jetzt den Wrapper-Lock statt direkten COM-Zugriff;
ein eigener Test prüft die resultierende Indexfolge. Native Default-IBs und Tree-IBs werden
im neutralen Modus nicht erzeugt. Diligent lädt Original-Indizes direkt aus CPU-Daten.

## 7. RenderTargets und Surfaces

DiligentD3D11Backend besitzt unverändert D3D11-Swapchain, RTV und DepthBuffer.
Der neue neutrale GPU-Test zeichnet, liest D3D11-Pixel und ersetzt diese Targets per Resize.
MapOutdoor-Shadow-Surfaces bleiben Legacy-only; dynamische Diligent-Schatten bleiben aus.
Snow-Blur bleibt sein deaktivierter Legacy-Hilfspfad. GrpShadowTexture ist unbenutzter Helper.
Legacy-Screenshot-Surfaces bleiben Legacy; Diligent-Screenshots verwenden eigene D3D11-Stagingdaten.
**Ausnahme:** das ausdrücklich behaltene D3D9Ex-Device erzeugt selbst implizite Backbuffer/
Auto-Depth-Surfaces. Diese sind keine Asset-/Upload-Quelle. Reset kann sie intern ersetzen.
Sie sind nicht Teil der gezählten 49 expliziten Ressourcen-Erzeugungspunkte.

## 8. Shader und Declarations

Startup-PT/PNT/PNT2-Declarations, Tree-Declarations/Shader und Debug-D3DX-Meshes werden
im neutralen Modus nicht angelegt. Diligent-Pipelines verwenden ihre vorhandenen eigenen
Shader/Input-Layouts. Optionale native Member und tote Shader-Wrapper bleiben für Legacy/E.
Die numerische StateManager-Snapshot-Logik wird nicht ersetzt.

## 9. Ownership

| Ressource | CPU-/Source-Besitzer | Diligent-GPU-Besitzer | Legacy-GPU-Besitzer |
|---|---|---|---|
| Image/SubImage | ImageTexture/shared TextureResource | Image bzw. bisheriger Subsystem-Cache | ImageTexture-COM |
| Font | Font-Atlas und ImageTexture-Seiten | Font-Texturcache | Font-Seiten-ImageTexture |
| DIB/Quest-Banner | DIB und BlockTexture-Source | BlockTexture-UI-Handle | BlockTexture-COM |
| Terrain-Alpha/Markierung | CTerrain/TerrainAlphaImage | Terrain-/World-Renderer, bestehende Handles | CTerrain-Splat/Markierung |
| Modell/Actor/Dungeon | Modell-/Deform-Wrapper und bestehende Renderdaten | Actor-/Static-/Dungeon-Cache | VB-/IB-Wrapper |
| Tree | SpeedTree-Originaldaten/TreeRenderData | Tree-Renderer/Renderdaten | Tree-Basis; Instanzen leihen |
| Effect/Water/UI | Bestehende CPU-Drawdaten und Assets | Jeweiliger Renderer | Bestehende Legacy-Wrapper |
| Binding | shared CPU-Source, Save/Restore hält Besitzer | Kein GPU-Eigentum | Native Referenz, vorhandene Legacy-Regeln |

Kein neuer globaler ResourceManager. Diligent-Handles bleiben in ihren vorhandenen
Subsystemen, nicht zwangsläufig direkt im neutralen Source-Objekt. Kein Backend besitzt
ein GPU-Handle des anderen. CPU-Texturbatches werden am Draw-Ende geleert, damit ihre
neuen shared Besitzer keine Fontseiten bis zum nächsten Draw/Prozessende festhalten.

## 10. Lifecycle

Load/Decode -> eigene CPU-Daten -> synchroner Upload -> bestehender GPU-Cache -> Draw.
Map-/Asset-Unload setzt Source-/Subsystem-Owner zurück; Save/Restore darf einen Source
so lange halten, wie er logisch gebunden ist. Unbind löscht die Source auch dann, wenn
der optionale native Pointer schon null war. Font-/DIB-Updates erhöhen die Source-Revision;
ihre bestehenden expliziten Uploads aktualisieren die GPU-Seite.
Image-CPU-Daten überleben DestroyDeviceObjects/CreateDeviceObjects. Buffer-Recreate
legt CPU-Speicher neu an, originale Modell-/Terrain-Produzenten befüllen ihn wie zuvor.
Diligent-Resize ersetzt seine Swapchain-Targets, nicht seine Assetquelle.
Shutdown: bisherige Pool-/Map-/ResourceManager-Freigaben, Bindings und Backend-Shutdown.
Unit-/GPU-Lifecycles und isolierte Welt-/Dungeon-/Map-Lifecycles geprüft;
normale Map-/Relog-Lifecycles noch offen.

## 11. Backend-Trennung

WinMain setzt `neutralResources` einmal aus den bereits validierten StartupOptions,
vor Main/App/Device/Asset-Aufbau. ON ohne Argument bleibt Diligent; explizit Legacy und
OFF bleiben Legacy. Kein Hot-Switching und kein stiller nativer Ressourcen-Fallback.
`TextureBinding` enthält bewusst noch den optionalen Legacy-Typ; der neutrale Datenkern
selbst benötigt kein D3D9-Interface. Das Entfernen der Headers/Enums ist nicht M13A.

## 12. Diligent-Erzeugungspfad

Pack-/Decodebytes, Atlas/DIB-Pixel bzw. originale CPU-Geometrie -> neutrale Source/View
oder bereits bestehender direkter Assetloader -> Diligent UploadTexture/UploadGeometry/
UploadVertices/UploadIndices -> D3D11-Ressource. Keine D3D9-Erzeugung mit anschließendem Copy-Back.
Der Ressourcentest betreibt den echten Diligent-Renderer bei aktivem nativen Allocation-Guard.

## 13. Legacy-Erzeugungspfad

Dieselben bisherigen Wrapper im Legacy-Modus -> bisherige native Create*/D3DX*-Aufrufe.
Die neue Telemetrie zählt und ruft dort die ursprüngliche Funktion auf; sie blockiert nicht.
Material-/Numeric-State-, Renderreihenfolge und Legacy-Fallback bleiben erhalten.
Die Face-IB-Überladung benutzt funktional denselben Lock/Unlock nun über den Wrapper.

## 14. Verbleibende gemeinsame Klassen

ImageTexture, FontTexture, BlockTexture, VertexBuffer, IndexBuffer, GrannyMaterial,
CTerrain, StateManager und Render-Bridges bleiben gemeinsam. Native Member dürfen im
neutralen Modus null sein. Existenz/Identität/Metadata kommt dort aus CPU-/Assetdaten.
CStateManager bleibt weiterhin zentrale numerische Kompatibilitätsschicht.

## 15. Native Erzeugungstelemetrie

`NativeResourceAudit.h` überwacht alle 49 expliziten nativen Erzeugungsstellen einschließlich
Cube/Volume und D3DX-Helper. Bei neutralem Modus wird ein versehentlicher Versuch gezählt,
vor dem Allocator abgewiesen und höchstens einmal pro Kategorie mit Datei/Zeile gemeldet.
WinMain schreibt nach Main `native-resource-audit.log` mit attempts/succeeded/blocked pro Typ,
Device-Erzeugungen und lebenden neutralen Source-Objekten. Kein Frame-Log-Spam.

Im erweiterten ResourceSource-GPU-Test: attempts=0, creations=0, blocked=0;
CompatibilityDeviceCreations=1; SourceTextures=0, SourceBuffers=0 nach Shutdown.
Erst anschließend provoziert der Test absichtlich genau einen blockierten Fake-Allocator,
um den Guard selbst zu prüfen. Dessen Testordner-Violation-Log ist kein Produktionsfehler.
**Normaler Defaultstart:** noch keine M13A-Messung. D3DX-Mesh-interne VB/IBs zählen als
ein Helper-Aufruf; Device-implizite Surfaces sind separat genannt, nicht verschwiegen.
Die Zähler sind kumulative Create-Zähler, kein universeller COM-Live-Object-Tracker.

## 16. Default-Diligent-Test

Automatischer neutraler CPU-/D3D11-GPU-Test bestanden; das ist kein normaler Login.
Der isolierte Weltstart `world-initial` wurde vor Prozessstart durch Windows/User abgebrochen.
Kein PID, kein Client-Exitcode, keine Ingame-Evidenz daraus. Aktuelle private ON-Fixtures:
`build/milestone13a/world-on`, `preview-on`, `normal-on` (Originalpakete für normal).
Keine Produktivpakete oder Runtime-EXE überschrieben.

Erster echter Default-Weltlauf `world-on`, PID 66688: A1/B1/A1 und dreimaliges
Minimize/Restore visuell bestätigt. Beim Dungeon-Schritt vorzeitig beendet (76,2 s,
Exitcode 0), aber mit Renderer-Fehlermeldung: **kein bestandener Gesamtlauf**.
Alle expliziten nativen attempts/creations/blocked=0, SourceTextures/SourceBuffers=0;
alle bestehenden Subsystem-Shutdownzähler=0. Ursache im neuen Audit-Code: eine zu strenge
Gleichsetzung der FVF-Allokationsgröße (48 Bytes) mit dem tatsächlichen Dungeon-PNT2-Drawformat
(40 Bytes). Die ursprüngliche reine Kapazitätsprüfung bleibt nun erhalten und benutzt
CPU-Wrapper-Metadata; kein Granny-/Formatumbau. Ein CPU-Test deckt diese Abweichung ab.
Anschließende Startversuche von `world-corrected/default-bin` wurden zunächst vor Prozessstart
abgebrochen (Starter-Exit 1, kein Client-PID); daraus ergibt sich kein Client-Testergebnis.
Aktuelle ON-Fixtures mit Korrektur: `world-corrected`,
`preview-corrected`, `normal-corrected`; OFF-Fixtures: `world-off`, `normal-off`.

**Korrigierter vollständiger Weltlauf am 2026-09-13 bestanden:**
`world-corrected`, PID 58544, Defaultstart ohne Rendererargument;
`Renderer: Diligent D3D11`, `Selection=default DiligentCompiled=1`.
A1 -> B1 -> A1 -> monkeydungeon -> guild_01 -> A1,
`completed phases=6`, automatischer Shutdown nach 151,4 s mit Exitcode 0.
Dungeon-Geometrie ist in der Sichtprüfung vorhanden, die Telemetrie zeigt dort unter
anderem `dungeon_draws=94`; der vorherige Abbruch ist in diesem Wiederholungslauf behoben.
Welt, Figuren, Bäume, Wasser und Minimap sowie dreimaliges Minimize/Restore vom Benutzer
mit „passt“ bestätigt. Zusätzliche Sichtprüfung über Computer Use für B1, Dungeon und
Gildenkarte; sechs vom Test gespeicherte Karten-Screenshots vorhanden.
`log/syserr.txt` ist leer; keine Error-/Failed-/Assertion-/Device-Removed-Meldung in den
Testlogs und kein `native-resource-violations.log`.
Alle expliziten nativen attempts/creations/blocked=0; CompatibilityDeviceCreations=1
mit der unter 7 beschriebenen Ausnahme impliziter Targets. Alle protokollierten nativen
draws/states/texture_binds/target_changes=0. SourceTextures=0 und SourceBuffers=0
sowie alle vorhandenen Subsystem-Shutdownzähler=0.
Evidenz: `build/milestone13a/world-corrected/default-exit.txt`,
`default-resources.csv` und `default-runtime/{special-world-test,renderer-startup,
native-resource-audit,native-render-audit,terrain-renderer}.log`.
Nur der vorbereitete Test wurde gestartet; keine neue Source-/Binary-/Packänderung.

Offene normale Abnahme: Login, Charakterauswahl/Ingame, Terrain, Gebäude, Bäume,
Actors/Mounts, Effekte/Wasser, UI/Text/Nameplates/Schadenszahlen, Bodenitems und Questdialoge.

## 17. Expliziter Legacy-Fallback

Renderer.LegacyD3D9 und native GPU-Paritätstests bestanden (ON).
Normaler M13A-Login mit `--renderer=legacy-d3d9` noch offen; keine Freigabe aus alten Tests übernommen.

## 18. Diligent-OFF

Release-OFF erfolgreich, alle 12 OFF-Renderer-Tests und die vollständige OFF-Suite
(16/16, Exit 0) erfolgreich. OFF bleibt Legacy default; kein Diligent-Code wird vorausgesetzt.

## 19. Mapwechsel

Isolierter M13A-Welttest bestanden: A1 -> B1 -> A1 -> Dungeon -> Gildenfläche -> A1,
alle sechs Phasen abgeschlossen (PID 58544). Ein echter Ingame-Session-/Teleportwechsel
mit normalem Client bleibt offen; der isolierte Lauf ersetzt diesen nicht.

## 20. Relog

Ingame -> Charakterauswahl -> Ingame für M13A noch offen.
Keine Account-/Serverdaten automatisiert geändert; Anmeldung erfolgt durch den Benutzer.

## 21. Resize

ResourceSource: CPU-State-Reset am behaltenen Device und Diligent-Targetgrößen
400x300 -> 640x360 -> 320x240, jeweils mit Source-Upload/Draw/Pixellesen, bestanden.
Bestehende Backend-/Paritätstests ebenfalls bestanden. Mehrfaches normales Client-Resize offen.

## 22. Minimize/Restore

Automatische 0x0-Suspend-/Restore-Zyklen bestanden, ohne native Ressourcenversuche.
Echtes dreimaliges Minimize/Restore im korrigierten Weltfenster (PID 58544) vom Benutzer
bestätigt; Darstellung korrekt. Normale Client-/vollständige Ingame-UI-Abnahme bleibt offen.

## 23. Shutdown

ResourceSource und alle 14 ON-Renderer-Tests: Exitcode 0, inklusive wiederholtem Backend-Shutdown.
Korrigierter vollständiger Welttest PID 58544: automatischer Shutdown, Exitcode 0,
151,4 s, sechs abgeschlossene Phasen und alle Subsystem-Shutdownzähler=0.
Normale Default-/Legacy-Client-Exitcodes fehlen noch. Der abgebrochene Starter hatte
Shell-Exitcode 1; das ist ausdrücklich kein Client-Absturz und kein erfolgreicher Shutdown.

## 24. Offene Ressourcen

ResourceSource nach Shutdown: SourceTextures=0, SourceBuffers=0, Diligent-Texturhandle
vor Renderer-Zerstörung freigegeben. Backend-/Paritytests prüfen ihre bestehenden GPU-Lifetimes.
Vollständiger korrigierter Weltlauf: SourceTextures=0, SourceBuffers=0;
Text/UI/Water/World/Effect/Tree/Actor/Attachment/Mount/Object-Shutdownzähler jeweils 0.
31 Prozessmesspunkte über 151,4 s: Private Bytes maximal 468,3 MiB beim Dungeon-Laden,
danach Rückgang; letzter Messpunkt 398,2 MiB. Erste A1-Phase bei 25,1 s: 373,6 MiB.
Working Set maximal 382,3 MiB, zuletzt 352,0 MiB. Handles nach initialem Laden
1054–1063, zuletzt 1052. Kein fortlaufend monotoner Anstieg im gesamten Durchlauf;
die höhere abschließende RAM-Basis ist ohne wiederholten Langlauf nicht als Leak oder
als erwiesene Leakfreiheit zu bewerten. Zusätzliche CPU-Mips und bestehende Caches bleiben
bei einer Langlaufmessung zu berücksichtigen.
Keine pauschale Aussage über alle normalen Clientressourcen oder Legacy-COM-Leaks.

## 25. Builds ON/OFF

Release ON: erfolgreich. ON-Testkopien nach Dungeon-Korrektur, SHA256:
`F0342063D36E2482A1128AEA165FF014E9E4AFCC70FEA4753D771441DA6FC598`.
Logs: `build/milestone13a/release-on-final.log`, `release-on-neutral-gpu.log`,
`resource-gpu-test-build.log`. Vorhandene Python/zlib-PDB-Warnungen nicht als Fehler gewertet.
Dungeon-Korrektur: `release-on-dungeon-fix.log`, Exit 0.
Release OFF: `release-off.log`, Exit 0. EXE SHA256:
`2AEA8F4D120FBB72AE1DBCC945DA26CB308F80289CBD5AF1EA80399B1FB2F79F`.
Abschließend denselben Source wieder mit ON gebaut, `release-on-restored.log`, Exit 0.
Arbeitsbuild steht auf ON; SHA256 der neu verlinkten EXE:
`C02DB4B6AE949A31844B20166CFD361D7FE24589FFCBBE27690FA3092B7ADB26`.
Keine Produktiv-Runtime-EXE ersetzt. Die privaten korrigierten Testkopien bleiben erhalten.

## 26. Testsuite

| Prüfung | Ergebnis / Evidenz |
|---|---|
| Vollständige ON-Suite vor letzten Reviewkorrekturen | 18/18, Exit 0, `suite-on.log` |
| ON-Renderer nach Dungeon-Fix | 14/14, Exit 0, `renderer-on-dungeon-verified.log` und `renderer-on-dungeon-details.log` |
| ON-Renderer nach Wiederherstellung des Arbeitsbuilds | 14/14, Exit 0, `renderer-on-restored.log`, `renderer-on-restored-details.log` |
| Zweite vollständige ON-Suite | 18/18, Exit 0, `suite-on-final.log`; unmittelbar vor Dungeon-Korrektur, danach alle 14 Renderer erneut geprüft |
| OFF-Build / Renderer | Exit 0 / 12 von 12, `release-off.log`, `renderer-off.log`, `renderer-off-details.log` |
| Vollständige OFF-Suite | 16/16, Exit 0, `suite-off.log`, `suite-off-details.log`, 412,92 s |
| Neuer Texture-/Buffer-/Alpha-/Ownership-/GPU-Test | Bestanden, in Renderer.ResourceSource |
| Native Allocation-Guard Coverage | 49/49 gefundene Erzeugungsstellen |
| Korrigierter Default-Welttest | Bestanden, PID 58544, sechs Phasen inkl. Dungeon/Gildenkarte, 151,4 s, Exit 0, native creations=0, Shutdownzähler=0; `world-corrected` |
| Normale Default-/Legacy-Sessionabnahme | Offen |

Keine Tests deaktiviert. Ein Zwischenlauf des neuen GPU-Tests las Screenshots nach EndFrame
und scheiterte am vorhandenen Capture-Vertrag. Nur der Test wurde auf Capture vor EndFrame
korrigiert; der anschließende vollständige Renderer-Lauf ist grün. Fuzz-/Zstd-Tests unverändert.

## 27. Bekannte Restabhängigkeiten

D3D9Ex-Device/implizite Targets; D3D9-Headers, Libs, Enums, FVF-Größenberechnung,
Capability-Abfragen und numerische StateManager-Kompatibilität; optionale Legacy-COM-Member;
native Referenztests; Legacy-Shadow/Readback; inaktive Logo-/Shader-/Shadow-/Grass-Helfer.
Keine dynamischen Diligent-Schatten, kein GPU-Skinning, keine neue Materialfunktion.
Bestehende Diligent-Caches laden teils dieselben Assets nochmals direkt; deren Vereinheitlichung
ist kein notwendiger Bestandteil dieser Entkopplung und wurde nicht zusätzlich angegangen.

## 28. Empfehlung zu M13B und Stopp

**Noch keine Freigabe für M13B.** Der korrigierte isolierte Weltlauf ist bestanden.
Erst die normalen Client-/Session-Nachweise abschließen, einschließlich native creations=0,
Subsystemressourcen=0 und Exitcode 0.
Danach kann M13B separat geplant werden. In dieser Änderung wird M13B nicht begonnen.

## Git-Diff / Scope

Gezielte Erweiterung bestehender Texture-/Buffer-Wrapper und Binding-Leser statt ResourceManager-
Rewrite. Neue Dateien: ResourceData, TextureBinding, TextureSource, NativeResourceAudit,
ResourceSourceTest und Audit-/Berichtsdateien. Kein bestehender Renderer-Drawcall migriert.
Granny-/CPU-Skinning-Algorithmen, Gameplay/Netzwerk und Original-UI-/Packdaten unverändert.
Zwei bereits gemischt codierte SpeedTree-Kommentarzeilen wurden für die Bearbeitung nach UTF-8
normalisiert; keine Codeänderung daraus. `git diff --check` erfolgreich.
56 bestehende Dateien geändert (424 hinzugefügte/215 entfernte Zeilen), zusätzlich neue
Source-/Test-/Auditdateien; die großen CSVs sind Dokumentation, kein weiterer Renderer-Umbau.
Kein Commit/Push, keine Löschung. Vorhandene Runtime-Änderung `config/channel.inf` unangetastet.

## Wiederaufnahme der offenen Live-Abnahme

Alle folgenden Testordner sind private Kopien. `world-corrected/default` ist erfolgreich
abgeschlossen und darf nicht überschrieben werden; die übrigen unten genannten Läufe
sind noch offen. Startfreigaben und Login bleiben beim Benutzer. Aus dem Source-Verzeichnis:

```powershell
# Charaktervorschauen und Quest-/Bannertext
tests/Renderer/run_effects.ps1 -TestRoot build/milestone13a/preview-corrected -Backend default
# Normaler Defaultstart mit Originalpaketen: gesamte Ingame-/Relog-/Map-/Fenster-Abnahme
tests/Renderer/run_effects.ps1 -TestRoot build/milestone13a/normal-corrected -Backend default
# Expliziter Legacy-Fallback desselben ON-Builds
tests/Renderer/run_effects.ps1 -TestRoot build/milestone13a/normal-corrected -Backend legacy
# Legacy-only OFF ohne Rendererargument
tests/Renderer/run_effects.ps1 -TestRoot build/milestone13a/world-off -Backend default
```

Nacheinander ausführen, Start-/Exit-/Phasen-/Fehler-/Ressourcenlogs zusammen bewerten.
Für einen bestandenen Weltlauf müssen **alle sechs Phasen** abgeschlossen sein, nicht
nur der Prozess Exitcode 0 liefern. Normale Sessions müssen die unter 16–24 genannten
Punkte prüfen. Ohne diese Evidenz keine abschließende M13A-Freigabe.
