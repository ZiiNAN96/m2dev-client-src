#pragma once
// ZiiNAN: Offline fixture feeds the original guild-land API; absent from non-test builds.
static PyObject* backgroundTestGuildArea(PyObject*,PyObject* args)
{
    int x1=0,y1=0,x2=0,y2=0;
    if(!PyArg_ParseTuple(args,"iiii",&x1,&y1,&x2,&y2)) return nullptr;
    auto& background=CPythonBackground::Instance();
    if(!background.IsMapReady()) return PyErr_Format(PyExc_RuntimeError,"Fixture map is not ready");
    background.GetMapOutdoorRef().ClearGuildArea();
    background.RegisterGuildArea(x1,y1,x2,y2);
    background.VisibleGuildArea();
    Py_RETURN_NONE;
}
