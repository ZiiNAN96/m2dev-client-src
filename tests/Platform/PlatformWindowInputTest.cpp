#include "Platform/InputKeyCodes.h"
#include "Platform/PlatformInput.h"
#include "Platform/PlatformWindow.h"

#include "EterLib/Input.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>

static_assert(DIK_ESCAPE == 0x01);
static_assert(DIK_A == 0x1e);
static_assert(DIK_SPACE == 0x39);
static_assert(DIK_F12 == 0x58);
static_assert(DIK_NUMPADENTER == 0x9c);
static_assert(DIK_RCONTROL == 0x9d);
static_assert(DIK_HOME == 0xc7);
static_assert(DIK_DELETE == 0xd3);
static_assert(DIK_APPS == 0xdd);
static_assert(DIK_ESC == DIK_ESCAPE);
static_assert(DIK_LALT == DIK_LMENU);
static_assert(DIK_RALT == DIK_RMENU);
static_assert(DIK_PGUP == DIK_PRIOR);
static_assert(DIK_PGDN == DIK_NEXT);

namespace
{
class TestContext
{
public:
    void Check(bool condition, const char* description)
    {
        if (condition)
            return;
        std::cerr << "FAILED: " << description << '\n';
        ++m_failures;
    }

    [[nodiscard]] int Result() const noexcept { return m_failures == 0 ? 0 : 1; }

private:
    int m_failures = 0;
};

bool DrainEvents(Platform::PlatformWindow& window, TestContext& test)
{
    for (int count = 0; count < 256; ++count)
    {
        const auto result = window.PollEvents();
        if (result == Platform::PollResult::Idle)
            return true;
        if (result == Platform::PollResult::Quit)
        {
            test.Check(false, "unexpected quit message while draining test-window events");
            return false;
        }
    }
    test.Check(false, "test-window event queue drains within a bounded number of messages");
    return false;
}

bool ConsumeQuitEvent(Platform::PlatformWindow& window)
{
    for (int count = 0; count < 32; ++count)
    {
        if (window.PollEvents() == Platform::PollResult::Quit)
            return true;
    }
    return false;
}

std::string UniqueTitle(const char* suffix)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::string("M2 C2-X PlatformWindowInputTest ") + suffix + " " + std::to_string(stamp);
}

