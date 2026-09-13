#include "DDSImageData.h"
#include <algorithm>
#include <cstring>

HRESULT ImageData::GetDDS2DView(const uint8_t* data,size_t size,DDS2DView& result) noexcept
{
    result={};
    if(!data) return E_POINTER;
    if(size<128) return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
    const auto word=[&](size_t offset) { uint32_t v; std::memcpy(&v,data+offset,4); return v; };
    if(word(0)!=0x20534444 || word(4)!=124 || word(76)!=32) return E_INVALIDARG;
    if((word(8)&0x800000) || (word(112)&0x200)) return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    DDS2DView view;
    view.width=word(16); view.height=word(12); view.mipCount=word(28)?word(28):1;
    const auto flags=word(80),fourCC=word(84),bits=word(88);
    const auto masks=[&](uint32_t r,uint32_t g,uint32_t b,uint32_t a) {
        return word(92)==r && word(96)==g && word(100)==b && word(104)==a;
    };
    if(flags&4) {
        switch(fourCC) {
        case 0x31545844: view.format=DDSFormat::BC1; break;
        case 0x33545844: view.format=DDSFormat::BC2; break;
        case 0x35545844: view.format=DDSFormat::BC3; break;
        default: return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }
    } else if(flags&0x40) {
        if(bits==32 && masks(0xff0000,0xff00,0xff,0xff000000)) view.format=DDSFormat::BGRA8;
        else if(bits==32 && masks(0xff0000,0xff00,0xff,0)) view.format=DDSFormat::BGRX8;
        else if(bits==32 && masks(0xff,0xff00,0xff0000,0xff000000)) view.format=DDSFormat::RGBA8;
        else if(bits==16 && masks(0x7c00,0x3e0,0x1f,0x8000)) view.format=DDSFormat::B5G5R5A1;
    }
    if(!view.width || !view.height || view.width>8192 || view.height>8192 ||
       view.mipCount>view.mips.size() || view.format==DDSFormat::Unknown) return E_INVALIDARG;
    uint32_t maximum=1;
    for(uint32_t extent=(std::max)(view.width,view.height);extent>1;extent>>=1)++maximum;
    if(view.mipCount>maximum) return E_INVALIDARG;
    uint32_t width=view.width,height=view.height;
    size_t remaining=size-128; const uint8_t* pixels=data+128;
    for(uint32_t mip=0;mip<view.mipCount;++mip) {
        const bool compressed=view.format==DDSFormat::BC1 || view.format==DDSFormat::BC2 || view.format==DDSFormat::BC3;
        const size_t pitch=compressed ? size_t((width+3)/4)*(view.format==DDSFormat::BC1?8:16) :
            size_t(width)*(view.format==DDSFormat::B5G5R5A1?2:4);
        const size_t rows=compressed?(height+3)/4:height;
        const size_t bytes=pitch*rows;
        if(!bytes || bytes>remaining) return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
        view.mips[mip]={pixels,bytes,pitch};
        pixels+=bytes; remaining-=bytes; width=(std::max)(1u,width>>1); height=(std::max)(1u,height>>1);
    }
    result=view; return S_OK;
}
