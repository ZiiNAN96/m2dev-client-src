#pragma once

#include "Platform/InputKeyCodes.h"
#include "Platform/PlatformInput.h"

// Compatibility names consumed by the Python API. Values remain the historical DIK ABI.
inline constexpr int DIK_ESCAPE = Platform::InputKey::Escape;
inline constexpr int DIK_ESC = DIK_ESCAPE;
inline constexpr int DIK_1 = Platform::InputKey::Digit1;
inline constexpr int DIK_2 = Platform::InputKey::Digit2;
inline constexpr int DIK_3 = Platform::InputKey::Digit3;
inline constexpr int DIK_4 = Platform::InputKey::Digit4;
inline constexpr int DIK_5 = Platform::InputKey::Digit5;
inline constexpr int DIK_6 = Platform::InputKey::Digit6;
inline constexpr int DIK_7 = Platform::InputKey::Digit7;
inline constexpr int DIK_8 = Platform::InputKey::Digit8;
inline constexpr int DIK_9 = Platform::InputKey::Digit9;
inline constexpr int DIK_0 = Platform::InputKey::Digit0;
inline constexpr int DIK_MINUS = Platform::InputKey::Minus;
inline constexpr int DIK_EQUALS = Platform::InputKey::Equals;
inline constexpr int DIK_BACK = Platform::InputKey::Backspace;
inline constexpr int DIK_TAB = Platform::InputKey::Tab;
inline constexpr int DIK_Q = Platform::InputKey::Q;
inline constexpr int DIK_W = Platform::InputKey::W;
inline constexpr int DIK_E = Platform::InputKey::E;
inline constexpr int DIK_R = Platform::InputKey::R;
inline constexpr int DIK_T = Platform::InputKey::T;
inline constexpr int DIK_Y = Platform::InputKey::Y;
inline constexpr int DIK_U = Platform::InputKey::U;
inline constexpr int DIK_I = Platform::InputKey::I;
inline constexpr int DIK_O = Platform::InputKey::O;
inline constexpr int DIK_P = Platform::InputKey::P;
inline constexpr int DIK_LBRACKET = Platform::InputKey::LeftBracket;
inline constexpr int DIK_RBRACKET = Platform::InputKey::RightBracket;
inline constexpr int DIK_RETURN = Platform::InputKey::Return;
inline constexpr int DIK_LCONTROL = Platform::InputKey::LeftControl;
inline constexpr int DIK_A = Platform::InputKey::A;
inline constexpr int DIK_S = Platform::InputKey::S;
inline constexpr int DIK_D = Platform::InputKey::D;
inline constexpr int DIK_F = Platform::InputKey::F;
inline constexpr int DIK_G = Platform::InputKey::G;
inline constexpr int DIK_H = Platform::InputKey::H;
inline constexpr int DIK_J = Platform::InputKey::J;
inline constexpr int DIK_K = Platform::InputKey::K;
inline constexpr int DIK_L = Platform::InputKey::L;
inline constexpr int DIK_SEMICOLON = Platform::InputKey::Semicolon;
inline constexpr int DIK_APOSTROPHE = Platform::InputKey::Apostrophe;
inline constexpr int DIK_GRAVE = Platform::InputKey::Grave;
inline constexpr int DIK_LSHIFT = Platform::InputKey::LeftShift;
inline constexpr int DIK_BACKSLASH = Platform::InputKey::Backslash;
inline constexpr int DIK_Z = Platform::InputKey::Z;
inline constexpr int DIK_X = Platform::InputKey::X;
inline constexpr int DIK_C = Platform::InputKey::C;
inline constexpr int DIK_V = Platform::InputKey::V;
inline constexpr int DIK_B = Platform::InputKey::B;
inline constexpr int DIK_N = Platform::InputKey::N;
inline constexpr int DIK_M = Platform::InputKey::M;
inline constexpr int DIK_COMMA = Platform::InputKey::Comma;
inline constexpr int DIK_PERIOD = Platform::InputKey::Period;
inline constexpr int DIK_SLASH = Platform::InputKey::Slash;
inline constexpr int DIK_RSHIFT = Platform::InputKey::RightShift;
inline constexpr int DIK_MULTIPLY = Platform::InputKey::Multiply;
inline constexpr int DIK_LMENU = Platform::InputKey::LeftAlt;
inline constexpr int DIK_LALT = DIK_LMENU;
inline constexpr int DIK_SPACE = Platform::InputKey::Space;
inline constexpr int DIK_CAPITAL = Platform::InputKey::Capital;
inline constexpr int DIK_F1 = Platform::InputKey::F1;
inline constexpr int DIK_F2 = Platform::InputKey::F2;
inline constexpr int DIK_F3 = Platform::InputKey::F3;
inline constexpr int DIK_F4 = Platform::InputKey::F4;
inline constexpr int DIK_F5 = Platform::InputKey::F5;
inline constexpr int DIK_F6 = Platform::InputKey::F6;
inline constexpr int DIK_F7 = Platform::InputKey::F7;
inline constexpr int DIK_F8 = Platform::InputKey::F8;
inline constexpr int DIK_F9 = Platform::InputKey::F9;
inline constexpr int DIK_F10 = Platform::InputKey::F10;
inline constexpr int DIK_NUMLOCK = Platform::InputKey::NumLock;
inline constexpr int DIK_SCROLL = Platform::InputKey::Scroll;
inline constexpr int DIK_NUMPAD7 = Platform::InputKey::Numpad7;
inline constexpr int DIK_NUMPAD8 = Platform::InputKey::Numpad8;
inline constexpr int DIK_NUMPAD9 = Platform::InputKey::Numpad9;
inline constexpr int DIK_SUBTRACT = Platform::InputKey::Subtract;
inline constexpr int DIK_NUMPAD4 = Platform::InputKey::Numpad4;
inline constexpr int DIK_NUMPAD5 = Platform::InputKey::Numpad5;
inline constexpr int DIK_NUMPAD6 = Platform::InputKey::Numpad6;
inline constexpr int DIK_ADD = Platform::InputKey::Add;
inline constexpr int DIK_NUMPAD1 = Platform::InputKey::Numpad1;
inline constexpr int DIK_NUMPAD2 = Platform::InputKey::Numpad2;
inline constexpr int DIK_NUMPAD3 = Platform::InputKey::Numpad3;
inline constexpr int DIK_NUMPAD0 = Platform::InputKey::Numpad0;
inline constexpr int DIK_DECIMAL = Platform::InputKey::Decimal;
inline constexpr int DIK_F11 = Platform::InputKey::F11;
inline constexpr int DIK_F12 = Platform::InputKey::F12;
inline constexpr int DIK_NEXTTRACK = Platform::InputKey::NextTrack;
inline constexpr int DIK_NUMPADENTER = Platform::InputKey::NumpadEnter;
inline constexpr int DIK_RCONTROL = Platform::InputKey::RightControl;
inline constexpr int DIK_MUTE = Platform::InputKey::Mute;
inline constexpr int DIK_CALCULATOR = Platform::InputKey::Calculator;
inline constexpr int DIK_PLAYPAUSE = Platform::InputKey::PlayPause;
inline constexpr int DIK_MEDIASTOP = Platform::InputKey::MediaStop;
inline constexpr int DIK_VOLUMEDOWN = Platform::InputKey::VolumeDown;
inline constexpr int DIK_VOLUMEUP = Platform::InputKey::VolumeUp;
inline constexpr int DIK_WEBHOME = Platform::InputKey::WebHome;
inline constexpr int DIK_NUMPADCOMMA = Platform::InputKey::NumpadComma;
inline constexpr int DIK_DIVIDE = Platform::InputKey::Divide;
inline constexpr int DIK_SYSRQ = Platform::InputKey::SystemRequest;
inline constexpr int DIK_RMENU = Platform::InputKey::RightAlt;
inline constexpr int DIK_RALT = DIK_RMENU;
inline constexpr int DIK_PAUSE = Platform::InputKey::Pause;
inline constexpr int DIK_HOME = Platform::InputKey::Home;
inline constexpr int DIK_UP = Platform::InputKey::Up;
inline constexpr int DIK_PRIOR = Platform::InputKey::PageUp;
inline constexpr int DIK_PGUP = DIK_PRIOR;
inline constexpr int DIK_LEFT = Platform::InputKey::Left;
inline constexpr int DIK_RIGHT = Platform::InputKey::Right;
inline constexpr int DIK_END = Platform::InputKey::End;
inline constexpr int DIK_DOWN = Platform::InputKey::Down;
inline constexpr int DIK_NEXT = Platform::InputKey::PageDown;
inline constexpr int DIK_PGDN = DIK_NEXT;
inline constexpr int DIK_INSERT = Platform::InputKey::Insert;
inline constexpr int DIK_DELETE = Platform::InputKey::Delete;
inline constexpr int DIK_LWIN = Platform::InputKey::LeftWindows;
inline constexpr int DIK_RWIN = Platform::InputKey::RightWindows;
inline constexpr int DIK_APPS = Platform::InputKey::Apps;

class CInputDevice
{
public:
    CInputDevice() = default;
    virtual ~CInputDevice() = default;
};

class CInputKeyboard : public CInputDevice
{
public:
    CInputKeyboard();
    ~CInputKeyboard() override;

    bool InitializeKeyboard(Platform::NativeWindowHandle window);
    void UpdateKeyboard();
    void ResetKeyboard();
    bool IsPressed(int index) const;
    void DisableAccessibilityShortcuts();
    void RestoreAccessibilityShortcuts();

protected:
    virtual void OnKeyDown(int index) = 0;
    virtual void OnKeyUp(int index) = 0;

private:
    Platform::PlatformInput m_input;
};
