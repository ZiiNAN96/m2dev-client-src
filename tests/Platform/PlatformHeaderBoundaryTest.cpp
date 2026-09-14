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

#include "EterBase/CRC32.h"
#include "EterBase/Debug.h"
#include "Renderer/DrawStateTypes.h"

#if defined(_WINDOWS_) || defined(_INC_WINDOWS)
#error "A neutral public header leaked <windows.h>"
#endif

#if defined(_WINSOCKAPI_) || defined(_WINSOCK2API_)
#error "A neutral public header leaked a Winsock header"
#endif

#if defined(__DINPUT_INCLUDED__)
#error "A neutral public header leaked <dinput.h>"
#endif

#if defined(_INC_SHELLAPI)
#error "A neutral public header leaked <shellapi.h>"
#endif

#if defined(_INC_MMSYSTEM)
#error "A neutral public header leaked <mmsystem.h>"
#endif

#include <cstdint>
#include <type_traits>

static_assert(sizeof(Platform::NativeWindowHandle) == sizeof(void*));
static_assert(std::is_standard_layout_v<Platform::NativeWindowHandle>);
static_assert(std::is_trivially_copyable_v<Platform::NativeWindowHandle>);
static_assert(std::is_standard_layout_v<Platform::Point>);
static_assert(std::is_standard_layout_v<Platform::Rect>);
static_assert(std::is_same_v<Platform::ScanCode, std::uint16_t>);
static_assert(!std::is_copy_constructible_v<Platform::PlatformWindow>);
static_assert(std::is_move_constructible_v<Platform::PlatformWindow>);
static_assert(!std::is_copy_constructible_v<Platform::PlatformInput>);
static_assert(std::is_move_constructible_v<Platform::PlatformInput>);
static_assert(!std::is_copy_constructible_v<Platform::DynamicLibrary>);
static_assert(std::is_move_constructible_v<Platform::DynamicLibrary>);

static_assert(Platform::InputKey::Escape == 0x01);
static_assert(Platform::InputKey::A == 0x1e);
static_assert(Platform::InputKey::F12 == 0x58);
static_assert(Platform::InputKey::NumpadEnter == 0x9c);
static_assert(Platform::InputKey::RightControl == 0x9d);
static_assert(Platform::InputKey::Home == 0xc7);
static_assert(Platform::InputKey::Delete == 0xd3);
static_assert(Platform::InputKey::Apps == 0xdd);

static_assert(Renderer::StateAlphaBlendEnable == 27);
static_assert(Renderer::SamplerAddressU == 1);
static_assert(Renderer::TopologyTriangleList == 4);
static_assert(Renderer::VertexPosition == 2);
static_assert(Renderer::VertexNormal == 16);

int main()
{
    return 0;
}
