#include "EterLib/StdAfx.h"
#include "EterLib/TerrainTextureLoader.h"
#include "EterLib/StaticObjectTextureLoader.h"
#include "EterImageLib/DDSImageData.h"
#include "TerrainTextureFixtures.h"
#include "GameLib/TerrainAlphaImage.h"
#include <iostream>
#include <stdexcept>

static void Check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
class UploadProbe final : public Renderer::ITerrainRenderer
{
public:
    uint32_t width=0,height=0;
    Renderer::TerrainTextureFormat format{};
    std::vector<std::vector<uint8_t>> mips;
    std::vector<size_t> strides;
    Renderer::TerrainTexturePtr UploadTexture(const Renderer::TerrainTextureData& data) override
    {
        width=data.width; height=data.height; format=data.format; mips.clear(); strides.clear();
        if(!width || data.mips.empty()) return {};
        for(const auto& mip:data.mips)
        {
            const auto* bytes=static_cast<const uint8_t*>(mip.data);
            mips.emplace_back(bytes,bytes+mip.size); strides.push_back(mip.rowStride);
        }
        return std::make_shared<Renderer::TerrainTexture>();
    }
    void ReleaseTexture(Renderer::TerrainTexturePtr& p) override { p.reset(); }
    Renderer::TerrainSplatMaterialPtr CreateSplatMaterial(const Renderer::TerrainTexturePtr&,const Renderer::TerrainTexturePtr&) override { return {}; }
    void ReleaseSplatMaterial(Renderer::TerrainSplatMaterialPtr& p) override { p.reset(); }
    void SetSplatVertices(const Renderer::TerrainSplatVertex*,uint32_t) override {}
    void DrawSplat(const Renderer::TerrainBufferPtr&,const Renderer::TerrainBufferPtr&,uint32_t,bool,
                   const Renderer::TerrainSplatMaterialPtr&,const Renderer::TerrainSplatParameters&) override {}
    void DrawTerrainSolid(const Renderer::TerrainBufferPtr&,const Renderer::TerrainBufferPtr&,uint32_t,bool,const std::array<float,4>&) override {}
    Renderer::TerrainBufferPtr UploadVertices(const void*,uint32_t,uint32_t) override { return {}; }
    Renderer::TerrainBufferPtr UploadIndices(const uint16_t*,uint32_t) override { return {}; }
    void BeginTerrain(const Renderer::TerrainMatrices&,bool,const std::array<float,16>*) override {}
    void DrawTerrain(const Renderer::TerrainBufferPtr&,const Renderer::TerrainBufferPtr&,uint32_t,bool,const Renderer::TerrainTexturePtr&) override {}
};
int main()
{
    try
    {
        UploadProbe probe;
        TerrainAlphaImage alpha;
        for(bool fourBit:{false,true}) for(uint32_t mip=0;mip<5;++mip)
        {
            const uint32_t size=256u>>mip, stride=size*(fourBit ? 2 : 4)+16;
            std::vector<uint8_t> packed(size*stride,0);
            for(uint32_t y=0;y<size;++y) for(uint32_t x=0;x<size;++x)
                packed[y*stride+x*(fourBit ? 2 : 4)+(fourBit ? 1 : 3)]=uint8_t(x+y);
            alpha.Capture(mip,packed.data(),size,stride,fourBit);
            for(uint32_t y=0;y<size;++y) for(uint32_t x=0;x<size;++x)
                Check(alpha.Mip(mip)[y*size+x]==(fourBit ? (uint8_t(x+y)>>4)*17 : uint8_t(x+y)),"alpha pitch/channel/quantization");
        }
        const auto retained=alpha.Mip(0);
        alpha.Capture(5,nullptr,0,0,false);
        alpha.Capture(0,nullptr,256,1024,false);
        alpha.Capture(0,retained.data(),256,255,false);
        Check(alpha.Mip(0)==retained,"invalid alpha captures ignored safely");
        alpha.Clear();
        for(uint32_t mip=0;mip<5;++mip) Check(alpha.Mip(mip).empty(),"alpha CPU storage released");
        std::cout<<"Alpha capture: five mips / padded pitch / A8+A4 / bounds / release PASS\n";
        auto dds=TerrainFixture::DDS(7,5,3);
        ImageData::DDS2DView view;
        Check(SUCCEEDED(ImageData::GetDDS2DView(dds.data(),dds.size(),view)),"DDS CPU view");
        Check(view.mips[0].data==dds.data()+128 && view.mips[1].data==dds.data()+160,"borrowed mip offsets");
        auto texture=LoadTerrainTextureMemory(dds.data(),dds.size(),probe);
        Check(texture && probe.width==7 && probe.height==5 && probe.mips.size()==3,"DDS dimensions/mips");
        Check(probe.format==Renderer::TerrainTextureFormat::BC1 && probe.strides[0]==16 && probe.mips[0].size()==32,"BC1 odd-size row layout");
        Check(probe.mips[1].size()==8 && probe.mips[2].size()==8,"sub-block mips retained");
        for(uint32_t fourCC : {0x33545844u,0x35545844u})
        {
            auto compressed=TerrainFixture::DDS(8,8,4,fourCC);
            Check(LoadTerrainTextureMemory(compressed.data(),compressed.size(),probe)!=nullptr,"BC2/BC3 upload");
            Check(probe.strides[0]==32 && probe.mips[0].size()==64,"16-byte compression blocks");
        }
        auto tga=TerrainFixture::TGA();
        Check(LoadTerrainTextureMemory(tga.data(),tga.size(),probe)!=nullptr,"existing STB decoder");
        const std::vector<uint8_t> expected{255,0,0,255,0,255,0,255,0,0,255,255,255,255,0,255};
        Check(probe.format==Renderer::TerrainTextureFormat::RGBA8 && probe.mips.size()==1 && probe.mips[0]==expected,"RGBA channels/top-origin/one mip");
        auto truncated=dds; truncated.pop_back();
        Check(!LoadTerrainTextureMemory(truncated.data(),truncated.size(),probe),"truncated mip rejection");
        for(const auto field : {std::pair{4u,123u},std::pair{16u,0u},std::pair{16u,8193u},std::pair{28u,15u},std::pair{112u,0x200u},std::pair{84u,0x30315844u}})
        {
            auto bad=dds; TerrainFixture::Word(bad,field.first,field.second);
            Check(!LoadTerrainTextureMemory(bad.data(),bad.size(),probe),"invalid DDS rejected");
        }
        Check(!LoadTerrainTextureMemory(nullptr,0,probe),"empty texture rejected");
        auto staticDDS=TerrainFixture::B5G5R5A1DDS();
        Check(LoadStaticObjectTextureMemory(staticDDS.data(),staticDDS.size(),probe)!=nullptr,"static B5G5R5A1 upload");
        Check(probe.format==Renderer::TerrainTextureFormat::B5G5R5A1 && probe.mips.size()==5 &&
              probe.strides[0]==32 && probe.mips[0].size()==512 && probe.mips[4].size()==2,"original 16-bit channel layout and five mips");
        size_t cursor=128;
        for(const auto& mip:probe.mips) {
            Check(std::equal(mip.begin(),mip.end(),staticDDS.begin()+cursor),"16-bit mip bytes retained exactly");
            cursor+=mip.size();
        }
        Check(!LoadTerrainTextureMemory(staticDDS.data(),staticDDS.size(),probe),"terrain texture format contract unchanged");
        staticDDS.pop_back();
        Check(!LoadStaticObjectTextureMemory(staticDDS.data(),staticDDS.size(),probe),"truncated static final mip rejected");
        Check(!LoadStaticObjectTextureMemory(nullptr,0,probe),"empty static texture rejected");
        std::cout<<"Static texture CPU: original B5G5R5A1/five mips/byte identity/truncation/terrain isolation PASS\n";
        std::cout<<"Terrain texture CPU: BC1/2/3, mip bounds, DDS rejection, TGA RGBA orientation PASS\n";
        return 0;
    }
    catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
