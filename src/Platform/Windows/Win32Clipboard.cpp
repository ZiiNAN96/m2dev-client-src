#include "Platform/PlatformClipboard.h"

#include <windows.h>

#include <utf8.h>

#include <cstring>

namespace Platform::Clipboard
{
namespace
{
[[nodiscard]] HWND ToWindow(NativeWindowHandle owner) noexcept
{
    return static_cast<HWND>(owner.value);
}
}

bool SetText(std::string_view text, NativeWindowHandle owner) noexcept
{
    try
    {
        const std::wstring wideText = Utf8ToWide(std::string(text));
        if (!OpenClipboard(ToWindow(owner)))
            return false;

        if (!EmptyClipboard())
        {
            CloseClipboard();
            return false;
        }

        const SIZE_T bytes = (wideText.size() + 1) * sizeof(wchar_t);
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (!memory)
        {
            CloseClipboard();
            return false;
        }

        auto* destination = static_cast<wchar_t*>(GlobalLock(memory));
        if (!destination)
        {
            GlobalFree(memory);
            CloseClipboard();
            return false;
        }

        std::memcpy(destination, wideText.c_str(), bytes);
        GlobalUnlock(memory);
        if (!SetClipboardData(CF_UNICODETEXT, memory))
        {
            GlobalFree(memory);
            CloseClipboard();
            return false;
        }

        CloseClipboard();
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool GetText(std::string& text, NativeWindowHandle owner) noexcept
{
    text.clear();
    if (!OpenClipboard(ToWindow(owner)))
        return false;

    const HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (!data)
    {
        CloseClipboard();
        return false;
    }

    const auto* source = static_cast<const wchar_t*>(GlobalLock(data));
    if (!source)
    {
        CloseClipboard();
        return false;
    }

    try
    {
        text = WideToUtf8(source);
    }
    catch (...)
    {
        GlobalUnlock(data);
        CloseClipboard();
        return false;
    }

    GlobalUnlock(data);
    CloseClipboard();
    return true;
}
}
