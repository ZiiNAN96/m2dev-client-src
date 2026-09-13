#pragma once
#include <Windows.h>
#include <array>
#include <cstdint>
#include <cstddef>

namespace ImageData {
// ZiiNAN: Removed final D3D9 compile-time dependency. CPU-only borrowed DDS views.
enum class DDSFormat { Unknown, BC1, BC2, BC3, BGRA8, BGRX8, RGBA8, B5G5R5A1 };
struct DDS2DView {
    struct Mip { const uint8_t* data=nullptr; size_t size=0,rowPitch=0; };
    uint32_t width=0,height=0,mipCount=0;
    DDSFormat format=DDSFormat::Unknown;
    std::array<Mip,14> mips{};
};
HRESULT GetDDS2DView(const uint8_t*,size_t,DDS2DView&) noexcept;
}
