#include "EterLib/StdAfx.h"
#include "EterLib/GrpDevice.h"
#include "EterLib/GrpScreen.h"
#include "EterLib/GrpImageTexture.h"
#include "EterLib/GrpVertexBuffer.h"
#include "EterLib/GrpIndexBuffer.h"
#include "EterLib/DrawStateView.h"
#include "EterLib/SourceResourceAudit.h"
#include "EterLib/TextureSource.h"
#include "EterLib/Camera.h"
#include "GameLib/TerrainAlphaImage.h"
#include "TerrainTextureFixtures.h"
#include "Renderer/DiligentTerrainRenderer.h"
#include <iostream>
#include <stdexcept>

float CCamera::CAMERA_MAX_DISTANCE=2500.0f;
static void Check(bool condition,const char* error) { if(!condition) throw std::runtime_error(error); }

static void CheckDiligentResources(HWND window)
{
    Renderer::DiligentD3D11Backend backend;
    Check(backend.Initialize({window,400,300}),"Diligent initialize with CPU resource sources");
    {
        Renderer::DiligentTerrainRenderer terrain(backend);
        Check(terrain.Initialize(),"Diligent pipeline/depth resources without native declarations");
        CGraphicVertexBuffer sourceVertices;
        Check(sourceVertices.Create(289,Renderer::VertexPosition|Renderer::VertexNormal),"CPU upload vertex source");
        void* data=nullptr; Check(sourceVertices.Lock(&data),"CPU upload vertex lock");
        auto* v=static_cast<float*>(data);
        const float triangle[18]={-.5f,-.5f,.5f,0,0,1, -.5f,.5f,.5f,0,0,1, .5f,-.5f,.5f,0,0,1};
        memcpy(v,triangle,sizeof(triangle));
        auto vertices=terrain.UploadVertices(v,289,24); sourceVertices.Unlock();
        const uint16_t triangleIndices[]={0,1,2};
        auto indices=terrain.UploadIndices(triangleIndices,3);
        auto source=Renderer::TextureResource::Dynamic(4,4,Renderer::TerrainTextureFormat::RGBA8);
        std::fill(source->mips[0].pixels.begin(),source->mips[0].pixels.end(),255);
        auto texture=terrain.UploadTexture(source->View());
        Check(vertices && indices && texture,"direct CPU-to-Diligent buffers and texture");
        Renderer::TerrainMatrices matrices{};
        for(auto* m:{&matrices.world,&matrices.view,&matrices.projection}) (*m)[0]=(*m)[5]=(*m)[10]=(*m)[15]=1;
        for(auto size:{std::pair{400u,300u},std::pair{640u,360u},std::pair{320u,240u}}) {
            Check(backend.Resize(0,0) && !backend.BeginFrame(),"Diligent minimized surface");
            Check(backend.Resize(size.first,size.second) && backend.BeginFrame(),"Diligent restore/resize");
            backend.Clear({true,Renderer::ClearColor{.1f,.2f,.3f,1}});
            terrain.ResetFrame(); terrain.BeginTerrain(matrices,true);
            terrain.DrawTerrain(vertices,indices,3,false,texture);
            Check(!terrain.Failed() && terrain.DrawCount()==1,"neutral-source draw after resize");
            std::vector<uint8_t> pixels; uint32_t width=0,height=0;
            Check(backend.CaptureRGB(pixels,width,height) && width==size.first && height==size.second,"Diligent target readback extent");
            Check(pixels.size()==size_t(width)*height*3 && pixels[0]>=24 && pixels[0]<=27,"Diligent clear pixels");
            backend.EndFrame();
            backend.Present();
        }
        terrain.ReleaseTexture(texture); vertices.reset(); indices.reset();
        Check(terrain.LiveTextureCount()==0,"Diligent source texture released");
    }
    backend.Shutdown(); backend.Shutdown();
}

