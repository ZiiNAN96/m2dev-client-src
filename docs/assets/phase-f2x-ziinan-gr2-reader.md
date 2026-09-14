# F2-X – Native ZiiNAN GR2 Reader

Stand: 2026-09-14. F2-X ist ein opt-in Reader-Nachweis. Production Default bleibt Granny. Keine F3-Migration, kein F4-Removal, keine F5-Arbeit; kein Commit/Push.

## 1. Corpus Audit

Originalbestand: `../m2dev-client/assets`, rekursiv 9.166 GR2-Dateien. Keine proprietären Dateien wurden ins Source-Repository übernommen. Der Scanner liest Header, Sections, Relocations, typisierte Objekte, Models/Meshes und Animationskurven. Er rendert und bindet nicht jeden Clip an jedes mögliche Zielskelett. **Parsed ist daher keine Behauptung vollständiger visueller oder Controller-Parität für alle Dateien.**

Reproduzierbar: `GR2CorpusScanner <asset-root> <report-directory>`. `files.tsv` enthält jede Datei mit Status und Fehlertext, `summary.tsv` die Histogramme. Finale Scan-Evidenz: `build/f2x/corpus-final/`, Laufzeit 33,557 s. Validierte Metadaten-Histogramme schließen fünf Dateien mit falscher deklarierter Dateigröße aus; semantische Histogramme zählen nur erfolgreich gelesene Dokumente.

## 2. Version Matrix

| Variante | Header-Inventar | Vollständig validierte Header | Reader |
|---|---:|---:|---|
| v6, alte 32-Bit-Magic | 9.155 | 9.150 | unterstützt |
| v7, neue 32-Bit-Magic | 11 | 11 | unterstützt |
| 32-Bit-Disk-Pointer, Little Endian | 9.166 | 9.161 | explizites Byte-Decoding |
| Big Endian / 64-Bit-Disk-Pointer / andere Versionen | 0 | 0 | explizit abgelehnt |

Magic (Bytefolge): v6 `b867b0caf86db10f84728c7e5e19001e`, v7 `29de6cc0baa4532b25f5b7a5f666e2ee`. Section-Array relativ zum File-Info-Header: 56 bzw. 72 Bytes. Validierte Tags: `0x8000000f` 44, `0x80000010` 9.102, `0x80000011` 4, `0x80000032` 1, `0x80000037` 1, `0x80000039` 9. Tags sind Metadaten; die tatsächlichen Typdeskriptoren bestimmen das Layout.

## 3. Section Layout

Alle Dateien haben acht Sections. Slotnamen beschreiben die beobachtete Legacy-Nutzung, keine zusätzliche Formatheuristik. Nichtleere Sections in den 9.161 validierten Headern:

| Slot | Beobachtete Nutzung | Nichtleer |
|---|---|---:|
| 0 | Main/Objects | 9.161 |
| 1 | Rigid vertices | 2.793 |
| 2 | Rigid indices | 2.793 |
| 3 | Deformable vertices | 1.745 |
| 4 | Deformable indices | 1.745 |
| 5 | Texture data | 187 |
| 6 | Types/discardable data | 9.161 |
| 7 | unbenutzt | 0 |

Alignment: 54.966 Sections mit 4, 18.322 mit 32 Bytes. Der Reader patcht keine Host-Pointer und benötigt deshalb keine künstliche LP64-Strukturkonvertierung.

## 4. Compression Matrix

| Typ | Gesamtes Header-Inventar | Validierte Header | Unterstützung |
|---|---:|---:|---|
| 0 / raw | 9.166 | 9.161 | exakt gleiche Ein-/Ausgabelänge |
| 2 / Oodle1 | 64.162 | 64.127 | eigener begrenzter Decoder |
| andere | 0 | 0 | `unsupported compression`, kein Fallback |

Oodle1 verwendet drei durch Stop-Offsets begrenzte Blockmodelle. Arithmetic-Intervalle, Symbolkapazitäten, Distanzen, Ausgangsgrenzen und Modellparameter werden geprüft. Ein maximal vier Byte großes, lokal mit Null gefülltes terminales Lookahead-Wort verursacht keinen Zugriff außerhalb des Inputs. Die tatsächlichen Dateien `07200.gr2` und `gnoll_boss_lod_01.gr2` prüfen diesen Randfall bzw. uninitialisierte Encoder-Paddingbits zusätzlich gegen Granny.

