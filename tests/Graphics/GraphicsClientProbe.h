#pragma once
#include "Graphics/AtmosphereConfig.h"
#include "Renderer/WaterDiagnostics.h"
#include "Vegetation/VegetationRenderer.h"
#include "Renderer/WorldResidencyDiagnostics.h"
#include "Renderer/EffectRenderData.h"
#include "Renderer/Diagnostics.h"
static PyObject* systemTestDisplayGeometry(PyObject*, PyObject*)
{
    struct ViewportProbe : CGraphicBase { static Math::Viewport Read() { return ms_Viewport; } };
    const auto cpu = ViewportProbe::Read();
    const auto& render = Renderer::displayLayoutAudit;
    const auto client = CPythonApplication::Instance().GetPlatformWindow().GetClientRect();
    const auto outer = CPythonApplication::Instance().GetPlatformWindow().GetWindowRect();
    auto& ui = UI::CWindowManager::Instance();
    return Py_BuildValue("{s:i,s:i,s:I,s:I,s:l,s:l,s:I,s:I,s:I,s:I,s:i,s:i,s:K}",
        "ClientWidth", int(client.right-client.left), "ClientHeight", int(client.bottom-client.top),
        "BackbufferWidth", render.backbufferWidth, "BackbufferHeight", render.backbufferHeight,
        "UIScreenWidth", ui.GetScreenWidth(), "UIScreenHeight", ui.GetScreenHeight(),
        "CPUViewportWidth", cpu.Width, "CPUViewportHeight", cpu.Height,
        "RenderViewportWidth", render.uiViewportWidth, "RenderViewportHeight", render.uiViewportHeight,
        "OuterWidth", int(outer.right-outer.left), "OuterHeight", int(outer.bottom-outer.top),
        "UIDraws", render.uiDraws);
}
// Native input replay for the private HUD regression probe. This exercises
// production picking/capture/wheel routing inside the owned client process.
static PyObject* systemTestUIInput(PyObject*, PyObject* args)
{
    const char* action;
    int x, y, delta = 0;
    if (!PyArg_ParseTuple(args, "sii|i", &action, &x, &y, &delta)) return nullptr;
    auto& ui = UI::CWindowManager::Instance();
    ui.RunMouseMove(x, y);
    if (!strcmp(action, "wheel")) return PyBool_FromLong(ui.RunMouseWheel(delta));
    else if (!strcmp(action, "down")) ui.RunMouseLeftButtonDown(x, y);
    else if (!strcmp(action, "up")) ui.RunMouseLeftButtonUp(x, y);
    else if (strcmp(action, "move")) return PyErr_Format(PyExc_ValueError, "Unknown UI input action");
    return Py_BuildNone();
}
static PyObject* systemTestActorTiming(PyObject*, PyObject* args)
{
    int vid;
    if (!PyArg_ParseTuple(args, "i", &vid)) return nullptr;
    auto* actor = CPythonCharacterManager::Instance().GetInstancePtr(vid);
    if (!actor) return PyErr_Format(PyExc_ValueError, "Timing actor does not exist");
    TPixelPosition position;
    actor->NEW_GetPixelPosition(&position);
    return Py_BuildValue("{s:f,s:f,s:f,s:f,s:i,s:i,s:I,s:I}",
        "x", position.x, "y", position.y, "z", position.z, "localTime", actor->GetGraphicThingInstanceRef().GetLocalTime(),
        "attacking", int(actor->IsAttacking()), "walking", int(actor->IsWalking()),
        "effects", Renderer::effectRuntime.instances, "particles", Renderer::effectRuntime.particles);
}
static PyObject* systemTestWorldResidency(PyObject*,PyObject*)
{
    const auto& w=Renderer::worldResidency;
    return Py_BuildValue("{s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K}",
        "terrainLoaded",w.terrainLoaded,"terrainUnloaded",w.terrainUnloaded,
        "areasLoaded",w.areasLoaded,"areasUnloaded",w.areasUnloaded,
        "terrainResident",w.terrainResident,"areasResident",w.areasResident,
        "terrainAssignments",w.terrainAssignments,"treeBlockerDraws",w.treeBlockerDraws,
        "instanceBufferCreates",w.instanceBufferCreates,"treeLodSwitches",Vegetation::statistics.lodChanges,
        "treeCreated",Vegetation::statistics.created,"terrainVisible",w.terrainVisible,"terrainDrawn",w.terrainDrawn);
}
static PyObject* systemTestVegetationTime(PyObject*,PyObject* args)
{
    float seconds;if(!PyArg_ParseTuple(args,"f",&seconds))return nullptr;
    Vegetation::developmentSeconds=std::isfinite(seconds)?seconds:-1;return Py_BuildNone();
}
static PyObject* systemTestVegetationStats(PyObject*,PyObject*)
{
    const auto&s=Vegetation::statistics;
    const auto&g=Vegetation::grassPreparation;
    return Py_BuildValue("{s:K,s:K,s:K,s:K,s:K,s:K,s:d,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:d}","visible",s.visible,"culled",s.culled,"draws",s.submitted,"triangles",s.triangles,
        "uploads",std::uint64_t(Renderer::vegetationInstanceUploads.load()),"instanceBytes",std::uint64_t(Renderer::vegetationInstanceBytes.load()),"cpuMs",s.cpuMilliseconds,"instances",std::uint64_t(Vegetation::liveInstances.load()),
        "grassPlacements",g.placements,"grassCells",g.cells,"grassTiles",g.tiles,"grassExpectedTiles",g.expectedTiles,"grassBytes",g.bytes,"grassSerial",g.serial,"grassLoadMs",g.milliseconds);
}
static PyObject* systemTestSkyTime(PyObject*,PyObject* args)
{
    double seconds;if(!PyArg_ParseTuple(args,"d",&seconds))return nullptr;
    Graphics::developmentSkySeconds=std::isfinite(seconds)?seconds:-1;
    return Py_BuildNone();
}
static PyObject* systemTestWaterTime(PyObject*,PyObject* args)
{
    double seconds;int view=0;if(!PyArg_ParseTuple(args,"d|i",&seconds,&view))return nullptr;
    if(view<0||view>4){PyErr_SetString(PyExc_ValueError,"Water diagnostic view must be 0..4");return nullptr;}
    Renderer::waterTestSeconds=std::isfinite(seconds)?seconds:-1;
    Renderer::waterTestView=unsigned(view);
    return Py_BuildNone();
}
static PyObject* systemTestGraphicsSun(PyObject*,PyObject* args)
{
    int state;
    if(!PyArg_ParseTuple(args,"i",&state))return nullptr;
    if(state < -1 || state > 2){PyErr_SetString(PyExc_ValueError,"Sun state must be -1 (map), 0, 1 or 2");return nullptr;}
    Graphics::developmentSunState=state;
    return Py_BuildNone();
}
// Only present when M2_BUILD_RENDERER_TESTS is enabled. The owned client performs
// its own native window transitions; no cross-process control/elevation needed.
static PyObject* systemTestGraphicsWindow(PyObject*, PyObject*)
{
    auto& window = CPythonApplication::Instance().GetPlatformWindow();
    const auto old = window.GetWindowRect();
    window.SetSize(900,650);
    const auto resized = window.GetWindowRect();
    const bool resize = resized.right - resized.left == 900 && resized.bottom - resized.top == 650;
    window.Minimize();
    const bool minimized = window.IsWindowMinimized();
    window.Restore();
    const bool restored = !window.IsWindowMinimized();
    window.SetSize(old.right - old.left, old.bottom - old.top);
    const auto final = window.GetWindowRect();
    const bool original = final.right - final.left == old.right - old.left && final.bottom - final.top == old.bottom - old.top;
    return Py_BuildValue("{s:i,s:i,s:i,s:i}", "resize", int(resize), "minimize", int(minimized), "restore", int(restored), "originalSize", int(original));
}
