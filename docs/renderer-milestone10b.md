# Milestone 10B – weltgebundene Texte

Stand: 13.09.2026. Ausschließlich M10B; Legacy D3D9Ex bleibt Standard. Kein neuer Font, keine neue Projektion/Animation, kein D3D9-Removal. Die vor Implementierung erstellte [Analyse mit 30 Fragen und Kategorienmatrix](renderer-milestone10b-analysis.md) beschreibt die nativen Pfade.

## Umsetzung und Grenzen

| Nr. | Abschlussfrage | Ergebnis |
|---|---|---|
| 1 | Nameplate-Architektur | `CPythonTextTail::Render` verwendet einen begrenzten `FloatingTextScope` statt des bisherigen Diligent-Ausschlusses. Native Textinstanzen, Atlasseiten, Boxen und Marken gehen durch die bestehenden M9/M10A-Binder. |
| 2 | Projektion | Unverändert: Ownerposition + gespeicherte Höhe → `CScreen::ProjectPosition`/D3DXVec3Project mit nativer View/Projection/Viewport → floor(X/Y). Keine nachträgliche Renderer-Verschiebung. |
| 3 | Player | `CInstanceBase::AttachTextTail/RefreshTextTail` → `RegisterCharacterTextTail` → `Update/Arrange/Render`. Name, Titel und Level behalten Originalwerte. |
| 4 | NPC/Mob/Boss | Derselbe Actor-/Tail-Pfad. Native Typ-/Farb- und Sichtbarkeitsregeln bleiben erhalten. Automatischer Originalclient verwendet Händlerinnen, Hund und Orc Lord. |
| 5 | Gilde | Vorhandener CPythonGuild-Name und `CGraphicMarkInstance`-Atlas; kein weiterer Renderer. Gildenname im normalen Ingame-Client gesehen. Native Mark-/Guildtext-Pixel und Mark-Owner-Freigabe im finalen GPU-Test mit Testatlas geprüft. Offline-Fallback „Noname“ ist kein Nachweis eines echten heruntergeladenen Serveremblems. |
| 6 | Shop | Originales `PrivateShopAdvertisementBoard`, UI_BOTTOM/ThinBoard/TextLine, `chr.GetProjectPosition(vid,220)`. Bereits M9/M10A, keine Produktionsänderung. Im Legacy-OFF- und Diligent-1920-Test sichtbar geprüft. Offline-Harness aktualisiert das unveränderte Board mit aktuellen Weltmatrizen, da dessen Netzwerk-GamePhase fehlt. Kein Kauf/Verkaufstest. |
| 7 | Bodenitems | Native ThingInstance-Owner, Offset −10, Register/Delete/SetOwnership, Originalbox und überlappende Labels. Native Owner-Zeile und Auswahlfarbe im GPU-Test. Kein neuer Stack-/Loot-Pfad. |
| 8 | Damage | `RecvDamageInfoPacket` → `AddDamageEffect` → Queue/`ProcessDamage` → `SetEffectTextures/CreateEffect` → EffectLib-Partikel → bestehender M7-Binder. Ausschließlich der frühere `damagevalue/`-Ausschluss wurde entfernt. |
| 9 | Damage-Typen | Normal/Target, alternierende Self-Damage, Krit und DB über Originalzahlen; Dodge/Block über vorhandene Miss-Assets. Kein Heal-Zahlentyp. Auskommentierter Krit-/Poison-Spezialzweig und deaktivierte Nichtziel-Ausgabe bleiben deaktiviert. |
| 10 | Animation | Unveränderte MSE-Emitter-, Bewegungs-, Gravity-, Scale- und Lifetime-Kurven. GPU-Test lädt die fünf tatsächlich benutzten Originalscripts und DDS-Overrides, wiederholt Target-/Self-Instanzen nach Ablauf und vergleicht 216 Bilder. |
| 11 | Farbe/Alpha | Originale Vertex-/Materialfarben, Damage-DDS und native Alpha-Kurven. Keine neue Transparenz-/Farbtabelle. |
| 12 | Outline/Shadow | Unveränderte CGraphicTextInstance-/LCD-Pässe aus M10A; keine zusätzlichen Schatten. Native Itemboxen behalten ihren Stil. |
| 13 | Visibility | Originales ShowAll/Picking/CanPickInstance/isShow und Distanzlimit 3500; keine neue Raycast-Occlusion. Native Distanz-Z-Regel bleibt aktiv. Kamera-hinten/außerhalb Screen werden nicht an Bildschirmränder geklemmt. |
| 14 | Mount | Native Rider-Modelhöhe +110 statt +10, im normalen Netzwerkpfad Actor-/Tail-Neuregistrierung bei Mountwechsel. Kein Bone-/Granny-Eingriff. Offline-Test bildet die Owner-Neuerstellung ab; echter Mountwechsel separat manuell. |
| 15 | Große Actors | Unveränderte BoundBox-Höhe des nativen Modells. Orc Lord im Clienttest; hohe synthetische Owner im GPU-Test. Keine erfundene Boss-Offsettabelle. |
| 16 | Reihenfolge | Originale RenderGame-Reihenfolge einschließlich Damage-Effekten; danach game.py Tail-Update/Arrange vor PopState, TextTail.Render nach Interface-State, anschließend UI/Cursor. Keine globale Sortierung geändert. |
| 17 | State-Isolation | Floating-Draws übernehmen native Depth-Test/Write/Function, gewöhnliche UI bleibt depthfrei. Der bestehende deterministische Binder setzt PSO, SRV oder null, Sampler, vollständige Konstanten, Viewport/Scissor, Blend/Cull/Depth und Buffer pro Draw. `FloatingTextScope` ist RAII/nestbar, verändert keinen D3D9-State und endet vor normaler UI. Kein globaler Reset pro Draw. |
| 18 | Resize | Native GPU-Vergleiche 800×600, 1024×768, 1920×1080, 1280×720; richtige Viewport-/Screen-Dimensionen und Positionsvergleich. Zusätzliche reale Clientstarts siehe Testtabelle. |
| 19 | Minimize/Restore | Diligent-Autotest PID 10048 und Legacy-Autotest PID 63504 dreimal vom Nutzer bestätigt; normaler Diligent-Fenstertest ebenfalls im Abnahmeumfang. GPU prüft zusätzlich Nullgröße → Restore an vier Auflösungen. |
| 20 | Mapwechsel | Offline-Originalclient A1 → B1 → A1 mit zwölf Spawn/Mount/Distanz/Hide/Despawn/Drain-Phasen. Nach jeder Owner-Löschung sind alle Tail-Maps und sichtbaren Listen 0. Echter Ingame-Wechsel gesondert dokumentiert. |
| 21 | Relog | Nutzer bestätigt Charakterauswahl → Ingame einschließlich erneut sichtbarem Charakter, Namen und Schadenszahlen sowie A1 → B1 → A1. Die Figur in der Charakterauswahl selbst bleibt unsichtbar: bereits bestehender, nicht migrierter 3D-Preview-Pfad (`introSelect.py::CharacterRenderer`, `ActorRenderBridge`/`actorWorldFrame`-Gate), kein M10B-Fix und keine Vorschau-Migration. |
| 22 | Lifetime | Echter Regressionstest belegte einen verbliebenen sichtbaren Item-Tail-Zeiger nach Delete. `DeleteTextTail` entfernt jetzt Listenzeiger; Clear gibt Text-/Mark-/Title-/Level-/Ownerinstanzen vor Poolfreigabe frei; Destroy ruft Clear auf. Application zerstört Tail-Pool erst nach Actor-/Item-Ownern. Beide Backends profitieren von derselben begrenzten Korrektur. |
| 23 | Shutdown | Bislang abgeschlossene Clients Exitcode 0; konkrete PIDs siehe Tabelle. Native Damage-Instanzen laufen ab, wiederholtes Tail-Destroy ist getestet. |
| 24 | Offene Ressourcen | Abgeschlossene Diligent-Läufe melden nach Shutdown Text-/UI-/Effect-Texturen und Buffer, Effect-/Partikellaufzeit sowie Actor/Attachment/Mount/Tree/Object/World-Ressourcen jeweils 0. GPU prüft zusätzlich Font-Referenzen und alle Tail-Einträge nach Clear. |
| 25 | Build ON/OFF | Release ON/OFF und abschließendes Restore ON bestanden. Zusätzlicher Produktions-ON-Build ohne Renderer-Testzugänge bestanden; beide neuen Diagnose-Funktionsnamen im Binary nicht enthalten. Finale Konfiguration enthält wieder die vollständigen Renderer-Tests. |
| 26 | Testsuite | Vollständige ON-Suite 15/15 (471.89 s), OFF-Suite 13/13 (456.45 s), darin Renderer ON 11/11 und OFF 9/9. Abschließendes ON-Renderer-Suite einschließlich erweitertem Guild-/Damage-Orakel 11/11 (16.19 s). Kein Test zur Umgehung eines Fehlers deaktiviert. |
| 27 | Legacy Regression | Normalstart ohne Rendererargument, Originalpakete; Nutzer bestätigt Darstellung, PID 61136 Exitcode 0. Keine D3D9-Drawcalls entfernt; einzige gemeinsame Verhaltenskorrektur ist die bestätigte Tail-Lifetime-Bereinigung. |
| 28 | Sonderfälle | Kein neuer Heal-/Poison-/Krit-Style; kein Font-/High-DPI-Redesign. EventSet/Quest-Sondertexte, Minimap/Atlas und separate 3D-Preview-Pfade sind nicht durch M10B pauschal freigegeben. Echte Gildenmarke/Shopserverinteraktion nur bei ausdrücklicher Prüfung. |
| 29 | D3D9-Audit | Als separaten nächsten Auftrag empfehlen: verbliebene direkten Draws/Exclusions sowie Offscreen-/Shadow-/Preview-/Event-/Minimap-Pfade inventarisieren, State-/Ressourcenabhängigkeiten und Bildparität prüfen. Nicht aus M10B auf vollständige D3D9-Unabhängigkeit schließen. Audit/Removal hier nicht begonnen. |

