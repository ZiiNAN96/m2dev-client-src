#include "StdAfx.h"
#include "Input.h"

CInputKeyboard::CInputKeyboard()
{
    ResetKeyboard();
}

CInputKeyboard::~CInputKeyboard() = default;

bool CInputKeyboard::InitializeKeyboard(Platform::NativeWindowHandle window)
{
    // ZiiNAN: Platform abstraction
    return m_input.Initialize(window,
        [this](Platform::ScanCode code) { OnKeyDown(static_cast<int>(code)); },
        [this](Platform::ScanCode code) { OnKeyUp(static_cast<int>(code)); });
}

void CInputKeyboard::UpdateKeyboard() { m_input.Update(); }
void CInputKeyboard::ResetKeyboard() { m_input.Reset(); }

bool CInputKeyboard::IsPressed(int index) const
{
    return index >= 0 && index < 256 && m_input.IsPressed(static_cast<Platform::ScanCode>(index));
}

void CInputKeyboard::DisableAccessibilityShortcuts() { m_input.DisableAccessibilityShortcuts(); }
void CInputKeyboard::RestoreAccessibilityShortcuts() { m_input.RestoreAccessibilityShortcuts(); }
