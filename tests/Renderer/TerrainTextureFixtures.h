#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace TerrainFixture
{
inline void Word(std::vector<uint8_t>& bytes, size_t offset, uint32_t value) { memcpy(bytes.data()+offset,&value,4); }
inline std::vector<uint8_t> DDS(uint32_t width=64, uint32_t height=64, uint32_t mips=7, uint32_t fourCC=0x31545844)
{
    std::vector<uint8_t> bytes(128);
    Word(bytes,0,0x20534444); Word(bytes,4,124); Word(bytes,8,0xA1007);
    Word(bytes,12,height); Word(bytes,16,width); Word(bytes,28,mips);
    Word(bytes,76,32); Word(bytes,80,4); Word(bytes,84,fourCC); Word(bytes,108,0x401008);
    const uint32_t blockSize=fourCC==0x31545844 ? 8 : 16;
    const uint16_t colors[]{0xf800,0x07e0,0x001f,0xffe0,0xf81f,0x07ff,0xffff};
    for(uint32_t mip=0;mip<mips;++mip)
    {
        const auto start=bytes.size();
        const auto size=size_t((width+3)/4)*((height+3)/4)*blockSize;
        bytes.resize(start+size);
        for(size_t offset=start;offset<bytes.size();offset+=blockSize)
        {
            // Each BC1 mip is a distinct constant color, for observed LOD selection.
            const auto color=colors[mip%7];
            memcpy(bytes.data()+offset+blockSize-8,&color,2);
        }
        width=std::max(1u,width>>1); height=std::max(1u,height>>1);
    }
    return bytes;
}
inline std::vector<uint8_t> TGA()
{
    // Top-origin, asymmetric 2x2 BGR pixels: red, green, blue, yellow.
    std::vector<uint8_t> bytes(18);
    bytes[2]=2; bytes[12]=2; bytes[14]=2; bytes[16]=24; bytes[17]=0x20;
    const uint8_t pixels[]{0,0,255,0,255,0,255,0,0,0,255,255};
    bytes.insert(bytes.end(),std::begin(pixels),std::end(pixels));
    return bytes;
}
inline std::vector<uint8_t> GradientDDS()
{
    std::vector<uint8_t> bytes(128+32*32*4);
    Word(bytes,0,0x20534444); Word(bytes,4,124); Word(bytes,8,0x100F);
    Word(bytes,12,32); Word(bytes,16,32); Word(bytes,20,128); Word(bytes,28,1);
    Word(bytes,76,32); Word(bytes,80,0x41); Word(bytes,88,32);
    Word(bytes,92,0xff0000); Word(bytes,96,0xff00); Word(bytes,100,0xff); Word(bytes,104,0xff000000);
    Word(bytes,108,0x1000);
    for(uint32_t y=0;y<32;++y) for(uint32_t x=0;x<32;++x)
    {
        const auto p=128+(y*32+x)*4;
        bytes[p]=uint8_t(20+140*((x/8+y/8)%2)); bytes[p+1]=uint8_t(20+y*6);
        bytes[p+2]=uint8_t(20+x*6); bytes[p+3]=255;
    }
    return bytes;
}
inline std::vector<uint8_t> AlphaDDS()
{
    auto bytes=GradientDDS();
    const uint8_t levels[]{0,1,127,128,254,255,128,0};
    for(uint32_t y=0;y<32;++y) for(uint32_t x=0;x<32;++x)
        bytes[128+(y*32+x)*4+3]=levels[x/4];
    return bytes;
}
inline std::vector<uint8_t> B5G5R5A1DDS()
{
    auto bytes=GradientDDS(); bytes.resize(128);
    Word(bytes,8,0x2100F); Word(bytes,12,16); Word(bytes,16,16); Word(bytes,20,32); Word(bytes,28,5);
    Word(bytes,88,16); Word(bytes,92,0x7c00); Word(bytes,96,0x3e0); Word(bytes,100,0x1f); Word(bytes,104,0x8000);
    Word(bytes,108,0x401008);
    for(uint32_t mip=0,size=16;mip<5;++mip,size=std::max(1u,size>>1))
        for(uint32_t y=0;y<size;++y) for(uint32_t x=0;x<size;++x) {
            const uint16_t pixel=uint16_t((((x+y)%2)!=0 ? 0x8000 : 0)|((31-mip*5)<<10)|((x*2%32)<<5)|(y*2%32));
            bytes.push_back(uint8_t(pixel)); bytes.push_back(uint8_t(pixel>>8));
        }
    return bytes;
}
}
