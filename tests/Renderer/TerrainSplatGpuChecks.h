#pragma once
#include "GameLib/AreaTerrain.h"
#include <fstream>
#include <filesystem>

// Optional local evidence, never used by the distributed client or ordinary CTest runs.
static void SaveSplatReadback(const std::vector<uint32_t>& pixels, uint32_t width, uint32_t height,
                              bool rgba, const std::string& name)
{
    const char* directory=std::getenv("M2_TERRAIN_TEST_CAPTURE_DIR");
    if(!directory || !*directory) return;
    const auto path=std::filesystem::path(directory)/(name+".bmp");
    std::ofstream file(path,std::ios::binary);
    Check(bool(file),"open optional splat readback file");
    BITMAPFILEHEADER header{}; header.bfType=0x4d42;
    header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER); header.bfSize=header.bfOffBits+width*height*4;
    BITMAPINFOHEADER info{}; info.biSize=sizeof(info); info.biWidth=width; info.biHeight=-LONG(height);
    info.biPlanes=1; info.biBitCount=32; info.biCompression=BI_RGB;
    file.write(reinterpret_cast<const char*>(&header),sizeof(header));
    file.write(reinterpret_cast<const char*>(&info),sizeof(info));
    for(auto pixel:pixels)
    {
        if(rgba) pixel=(pixel&0xff00ff00)|((pixel&255)<<16)|((pixel>>16)&255);
        file.write(reinterpret_cast<const char*>(&pixel),4);
    }
    Check(bool(file),"write optional splat readback file");
}

class SplatTextureSetProbe : public CTextureSet
{
public:
    SplatTextureSetProbe() { m_Textures.resize(5); }
};
class SplatTerrainProbe : public CTerrain
{
public:
    void Build(bool fourBit)
    {
        for(int y=0;y<TILEMAP_RAW_YSIZE;++y) for(int x=0;x<TILEMAP_RAW_XSIZE;++x)
            m_abyTileMap[y*TILEMAP_RAW_XSIZE+x]=y>=129 ? 3 : (x>=129 ? 2 : 1);
        const auto old=ms_bSupportDXT; ms_bSupportDXT=!fourBit;
        RAW_AllocateSplats(); // Actual tile counting, neighbourhood expansion, smoothing and five mips.
        ms_bSupportDXT=old;
    }
    const TerrainAlphaImage& Alpha(uint32_t layer) const { return m_rendererAlpha[layer]; }
};

