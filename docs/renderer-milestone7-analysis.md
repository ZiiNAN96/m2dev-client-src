# Milestone 7 – Analyse vor Implementierung

Stand: 12.09.2026, Source-Basis `6ca598a`. Keine Simulation, Granny-, Terrain-, Actor- oder UI-Migration. Legacy bleibt Standard. Die folgenden Aussagen stammen aus dem vorhandenen Source und den lokalen Originalassets.

## Komponentenmatrix

| Komponente | Geometrie / Upload heute | Zustand / Reihenfolge | M7 |
|---|---|---|---|
| Particle Billboard | CPU `TPTVertex[4]`, DrawPrimitiveUP TriangleStrip | Material aus ParticleProperty, Frustum-Test, Frame-Buckets | Diligent-Submission |
| 2-/3-Face Particle | dieselben vier CPU-Vertices zwei-/dreimal transformiert | unveränderte native Einzelreihenfolge | Diligent-Submission |
| Mesh Effect | `.mde`, expandierte `TPTVertex`-Dreiecksliste, DrawPrimitiveUP | Mesh-/Texturframe, Matrix, Visibility × Alpha | Diligent-Submission |
| Weapon Trace | CPU-Spline, `TPDTVertex`-Strip, DrawPrimitiveUP | SrcAlpha/InvSrcAlpha, kein Depth Write | Diligent-Submission |
| Flying Trace | CPU-Segmente, sechs PDT-Vertices je Strip, DrawPrimitiveUP | SrcAlpha/One, Alpha > 0, native Segment-Sortierung | Diligent-Submission |
| Snow | separates `CSnowEnvironment`, dynamischer D3D9-VB + IB, TriangleList | Alpha Blend, kein Depth Write | separater Geometriepfad prüfen; kein Blur/Postprocessing |
| Light | `CLightInstance` → `CLightManager`, keine eigenen Vertices | Registrierung/Update/Löschung von D3D9-Lichtern | Simulation beibehalten, kein neues Lichtsystem |
| Sound | `.mss` / SoundInstanceVector → AudioLib | Frame/Position, keine Grafik | unverändert |
| Screen filter | `CScreenFilter` → RenderBar2d | Ortho/Blend; SetEnable setzt bereits immer FALSE | ausgeschlossen |
| Camera shake | bestehende Kamera-/Actor-Transformation | keine eigene Geometrie | unverändert übernommen |
| Damage numbers / Miss | ebenfalls EffectLib, `effect/affect/damagevalue/` | Texturwechsel in ProcessDamage | ausdrücklich von Diligent-Submission ausschließen |

## Call-Chains und 39 Analysepunkte

