# M10B – Analyse vor Implementierung

13.09.2026, Basis `abd048e` (M9/M10A), Worktree zu Beginn sauber.
Nur bestehende weltgebundene Texte; kein Font-/Layout-/Shader-Redesign.

## 30 Legacy-Fragen

| Nr. | Frage | Befund / konkrete Quelle |
|---|---|---|
| 1 | Player-Namen | `UserInterface/InstanceBaseEffect.cpp::AttachTextTail/RefreshTextTail` → `PythonTextTail.cpp::RegisterCharacterTextTail/Render` → `EterLib/GrpTextInstance.cpp::Render`. |
| 2 | NPC/Mob | Derselbe `CInstanceBase`/`CPythonTextTail`-Pfad, Instanztyp entscheidet Farbe, keine eigene Font-Engine. |
| 3 | Gilde | `RegisterCharacterTextTail`: CPythonGuild-Name, CGraphicTextInstance plus CGraphicMarkInstance mit vorhandener Mark-Atlas-Seite; `ArrangeTextTail` positioniert beide. |
| 4 | Damage | `PythonNetworkStreamPhaseGame.cpp::RecvDamageInfoPacket` → `CInstanceBase::AddDamageEffect` → Queue → `ProcessDamage` → `CEffectManager::SetEffectTextures/CreateEffect` → `CEffectInstance`/`CParticleSystemInstance`. Keine Glyphen. |
| 5 | Andere Floating-Texte | Chat-/Info-Tails, Level/Alignment-Titel, Shop-AdvertisementBoard, Ground-Item/Owner. Bildschirmgebundene Tip-/System-Widgets gehören bereits zu M9/M10A; EventSet/Quest-Sondertexte nicht pauschal freigeben. |
| 6 | Projektion | `CPythonTextTail::UpdateTextTail` → `CScreen::ProjectPosition` → D3DXVec3Project; anschließend floor(X/Y). Keine neue Projektion nötig. |
| 7 | Kamera | Vorhandene ms_Viewport, ms_matProj/View/World; Identity vor Projektion. `PythonApplication::RenderGame` setzt Perspektive; `game.py::OnRender` aktualisiert Tails vor PopState/SetInterfaceRenderState. |
| 8 | Höhe | Owner-Position.z + beim Registrieren gespeichertes fHeight. Name unten/zentriert ausgerichtet; Chat kann Namen zusätzlich 17 px verschieben. |
| 9 | Race/Model | `CGraphicThingInstance::GetHeight` benutzt BoundBox des aktuellen Modells in LODController[0]; keine neue Race-Tabelle. |
| 10 | Mount | AttachTextTail addiert 110 statt 10. `NetworkActorManager::__AppendCharacterManagerActor` löscht/erstellt bei Mountstatus den Actor neu; Nameplate folgt Rider-Owner, nicht Mount-Bone. Direkter Python-Test-Mount allein ist nicht der Netzwerk-Recreate-Pfad. |
| 11 | Große Mobs | Dieselbe reale Modelhöhe +10; kein separater Boss-Renderer/Skalierungsfaktor. Existierende Modellgrenzen unverändert übernehmen. |
| 12 | Distanz | ShowAll: Character/Item nur unter 3500 Spieleinheiten. Unter 1300 Z=0; darüber Z=projectedZ×(-OrthoDepth)+10. Picking/Target-Aufrufe behalten ihre bestehenden Regeln. |
| 13 | Visibility | Character: Owner isShow, existierende Instanz, keine GuildWall, CanPickInstance: Frustum, Alive und Unsichtbarkeitsregeln. Chat: Owner isShow; Items eigene Liste. `game.py` steuert Alt/AlwaysShowName/Target/Picking. |
| 14 | Occlusion | Kein zusätzlicher Raycast. TextTail setzt Depth nicht selbst ab: native ZENABLE/ZWRITE/ZFUNC bleiben wirksam; Entfernung/Z beeinflusst Overlay/Verdeckung. M10A schaltet Depth für UI pauschal ab – daher begrenzter Floating-Kontext nötig, der exakt native Depth-Werte übernimmt. |
| 15 | Farben | `InstanceBaseEffect::GetNameColorIndex/GetIndexedNameColor`; Item weiß, Ownership gelb, Auswahl grün; Chat weiß, Info rosa; Gilde 0xFFEFD3FF. |
| 16 | Outline/Shadow | Character/Guild/Title/Level/Chat/Info original Outline; M10A native LCD-Doppelpass. Item hat Originalbox (schwarzer Rand, 0.3 Alpha), keine erfundene Outline. |
| 17 | Empire/PvP | GetNameColorIndex: Killer/PvP/Duell/Gildenkrieg/Party/Empire; SetEmpireNameMode und vorhandene Farbtabellen. Renderer übernimmt fertige Vertexfarben. |
| 18 | Titel/Alignment | RefreshTextTail/GetAlignmentGrade → AttachTitle; Level „Lv …“ mit vorhandener Farbe; Arrange/TextTailBiDi setzt relative Positionen. |
| 19 | Shops | `assets/root/uiPrivateShopBuilder.py::PrivateShopAdvertisementBoard` (UI_BOTTOM), native ThinBoard+TextLine, `chr.GetProjectPosition(vid,220)`, IsShowSalesText. Bereits durch M9/M10A abbildbar; kein eigener Renderer nötig. |
| 20 | Ground Items | PythonItem::CreateItem → RegisterItemTextTail mit ThingInstance-Owner, Offset -10; SetOwnership → SetItemTextTailOwner. Arrange entflechtet benachbarte Beschriftungen, maximal 20 Wiederholungen. Kein eigener Stack-Zahlpfad im Tail. |
| 21 | Damage-Ausgabe | Ziffern rechts→links, max. acht; pro Ziffer DDS-Override, 30 Welt-Einheiten Abstand kameraorientiert; Start über nativer Modelhöhe. Target oder alternierende Self-Damage-Effekte. |
| 22 | Typen | Enum NORMAL, POISON, DODGE, BLOCK, PENETRATE, CRITICAL. Kein Heal-Flag/Heal-Zahlenzweig vorhanden. |
| 23 | Krit/DB/Miss | Dodge/Block nutzen Self-/Target-Miss-Asset. Krit-Sondereffekt auskommentiert, Ziffern bleiben. Penetrate verwendet normalen Zahlpfad. Poison-Sonderzweig und Nichtziel-Ausgabe deaktiviert; nicht neu aktivieren. |
| 24 | Animation | Originale .mse unter `assets/Effect/ymir work/effect/affect/damagevalue/`, vorhandene Emitter-/Partikelruntime. Target: nicht-loopend, Lebenszeit 0.5 s, Kamera-Billboard; tatsächliche Spawn-/Ablaufzeit durch vorhandene Runtime. |
| 25 | Scale/Movement/Fade | TimeEventPosition/Velocity/Gravity/ScaleX/Y/Color/Alpha aus Originalassets; z.B. Target Alpha 0.701→1→0.041 und Scale 0.1→0.45→ca.0.13/0.15. Keine neuen Kurven/Animationszeiten. |
| 26 | Texture/Glyph | Damage/Miss eigene DDS-Partikelbilder. Namen/Chat/Owner echte vorhandene Fontatlasseiten. Gildenmarke Bildatlas, Shop native UI-Bilder+Text. |
| 27 | Reihenfolge | RenderGame: Welt/Actors/Wasser/Snow/Area-Effekte → EffectManager (auch Damage) → Items/Fly → Blocker/Lensflare. Danach `game.py`: Update/Arrange Tails → PopState/Interface → TextTail.Render (Character/Guild/Title/Level, Items+Box, Chat) → übrige Widgets/Cursor gemäß WindowManager. Keine Umsortierung. |
| 28 | Bounds/Clip | Actor-Frustumfilter vor Nameplate; native Clipspace/Viewport/Scissor danach. Items/Chats besitzen keinen neuen unabhängigen Behind-Camera-Filter. Z-Werte und Clipregeln beibehalten, nicht Namen an Bildschirmränder klammern. |
| 29 | Lifetime | Character-/Item-/Chat-Maps, sichtbare Listen, TextTail-Pool. Delete gibt Text-/Markinstanzen frei; Chat/Info läuft nach standardmäßig 5000 ms ab. Auffällig: Delete entfernt sichtbaren Listenpointer nicht; Clear/Destroy geben Pool frei, ohne enthaltene Text-/Markowner aufzuräumen, Destroy lässt Maps zurück. Application zerstört Tails vor Character/Items. Mit gezieltem Test belegen und auf diese Ownership-Stellen begrenzen. Damage: EffectManager löscht abgelaufene Instanzen; EffectResources am Effektowner, globale Texture-Tabelle nur weak_ptr. |
| 30 | Native Draws | Glyph-Batches: DrawPrimitiveUP TRIANGLELIST (zwei LCD-Pässe). Itembox: CScreen Bar/Lines; Marke: DrawIndexedPrimitive. Damage: native Partikel-TRIANGLESTRIP via EffectRenderBridge. Alle nativen Draws bleiben; keine D3D9-Entfernung. |