static void CheckAlpha()
{
    std::vector<uint8_t> source(258*258);
    for(size_t i=0;i<source.size();++i) source[i]=uint8_t((i*37)^(i/19));
    for(bool fourBit:{false,true}) {
        TerrainAlphaImage actual; actual.Build(source.data(),fourBit);
        std::vector<uint8_t> reference(256*256);
        for(unsigned y=0;y<256;++y) for(unsigned x=0;x<256;++x) {
            unsigned sum=0;
            for(unsigned dy=0;dy<3;++dy) for(unsigned dx=0;dx<3;++dx)
                if(dx!=1 || dy!=1) sum+=source[(y+dy)*258+x+dx];
            reference[y*256+x]=uint8_t(((sum/8)+source[(y+1)*258+x+1])/2);
        }
        unsigned size=256;
        for(unsigned level=0;level<5;++level) {
            Check(actual.Mip(level).size()==reference.size(),"alpha mip dimensions");
            for(size_t i=0;i<reference.size();++i)
                Check(actual.Mip(level)[i]==(fourBit ? (reference[i]/16)*17 : reference[i]),"alpha filter/quantization parity");
            std::vector<uint8_t> next(size*size/4);
            for(unsigned y=0;y<size/2;++y) for(unsigned x=0;x<size/2;++x) {
                const unsigned i=y*2*size+x*2;
                next[y*(size/2)+x]=uint8_t((unsigned(reference[i])+reference[i+1]+reference[i+size]+reference[i+size+1])/4);
            }
            reference.swap(next); size/=2;
        }
        actual.Clear(); Check(actual.Mip(0).empty(),"alpha unload");
        actual.Build(source.data(),fourBit); Check(!actual.Mip(4).empty(),"alpha reload");
    }
}