Der Arithmetic/LZ-Algorithmus ist eine sicherheitsgeprüfte Adaption der unter **Boost Software License 1.0** veröffentlichten [nwn2mdk-Implementierung](https://github.com/Arbos/nwn2mdk/blob/master/nwn2mdk-lib/gr2_decompress.cpp), mit deren xoreos-tools-Provenienz. Attribution im Quelltext, vollständige Lizenz unter `src/AssetRuntime/GR2/LICENSE-Boost.txt`. Kein Granny-SDK-Code, SDK-Decompressor oder SDK-Allocator im Reader. Weitere Format-Recherche: [blendergranny](https://github.com/Rasetsuu/blendergranny/tree/main/io_scene_gr2/gr2); nicht als Laufzeitabhängigkeit eingebunden.

## 5. Relocations

6.057.774 Fixups und 3.118 Marshalling-Einträge in den validierten Headern. Fixups werden als geprüfte `(section, offset)`-Handles gespeichert. Quellwort und Zieladresse müssen innerhalb expandierter Sections liegen; doppelte Fixup-Quellen scheitern. Ein nichtleeres Pointerwort ohne Fixup scheitert. Bei Arrays mit Count 0 wird das unbenutzte Pointerwort nicht ausgewertet. Marshalling-Referenzen/Counts werden geprüft; native Strukturkonvertierung wird nicht ausgeführt.

## 6. File Safety Model

Fixed-width Integer und explizites Little-Endian-Lesen; kein `reinterpret_cast` einer Datei auf SDK-/Hoststrukturen. Checked Addition/Multiplikation über Offset/Size-Prüfungen. Header, Payloads, Relocation- und Marshallingbereiche dürfen nicht überlappen. CRC32 wird ab Ende des versionsabhängigen File-Info-Headers geprüft. Deklarierte Größe muss exakt stimmen.

Grenzen: Input 256 MiB; gesamte Expansion 256 MiB; zusätzlich kumulatives typisiertes Arbeits-/Allokationsbudget 256 MiB, einschließlich mehrfach referenzierter Arrays; höchstens 64 Sections, 4 Mio. Fixups/Arrayelemente, 4.096 Typen, 65.536 Typmember, 512 Member pro Typ, 64 Ebenen für Inline-/Materialgraphen und 4.096 Zeichen pro String. Animationsimport: 600 s Dauer, 65.536 Knots pro Curve, 2 Mio. erzeugte Keys, begrenzte Unterteilung. Alle konsumierten Floatwerte müssen endlich sein. Grenzverletzungen liefern eine Diagnose statt eines Fallbacks. Das ist ein deterministisch geprüfter Parser, kein behaupteter vollständiger Sicherheitsbeweis.

## 7. Reader Architecture

`GR2File` enthält Header/Sections/Relocations; `GR2Compression` den Decoder; `GR2Types` den typisierten Objektzugriff; `GR2ModelReader` Model und Skeleton; `GR2MeshReader` Geometrie/Remaps; `GR2MaterialReader` Legacy-Materialien; `GR2AnimationReader` Curves und Clipimport; `GR2AssetProvider` besitzt Daten und bindet sie an bestehende Interfaces. Zusammengefasste Module vermeiden unnötige Mini-Dateien.

`AssetRuntimeGR2` linkt nur `AssetRuntime` und `AnimationRuntime`, keine Granny-/Windows-/Diligent-Bibliothek. Source-Architekturtest und GCC-Build prüfen diese Grenze. `GR2CorpusScanner` linkt ebenfalls nur diesen portablen Pfad.

## 8. Models

4.402 Model-Datensätze in erfolgreich gelesenen Dokumenten. Namen, Meshbindungen, Skeleton-Zuordnung und InitialPlacement werden aus den Typdeskriptoren gelesen. InitialPlacement bleibt Reader-Metadatum; der bestehende Consumer verwendet weiterhin seine vorhandene Transformationskonvention. Die Referenzprüfung vergleicht alle InitialPlacement-Komponenten exakt. Leere Model-/Skeleton-Metadaten in Animationsdateien bleiben gültige Adapter-Datensätze; ihre null Meshes erzeugen keine Draws. Das erhält insbesondere Sura-Clips mit leeren Model-Records.

## 9. Meshes

7.318 Meshes, direkt gelesene Positions-/Normal-/UV-Streams, Bounds, Materialgruppen und Bone-Bindings. Canonical Upload-Puffer gehören dem Dokument. Morph Targets werden explizit als nicht unterstützt abgelehnt.

## 10. Vertex Formats

| Disk-Stride | Layout | Meshes |
|---:|---|---:|
| 32 | float3 Position, float3 Normal, float2 UV0 | 3.958 |
| 40 | float3 Position, normalized-u8x4 Weights, u8x4 BoneIndices, float3 Normal, float2 UV0 | 3.071 |
| 40 | float3 Position, float3 Normal, float2 UV0, float2 UV1 | 289 |

Komponententypen und Breiten werden geprüft. Unbekannte Komponenten oder gewichtetes Dual-UV werden nicht still interpretiert. Die neutralen Uploadformate bleiben die bereits vorhandenen PNT/PWNT/PNT2-Formate.

## 11. Indices

Alle 7.318 erfolgreich gelesenen Meshes besitzen 32-Bit-Quellindizes. Reader unterstützt außerdem explizite 16-Bit-Streams. Anzahl durch drei teilbar, jeder Index innerhalb Vertexcount; Gruppenbereich geprüft. Exakte Referenzprüfung erfolgt für 16- und 32-Bit-Kopien. Ein neutraler `preferredIndexWidth` erlaubt nach vollständiger Werteprüfung den vorhandenen 16-Bit-Upload. Dies ist für den bestehenden Actor-GPU-Pfad erforderlich. Zu breite skinned Indizes werden explizit abgelehnt; GPU-Skinning selbst bleibt unverändert.

## 12. Materials

6.209 Model-Materialien: Namen, Diffuse/Opacity, Matching-Texture-Pfade, Blend-Material-Reihenfolge, `Two-sided` und vorhandene Culling-/Stage-Semantik. Rekursive Map-Verweise sind zyklus- und tiefenbegrenzt. Keine PBR-Erweiterung.

## 13. Texture Paths

6.718 nichtleere Texture-Stage-Referenzen. Strings einschließlich `d:/ymir work/...` werden unverändert übernommen. Pack-Auflösung und Legacy-Ladewege bleiben bestehender Consumer-Code. Eingebettete Texture-Section-Daten werden sicher expandiert, jedoch nicht zu einem neuen Texture-Ladesystem ausgebaut. Der reale Client-Smoke verwendet Originalpakete/Originaltexturen; die isolierten GPU-Testprogramme verwenden wie die F1-Harnesses eine Diagnosetextur.

## 14. Skeleton

4.402 Skeleton-Datensätze, insgesamt 120.529 Bones. Name, ParentIndex, LocalBind und InverseBind werden direkt gelesen. Die unveränderte Runtime validiert einen zusammenhängenden Root, eindeutige Bone-Namen und gültige Hierarchie. Mehrere Roots bleiben ein dokumentierter Unsupported-Fall.

## 15. Bone Order

Originalreihenfolge und Bone-IDs bleiben erhalten; keine Sortierung oder Neunummerierung. Exakte Vergleiche von Namen, Reihenfolge, Parents und sämtlichen Bind-Matrix-/Transformkomponenten für alle Referenzmodels.

## 16. Skin Weights

Vier Byte-Influences, Summe exakt 255, nichtnull gewichtete Bone-Indizes innerhalb der Bone-Bindings. Nullgewichtige Slots müssen nicht künstlich auf Bone 0 umgeschrieben werden. Exakte PWNT-Byte-Parität prüft gleichzeitig Positionen, Normals, UVs, Weights und Indizes.

## 17. Remaps

205.646 Bone-Bindings im gelesenen Bestand. Zuordnung ausschließlich über explizite Bone-Namen und Originalreihenfolge. Hair/Armor-LOD und Weapon verwenden die bestehenden Pose-Owner-/Attachment-Consumers. Kein SDK-Remap und keine Namensheuristik im Reader.

## 18. Animation Format

5.158 Animationen, 6.651 Track Groups, 362.787 Transform Tracks. Duration, TimeStep, Namen, Gruppen, InitialPlacement, Text Events, AccumulationFlags und LoopTranslation werden direkt gelesen. Gruppenbindung erfolgt exakt anhand des Modelnamens; `NoMatchingTracks` bleibt für an starre Attachments verteilte Clips ein normaler, expliziter Status.

## 19. Curve/Track Formats

Alle erfolgreich gelesenen Disk-Curves besitzen Legacy-Float32-Felder `Degree`, `Knots`, `Controls`; die SDK-interne Konvertierung in modernere Curve-Container ist kein Diskformat-Beweis.

| Dimension | Degree | Art | Anzahl |
|---:|---:|---|---:|
| 3 | 0 | konstant / identity | 319.420 / 2.453 |
| 3 | 2 | Spline | 40.914 |
| 4 | 0 | konstant / identity | 78.268 / 76.224 |
| 4 | 2 | Spline | 208.295 |
| 9 | 0 | konstant / identity | 41.845 / 317.943 |
| 9 | 1 | Spline | 2.999 |

AccumulationFlags: 0 bei 30 Gruppen, 2 bei 5.190, 3 bei 1.431. Moderne Float-Curve-Form 1 ist lesbar; andere moderne komprimierte Curveformen und Scalar-/Vector-Tracks werden explizit abgelehnt. Keine theoretische Vollimplementierung sämtlicher GR2-Varianten.

## 20. Animation Decode

Eigene Auswertung der ursprünglichen Spline-Koeffizienten beim Import, adaptive Unterteilung an Knots mit Mittelpunkt-/Viertelpunktprüfung in bestehende lineare Runtime-Tracks. Keine Granny-Samples, keine temporäre SDK-Bake-Strecke, keine zusätzliche Frame-Animationsengine. Unterteilungsziel 1e-5 für Translation und 5e-7 für Quaternion-/Scale-Komponenten plus Float-Rundungsgrenze; begrenzte Tiefe und Keyanzahl. Diese Fehlerkontrolle ist keine formale globale Spline-Fehlerschranke; die numerische Parität ist zusätzlich separat gemessen.

Loop-Nachbarschaften werden bei Bedarf getrennt importiert (vorheriger/nächster Loop). Endliche Clips erhalten geklemmte Randdaten. Lineare Root-Bewegung ergibt sich aus direkt gelesener `LoopTranslation / Duration`, einschließlich Geschwindigkeit und vorhandener Weltmatrix. PeriodicLoop/RootMotion-Objekte werden ausdrücklich abgelehnt.

## 21. AssetRuntime Integration

`LoadModel` wählt für `.gr2` genau einen Provider. `--gr2-reader=ziinan` schaltet Reader und Animation Runtime auf ZiiNAN; widersprüchliche explizite Animationseinstellungen scheitern. Standard bleibt `granny`; GLB-Routing bleibt unverändert. Fehler enthalten Dateikontext/Diagnose, kein stiller Granny-Fallback.

## 22. AnimationRuntime Integration

Unveränderte `RuntimeSkeleton`, `RuntimeAnimationClip`, `Sample`, `Blend`, `Evaluate`, `BuildPalette`. Der Reader-Provider implementiert Clock/Loop/Speed, Endposition, Motion Copy, einfache Überblendung und Root-Translation ohne SDK. Fertig gebundene Clips gehören dem Animation-Dokument und werden für identische Animation-, Track-Group- und Skeleton-Binding-IDs zwischen Instanzen wiederverwendet. Grenze je Dokument: 64 Bindings und 64 MiB Key-/Track-Speicher; bei Überschreitung bleibt die Bindung korrekt, wird aber nicht zusätzlich gehalten. Es gibt keinen globalen Reader-Cache, Fingerprinting oder Retention über die Dokument-Lebensdauer hinaus. Fehlerhafte Bindung invalidiert die alte Pose. Beliebige verschachtelte Mehrfach-Überblendungen sind nicht als vollständig Granny-paritätisch nachgewiesen. Der native Rendernachweis verwendet die produktive GPU-Einstellung; ein eigener CPU-Deformer wird nicht hinzugefügt.

## 23. Granny Reference Harness

`GR2ParityTest` besitzt einen getrennten SDK-Referenzpfad. Beide Seiten lesen dieselben Originaldateien unabhängig. Nur die Referenzseite ruft Granny auf. Der Reader-/Actor-Test selbst benötigt keine SDK-Datenextraktion. `GrannyFileReads` zählt den einzigen produktiven SDK-Dateiladeort und wird im vollständigen Client-Audit ausgegeben.

## 24. Static Data Parity

Elf Originalmodelle: Warrior, Hair, Weapon 00010, Wolf, Boss, Horse, Snow-House, Gongjakso, Weapon 07200, Gnoll-Boss-LOD und Deviltower2f mit echtem Dual-UV-Layout. Exakte Counts, Namen, InitialPlacement, Meshlayouts/Strides, Materialpfade/-flags/-gruppen, Indizes und Uploadbytes bestanden.

## 25. Skeleton Parity

Bone-Count/-Order/-Name/-Parent, LocalBind, InverseBind und Mesh-to-Skeleton-Remaps exakt. Die sichtbaren Renderpfade prüfen zusätzlich Hair mit gemeinsamem Pose-Owner, Weapon-Attachment und Body-LOD-Wechsel. Die vollständige Remap-Kombinatorik des gesamten Bestands wurde nicht getestet.

## 26. Animation Parity

13 Loop-Clips plus endlicher Warrior-Attack. Zeitpunkte: 0/10/25/50/75/90/100 %, unmittelbar vor/nach Loop-Ende und nach zwei Loops. Unveränderte F1-X-Toleranzen:

| Größe | Maximaler Release-Fehler | Grenze |
|---|---:|---:|
| Translation | 3,052e-5 | 2e-4 |
| Rotation (rad) | 1,854e-6 | 2e-5 |
| Scale/Shear | 2,384e-7 | 2e-5 |
| World-Matrix | 3,491e-4 | 2e-3 |
| Composite/Palette | 7,172e-4 | 2e-3 |
| Root-Matrix | 1,145e-5 | 2e-4 |

Root-Test: gedrehte/verschobene Ausgangsmatrix, Geschwindigkeiten 0,5/1/1,7, mehrere Clocks einschließlich Loop-Grenzen. Grenzen wurden nicht angehoben.

## 27. Vertex Parity

224.540 skalare Vertexpaare an allen Pose-Zeitpunkten und 22.454 GPU-Readback-Paare bei 25 %, mit vorhandener Skinning-/Palettenkonvention. Maximaler Positionsfehler 3,443e-4 bei Grenze 5e-3; Normalfehler 4,262e-6 bei Grenze 5e-5. Derselbe existierende GPU-Harness verarbeitet native und Referenzpaletten.

## 28. Warrior

`warrior_novice.gr2`: Wait/Walk/Run/Attack, endlicher Attack, reale Actor-Submission sowie Near/Far/Near über drei Body-LODs. Root-Bewegung separat geprüft.

## 29. Wolf

`wolf.gr2`: 00/02/03/20, Mesh-/Bone-/Pose-/Vertex-Parität und echter GPU-Actor. Wildhunde zusätzlich im vollständigen A1-Smoke.

## 30. Boss

`misterious_diseased_bosshost.gr2`: 00/20 mit Scale-/Shear-Daten; Daten-, Pose- und Vertexvergleich. Der vollständige Client-Smoke nutzt zusätzlich den bestehenden Boss-Actor 691.

## 31. Mount

`horse_normal.gr2`: 00/02/03, Root-Bewegung, Daten-/Pose-/Vertexvergleich und Actor-Rendering. Die vollständige A1-Szene enthält Horse 20101 sowie Mount 20104 mit Rider.

## 32. Hair/Attachment

`hair_1_1.gr2` und Weapon `00010.gr2`, originale Body-LOD-Varianten, bestehende Shared-Pose-/Bone-Attachment-Wege. Weder Granny-Bone-Reihenfolge noch Skinning-Konvention geändert.

## 33. Static Building/Prop

Snow-House und Gongjakso exakt gegen Granny. Gongjakso rendert direkt über `DiligentStaticObjectRenderer`, inklusive Materialgruppen, Bounding-Kamera, Pixel-Readback, Resize/Suspend/Restore. Keine Granny-Extraktion für den Assetpfad. Vollständiger A1-Smoke prüft zusätzlich Originalwelt- und Camera-Blocker-Submissions.

## 34. Corpus Scan Results

| Status | Dateien |
|---|---:|
| parsed | **9.114 (99,43 %)** |
| unsupported version | 0 |
| unsupported compression | 0 |
| unsupported curve/type | 18 |
| malformed | 34 |
| other failure | 0 |
| Gesamt | **9.166** |

Der Scanner beendet eine vollständig durchlaufene Inventur mit Exitcode 0 auch bei gemeldeten Assetfehlern. Dieser Exitcode ist **kein** 100-%-Coverage-Test; maßgeblich sind die Statuszahlen.

## 35. Unsupported Cases

18 Unsupported-Dateien: vier Multi-Root-Skeletons (beide `assassin.gr2`-Altbestände, `hair_11_1.gr2`, `warrior_rabbit1_backup.gr2`); 14 PeriodicLoop/RootMotion-Dateien (Doctor/die, Sura/run10, Crustacean Officer/Soldier walk/run, Ent Boss1/normal_attack, Ent Boss2 walk/run, Manticore Boss2/special_attack, Ogre Boss2/run, Ogre Soldier/normal_attack1, Halloween1/walk1, Pig Young1/walk).

34 Validierungsfehler: fünf Skipia-Haven-Dateien mit falscher deklarierter Größe; 20 nicht eindeutige/leere Track-Targets; sechs nicht eindeutige/leere Bone-Namen; ein leeres Redthief2-Soldier-LOD-Mesh; zwei Redthief-General-Clips mit nichtendlichen Floats. `malformed` beschreibt die Verletzung des Reader-Vertrags; es behauptet nicht, dass ein toleranter Legacy-Loader diese Datei niemals darstellen kann. Vollständige Pfade/Erstfehler in `files.tsv`; nach dem ersten Fehler wird die Datei nicht heuristisch weiterinterpretiert.

## 36. Failure Tests

30 deterministische Prüfungen: verkürzte Header, Magic/Version/Sectioncount, Offset/Overlap/Alignment/Expansion, unbekannte Compression, ungültiges Oodle1-Modell, CRC, Relocation, riesiges Array, String ohne Terminator, Meshindex, Parent/Cycle, Weight/Joint, Curve-Count/Degree, NaN und kumulatives Budget. Synthetische Fixtures werden im Test erzeugt; keine kopierten Originalassets. Zusätzlich Input-Freigabe, Upload-Freigabe, weitere Poseauswertung und Invalidierung einer alten Pose nach Bindefehler. Zusätzliche Regressionen: Bone-Matrizen vor dem ersten Frame und zwischen Motionwechsel/Kollisionsabfrage, sieben reale Sura-Dateien mit leeren Model-Records, Dokument-Lifetime beim fehlgeschlagenen End-Binding sowie gemeinsame Clipnutzung mehrerer Instanzen sowie Trennung verschiedener Track Groups bei identischem Skeleton-Layout. Kein Fuzzer.

## 37. Release

Release-Client gebaut. Vollständiges bestehendes Fast Gate: **42/42, 68,32 s**. Nach allen nativen Korrekturen F2 erneut **4/4, 18,38 s**; Quelle `build/f2x/release-final-gate.log`, Detaildaten `release-final-detail.log`. Bekannte externe LNK4099-Meldungen zu Python/zlib-PDBs sowie LTCG-Neustart-Hinweise bleiben; Build wird nicht als warning-free bezeichnet.

## 38. Debug

Vollständiger Debug-Client gebaut, vollständiges Fast Gate **42/42, 191,92 s**. Nach allen nativen Korrekturen F2 erneut **4/4, 56,60 s** (`debug-final-gate.log`, Details `debug-final-detail.log`). Der Debug-Linker meldet außerdem weiterhin LNK4098 zu LIBCMT; kein warning-free-Build.

## 39. GCC/LP64

Bestehender Cygwin/GCC-12.4-LP64-CMake-Pfad vollständig gebaut; **13/13 Tests bestanden, 1,44 s**. Native GR2-Safety und SDK-Unabhängigkeit darin enthalten. Kein Windows-/Granny-Link für den Reader-Core. Evidenz `gcc-all-build.log`, `gcc-all-gate.log`. Nach allen nativen Korrekturen erneut F2 **2/2, 0,07 s** (`gcc-final-gate.log`).

## 40. First-Use Performance

Kurze Release-Einzelmessung pro Fall (`release-sharing-detail.log`), gespeicherte Quelldateibytes vor Beginn bereitgestellt. Jeweils Model+Clip laden und erstes Binding; Referenz ruft die unveränderte vorhandene `ImportSkeleton`/`ImportAnimation`-Strecke auf, neutraler Importcache vor jeder Messung geleert. Native Seite bindet einen endlichen Clip; die alte Importstrecke erzeugt ihre bisherigen Randvarianten. Keine veränderten Cache- oder Sample-Algorithmen auf der Granny-Seite.

| Clip | Native Reader + erstes Binding | Granny → ZiiNAN Import |
|---|---:|---:|
| Warrior Attack | 49,00 ms | 535,16 ms |
| Wolf 20/Attack | 57,53 ms | 645,86 ms |
| Boss 20 | 54,99 ms | 527,23 ms |
| Horse 03/Run | 32,01 ms | 361,79 ms |

Die bisherige mehrere hundert Millisekunden große Importklasse entfällt in diesen Fällen. Dies ist kein Nachweis vollständig hitchfreier Frames, kein Disk-Cold-Cache-Benchmark und keine systematische Worst-Case-Messung über alle Clips. Wiederkehrende Bindings innerhalb desselben Asset-Dokuments werden wiederverwendet. Separater identischer GCC-Diagnosetest mit 20 Instanzen desselben Warrior-Wait-Clips: vor Dokumentnutzung 4.532,12 ms, danach 231,19 ms (einschließlich des ersten Imports). Das ist ein Wiederholungsbenchmark im portablen Build, kein zusätzlicher Release-First-Use-Wert. Die F1-Übergangsstrecke wurde nicht optimiert.

Zusätzlicher manueller Lauf mit neuen Mobs: `runtime/manual-03/animation-stalls.csv`, `animation-stall-summary.txt`, Auswertung `analysis.json`. 17.942 präsentierte Frames insgesamt, keine verworfenen Messzeilen. Unter aktiven, nicht minimierten Ingame-Displayzeilen wurden 197 Frames über 20 ms erfasst, davon 53 über 50 ms und 17 über 100 ms; die CSV enthält nur langsame Frames und ist keine vollständige Perzentilverteilung. 52 der 53 Frames über 50 ms enthalten native Lade-/Importarbeit. Größte aktive Spitze beim Welt-Eintritt: 1.009,77 ms mit 21 Clip-Imports und 896,14 ms Lade-/Importzeit. Später 778,94 ms mit 14 Imports und 694,04 ms Lade-/Importzeit. Maximaler einzelner gemessener Lade-/Importabschnitt über den gesamten Lauf: 297,40 ms. Poseauswertung dagegen maximal 0,220 ms je Instanz und 10,09 ms summiert zwischen aktiven Spielbildern. Diese Werte grenzen die verbleibenden First-Use-Spitzen von fortlaufender Poseauswertung ab: Es gibt weiterhin echte Lade-Hitches, auch wenn der Benutzer sie im erweiterten Test nur noch als minimal wahrnimmt. Die Aufzeichnung benennt keine einzelnen Assetpfade; keine Zuordnung zu einem bestimmten Mob wird behauptet. Kein weiterer Cache-/Prewarm-/Runtime-Umbau innerhalb F2-X.

## 41. Native Reader Smoke

`tests/AssetRuntime/run_gr2_smoke.ps1`: Native-Smoke bestanden, nach Spawn-Korrektur erweitert um INIT/WARRIOR/ASSASSIN/SURA/SHAMAN-Registrierung: Finaler Stand: 859 native Reads, 0 Granny-Reads, 26.888 eigene Poseauswertungen, 0 SDK-Pose/Import-Samples, 0 CPU-Deformationen/Fallbacks, 0 Fehler; Exit 0 nach 62,8 s (`runtime/native-05`). 22 native Clip-Dekodierungen, 70 dokumentgebundene Wiederverwendungen, 0 Speicherbudget-Bypasses. Granny-Regression desselben Stands: 859 Granny-Reads, 0 native Reads/Dekodierungen, 26.733 Referenz-Poseauswertungen, 0 CPU-Deformationen/Fallbacks, Exit 0 nach 62,4 s (`runtime/granny-02`). Isolierte frisch gebaute Client-Kopie, Originalpakete per Hardlink, eigener Root-Testpack, A1 mit sechs Actors, Idle/Walk/Run/Attack und Near/Far/Near. Keine Änderungen an originalen Paketen/Assets/Config. Getrennte Läufe für Native/ZiiNAN und Granny/Granny.

## 42. Manual Result

Erster Versuch `manual-01`: vom Benutzer bestätigter Absturz beim Welt-Eintritt mit Krieger, Exit `0xc0000005`. Reader-Pose-Lifetime korrigiert: Bind-Pose sofort verfügbar, gültige Pose bei erfolgreichem Motionwechsel erhalten; fehlende Matrix wird im bestehenden Kollisionspfad geprüft. Sura-Metadaten-Adapterfehler separat korrigiert. Zweiter Versuch `manual-02`: Benutzer bestätigt erfolgreichen Welt-Eintritt, meldet aber starke Verzögerungen auch im Stand. Exit 0 und Ressourcen 0, 967 native Reads ohne Granny. Wiederholte native Clip-Dekodierung wurde anschließend am Dokument gebunden. Die korrigierte Kopie `manual-03` wurde nach Ende sämtlicher Builds/GPU-Tests mit `--animation-stall-audit` getestet. Benutzer bestätigt: „sind weg sieht sehr gut aus“; auf die gesonderte Frage zu Resize und Minimize/Restore: „passt alles“. Damit sind Welt-Eintritt, Behebung der starken Lags und manuelle Darstellung/Fensterbedienung bestätigt. Beim anschließenden freiwilligen Test weiterer Mobs meldet der Benutzer noch ganz minimale Ruckler; keine Behauptung vollständiger Hitchfreiheit. Finaler Shutdown: Exit 0 und alle erfassten Owner/Ressourcen 0. 1.250 native Reads, 0 Granny-Reads, 173 Clip-Dekodierungen, 28.464 dokumentgebundene Wiederverwendungen, 0 Budget-Bypasses, 1.381.163 eigene Poseauswertungen, 0 SDK-Samples/CPU-Deformationen/Fallbacks/Runtimefehler. Evidenz `runtime/manual-03/`. Automatisierte Bilder/Actor-Submissions ersetzen diese manuelle Bestätigung nicht. `prepare_gr2_manual_smoke.ps1` stellt eine isolierte interaktive Kopie bereit.

## 43. Resize/Minimize

GR2-Renderharness: World-Resize 256→320, Suspension 0 und Restore auf 256 bestanden. Native Betriebssystemfenster: Benutzer bestätigt Resize und Minimize/Restore im finalen Lauf `manual-03` mit „passt alles“. Schließen abschließend durch Exitcode 0 und Ressourcen-Audit bestätigt.

## 44. Shutdown

Harness: Reader-Dokumente, Model-/Animation-/Binding-Owner, Runtime-Skeletons/-Clips und GPU-Ressourcen nach Teardown null. Native-05 und Granny-02: Exitcode 0, sämtliche erfassten Reader-/Asset-/Animation-/Skinning-/GPU-/Source-Owner 0. Manual-02 und der finale Manual-03 ebenfalls Exitcode 0 und Owner 0. Manual-01 war ein echter Absturz und zählt ausdrücklich nicht als sauberer Shutdown. Der Client protokolliert zusätzlich `GR2ReaderResources`, `NativeGR2Reads`, `GrannyFileReads`.

## 45. Resource Lifetime

`File` besitzt nur während des Loads expandierte Sections und Offset-Referenzen. Dokumente besitzen Kopien von Geometrie, Metadaten, Skeleton und Source-Curves. Instanzen halten neutrale Handles und fertig importierte Runtime-Clips; begrenzte gemeinsame Bindings gehören dem Animation-Dokument und besitzen keine Rückreferenz darauf. Kein Pointer auf temporären Input und keine Granny-Lifetime-Abhängigkeit. `ReleaseUploadData` gibt nur Vertex-/Index-Kopien frei; Animation/Skeleton bleiben gültig. Kein globaler Reader-Cache.

## 46. Remaining Granny Dependencies

Produktionsdefault, bisheriger Provider, Referenz-/Parity-Harness, SDK-Library und Legacy-EterGrnLib-Namen bleiben erhalten. Der neue Reader/Core nutzt sie nicht. F2 ist keine Linkerbereinigung. F1-Importcache und AnimationRuntime-Sampler wurden nicht verändert.

## 47. Git Diff

Neue Reader/Core-Dateien inklusive Lizenz, Scanner, Safety-/Parity-/Render-/Architekturtests, Smoke-Helfer und dieser Bericht. Kleine bestehende Änderungen: Providerwahl, neutraler Index-Upload-Hinweis/Consumer, CLI, Startup-/Shutdown-Diagnose, ein SDK-Lesezähler und CMake-/Startup-Tests. Der manuell gefundene Spawn-Fehler erforderte außerdem zwei Missing-Matrix-Prüfungen in `ActorInstanceCollisionDetection.cpp`; die bestehende Absturzdiagnose protokolliert jetzt Modulbasis/Fault-Adresse für eindeutige Zuordnung. Kein GPU-Skinning-Algorithmus geändert. Evidenz bleibt unter ignoriertem `build/f2x`; keine Logs, Caches, extrahierten proprietären Assets oder Binaries in den neuen Source-Dateien. Aktuell 11 geänderte bestehende Dateien (+82/-8) und 24 neue Source-/Test-/Dokumentdateien. `git diff --check` bestanden. Der vorbestehende Client-Config-Diff und acht untracked Client-Logs bleiben unangetastet. Kein Commit/Push.

## 48. GO/NO-GO für F3-X

**GO für den Abschluss von F2-X.** Eigener Granny-freier Reader, reale Actor-/World-Pfade, Daten-/Pose-/Vertex-Parität, Release/Debug/GCC, manuelle Darstellung/Fensterbedienung und sauberer Shutdown sind nachgewiesen. Die starken wiederkehrenden Lags sind behoben. Einschränkungen bleiben: 52 explizit abgelehnte Corpus-Dateien, nicht vollständig nachgewiesene komplexe Mehrfach-Blends und messbare erste Lade-/Decode-Spitzen bei neu auftretenden Clips. Der kurze First-Use-Vergleich verbessert die geprüften Fälle deutlich, beweist aber keine Hitchfreiheit in jeder Spielsituation. F3-X benötigt einen eigenen Auftrag; Granny bleibt Default. Hier nach F2-X gestoppt, kein Commit/Push.

