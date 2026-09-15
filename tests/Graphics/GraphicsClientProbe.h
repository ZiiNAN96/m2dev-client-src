#pragma once
#include "Graphics/AtmosphereConfig.h"
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