1. **Manager:** `EffectLib/EffectManager.{h,cpp}`, `CEffectManager`, CRC-basierte Daten-, Instanz- und Cache-Maps. `CArea` hält zusätzlich eigene, nicht im normalen Manager registrierte Instanzen; `CFlyingManager` verwaltet Flugobjekte.
2. **Instanzen:** `EffectInstance.{h,cpp}`, `CEffectInstance`, globales Transform und Vektoren für Particle-, Mesh-, Light-Instanzen. Pools geben Instanzen nach Clear/Destroy zur Wiederverwendung zurück.
3. **Partikelsysteme:** `ParticleSystemData`, `CParticleProperty`, `CEmitterProperty`, `CParticleSystemInstance`, `CParticleInstance` in `EffectLib`. Emitter erzeugen CPU-Instanzen; Rendern erzeugt keine neue Simulation.
4. **Tatsächliche Komponenten:** `EffectData.cpp::LoadScript` erkennt particle, mesh und light. Sound wird separat geladen. Typ-Enums allein sind kein Nachweis eines zusätzlichen Renderpfads.
5. **Trennung:** Particles/Billboards gemeinsam; Meshes getrennt; Waffen- und Flugspuren in GameLib; Light ohne Draw; ScreenFilter/Snow separat; Sound in AudioLib.
6. **Echte Draws:** sieben `DrawPrimitiveUP`-Stellen in `ParticleSystemInstance.cpp` (Normal/Attach/TwoSide/ThreeSide), eine in `EffectMeshInstance.cpp`, je eine in `WeaponTrace.cpp` und `FlyTrace.cpp`; Snow besitzt zusätzlich den VB/IB-Draw.
7. **Nur Simulation ohne eigene Geometrie:** Emitter/Controller, Light-Komponente, Flugbewegung. Sie werden nicht durch Diligent ersetzt.
8. **Audio/Gameplay:** Soundevents, Treffer-/Zielereignisse und MotionEvents lösen Effektinstanzen aus. Netzwerk/Skilllogik bleibt unverändert.
9. **Laden:** `CEffectManager::RegisterEffect[2]` → `CEffectData::LoadScript` → `CTextFileLoader`; Bilder über `CResourceManager`/`CGraphicImage`; Mesh über `CEffectMesh::OnLoad`. Dateien kommen durch `CPackManager`.
10. **Formate:** `.mse` Textskripte; `.mde` binär, Signaturen `EffectData` (001) und `MDEData002` (002); `.ifl` Texturliste; `.mss` Soundevents. Original-MSE-Referenzen enthalten DDS, JPG, TGA und BMP. Keine GR2-Meshes im EffectMesh-Pfad.
11. **Emission:** `CParticleSystemInstance::CreateParticles` berechnet Rate × Zeit + Rest, Maximum, Lebensdauer, Point/Ellipse/Square/Sphere, Richtung/Geschwindigkeit, Startgröße/-farbe/-rotation und Texturframe. Native Zufallszahlen bleiben erhalten.
12. **Update:** `CPythonApplication::UpdateGame`: Background.Update → CharacterManager.Update → EffectManager.Update/UpdateSound → FlyingManager.Update. Effekt → Element.Update → ParticleSystem.OnUpdate → Particle.Update. `CArea::RenderEffect` aktualisiert seine eigenen Effekte vor deren Rendern.
13. **CPU-Daten:** `CParticleInstance` hält Position/LastPosition/Velocity, HalfSize/Scale/Rotation, D3DXCOLOR und vier `TPTVertex`; `Transform` erzeugt finale Kamera-Vertices. EffectMesh hält pro Frame `PDTVertexVector` (trotz Namen tatsächlich TPTVertex). Trails halten Zeit-/Positionslisten und fertige PDT-Vertices.
14. **Billboards:** Particle NONE/ALL, Y, LIE, 2FACE, 3FACE; Stretch entlang der Bewegung. Native Transform-Funktionen inklusive Rotationen und Kamera-Up/Cross/View bleiben vollständig bestehen. Mesh NONE/ALL/Y/MOVE hat eigene native Matrixberechnung, ebenfalls unverändert.
15. **Topologien:** normaler EffectLib-/Trail-Pfad nur TriangleList und TriangleStrip. Kein Fan, kein LineList/LineStrip erforderlich. Snow benutzt indizierte TriangleList.
16. **DrawPrimitiveUP:** siehe Punkt 6; die Diligent-Submission erfolgt synchron genau dort, nach der nativen CPU-Berechnung und vor dem nächsten Material-/Transformwechsel.
17. **DrawIndexedPrimitiveUP:** in diesen normalen Effektpfaden nicht vorhanden. EffectMesh expandiert seine gespeicherten Indizes bereits beim Laden. Keine zusätzliche Indexparser-Migration nötig.
18. **Dynamischer VB:** nicht in EffectLib/Trails; `CSnowEnvironment::Render` sperrt einen bestehenden dynamischen VB mit DISCARD. `CScreen` besitzt andere allgemeine dynamische Geometriepfade, die hier nicht umgebaut werden.
19. **Vertexformate:** XYZ+TEX1 = 20 Byte; XYZ+DIFFUSE+TEX1 = 24 Byte. D3DCOLOR liegt als BGRA-Bytes vor. Geplanter neutraler Upload: Position3 + packed Color + UV2, 24 Byte; bei TPT weißes Diffuse, Farbe weiterhin über TextureFactor.
20. **Texturen:** ParticleProperty.ImageVector → CGraphicImageInstance; Mesh-DiffuseMap oder IFL-Liste → Mesh-TextureInstanceVector. WeaponTrace bindet im aktuellen Rendercode sogar bei UseTexture NULL; FlyingTrace bindet NULL und verwendet ausschließlich Diffuse. Dies ist kein Anlass für eine neue Trail-Texturfunktion.
21. **Animation:** Particle.UpdateTextureAnimation verwendet Delay und NONE/CW/CCW/RANDOM_FRAME/RANDOM_DIRECTION, optional zufälliger Start; System verschiebt Instanzen zwischen Frame-Buckets. Mesh hat getrennte CFrameController für Geometrie und je Submesh-Texturliste, Delay/Loop/StartFrame. Diligent übernimmt nur den tatsächlich gebundenen Frame.
22. **UV:** Particle initialisiert feste vier UVs; Rotation erfolgt geometrisch. Mesh-UVs sind frameabhängig, beim Laden wird V negiert. Trail-UVs entstehen aus Lebensalter/Seite. Kein eigener UV-Scroller in den untersuchten EffectLib-Klassen. Falls ein TextureTransform vom nativen Kontext wirksam ist, muss der Draw ihn explizit erfassen statt einen Default vorauszusetzen.
23. **Blend:** EffectInstance setzt AlphaBlend TRUE, SrcAlpha/InvSrcAlpha; Particle überschreibt Src/Dst aus Daten; Mesh überschreibt BlendEnable/Src/Dst. Lokaler MSE-Scan: Src 1,2,3,4,5,6,7,8,12; Dst 1,2,3,4,5,6,7,8,13 (Particle/Mesh vereint). Historische BOTH-Werte benötigen native Prüfung, keine geratenen Zuordnungen. ColorOp 2,3,4,5,6,8 (Select1/2, Modulate/2X/4X, AddSigned).
24. **Alpha-Test:** EffectInstance schaltet ihn aus; WeaponTrace ebenfalls (speichert trotzdem Ref 0x11/GREATER); FlyTrace TRUE, Ref 0/GREATER. Der wirksame Ref/Func wird pro Draw übernommen, keine neue Schwelle.
25. **Depth-Test:** EffectInstance/Particles/Meshes erben ihn; WeaponTrace explizit TRUE/LESSEQUAL; FlyTrace explizit LESS. Capture liest den tatsächlichen Zustand.
26. **Depth Write:** EffectInstance und WeaponTrace FALSE; FlyTrace lässt den bestehenden Zustand unverändert (die entsprechende Save-Zeile ist auskommentiert). Nicht pauschal alle Effekte auf NoWrite setzen.
27. **Cull:** EffectInstance und beide Trails NONE. Vollständiger neutraler Draw-Zustand verhindert Übernahme vom vorherigen Actor-/Tree-PSO.
28. **Fog:** EffectInstance ändert Fog nicht. Area-Effekte liegen innerhalb BeginEnvironment/EndEnvironment, der normale EffectManager danach. `MapManager::BeginEnvironment` setzt EXP oder LINEAR/RangeFog; EndEnvironment restauriert FogEnable. Texturfaktor-Farbe benötigt keine neue Beleuchtung. Lighting kann im nativen Kontext TRUE sein, wird aber bei TFACTOR/Textur nicht als Farbeingang benutzt.
29. **Matrizen:** native D3DX Row-Vector-Matrizen, World → View → Projection, D3D-Tiefenbereich. Partikel/Trails bauen Weltvertices und setzen Identity. Mesh setzt eigene Billboard/Positionsmatrix × Effektmatrix. Y-Vorzeichen folgt ausschließlich dem Originalpfad. D3D9-Pixelzentren werden wie bei den bestehenden Diligent-Renderern berücksichtigt.
30. **Actor/Bone:** `ActorInstanceMotionEvent.cpp::ProcessMotionEventEffectEvent`/EffectToTarget und `ActorInstanceAttach.cpp::AttachEffectByID`; `UpdateAttachingInstances`: bei Bone `BoneMatrix × Translation × ActorWorld`, sonst Translation × ActorWorld. Waffennahe allgemeine MSEs nutzen denselben Pfad. TraceProcess nach Deform liest CompositeBone/Bone-Matrizen.
31. **World Space:** `CArea::__SetEffectInstance`/eigene Instanzmap; `CMapOutdoor::SpecialEffect_Create`; nicht angehängte Particle-Positionen werden bereits beim Spawn in Weltkoordinaten transformiert. FlyingInstance aktualisiert die Matrizen seiner Manager-Effekte und Trailpositionen.
32. **Camera Space:** Billboard-Ausrichtung benutzt Kameraachsen, bleibt Weltgeometrie. Mesh-Billboards benutzen inverse View. ScreenFilter ist der separate Ortho-Pfad und ausdrücklich ausgeschlossen.
33. **Sorting:** EffectManager und CArea sortieren nach EffectData-Pointer (`LessRenderOrder`), nicht nach Kameraabstand. Particles laufen in Texturframe-Buckets und Listenreihenfolge, mit Frustum-Test. FlyTrace sortiert Segmente per Projektion auf die Kamerablickrichtung. WeaponTrace behält seine Spline-Segmentfolge. Alles bleibt unverändert.
34. **Additiv:** Particle-Default SrcAlpha/One, Mesh-Default SrcColor/One; FlyTrace SrcAlpha/One. Originaldaten verwenden überwiegend Dst=One. Farbe/Alpha-Faktoren sind nicht austauschbar.
35. **Normal Alpha:** SrcAlpha/InvSrcAlpha bei EffectInstance, WeaponTrace und vielen Particle-/Mesh-Skripten, außerdem Snow.
36. **Spezialmodi:** Originaldaten enthalten SrcColor, InvSrcColor, DestAlpha, InvDestAlpha und historische BOTH-Werte. BlendOp wird von den normalen Effektdateien nicht verändert; FlyTrace setzt ADD explizit. Multiply-artige Farbfaktor-Kombinationen werden anhand Src/Dst abgebildet, nicht über einen ungenauen Materialnamen. Kein Beleg für einen eigenen Subtractive-Effect-Skriptmodus.
37. **Start/Stop:** Element.Update wartet Startzeit ab; Active steuert Emission; Cycle/LoopCount und ParticleLifeTime/FrameController bestimmen Ende. EffectInstance bleibt alive solange eine Komponente lebt; Manager.Update löscht tote Instanzen. Versteckte Instanzen können weiter simulieren, erhalten aber keine zusätzlichen Diligent-Draws.
38. **Mapwechsel:** CArea.Clear löscht UnsafeEffectInstances; MapOutdoor.Destroy löscht SpecialEffects/XMas; Actor-Clear löscht AttachingEffects und WeaponTraces; FlyingInstance.Clear zerstört Manager-Effect-IDs und FlyTraces. EffectManager.DeleteAllInstances/Destroy sind die globale Grenze. Neue GPU-Texturreferenzen müssen an denselben Instanz-Clear gebunden sein.
39. **Globale Ressourcen:** EffectManager (Daten/Instanzen/Cache), ResourceManager (Images/Meshes), PackManager, CLightManager, Camera/Timer und dynamische Pools. Poolkapazität ist keine Anzahl lebender Effekte. Diligent zählt echte Textur-/Buffer-Handles, nicht Poolkapazitäten.

