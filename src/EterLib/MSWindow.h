#pragma once

#include "Platform/PlatformWindow.h"

class CMSWindow
{
public:
    CMSWindow();
    virtual ~CMSWindow();

    void Destroy();
    bool Create(const Platform::WindowCreateInfo& info);

    void Show();
    void Hide();
    void SetVisibleMode(bool visible);
    void SetPosition(int x, int y);
    void SetCenterPosition();
    void SetText(const char* text);
    void AdjustSize(int width, int height);
    void SetSize(int width, int height);

    bool IsVisible() const;
    bool IsActive() const;
    bool IsWindowMinimized() const;
    Platform::Point GetMousePosition() const;
    Platform::Rect GetClientRect() const;
    Platform::Rect GetWindowRect() const;
    int GetScreenWidth() const;
    int GetScreenHeight() const;

    Platform::NativeWindowHandle GetNativeHandle() const;
    Platform::PlatformWindow& GetPlatformWindow();
    const Platform::PlatformWindow& GetPlatformWindow() const;

protected:
    virtual std::intptr_t WindowProcedure(const Platform::NativeMessage& message);

    Platform::PlatformWindow m_platformWindow;
};
