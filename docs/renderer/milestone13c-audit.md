# M13C – Audit vor Änderungen

Basis: 04017a9 (M13B), zu Beginn sauberer Source-Checkout, 2026-09-13.
Keine Commits/Resets und keine Phase-B-Funktionen. Server und Originalpakete bleiben unverändert.

## Include-Kategorien und Änderungspunkte

| Kategorie | Direkte Includes / Dateien | Behandlung |
|---|---|---|
| B/D | EterLib/StdAfx.h, SphereLib/StdAfx.h, SpeedTreeLib/SpeedTreeWrapper.h, SpeedTreeMaterial.h, SpeedTreeForestDirectX.cpp | Projekt-Mathe und CPU-Renderdaten statt d3d9/d3dx9/d3d9types. SpeedTree-SDK unverändert. |
| B | EffectLib/EffectMesh.h, Type.h, SimpleLightData.h; EterLib/Ray.h, Util.h | Neutrale Vektor-/Matrix-/Quaternion-/Plane-/Color-Typen. |
| D/C | EterLib/DecodedImageData.h, TextureBinding.h, UIRenderBridge.h; EterGrnLib/Material.h | Backend-neutrale Formate, Materialien und Texturquellen; keine Dummy-COM-Typen. |
| A/C | EterImageLib/DDSTextureLoader9.h/.cpp | Vorhandenen CPU-DDS-View-Parser behalten/neutralisieren; nativen Textur-/Surface-Erzeugungsteil entfernen. |
| A/D | EterLib/StateManager.h, NativeStateView.h, NativeResourceAudit.h; SpeedTreeLib/VertexShaders.h | CPU-State weiterverwenden, irreführende Namen/Signaturen entfernen; alte Shader-/Declarationpfade entfernen. |
| E | extern/include: mitgelieferte SDK-Header; unbenutzte Tool-/historische Testquellen | Nicht Teil des bereinigten normalen Builds; verbleibende Fundstellen einzeln ausweisen. |

Rund 251 Source-Dateien verwenden noch Mathe-/State-/native Typnamen. Die große
Zahl entsteht überwiegend durch geteilte Vektor- und Matrixtypen, nicht durch
251 aktive Renderer. Keine weitreichende Verhaltensänderung daraus ableiten.

## Mathevertrag

Neue Projekt-Mathe auf dem header-only DirectXMath des vorhandenen Windows SDK.
Unaligned, dicht gepackte Float-Speicherlayouts bleiben für CPU-Skinning, Granny-
Übergaben und Vertexdaten identisch. Row-vector-Multiplikation, Translation in
Matrixzeile 4, RH-Projektionen, Reihenfolge von Quaternionprodukten und
Yaw/Pitch/Roll sowie lokale/globale Matrixstack-Multiplikation explizit testen.
Kein D3D9-Typedef-Fassadenheader; neutrale Projekt-Typen und Funktionsnamen.

## Ressourcen / State / toter Code

- GrpTexture/ImageTexture/FontTexture, GrpVertexBuffer/IndexBuffer, BlockTexture,
  TerrainAlphaImage: nur M13A-CPU-Quellen behalten, COM-Felder/Locks entfernen.
- StateManager ist seit M13B CPU-Datenbesitzer: erhalten, neutral benennen,
  native Shader/Stream/Indices/RenderTarget-APIs und deren wirkungslose Aufrufe entfernen.
- GrpD3DXBuffer, GrpVertexShader, GrpPixelShader, GrpShadowTexture:
  verbleibende Referenzen vor Löschung prüfen.
- GrpBase/GrpDevice: tote Geräteslots/Caps/Presentation-/Debugmesh-Felder entfernen,
  COM-Matrixstack durch CPU-Stack ersetzen.
- DDS- und STB-Decodierung, Simulation, Granny, CPU-Skinning, Spiel- und UI-Verhalten bleiben.

## Build / CLI / Tests

- extern/library/DirectX/CMakeLists.txt verlinkt d3d9.lib, d3dx9.lib,
  dinput8.lib und dxguid.lib. Nur Graphics-Altlasten entfernen; DirectInput ist unabhängig.
- Source- und Renderer-CMake enthalten M2_ENABLE_DILIGENT_D3D11/OFF-Äste;
  Diligent wird unbedingter Produktionspfad.
- CLI kanonisch --renderer=d3d11, kein Legacy-Wert; bisheriges
  --renderer=diligent-d3d11 bei Bedarf nur als dokumentierter Übergangsname desselben Backends.
- Native, bereits unbebaute Paritätsorakel begründet entfernen; CPU-/Diligent-
  Invarianten nicht löschen. Neue Math-/Compile-/Import-Architekturtests.
- Release/Debug, vollständige Testsuite (Git-sh für Zstandard), sechs Weltphasen,
  normale Login-/Ingame-/UI-/Fenster-Abnahme und Ressourcenfreigabe.
- Binary-Audit: weder d3d9.dll noch d3dx9_*.dll als notwendiger Clientimport.

M13B-Paritätsgrenze bleibt: kein aktuelles zweites natives D3D9-Backend als GPU-Orakel.
M13C-Abnahme muss neu erfolgen; alte M13B-Läufe ersetzen sie nicht.