## Kleinste geplante Änderungspunkte

* Neu `Renderer/EffectRenderData.h`: API-neutraler Draw-/Vertexvertrag, Scope und gezählte Laufzeitdaten. Keine D3D-/Granny-Typen.
* Neu `Renderer/DiligentEffectRenderer.{h,cpp}`: TriangleList/Strip, ein wiederverwendeter Dynamic-VB mit DISCARD und bedarfsgerechtem Wachstum, vollständiger PSO/SRV/Sampler/Constant-Bind. Keine verzögerte Queue und damit kein zusätzlicher Frame Lag.
* Neu `EffectLib/EffectRenderBridge.{h,cpp}`: ausschließlich lesender Adapter des wirksamen nativen Drawzustands; Originaltextur über vorhandenen StaticObject/TerrainTextureLoader, kein neuer Decoder.
* `EffectInstance.{h,cpp}`: Ressourcenscope, Clear und Zähler; `ParticleSystemInstance.{h,cpp}` / `EffectMeshInstance.cpp`: Texturidentität und fertige CPU-Vertices an den Adapter. Keine Emission/Update/Billboard-Berechnung ändern.
* `GameLib/WeaponTrace.cpp` / `FlyTrace.cpp`: je ein synchroner Submission-Aufruf. Snow gegebenenfalls nur vorhandene CPU-Quad-Geometrie, kein Blur.
* `Renderer/TerrainPresentation.cpp`: Renderer-Lebenszyklus, WorldFrame-Gate, vorhandene begrenzte Diagnosedatei und Shutdown-Zähler. Keine Login-/UI-Komposition.
* CMake und isolierte Renderer-Tests: eigene Effektfälle, echte Originalassets, keine bestehenden Tests entfernen.

