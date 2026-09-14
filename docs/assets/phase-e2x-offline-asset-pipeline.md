# E2-X – ZiiNAN Offline Asset Pipeline

Stand: 14.09.2026. **E2-X GO: Release/Debug/GCC, echtes Rendering, manuelle Abnahme und Shutdown bestanden.**
Kein E3/F, GR2-Konverter, Granny-Ersatz, Animationsevaluator, PBR oder Android-Toolport.

## 1. E1-X Baseline

Sauberer Source-Checkout auf **7424051**, `feat(assets): add native glTF asset provider`.
Vor Änderungen bestanden die zehn relevanten E1-X/D1-X-Tests in Release (2,85 s) und
Debug (2,73 s), einschließlich echter GLB-Draws, Granny-Parität und Consumer-Verträgen.
Die historische E1-X-Gesamtabnahme 26/26 bleibt im E1-X-Bericht; sie wird hier nicht als
erneut ausgeführte vollständige Suite ausgegeben. E1-X bleibt als vorhandener Commit erhalten.
`src/` bleibt vollständig unverändert. Client-Checkout: vorgefundene `config/channel.inf`
und acht unversionierte Rendererlogs bleiben bestehen.

## 2. Tool Architecture

```text
Source FBX / OBJ / DAE / PLY / STL
  -> ziinan-asset-tool
     -> privater AssimpImporter
     -> besitzendes neutrales Scene-Modell
     -> Process / Validate / meshoptimizer
     -> cgltf_write -> eingebettetes GLB -> E1-X-Ausgangsprüfung

Unveränderter Client
  -> ResourceManager / AssetRuntime
     -> GrannyProvider (GR2) oder GlTFProvider (GLB)
     -> vorhandene Modell-/Material-Consumer -> Diligent D3D11
```

Neue eigene Grenzen liegen ausschließlich in `tools/AssetTool`. Das Tool benutzt den
bestehenden GlTFProvider als Ausgangsprüfung; daraus entsteht keine umgekehrte
Client-Abhängigkeit auf Tool, Assimp oder meshoptimizer.

## 3. CLI

```text
ziinan-asset-tool convert input.fbx output.glb [Optionen]
ziinan-asset-tool validate input.obj [Optionen]
ziinan-asset-tool inspect model.glb [--json]
ziinan-asset-tool batch input-directory output-directory [Optionen]
ziinan-asset-tool --help
ziinan-asset-tool --version
```

Optionen: `--json`, `--no-optimize`, `--generate-normals` (Default),
`--no-generate-normals`, `--generate-tangents`, `--meters-per-unit <positiv>`,
`--ticks-per-second <positiv>`. Optionen folgen den Pfaden. Vorhandene Ausgaben werden
von der CLI abgelehnt; es gibt kein unbeabsichtigtes Überschreiben. `--version` nennt
E2-X.1 und die drei Bibliotheksversionen. Skins bleiben in E1-X Metadaten.

## 4. Assimp Integration

Assimp **6.0.2**, import-only, statisch, gepinnter Download mit SHA256
`d1822d9a19c9205d6e8bc533bf897174ddb360ce504680f294170cc1d6319751`.
Kein Assimp war vorher im Checkout integriert. FetchContent wird ausschließlich durch
das opt-in Tool aktiviert. Tests, Exporter, Viewer und Installtargets sind abgeschaltet.
Die lokalen geprüften Quellen liegen für diese Abnahme in `build/e2x/deps`.

