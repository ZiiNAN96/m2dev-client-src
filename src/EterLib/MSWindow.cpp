#include "StdAfx.h"
#include "MSWindow.h"

CMSWindow::CMSWindow()
{
    // ZiiNAN: Platform abstraction
    m_platformWindow.SetMessageHandler([this](const Platform::NativeMessage& message, bool& handled) {
        handled = true;
        return WindowProcedure(message);
    });
}

CMSWindow::~CMSWindow()
{
    m_platformWindow.SetMessageHandler({});
}

void CMSWindow::Destroy() { m_platformWindow.Destroy(); }
bool CMSWindow::Create(const Platform::WindowCreateInfo& info) { return m_platformWindow.Create(info); }
void CMSWindow::Show() { m_platformWindow.Show(true); }
void CMSWindow::Hide() { m_platformWindow.Show(false); }
void CMSWindow::SetVisibleMode(bool visible) { m_platformWindow.Show(visible); }
void CMSWindow::SetPosition(int x, int y) { m_platformWindow.SetPosition(x, y); }

void CMSWindow::SetCenterPosition()
{
    const auto rect = GetClientRect();
    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;
    SetPosition((GetScreenWidth() - width) / 2, (GetScreenHeight() - height) / 2);
}

void CMSWindow::SetText(const char* text) { m_platformWindow.SetTitle(text); }
void CMSWindow::AdjustSize(int width, int height) { m_platformWindow.AdjustClientSize(width, height); }
void CMSWindow::SetSize(int width, int height) { m_platformWindow.SetSize(width, height); }
bool CMSWindow::IsVisible() const { return m_platformWindow.IsVisible(); }
bool CMSWindow::IsActive() const { return m_platformWindow.IsActive(); }
bool CMSWindow::IsWindowMinimized() const { return m_platformWindow.IsWindowMinimized(); }
Platform::Point CMSWindow::GetMousePosition() const { return m_platformWindow.GetMousePosition(); }
Platform::Rect CMSWindow::GetClientRect() const { return m_platformWindow.GetClientRect(); }
Platform::Rect CMSWindow::GetWindowRect() const { return m_platformWindow.GetWindowRect(); }
int CMSWindow::GetScreenWidth() const { return m_platformWindow.GetScreenWidth(); }
int CMSWindow::GetScreenHeight() const { return m_platformWindow.GetScreenHeight(); }
Platform::NativeWindowHandle CMSWindow::GetNativeHandle() const { return m_platformWindow.GetNativeHandle(); }
Platform::PlatformWindow& CMSWindow::GetPlatformWindow() { return m_platformWindow; }
const Platform::PlatformWindow& CMSWindow::GetPlatformWindow() const { return m_platformWindow; }

std::intptr_t CMSWindow::WindowProcedure(const Platform::NativeMessage& message)
{
    return m_platformWindow.DefaultWindowProcedure(message);
}
