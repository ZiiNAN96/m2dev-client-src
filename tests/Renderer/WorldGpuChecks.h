#pragma once
// ZiiNAN: Native D3D9 fixed-function output is the effect material oracle.
#include "Renderer/DiligentWorldRenderer.h"
#include "EterLib/NativeMaterialSnapshot.h"

static void WorldGpuChecks(Renderer::LegacyD3D9Backend& legacy,Renderer::DiligentD3D11Backend& modern)
{
    using namespace Renderer;
    DiligentWorldRenderer effects(modern); Check(effects.Initialize(),"effect shader initialization");
    auto* device=STATEMANAGER.GetDevice();
    const auto bytes=TerrainFixture::AlphaDDS();
    auto texture=LoadStaticObjectTextureMemory(bytes.data(),bytes.size(),effects);
    auto native=LegacyProbe::Texture(bytes);
    const std::array<float,16> identity={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    const char* originals=std::getenv("M2_WATER_TEXTURE_DIR");
    for(unsigned test=0;test<(originals ? 60u : 30u);++test) {
        if(test>=30) {
            const auto path=std::filesystem::path(originals)/((test-29<10 ? "0" : "")+std::to_string(test-29)+".dds");
            std::ifstream file(path,std::ios::binary);
            Check(bool(file),"original water texture available");
            std::vector<uint8_t> frame((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
            effects.ReleaseBindings(); texture.reset();
            Check(effects.LiveTextureCount()==0,"old water frame GPU lifetime");
            texture=LoadStaticObjectTextureMemory(frame.data(),frame.size(),effects); native=LegacyProbe::Texture(frame);
            Check(texture!=nullptr,"original water DDS upload");
        }
        const unsigned width=test%2 ? 192 : 160,height=test%2 ? 144 : 120;
        Check(legacy.Resize(width,height) && modern.Resize(width,height),"effect resize");
        if(test%10==0) Check(modern.Resize(0,0) && !modern.BeginFrame() && modern.Resize(width,height),"effect suspend restore");
        ObjectLegacyProbe::AlphaTarget alphaTarget(width,height);
        EffectDraw d; d.matrices={identity,identity,identity}; d.textureTransform=identity; d.factor={.6f,.8f,.4f,.65f};
        const bool water=test<10 || test>=16,lit=test==9 || test==16 || test==17 || (test>=22 && test<30);
        d.strip=!water; d.depthWrite=false; d.blend=water;
        d.colorOp=2; d.colorArg1=2; d.alphaOp=2; d.alphaArg1=0;
        d.textureCoordinates=water ? 0x20000 : 0; d.textureTransformFlags=2;
        d.textureTransform[0]=.7f; d.textureTransform[5]=-.9f;
        if(test<4) { d.fog=test; d.fogParameters={0,1,2,0}; d.fogColor={.2f,.3f,.4f,1}; }
        if(test==4) d.rangeFog=true;
        if(test==5) { d.alphaTest=true; d.alphaReference=128; }
        if(test==6) { d.sampler.min=d.sampler.mag=3; d.sampler.mip=2; d.sampler.anisotropy=4; }
        if(test==7) d.depthTest=false;
        if(test==8 || test==9 || test==17) { d.textured=false; d.blend=false; }
        if(test==10) { d.textured=false; d.blend=false; d.colorOp=3; d.colorArg1=d.colorArg2=0; d.alphaOp=1; }
        if(test==11) { d.blend=false; d.alphaOp=1; d.sampler.addressU=d.sampler.addressV=3; }
        if(test>=12 && test<=15) {
            d.colorOp=20; d.colorArg2=0; d.alphaArg1=2; d.blend=true; d.src=2; d.dst=4;
            d.textureTransform[8]=float(test-12)*.25f; d.textureTransform[9]=float(test-12)*.35f;
        }
        if(test>=18) {
            d.matrices.world[14]=-.15f; d.matrices.view[12]=.2f; d.matrices.view[13]=.1f;
            d.textureTransform[12]=-.14f; d.textureTransform[13]=.09f;
            d.alphaTest=test%2; d.alphaReference=60;
        }
        if(test>=22 && test<30) { d.textured=false; d.blend=false; d.alphaTest=false; }
        if(test>=30) { d.textureTransform[0]=4; d.textureTransform[5]=-4; d.sampler.min=d.sampler.mag=3; d.sampler.mip=2; d.sampler.anisotropy=4; }
        const auto part=water ? WorldPart::Water : (test>=12 ? WorldPart::Cloud : WorldPart::Sky);
        std::vector<EffectVertex> v;
        const EffectVertex quad[]={{{-.8f,-.8f,.5f},0xa099cc66,{-.1f,.2f}},{{-.8f,.8f,.5f},0x70cc9966,{-.1f,1.3f}},
            {{.8f,-.8f,.5f},0xe06699cc,{1.2f,.2f}},{{.8f,.8f,.5f},0x80cc6699,{1.2f,1.3f}}};
        if(d.strip) v.assign(quad,quad+4); else for(unsigned i:{0u,1u,2u,2u,1u,3u}) v.push_back(quad[i]);
        Check(legacy.BeginFrame() && modern.BeginFrame(),"effect begin frame");
        const ClearColor background={.1f,.2f,.3f,.4f}; legacy.Clear({true,background}); modern.Clear({true,background}); effects.ResetFrame();
        const auto rs=[&](D3DRENDERSTATETYPE type,DWORD value) { Check(SUCCEEDED(device->SetRenderState(type,value)),"effect native render state"); };
        const auto ts=[&](D3DTEXTURESTAGESTATETYPE type,DWORD value) { Check(SUCCEEDED(device->SetTextureStageState(0,type,value)),"effect native texture state"); };
        const auto ss=[&](D3DSAMPLERSTATETYPE type,DWORD value) { Check(SUCCEEDED(device->SetSamplerState(0,type,value)),"effect native sampler state"); };
        for(auto type:{D3DRS_LIGHTING,D3DRS_SPECULARENABLE,D3DRS_SEPARATEALPHABLENDENABLE,D3DRS_STENCILENABLE,D3DRS_SCISSORTESTENABLE}) rs(type,FALSE);
        rs(D3DRS_COLORWRITEENABLE,15); rs(D3DRS_FILLMODE,D3DFILL_SOLID); rs(D3DRS_SHADEMODE,D3DSHADE_GOURAUD);
        rs(D3DRS_ZENABLE,d.depthTest); rs(D3DRS_ZWRITEENABLE,d.depthWrite); rs(D3DRS_ZFUNC,d.depthFunction); rs(D3DRS_CULLMODE,d.cull+1);
        rs(D3DRS_ALPHABLENDENABLE,d.blend); rs(D3DRS_SRCBLEND,d.src); rs(D3DRS_DESTBLEND,d.dst); rs(D3DRS_BLENDOP,d.blendOp);
        rs(D3DRS_ALPHATESTENABLE,d.alphaTest); rs(D3DRS_ALPHAFUNC,d.alphaFunction); rs(D3DRS_ALPHAREF,d.alphaReference);
        rs(D3DRS_TEXTUREFACTOR,D3DCOLOR_COLORVALUE(d.factor[0],d.factor[1],d.factor[2],d.factor[3]));
        // Use quantized native TFACTOR, not a different float material color in the reference shader.
        D3DXCOLOR quantized(D3DCOLOR_COLORVALUE(d.factor[0],d.factor[1],d.factor[2],d.factor[3])); d.factor={quantized.r,quantized.g,quantized.b,quantized.a};
        ts(D3DTSS_COLOROP,d.colorOp); ts(D3DTSS_COLORARG1,d.colorArg1); ts(D3DTSS_COLORARG2,d.colorArg2);
        ts(D3DTSS_ALPHAOP,d.alphaOp); ts(D3DTSS_ALPHAARG1,d.alphaArg1); ts(D3DTSS_ALPHAARG2,d.alphaArg2);
        ts(D3DTSS_TEXCOORDINDEX,d.textureCoordinates); ts(D3DTSS_TEXTURETRANSFORMFLAGS,d.textureTransformFlags);
        device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE); device->SetTexture(1,nullptr);
        device->SetTexture(0,d.textured ? native.Get() : nullptr);
        ss(D3DSAMP_ADDRESSU,d.sampler.addressU); ss(D3DSAMP_ADDRESSV,d.sampler.addressV);
        ss(D3DSAMP_MINFILTER,d.sampler.min); ss(D3DSAMP_MAGFILTER,d.sampler.mag); ss(D3DSAMP_MIPFILTER,d.sampler.mip);
        ss(D3DSAMP_MAXANISOTROPY,d.sampler.anisotropy); ss(D3DSAMP_MAXMIPLEVEL,0); ss(D3DSAMP_MIPMAPLODBIAS,0); ss(D3DSAMP_BORDERCOLOR,0);
        rs(D3DRS_FOGENABLE,d.fog!=0); rs(D3DRS_FOGVERTEXMODE,d.fog); rs(D3DRS_FOGTABLEMODE,D3DFOG_NONE); rs(D3DRS_RANGEFOGENABLE,d.rangeFog);
        rs(D3DRS_FOGCOLOR,D3DCOLOR_COLORVALUE(.2f,.3f,.4f,1));
        const auto bits=[](float f) { DWORD b; memcpy(&b,&f,4); return b; };
        rs(D3DRS_FOGSTART,bits(d.fogParameters[0])); rs(D3DRS_FOGEND,bits(d.fogParameters[1])); rs(D3DRS_FOGDENSITY,bits(d.fogParameters[2]));
        for(auto entry:{std::pair<D3DTRANSFORMSTATETYPE,const std::array<float,16>*>(D3DTS_WORLD,&d.matrices.world),
            {D3DTS_VIEW,&d.matrices.view},{D3DTS_PROJECTION,&d.matrices.projection},{D3DTS_TEXTURE0,&d.textureTransform}})
            Check(SUCCEEDED(device->SetTransform(entry.first,reinterpret_cast<const D3DMATRIX*>(entry.second->data()))),"effect native transforms");
        device->SetVertexShader(nullptr); device->SetPixelShader(nullptr); device->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);
        D3DMATERIAL9 material{}; material.Diffuse={1,1,1,1}; material.Ambient={.3f,.6f,.9f,1};
        device->SetMaterial(&material); for(DWORD i=0;i<8;++i) device->LightEnable(i,FALSE);
        rs(D3DRS_LIGHTING,lit); rs(D3DRS_AMBIENT,0xff8090a0); rs(D3DRS_COLORVERTEX,TRUE);
        rs(D3DRS_DIFFUSEMATERIALSOURCE,D3DMCS_COLOR1); rs(D3DRS_AMBIENTMATERIALSOURCE,D3DMCS_MATERIAL); rs(D3DRS_EMISSIVEMATERIALSOURCE,D3DMCS_MATERIAL);
        if(test>=22 && test<30) {
            D3DLIGHT9 light{}; light.Type=test==22 ? D3DLIGHT_DIRECTIONAL : test==24 ? D3DLIGHT_SPOT : D3DLIGHT_POINT;
            light.Direction={0,0,1}; light.Position={0,0,-1}; light.Range=20; light.Ambient={.2f,.3f,.1f,0}; light.Diffuse={1,1,1,1};
            light.Attenuation0=1; light.Attenuation1=.3f; light.Attenuation2=.1f; light.Theta=.3f; light.Phi=2; light.Falloff=1;
            device->SetLight(0,&light); device->LightEnable(0,TRUE);
            if(test==25) rs(D3DRS_AMBIENTMATERIALSOURCE,D3DMCS_COLOR1);
            if(test==26) rs(D3DRS_EMISSIVEMATERIALSOURCE,D3DMCS_COLOR1);
            if(test==27) rs(D3DRS_DIFFUSEMATERIALSOURCE,D3DMCS_MATERIAL);
            if(test==28) rs(D3DRS_COLORVERTEX,FALSE);
            if(test==29) rs(D3DRS_AMBIENTMATERIALSOURCE,D3DMCS_COLOR2);
        }
        if(water) {
            struct NativeWater { float p[3]; uint32_t color; };
            std::vector<NativeWater> source(v.size());
            for(size_t i=0;i<v.size();++i) { memcpy(source[i].p,v[i].position.data(),12); source[i].color=v[i].color; }
            device->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE);
            Check(SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLELIST,2,source.data(),16)),"water native 16-byte draw");
        } else Check(SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v.data(),24)),"sky native draw");
        std::string snapshotError; auto captured=d;
        Check(CaptureNativeMaterial(captured,snapshotError),"world native snapshot");
        captured.strip=d.strip; captured.textured=d.textured; d=captured;
        std::vector<EffectVertex> resolved;
        if(water && !d.textured) Check(ResolveNativeWaterDiffuse(v.data(),uint32_t(v.size()),resolved),"native water ambient diffuse");
        effects.Draw(resolved.empty() ? v.data() : resolved.data(),uint32_t(v.size()),d.textured ? texture : TerrainTexturePtr{},d,part);
        Check(!effects.Failed() && effects.DrawCount(part)==1,"effect material submission");
        legacy.EndFrame(); modern.EndFrame();
        const auto a=LegacyProbe::Read(width,height),b=BackendTestAccess::Read(modern,false),depth=BackendTestAccess::Read(modern,true);
        double error=0; size_t compared=0,bad=0,depthErrors=0;
        for(unsigned y=height/5;y<height*4/5;++y) for(unsigned x=width/5;x<width*4/5;++x) {
            const size_t i=y*width+x; ++compared;
            for(unsigned c=0;c<4;++c) { const int delta=std::abs(int((a[i]>>((c==3 ? 3 : 2-c)*8))&255)-int((b[i]>>(c*8))&255)); error+=delta; if(delta>4) ++bad; }
            if(!d.alphaTest && d.cull==0 && ((depth[i]&0xffffff)!=0xffffff)!=(d.depthTest && d.depthWrite)) ++depthErrors;
        }
        std::cout<<"World native case="<<test<<" mean="<<error/(compared*4)<<" channel-errors="<<bad<<" depth-errors="<<depthErrors<<'\n';
        std::cout<<"world center native="<<std::hex<<a[height/2*width+width/2]<<" diligent="<<b[height/2*width+width/2]<<std::dec<<'\n';
        SaveSplatReadback(a,width,height,false,"world-case"+std::to_string(test)+"-d3d9");
        SaveSplatReadback(b,width,height,true,"world-case"+std::to_string(test)+"-d3d11");
        Check(error/(compared*4)<1.5 && bad<compared/100+1 && !depthErrors,"native effect color alpha depth parity");
        legacy.Present(); modern.Present();
    }
    // ZiiNAN: Terrain behind water, opaque object in front, then a transparent effect over water.
    Check(legacy.Resize(160,120) && modern.Resize(160,120),"water composition size");
    {
        ObjectLegacyProbe::AlphaTarget alphaTarget(160,120);
        Check(legacy.BeginFrame() && modern.BeginFrame(),"water depth composition frame");
        legacy.Clear({true,ClearColor{0,0,0,1}}); modern.Clear({true,ClearColor{0,0,0,1}});
        const auto rs=[&](D3DRENDERSTATETYPE type,DWORD value) { Check(SUCCEEDED(device->SetRenderState(type,value)),"water composition state"); };
        for(auto state:{D3DRS_LIGHTING,D3DRS_FOGENABLE,D3DRS_ALPHATESTENABLE,D3DRS_SPECULARENABLE,D3DRS_STENCILENABLE,D3DRS_SEPARATEALPHABLENDENABLE}) rs(state,FALSE);
        rs(D3DRS_COLORWRITEENABLE,15); rs(D3DRS_CULLMODE,D3DCULL_NONE); rs(D3DRS_ZENABLE,TRUE); rs(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
        rs(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA); rs(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA); rs(D3DRS_BLENDOP,D3DBLENDOP_ADD);
        device->SetVertexShader(nullptr); device->SetPixelShader(nullptr); device->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);
        for(auto transform:{D3DTS_WORLD,D3DTS_VIEW,D3DTS_PROJECTION,D3DTS_TEXTURE0}) device->SetTransform(transform,reinterpret_cast<const D3DMATRIX*>(identity.data()));
        device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE); device->SetTexture(1,nullptr);
        device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1); device->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);
        device->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE); device->SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);
        device->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,0);
        for(auto sample:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER}) device->SetSamplerState(0,sample,D3DTEXF_POINT);
        device->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE); device->SetSamplerState(0,D3DSAMP_MAXMIPLEVEL,0); device->SetSamplerState(0,D3DSAMP_MIPMAPLODBIAS,0);
        device->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_WRAP); device->SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_WRAP);
        const auto submit=[&](float left,float right,float bottom,float top,float z,uint32_t color,bool water,bool write) {
            EffectVertex v[]={{{left,bottom,z},color,{.3f,.4f}},{{left,top,z},color,{.3f,.4f}},
                {{right,bottom,z},color,{.3f,.4f}},{{right,top,z},color,{.3f,.4f}}};
            EffectDraw d; d.matrices={identity,identity,identity}; d.textureTransform=identity;
            d.textured=water; d.depthWrite=write; d.blend=!write; d.colorOp=d.alphaOp=2; d.colorArg1=water ? 2 : 0; d.alphaArg1=0;
            d.sampler.min=d.sampler.mag=1;
            rs(D3DRS_ZWRITEENABLE,write); rs(D3DRS_ALPHABLENDENABLE,d.blend);
            device->SetTexture(0,water ? native.Get() : nullptr); device->SetTextureStageState(0,D3DTSS_COLORARG1,d.colorArg1);
            Check(SUCCEEDED(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,v,24)),"native composed world layer");
            effects.Draw(v,4,water ? texture : TerrainTexturePtr{},d,WorldPart::Water);
        };
        submit(-.9f,.9f,-.9f,.9f,.8f,0xff885522,false,true);
        submit(-.9f,0,-.9f,.9f,.3f,0xff33bb44,false,true);
        submit(-.9f,.9f,-.9f,.9f,.5f,0x991155dd,true,false);
        submit(.2f,.6f,-.3f,.3f,.4f,0x99ff3300,false,false);
        legacy.EndFrame(); modern.EndFrame();
        const auto a=LegacyProbe::Read(160,120),b=BackendTestAccess::Read(modern,false),depth=BackendTestAccess::Read(modern,true);
        for(unsigned y=30;y<90;++y) for(unsigned x=30;x<130;++x) {
            if(x>=78 && x<=82) continue;
            const auto i=y*160+x;
            for(unsigned c=0;c<4;++c) Check(std::abs(int((a[i]>>((c==3 ? 3 : 2-c)*8))&255)-int((b[i]>>(c*8))&255))<=4,"water/world color and transparency parity");
            const float actual=float(depth[i]&0xffffff)/16777215.0f;
            Check(std::abs(actual-(x<80 ? .3f : .8f))<.0001f,"water and effect preserve opaque-world depth");
        }
        SaveSplatReadback(a,160,120,false,"world-depth-d3d9"); SaveSplatReadback(b,160,120,true,"world-depth-d3d11");
        std::cout<<"Water over terrain / behind object / before effect / unchanged depth: PASS\n";
    }
    effects.ReleaseBindings(); texture.reset(); Check(effects.LiveTextureCount()==0,"world map image lifetime");
    {
        // Original patch construction/Clear, not just a stand-alone shared_ptr ownership test.
        worldRenderer=&effects;
        CTerrainPatch patch;
        SWaterVertex original[3]{};
        patch.BuildWaterVertexBuffer(original,3);
        Check(patch.waterGeometry && waterGeometryCount==1 && patch.waterGeometry->vertices.size()==3,"native water patch capture");
        std::weak_ptr<WaterGeometry> lifetime=patch.waterGeometry;
        patch.Clear();
        Check(lifetime.expired() && waterGeometryCount==0,"native water patch release at map teardown");
        worldRenderer=nullptr;
    }
    effects.Shutdown(); Check(effects.LiveBufferCount()==0 && effects.LiveTextureCount()==0 && !effects.Failed(),"world shutdown resources zero");
    std::cout<<"World water/sky/cloud native parity and lifetime: PASS\n";
}