## Testnachweise

Alle Nachweise liegen unter `build/milestone10b/` und sind lokale Buildartefakte, nicht eingecheckte Binärdateien.

| Prüfung | Nachweis / Status |
|---|---|
| Reproduzierter Lifetime-Fehler vor Fix | `gpu-lifetime-before.log` / `gpu-lifetime-before-details.log`: Fehler „deleted tail leaves no visible pointer or font owner“. |
| Nach Lifetime-Fix | `gpu-cleanup.log`: PASS. |
| Voller nativer Pixelvergleich | `renderer-final-on-details.log`: 16 Floating-Fälle einschließlich Gildenname/Marke, 216 Damage-Bilder, weiterhin ursprüngliche 28 Text- und 32 UI-Fälle sowie ältere Welt-/Actor-/Effekttests bestanden. |
| Pixelgenauigkeit | Floating/Damage: 0 RGB-Kanäle außerhalb Toleranz 4/255; identische native Renderbefehle liefern die Referenz. LCD-Rundungen innerhalb Toleranz sind kein behaupteter bitidentischer Vergleich. Floating-native X/Y vor/nach Diligent identisch; Outline/Farbe/Boxen über Pixelvergleich geprüft. |
| Damage-Animation | Target, damage, damage_1, miss, target_miss: sichtbare Bilder, Positions-/Scaleänderung, Alphaänderung, Ablauf und SRV-Freigabe. Kein Depth-Write. Anfänglich zu enger Test-Kameraausschnitt korrigiert, keine Spielanimation geändert. |
| Diligent erster Lauf | `floating-initial/`, PID 65460, Exit 0, 181.4 s; frühe Fixture ohne komplette Farb-/Shopinitialisierung, kein finaler visueller Nachweis. |
| Diligent 1024×768 | `floating-visible/`, PID 10048, Exit 0, 181.1 s, zwölf Phasen, Nutzer Minimize/Restore bestätigt. |
| Legacy 1024×768 | `floating-visible/`, PID 63504, Exit 0, 180.8 s, zwölf Phasen; Nutzer dreifaches Minimize/Restore bestätigt. |
| Diligent 800×600 | `floating-800/`, PID 65728, Exit 0, 61.2 s; zwölf Phasen, korrekte Namen/Itemboxen gesehen. Shop-Projektionszeitpunkt der Offline-Fixture danach präzisiert. |
| Normal Legacy | `normal-final/legacy-*`, PID 61136, Exit 0, 45.7 s; Nutzer „sieht gut aus“. Optionale Gilde/Shop/Relog nicht separat behauptet. |
| Normal Diligent | `normal-final/diligent-*`, PID 18796, Exit 0, 852.7 s. Namen, Bodenlabels, Gildenname und UI gesehen. Nutzer bestätigt Relog zurück Ingame und A1/B1/A1. Damage-Rückfrage durch Nutzer geklärt: Mob war bereits tot, kein bestätigter Rendererfehler. Regulärer Shutdown über X durch Computer Use, alle beobachteten Renderer-Owner 0. |
| Release OFF echter Client | `floating-off/`, PID 52268, Exit 0, 60.8 s, 800×600, zwölf Phasen/A1-B1-A1, native Shopbeschriftung und Itemlabels sichtbar. |
| 1920×1080 Client | Nach zwei Startabbrüchen mit erneuter Nutzerfreigabe gestartet: `floating-1920-final/`, PID 66404, Exit 0, 61.2 s, zwölf Phasen/A1-B1-A1, Ressourcen nach Shutdown 0. Native Shopnamen, Spieler/NPC/Boss/Item-/Chatbeschriftungen sichtbar geprüft. Frühere Abbrüche waren keine laufenden Spielclients. |

