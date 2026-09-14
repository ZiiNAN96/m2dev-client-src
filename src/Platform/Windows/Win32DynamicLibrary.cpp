#include "Platform/PlatformDynamicLibrary.h"

#include <windows.h>

#include <utf8.h>

#include <string>
#include <utility>
#include <vector>

namespace Platform
{
namespace
{
[[nodiscard]] HMODULE ToModule(std::uintptr_t value) noexcept
{
    return reinterpret_cast<HMODULE>(value);
}
}

DynamicLibrary::~DynamicLibrary()
{
    Reset();
}

DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept
    : m_nativeHandle(std::exchange(other.m_nativeHandle, 0))
{
}

DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& other) noexcept
{
    if (this != &other)
    {
        Reset();
        m_nativeHandle = std::exchange(other.m_nativeHandle, 0);
    }
    return *this;
}

bool DynamicLibrary::Load(std::string_view path) noexcept
{
    Reset();
    if (path.empty())
        return false;

    try
    {
        const std::wstring widePath = Utf8ToWide(std::string(path));
        if (widePath.empty())
            return false;

        const HMODULE module = LoadLibraryW(widePath.c_str());
        if (!module)
            return false;
        m_nativeHandle = reinterpret_cast<std::uintptr_t>(module);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool DynamicLibrary::LoadSystem(std::string_view libraryName) noexcept
{
    Reset();
    if (libraryName.empty())
        return false;

    try
    {
        const UINT required = GetSystemDirectoryW(nullptr, 0);
        if (required == 0)
            return false;

        std::vector<wchar_t> buffer(static_cast<std::size_t>(required) + 1);
        const UINT length = GetSystemDirectoryW(buffer.data(), static_cast<UINT>(buffer.size()));
        if (length == 0 || length >= buffer.size())
            return false;

        const std::wstring wideName = Utf8ToWide(std::string(libraryName));
        if (wideName.empty())
            return false;

        std::wstring path(buffer.data(), length);
        if (!path.empty() && path.back() != L'\\' && path.back() != L'/')
            path.push_back(L'\\');
        path += wideName;

        const HMODULE module = LoadLibraryW(path.c_str());
        if (!module)
            return false;
        m_nativeHandle = reinterpret_cast<std::uintptr_t>(module);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void* DynamicLibrary::GetSymbol(const char* symbolName) const noexcept
{
    if (!IsLoaded() || !symbolName || !*symbolName)
        return nullptr;
    return reinterpret_cast<void*>(GetProcAddress(ToModule(m_nativeHandle), symbolName));
}

void DynamicLibrary::Reset() noexcept
{
    if (IsLoaded())
        FreeLibrary(ToModule(m_nativeHandle));
    m_nativeHandle = 0;
}

bool DynamicLibrary::IsLoaded() const noexcept
{
    return m_nativeHandle != 0;
}
}
