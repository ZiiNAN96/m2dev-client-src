#pragma once
#include "Graphics/AtmosphereConfig.h"
#include "Renderer/WaterDiagnostics.h"
#include "Vegetation/VegetationRenderer.h"
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
