#include "StdAfx.h"
#include "PythonUtils.h"

#include <limits>

// ZiiNAN: 64-bit safety cleanup

IPythonExceptionSender * g_pkExceptionSender = NULL;

bool __PyCallClassMemberFunc_ByCString(PyObject* poClass, const char* c_szFunc, PyObject* poArgs, PyObject** poRet);
bool __PyCallClassMemberFunc_ByPyString(PyObject* poClass, PyObject* poFuncName, PyObject* poArgs, PyObject** poRet);
bool __PyCallClassMemberFunc(PyObject* poClass, PyObject* poFunc, PyObject* poArgs, PyObject** poRet);

PyObject * Py_BadArgument()
{
	PyErr_BadArgument();
	return NULL;
}

PyObject * Py_BuildException(const char * c_pszErr, ...)
{
	if (!c_pszErr)
	{
		PyErr_Clear();
		return Py_BuildNone();
	}

	char szErrBuf[512+1];
	va_list args;
	va_start(args, c_pszErr);
	vsnprintf(szErrBuf, sizeof(szErrBuf), c_pszErr, args);
	va_end(args);

	PyErr_SetString(PyExc_RuntimeError, szErrBuf);
	return NULL;
}

PyObject * Py_BuildNone()
{
	Py_INCREF(Py_None);
	return Py_None;
}

void Py_ReleaseNone()
{
	Py_DECREF(Py_None);
}

bool PyTuple_GetObject(PyObject* poArgs, int pos, PyObject** ret)
{
	if (pos >= PyTuple_Size(poArgs))
		return false;

	PyObject * poItem = PyTuple_GetItem(poArgs, pos);

	if (!poItem)
		return false;
	
	*ret = poItem;
	return true;
}

bool PyTuple_GetLong(PyObject* poArgs, int pos, long* ret)
{
	if (pos >= PyTuple_Size(poArgs))
		return false;

	PyObject* poItem = PyTuple_GetItem(poArgs, pos);

	if (!poItem)
		return false;

	const long value = PyLong_AsLong(poItem);
	if (PyErr_Occurred())
		return false;

	*ret = value;
	return true;
}

bool PyTuple_GetLongLong(PyObject* poArgs, int pos, long long* ret)
{
	if (pos >= PyTuple_Size(poArgs))
		return false;

	PyObject* poItem = PyTuple_GetItem(poArgs, pos);

	if (!poItem)
		return false;

	const long long value = PyLong_AsLongLong(poItem);
	if (PyErr_Occurred())
		return false;

	*ret = value;
	return true;
}

bool PyTuple_GetDouble(PyObject* poArgs, int pos, double* ret)
{
	if (pos >= PyTuple_Size(poArgs))
		return false;

	PyObject* poItem = PyTuple_GetItem(poArgs, pos);

	if (!poItem)
		return false;

	*ret = PyFloat_AsDouble(poItem);
	return true;
}

bool PyTuple_GetFloat(PyObject* poArgs, int pos, float* ret)
{
	if (pos >= PyTuple_Size(poArgs))
		return false;

	PyObject * poItem = PyTuple_GetItem(poArgs, pos);

	if (!poItem)
		return false;

	*ret = float(PyFloat_AsDouble(poItem));
	return true;
}

bool PyTuple_GetByte(PyObject* poArgs, int pos, unsigned char* ret)
{
	int val;
	if (!PyTuple_GetInteger(poArgs, pos, &val))
		return false;
	if (val < 0 || val > std::numeric_limits<unsigned char>::max())
	{
		PyErr_SetString(PyExc_OverflowError, "value does not fit in an unsigned byte");
		return false;
	}

	*ret = static_cast<unsigned char>(val);
	return true;
}

bool PyTuple_GetInteger(PyObject* poArgs, int pos, unsigned char* ret)
{
	int val;
	if (!PyTuple_GetInteger(poArgs, pos, &val))
		return false;
	if (val < 0 || val > std::numeric_limits<unsigned char>::max())
	{
		PyErr_SetString(PyExc_OverflowError, "value does not fit in an unsigned byte");
		return false;
	}

	*ret = static_cast<unsigned char>(val);
	return true;
}

bool PyTuple_GetInteger(PyObject* poArgs, int pos, WORD* ret)
{
	int val;
	if (!PyTuple_GetInteger(poArgs, pos, &val))
		return false;
	if (val < 0 || static_cast<unsigned int>(val) > std::numeric_limits<WORD>::max())
	{
		PyErr_SetString(PyExc_OverflowError, "value does not fit in a WORD");
		return false;
	}

	*ret = static_cast<WORD>(val);
	return true;
}

