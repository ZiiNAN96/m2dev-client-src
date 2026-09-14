#pragma once

#include <cstdint>
#include <string_view>

namespace Platform
{
class DynamicLibrary final
{
public:
    DynamicLibrary() noexcept = default;
    ~DynamicLibrary();

    DynamicLibrary(const DynamicLibrary&) = delete;
    DynamicLibrary& operator=(const DynamicLibrary&) = delete;
    DynamicLibrary(DynamicLibrary&& other) noexcept;
    DynamicLibrary& operator=(DynamicLibrary&& other) noexcept;

    // ZiiNAN: Platform abstraction
    [[nodiscard]] bool Load(std::string_view path) noexcept;
    [[nodiscard]] bool LoadSystem(std::string_view libraryName) noexcept;
    [[nodiscard]] void* GetSymbol(const char* symbolName) const noexcept;
    void Reset() noexcept;
    [[nodiscard]] bool IsLoaded() const noexcept;

private:
    std::uintptr_t m_nativeHandle = 0;
};
}
