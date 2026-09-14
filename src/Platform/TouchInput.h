#pragma once

#include <cstdint>
#include <functional>

namespace Platform
{
enum class TouchPhase : std::uint8_t { Down, Move, Up, Cancel };

struct TouchEvent
{
    TouchPhase phase = TouchPhase::Cancel;
    std::int32_t pointerId = -1;
    float x = 0;
    float y = 0;
};

using TouchEventHandler = std::function<void(const TouchEvent&)>;
}
