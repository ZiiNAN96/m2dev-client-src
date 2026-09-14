// ZiiNAN: 64-bit safety cleanup
#ifdef _DEBUG
#undef _DEBUG
#define M2_RESTORE_DEBUG_AFTER_PYTHON
#endif
#include <python/Python.h>
#ifdef M2_RESTORE_DEBUG_AFTER_PYTHON
#define _DEBUG
#undef M2_RESTORE_DEBUG_AFTER_PYTHON
#endif
#include <Windows.h>

#include <array>
#include <cstdint>

static_assert(sizeof(void*) == 8);
static_assert(sizeof(std::uintptr_t) == sizeof(void*));
static_assert(sizeof(long) == 4);
static_assert(sizeof(LONG_PTR) == sizeof(void*));
static_assert(sizeof(ULONG_PTR) == sizeof(void*));
static_assert(sizeof(WPARAM) == sizeof(void*));
static_assert(sizeof(LPARAM) == sizeof(void*));
static_assert(sizeof(LRESULT) == sizeof(void*));
static_assert(sizeof(HANDLE) == sizeof(void*));
static_assert(sizeof(HWND) == sizeof(void*));
static_assert(sizeof(Py_ssize_t) == sizeof(void*));

int main()
{
	PyConfig config;
	PyConfig_InitIsolatedConfig(&config);
	config._install_importlib = 0;
	config._init_main = 0;
	const PyStatus status = Py_InitializeFromConfig(&config);
	PyConfig_Clear(&config);
	if (PyStatus_Exception(status))
		return 1;

	int local_value = 0;
	const std::array<void*, 2> pointers = {
		&local_value,
		reinterpret_cast<void*>(std::uintptr_t{0x0000000123456789ULL}),
	};

	for (void* original : pointers)
	{
		PyObject* encoded = PyLong_FromVoidPtr(original);
		if (!encoded)
			return 2;

		void* decoded = PyLong_AsVoidPtr(encoded);
		const bool conversion_failed = PyErr_Occurred() != nullptr;
		Py_DECREF(encoded);
		if (conversion_failed || decoded != original)
			return 3;
	}

	return Py_FinalizeEx() < 0 ? 4 : 0;
}