bool PyTuple_GetInteger(PyObject* poArgs, int pos, int* ret)
{
	if (pos >= PyTuple_Size(poArgs))
		return false;

	PyObject* poItem = PyTuple_GetItem(poArgs, pos);
	
	if (!poItem)
		return false;
	
	const long value = PyLong_AsLong(poItem);
	if (PyErr_Occurred())
		return false;
	if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
	{
		PyErr_SetString(PyExc_OverflowError, "value does not fit in an int");
		return false;
	}

	*ret = static_cast<int>(value);
	return true;
}

bool PyTuple_GetUnsignedLong(PyObject* poArgs, int pos, unsigned long* ret)
{
	if (pos >= PyTuple_Size(poArgs))
		return false;

	PyObject * poItem = PyTuple_GetItem(poArgs, pos);
	
	if (!poItem)
		return false;
	
	const unsigned long value = PyLong_AsUnsignedLong(poItem);
	if (PyErr_Occurred())
		return false;

	*ret = value;
	return true;
}

bool PyTuple_GetUnsignedLongLong(PyObject* poArgs, int pos, unsigned long long* ret)
{
	if (pos >= PyTuple_Size(poArgs))
		return false;

	PyObject* poItem = PyTuple_GetItem(poArgs, pos);

	if (!poItem)
		return false;

	const unsigned long long value = PyLong_AsUnsignedLongLong(poItem);
	if (PyErr_Occurred())
		return false;

	*ret = value;
	return true;
}

bool PyTuple_GetUnsignedInteger(PyObject* poArgs, int pos, unsigned int* ret)
{
	if (pos >= PyTuple_Size(poArgs))
		return false;

	PyObject* poItem = PyTuple_GetItem(poArgs, pos);
	
	if (!poItem)
		return false;
	
	const unsigned long value = PyLong_AsUnsignedLong(poItem);
	if (PyErr_Occurred())
		return false;
	if (value > std::numeric_limits<unsigned int>::max())
	{
		PyErr_SetString(PyExc_OverflowError, "value does not fit in an unsigned int");
		return false;
	}

	*ret = static_cast<unsigned int>(value);
	return true;
}

bool PyTuple_GetString(PyObject* poArgs, int pos, char** ret)
{
	if (pos >= PyTuple_Size(poArgs))
		return false;

	PyObject* poItem = PyTuple_GetItem(poArgs, pos);

	if (!poItem)
		return false;

	if (!PyString_Check(poItem)) 
		return false;

	*ret = PyString_AsString(poItem);
	return true;
}

bool PyTuple_GetBoolean(PyObject* poArgs, int pos, bool* ret)
{
	if (pos >= PyTuple_Size(poArgs))
		return false;
	
	PyObject* poItem = PyTuple_GetItem(poArgs, pos);

	if (!poItem)
		return false;

	const int value = PyObject_IsTrue(poItem);
	if (value < 0)
		return false;

	*ret = value != 0;
	return true;
}

bool PyCallClassMemberFunc(PyObject* poClass, PyObject* poFunc, PyObject* poArgs)
{
	PyObject* poRet;

	// NOTE : NULL 체크 추가.. - [levites]
	if (!poClass)
	{
		Py_XDECREF(poArgs);
		return false;
	}

	if (!__PyCallClassMemberFunc(poClass, poFunc, poArgs, &poRet))
		return false;

	Py_DECREF(poRet);
	return true;
}

bool PyCallClassMemberFunc(PyObject* poClass, const char* c_szFunc, PyObject* poArgs)
{
	PyObject* poRet;

	// NOTE : NULL 체크 추가.. - [levites]
	if (!poClass)
	{
		Py_XDECREF(poArgs);
		return false;
	}

	if (!__PyCallClassMemberFunc_ByCString(poClass, c_szFunc, poArgs, &poRet))
		return false;

	Py_DECREF(poRet);
	return true;
}

bool PyCallClassMemberFunc_ByPyString(PyObject* poClass, PyObject* poFuncName, PyObject* poArgs)
{
	PyObject* poRet;

	// NOTE : NULL 체크 추가.. - [levites]
	if (!poClass)
	{
		Py_XDECREF(poArgs);
		return false;
	}

	if (!__PyCallClassMemberFunc_ByPyString(poClass, poFuncName, poArgs, &poRet))
		return false;
	
	Py_DECREF(poRet);
	return true;
}