void TestWindowAndInput(TestContext& test)
{
    Platform::PlatformInput nullInput;
    test.Check(!nullInput.Initialize({}, {}, {}), "input rejects a null native window");
    test.Check(!nullInput.IsPressed(0), "uninitialized input begins released");
    test.Check(!nullInput.IsPressed(256), "input rejects the first out-of-range scan code");
    test.Check(!nullInput.IsPressed(UINT16_MAX), "input rejects the largest scan code");
    nullInput.Reset();

    Platform::WindowCreateInfo info;
    info.title = UniqueTitle("main");
    info.resizable = true;
    info.quitOnClose = true;

    Platform::PlatformWindow original;
    test.Check(original.Create(info), "test window is created");
    const auto originalHandle = original.GetNativeHandle();
    test.Check(static_cast<bool>(originalHandle), "created window exposes a native handle");
    test.Check(original.IsNativeHandleValid(originalHandle), "created native handle is valid");
    test.Check(original.HasWindowWithTitle(info.title.c_str()), "created window retains its title");

    Platform::PlatformWindow moved(std::move(original));
    test.Check(!original.GetNativeHandle(), "move construction clears the source handle");
    original.Destroy();
    test.Check(moved.GetNativeHandle() == originalHandle, "move construction preserves the window handle");

    Platform::PlatformWindow window;
    Platform::WindowCreateInfo replacedInfo;
    replacedInfo.title = UniqueTitle("move-target");
    replacedInfo.quitOnClose = false;
    test.Check(window.Create(replacedInfo), "move-assignment target window is created");
    const auto replacedHandle = window.GetNativeHandle();
    window = std::move(moved);
    test.Check(!moved.GetNativeHandle(), "move assignment clears the source handle");
    moved.Destroy();
    test.Check(window.GetNativeHandle() == originalHandle, "move assignment preserves the source window");
    test.Check(!window.IsNativeHandleValid(replacedHandle), "move assignment destroys the replaced window");

    window.AdjustClientSize(320, 240);
    auto clientRect = window.GetClientRect();
    test.Check(clientRect.right - clientRect.left == 320 && clientRect.bottom - clientRect.top == 240,
               "AdjustClientSize produces the requested client dimensions");
    window.SetPosition(24, 36);
    const auto windowRect = window.GetWindowRect();
    test.Check(windowRect.left == 24 && windowRect.top == 36, "SetPosition moves the test window");

    window.Show(true);
    if (!DrainEvents(window, test))
        return;
    test.Check(window.IsVisible(), "shown test window reports visible");

    Platform::PlatformInput input;
    bool callbackOutOfRange = false;
    const auto observeCode = [&callbackOutOfRange](Platform::ScanCode code) {
        callbackOutOfRange = callbackOutOfRange || code >= 256;
    };
    test.Check(input.Initialize(window.GetNativeHandle(), observeCode, observeCode),
               "DirectInput keyboard initializes against the test window");
    input.Update();
    test.Check(!callbackOutOfRange, "input callbacks preserve the 0..255 scan-code range");
    input.Reset();
    bool anyPressed = false;
    for (Platform::ScanCode code = 0; code < 256; ++code)
        anyPressed = anyPressed || input.IsPressed(code);
    test.Check(!anyPressed, "Reset clears every keyboard state entry");
    test.Check(!input.IsPressed(256), "state lookup rejects scan code 256 after initialization");
    test.Check(!input.IsPressed(UINT16_MAX), "state lookup rejects the maximum scan code after initialization");

    auto firstChild = window.CreateChildRenderSurface(160, 120);
    auto secondChild = window.CreateChildRenderSurface(80, 60);
    test.Check(static_cast<bool>(firstChild) && static_cast<bool>(secondChild),
               "multiple renderer child surfaces are created");
    if (firstChild && secondChild)
    {
        const auto firstHandle = firstChild->GetNativeHandle();
        const auto secondHandle = secondChild->GetNativeHandle();
        test.Check(window.IsNativeHandleValid(firstHandle) && window.IsNativeHandleValid(secondHandle),
                   "renderer child handles are valid");
        test.Check(firstChild->Resize(200, 140), "renderer child surface resizes");
        const auto childRect = firstChild->GetClientRect();
        test.Check(childRect.right - childRect.left == 200 && childRect.bottom - childRect.top == 140,
                   "renderer child exposes its resized client dimensions");
        firstChild->Show(true);
        test.Check(firstChild->IsVisible(), "renderer child can be shown without activation");
        firstChild.reset();
        test.Check(!window.IsNativeHandleValid(firstHandle), "destroying one child releases only its native window");
        test.Check(window.IsNativeHandleValid(secondHandle), "the sibling child remains owned and valid");
        secondChild.reset();
        test.Check(!window.IsNativeHandleValid(secondHandle), "destroying the final child releases its native window");
        test.Check(window.IsNativeHandleValid(originalHandle), "child destruction leaves the parent window valid");
    }

    for (int cycle = 0; cycle < 3; ++cycle)
    {
        window.Minimize();
        DrainEvents(window, test);
        test.Check(window.IsWindowMinimized(), "test window reports minimized state");
        window.Restore();
        DrainEvents(window, test);
        test.Check(!window.IsWindowMinimized(), "test window reports restored state");
    }

    window.RequestQuit(0);
    test.Check(ConsumeQuitEvent(window), "RequestQuit posts a quit event");

    auto parentOwnedChild = window.CreateChildRenderSurface(32, 32);
    test.Check(static_cast<bool>(parentOwnedChild), "final renderer child surface is created");
    const auto parentOwnedChildHandle = parentOwnedChild
        ? parentOwnedChild->GetNativeHandle()
        : Platform::NativeWindowHandle{};

    window.Show(false);
    DrainEvents(window, test);
    test.Check(window.RequestClose(), "WM_CLOSE is posted to the test window");
    test.Check(ConsumeQuitEvent(window), "quit-on-close converts WM_CLOSE into a quit event");
    test.Check(window.IsNativeHandleValid(originalHandle), "quit-on-close does not destroy the window implicitly");
    window.Destroy();
    test.Check(!window.GetNativeHandle(), "Destroy clears the public native handle");
    test.Check(!window.IsNativeHandleValid(originalHandle), "Destroy releases the native test window");
    test.Check(!window.IsNativeHandleValid(parentOwnedChildHandle),
               "destroying the parent also releases its native child window");
    parentOwnedChild.reset();
}
}

int main()
{
    TestContext test;
    TestWindowAndInput(test);
    return test.Result();
}
