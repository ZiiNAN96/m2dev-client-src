#pragma once
#include "LightingProofState.h"
#include "Renderer/SceneLightingRuntime.h"
#include "Renderer/SkinningBenchmark.h"
static PyObject* systemSetLightingProof(PyObject*,PyObject* args)
{
    int stage{},frozen{},debug{};if(!PyArg_ParseTuple(args,"ii|i",&stage,&frozen,&debug))return nullptr;
    Renderer::materialDebugView=debug==2?Renderer::MaterialDebugView::Normal:Renderer::MaterialDebugView::Lit;
    Renderer::lightingProofFrozen=frozen!=0;
    Renderer::skinningBenchmarkEnabled=stage>=0;
    Renderer::skinningBenchmarkCurrent.stage=stage;
    return Py_BuildNone();
}
static PyObject* systemGetLightingProof(PyObject*,PyObject*)
{
    using namespace Renderer;const auto& light=sceneLighting.Get();
    return Py_BuildValue("{s:K,s:K,s:K,s:K,s:K,s:K,s:(fff),s:(fff),s:(fff),s:(fff),s:f,s:d}",
        "revision",static_cast<unsigned long long>(sceneLighting.Revision()),
        "buffers",static_cast<unsigned long long>(liveLightBuffers.load()),
        "updates",static_cast<unsigned long long>(lightBufferUpdates.load()),
        "pbrDraws",static_cast<unsigned long long>(pbrDraws.load()),
        "terrainDraws",static_cast<unsigned long long>(modernTerrainDraws.load()),
        "vegetationDraws",static_cast<unsigned long long>(modernVegetationDraws.load()),
        "direction",light.sun.direction[0],light.sun.direction[1],light.sun.direction[2],
        "color",light.sun.color[0],light.sun.color[1],light.sun.color[2],
        "sky",light.ambientSkyColor[0],light.ambientSkyColor[1],light.ambientSkyColor[2],
        "ground",light.ambientGroundColor[0],light.ambientGroundColor[1],light.ambientGroundColor[2],
        "intensity",light.sun.intensity,"worldCpuUs",skinningBenchmarkCurrent.worldUs);
}
