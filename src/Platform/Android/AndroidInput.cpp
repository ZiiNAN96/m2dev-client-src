#include "AndroidInput.h"
#include <android/input.h>
#include <utility>

namespace Platform::Android
{
Input::Input(TouchEventHandler handler) : m_handler{std::move(handler)} {}

void Input::Emit(TouchEvent event)
{
    if (event.phase == TouchPhase::Down || event.phase == TouchPhase::Move)
        m_touches[event.pointerId] = event;
    else
        m_touches.erase(event.pointerId);
    if (m_handler)
        m_handler(event);
}

bool Input::Handle(AInputEvent* event)
{
    if (!event || AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION ||
        (AInputEvent_getSource(event) & AINPUT_SOURCE_TOUCHSCREEN) != AINPUT_SOURCE_TOUCHSCREEN)
        return false;

    const auto action = AMotionEvent_getAction(event);
    const auto masked = action & AMOTION_EVENT_ACTION_MASK;
    if (masked == AMOTION_EVENT_ACTION_CANCEL)
    {
        Cancel();
        return true;
    }
    const auto count = AMotionEvent_getPointerCount(event);
    const auto emitPointer = [&](std::size_t index, TouchPhase phase)
    {
        if (index < count)
            Emit({phase, AMotionEvent_getPointerId(event, index),
                  AMotionEvent_getX(event, index), AMotionEvent_getY(event, index)});
    };
    if (masked == AMOTION_EVENT_ACTION_MOVE)
    {
        for (std::size_t index = 0; index < count; ++index)
            emitPointer(index, TouchPhase::Move);
        return true;
    }
    const auto index = static_cast<std::size_t>((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                                               AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
    if (masked == AMOTION_EVENT_ACTION_DOWN || masked == AMOTION_EVENT_ACTION_POINTER_DOWN)
    {
        emitPointer(index, TouchPhase::Down);
        return true;
    }
    if (masked == AMOTION_EVENT_ACTION_UP || masked == AMOTION_EVENT_ACTION_POINTER_UP)
    {
        emitPointer(index, TouchPhase::Up);
        return true;
    }
    return false;
}

void Input::Cancel()
{
    auto touches = std::move(m_touches);
    m_touches.clear();
    for (auto& [id, event] : touches)
    {
        event.phase = TouchPhase::Cancel;
        if (m_handler)
            m_handler(event);
    }
}
}
