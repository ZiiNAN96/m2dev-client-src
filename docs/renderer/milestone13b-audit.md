# M13B – Audit vor Implementierung

2026-09-13. Basis: vorhandener, nicht committeter M13A-Stand. Keine Rücksetzung.
Die finale manuelle M13A-Diligent-/Legacy-Sessionabnahme wurde vom Benutzer bewusst
übersprungen; sie ist keine bestandene Regression. Der korrigierte isolierte Weltlauf
(PID 58544, sechs Phasen, Exit 0) ist nachgewiesen.

## Einstieg und minimale Schnittstellen

| Bereich | Konkreter Einstieg | M13B-Behandlung |
|---|---|---|
| Auswahl | Renderer/StartupOptions.h: StartupOptions, BackendKind; UserInterface/UserInterface.cpp: WinMain/Main | Nur Diligent; Legacy-Wert expliziter Fehler |
| Backend | EterLib/LegacyD3D9Backend.{h,cpp}; PythonApplication::CreateDevice; RendererBootstrap.cpp | Aus Produktionsbuild und Auswahl entfernen |
| Device | EterLib/GrpDevice.cpp: CGraphicDevice::Create -> Direct3DCreate9Ex -> CreateDeviceEx | Entfernen; Fenster-/CPU-Matrixinitialisierung erhalten |
| Frame | PythonApplication::Process -> Legacy Begin/Clear/End/Present plus TerrainPresentation; GrpScreen.cpp: CScreen::Begin/End/Clear/Show | Nur bestehender Diligent-Presentation-/IRenderBackend-Lifecycle |
| Resize/Reset | GrpDevice::ResizeBackBuffer/Reset/GetDeviceState; GrpScreen::IsLostDevice/RestoreDevice; PythonApplicationProcedure WM_SIZE | Diligent-Resize und CPU-Viewport; kein D3D9-Reset |
| State | EterLib/StateManager.{h,cpp}: CStateManager, NativeStateView.h | CPU-Drawbeschreibung ohne Device-Seed/-Owner/-Dispatch |
| Material | NativeMaterialSnapshot.cpp; Text/UI/Tree/World/Effect-Bridges | Device-Existenzbedingungen und native Reads entfernen |
| Kamera | GrpBase/GrpScreen: ms_matWorld/View/Proj, MatrixStack; StateManager Transform-Stacks | Mathematische Werte erhalten, kein Device-Transform |
| Anzeige | PythonSystem::GetDisplaySettings -> GetD3D/EnumAdapterModes | Windows-Displayenumeration statt D3D9 |
| GPU-Memory/Gamma/Screenshot | GrpBase::GetAvailableTextureMemory; PythonGraphic::SetGamma/SaveScreenShot | Diligent/neutral bzw. bisher in Diligent inaktiven Gamma-Zweig entfernen |
| Ressourcen | ResourceData::UseNeutralResources; NativeResourceAudit; Wrapper | Neutraler Pfad immer aktiv, native Erzeugungen nicht mehr aktivierbar |
| OFF | CMakeLists.txt und Renderer/CMakeLists.txt | Konfiguration klar ablehnen, kein funktionaler Renderer übrig |

Registrierung geschieht durch direkte Konstruktion, nicht über ein Pluginregister.
Nur `--renderer=legacy-d3d9` aktiviert Legacy; keine Renderer-Config und keine Aliase
im StartupOptions-Parser. Nicht-Renderer-Argumente bleiben unverändert.

## StateManager-Kategorien

- A: Device-Owner/AddRef/Release, Seed/Get*-Abfragen, native Set*/Draw*/BeginScene/
  EndScene, RenderTarget/DepthSurface, native Shader/Stream/FVF-Dispatch entfernen.
- B: CPU-Matrizen/Viewport, Licht-/Materialwerte, Save/Restore-Semantik erhalten.
- C: Diligent benötigt Blend/Alpha/Cull/Depth/Fog/Sampler/Texture-Combiner und
  TextureBinding aus diesen CPU-Werten. Explizite Defaults ersetzen Geräte-Defaults.
  Alte Enumwerte dürfen als Datentokens bis M13C bleiben; kein GPU-State-Cache.
- D: native Paritätsprüfungen, D3DX-Debugmeshes, alte Shadow-/Browser-Gamma-Helfer
  getrennt betrachten. Keine neue Schatten-/Gammafunktion implementieren.

Produktive Verbraucher: EterLib (Bild/Text/UI/Welt), EterGrnLib (Actor-Material),
GameLib (Terrain/statische Objekte/Actors/Wasser), SpeedTreeLib, EffectLib,
EterPythonLib und UserInterface (Viewport/Kamera/Minimap/Renderreihenfolge).
Unverändert bleiben Geometrieproduzenten, Animation, Granny, Skinning, UI-/Packdaten.

## Tests vor Änderung

- Renderer.StartupOptions: OFF-/Legacy-Erwartungen durch Zurückweisung ersetzen.
- Renderer.LegacyD3D9: spezifischer Test eines entfernten Produktbackends (B).
- Renderer.DiligentD3D11: Frame-/Pixel-/Resize-/Lifetime-Test behalten (C).
- Renderer.NativeStateView: bisher echtes Device + Poison; künftig CPU-Defaults,
  Save/Restore, Material-/Licht-/Texture-Lifetime ohne Device nachweisen (A/C).
- Renderer.ResourceSource: retained compatibility device entfernen; alle neutralen
  Buffer-/Textur-/Alpha-/GPU-/Lifetime-Prüfungen behalten (C).
- Renderer.TerrainGpuParity und eingebundene *GpuChecks.h: enthalten sowohl native
  D3D9-Referenzzeichnung als auch wichtige neutrale/Diligent-Regressionsprüfungen.
  Nicht pauschal löschen oder als bestanden auslassen; Testadapter gesondert migrieren.
- Actor/Tree/Effect/World/UI/Text-Policy, TerrainTextureCpu, ScreenshotJPEG und
  übrige vollständige Testsuite erhalten.

## Verifikation und Grenzen

Release und vorhandenen Debug-Build prüfen; OFF muss mit erklärender Meldung scheitern.
CLI invalid/Legacy und Diligent-Init-Fehler ohne Fallback prüfen. Native Device-/Draw-/
Allocation-Telemetrie sowie Shutdownzähler aus realen M13B-Läufen, nicht M13A übernehmen.
Isolierte Weltfolge plus normale Login/Charakterauswahl/Ingame/Questdialog-/Fenster-/
Sessionabnahme. Benutzer übernimmt Windows-Freigaben und Login.
Kein M13C: Header, Libs, Typedefs und Enumtokens nicht global bereinigen.
