#pragma once
#include <cstdint>
#include <vector>

// ZiiNAN: Encode the composed Diligent RGB surface with the existing image library.
bool SaveScreenshotJPEG(const wchar_t* path,const std::vector<uint8_t>& rgb,uint32_t width,uint32_t height);
