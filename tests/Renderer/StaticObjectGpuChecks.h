#pragma once
#include "Renderer/DiligentStaticObjectRenderer.h"
#include "GameLib/StaticObjectBridge.h"
#include "EterLib/StaticObjectTextureLoader.h"

// Uses the native D3D9 fixed-function renderer as the reference, not another shader.
class ObjectLegacyProbe : public LegacyProbe
{
public:
    struct AlphaTarget
    {
        Microsoft::WRL::ComPtr<IDirect3DSurface9> original,target;
        AlphaTarget(uint32_t width,uint32_t height)
        {
            Check(SUCCEEDED(ms_lpd3dDevice->GetRenderTarget(0,&original)),"original native target");
            // Windowed D3D9 backbuffer is X8R8G8B8: its unused byte is not alpha.
            Check(SUCCEEDED(ms_lpd3dDevice->CreateRenderTarget(width,height,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr)) &&
                  SUCCEEDED(ms_lpd3dDevice->SetRenderTarget(0,target.Get())),"native alpha reference target");
        }
        ~AlphaTarget() { ms_lpd3dDevice->SetRenderTarget(0,original.Get()); }
    };
    static void Light(const D3DLIGHT9& light) { ms_lpd3dDevice->LightEnable(0,TRUE); STATEMANAGER.SetLight(0,&light); }
    static void SelectionLight(bool enabled)
    {
        D3DLIGHT9 light{};
        light.Type=D3DLIGHT_POINT; light.Position={0,200,200};
        light.Diffuse=light.Ambient={1,1,1,1}; light.Range=500;
        light.Attenuation0=0.1f; light.Attenuation1=0.01f;
        Check(SUCCEEDED(ms_lpd3dDevice->SetLight(1,&light)) &&
              SUCCEEDED(ms_lpd3dDevice->LightEnable(1,enabled)),"character selection light setup");
    }
};
static void StaticObjectChecks(LegacyProbe& screen,Renderer::LegacyD3D9Backend& legacy,
    Renderer::DiligentD3D11Backend& modern,Renderer::DiligentTerrainRenderer& terrain)
{
    using namespace Renderer;
    DiligentStaticObjectRenderer objects(modern);
    Check(objects.Initialize(),"static object pipeline initialization");
    StaticObjectSource source;
    // Prefix vertices/indices exercise nonzero base vertex AND group start offsets.
    source.vertices={{{0,0,0,0,0,1,0,0}},{{0,0,0,0,0,1,0,0}},
        {{0,0,0,0,0,1,-0.2f,0.1f}},{{0,-3200,0,0,0,1,-0.2f,4.1f}},
        {{3200,0,0,0,0,1,3.8f,0.1f}},{{3200,-3200,0,0,0,1,3.8f,4.1f}}};
    source.indices={0,0,0,0,1,2,2,1,3};
    auto geometry=objects.UploadGeometry(source);
    const auto image=TerrainFixture::GradientDDS();
    auto texture=LoadTerrainTextureMemory(image.data(),image.size(),objects);
    auto legacyTexture=LegacyProbe::Texture(image);
    Check(geometry && texture && objects.LiveGeometryCount()==1 && objects.LiveTextureCount()==1,"static object uploads");
    const auto alphaBytes=TerrainFixture::AlphaDDS(),b5Bytes=TerrainFixture::B5G5R5A1DDS();
    auto alphaTexture=LoadStaticObjectTextureMemory(alphaBytes.data(),alphaBytes.size(),objects);
    auto b5Texture=LoadStaticObjectTextureMemory(b5Bytes.data(),b5Bytes.size(),objects);
    auto legacyAlpha=LegacyProbe::Texture(alphaBytes),legacyB5=LegacyProbe::Texture(b5Bytes);
    Check(alphaTexture && b5Texture,"static alpha and native 16-bit uploads");
    for(int pose=0;pose<27;++pose) {
        const uint32_t width=pose<6 ? 640 : 800,height=pose<6 ? 480 : 600;
        Check(legacy.Resize(width,height) && modern.Resize(width,height),"static object resize");
        if(pose==6 || pose==19 || pose==23) { Check(modern.Resize(0,0) && !modern.BeginFrame(),"static object minimize"); Check(modern.Resize(width,height),"static object restore"); }
        ObjectLegacyProbe::AlphaTarget alphaTarget(width,height);
        screen.SetPositionCamera(1600,-1600,0,7000,45,float((pose%3)*70));
        screen.SetPerspective(30,float(width)/height,100,25600);
        StaticObjectDraw draw;
        draw.matrices=screen.Matrices(); draw.firstIndex=3; draw.indexCount=6; draw.baseVertex=2; draw.vertexCount=4;
        D3DXMATRIX world,rotation,scale,view,normal;
        D3DXMatrixRotationYawPitchRoll(&rotation,0.08f*(pose%3),0.03f*(pose%2),0.17f*(pose%3));
        D3DXMatrixScaling(&scale,pose==8 ? -1.0f : 1.0f,pose==9 ? 0.7f : 1.0f,1.0f);
        world=scale*rotation; world._41=pose==8 ? 3200 : float(pose%2)*80; world._43=80;
        memcpy(draw.matrices.world.data(),&world,64); memcpy(&view,draw.matrices.view.data(),64);
        normal=world*view; D3DXMatrixInverse(&normal,nullptr,&normal); D3DXMatrixTranspose(&normal,&normal);
        memcpy(draw.normalTransform.data(),&normal,64);
        draw.cull=pose==8 ? StaticObjectCull::CounterClockwise : (pose==7 ? StaticObjectCull::None : StaticObjectCull::Clockwise);
        draw.sampling.wrapU=draw.sampling.wrapV=pose!=10;
        draw.sampling.linearMin=draw.sampling.linearMag=draw.sampling.linearMip=pose!=11;
        draw.anisotropic=pose==12; draw.maxAnisotropy=1;
        D3DMATERIAL9 material{};
        material.Diffuse={0.7f,0.8f,0.9f,1}; material.Ambient={1,1,1,1};
        if(pose==13) material.Diffuse.a=0.4f;
        D3DLIGHT9 light{};
        light.Type=D3DLIGHT_DIRECTIONAL; light.Direction={0.3f,0.2f,-1};
        light.Diffuse={0.7f,0.6f,0.5f,1}; light.Ambient={0.1f,0.12f,0.15f,1};
        D3DXVECTOR3 direction(-light.Direction.x,-light.Direction.y,-light.Direction.z);
        D3DXVec3TransformNormal(&direction,&direction,&view); D3DXVec3Normalize(&direction,&direction);
        draw.lightDirection={direction.x,direction.y,direction.z,0};
        draw.ambient={0.1f,0.12f,0.15f,1}; draw.diffuse={0.49f,0.48f,0.45f,0};
        if(pose==13) { draw.ambient={1,1,1,1}; draw.diffuse={}; }
        draw.fog=pose>=3 && pose<=5 ? TerrainFog(pose-2) : TerrainFog::None;
        draw.rangeFog=true; draw.fogParameters={1000,12000,0.0001f,0}; draw.fogColor={0.2f,0.3f,0.4f,1};
        Check(legacy.BeginFrame() && modern.BeginFrame(),"static object begin");
        legacy.Clear({true,ClearColor{0,0,0,1}}); modern.Clear({true,ClearColor{0,0,0,1}});
        STATEMANAGER.SetTransform(D3DTS_WORLD,&world);
        STATEMANAGER.SetFVF(D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1);
        STATEMANAGER.SetRenderState(D3DRS_LIGHTING,TRUE); STATEMANAGER.SetRenderState(D3DRS_COLORVERTEX,FALSE);
        STATEMANAGER.SetRenderState(D3DRS_NORMALIZENORMALS,FALSE); STATEMANAGER.SetRenderState(D3DRS_SPECULARENABLE,FALSE);
        STATEMANAGER.SetRenderState(D3DRS_AMBIENT,0); STATEMANAGER.SetMaterial(&material); ObjectLegacyProbe::Light(light);
        // ResetEx preserves driver states, while this legacy cache resets to zero.
        // Explicitly initialize the native reference (no production StateManager fix).
        STATEMANAGER.SetRenderState(D3DRS_FOGENABLE,draw.fog==TerrainFog::None);
        STATEMANAGER.SetRenderState(D3DRS_FOGENABLE,draw.fog!=TerrainFog::None);
        STATEMANAGER.SetRenderState(D3DRS_FOGVERTEXMODE,static_cast<DWORD>(draw.fog));
        STATEMANAGER.SetRenderState(D3DRS_FOGTABLEMODE,D3DFOG_NONE); STATEMANAGER.SetRenderState(D3DRS_RANGEFOGENABLE,TRUE);
        STATEMANAGER.SetRenderState(D3DRS_FOGCOLOR,D3DXCOLOR(0.2f,0.3f,0.4f,1));
        DWORD bits; memcpy(&bits,&draw.fogParameters[0],4); STATEMANAGER.SetRenderState(D3DRS_FOGSTART,bits);
        memcpy(&bits,&draw.fogParameters[1],4); STATEMANAGER.SetRenderState(D3DRS_FOGEND,bits);
        memcpy(&bits,&draw.fogParameters[2],4); STATEMANAGER.SetRenderState(D3DRS_FOGDENSITY,bits);
        STATEMANAGER.SetRenderState(D3DRS_ALPHATESTENABLE,TRUE); STATEMANAGER.SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
        STATEMANAGER.SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE); STATEMANAGER.SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
        STATEMANAGER.SetRenderState(D3DRS_CULLMODE,static_cast<DWORD>(draw.cull)+1);
        STATEMANAGER.SetRenderState(D3DRS_ZENABLE,TRUE); STATEMANAGER.SetRenderState(D3DRS_ZWRITEENABLE,TRUE);
        STATEMANAGER.SetRenderState(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
        STATEMANAGER.SetTexture(0,legacyTexture.Get()); STATEMANAGER.SetTexture(1,nullptr);
        STATEMANAGER.SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);
        STATEMANAGER.SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
        STATEMANAGER.SetTextureStageState(0,D3DTSS_COLOROP,pose==13 ? D3DTOP_SELECTARG1 : D3DTOP_MODULATE);
        STATEMANAGER.SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE); STATEMANAGER.SetTextureStageState(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
        STATEMANAGER.SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);
        STATEMANAGER.SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE); STATEMANAGER.SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
        STATEMANAGER.SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
        STATEMANAGER.SetSamplerState(0,D3DSAMP_ADDRESSU,draw.sampling.wrapU ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP);
        STATEMANAGER.SetSamplerState(0,D3DSAMP_ADDRESSV,draw.sampling.wrapV ? D3DTADDRESS_WRAP : D3DTADDRESS_CLAMP);
        STATEMANAGER.SetSamplerState(0,D3DSAMP_MINFILTER,draw.sampling.linearMin ? D3DTEXF_LINEAR : D3DTEXF_POINT);
        STATEMANAGER.SetSamplerState(0,D3DSAMP_MAGFILTER,draw.sampling.linearMag ? D3DTEXF_LINEAR : D3DTEXF_POINT);
        STATEMANAGER.SetSamplerState(0,D3DSAMP_MIPFILTER,draw.sampling.linearMip ? D3DTEXF_LINEAR : D3DTEXF_POINT);
        if(draw.anisotropic) {
            STATEMANAGER.SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_ANISOTROPIC);
            STATEMANAGER.SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_ANISOTROPIC);
            STATEMANAGER.SetSamplerState(0,D3DSAMP_MAXANISOTROPY,draw.maxAnisotropy);
        }
        if(pose==13) {
            // Normal character selection leaves native light 1 enabled. It must
            // not reject texture-only RGB, but lit multi-light draws stay out of scope.
            StaticObjectDraw captured;
            Check(CaptureStaticMapObjectDraw(captured),"texture-only adapter baseline");
            ObjectLegacyProbe::SelectionLight(true);
            STATEMANAGER.SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);
            Check(!CaptureStaticMapObjectDraw(captured),"lit multiple-light contract still excluded");
            STATEMANAGER.SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
            Check(CaptureStaticMapObjectDraw(captured),"texture-only adapter after character selection");
            Check(captured.ambient[3]==material.Diffuse.a,"texture-only material alpha preserved");
            draw.ambient=captured.ambient; draw.diffuse=captured.diffuse;
        }
        auto selectedTexture=texture;
        if(pose>=14) {
            selectedTexture=pose<16 ? b5Texture : alphaTexture;
            STATEMANAGER.SetTexture(0,pose<16 ? legacyB5.Get() : legacyAlpha.Get());
            STATEMANAGER.SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_POINT);
            STATEMANAGER.SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_POINT);
            STATEMANAGER.SetSamplerState(0,D3DSAMP_MIPFILTER,pose==15 ? D3DTEXF_POINT : D3DTEXF_NONE);
            STATEMANAGER.SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);
            if(pose>=16 && pose<20) {
                STATEMANAGER.SetRenderState(D3DRS_ALPHATESTENABLE,TRUE);
                STATEMANAGER.SetRenderState(D3DRS_ALPHAFUNC,pose%2 ? D3DCMP_GREATER : D3DCMP_GREATEREQUAL);
                STATEMANAGER.SetRenderState(D3DRS_ALPHAREF,pose<18 ? 128 : (pose==18 ? 1 : 0));
            }
            if(pose>=20 && pose<25) {
                STATEMANAGER.SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
                STATEMANAGER.SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
                STATEMANAGER.SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
                STATEMANAGER.SetRenderState(D3DRS_BLENDOP,D3DBLENDOP_ADD);
                STATEMANAGER.SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE,FALSE);
                STATEMANAGER.SetRenderState(D3DRS_ZWRITEENABLE,pose!=21);
            }
            if(pose>=22 && pose<25) {
                STATEMANAGER.SetTexture(1,legacyAlpha.Get());
                STATEMANAGER.SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
                STATEMANAGER.SetTextureStageState(1,D3DTSS_COLORARG1,D3DTA_CURRENT);
                STATEMANAGER.SetTextureStageState(1,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);
                STATEMANAGER.SetTextureStageState(1,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
                STATEMANAGER.SetTextureStageState(1,D3DTSS_TEXCOORDINDEX,D3DTSS_TCI_CAMERASPACEPOSITION);
                STATEMANAGER.SetTextureStageState(1,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_COUNT2);
                D3DXMATRIX mask; D3DXMatrixIdentity(&mask);
                mask._11=mask._22=0.0003f; mask._41=mask._42=0.5f;
                STATEMANAGER.SetTransform(D3DTS_TEXTURE1,&mask);
                STATEMANAGER.SetSamplerState(1,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
                STATEMANAGER.SetSamplerState(1,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
                STATEMANAGER.SetSamplerState(1,D3DSAMP_MINFILTER,D3DTEXF_POINT);
                STATEMANAGER.SetSamplerState(1,D3DSAMP_MAGFILTER,D3DTEXF_POINT);
                STATEMANAGER.SetSamplerState(1,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
                if(pose==23) ObjectLegacyProbe::SelectionLight(true);
                if(pose==24) STATEMANAGER.SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);
            }
            StaticObjectDraw captured;
            if(pose>=25) {
                // Same pre-shadow capture used by RenderArea, followed by the
                // original base stage (without transferring its shadow texture).
                STATEMANAGER.SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
                material.Diffuse.a=0.4f; STATEMANAGER.SetMaterial(&material);
                if(pose==26) ObjectLegacyProbe::SelectionLight(true);
            }
            Check(CaptureStaticMapObjectDraw(captured,pose>=22 && pose<25,pose>=25),"static 4B original state capture");
            if(pose>=25) {
                STATEMANAGER.SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);
                STATEMANAGER.SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
            }
            captured.matrices=draw.matrices; captured.normalTransform=draw.normalTransform;
            captured.firstIndex=3; captured.indexCount=6; captured.baseVertex=2; captured.vertexCount=4;
            if(pose>=22 && pose<25) captured.cameraAlpha=alphaTexture;
            if(pose==23) Check(captured.pointPositionRange[3]==500,"original native point light captured");
            draw=captured;
        }
        STATEMANAGER.DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST,0,4,2,source.indices.data()+3,D3DFMT_INDEX16,source.vertices.data()+2,32);
        objects.ResetFrame(); objects.Draw(geometry,selectedTexture,draw);
        legacy.EndFrame(); modern.EndFrame();
        Check(!objects.Failed() && objects.DrawCount()==1,"static object draw");
        const auto d9=LegacyProbe::Read(width,height),d11=BackendTestAccess::Read(modern,false),depth=BackendTestAccess::Read(modern,true);
        size_t covered=0,edges=0,alphaErrors=0,alphaBoundary=0; double error=0;
        for(size_t i=0;i<d9.size();++i) {
            const bool a=(d9[i]&0xffffff)!=0,b=(d11[i]&0xffffff)!=0;
            edges+=a!=b; if(!a || !b) continue; ++covered;
            Check(((depth[i]&0xffffff)<0xffffff)==draw.depthWrite,"object depth write state");
            if(pose>=14 && std::abs(int(d9[i]>>24)-int(d11[i]>>24))>2) {
                // Point sampling may choose opposite texels at an exact boundary.
                // Only accept a one-pixel boundary in BOTH native and Diligent images.
                bool nativeEdge=false,modernEdge=false;
                for(int offset:{-1,1,-int(width),int(width)}) {
                    const auto neighbour=int64_t(i)+offset;
                    if(neighbour<0 || neighbour>=int64_t(d9.size())) continue;
                    nativeEdge|=std::abs(int(d9[i]>>24)-int(d9[neighbour]>>24))>2;
                    modernEdge|=std::abs(int(d11[i]>>24)-int(d11[neighbour]>>24))>2;
                }
                if(nativeEdge && modernEdge) ++alphaBoundary;
                else ++alphaErrors;
            }
            if(pose==13) Check(std::abs(int(d11[i]>>24)-102)<=1,"texture-only GPU material alpha");
            for(int c=0;c<3;++c) error+=std::abs(int((d9[i]>>(16-c*8))&255)-int((d11[i]>>(c*8))&255));
        }
        const double mean=covered ? error/(covered*3) : 999;
        if(pose==0 || pose==13 || pose>=14) {
            SaveSplatReadback(d9,width,height,false,"object-pose"+std::to_string(pose)+"-d3d9");
            SaveSplatReadback(d11,width,height,true,"object-pose"+std::to_string(pose)+"-d3d11");
        }
        if(mean>=1.5) {
            SaveSplatReadback(d9,width,height,false,"object-failure-d3d9");
            SaveSplatReadback(d11,width,height,true,"object-failure-d3d11");
            std::cout << "center d9=" << std::hex << d9[width*(height/2)+width/2] << " d11=" << d11[width*(height/2)+width/2] << std::dec << '\n';
        }
        std::cout<<"Static object pose="<<pose<<" pixels="<<covered<<" RGB="<<mean<<" edges="<<edges<<" alpha-errors="<<alphaErrors<<" alpha-boundary="<<alphaBoundary<<'\n';
        Check(!alphaErrors && alphaBoundary<(width+height)/10,"original object alpha channel");
        Check(covered>1000 && mean<1.5 && edges<(width+height)/10,"static diffuse/transform/light/fog/cull parity");
        legacy.Present(); modern.Present();
        if(pose==13 || pose==23 || pose==26) ObjectLegacyProbe::SelectionLight(false);
    }
    // Same depth target as terrain, both submission orders, object above/below ground.
    {
        constexpr uint32_t width=800,height=600;
        screen.SetPositionCamera(1600,-1600,0,7000,45,0);
        screen.SetPerspective(30,float(width)/height,100,25600);
        const auto matrices=screen.Matrices();
        float vertices[289][6]{};
        vertices[0][0]=0; vertices[0][1]=0;
        vertices[1][0]=0; vertices[1][1]=-3200;
        vertices[2][0]=3200; vertices[2][1]=0;
        vertices[3][0]=3200; vertices[3][1]=-3200;
        const uint16_t indices[]={0,1,2,2,1,3};
        auto vb=terrain.UploadVertices(vertices,289,24),ib=terrain.UploadIndices(indices,6);
        StaticObjectDraw draw;
        draw.matrices=matrices; draw.firstIndex=3; draw.indexCount=6; draw.baseVertex=2; draw.vertexCount=4;
        for(float heightOffset:{-200.0f,200.0f}) {
            draw.matrices.world[14]=heightOffset;
            std::vector<uint32_t> firstColor,firstDepth;
            for(int order=0;order<2;++order) {
                Check(modern.BeginFrame(),"terrain/object overlap begin"); modern.Clear({true,ClearColor{0,0,0,1}});
                terrain.ResetFrame(); terrain.BeginTerrain(matrices,true);
                const auto ground=[&]() { terrain.DrawTerrainSolid(vb,ib,6,false,{0.1f,0.8f,0.2f,1}); };
                if(order==0) objects.Draw(geometry,texture,draw);
                ground();
                if(order==1) objects.Draw(geometry,texture,draw);
                modern.EndFrame();
                const auto color=BackendTestAccess::Read(modern,false),depth=BackendTestAccess::Read(modern,true);
                if(order==0) { firstColor=color; firstDepth=depth; }
                else Check(color==firstColor && depth==firstDepth,"terrain/object shared depth independent of draw order");
                const uint32_t center=color[width*(height/2)+width/2];
                const bool green=((center>>8)&255)>170 && (center&255)<50;
                Check(green==(heightOffset<0),"terrain hides behind object; front object remains visible");
                modern.Present();
            }
        }
        Check(!terrain.Failed() && !objects.Failed(),"terrain/object overlap pipelines");
        std::cout<<"Terrain/object shared depth front/behind and both submission orders: PASS\n";
    }
    objects.ReleaseBindings();
    std::weak_ptr<StaticObjectGeometry> weakGeometry=geometry; std::weak_ptr<TerrainTexture> weakTexture=texture;
    geometry.reset(); texture.reset(); alphaTexture.reset(); b5Texture.reset();
    Check(weakGeometry.expired() && weakTexture.expired() && !objects.LiveGeometryCount() && !objects.LiveTextureCount(),"static resource map-release lifetime");
    {
        DiligentStaticObjectRenderer invalid(modern); Check(invalid.Initialize(),"invalid input test initialize");
        StaticObjectSource bad=source; bad.indices[0]=65535;
        Check(!invalid.UploadGeometry(bad) && invalid.Failed(),"invalid static index rejected");
    }
    STATEMANAGER.SetTexture(0,nullptr); STATEMANAGER.SetRenderState(D3DRS_CULLMODE,D3DCULL_CW);
    std::cout<<"Static original PNT/uint16 offsets, transforms/reflection, light/fog, sampling, resize/suspend/release: PASS\n";
}