### Aussagegrenzen der Tests

Der GPU-Test kompiliert die echte `PythonTextTail.cpp` und verwendet originale Text-/Font-/Projektions-/Arrange-/Drawmethoden. Seine World-Owner sind synthetisch. Nicht benutzte Actor/Guild/Application-Dienste sind fail-closed Linkadapter (werfen bei Aufruf), keine Erfolgsstubs. Reale Race-Modelhöhen, Spawn/Despawn, Mounts und Damage-Queue werden im isolierten Originalclient geprüft; Login und Netzwerkzustände nur im normalen Client.

Zwei kleine Python-Diagnosezugänge existieren ausschließlich bei `M2_BUILD_RENDERER_TESTS=ON`: nativen Tail nach Test-Actoraufbau aktualisieren und vorhandene Damage-Queue speisen. Keine neuen Netzwerkpakete oder permanenten zusätzlichen Produktionslogs. Private Testpakete/Configs liegen getrennt vom Benutzerclient. Der automatische Shop-Test benutzt die unveränderte Boardklasse, keine neue Shop-Implementierung.

Die Ressourcenmessung ist ein Kurzzeit-Regressionscheck, keine allgemeine Leak-Freiheitsgarantie. Offline-Legacy nach Warmup 321.1 → 321.2 MiB (Peak 333.4); Diligent 439.0 → 454.8 MiB (Peak 501.8) beim Durchlaufen neuer Karten/Assets. Normal-Diligent fällt nach Karten-/Actorwechseln wieder auf etwa 649 MiB zurück und bleibt dort in wiederholten Messungen stabil. Keine fortlaufend steigende Kurve im stabilen Abschnitt; alle beobachteten Grafik-/Effektowner nach Shutdown 0. Vorhandenes `m2dev-client/config/channel.inf` wurde nicht verändert.