Assimp erhält eigene Include-Pfade und Compilerdefinitionen. Dies verhindert sowohl
das Shadowing seines `utf8.h` durch den Client als auch den Konflikt von `/utf-8`
mit Assimp `/source-charset:utf-8`. Keine Änderung an Vendor-Quellen nötig.
Assimp-Warnungen und intern wiederherstellbare Fehler werden über einen privaten,
RAII-gebundenen LogStream in `Report` übernommen; fehlende Materialien sind dadurch
sichtbar. Erzeugter Loggerzustand wird zurückgesetzt, vorhandene Streams bleiben
erhalten. Import/Batch läuft seriell; nach 1024 Diagnosen wird die Quelle abgelehnt.
Die statische MSVC-Runtime entspricht dem Projekt. Lizenzen einschließlich der im
Assimp-Lizenztext aufgelisteten Drittkomponenten liegen in `tools/AssetTool/licenses`.
[Assimp 6.0.2](https://github.com/assimp/assimp/releases/tag/v6.0.2).

## 5. Supported Source Formats

FBX, OBJ und COLLADA/DAE sind Pflichtformate und geprüft. PLY und STL sind zusätzlich
aktiviert; PLY wird im mittleren Konvertierungs-Sanity verwendet. 3DS bleibt deaktiviert:
seine Master-Scale-Semantik wurde nicht als physikalischer Einheitenvertrag übernommen.
Blender-Nativdateien, GR2 und exotische Formate werden nicht importiert. GLB wird bei
`validate`/`inspect` direkt über E1-X geprüft, nicht erneut über Assimp konvertiert.

## 6. Internal Offline Representation

`Scene.h`: Scene, Node, Mesh, Vertex, Material, Image, Skeleton, Joint, Animation,
Channel, Bounds, Report und Options. Alle Daten sind besitzende Standard-C++-Werte.
Kein Assimp-, Renderer- oder AssetRuntime-Typ befindet sich im neutralen Header.
Assimp-Zeiger verlassen `AssimpImporter.cpp` nicht. Pro Mesh eine Materialzuordnung;
Nodes können mehrere Meshes referenzieren. Geteilte Meshdaten bleiben möglich.

## 7. Coordinate Convention

Ziel ist die E1-X-Eingangskonvention: **rechtshändig, +Y oben, +Z vorne, Meter,
CCW-Dreiecke, Bild-/UV-Ursprung oben links, column-major-Matrizen mit Spaltenvektoren**.
Hierarchische Matrix: `world = parent * local`. Der zusätzliche kanonische Root
trägt die deterministische Quellenbasis und Einheitenumrechnung, gemeinsam für
Geometrie, Jointgraph und Animation. Authored Spiegelungen bleiben Node-Transforms;
E1-X wendet seine vorhandene Windingkorrektur beim Backen genau einmal an.

Die bestehende Runtime-Abbildung `100 * (x, -z, y)` bleibt unverändert. Kein
formatabhängiger Faktor wird in Client/Renderer ergänzt. Unitless OBJ/PLY/STL
setzen RH/Y-up voraus und melden dies; abweichende Achsen sind im Authoring zu exportieren.
[glTF-Konvention](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#coordinate-system-and-units).

## 8. Unit Convention

FBX: `UnitScaleFactor` ist in Assimp 6.0.2 ein **Float-Metadatum** in Zentimetern
pro Quelleinheit; das Tool liest den exakten Typ und multipliziert mit 0,01.
Es deaktiviert Assimp-FBX-Rootkorrekturen und erzeugt die gleiche Basis aus geprüften,
verschiedenen Achsenindices [0,2] und Vorzeichen ±1 selbst. Damit gibt es keine
doppelte Skalierung und keinen ungeprüften Metadaten-Achsenindex.

DAE: Assimp verarbeitet `<unit meter>` und `up_axis` am Root. Der Tool-Root skaliert
dann nicht erneut; `sourceMetersPerUnit` berichtet die tatsächliche importierte
Root-Einheit. Bei explizitem Override wird Assimp-DAE-Unitscaling abgeschaltet.
OBJ/PLY/STL besitzen keinen zuverlässigen Einheitenvertrag: Default 1 m/Einheit mit
Warnung, alternativ `--meters-per-unit`. Null, negative, NaN/Inf und nicht darstellbare
Skalierung werden abgelehnt. Der CLI-Faktor verändert keine GR2-Daten.

[FBX-Implementierung](https://github.com/assimp/assimp/blob/v6.0.2/code/AssetLib/FBX/FBXConverter.cpp),
[FBX-Metadatentypen](https://github.com/assimp/assimp/blob/v6.0.2/code/AssetLib/FBX/FBXDocument.h),
[COLLADA-Implementierung](https://github.com/assimp/assimp/blob/v6.0.2/code/AssetLib/Collada/ColladaLoader.cpp).

## 9. Mesh Import

Positionen, Normalen, UV0, Indices, mehrere Meshes/Materialien und Node-Transforms
werden übernommen. Tangenten sind optional. Vertexfarben, zusätzliche UV-Sets,
Morphs und nicht als TRIANGLES darstellbare Topologien werden ausdrücklich abgelehnt.
Nicht verwendete Kameras/Lichter und nicht gerenderte Materialsemantik erzeugen
benannte Warnungen statt stillen Verschwindens.

## 10. Triangulation

Assimp trianguliert Polygone offline. Die neutrale Validierung verlangt eine
nichtleere, durch drei teilbare Indexliste und der Writer gibt ausschließlich
`TRIANGLES` aus. Das OBJ-Marktstand-Fixture enthält 126 Quads, die zu 252 Dreiecken werden.

## 11. Normals

Default: ausschließlich fehlende Normalen erzeugen. Vorhandene Normalenrichtungen
bleiben erhalten; nicht normierte Längen werden mit Diagnose auf Einheitslänge
gebracht, damit GLB-NORMAL den glTF-Vertrag erfüllt. `--no-generate-normals` deaktiviert die Offline-Erzeugung;
bei solchen GLBs bleibt E1-Xs vorhandene Fallback-Erzeugung maßgeblich. Der neutrale
Processing-Core kann für einen späteren Importer ebenfalls flächengewichtete
Normalen bilden, mit dokumentierter +Y-Normale für unbestimmte Vertices.

## 12. Tangents

`--generate-tangents` aktiviert Assimp-Tangentenberechnung nach UV-Konversion.
Vorhandene Tangenten werden als Metadaten erhalten; das Vorzeichen wird für den
V-Flip korrigiert. Authored Tangenten zusammen mit UV-Rotation/-Skalierung werden
abgelehnt, damit keine falsche Tangentenbasis exportiert wird. Der neutrale Fallback
verwendet UV-Ableitungen und Orthogonalisierung. Kein Normalmap-/PBR-Renderer.

## 13. Indices

Vor meshoptimizer werden alle Indices auf Vertexbereich, Dreiergruppen und
Element-/Speichergrenzen geprüft. Export: UInt32-Indices ohne Quantisierung oder
Kompression. E1-X wählt seine Uploadbreite selbst anhand der tatsächlich verwendeten
Indices. Leere Meshes/Vertices/Indexlisten und außerhalb liegende Referenzen sind Fehler.

## 14. Degenerate Geometry

Entartete/nahezu kollineare Dreiecke werden gezählt und mit Mesh, Quellnode und Anzahl
gemeldet. Sie bleiben erhalten. Der relative Test verwendet Kreuzproduktquadrat
gegen `max(edgeSquared)^2 * 1e-24`. Keine stille Massenlöschung.

## 15. Bounds

Lokale Meshbounds und über die vollständige Nodehierarchie transformierte Modellbounds
werden berechnet und auf Endlichkeit, min≤max und E1-X-Koordinatenlimit geprüft.
Singuläre Meshworld-Transforms, nichtaffine Matrizen und NaN/Inf sind Fehler.
CLI-Bounds verwenden Meter; GLB-Inspect rechnet die providerinternen Clientbounds
in die kanonische Basis zurück.

## 16. Materials

BaseColorFactor und Diffuse/BaseColor-Textur, Opazität, ableitbares AlphaMode/Cutoff
und DoubleSided werden auf Core-glTF abgebildet. Nicht texturierte Materialien
nutzen in der Runtime weiterhin das vorhandene weiße Fallbackbild. Kein PBR-Zwang;
Metallic=0/Roughness=1 im Writer. Spekulare/emissive/PBR-/weitere Textursemantik wird
gegebenenfalls als nicht übernommene Authoringinformation gewarnt.
Vier benutzte Marktstand-Materialien bleiben erhalten; Assimp und E1-X ergänzen je
einen unbenutzten Default, daher unterscheiden sich reine Material-Arrayzählungen.

## 17. Texture Resolution

Relative Pfade werden gegen das Quellverzeichnis aufgelöst; Windows-Backslashes
werden normalisiert. UTF-8-Namen gehen durch `std::filesystem` und native Windows-
Wide-File-I/O. Fehlende MTLs und Bilder sind Fehler. Dateinamencase wird nicht durch
unscharfe Suche geraten; portabel müssen die Namen exakt passen. GLB enthält keine
absoluten Entwicklerpfade oder externe Bild-URIs.

## 18. Embedded Textures

Externe PNG/JPEG-Dateien und komprimierte eingebettete Assimp-Bilder werden in
GLB-BufferViews eingebettet. Unkomprimierte eingebettete Texel werden zu PNG kodiert.
Signatur, MIME, deklarierte Dimensionen und vollständiger stb-Decode werden geprüft:
maximal 8192 pro Dimension, 16.777.216 Pixel und 32 MiB kodierte Bytes je Bild.
Nur lineares Repeat-Sampling passt zu E1-X; Clamp/Mirror werden abgelehnt.
Keine KTX2/BasisU/BC7/ASTC-Kompression oder zusätzliche Runtime-Dateien.

## 19. Skeleton Import

Jointnamen, Quellnodes, Hierarchie, Inverse-Bind-Matrizen und Mesh-Joint-Mapping
werden gespeichert. Ein globales Skin entspricht dem E1-X-Vertrag. Wenn derselbe
Joint in mehreren Meshes verschiedene Inverse-Binds benötigt, wird die nicht
darstellbare Kombination abgelehnt. FBX-Pivotketten bleiben für Animation erhalten.
Nicht eindeutige namensbasierte Bone-/Channel-Ziele sind Fehler.

## 20. Skin Weights

Joint-/Vertexbereiche, negative/nichtendliche Gewichte und positive Gesamtsumme
werden geprüft. Doppelte Joint-Einflüsse werden zusammengeführt, danach stabil
nach Gewicht absteigend/Joint-ID aufsteigend sortiert. Höchstens vier Einflüsse
bleiben, Reduktion wird gemeldet, anschließend deterministische Normalisierung.
Nullfüllslots erhalten gültigen Joint 0. Export JOINTS_0 UInt16 / WEIGHTS_0 Float32.
Keine Änderung des produktiven Granny-/GPU-Bytegewichtstreams.

## 21. Animation Import

Clipname, Dauer, Translation/Rotation/Scale-Kanäle und Keyarrays werden offline
übernommen. Animierte Nodes müssen verlustfrei als TRS darstellbar sein; animiertes
Shear wird abgelehnt. Quaternionen sind endlich, ungleich null, normiert und in
kontinuierlicher Vorzeichenfolge. Kein Euler-Zwischenschritt. Liegt die deklarierte
Clipdauer hinter dem letzten Key, ergänzen begrenzte Hold-Keys die Dauer mit Warnung.
Mesh-/Morphanimation und vom Importer exponierte nicht unterstützte Interpolation
werden abgelehnt. Assimp 6.0.2 übernimmt insbesondere COLLADA-STEP/Bezier-Modi nicht
zuverlässig in seine Keys; animierte DAE/FBX-Quellen melden daher explizit, dass
sampled TRS erhalten sind, aber die ursprüngliche Kurveninterpolation nicht als
verlustfrei bewiesen ist. Ein STEP-DAE-Test prüft diese Warnung.

## 22. Timebase

`seconds = sourceTicks / sourceTicksPerSecond`. Keine 24/25/30-Annahme im Tool.
Fehlende/ungültige Rate erfordert einen expliziten Override. Keys müssen auch nach
Float32-Konversion nichtnegativ und strikt steigend sein. DAE liefert über den
Importer 1000 ticks/s; FBX verwendet eine konsistente importerinterne Rate für die
aus FBX-Zeitstempeln gebildeten Keys. Extrapolation außerhalb des Clips wird gewarnt.
GLB-Clips bleiben in E1-X ausschließlich Metadaten, ohne Sampling/Mixer.

## 23. meshoptimizer Integration

meshoptimizer **0.25**, MIT, privates statisches Tooltarget. SHA256 des Quellarchivs:
`68b2fef4e4eaad98e00c657c1e7f8982a7176e61dd7efdeaec67a025b8519be9`.
Kein gltfpack, Decoder oder neues Runtime-Kompressionsformat.
[Release/API](https://github.com/zeux/meshoptimizer/tree/v0.25).

## 24. Vertex Cache Optimization

Rigide OPAQUE-/MASK-Meshes erhalten `meshopt_optimizeVertexCache`.
Tests vergleichen sämtliche Dreieckspositionen, Normalen, UVs und Winding vor/nach
Reordering. Statistik: modellierte ACMR mit 16 Cacheeinträgen, über Dreiecke gewichtet.
Dies ist keine gemessene GPU-Zeit und keine FPS-Prognose.

## 25. Vertex Fetch Optimization

`meshopt_optimizeVertexFetchRemap` + Indexremap ordnet den vollständigen Vertexdatensatz
einschließlich aller vorhandenen Attribute um. Nur nicht referenzierte Vertices
werden dabei mit gemeldeter Anzahl entfernt. Skinned-Meshes bleiben von beiden
Optimierungsschritten ausgeschlossen. Geometrie und Attribute der gezeichneten
Dreiecke bleiben unverändert.

## 26. Overdraw Decision

Kein Overdraw-Reordering in E2-X. BLEND behält ausdrücklich die originale
Dreiecksreihenfolge und erhält nur Fetchremapping. Dies vermeidet Veränderungen
von Alpha-/Foliage-Darstellung durch eine rein geometrische Sortierung.

## 27. LOD Infrastructure

`MakeStaticLOD(source, ratio, output, report)` erzeugt eine getrennte neutrale Szene
mit `meshopt_simplify`, konfigurierbarem Verhältnis, gesperrten Grenzen und maximal
1% relativem geometrischem Fehler. LOD0 bleibt unverändert. Nur opake, statische
Assets ohne Animation/Skin; Alpha und Skinned-LOD werden abgelehnt. Nicht erreichbare
Zielreduktion wird gemeldet. Unit-Test beweist separate Reduktion.
Noch kein CLI-LOD-Batch, automatisches LOD-Manifest oder Runtime-LOD-Auswahlverfahren.

## 28. GLB Writer

Der passende **cgltf_write 1.15** aus demselben Commit wie E1-Xs Parser serialisiert
Meshaccessoren, Materialien, Bilder, Nodes, Skin und Animation. Eine begrenzte
28-Byte-GLB-Hülle ermöglicht native Unicode-Ausgabepfade. Keine eigene glTF-JSON-
Implementierung. Strings werden vor cgltf korrekt escaped; Arrays besitzen stabile
Adressen. Alle Views sind vierfach ausgerichtet. Kein Zufallswert/Zeitstempel im GLB.
Vor dem Schreiben lädt der bestehende GlTFProvider die vollständigen Ausgabebytes.
[Writer-Vertrag](https://github.com/jkuhlmann/cgltf/blob/v1.15/cgltf_write.h).

## 29. Validate Command

Quellen durchlaufen Import, Normalisierung und vollständige Validierung ohne
Meshreordering oder Dateiausgabe. GLB nutzt unveränderten E1-X-Parser/Provider plus
vollständigen Decode eingebetteter Materialbilder. Geprüft werden Datei, Meshes,
Attribute, Indices, Materialien/Bilder, Hierarchie, Bounds, Skin und Animationsdaten.
Echte Fehler ergeben einen Nichtnull-Exitcode und benannte Diagnosen.

## 30. Inspect Command

Text und JSON berichten Typ STATIC/SKINNED/ANIMATED_RIGID, Mesh-/Vertex-/Indexzahlen,
Materialien, Bilder, Bones, Clips, maximale Influences und Bounds. Quellen zusätzlich
Einheitenentscheidung und Processing-Statistik. GLB zusätzlich Runtime-Renderbarkeit.
Synthetischer E1-X-Static-Root wird nicht als importierter Bone gezählt.

## 31. JSON Output

Kleines versioniertes JSON-Objekt mit `issues[{severity,code,context,message}]`,
`errors` und `warnings`. Fehlerwerte NaN/Inf werden als null ausgegeben, damit auch
Fehlerberichte syntaktisch gültig bleiben. Batch erzeugt ein JSON-Objekt pro Asset
(JSON Lines), womit eine einfache Manifestgrundlage vorhanden ist.

## 32. Batch Conversion

Rekursiver Scan ausschließlich unterstützter Quellen; relative Ordner bleiben
erhalten. Sortierte Reihenfolge, keine Symlink-Dateien. Ausgabe darf nicht unter
der Eingabe liegen. Kollisionsprüfung erfolgt vor Konvertierung: gleiche Basenames
in getrennten Ordnern sind erlaubt; `same.obj` und `same.dae` im gleichen Ordner oder
bereits vorhandene GLBs werden abgelehnt. Case-Kollisionen werden konservativ auch
auf case-sensitiven Systemen erkannt. Incremental-Hashes/Timestamps sind aufgeschoben.

## 33. Determinism

Wiederholte Konvertierung desselben Marktstands ergibt byteidentische GLBs bei
gleichem Tool/Optionen; geprüft sowohl API als auch CLI. Reihenfolge, Jointtie-Breaks,
Namensfallbacks und Generatorversion sind fest. Unterschiedliche Compiler-/Assimp-
Versionen oder Plattformen erhalten keine unbewiesene Byteidentitätszusage.

## 34. Failure Semantics

Exit 0 Erfolg, 2 CLI-Nutzung, 3 Import/Validierung/Exportinhalt, 4 Ausgabedatei.
Tests: missing/unsupported/corrupt source, leere Szene, defekte Indices/Texturen,
negative/NaN-/Nullgewichte, falsche Bones, zyklische Hierarchie, NaN/Inf,
ungültige Animation und nicht beschreibbarer Ausgabeparent. Ausnahmebehandlung
um die CLI und Import-/Exportgrenzen fängt unter anderem Allocationfehler.

Quellen und kombinierte Assimp-I/O-Reads maximal 256 MiB. Neutraler Processingbestand
maximal 256 MiB geschätzte Streams; insgesamt und nach Instanzexpansion maximal
4.194.304 Vertices/Indices, 32.768 Objekte, Hierarchietiefe einschließlich generierter
Exportnodes maximal 256. Mesh-Einzelmengen werden vor Assimp-Postprocessing geprüft;
Aggregate/Instanzexpansion nach Import und vor meshoptimizer in der neutralen Verarbeitung.
Diese Grenzen sind kein hartes Betriebssystem-Speicherlimit für den internen Assimp-
Parser; gezielt bösartige Parserinputs wurden nicht gefuzzt.

## 35. Path Portability

Native `std::filesystem::path`-Streams, Windows-`wmain` und UTF-8-Assimp-I/O-Adapter.
Pfadtests mit Leerzeichen, Unicode, relativen Bildpfaden und Backslashes bestehen.
Keine feste Laufwerksannahme im Toolcode. Nur lokale Abnahmebefehle nennen diesen
Workspace. Fehlende Ausgabeordner werden bei Einzelkonvertierung nicht still angelegt;
Batch legt seine geprüfte Ordnerstruktur an.

## 36. Build Structure

`M2_BUILD_ASSET_TOOL=OFF` ist Default: reguläre Clientbuilds laden/bauen Assimp nicht.
Mit ON: `AssetToolCore`, `ZiiNANAssetTool` (Dateiname `ziinan-asset-tool`) und kurze Tests.
Der Tool-Core ist C++20; Assimp verwendet seine eigenen C++17-Einstellungen.
Windows x64 ist produktiver Nachweis, portable Builds benötigen keinen Client.

```powershell
cmake -S . -B build-e2x-tool -A x64 -DM2_BUILD_WINDOWS_CLIENT=OFF -DM2_BUILD_RENDERER_TESTS=OFF -DM2_BUILD_ASSET_TOOL=ON
cmake --build build-e2x-tool --config Release --target ZiiNANAssetTool AssetToolTest
ctest --test-dir build-e2x-tool -C Release -L e2x --output-on-failure
```

Erste Konfiguration braucht die gepinnten Downloadarchive oder lokale Quellen über
`FETCHCONTENT_SOURCE_DIR_ZIINAN_ASSIMP` und `FETCHCONTENT_SOURCE_DIR_ZIINAN_MESHOPTIMIZER`.
Danach offline baubar. Abnahme verwendet den vorhandenen Windowsbuild
`build-c3x/windows`, um Diligent/Granny unverändert mitzubenutzen.

## 37. Runtime Assimp Isolation

CTest sucht Includes/Links im gesamten `src/`. CMake prüft zusätzlich die tatsächliche
transitive Runtime-Linkclosure, auch private statische Links. Ergebnis: **null
Assimp-/meshoptimizer-/AssetTool-Abhängigkeiten**. Auditdatei:
`build-c3x/windows/asset-tool-runtime-isolation.txt`. `dumpbin /DEPENDENTS` bestätigt
keine Assimp-DLL. Statische Isolation ist zusätzlich durch identisches Release-Client-
Binary zu E1-X bewiesen: SHA256
`DD64E397AF59E7785E0F16C07B8A060157A5EDB16AF5165F333BE10F51DE872F`.

## 38. Roundtrip Proof

Frisches OBJ -> Assimp -> neutrale Verarbeitung/meshoptimizer -> GLB -> normaler
ResourceManager/AssetRuntime/GlTFProvider -> Modell-/Materialconsumer -> echte
Diligent-Draws und Pixelreadback. `AssetTool.DiligentRoundtrip` benutzt den vorhandenen
GlTFRenderTest mit separatem Testmodus; alle E1-X-Defaultprüfungen bleiben erhalten.
Zusätzlich echter Client in Originalwelt: siehe 42 und 51.

## 39. OBJ Result

Simple Quad, fehlende Normalen, Texturpfade, Alpha mit authored Normalen und
Marktstand mit 21 Teilen geprüft. Vier verwendete Materialien, 504 Vertices,
756 Dreiecksindices und ein eingebettetes PNG bleiben erhalten. Missing MTL/Bild
und kaputte Indices werden diagnostiziert. Kein GLB benötigt die Source-Texturen.

## 40. DAE Result

Zentimeter-/Node-Fixture: (100,200,300) cm Translation und x-Scale 2 ergeben
Meterbounds **(1,2,3) bis (3,3,3)**. Skin-Fixture: zwei benannte Joints,
Inverse-Binds, normalisierte Gewichte und 2-Sekunden-TRS-Clip gelangen korrekt in
E1-X-Metadaten; das Model bleibt dort ausdrücklich nicht renderbar.

## 41. FBX Result

Eigenes kleines ASCII-FBX 7.4 mit Zentimeter-Metadaten und Translation ergibt
Meterbounds **(1,2,3) bis (2,3,3)**. Zusätzlich geprüft: `UnitScaleFactor=100`
ändert den Quellmaßstab tatsächlich auf Meter und verhindert eine versteckte
fest verdrahtete Zentimeterannahme. Kein fremdes oder großes Source-FBX eingebracht.

## 42. Realistic Asset Proof

Eigenes Marktstand-Fixture, keine bloße Triangle-Demo: 21 Holzteile/Pfosten,
rote Markise, sieben helle Fransen, Schild und drei grüne Warenkisten. 2,9 m breit,
2,47 m hoch. Die vollständig selbst erzeugte Geometrie und 108-Byte-Textur haben
keine fremden Assetlizenzabhängigkeiten. Native Readbacks:
`build-c3x/windows/tests/AssetTool/e2x-converted-stall-static-camera-{0,1,2}.bmp`.

Die Clientkopie `build-e2x/runtime/e2x-auto-02` konvertiert neu, packt GLB und winzige
MSM ausschließlich in ihr privates Rootpack und instanziiert den Stand als lokalen
rigiden NPC 57999 neben den originalen Player/NPC/Mob/Boss/Mounts. Originalpacks
werden nur lesend verwendet; produktive Map-/Race-/Rootdaten bleiben unverändert.

## 43. Visual Sanity

Die drei nativen Diligent-Readbacks wurden gegen die authored Fixture-Geometrie
geprüft: aufrechte Pfosten, rote Markise, helle Leiste/Schild, grüne Kisten und
asymmetrisches blau/weißes Texturdetail. Maßstab und Bounds stimmen.
Originalwelt-Screenshot `e2x-world-0-near-0914_144649.jpg` zeigt GLB neben GR2-Player
und NPC; der Rand schneidet einen Teil des Stands ab. Der separate native Readback
zeigt das vollständige Objekt. Kein pixelperfekter DCC-Referenzvergleich behauptet.

## 44. Optimization Stats

| Asset | Quellbytes | Vertices vorher/nachher | Indices vorher/nachher | GLB-Bytes | modellierte ACMR vorher/nachher |
|---|---:|---:|---:|---:|---:|
| Marktstand OBJ | 21.885 (+241 MTL +108 PNG) | 504 / 504 | 756 / 756 | 48.744 | 2,0 / 2,0 |
| Mittleres PLY-Gitter | 203.603 | 4.225 / 4.225 | 24.576 / 24.576 | 201.328 | 1,015625 / 0,631104 |

Beim kleinen Stand gibt es keinen gemessenen Cachegewinn; seine geteilten Quad-
Face-Corners sind bereits lokal. GLB ist dort größer als die Quelle, da Floatdaten,
Normals, Node-/Materialstruktur und Bilder eingebettet werden. Keine Kompressions-
oder FPS-Werbezahlen. Beim Gitter verändert sich nur Reihenfolge/Fetchlayout.
Einmalige Prozesssanity inklusive Start: einfaches OBJ 28,46 ms, Stand 17,57 ms,
mittleres PLY 19,47 ms. Keine statistische Benchmarkreihe; OS-Cache/Parallelbuilds
können die Werte beeinflussen. Belege `build/e2x/sanity-*.json`, `sanity-timings.csv`.

## 45. E1-X Regression

Zehn relevante bestehende Assettests bleiben grün. E1-GLB-Parsing, eingebettete
Materialien, normale Consumer, Wide Indices, beide vorhandenen GLB-Renderfixtures,
Provider-/Headergrenzen und Granny-Parität sind im Gate enthalten. Keine Runtime-
Quelländerung; Release-Client ist byteidentisch zur abgeschlossenen E1-X-Abnahme.

## 46. GR2 Regression

Kurzer Originalwelt-Smoke: drei Phasen a1 -> b1 -> a1, Near/Far/Near,
Player, NPC 9003, Mob 101, Boss 691, Mount 20101 plus Rider/Mount 20104,
Armorwechsel, Hair und Weapon. Normale World-/CameraBlocker- und Actor-Submissions
werden in Logs geprüft. Keine komplette Phase-B-Suite und kein Langzeitlauf.

## 47. GPU Skinning

Default `Skinning=gpu`, `SkinningSelection=default`. Auto-Smoke:
GPUFrames=17.697, PaletteUpdates=14.269, AllCPUDeformationCalls=0,
AllCPUDeformationVertices=0, GPUFallbacks=0, SkinPreparationFailures=0.
Die historische Diagnoselabel `CPU-skinned main body` wird vom bestehenden Actor-
Bridge auch für rigide GLB-Bodies benutzt; `deform_vertices=0`, `rigid_vertices=504`,
`rigid=1` und die globalen Nullzähler belegen den tatsächlich rigiden Pfad.

## 48. Release Build

**PASS:** Tool, Core, Import-/Unit-Tests, echter Diligent-Test und Client gebaut.
E2-X- plus relevante E1-X/D1-X-Tests **16/16**, 3,99 s.
Logs: `build/e2x/release-acceptance-build.log`, `release-acceptance-tests.log`; erster kompletter
Client-Targetbau in `release-build.log`. Das bestehende Client-Binary wurde nicht
inhaltlich verändert. Bekannte externe CMake-/PDB-Warnungen werden nicht als neue
Toolfehler oder pauschal warnungsfreier vollständiger Clean-Build dargestellt.

## 49. Debug Build

**PASS:** dieselben Targets einschließlich Client in Debug gebaut.
Relevantes Gate **16/16**, 4,20 s. Logs `build/e2x/debug-acceptance-build.log`,
`debug-acceptance-tests.log`. Enthalten sind alle finalen Diagnose-/Normalen-/
Interpolationsergänzungen samt gezielten Tests. Kein weiterer großer Testlauf.

## 50. GCC/LP64

Separates Cygwin/GNU-12.4-LP64-Toolbuild unter `build/e2x/gcc`, ohne Windowsclient
oder Diligent. Quellen aus demselben gepinnten Cache. Assimp benötigt unter Cygwin
gezielt `<strings.h>`, `_DEFAULT_SOURCE` und für seine C-minizip-Objekte
`MINIZIP_FOPEN_NO_64` (Cygwin-`off_t` ist schon 64 Bit). Diese Optionen gelten nur
für das Tool-Assimp-Target. **PASS: 12/12**, 0,91 s; acht vorhandene portable
Platform-/AssetRuntime-Tests und vier neue Tooltests. Vollständiger Neubau der
finalen Gate-Konfiguration 82,61 s. Native Windows-CMake/Cygwin-Testpfade werden nur
unter CYGWIN relativ übergeben. Belege `build/e2x/gcc-build-final-gate.log` und
`gcc-tests-final-gate.log`. Keine projekteigene GCC-Portabilitätslücke offen.

## 51. Runtime Smoke

**PASS:** eigener Release-Prozess PID 63584, 61,5 s, 3.582 Renderframes,
regulärer Exitcode 0. Private Copy/GLB-SHA-Abgleich, drei Phasen, sechs Screenshots,
GLB-Datei-/Materialsubmissions und paralleler GR2-Pfad geprüft.
`tests/AssetTool/run_runtime_smoke.ps1 -Name e2x-auto-02 -BuildDirectory build-c3x/windows -AuditOnly`
prüft den vorhandenen Lauf ohne Wiederholung. `build/e2x/runtime-audit.log` ist grün.
Ein anfänglicher Auditregex erwartete `/`, während Runtimepfade `\` enthalten;
der korrigierte Audit akzeptiert beide und prüft denselben unveränderten Lauf.

**Finaler Lauf nach den Reviewkorrekturen ebenfalls PASS:**
`build-e2x/runtime/e2x-auto-final`, PID 69200, 61,4 s, Exitcode 0,
GPUFrames=17.728, PaletteUpdates=14.294, CPU-Deformation/Fallbacks/überwachte
Restressourcen=0. Frischer `syserr.txt` leer. `build/e2x/runtime-final-smoke.log`
enthält den vollständigen bestandenen Audit. Die finale GLB-Datei ist byteidentisch
mit dem finalen Release-CTest-Output: SHA256
`88C8FA349FA4FDC93DA7934504CC980D0ADB9AA3C1C08CA05242E857D2F0BD3F`.
Release-Tool SHA256:
`C0F73D6F4D96AF4F624A5857E10464B9B8571B3CF3C738866B9625A0E3177D57`.
Identitäten und Vergleich zur manuellen Geometrie: `build/e2x/artifact-identities.json`.

## 52. Resize/Minimize

Automatische Diligent-Tests in Release/Debug: Resize 640->720->640,
Minimize/Restore über 0x0-Suspend, weitere Draws/Readbacks erfolgreich.
Die private manuelle Kopie `build-e2x/runtime/e2x-manual-01` wurde geöffnet,
normaler Login/Character Select bis Ingame erfolgt. Ihr privater Testhook hat den
frisch konvertierten Stand gespawnt und `e2x-manual-ingame-0914_145005.jpg` erzeugt.
**Manuelle Gesamtabnahme positiv:** Auf die konkrete Checkliste Login -> Character
Select -> Ingame/Stand/GR2-Welt -> Resize -> Minimize/Restore -> X antwortete der
Nutzer: **„Alles geprüft und korrekt“**. Der eigene Ingame-Screenshot zeigt den
texturierten Stand gemeinsam mit Player, originalen Mobs, Welt und UI.

## 53. Shutdown

Auto-Smoke Exitcode 0. Nativer Renderer-Test löst Instanzen, Materialien, Geometrie,
Caches und Backend regulär auf. **Manueller Client PID 69268 ebenfalls regulär
beendet:** `renderer-startup.log` enthält ExitCode=0. Frischer Shutdown-Audit meldet
CPU-Deformation/Fallbacks/alle überwachten Ressourcen=0; Beleg `build/e2x/manual-audit.json`.
`syserr.txt` enthält genau 34 Bytes: einmal die schon bei E1-X dokumentierte
`invalid idx 0`-Gildenmarkendiagnose aus dem unveränderten MarkManager. Kein
Provider-/Rendererfehler. Prozessende allein wird nicht als Ressourcenbeweis verwendet.

## 54. Resource Lifetime

Auto-Smoke: SourceTextures, SourceBuffers, SkinMeshes, BoneRemaps, BonePalettes,
AssetDocuments, AnimationInstances, MeshBindings, PrototypeGeometry,
PrototypePalettes und StaticSkinMeshes nach Shutdown jeweils **0**.
Native GLB-Tests prüfen unveränderte Live-Counts über mehrere Draws und vollständig
freigegebene Dokument-/Binding-/GPU-Owner. Keine neue Runtime-Assimp-Lebensdauer.

Der manuelle Lauf endet zusätzlich mit GPUFrames=1.502.421,
PaletteUpdates=1.497.345, CPU-Deformation=0 und Fallbacks=0 sowie allen genannten
überwachten Ressourcen=0. Automatische und manuelle Belege bleiben getrennt.
Die letzte Normalisierung nach der manuellen Sichtprüfung änderte ausschließlich
72 NORMAL-Floatkomponenten eines Meshes um maximal 5,96e-8; GLB-JSON, Geometrie,
UVs, Materialien und Texturen sind identisch. Diese finalen Daten wurden nochmals
nativ in Release/Debug gerendert und im finalen 60-Sekunden-Client-Smoke geprüft.
Die manuelle Fensterbedienung bezieht sich auf dasselbe byteidentische Client-Binary.

## 55. Known Limitations

- E1-X-Sampler-/Material-/Bild-/Skin-Grenzen bleiben verbindlich; unsupported Fälle
  sind diagnostiziert. OBJ/PLY/STL besitzen dokumentierte Achsen-/Unitdefaults.
- Ein gemeinsames Skin; unterschiedliche Inverse-Bind-Spaces für denselben Joint
  werden abgelehnt. Skins/Clips sind Daten, keine neue Runtime-Animation.
- Assimp kann Authoringdetails schon beim Import normalisieren oder Keys backen;
  kein allgemeiner verlustfreier Roundtrip beliebiger DCC-Szenen behauptet.
- Keine externe DCC-Referenzsession, Skin-Visualisierung oder automatischen
  Skinned-LODs; keine Mesh-Kompression, Texturkompression, Incremental-DB.
- Der Sourcegrößen-/Processing-Budgetcheck ersetzt kein OS-Speicherlimit um Assimp.
  Keine Fuzzer, Langzeitstresstests oder kompletten GPU-Benchmarks durchgeführt.
- GLB enthält keine Ausgangspfad-Referenzen; `asset.generator` enthält die Toolversion.
  Vollständige Animationsextrapolation und nicht unterstützte Kurventypen sind begrenzt.

## 56. E3/F Roadmap Notes

Nur dokumentiert: versioniertes Batchmanifest/Contenthash-Cache, zusätzliche
Quellenachsenoptionen, weitere nachgewiesene Sampler/Imageformate, reviewed statische
LOD-Pakete. Eine eigene Animation Runtime ist ein gesonderter späterer Auftrag.
Kein GR2-Konverter, ozz-animation, Granny Replacement, Mixer, Pose-Evaluator, World
Editor, Android-Toolport oder Visual Remaster wurde begonnen.

## 57. Git Diff

Nur Root-CMake opt-in, neue Tool-/Test-/Fixture-/Dokumentationsdateien, passender
cgltf-Writer und schmaler optionaler Modus im bestehenden GlTFRenderTest.
`src/` unverändert. Abhängigkeitssourcen/-archive, Binaries, PDBs, Runtimekopien,
temporäre GLBs/Bilder und Logs liegen in ignorierten Buildverzeichnissen.
Abschließender `git diff --check` **PASS**, zusätzlich Whitespaceprüfung jeder
neuen Textdatei mit `git diff --no-index --check`. Diff: **3 bestehende Dateien
geändert (+22/-7), 33 neue Dateien** einschließlich Report, Tool, Tests/kleiner
Sourcefixtures, Lizenzen und unverändertem cgltf-Writer. Unversionierte Dateien
sind explizit erfasst, obwohl normales `git diff --stat` sie nicht mitzählt.
Clientstatus entspricht der Baseline. Statusbeleg `build/e2x/final-source-status.txt`.
Kein Commit, Push, Reset, Rebase, Restore, Clean oder Stash.

## 58. GO/NO-GO

**E2-X GO.** Eigenständiges Tool, offline isoliertes Assimp, FBX/OBJ/DAE-Import,
deterministische Koordinaten/Units, GLB-Writer und Validator, Material-/Bilddaten,
meshoptimizer, echte finale GLB-Client-/Diligent-Darstellung und paralleles GR2 sind
nachgewiesen. Release **16/16**, Debug **16/16**, GCC/LP64 **12/12** bestanden.
Nutzer bestätigt die konkrete manuelle Gesamtliste; manueller und finaler
automatischer Shutdown jeweils Exit 0 und null überwachte Restressourcen,
CPU-Deformationen und Fallbacks. Der Runtime-Client bleibt byteidentisch zu E1-X.
Die dokumentierten Import-/Animations-/LOD-Grenzen sind keine Zusage späterer
Runtimefunktionen. **Nach E2-X STOP. Kein Commit/Push, E3 oder Phase F.**
