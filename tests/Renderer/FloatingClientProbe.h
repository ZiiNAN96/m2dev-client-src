#pragma once
// ZiiNAN: Diligent floating text rendering; local fixture hooks, only in renderer-test builds.
static PyObject* chrtestRefreshTextTail(PyObject*,PyObject* args)
{
    unsigned int vid=0,guild=0,level=0;
    if(!PyArg_ParseTuple(args,"I|II",&vid,&guild,&level)) return nullptr;
    auto* actor=CPythonCharacterManager::Instance().GetInstancePtr(vid);
    if(!actor) return PyErr_Format(PyExc_ValueError,"Unknown fixture actor %u",vid);
    actor->ChangeGuild(guild); // Original detach/register computes current model and mount height.
    if(level) actor->UpdateTextTailLevel(level);
    return Py_BuildValue("f",actor->GetGraphicThingInstanceRef().GetHeight()+(actor->IsMountingHorse() ? 110.0f : 10.0f));
}
static PyObject* chrtestAddDamageEffect(PyObject*,PyObject* args)
{
    unsigned int vid=0,damage=0; unsigned char flag=0; int self=0,target=0;
    if(!PyArg_ParseTuple(args,"IIBpp",&vid,&damage,&flag,&self,&target)) return nullptr;
    auto* actor=CPythonCharacterManager::Instance().GetInstancePtr(vid);
    if(!actor) return PyErr_Format(PyExc_ValueError,"Unknown fixture actor %u",vid);
    actor->AddDamageEffect(damage,flag,self,target);
    Py_RETURN_NONE;
}
