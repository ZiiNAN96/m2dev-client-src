#include "StdAfx.h"
#include "MSApplication.h"

#include <windows.h>

CMSApplication::CMSApplication() = default;
CMSApplication::~CMSApplication() = default;

void CMSApplication::Initialize(void* instance)
{
    m_platformWindow.SetInstance(instance);
}

void CMSApplication::MessageLoop()
{
    while (true)
    {
        const auto result = PollEvents();
        if (result == Platform::PollResult::Quit)
            return;
        if (result == Platform::PollResult::Idle)
            m_platformWindow.WaitForEvents();
    }
}

Platform::PollResult CMSApplication::PollEvents() { return m_platformWindow.PollEvents(); }
bool CMSApplication::IsMessage() { return m_platformWindow.HasPendingEvents(); }
bool CMSApplication::MessageProcess() { return PollEvents() != Platform::PollResult::Quit; }

std::intptr_t CMSApplication::WindowProcedure(const Platform::NativeMessage& message)
{
    if (message.id == WM_CLOSE)
        m_platformWindow.RequestQuit(0);
    return CMSWindow::WindowProcedure(message);
}