bool PyCallClassMemberFunc(PyObject* poClass, const char* c_szFunc, PyObject* poArgs, bool* pisRet)
{
	PyObject* poRet;

	if (!__PyCallClassMemberFunc_ByCString(poClass, c_szFunc, poArgs, &poRet))
		return false;

	if (PyNumber_Check(poRet))
	{
		const int value = PyObject_IsTrue(poRet);
		if (value < 0)
		{
			Py_DECREF(poRet);
			return false;
		}
		*pisRet = value != 0;
	}
	else
		*pisRet = true;

	Py_DECREF(poRet);
	return true;
}

bool PyCallClassMemberFunc(PyObject* poClass, const char* c_szFunc, PyObject* poArgs, long * plRetValue)
{
	PyObject* poRet;

	if (!__PyCallClassMemberFunc_ByCString(poClass, c_szFunc, poArgs, &poRet))
		return false;

	if (PyNumber_Check(poRet))
	{
		const long value = PyLong_AsLong(poRet);
		if (PyErr_Occurred())
		{
			Py_DECREF(poRet);
			return false;
		}
		*plRetValue = value;
		Py_DECREF(poRet);
		return true;
	}

	Py_DECREF(poRet);
	return false;
}

/*
 *	이 함수를 직접 호출하지 않도록 한다.
 *	부득이 하게 직접 호출할 경우에는 반드시 false 가 리턴 됐을 때
 *	Py_DECREF(poArgs); 를 해준다.
 */
bool __PyCallClassMemberFunc_ByCString(PyObject* poClass, const char* c_szFunc, PyObject* poArgs, PyObject** ppoRet)
{
	if (!poClass) 
	{
		Py_XDECREF(poArgs);
		return false;
	}

	PyObject * poFunc = PyObject_GetAttrString(poClass, (char *)c_szFunc);	// New Reference

	if (!poFunc)
	{		
		PyErr_Clear();
		Py_XDECREF(poArgs);
		return false;
	}

	if (!PyCallable_Check(poFunc)) 
	{
		Py_DECREF(poFunc);
		Py_XDECREF(poArgs);
		return false;
	}

	PyObject * poRet = PyObject_CallObject(poFunc, poArgs);	// New Reference

	if (!poRet)
	{
		if (g_pkExceptionSender)
			g_pkExceptionSender->Clear();

		PyErr_Print();

		if (g_pkExceptionSender)
			g_pkExceptionSender->Send();

		Py_DECREF(poFunc);
		Py_XDECREF(poArgs);
		return false;
	}

	*ppoRet = poRet;

	Py_DECREF(poFunc);
	Py_XDECREF(poArgs);
	return true;
}

bool __PyCallClassMemberFunc_ByPyString(PyObject* poClass, PyObject* poFuncName, PyObject* poArgs, PyObject** ppoRet)
{
	if (!poClass) 
	{
		Py_XDECREF(poArgs);
		return false;
	}

	PyObject * poFunc = PyObject_GetAttr(poClass, poFuncName);	// New Reference

	if (!poFunc)
	{		
		PyErr_Clear();
		Py_XDECREF(poArgs);
		return false;
	}

	if (!PyCallable_Check(poFunc)) 
	{
		Py_DECREF(poFunc);
		Py_XDECREF(poArgs);
		return false;
	}

	PyObject * poRet = PyObject_CallObject(poFunc, poArgs);	// New Reference

	if (!poRet)
	{
		if (g_pkExceptionSender)
			g_pkExceptionSender->Clear();

		PyErr_Print();

		if (g_pkExceptionSender)
			g_pkExceptionSender->Send();

		Py_DECREF(poFunc);
		Py_XDECREF(poArgs);
		return false;
	}

	*ppoRet = poRet;

	Py_DECREF(poFunc);
	Py_XDECREF(poArgs);
	return true;
}

bool __PyCallClassMemberFunc(PyObject* poClass, PyObject * poFunc, PyObject* poArgs, PyObject** ppoRet)
{
	if (!poClass) 
	{
		Py_XDECREF(poArgs);
		return false;
	}

	if (!poFunc)
	{		
		PyErr_Clear();
		Py_XDECREF(poArgs);
		return false;
	}

	if (!PyCallable_Check(poFunc)) 
	{
		Py_DECREF(poFunc);
		Py_XDECREF(poArgs);
		return false;
	}

	PyObject * poRet = PyObject_CallObject(poFunc, poArgs);	// New Reference

	if (!poRet)
	{
		PyErr_Print();
		Py_DECREF(poFunc);
		Py_XDECREF(poArgs);
		return false;
	}

	*ppoRet = poRet;

	Py_DECREF(poFunc);
	Py_XDECREF(poArgs);
	return true;
}