int main()
{
    HWND window=CreateWindowW(L"STATIC",L"Neutral resource ownership test",WS_OVERLAPPEDWINDOW,0,0,320,240,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    try {
        // ZiiNAN: Backend-neutral graphics resource ownership
        CGraphicDevice graphics;
        Check(window && graphics.Create({window},320,240)==CGraphicDevice::CREATE_OK,"CPU graphics context without device");
        {
            auto bytes=TerrainFixture::DDS(7,5,3);
            auto source=DecodeTextureSource(bytes.data(),bytes.size(),"fixture.dds");
            Check(source && source->desc.width==7 && source->desc.height==5 && source->desc.mipLevels==3,"asset metadata without native texture");
            Check(source->desc.format==Renderer::TerrainTextureFormat::BC1 && source->asset=="fixture.dds","asset format/reference");
            const auto first=source->mips[0].pixels; bytes.assign(bytes.size(),0);
            Check(first==source->mips[0].pixels,"source owns borrowed decoder bytes");
            Check(!DecodeTextureSource(bytes.data(),bytes.size(),"invalid.dds"),"invalid image is rejected");
            Check(!Renderer::TextureResource::Copy({4,4,Renderer::TerrainTextureFormat::BGRA8,{{first.data(),first.size(),0}}}),"invalid row stride");

            CGraphicImageTexture texture,alias;
            Check(texture.Create(8,4,Renderer::TerrainTextureFormat::BGRA8) && texture.GetSource()!=nullptr,"dynamic CPU texture");
            int pitch=0; void* pixels=nullptr;
            Check(texture.Lock(&pitch,&pixels) && pitch==32 && pixels,"CPU atlas lock");
            memset(pixels,0x7b,size_t(pitch)*4);
            void* rejected=nullptr; Check(!texture.Lock(&pitch,&rejected),"double texture lock rejected");
            texture.Unlock(); Check(texture.GetSource()->revision==1,"dynamic update revision");
            Check(!texture.Lock(&pitch,&rejected,9),"mip bounds");
            alias.CreateFromTexturePointer(&texture);
            Check(alias.GetTextureBinding()==texture.GetTextureBinding(),"shared subimage identity");
            auto binding=texture.GetTextureBinding();
            std::weak_ptr<Renderer::TextureResource> lifetime=binding.source;
            DRAWSTATE.SetTexture(0,binding);
            DRAWSTATE.SaveTexture(0,TextureBinding(source));
            DRAWSTATE.SaveTexture(0,nullptr);
            texture.Destroy(); alias.Destroy(); binding={};
            Check(!lifetime.expired(),"saved binding retains original owner");
            Check(!DrawStateView().GetTextureBinding(0),"neutral unbind");
            DRAWSTATE.RestoreTexture(0);
            Check(DrawStateView().GetTextureBinding(0).source==source,"nested restore source");
            DRAWSTATE.RestoreTexture(0);
            Check(DrawStateView().GetTextureBinding(0).source==lifetime.lock(),"nested restore dynamic page");
            DRAWSTATE.SetTexture(0,nullptr);
            Check(lifetime.expired(),"null binding clears source");
            DRAWSTATE.SetTexture(0,TextureBinding(source));
            Check(graphics.ResizeBackBuffer(400,300),"resize without native resources");
            Check(DrawStateView().GetTextureBinding(0).source==source,"resize preserves CPU bindings without device reset");
            DRAWSTATE.SetTexture(0,nullptr);

            CGraphicVertexBuffer vertices;
            Check(vertices.Create(4,Renderer::VertexPosition|Renderer::VertexTex1),"CPU vertex creation");
            Check(!vertices.IsEmpty() && !vertices.IsEmpty() && vertices.GetVertexStride()==20,"vertex metadata/presence");
            void* data=nullptr; Check(vertices.LockRange(4,&data),"vertex producer lock");
            memset(data,0x23,80); Check(vertices.Unlock(),"vertex unlock");
            Check(!vertices.LockRange(5,&data),"vertex range bounds");
            Check(vertices.Lock(&data) && static_cast<uint8_t*>(data)[79]==0x23,"vertex data retained"); vertices.Unlock();
            Check(!vertices.Copy(81,first.data()),"vertex copy bounds");
            vertices.DestroyDeviceObjects(); Check(vertices.IsEmpty(),"vertex destruction");
            Check(vertices.CreateDeviceObjects() && !vertices.IsEmpty(),"vertex recreate remains CPU");
            CGraphicVertexBuffer dungeonVertices;
            // The original dungeon FVF ORs TEX1/TEX2 (48 allocation bytes); its draw source is PNT2 (40 bytes).
            Check(dungeonVertices.Create(3,Renderer::VertexPosition|Renderer::VertexNormal|Renderer::VertexTex1|Renderer::VertexTex2),"dungeon CPU capacity");
            Check(dungeonVertices.GetBufferSize()>=3*40 && dungeonVertices.GetVertexStride()==48,"allocation capacity is not draw stride");
            std::array<uint8_t,3*40> dungeonPixels{}; dungeonPixels.back()=0x6c;
            Check(dungeonVertices.Copy(int(dungeonPixels.size()),dungeonPixels.data()) && dungeonVertices.Lock(&data),"dungeon packed PNT2 source");
            Check(memcmp(data,dungeonPixels.data(),dungeonPixels.size())==0,"dungeon packed source parity"); dungeonVertices.Unlock();

            CGraphicIndexBuffer indices;
            const uint16_t triangle[]={0,2,1};
            Check(indices.Create(3,Renderer::IndexFormat::UInt16) && indices.GetIndexCount()==3,"CPU index creation");
            Check(indices.Copy(sizeof(triangle),triangle) && indices.Lock(&data),"CPU index producer");
            Check(memcmp(data,triangle,sizeof(triangle))==0,"index data retained"); indices.Unlock();
            Check(!indices.Copy(7,triangle),"index copy bounds");
            TFace face{};
            face.indices[0]=0; face.indices[1]=2; face.indices[2]=1;
            Check(indices.Create(1,&face) && indices.GetIndexCount()==3,"CPU face index creation");
            Check(indices.Lock(&data) && memcmp(data,triangle,sizeof(triangle))==0,"face index source parity"); indices.Unlock();
            Renderer::CpuBuffer buffer;
            Check(buffer.Create(32) && buffer.Lock(8,24,&data),"buffer subrange");
            Check(!buffer.Lock(0,1,&data),"double buffer lock"); buffer.Unlock();
            Check(!buffer.Lock(31,2,&data),"buffer end bounds");
            Check(!buffer.Lock(size_t(-1),1,&data),"buffer overflow bounds");
            buffer.Clear(); Check(!buffer.Unlock(),"released lock");
            CheckAlpha();
        }
        CheckDiligentResources(window);
        graphics.Destroy(); DestroyWindow(window); window=nullptr;
        Check(Renderer::liveSourceTextures==0 && Renderer::liveSourceBuffers==0,"CPU source lifetime leak");
        Renderer::WriteSourceResourceAudit(std::cout);
        std::cout << "Texture/buffer identity, metadata, bounds, reload, alpha parity, resize, shutdown and source lifetime: PASS\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; if(window) DestroyWindow(window); return 1; }
}