## Kategorien vor Änderung

| Typ | 3D-gebunden | animiert | Daten | Diligent vor M10B |
|---|---|---|---|---|
| Player-/NPC-/Mob-/Bossname | ja | folgt Owner | Glyphen | ausgeschlossen in TextTail.Render |
| Gilde/Level/Titel | ja | folgt Owner | Glyphen + Markbild | gleicher Ausschluss |
| Shopname | ja | Board folgt Projektion | UI/Glyphen | M9/M10A, jetzt ausdrücklich prüfen |
| Bodenitem/Owner | ja | Arrange/Drop-Owner | Glyphen + Box | TextTail-Ausschluss |
| Chat-/Info-Tail | ja | zeitbegrenztes Folgen | Glyphen | TextTail-Ausschluss |
| Damage-Ziffer | ja | native Partikel | DDS | EffectRenderScope filtert damagevalue |
| Miss/Block | ja | native Partikel | DDS | derselbe Effektfilter |
| Bildschirm-Systemmeldung | nein | native UI-Logik | UI/Glyphen | M10A; kein neuer Weltpfad |

## Minimale Integrationsstellen / Testplan

- `PythonTextTail.cpp`: Ausschluss durch eng begrenzten Floating-Scope ersetzen. Keine Projektions-/Layout-/Farbänderung.
- `Renderer/UIRenderData.h`, `EffectRenderData.h`, `DiligentTextRenderer.h`, `DiligentUIRenderer.h`, `EterLib/TextRenderBridge.cpp`, `UIRenderBridge.cpp`: Floating-Draws behalten native Depth, alle bisherigen UI-Defaults unverändert. Ein vorhandener Binder, kein weiterer Renderer/Shader.
- `EffectLib/EffectRenderBridge.cpp`: ausschließlich alten Damage-Assetfilter entfernen; weiterhin originale Instanz-/Texture-Owner, Material-Snapshot und World-Gate.
- TextTail-Lifetime gesondert testen; nur bestätigte Owner-/Listen-/Shutdown-Reihenfolge korrigieren. Kein Pool-/Actor-/Mount-Umbau.
- `tests/Renderer`: Policy- und native GPU-Positions/Pixel/Depth-Vergleiche, reale Damage-Assets/Animation, isolierter Originalclient mit Namen/Mount/Boss/Items/Shop/Chat/Info/Spawn/Despawn, A1/B1/A1, 800/1024/1920 und Fensterzuständen. Kleine diagnostische Damage-Injektion nur bei Bedarf im bestehenden Python-Testbereich, keine Netzwerkpakete.
- Anschließend normale Legacy-/Diligent-Nutzerabnahme, ON/OFF Release und volle Testsuiten. Keine ungeprüften Interaktionen als bestanden deklarieren.
- Stop nach M10B. EventSet, Minimap/Atlas, separate Character-Select-3D-Vorschau und D3D9-Audit bleiben gesondert; Audit nur empfehlen.
