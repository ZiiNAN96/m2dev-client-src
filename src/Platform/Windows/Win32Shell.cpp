#include "Platform/PlatformShell.h"

#include <windows.h>
#include <shellapi.h>

#include <utf8.h>

#include <string>

namespace Platform::Shell
{
namespace
{
[[nodiscard]] HWND ToWindow(NativeWindowHandle owner) noexcept
{
    return static_cast<HWND>(owner.value);
}
}

bool Open(std::string_view target, NativeWindowHandle owner) noexcept
{
    if (target.empty())
        return false;
    try
    {
        const std::wstring wideTarget = Utf8ToWide(std::string(target));
        if (wideTarget.empty())
            return false;
        const HINSTANCE result = ShellExecuteW(ToWindow(owner), L"open", wideTarget.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return reinterpret_cast<std::intptr_t>(result) > 32;
    }
    catch (...)
    {
        return false;
    }
}

bool OpenUrl(std::string_view url, NativeWindowHandle owner) noexcept
{
    return Open(url, owner);
}
}
