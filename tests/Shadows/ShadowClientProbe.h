#pragma once
#include "Renderer/ShadowAmbientRuntime.h"
static PyObject* systemSetDepthProof(PyObject*,PyObject* args){
    int ao{},cascades{};float x{},y{},z{};
    if(!PyArg_ParseTuple(args,"ii|fff",&ao,&cascades,&x,&y,&z))return nullptr;
    Renderer::ambientDebugView=ao==1;Renderer::cascadeDebugView=cascades==1;
    if(PyTuple_Size(args)==5){auto light=Renderer::sceneLighting.Get();light.sun.direction={x,y,z};Renderer::sceneLighting.Set(light);}
    return Py_BuildNone();
}
static PyObject* systemGetDepthProof(PyObject*,PyObject*){
    using namespace Renderer;
    return Py_BuildValue("{s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:K,s:d,s:d,s:d}",
        "shadowMaps",static_cast<unsigned long long>(liveShadowMaps.load()),"shadowViews",static_cast<unsigned long long>(liveShadowViews.load()),
        "shadowPSOs",static_cast<unsigned long long>(liveShadowPipelines.load()),"aoTargets",static_cast<unsigned long long>(liveAOTargets.load()),
        "aoViews",static_cast<unsigned long long>(liveAOViews.load()),"aoPSOs",static_cast<unsigned long long>(liveAOPipelines.load()),
        "shadowDraws",static_cast<unsigned long long>(shadowDraws),"casters",static_cast<unsigned long long>(shadowCasters),
        "shadowBytes",static_cast<unsigned long long>(shadowMemory),"aoBytes",static_cast<unsigned long long>(aoMemory),
        "shadowCpuUs",shadowCpuUs,"aoCpuUs",aoCpuUs,"lastAOGpuUs",lastAOGpuUs);
}
