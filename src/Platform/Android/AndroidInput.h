#pragma once

#include "Platform/TouchInput.h"
#include <unordered_map>

struct AInputEvent;

namespace Platform::Android
{
class Input final
{
public:
    explicit Input(TouchEventHandler handler = {});
    bool Handle(AInputEvent* event);
    void Cancel();

private:
    void Emit(TouchEvent event);
    TouchEventHandler m_handler;
    std::unordered_map<std::int32_t, TouchEvent> m_touches;
};
}
