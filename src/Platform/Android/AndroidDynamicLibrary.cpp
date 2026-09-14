#include "Platform/PlatformDynamicLibrary.h"
#include <dlfcn.h>
#include <string>
#include <utility>

namespace Platform
{
DynamicLibrary::~DynamicLibrary() { Reset(); }
DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept : m_nativeHandle{std::exchange(other.m_nativeHandle, 0)} {}
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
    if (path.empty() || path.find('\0') != std::string_view::npos)
        return false;
    try
    {
        const std::string terminated{path};
        m_nativeHandle = reinterpret_cast<std::uintptr_t>(dlopen(terminated.c_str(), RTLD_NOW | RTLD_LOCAL));
        return IsLoaded();
    }
    catch (...) { return false; }
}

bool DynamicLibrary::LoadSystem(std::string_view libraryName) noexcept
{
    Reset();
    if (libraryName.find('/') != std::string_view::npos || libraryName.find('\\') != std::string_view::npos)
        return false;
    return Load(libraryName);
}

void* DynamicLibrary::GetSymbol(const char* symbolName) const noexcept
{
    return IsLoaded() && symbolName ? dlsym(reinterpret_cast<void*>(m_nativeHandle), symbolName) : nullptr;
}

void DynamicLibrary::Reset() noexcept
{
    if (IsLoaded())
        dlclose(reinterpret_cast<void*>(m_nativeHandle));
    m_nativeHandle = 0;
}

bool DynamicLibrary::IsLoaded() const noexcept { return m_nativeHandle != 0; }
}
