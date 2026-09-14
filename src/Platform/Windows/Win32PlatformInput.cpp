#include "Platform/PlatformInput.h"

#include <windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#include <array>
#include <cstring>
#include <utility>

namespace Platform
{
struct PlatformInput::Impl
{
    IDirectInput8W* directInput = nullptr;
    IDirectInputDevice8W* keyboard = nullptr;
    std::array<unsigned char, 256> state{};
    std::array<bool, 256> pressed{};
    KeyEventHandler keyDown;
    KeyEventHandler keyUp;
    DWORD stickyKeysFlags = 0;
    bool accessibilityShortcutsChanged = false;

    void ReleaseDevices()
    {
        if (keyboard)
        {
            keyboard->Unacquire();
            keyboard->Release();
            keyboard = nullptr;
        }
        if (directInput)
        {
            directInput->Release();
            directInput = nullptr;
        }
    }

    ~Impl()
    {
        RestoreAccessibilityShortcuts();
        ReleaseDevices();
    }

    void RestoreAccessibilityShortcuts()
    {
        if (!accessibilityShortcutsChanged)
            return;
        STICKYKEYS keys{};
        keys.cbSize = sizeof(keys);
        keys.dwFlags = stickyKeysFlags;
        SystemParametersInfoW(SPI_SETSTICKYKEYS, sizeof(keys), &keys, 0);
        accessibilityShortcutsChanged = false;
    }
};

PlatformInput::PlatformInput() : m_impl(std::make_unique<Impl>()) {}
PlatformInput::~PlatformInput() = default;
PlatformInput::PlatformInput(PlatformInput&&) noexcept = default;
PlatformInput& PlatformInput::operator=(PlatformInput&&) noexcept = default;

bool PlatformInput::Initialize(NativeWindowHandle window, KeyEventHandler keyDown, KeyEventHandler keyUp)
{
    if (!window)
        return false;
    if (m_impl->keyboard)
        return true;

    HRESULT result = DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION,
                                        IID_IDirectInput8W, reinterpret_cast<void**>(&m_impl->directInput), nullptr);
    if (FAILED(result))
    {
        m_impl->ReleaseDevices();
        return false;
    }
    result = m_impl->directInput->CreateDevice(GUID_SysKeyboard, &m_impl->keyboard, nullptr);
    if (FAILED(result))
    {
        m_impl->ReleaseDevices();
        return false;
    }
    if (FAILED(m_impl->keyboard->SetDataFormat(&c_dfDIKeyboard)))
    {
        m_impl->ReleaseDevices();
        return false;
    }
    const auto nativeWindow = static_cast<HWND>(window.value);
    if (FAILED(m_impl->keyboard->SetCooperativeLevel(nativeWindow, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE)))
    {
        m_impl->ReleaseDevices();
        return false;
    }

    m_impl->keyDown = std::move(keyDown);
    m_impl->keyUp = std::move(keyUp);
    m_impl->keyboard->Acquire();
    return true;
}

void PlatformInput::Update()
{
    if (!m_impl->keyboard)
        return;
    if (FAILED(m_impl->keyboard->GetDeviceState(static_cast<DWORD>(m_impl->state.size()), m_impl->state.data())))
    {
        m_impl->keyboard->Acquire();
        return;
    }

    for (std::size_t index = 0; index < m_impl->state.size(); ++index)
    {
        const bool down = (m_impl->state[index] & 0x80u) != 0;
        if (down == m_impl->pressed[index])
            continue;
        m_impl->pressed[index] = down;
        const auto code = static_cast<ScanCode>(index);
        if (down)
        {
            if (m_impl->keyDown)
                m_impl->keyDown(code);
        }
        else if (m_impl->keyUp)
        {
            m_impl->keyUp(code);
        }
    }
}

void PlatformInput::Reset()
{
    m_impl->state.fill(0);
    m_impl->pressed.fill(false);
}

bool PlatformInput::IsPressed(ScanCode code) const
{
    return code < m_impl->pressed.size() && m_impl->pressed[code];
}

void PlatformInput::DisableAccessibilityShortcuts()
{
    if (m_impl->accessibilityShortcutsChanged)
        return;
    STICKYKEYS keys{};
    keys.cbSize = sizeof(keys);
    if (!SystemParametersInfoW(SPI_GETSTICKYKEYS, sizeof(keys), &keys, 0))
        return;
    m_impl->stickyKeysFlags = keys.dwFlags;
    keys.dwFlags &= ~(SKF_AVAILABLE | SKF_HOTKEYACTIVE);
    if (SystemParametersInfoW(SPI_SETSTICKYKEYS, sizeof(keys), &keys, 0))
        m_impl->accessibilityShortcutsChanged = true;
}

void PlatformInput::RestoreAccessibilityShortcuts()
{
    m_impl->RestoreAccessibilityShortcuts();
}
}