static void SplatChecks(LegacyProbe& screen, Renderer::LegacyD3D9Backend& legacy,
                        Renderer::DiligentD3D11Backend& modern, Renderer::DiligentTerrainRenderer& renderer)
{
    using namespace Renderer;
    SplatTextureSetProbe textureSet;
    CTerrainImpl::SetTextureSet(&textureSet);
    struct ResetTextureSet { ~ResetTextureSet() { CTerrainImpl::SetTextureSet(nullptr); } } resetTextureSet;
    uint32_t width=800,height=600;
    Check(legacy.Resize(width,height) && modern.Resize(width,height),"splat resize");
    HardwareTransformPatch_SSourceVertex vertices[289];
    for(int y=0;y<17;++y) for(int x=0;x<17;++x)
        vertices[y*17+x]={D3DXVECTOR3(11200.0f+x*200,-11200.0f-y*200,0),D3DXVECTOR3(0,0,1)};
    CTerrainPatch patch; patch.BuildTerrainVertexBuffer(vertices);
    const uint16_t indices[]{0,272,16,16,272,288};
    auto ib=renderer.UploadIndices(indices,6);
    std::array<TerrainTexturePtr,3> colors;
    std::array<Microsoft::WRL::ComPtr<IDirect3DTexture9>,3> legacyColors;
    for(int layer=0;layer<3;++layer)
    {
        auto image=TerrainFixture::GradientDDS();
        for(size_t offset=128;offset<image.size();offset+=4)
        {
            image[offset]=layer==2 ? 240 : 20;
            image[offset+1]=layer==1 ? 230 : 20;
            image[offset+2]=layer==0 ? 220 : 20;
            image[offset+3]=layer==0 ? 96 : 17; // Base alpha must not be forced opaque; overlays use mask, not17.
        }
        colors[layer]=LoadTerrainTextureMemory(image.data(),image.size(),renderer);
        legacyColors[layer]=LegacyProbe::Texture(image);
        Check(colors[layer]!=nullptr,"splat color upload");
    }
    for(bool fourBit:{false,true})
    {
        width=800; height=600;
        Check(legacy.Resize(width,height) && modern.Resize(width,height),"restore splat extent");
        SplatTerrainProbe alphaTerrain;
        alphaTerrain.Build(fourBit);
        auto& splats=alphaTerrain.GetTerrainSplatPatch();
        Check(!splats.Splats[4].Active && splats.TileCount[4]==0 && alphaTerrain.Alpha(4).Mip(0).empty(),"unused layer skipped by original generator");
        std::array<TerrainSplatMaterialPtr,3> materials;
        for(uint32_t layer=1;layer<=3;++layer)
        {
            Check(splats.Splats[layer].Active && splats.TileCount[layer]>0,"actual active splat layer");
            Check(splats.PatchTileCount[3*8+3][layer]>0,"existing neighbour patch counting");
            for(uint32_t mip=0;mip<5;++mip)
            {
                const auto& pixels=alphaTerrain.Alpha(layer).Mip(mip);
                Check(pixels.size()==size_t(256u>>mip)*(256u>>mip),"actual five alpha mips captured");
                if(fourBit) for(auto alpha:pixels) Check(alpha%17==0,"A4 quantization retained");
            }
            materials[layer-1]=alphaTerrain.GetSplatMaterial(layer,colors[layer-1]);
            Check(materials[layer-1]!=nullptr,"splat material creation");
        }
        Check(renderer.LiveMaterialCount()==3 && renderer.LiveAlphaCount()==3,"one alpha/material per terrain layer");
        std::vector<uint8_t> zeroMask(256*256,0);
        TerrainTextureData zeroData; zeroData.width=zeroData.height=256; zeroData.format=TerrainTextureFormat::Alpha8;
        for(uint32_t size=256;size>=16;size>>=1) zeroData.mips.push_back({zeroMask.data(),size*size,size});
        auto zeroAlpha=renderer.UploadTexture(zeroData);
        std::array<TerrainSplatMaterialPtr,3> zeroMaterials;
        for(int layer=0;layer<3;++layer) zeroMaterials[layer]=renderer.CreateSplatMaterial(colors[layer],zeroAlpha);
        auto zeroDDS=TerrainFixture::GradientDDS();
        for(size_t offset=131;offset<zeroDDS.size();offset+=4) zeroDDS[offset]=0;
        auto legacyZero=LegacyProbe::Texture(zeroDDS);
        std::vector<uint32_t> originalOrder;
        for(int pose=0;pose<9;++pose)
        {
            if(pose==8)
            {
                Check(modern.Resize(0,0) && !modern.BeginFrame(),"splat suspend");
                width=640; height=480;
                Check(legacy.Resize(width,height) && modern.Resize(width,height),"splat restore new extent");
            }
            screen.SetPositionCamera(12800,-12800,0,pose==2 ? 18000.0f : 6000.0f,65,pose==1 ? 80.0f : 0.0f);
            screen.SetPerspective(30,float(width)/height,100,100000);
            const auto matrices=screen.Matrices();
            D3DXMATRIX view,inverse,colorMatrix,alphaMatrix,identity;
            memcpy(&view,matrices.view.data(),64); D3DXMatrixInverse(&inverse,nullptr,&view);
            D3DXMatrixScaling(&colorMatrix,1.0f/640,-1.0f/640,0);
            D3DXMatrixScaling(&alphaMatrix,1.0f/25600,-1.0f/25600,0);
            alphaMatrix._41=alphaMatrix._42=4.6f/3200;
            colorMatrix=inverse*colorMatrix; alphaMatrix=inverse*alphaMatrix;
            D3DXMatrixIdentity(&identity);
            TerrainSplatParameters params;
            memcpy(params.colorTransform.data(),&colorMatrix,64); memcpy(params.alphaTransform.data(),&alphaMatrix,64);
            Check(legacy.BeginFrame() && modern.BeginFrame(),"splat begin");
            legacy.Clear({true,ClearColor{0,0,0,1}}); modern.Clear({true,ClearColor{0,0,0,1}});
            renderer.ResetFrame(); renderer.BeginTerrain(matrices,true); renderer.SetSplatVertices(nullptr,0);
            STATEMANAGER.SetTransform(D3DTS_WORLD,&identity);
            STATEMANAGER.SetTransform(D3DTS_TEXTURE0,&colorMatrix); STATEMANAGER.SetTransform(D3DTS_TEXTURE1,&alphaMatrix);
            STATEMANAGER.SetFVF(D3DFVF_XYZ|D3DFVF_NORMAL);
            STATEMANAGER.SetRenderState(D3DRS_LIGHTING,FALSE); STATEMANAGER.SetRenderState(D3DRS_FOGENABLE,FALSE);
            STATEMANAGER.SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
            STATEMANAGER.SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA); STATEMANAGER.SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
            STATEMANAGER.SetRenderState(D3DRS_ALPHATESTENABLE,TRUE);
            STATEMANAGER.SetRenderState(D3DRS_ALPHAFUNC,D3DCMP_GREATER); STATEMANAGER.SetRenderState(D3DRS_ALPHAREF,0);
            for(int stage=0;stage<2;++stage)
            {
                STATEMANAGER.SetTextureStageState(stage,D3DTSS_TEXCOORDINDEX,D3DTSS_TCI_CAMERASPACEPOSITION);
                STATEMANAGER.SetTextureStageState(stage,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_COUNT2);
                STATEMANAGER.SetSamplerState(stage,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
                STATEMANAGER.SetSamplerState(stage,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
                STATEMANAGER.SetSamplerState(stage,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);
                STATEMANAGER.SetSamplerState(stage,D3DSAMP_ADDRESSU,stage ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP);
                STATEMANAGER.SetSamplerState(stage,D3DSAMP_ADDRESSV,stage ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP);
            }
            STATEMANAGER.SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);
            STATEMANAGER.SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
            STATEMANAGER.SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_CURRENT);
            STATEMANAGER.SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);
            STATEMANAGER.SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
            STATEMANAGER.SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
            STATEMANAGER.SetTextureStageState(1,D3DTSS_COLORARG1,D3DTA_CURRENT);
            STATEMANAGER.SetTextureStageState(1,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
            STATEMANAGER.SetTextureStageState(2,D3DTSS_COLOROP,D3DTOP_DISABLE);
            struct STVertex { D3DXVECTOR4 position; DWORD diffuse,fog; D3DXVECTOR2 colorUV,alphaUV; } stVertices[289];
            if(pose==5 || pose==6)
            {
                TerrainSplatVertex attributes[289];
                D3DXMATRIX projection; memcpy(&projection,matrices.projection.data(),64);
                const auto frustum=view*projection;
                for(int i=0;i<289;++i)
                {
                    auto& st=stVertices[i]; auto& attr=attributes[i];
                    D3DXVec3Transform(&st.position,&vertices[i].kPosition,&frustum);
                    st.position.w=1/st.position.w;
                    st.position.x=(st.position.x*st.position.w+1)*width/2;
                    st.position.y=(1-st.position.y*st.position.w)*height/2;
                    st.position.z*=st.position.w;
                    st.diffuse=D3DCOLOR_ARGB(60+i%17*10,180,210,240);
                    st.fog=D3DCOLOR_ARGB(80+i/17*8,0,0,0);
                    const D3DXCOLOR diffuse(st.diffuse);
                    attr.diffuse={diffuse.r,diffuse.g,diffuse.b,diffuse.a}; attr.fog=float(st.fog>>24)/255;
                    const auto& p=vertices[i].kPosition;
                    attr.colorUV={p.x/640,-p.y/640}; attr.alphaUV={p.x/25600+4.6f/3200,-p.y/25600+4.6f/3200};
                    memcpy(&st.colorUV,attr.colorUV.data(),8); memcpy(&st.alphaUV,attr.alphaUV.data(),8);
                }
                renderer.SetSplatVertices(attributes,289); params.vertexUV=true;
                params.colorOp=TerrainColorOp::BlendDiffuseAlpha; params.textureFactor={0.25f,0.4f,0.6f,1};
                params.fog=TerrainFog::Vertex; params.fogColor=params.textureFactor;
                params.blend=pose==5;
                STATEMANAGER.SetRenderState(D3DRS_ALPHABLENDENABLE,params.blend);
                STATEMANAGER.SetRenderState(D3DRS_FOGENABLE,TRUE);
                STATEMANAGER.SetRenderState(D3DRS_FOGVERTEXMODE,D3DFOG_NONE);
                STATEMANAGER.SetRenderState(D3DRS_FOGCOLOR,D3DCOLOR_COLORVALUE(0.25f,0.4f,0.6f,1));
                STATEMANAGER.SetRenderState(D3DRS_TEXTUREFACTOR,D3DCOLOR_COLORVALUE(0.25f,0.4f,0.6f,1));
                STATEMANAGER.SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_BLENDDIFFUSEALPHA);
                STATEMANAGER.SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_TFACTOR);
                STATEMANAGER.SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);
                STATEMANAGER.SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
                STATEMANAGER.SetTextureStageState(1,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
                if(pose==6) STATEMANAGER.SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
                for(int stage=0;stage<2;++stage)
                {
                    STATEMANAGER.SetTextureStageState(stage,D3DTSS_TEXCOORDINDEX,stage);
                    STATEMANAGER.SetTextureStageState(stage,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
                }
                STATEMANAGER.SetFVF(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_SPECULAR|D3DFVF_TEX2);
            }
            if(pose==4)
            {
                // Original vertex range-fog, not a lighting modernization.
                params.fog=TerrainFog::Linear; params.fogStart=1000; params.fogEnd=12000; params.rangeFog=true;
                params.fogColor={0.25f,0.4f,0.6f,1};
                DWORD start,end; memcpy(&start,&params.fogStart,4); memcpy(&end,&params.fogEnd,4);
                STATEMANAGER.SetRenderState(D3DRS_FOGENABLE,TRUE); STATEMANAGER.SetRenderState(D3DRS_FOGVERTEXMODE,D3DFOG_LINEAR);
                STATEMANAGER.SetRenderState(D3DRS_RANGEFOGENABLE,TRUE);
                STATEMANAGER.SetRenderState(D3DRS_FOGSTART,start); STATEMANAGER.SetRenderState(D3DRS_FOGEND,end);
                STATEMANAGER.SetRenderState(D3DRS_FOGCOLOR,D3DCOLOR_COLORVALUE(0.25f,0.4f,0.6f,1));
            }
            const std::array<int,3> order=pose==3 ? std::array<int,3>{0,2,1} : std::array<int,3>{0,1,2};
            for(int layer:order)
            {
                params.alphaOp=layer==0 ? TerrainAlphaOp::Texture : TerrainAlphaOp::Mask;
                if(pose==5 && layer==0) params.alphaOp=TerrainAlphaOp::Diffuse;
                if(pose==6) params.alphaOp=TerrainAlphaOp::TextureTimesDiffuse;
                renderer.DrawSplat(patch.terrainGeometry,ib,6,false,pose==7 ? zeroMaterials[layer] : materials[layer],params);
                STATEMANAGER.SetTexture(0,legacyColors[layer].Get()); STATEMANAGER.SetTexture(1,pose==7 ? legacyZero.Get() : splats.Splats[layer+1].pd3dTexture);
                const bool software=pose==5 || pose==6;
                STATEMANAGER.SetTextureStageState(1,D3DTSS_ALPHAOP,layer==0 ? (software ? D3DTOP_SELECTARG2 : D3DTOP_DISABLE) : D3DTOP_SELECTARG1);
                STATEMANAGER.DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST,0,289,2,indices,D3DFMT_INDEX16,software ? static_cast<void*>(stVertices) : vertices,software ? sizeof(STVertex) : 24);
            }
            Check(!renderer.Failed() && renderer.SplatDrawCount()==3,"three original splat passes");
            legacy.EndFrame(); modern.EndFrame();
            const auto d9=LegacyProbe::Read(width,height), d11=BackendTestAccess::Read(modern,false);
            if(!fourBit && (pose==0 || pose==2 || pose==5 || pose==7))
            {
                SaveSplatReadback(d9,width,height,false,"splat-pose"+std::to_string(pose)+"-d3d9");
                SaveSplatReadback(d11,width,height,true,"splat-pose"+std::to_string(pose)+"-d3d11");
            }
            double error=0; size_t common=0,edges=0;
            for(size_t i=0;i<d9.size();++i)
            {
                const bool a=(d9[i]&0xffffff)!=0,b=(d11[i]&0xffffff)!=0;
                edges+=a!=b;
                if(!a || !b) continue;
                ++common;
                for(int c=0;c<3;++c) error+=std::abs(int((d9[i]>>(16-c*8))&255)-int((d11[i]>>(c*8))&255));
            }
            const double mean=error/(3*std::max(size_t(1),common));
            std::cout<<"Splat "<<(fourBit ? "A4" : "A8")<<" pose="<<pose<<" pixels="<<common<<" mean RGB="<<mean<<" edge="<<edges<<'\n';
            Check(common>4000 && edges<(width+height)/10 && mean<1.5,"D3D9/D3D11 splat parity");
            if(pose==7)
            {
                const auto center=d11[size_t(height/2)*width+width/2];
                Check(std::abs(int(center&255)-83)<=1 && ((center>>8)&255)<=8 && ((center>>16)&255)<=8,"base ignores empty mask; empty overlays contribute nothing");
            }
            if(pose==0) originalOrder=d11;
            if(pose==3)
            {
                size_t differences=0;
                for(size_t i=0;i<d11.size();++i) differences+=d11[i]!=originalOrder[i];
                Check(differences>50,"layer ordering affects overlap");
            }
            legacy.Present(); modern.Present();
        }
        STATEMANAGER.SetTexture(0,nullptr); STATEMANAGER.SetTexture(1,nullptr);
        for(auto& material:zeroMaterials) renderer.ReleaseSplatMaterial(material);
        renderer.ReleaseTexture(zeroAlpha);
        std::weak_ptr<TerrainSplatMaterial> lifetime=materials[0];
        materials={}; alphaTerrain.Clear();
        Check(lifetime.expired() && renderer.LiveMaterialCount()==0 && renderer.LiveAlphaCount()==0,"tile alpha/material release");
    }
    for(auto& color:colors) renderer.ReleaseTexture(color);
    CTerrainImpl::SetTextureSet(nullptr);
    std::cout<<"Original alpha generation / A4 / five mips / base alpha / ordered blends / fog / unload: PASS\n";
}
