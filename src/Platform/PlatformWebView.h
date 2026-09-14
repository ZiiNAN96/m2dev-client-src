#pragma once
#include "NativeTypes.h"

namespace Platform::WebView
{
// ZiiNAN: Platform abstraction
using ErrorHandler = void (*)(long result);
void Show(NativeWindowHandle parent, const char* url, const Rect& bounds, ErrorHandler onError);
void Move(const Rect& bounds);
void Hide();
bool IsVisible();
}