Ressourcenstrategie: GPU-Texturen gehören zu nativen Effektinstanzen; ein nur schwach referenzierender Cache darf gleiche Assets teilen, aber tote Effekte nicht am Leben halten. Dynamischer Uploadbuffer und PSO/Sampler bleiben beim Renderer und werden beim Shutdown vollständig freigegeben. SRB-Referenzen werden an Frame-/Shutdown-Grenzen gelöst. Kein globaler State-Reset nach jedem Draw.

## Prüfplan / Sonderfälle

Zuerst reine Zustands-/Topologieprüfungen und deterministischer D3D9/D3D11-Framebuffer-/Depth-Vergleich, inklusive Alpha- und Spezial-Blend, NULL-Textur, Materialreihenfolgen, Mips/Sampler und Bufferwachstum. Danach reale MSE/MDE/IFL-Instanzen, mehrere Skillklassen/World/Buff/Hit/Trails, Belastung und Spawn→End→Null-Texturressourcen. Anschließend private Clients mit Originalpaketen, A1→B1→A1, Resize/Minimize/Restore und beide normalen Ingame-Starts.

Vorhandene Sonderfälle werden nicht still repariert: WeaponTrace-NULL-Textur, Mesh-Billboard-Winkel/Frameregeln, native Sortierung und historische Blendwerte. Schadenszahlen sind zwar EffectLib-basiert, bleiben nach ausdrücklichem Scope ausgeschlossen. Dynamische Lichtwirkung auf bereits migrierten Materialien ist ein eigener Beleuchtungsumfang, keine Particle-Draw-Submission. Snow-Blur/ScreenFilter sind Postprocessing und bleiben außen vor.

API-Abgleich für die seltenen Legacy-Werte: [Microsoft D3DBLEND](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dblend), [Microsoft Texture Operations](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dtextureop). Die tatsächliche Treiberwirkung wird zusätzlich im nativen Test geprüft.
