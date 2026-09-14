#pragma once

#include "MSWindow.h"

class CMSApplication : public CMSWindow
{
public:
    CMSApplication();
    ~CMSApplication() override;

    void Initialize(void* instance);
    void MessageLoop();
    Platform::PollResult PollEvents();

    // Compatibility helpers for callers not yet moved to PollEvents.
    bool IsMessage();
    bool MessageProcess();

protected:
    std::intptr_t WindowProcedure(const Platform::NativeMessage& message) override;
};