Getestete Client-SHA256: ON `5CC8BBBFEE31E6DF5139CFCC33331838DC81DF609D649D6A7DCE6A88B4C345C7`; OFF `C272516246B61BDA798BC3AF5949EAC0CAEC51A21AA10AF43F8B5743AB782B3B`.

Zusätzlicher Produktions-ON-Build ohne Diagnosezugänge: `B9C89540FB651C0D89008F87DE70A7409F04A643877306062460768B2C86D61A`. Abschließend wiederhergestellter ON-Build: `DE6F530B3F22923A9B5763AB3DC584B2DFDB3357DAAB939250FC2CF07C9DC6C5`. Zwischen normaler Nutzerabnahme und diesem Restore keine weitere Produktionscodeänderung; nur Test-/Dokumentationsergänzungen. Übliche vorhandene Python/zlib-PDB-Linkerwarnungen bleiben. Im normalen Client zweimal die bereits aus M10A bekannte Meldung `invalid idx 0` bei Gildenmarken; keine neue M10B-Fehlermeldung daraus abgeleitet und kein fremder MarkManager-Fix.

## Diff und Abschlussgrenze

- Produktionsintegration: Floating-Scope plus begrenzte Depth-Ausnahme in vorhandenen UI/Text-Bindern; Damage-Filter entfernt.
- Minimaler gemeinsamer Fix: sichtbare Tail-Listen, Font-/Markowner und Shutdown-Reihenfolge.
- Tests: Policy, native GPU-Vergleiche, private Originalclient-Fixture und testgebundene Diagnosezugänge.
- Dokumentation: Voranalyse und dieser Abschlussbericht.
- Keine Shader-/Font-/Glyphatlas-/Layout-/Granny-/Skinning-/Netzwerkänderung. Keine Assets oder Originalpakete ersetzt, kein Commit/Push.

15 bestehende Dateien gezielt geändert, dazu sechs neue Testdateien und zwei Dokumente. Bei den bestehenden Dateien zeigt der normale Git-Diff 68 hinzugefügte/18 entfernte Zeilen; neue Dateien sind dabei noch nicht mitgezählt. `git diff --check` bestanden. Der Source-Checkout war bei Beginn auf `abd048e` sauber. Keine fremden Änderungen zurückgesetzt; bestehendes Runtime-`config/channel.inf` unangetastet.

Die Computer-Use-Prüfung ergänzte Nutzerabnahmen und GPU-Bildvergleiche um Sichtkontrollen der privaten Testfenster; die normale Diligent-Sitzung wurde abschließend gezielt über deren X geschlossen. Kein Login automatisiert, keine Windows-Freigabe bestätigt, kein fremder Spielclient beendet.

**Status: M10B im beauftragten Umfang abgeschlossen.** Namen einschließlich Gilde/Titel/Level, Bodenitem-/Ownerlabels, Shopbeschriftung, Chat-/Info-Tails und vorhandene Damage-/Miss-Partikel werden über die bisherigen Binder dargestellt. Legacy bleibt Standard. Echte Server-Gildenemblemübertragung, Shoptransaktionen und die bereits ausgeschlossene Character-Select-3D-Vorschau sind keine dadurch behauptete Freigabe. Alle gestarteten M10B-Testclients sind beendet; Exitcodes 0. Hier stoppen: keine Fontmodernisierung, keine weitere Renderer-Migration und kein D3D9-Removal.
