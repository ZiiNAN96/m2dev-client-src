#include "Platform/InputKeyCodes.h"
#include "Platform/NativeTypes.h"
#include "Platform/PlatformClipboard.h"
#include "Platform/PlatformDynamicLibrary.h"
#include "Platform/PlatformFilesystem.h"
#include "Platform/PlatformInput.h"
#include "Platform/PlatformNetworking.h"
#include "Platform/PlatformShell.h"
#include "Platform/PlatformTime.h"
#include "Platform/PlatformWebView.h"
#include "Platform/PlatformWindow.h"
#include "Platform/TouchInput.h"
#include "Platform/Android/AndroidLifecycle.h"
#include "Platform/Android/AndroidWindow.h"
#include "Platform/Android/AndroidInput.h"
#include "Platform/Android/AndroidFilesystem.h"
#include "Renderer/Vulkan/VulkanBootstrap.h"

#include <type_traits>

#if defined(_WINDOWS_) || defined(_INC_WINDOWS) || defined(_WINSOCKAPI_) || defined(_WINSOCK2API_) || defined(__DINPUT_INCLUDED__)
#error "A platform/bootstrap public header leaked a Windows SDK header"
#endif

static_assert(sizeof(Platform::NativeWindowHandle) == sizeof(void*));
static_assert(std::is_standard_layout_v<Platform::TouchEvent>);
static_assert(std::is_trivially_copyable_v<Platform::TouchEvent>);
static_assert(std::is_same_v<decltype(Platform::TouchEvent::pointerId), std::int32_t>);
static_assert(!std::is_copy_constructible_v<Platform::DynamicLibrary>);
static_assert(!std::is_copy_constructible_v<Renderer::VulkanBootstrap>);

int main() { return 0; }
